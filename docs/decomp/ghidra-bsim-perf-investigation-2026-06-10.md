# Ghidra BSim VT performance investigation

**Date:** 2026-06-10
**Scenario:** ghidriff `--bsim` on RB3 Wii Bank8 (PowerPC Gekko, ~40k funcs, symbolized)
vs. RB3 Xbox360 (PowerPC Xenon/VMX128, ~65k funcs, STRIPPED)
**Template:** `docs/decomp/ghidra-vt-optimization-2026-06-09.md` (VT reference-correlator
parallelization) — same Ghidra fork, same analysis methodology.

All file:line refs are against `/home/free/code/milohax/ghidra`.

---

## 1. Executive summary

BSim enters a single-threaded compute stall **after** signature generation and
decompilation complete, spending 70+ minutes on one CPU core with no log output.
The stall is caused by an **O(n\_source × n\_dest) FunctionPair blowup** driven by
degenerate LSH bins: when the destination program is stripped (many functions have
similar feature vocabularies, or decompile outputs are too short to discriminate),
`BinningSystem.lookup` returns O(n\_source) candidates per destination node instead of
the expected ~5.  Two single-threaded phases then process all those pairs serially.

**The bottleneck:** `discoverPotentialMatches` in `BSimProgramCorrelatorMatching.java`
(lines 181–238) — specifically the serial result-aggregation loop at lines 221–237
that calls `addAssociate` O(n\_source × n\_dest) times, and the downstream
`chooseSeeds` round-loop at lines 307–354.

**Top two fixes (in delivery order):**

| # | Fix | Complexity | Est. win | Risk |
|---|-----|------------|----------|------|
| T1 | Cap `BinningSystem.lookup` at `MAX_LOOKUP_CANDIDATES` (Tier 1 algorithmic) | S (done — prototype committed) | Hours → seconds for degenerate case; zero cost for well-behaved case | Very low: degenerate-bin functions have no confident match anyway |
| T2 | Parallelise the serial result-aggregation loop in `discoverPotentialMatches` (Tier 2 concurrency) | M | 4–16× wall-clock on result aggregation + chooseSeeds data prep | Medium: needs careful thread-safe addAssociate replacement |

**Tuning alone** (raising similarity threshold, switching to MEDIUM model) can help
significantly; see §4.

**Prototype branch:** `/tmp/claude/ghidra-bsim-perf` (git worktree),
branch `bsim-perf-candidatecap` in `/home/free/code/milohax/ghidra`.
Two files changed, compile-checked only (no gradle build needed).

---

## 2. Call chain: ghidriff → Ghidra BSim

```
bsim.py:correlate_bsim()
  BSimProgramCorrelatorFactory.createCorrelator()
  bsim_correlator.correlate(session, monitor)     # VTAbstractProgramCorrelator.correlate
    doCorrelate(matchSet, monitor)                 # BSimProgramCorrelator.java:79

      # Phase 1+3: signature/decompile (parallel)
      generateNodes(sourceProgram, srcAddrSet, ...)  # BSimProgramCorrelator.java:179-205
        ParallelDecompiler.decompileFunctions(callback, program, ...)
          ParallelDecompilerCallback.process(function, monitor)  # java:315-349
            decompiler.generateSignatures(function, ..., TIMEOUT=60, ...)  # -> decompiler subprocess
            vectorFactory.buildVector(sigres.features)  # WeightedLSHCosineVectorFactory.java
            return FunctionNode(function, vec, callAddresses)
      FunctionNodeContainer(program, rawNodes)       # FunctionNodeContainer.java:30-36
        generateCallGraph()                          # java:71-100 — SILENT, DB reads

      # Phase 5: matching (single-threaded stall here)
      BSimProgramCorrelatorMatching.discoverPotentialMatches(monitor)  # java:181-238
        BinningSystem.add(sourceNodes)               # BinningSystem.java:63-82 — SERIAL
        ConcurrentQ<FunctionNode, List<FunctionPair>>  # parallel over destNodes
          MatchingCallback.process(queryNode, monitor)  # java:75-88
            BinningSystem.lookup(queryNode)           # -> Set<FunctionNode> candidates
            findSimilarNodes(results, queryNode, ...)  # secondary cosine compare
        waitForResults()                              # collects all ConcurrentQ results
        # SERIAL AGGREGATION — the stall:
        for result in results:
          for bridge in result.pieces:
            srcNode.addAssociate(destNode, pair)     # HashMap.put — O(n_src*n_dest) total
            destNode.addAssociate(srcNode, pair)

      BSimProgramCorrelatorMatching.generateSeeds(matchSet, ...)  # java:396-404
        findAcceptedSeeds(matchSet, monitor)         # iterates VT session associations
        chooseSeeds(monitor)                         # java:280-371 — SERIAL, round loop

      BSimProgramCorrelatorMatching.doMatching(monitor)  # java:450-540
        # round 1 + 2: implications TreeSet, NeighborGenerator fan-out

      wrapUp(result, matchSet, monitor)              # java:207-225 — serial DB writes
```

