# Ghidra Version Tracking — Tier-2 concurrency design + prototype plan

**Date:** 2026-06-09
**Status:** Design + precise implementation plan (NOT a full implementation).
**Builds on:** [ghidra-vt-optimization-2026-06-09.md](./ghidra-vt-optimization-2026-06-09.md)
(Tier-1 = the surgical `VTAssociationDB.equals/hashCode` record-key fix, the *live* fix).

**Context:** RB3 decomp drives Ghidra VT headless (`analyzeHeadless` + our postScript
`tools/ghidra/RB3AutoVersionTrackingScript.java` → `AutoVersionTrackingTask`) to port
Bank-5 DWARF markup onto the Bank-8 target. Two PowerPC programs, ~40,000 functions each.
Tier-1 un-pins the apply phase's `synchronized decodeAddress` storm; **Tier-2 attacks the
remaining wall-clock**, which after Tier-1 is dominated by the single-threaded **correlation
+ scoring** phases that today run on one core while the other ~N-1 cores idle.

All file:line citations are against the fork tree at `/home/free/code/milohax/ghidra`.
Source root for VT is
`Ghidra/Features/VersionTracking/src/main/java/ghidra/feature/vt/`.

---

## 0. TL;DR

- The expensive, parallelizable work in VT is **read-only over the two programs**:
  function hashing (`MatchFunctions.matchFunctions`, MatchFunctions.java:55-71) and —
  the bigger prize — the **reference correlator's O(dest×src) cosine-similarity scoring**
  (`VTAbstractReferenceProgramCorrelator.findDestinations`,
  VTAbstractReferenceProgramCorrelator.java:121-163).
- The cheap-but-unparallelizable work is **DB mutation**: `createMatchSet`/`addMatch`/
  accept/block/apply-markup, all funneled through one `ghidra.util.Lock` shared from
  `session.getLock()` (AssociationDatabaseManager.java:69) inside one VT transaction
  (`AutoVersionTrackingTask.run`, AutoVersionTrackingTask.java:115-127). This is
  single-writer **by construction** and stays serial.
- **Architecture:** a *stateless parallel scoring phase* over immutable per-function
  **snapshots** (plain value objects: `long` keys, hashes, feature vectors — no `Address`,
  no `Program`, no `DBRecord`, no lock) emitting plain `CandidateMatch` value objects,
  followed by a *single-threaded serial commit phase* that drains candidates and writes
  them through the existing locked `addMatch` path in deterministic order. Reuse Ghidra's
  own `generic.concurrent.ConcurrentQ` + `GThreadPool` (the machinery `ParallelDecompiler`
  already uses) for the fan-out; do not invent a framework.
- **Highest-ROI first step:** parallelize **only** the `findDestinations` scoring loop of
  the reference correlator (snapshot the two LSH-vector maps to arrays, score the outer
  loop in a `ConcurrentQ`, collect `VTMatchInfo` to a list, `addMatch` them serially).
  It is the single biggest CPU sink at 40k functions, it is a pure nested-loop over
  precomputed vectors, and it is a self-contained ~1-file change.

---

## 1. Where the time actually goes (and why each phase is/ isn't parallelizable)

`AutoVersionTrackingTask.doRun` runs a fixed sequence of correlators
(AutoVersionTrackingTask.java:130-330): exact-symbol → exact-data →
exact-function-bytes → exact-function-instructions → exact-mnemonics →
**duplicate-function** → reference correlators (data-ref / function-ref / combined). Each
is `correlateAndPossiblyApply` = `correlator.correlate(...)` then accept+apply.

| # | Phase | Code (file:line) | Cost driver | Parallelizable? |
|---|---|---|---|---|
| 1 | Exact function hashing | `MatchFunctions.matchFunctions` hash loops (MatchFunctions.java:55-71); `hashFunction` (158-170) | Per-function `hasher.hash()` reads bytes/instructions; serial today | **Yes** — pure fn of function body; emit `(hash, entryKey, side)` |
| 2 | Bucketing identical hashes | MatchFunctions.java:79-97 | Map build + `k²` cross-product per bucket | Partial — fan-out hashes; fan-in into a merge map. Cross-product itself is trivial CPU (Tier-1b caps `k²`) |
| 3 | `FunctionMatch` scoring | `generateMatchFromMatchedFunctions` (FunctionMatchProgramCorrelator.java:85-117) | Per-match `getFunctionAt` + body length read | **Yes** (read-only) but low ROI — cheap |
| 4 | **Reference correlator scoring** | `findDestinations` nested loop (VTAbstractReferenceProgramCorrelator.java:121-163) + `transform` (178-237) | **O(dest×src)** `LSHCosineVectorAccum.compare` dot products | **Yes — biggest win.** Pure math over precomputed vectors |
| 5 | Reference feature extraction | `extractReferenceFeatures` (383-457), `accumulateFunctionReferences` (291-354) | Reference-graph walk (pointer chasing) | Partially — per-accepted-match walks are independent reads, but graph-walk is latency-bound; medium ROI |
| 6 | Dup-function operand map build | `createFunctionsMap` / `mapFunctionScalarAndAddressOperands` (AutoVersionTrackingTask.java ~979-1294) | Per-function operand read → plain `Map` | **Yes** (read-only, produces value maps); measure after Tier-1 |
| 7 | Create match sets / associations | `VTMatchSetDB.addMatch` (VTMatchSetDB.java:160-183) → `getOrCreateAssociationDB` (AssociationDatabaseManager.java:201-230) | DB write under `lock.acquire()` | **No** — single-writer |
| 8 | Accept + block-related + apply markup | `setAssociationAccepted`, `ApplyMarkupItemTask.run`; accept *blocks* conflicting related associations | Order-dependent DB mutation | **No** — single-writer + order-dependent |

**Conclusion:** rows 1, 4, 6 are the parallel surface; row 4 is the dominant CPU cost on
40k functions. Rows 7-8 are the irreducible serial commit.

### 1.1 Why rows 7-8 *cannot* be parallelized (the fundamental constraint)

Every VT write path funnels through one lock and one transaction:

- `AssociationDatabaseManager` takes its lock from the session:
  `lock = session.getLock();` (AssociationDatabaseManager.java:69). Every mutator —
  `getOrCreateAssociationDB` (201-230, `lock.acquire()` at 217), `setAssociationAccepted`,
  `blockRelatedAssociations`, `updateAssociationRecord` — runs under it.
- `VTMatchSetDB.addMatch` calls `getOrCreateAssociationDB` then `lock.acquire()` to insert
  the match record (VTMatchSetDB.java:162-178).
- `VTSessionDB.createMatchSet` is itself locked (VTSessionDB.java:514-521).
- The whole AutoVT run is one transaction opened in `run`
  (`session.startTransaction(NAME)`, AutoVersionTrackingTask.java:115; `endTransaction`
  at 126).

Underneath, the address map is `synchronized` (the Tier-1 `decodeAddress` finding),
encoding a *new* address can mutate the shared base-address table, and the DB is a
single-writer B-tree with transactional invariants. N threads mutating it concurrently
would corrupt the B-tree / address map and violate transaction atomicity. **This
serialization is fundamental, not incidental.** Therefore the commit phase is serial. The
leverage is that the *expensive proposal* work (scoring) is read-only and can move
off-thread; only the final mutation is serialized.

---

## 2. The actor / functional split

Two phases per parallelized correlator. The boundary between them is the only place that
matters for correctness, and it is drawn so that **workers share no mutable state and touch
no DB**, which makes the worker phase race-free by construction.

```
                          (calling thread, one DB read pass)
   Program A ─┐
              ├─►  SNAPSHOT BUILD  ──►  FunctionSnapshot[]      (immutable value objects)
   Program B ─┘                              │
                                             ▼
                        ┌─────────── PHASE P: PARALLEL SCORING ───────────┐
                        │  ConcurrentQ<WorkItem, List<CandidateMatch>>     │
                        │  workers: pure fn(snapshot[]) -> candidates      │
                        │  NO Program, NO Address, NO DBRecord, NO lock    │
                        └───────────────────┬─────────────────────────────┘
                                             │  results stream to one consumer
                                             ▼
                        ┌─────────── PHASE C: SERIAL COMMIT ──────────────┐
                        │  single thread, existing VT transaction + lock   │
                        │  sort candidates deterministically               │
                        │  matchSet.addMatch(...)  (locked, single-writer) │
                        │  accept / block / apply markup (order-dependent) │
                        └──────────────────────────────────────────────────┘
```