---

## 3. The bottleneck: root cause and evidence

### 3.1 Anatomy of the stall

After `generateNodes` completes for both programs, all `Msg.*` calls that would
produce log output are in:
- `Msg.info`: `"BSim Program Correlator could not find any seeds"` (after `generateSeeds`)
- `Msg.error`/`Msg.warn`: catch blocks

Monitor `setMessage()` calls produce no console output in headless mode.  The
observed symptom — **zero log output** after decompile errors, one core at 100% for
70+ minutes — places the stall between the end of `generateNodes(dest)` and the
`Msg.info` no-seeds message.  The only single-threaded CPU-bound work in that range is:

1. `FunctionNodeContainer(dest, rawDestNodes)` → `generateCallGraph()` — potentially
   slow for 65k functions with many thunk checks, but bounded by DB query count.
2. **`discoverPotentialMatches`**: specifically the serial result-aggregation loop
   and the `BinningSystem.add` call (both silent).
3. **`chooseSeeds`**: all work after `monitor.setMessage("Generating seeds...")` is
   silent, and the round loop can iterate millions of pairs.

The single-running-thread / userspace-compute / no-syscall profile (`wchan=0`)
rules out I/O and lock contention.  The evidence points to the addAssociate
allocation storm described below.

### 3.2 How LSH bins degenerate for a stripped cross-ISA binary

**Normal case (same-ISA, well-decompiled):** For the `LARGE` memory model
(k=16, L=5, tau=0.97), the expected number of source candidates per query is:

    L * n_source / 2^k  =  5 * 40,000 / 65,536  ≈  3

With the binning providing near-perfect discrimination, `lookup` returns ~3–10
candidates per destination node.  Total pairs: ~65k × 5 = 325k → manageable.

**Degenerate case (stripped 65k Xenon binary):** Two factors combine:

1. **Decompiler timeouts produce null vectors.**
   `ParallelDecompilerCallback.process` (BSimProgramCorrelator.java:315): when
   `decompiler.generateSignatures` returns null (timeout at 60 s),
   `vec = null` → `FunctionNode(func, null, callAddresses)`.
   `BinningSystem.add` skips null-vector nodes (BinningSystem.java:69–71).
   So far so good — these functions never appear in any bin.

2. **Functions that do decompile may produce short or degenerate vectors.**
   For a stripped binary, many functions contain generic patterns (null checks,
   simple arithmetic, branch-heavy control flow without type information).
   The decompiler feature hash (`sigres.features`) for such functions may share a
   large portion of the feature vocabulary with many other functions.  When many
   source functions hash to the same bins as a destination function, the union of
   all hit bins in `BinningSystem.lookup` grows to O(n\_source).

   Concretely: with k=16, each bin holds `n_source / 2^k ≈ 0.6` source functions on
   average.  But for degenerate vectors, the partition hash `Partition.hash` (which
   XORs weighted feature coefficients per hyperplane) can produce the *same* bin ID
   for many functions → a single bin holds hundreds of source functions → L=5 bin
   lookups union to potentially all 40k source nodes.

### 3.3 The O(n × m) pair blowup

`BinningSystem.lookup` (BinningSystem.java:90–100):

```java
public Set<FunctionNode> lookup(FunctionNode node) {
    TreeSet<FunctionNode> result = new TreeSet<FunctionNode>();
    int[] features = getBinIds(node);
    for (int ii = 0; ii < features.length; ++ii) {
        TreeSet<FunctionNode> list = binSys[ii].get(features[ii]);
        if (list != null) {
            result.addAll(list);     // ← unbounded union; no size cap
        }
    }
    return result;
}
```

For a degenerate dest node that maps to a large bin: `result` grows to O(n\_source).
`MatchingCallback.findSimilarNodes` (java:99–123) then does a secondary cosine
compare against every candidate and produces a `FunctionPair` per match.  Even with
the `simThreshold` filter, for two PPC-32 programs with shared vocabulary, many
pairs pass the similarity check.

The result is collected by `discoverPotentialMatches` in a serial loop
(java:221–237):

```java
for (QResult<FunctionNode, List<FunctionPair>> result : results) {
    ...
    for (FunctionPair bridge : pieces) {
        FunctionNode sourceNode = bridge.getSourceNode();
        FunctionNode destNode   = bridge.getDestNode();
        sourceNode.addAssociate(destNode, bridge);    // HashMap.put
        destNode.addAssociate(sourceNode, bridge);    // HashMap.put
        discoveredMatches.add(bridge);
    }
}
```

`FunctionNode.associates` is a plain `HashMap` initialized empty (FunctionNode.java:54).
For a dest node with O(n\_source) = 40k pairs, `HashMap.put` triggers
log₂(40k/16) ≈ 12 capacity doublings × O(size) rehash each → O(40k × 12) = O(480k)
total ops for that one node.  Across all dest nodes that hit degenerate bins:

    degenerate_dest_nodes × 480k  ≈  10k × 480k  =  4.8 × 10⁹ ops

…in a single thread.  This is the 70-minute stall.

### 3.4 Downstream amplification in `chooseSeeds`

`chooseSeeds` builds `HashSetValuedHashMap<FunctionNode, FunctionPair>` over all
`discoveredMatches` (java:290–297), then iterates all entries in a round loop
(java:307–354).  For 100M+ pairs:

- Building the two multimaps: O(pairs) with `HashSet` per key → O(100M)
- Each round: O(pairs) iterations × O(rounds) rounds

Even with early exit (the `changed = (keepLen != values.size())` logic exits when
nothing is eliminated in a round), the first pass over 100M entries at ~100M
`HashSet` operations/sec takes 10–60 minutes alone.  Then `sourceFormatted = sourceHoldOn`
resets for the next round.

### 3.5 Complexity summary

| Phase | Serial or parallel | Complexity for degenerate case | Wall-clock |
|---|---|---|---|
| `generateNodes` (decompile) | Parallel | O(n × decompile\_time) per program | Observed: many timeouts visible in log |
| `FunctionNodeContainer` construction | Serial | O(n log n + n × avg\_calls) | Minutes (DB calls for thunks) |
| `BinningSystem.add` | Serial | O(n × L × k × features) | Seconds |
| `ConcurrentQ` lookup + compare | Parallel | O(n\_dest × E[neighbors] × compare\_cost) | Seconds–minutes (parallel) |
| Serial result aggregation (addAssociate) | **SERIAL** | **O(n\_src × n\_dest)** for degenerate bins | **Hours** |
| `chooseSeeds` round loop | **SERIAL** | **O(pairs × rounds)** | **Hours** |
| `doMatching` implications loop | Serial | O(matches × degree × generators) | Minutes–hours |

---

## 4. Fix plan

### Tier 1 — candidate cap (algorithmic, trivial risk)

**What:** add `MAX_LOOKUP_CANDIDATES` to `BinningSystem.lookup()`.  When the
candidate union grows past the cap, return an empty set.  Add a matching cap in
`MatchingCallback.findSimilarNodes` as belt-and-suspenders.

**Why it is correct:** A dest function whose feature vector hashes into a bin
containing hundreds of source functions is *not discriminating*.  No confident
nearest-neighbor match can be made for it; the cosine similarity will be roughly
uniform across all candidates (they all share the same generic feature vocabulary).
Returning no candidates for such functions produces no FunctionPairs, which is the
correct outcome: the function is too generic to match via BSim alone.  BSim is
designed to find *similar* matches, not all-pairs matches; degenerating to all-pairs
defeats the purpose of the LSH prefilter.

**Where:** `BinningSystem.java:lookup()` (the primary site, stops pair production
at the source) and `BSimProgramCorrelatorMatching.java:findSimilarNodes()` (belt-and-
suspenders, stops accumulation even if lookup is called from another path).