### 2.1 The immutable snapshot record (defined precisely)

Built once, on the calling thread, in a single read pass over each program **before**
fan-out. Every field is a primitive or an array of primitives — deliberately **no**
`Address`, `Function`, `Program`, `DBRecord`, or `LSHCosineVectorAccum`-with-back-refs.

```java
/** Immutable, thread-safe; one per function per program. No DB, no Address. */
record FunctionSnapshot(
    long   entryKey,        // encoded address key as stored in the DB (the long the
                            // AddressMap already keeps). Pre-encoded on the calling thread
                            // via AddressMap.getKey(addr, false) — workers never encode.
    int    bodyLength,      // function.getBody().getNumAddresses()
    long   bodyHash,        // FunctionHasher.hash(function): byte / instr / mnemonic hash
    int[]  mnemonicNGram,   // mnemonic n-gram histogram (for similarity / LSH), optional
    long[] refOutKeys,      // outgoing reference target entryKeys, as longs
    int    side             // 0 = source program, 1 = destination program
) {}
```

For the reference correlator the per-function feature vector is its own immutable snapshot:

```java
/** Immutable view of one function's LSH feature vector. */
record VectorSnapshot(
    long   entryKey,        // pre-encoded address key
    int[]  featureIds,      // sorted; from LSHCosineVectorAccum entries
    double[] weights        // parallel to featureIds
) {}
```

`LSHCosineVectorAccum.compare(...)` (used at
VTAbstractReferenceProgramCorrelator.java:144) operates purely on these
`(featureIds, weights)` arrays — it is already a pure function of two vectors. We snapshot
the live `Map<Address, LSHCosineVectorAccum>` (`srcVectorsByAddress`/`destVectorsByAddress`,
lines 54-55) into `VectorSnapshot[]` arrays **after** `extractReferenceFeatures` has fully
populated them (so the maps are read-only at fan-out), then never touch the maps again.

### 2.2 The candidate value object (plain, DB-free)

Workers emit candidates as immutable values; **no** `VTAssociationDB`, **no** markup, **no**
DB row:

```java
record CandidateMatch(
    long   srcEntryKey,     // address keys (longs) — resolved to Address only in Phase C
    long   dstEntryKey,
    double similarity,
    double confidence,
    int    srcLength,
    int    dstLength,
    VTAssociationType type  // FUNCTION / DATA — an enum, immutable
) {}
```

In Phase C the committer turns `srcEntryKey`/`dstEntryKey` back into `Address` *once* (one
locked `decodeAddress` per accepted candidate, not per comparison) and builds the existing
`VTMatchInfo` (VTMatchInfo.java — plain setters at the `setSource/Destination*` methods)
to feed the **unchanged** `matchSet.addMatch(info)` path (VTMatchSetDB.java:160). The
commit code is byte-for-byte the existing loop (e.g.
FunctionMatchProgramCorrelator.java:76-82, or
VTAbstractReferenceProgramCorrelator.java:156-160) — we only move the *production* of the
infos off-thread.

### 2.3 Why the worker phase has zero races

Each worker is a pure function `snapshot[] -> List<CandidateMatch>`. It reads immutable
arrays (published safely: built on the calling thread, handed to `ConcurrentQ.add` which
crosses a happens-before via the queue lock at ConcurrentQ.java:264-273, and `record`
fields are final) and writes only its own local result list (returned, never shared). There
is no shared mutable state to race on, no DB handle, no `AddressMap`. The result consumer
appends to a single collection — and `ConcurrentQ`'s `collectResults` / `QItemListener`
delivery is the framework's job (see `DecompilerConcurrentQ.InternalResultListener`,
DecompilerConcurrentQ.java:157-174), so even result aggregation needs no client-side lock.