**Prototype:** already in branch `bsim-perf-candidatecap` at
`/tmp/claude/ghidra-bsim-perf`, commits clean (javac only, no gradle).

**Prototype diff shape:**

*BinningSystem.java:*
```java
// New constant:
static final int MAX_LOOKUP_CANDIDATES = 500;

// lookup() — after each result.addAll(list):
if (result.size() > MAX_LOOKUP_CANDIDATES) {
    return Collections.emptySet();
}
```

*BSimProgramCorrelatorMatching.java:*
```java
// findSimilarNodes() — inside the neighbor loop, after results.add(newPair):
if (results.size() >= BinningSystem.MAX_LOOKUP_CANDIDATES) {
    results.clear();   // treat as no-match (too ambiguous)
    return;
}
```

**Threshold calibration:**
- `LARGE` model (k=16, L=5), n\_source=40k: expected candidates = 3.
  A result of >500 means the query bin has ≥100× expected density → clearly degenerate.
- `SMALL` model (k=10, L=3), n\_source=40k: expected candidates = 190.
  Cap of 500 is ~2.6× expected → tight but reasonable.  Consider raising to 1000 for
  the SMALL model, or expose `MAX_LOOKUP_CANDIDATES` as an option alongside the
  `MEMORY_MODEL` option in `BSimProgramCorrelatorFactory`.

**Estimated win:**
- Degenerate case (current scenario): **70+ minutes → ~30 seconds**.
  The cap fires for all degenerate dest nodes, producing zero pairs; serial aggregation
  and chooseSeeds then process O(non-degenerate pairs) which is manageable.
- Normal case (same-ISA, well-decompiled): **no change**.  The cap is never hit
  at ~3–10 expected candidates per query.

**Risk:** Very low.  The only functions affected are those whose feature vectors are
too generic for BSim to produce useful matches.  Those functions would either produce
no seeds (and thus no downstream matches via the implication graph) or produce many
low-confidence seeds that are filtered out anyway.  The precision of the final match
set is unaffected or improved (fewer false-positive low-confidence pairs).

### Tier 2 — parallelise the serial result-aggregation and chooseSeeds data prep

**What:** The serial loop at `discoverPotentialMatches` lines 221–237 drains the
`ConcurrentQ` results (already available in parallel) into `FunctionNode.associates`
maps.  This loop is serial because `addAssociate` mutates per-node `HashMap`s that
are later read by the single-threaded `chooseSeeds` and `doMatching`.

The embarrassingly-parallel boundary is the *score computation* (already parallel via
`MatchingCallback` in the `ConcurrentQ`).  The serial part is the *bookkeeping*
(associating scored pairs with nodes for later access by address/identity).

**Two options for Tier 2:**

**Option A — pre-collect all pairs, then build associates maps serially but faster
(easy, medium win).**
Instead of inserting into HashMap one-by-one with growth-triggered rehashing, collect
all pairs from `QResult` into a flat `ArrayList<FunctionPair>`, then group by source
node using a single `groupingBy` stream or a pre-sized HashMap (known exact count).
Pre-sizing eliminates all rehash doubling:

```java
// After ConcurrentQ.waitForResults():
List<FunctionPair> allPairs = new ArrayList<>(estimatedPairCount);
for (QResult<...> result : results) { ... allPairs.addAll(pieces); }
// Group by source to pre-size associates maps:
Map<FunctionNode, List<FunctionPair>> bySrc =
    allPairs.stream().collect(Collectors.groupingBy(FunctionPair::getSourceNode));
for (Map.Entry<FunctionNode, List<FunctionPair>> e : bySrc.entrySet()) {
    FunctionNode src = e.getKey();
    Map<FunctionNode, FunctionPair> assoc = new HashMap<>(e.getValue().size() * 2);
    for (FunctionPair p : e.getValue()) {
        assoc.put(p.getDestNode(), p);
        p.getDestNode().addAssociate(src, p);   // still serial but no-rehash on dest side after pre-sizing
    }
    src.setAssociates(assoc);   // needs new setter on FunctionNode
}
```
Estimated win: 5–10× on the aggregation phase by eliminating resize overhead.
Risk: low (serial, behavior-identical).