---

## 3. Race conditions a *naive* parallelization would hit — and how the boundary removes each

A naive "just put `@Override doCorrelate` work on threads" would hit all of these. The
snapshot+commit boundary removes each one structurally (not with a band-aid lock):

1. **Concurrent B-tree / address-map corruption.** If workers called
   `addMatch`/`getOrCreateAssociationDB`/`setAccepted` directly they would race on the
   single-writer session B-tree and the `synchronized` `AddressMapDB`
   (the writes at VTMatchSetDB.java:167, AssociationDatabaseManager.java:217). *Removed:*
   workers emit `CandidateMatch` values only; **all** DB writes happen in serial Phase C on
   one thread.

2. **`AddressMap.getKey(addr, true)` mutating shared base-address state.** Encoding a
   *previously-unseen* address can append to the address-map's base table (the same map the
   Tier-1 analysis flagged as `synchronized`). Concurrent encodes would race. *Removed:*
   snapshots pre-encode **every** key on the calling thread with the non-mutating
   `getKey(addr, false)` form before fan-out; workers only *compare* existing `long`s and
   never encode.

3. **Accept-order dependence (the subtle one).** `setAssociationAccepted` **blocks**
   conflicting related associations, so the final applied set depends on the order accepts
   happen (`AssociationDatabaseManager` block/unblock related logic). Parallel accepts would
   make results non-deterministic *and* could double-apply conflicting markup. *Removed:*
   accept/block/apply stays entirely in serial Phase C; parallelism is confined to the
   *score computation that proposes* matches, never to acceptance. Phase C sorts candidates
   by `(srcEntryKey, dstEntryKey)` before committing so the run is reproducible — important
   for a markup port we re-run.

4. **`DBObjectCache` / lazy `record` refresh races.** `VTAssociationDB.refresh` can swap the
   underlying `record`, and the manager's `DBObjectCache` holds soft refs
   (AssociationDatabaseManager.java:70). Touching `VTAssociationDB`/`DBRecord` off-thread
   would race the cache + refresh. *Removed:* workers hold only immutable snapshots and
   never see a `VTAssociationDB` or `DBRecord`.

5. **`FunctionManager` / `Listing` iterator non-thread-safety.** `getFunctionAt`,
   `getFunctions(...)`, `ReferenceIterator` are program-DB reads that are **not** guaranteed
   concurrent-safe and may share cursors. *Removed:* all `Listing`/`FunctionManager` reads
   happen during the single-threaded snapshot pass; workers see only arrays. (This is the
   same discipline `ParallelDecompiler` follows — it snapshots the function list up front
   and hands `Function`/decompiler-input to workers that each own a private `DecompInterface`,
   never sharing a program cursor; ParallelDecompiler.java fan-out via `DecompilerConcurrentQ`.)

6. **`LazyMap` populate-on-get races.** `srcVectorsByAddress`/`destVectorsByAddress` are
   `LazyMap.lazyMap(...)` (VTAbstractReferenceProgramCorrelator.java:386-387) — a `get` of a
   missing key *mutates* the map. Workers calling `.get()` would race. *Removed:* the
   snapshot is taken after population is complete, into plain arrays; workers index arrays,
   never call `LazyMap.get`.

---

## 4. Reuse Ghidra's own `ConcurrentQ` (concrete config)

The VT module uses **none** of Ghidra's parallel machinery today (no `ConcurrentQ` import
anywhere under `Features/VersionTracking/src/main`). Auto-analysis and the decompiler
already parallelize this exact fan-out/fan-in shape, so we copy their recipe rather than
invent one.

**Reference pattern** (`DecompilerConcurrentQ`, DecompilerConcurrentQ.java:60-70):

```java
queue = new ConcurrentQBuilder<I, R>()
    .setCollectResults(collectResults)
    .setThreadPool(pool)                 // GThreadPool, often the shared analysis pool
    .setMonitor(monitor)
    .setListener(new InternalResultListener())   // streams each R to a consumer
    .build(callback);                    // QCallback<I,R>.process(item, monitor)
```