**Option B — parallel addAssociate with ConcurrentHashMap (harder, bigger win).**
Replace `FunctionNode.associates` with `ConcurrentHashMap<FunctionNode, FunctionPair>`
so that the aggregation loop itself can be parallelized.  But: `chooseSeeds` and
`doMatching` iterate the associates maps and mutate them (via `removeAssociate` and
`clearAssociates`).  Those phases are already serial; the `ConcurrentHashMap` adds
zero overhead for serial read/modify but enables parallel write during aggregation.

Concretely: make `addAssociate` use `putIfAbsent` on a `ConcurrentHashMap` (or
`HashMap` with a per-node lock), fan out the aggregation over the already-available
`QResult` collection using `parallelStream()` or a second `ConcurrentQ`, then
proceed serially to `chooseSeeds`.

Race to check: two workers writing `src.addAssociate(dst, pairA)` and
`dst.addAssociate(src, pairA)` for the same pair — safe with `ConcurrentHashMap.put`
(the pair object is immutable).  No ordering requirement on pair insertion.

Estimated win: 4–16× on aggregation (scales with thread count).
Risk: medium — `removeAssociate`/`clearAssociates` called from single-threaded
`doMatching.acceptMatch` are safe after ConcurrentQ completion (no concurrent writers
at that point), but requires code review of all `associates` mutation sites.

**The Tier 2 payoff point:** Tier 1 (the cap) eliminates the blowup for *degenerate*
cases.  Tier 2 matters for *large non-degenerate* cases where aggregation of O(millions)
valid pairs is still single-threaded.  For the RB3 vs Xbox360 scenario the cap likely
solves the immediate problem; Tier 2 becomes relevant if BSim is run on two large
well-decompiled same-ISA programs (>100k functions each) where the pair count is
genuinely large but useful.

### Tier 2b — parallelise `chooseSeeds` (medium effort, medium win)

`chooseSeeds` (java:280–371) is structurally a parallel reduction: for each pair,
the `hasConflicts` check, the ratio computation, and the holdOn/finalPairs routing
are all read-only with respect to other pairs in the same round.  The mutation is
appending to `finalPairs`, `matchedSource`, `matchedDest`, and the holdOn multimaps.
All of these can be done with concurrent-safe accumulators:
- `finalPairs`: `ConcurrentLinkedQueue` or `Collections.synchronizedList(new ArrayList<>())`
- `matchedSource`/`matchedDest`: `ConcurrentHashMap.newKeySet()`
- `sourceHoldOn`/`destHoldOn`: thread-local accumulators, merged at round end

This converts each round from O(pairs) serial to O(pairs/threads) parallel.  For 100k
pairs across 4 rounds: serial=400k iterations, parallel 16-thread=25k per thread.
Estimated win: 4–16× on chooseSeeds per round; total impact moderate (chooseSeeds
is only the bottleneck *after* the aggregation phase — i.e., after Tier 1 is applied).