It obtains the pool via `AutoAnalysisManager.getSharedAnalsysThreadPool()`
(DecompilerConcurrentQ.java:52; AutoAnalysisManager.java:1409-1413 → sized to
`number of processors + 1` by default in `GThreadPool`). In **headless** AutoVT there is no
`AutoAnalysisManager` tool wired up, so use a private pool by name instead — `ConcurrentQBuilder`
will create one (`getThreadPool()` → `GThreadPool.getPrivateThreadPool(name)`,
ConcurrentQBuilder.java:212-222).

### 4.1 Concrete builder config for the reference scorer (Phase P, row 4)

- **Queue item type `I`**: `VectorSnapshot` (one destination function's vector) — or, to
  bound memory and improve cache behavior, a `DestChunk` (`int from, int to`) indexing the
  destination array, so each work item scores a *block* of destinations against all sources.
- **Result type `R`**: `List<CandidateMatch>` for that item.
- **Callback `QCallback<DestChunk, List<CandidateMatch>>`**: for each dest in the chunk, loop
  all source `VectorSnapshot`s, compute `LSHCosineVectorAccum.compare` (reconstruct a
  transient accum from the arrays, or score the arrays directly), apply the existing
  `transform`/`refine` thresholds (VTAbstractReferenceProgramCorrelator.java:178-280) over
  immutable inputs, return candidates. **No** `matchSet.addMatch` inside.
- **Builder:**

```java
GThreadPool pool = GThreadPool.getPrivateThreadPool("VT Reference Correlator");
// pool.setMaxThreadCount(n)  // optional; defaults to #cpus+1

List<CandidateMatch> collected = Collections.synchronizedList(new ArrayList<>());
QCallback<DestChunk, List<CandidateMatch>> callback =
    (chunk, mon) -> scoreChunk(chunk, srcSnaps, dstSnaps, options, mon);

ConcurrentQ<DestChunk, List<CandidateMatch>> q =
    new ConcurrentQBuilder<DestChunk, List<CandidateMatch>>()
        .setThreadPool(pool)
        .setMonitor(monitor)                         // cancellation propagates per QCallback
        .setCollectResults(false)                    // we stream via the listener instead
        .setListener((QItemListener<DestChunk, List<CandidateMatch>>) result -> {
            List<CandidateMatch> r = result.getResult();
            if (r != null) collected.addAll(r);
        })
        .build(callback);

q.add(destChunks);          // ConcurrentQ.add(Collection) — ConcurrentQ.java:264
try { q.waitUntilDone(); }  // ConcurrentQ.java:426
finally { q.dispose(); }    // ConcurrentQ.java:577

// ---- PHASE C (serial, on this thread, inside the existing transaction) ----
collected.sort(Comparator.comparingLong(CandidateMatch::dstEntryKey)
                         .thenComparingLong(CandidateMatch::srcEntryKey));
for (CandidateMatch c : collected) {
    VTMatchInfo info = toMatchInfo(matchSet, c);   // decode keys -> Address once, set fields
    matchSet.addMatch(info);                       // existing locked single-writer path
}
```

This mirrors `DecompilerConcurrentQ`'s blocking `addAll`+`waitUntilDone`/`dispose` lifecycle
(DecompilerConcurrentQ.java:72-94, 120-127) and inherits its `TaskMonitor`-threaded
cancellation (each `QCallback.process` gets a monitor; `monitor.checkCancelled()` already
sits in the inner loops at VTAbstractReferenceProgramCorrelator.java:128, 194).

### 4.2 Chunking guidance

- Outer = destinations (~40k). Inner = all sources (~40k). A per-destination work item is
  fine (40k items, ~5-15µs each → good load balance), but a `DestChunk` of e.g. 64–256
  destinations amortizes queue overhead and keeps the source-array hot in L2. Start with
  per-destination items (simplest), switch to chunks only if queue overhead shows in a
  profile.
- Keep `collectResults(false)` + a streaming `QItemListener` so peak memory is bounded by
  candidates-in-flight, not all `QResult`s retained (matches the decompiler's streaming
  `process(...)` mode, DecompilerConcurrentQ.java:91-94).

---

## 5. Where the Tier-1 record-key change helps the parallel design

The Tier-1 fix (make `VTAssociationDB.equals/hashCode` compare the primitive DB record key
/ stored address `long`s instead of decoding `Address` under a lock) is not just the live
fix — it is **load-bearing for Tier-2's result-dedup**:

- Phase C must dedup candidates that several correlators (or several dest chunks) propose
  for the same `(source, dest)` pair before/while it commits. With Tier-1, association
  identity is a **lock-free, decode-free `long` compare** — the KEY INVARIANT is that
  `getOrCreateAssociationDB` returns exactly one record per `(source, dest)`
  (AssociationDatabaseManager.java:201-230), so the record key is strictly 1:1 with
  `(source, dest)` within a session. Phase C can therefore dedup on
  `(srcEntryKey, dstEntryKey)` longs (in the worker output) and on association `getKey()`
  (after commit) **without** taking the session lock or hitting `synchronized
  decodeAddress` per comparison. Without Tier-1, the commit-side dedup would re-introduce
  the very O(n²) locked-decode storm we eliminated.
- It also means the worker's `CandidateMatch` identity (`srcEntryKey`,`dstEntryKey` longs)
  is the *same* identity the DB uses post-commit — no translation layer, no
  `Address`-object churn across the phase boundary.

In short: **Tier-1 gives us a primitive, lock-free notion of association identity, which is
exactly what a parallel producer + serial deduping committer needs.** Ship Tier-1 first; it
is a precondition for Tier-2 being clean.

---

## 6. Per-correlator effort + ROI

Effort = S(≤0.5d) / M(1-2d) / L(3-5d) / XL(>1wk), including tests.

| Correlator / phase | Parallel? | Effort | ROI | Notes |
|---|---|---|---|---|
| **Reference scoring** `findDestinations` (VTAbstractReferenceProgramCorrelator.java:121-163) | Yes | **M** | **Highest** | O(dest×src) pure vector math; self-contained; the first thing to do |
| Reference feature extraction `extractReferenceFeatures` (383-457) | Partial | L | Medium | Graph walk is latency-bound; parallelize per-accepted-match ref walks if it shows in a profile |
| Function hashing `MatchFunctions.matchFunctions` (55-71) | Yes | M | Medium | Pure per-fn hash; helps but exact phases are already fast post-Tier-1 |
| Dup-function operand map `createFunctionsMap` (AutoVersionTrackingTask.java ~979-1021) | Yes | S-M | Low-Med | Read-only per fn → value maps; measure after Tier-1 (Tier-1 likely removes its dominance) |
| `FunctionMatch` scoring (FunctionMatchProgramCorrelator.java:85-117) | Yes | S | Low | Cheap per-match; only worth it if it shows up |
| Match-set / association create (VTMatchSetDB.java:160) | **No** | — | — | Single-writer; stays serial |
| Accept / block / apply markup | **No** | — | — | Single-writer + order-dependent; stays serial |

---

## 7. Phased rollout (cherry-pickable, upstreamable, layered on Tier-1)

Each phase is an independent commit in the fork that drops a rebuilt `VersionTracking.jar`
into `/opt/ghidra/.../VersionTracking/lib/` (same 12.1 version → ABI-compatible). No phase
depends on a later one; each is individually revertable.

- **Phase 0 — Tier-1 lands first (precondition).** `VTAssociationDB.equals/hashCode`
  record-key change + `getAllRelatedAssociations` long-dedup. (Detailed in the Tier-1 doc.)
  This is the live fix and the identity foundation for Phase 2's commit-side dedup.

- **Phase 1 — Extraction scaffolding (no behavior change, S-M).** Introduce
  `FunctionSnapshot`/`VectorSnapshot`/`CandidateMatch` records and a `snapshot(...)` builder,
  plus a `commitCandidates(matchSet, candidates)` serial committer that reproduces the
  *current* `addMatch` loop. Refactor `findDestinations` to call snapshot → (still serial)
  score → commit. **Verify byte-identical match output** vs the stock jar on the RB3 pair
  before adding any threads. This is the safety net: it proves the snapshot captures
  everything the scorer needs while still single-threaded.

- **Phase 2 — Parallelize reference scoring (M, highest ROI).** Wrap the Phase-1 scoring
  loop in a `ConcurrentQ` per §4.1. Commit stays serial + deterministic-sorted. Gate behind
  a system property (e.g. `-Dvt.parallel.scoring=true`, default on in our postScript, default
  off for an upstream PR) so it is trivially A/B-testable and revertable. **This is the
  recommended first implementation step.**

- **Phase 3 — Parallelize function hashing (M, optional).** Fan out
  `MatchFunctions.matchFunctions` hashing (rows 1-2) via `ConcurrentQ`, merging per-thread
  hash maps. Only pursue if hashing shows in a post-Phase-2 profile.

- **Phase 4 — Parallelize feature extraction / operand maps (L, measure-first).** Only if
  profiling after Phases 2-3 still shows these dominating. Likely unnecessary for our run.

**Upstreamability:** Phases 1-2 are a clean, self-contained refactor of one correlator that
introduces no new dependency (uses Ghidra's own `ConcurrentQ`), preserves output bit-for-bit
(deterministic sort), and keeps the commit serial — exactly the shape upstream already
accepts for `ParallelDecompiler`. The property gate lets upstream default it off pending
their own benchmarking. Each phase is a focused commit; nothing forces a big-bang merge.

---

## 8. Prototype validation plan (throwaway project; do NOT touch the live RB3 project)

1. **Build the fork module** (warm tree, deps fetched). Use a real JDK, not the JRE:
   ```bash
   JAVA_HOME=/usr/lib/jvm/java-26-openjdk \
     /home/free/code/milohax/ghidra/gradlew \
     -p /home/free/code/milohax/ghidra :VersionTracking:jar
   ```
   (A JRE-only `JAVA_HOME=/usr/lib/jvm/java-21-openjdk` fails with "does not provide
   JAVA_COMPILER". The benign `PowerPC:BE:64:Xenon previously defined` WARN at headless
   startup is unrelated — ignore it.)
2. **Drop the rebuilt jar** into the runtime Ghidra that pyghidra/headless loads:
   `/opt/ghidra/Ghidra/Features/VersionTracking/lib/VersionTracking.jar` (12.1 ↔ 12.1,
   ABI-compatible).
3. **Benchmark in a throwaway project under `/tmp`** — never
   `/home/free/code/milohax/rb3/ghidra_projects/RB3`, and never against the running
   pyghidra service on `:8001`. Import the two ELFs into a fresh `/tmp/vt-bench-<ts>`
   project, run the postScript headless, and compare to the stock-jar baseline on:
   - **wall-clock** of the reference-correlator phase (expect ~#cpu× speedup on row 4);
   - **match-set equality**: dump `(srcEntryKey, dstEntryKey, type, score)` sorted, diff
     against the serial baseline → must be **identical** (deterministic-sort invariant);
   - **no new warnings/errors** in the headless log beyond the known Xenon WARN.
4. **A/B via the property gate**: same project, `-Dvt.parallel.scoring=false` vs `true`,
   confirm identical match output and the expected wall-clock delta.

**Git hygiene:** commits in the fork repo only; stage only files changed there; **no
`Co-Authored-By` lines**. The rb3 repo gets only this doc.

---

## 9. Explicitly NOT in Tier-2

- **GPU offload** — ruled out in the Tier-1 doc §5: no numeric kernel large enough at 40k
  functions to amortize transfer/JNI cost; LSH already collapses the cross-product to
  near-neighbors; the remaining bottleneck is lock contention (Tier-1) and association
  count (Tier-1b), not arithmetic intensity.
- **Parallelizing the commit / accept / apply-markup path** — single-writer + transactional
  + accept-order-dependent. Stays serial by design; that is the whole point of the
  snapshot→commit boundary.
- **Concurrent program-DB reads inside workers** — forbidden; all `Listing`/`FunctionManager`/
  `ReferenceIterator` access is confined to the single-threaded snapshot pass.
```