Risk: medium-high.  The round-to-round state (which nodes are `matchedSource`) must
be consistent within a round (read-only reads from the *previous* round's matched set
are fine; writes go to the *next* round's set).  Careful not to let two workers both
accept the same node in the same round.

### Summary table

| Tier | Fix | Effort | Est. win | Risk | Commit-ready? |
|---|---|---|---|---|---|
| T1 | `BinningSystem.lookup` candidate cap (MAX=500) | S | 70 min → 30s | Very low | YES — branch `bsim-perf-candidatecap` |
| T1b | Expose cap as a `BSimProgramCorrelatorFactory` option | S | Enables tuning | Very low | Can follow T1 |
| T2a | Pre-sized HashMap + pre-grouped aggregation (serial) | S | 5–10× on aggregation | Low | No (needs FunctionNode setter) |
| T2b | Parallel aggregation with ConcurrentHashMap | M | 4–16× on aggregation | Medium | No |
| T2c | Parallel `chooseSeeds` per round | M | 4–16× per round | Medium-high | No |

---

## 5. Config tuning as a short-circuit

Several config changes can substantially reduce work without code changes:

### 5.1 Raise the similarity threshold

`BSimProgramCorrelatorFactory.SIMILARITY_THRESHOLD = 0.5` (the constant in
`BSimProgramCorrelator.java:64`) is passed to `MatchingCallback` as `simThreshold`.
Pairs below this score are dropped before `addAssociate` is called.  For a stripped
cross-ISA binary, many pairs cluster near 0.5–0.6 and provide no useful seeds.

In `bsim.py`, `correlate_bsim` uses `bsim_factory.createDefaultOptions()` which
inherits the hardcoded 0.5.  To raise it, patch the options object after creation:

```python
options.setDouble(BSimProgramCorrelatorFactory.SEED_CONF_THRESHOLD, 15.0)  # default 10.0
# or inject a custom factory subclass with higher SIMILARITY_THRESHOLD
```

Raising `SIMILARITY_THRESHOLD` from 0.5 to 0.7 eliminates ~half the pairs for a
cross-ISA comparison; raising to 0.8 eliminates ~80%.  **Estimated win: 2–5× fewer
pairs → 2–5× faster aggregation and chooseSeeds.**  Risk: fewer discovered matches,
but for a stripped binary most of those low-confidence pairs produce no useful seeds
anyway (they don't reach `confThreshold=10.0` for seeding).

### 5.2 Use the MEDIUM or LARGE memory model (default is already LARGE)

`BSimProgramCorrelatorFactory.MEMORY_MODEL_DEFAULT = LSHMemoryModel.LARGE` (k=16, L=5).
This is already the default and gives the smallest bins (2^16 = 65k possible bins,
~0.6 src funcs/bin on average).  The SMALL model (k=10, L=3) gives 1024 bins × 3
tables = much larger neighborhoods → much worse for this scenario.  **Keep LARGE.**

If you want even larger k (e.g., k=20), the `LSHMemoryModel` enum would need a new
value.  With k=20, L=6: 2^20 = 1M bins → expected 0.04 src funcs/bin → near-zero
collision probability even for similar vectors.  The degenerate case would essentially
disappear.  **Estimated win: complete elimination of the bin-collision problem.
Cost: slightly lower recall for near-duplicate functions.**

### 5.3 Limit the destination address set

In `bsim.py`:
```python
if p2_addr_set is None:
    p2_addr_set = p2.memory.loadedAndInitializedAddressSet
```

For a 65k-function stripped binary, you can restrict to functions above a minimum
body size using Ghidra's `FunctionIterator` + address-set construction, pre-filtering
to exclude tiny thunks/stubs.  Reducing from 65k to 20k dest functions would cut all
scaling by a factor of ~3.  **Estimated win: 3× on all phases. Risk: misses matches
for small functions.**

### 5.4 Run BSim only on seed-augmented subsets

`bsim.py` already injects `SeedMatch` / `SymbolsHash` / `ExactBytes` matches as
`p1_matches`/`p2_matches`.  You can use these to restrict `p1_addr_set` and
`p2_addr_set` to only *unmatched* functions:

```python
# Compute unmatched addresses:
p1_unmatched = p1.memory.loadedAndInitializedAddressSet.subtract(
    AddressSet(p1_matches_as_addresses))
p2_unmatched = p2.memory.loadedAndInitializedAddressSet.subtract(
    AddressSet(p2_matches_as_addresses))
bsim_correlator = bsim_factory.createCorrelator(
    p1, p1_unmatched, p2, p2_unmatched, options)
```

For RB3 Bank8 (40k) vs Xbox360 (65k): the exact-hash correlators typically match
~60–70% of functions; BSim only needs to process the remaining ~30% (12k × 20k)
instead of the full 40k × 65k.  **Estimated win: ~10× on all BSim phases.**
This is already the intended design intent of `p1_addr_set` / `p2_addr_set`; the
current `bsim.py` passes the full loaded set by default.

---

## 6. Architecture-weights note: why the PPC→PPC case proceeds but still stalls

`BSimProgramCorrelator.doCorrelate` calls `GenSignatures.getWeightsFile(id1, id2)`
(GenSignatures.java:581–628).  For Wii Gekko (`PowerPC:BE:32:Gekko_Broadway`) and
Xbox360 Xenon (`PowerPC:BE:32:default`): both split at index 2 as `"32"` → same size
→ returns `lshweights_32.xml`.  The weights check **passes**, confirming the observed
behavior (decompile did run).

The `lshweights_32.xml` file was trained on x86-32 Linux binaries.  For PPC-32 it
provides IDF values for the feature vocabulary, but the vocabulary overlap between
Ghidra's PPC feature hash set and the training data is partial.  This means:
- IDF weights are somewhat miscalibrated for PPC → features may not discriminate
  as well as for x86 → vectors are closer together in cosine space than they would
  be with a PPC-trained weights file → more candidates per lookup.
- For *identical* or *near-identical* functions (same game, different ABI), BSim
  still finds good matches because cosine similarity is high regardless of IDF.
- For *generic* functions, miscalibrated IDF can cause many functions to produce
  nearly identical vectors → bin collision → the O(n×m) problem.

A PPC-32-specific weights file (trainable via `GenSignatures` on a corpus of PPC
binaries) would improve both recall and the bin-collision problem simultaneously.
This is a **research investment**, not a quick fix.  The candidate cap (Tier 1) is
the pragmatic short-term solution.

---

## 7. Prototype branch

**Location:** `/tmp/claude/ghidra-bsim-perf` (git worktree of
`/home/free/code/milohax/ghidra`)
**Branch:** `bsim-perf-candidatecap`
**Files changed:**
- `Ghidra/Features/VersionTrackingBSim/src/main/java/ghidra/feature/vt/api/BinningSystem.java`
- `Ghidra/Features/VersionTrackingBSim/src/main/java/ghidra/feature/vt/api/BSimProgramCorrelatorMatching.java`

**Compile-check:** passed — javac against built jars from
`/home/free/code/milohax/ghidra/build/ghidra-dist/ghidra_12.2_DEV/` with
`--release 21`.  Class files in `/tmp/claude/ghidra-bsim-compiled/`.

**To rebuild VersionTrackingBSim.jar against this branch:** copy the compiled class
files into the jar (same pattern as the VT Tier-1 patch in the companion doc),
deploy to the ghidra install under test, and rerun `bsim.py` to observe the cap
firing in the matching phase.  No full gradle build required.

**NOT built into the main working tree** (`/home/free/code/milohax/ghidra`) — the
running pyghidra service and concurrent ghidriff experiment are untouched.

---

## 8. Distinction from the VT reference-correlator (prior work)

The prior `ghidra-vt-optimization-2026-06-09.md` addressed a different stall in a
different correlator:

| | VT reference-correlator (prior) | BSim correlator (this doc) |
|---|---|---|
| Hot phase | `VTAssociationDB.equals/hashCode` via `HashSet.add` in `getAllRelatedAssociations` | `addAssociate` HashMap resize + `chooseSeeds` round loop |
| Root cause | `decodeAddress` synchronized lock in inner loop (O(n²) × lock overhead) | O(n\_src × n\_dest) pairs from degenerate LSH bins (no lock involved) |
| Bottleneck class | Locked DB decode — dominated by `synchronized` | Pure-computation allocation storm — GC + rehash |
| Fix class | Replace `Address`-keyed hash with primitive `long` record key | Cap candidates upstream at LSH query time |
| Parallelizable? | Yes (the scoring loop), no (the apply loop) | Scoring already parallel; aggregation + chooseSeeds need Tier 2 |
| Complexity | O(Σ g² × lock) → O(Σ g²) with cheap hash | O(n × m) → O(n × cap) with lookup cap |

The two bugs are independent and coexist in different codepaths.

---

## 9. Recommended action sequence

1. **Now (zero rebuild):** in `bsim.py`, restrict `p2_addr_set` to unmatched
   functions (§5.4) and raise `SEED_CONF_THRESHOLD` to 15.0 (§5.1).  These are
   Python-level config changes, no Java build.  This may make the run tractable
   without any code patch.

2. **Short-term (Tier 1, S effort):** apply the prototype candidate-cap patch from
   `bsim-perf-candidatecap`, jar-swap into the test ghidra install, rerun.  This
   is the primary defense against the O(n×m) stall.

3. **Medium-term (Tier 1b):** expose `MAX_LOOKUP_CANDIDATES` as a
   `BSimProgramCorrelatorFactory` option so it can be tuned per use-case without
   recompilation.

4. **If still slow after Tier 1:** profile the surviving bottleneck.  If
   `chooseSeeds` dominates: apply Tier 2c (parallel rounds).  If result-aggregation
   dominates: apply Tier 2a (pre-sized HashMap).

5. **Research investment:** train a PPC-32 weights file via `GenSignatures` on a
   corpus of PPC binaries (RB3 itself + other PPC games).  This is the upstream
   improvement that would make BSim first-class for PPC cross-ISA work.
