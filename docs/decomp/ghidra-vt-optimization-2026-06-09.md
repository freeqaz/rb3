# Ghidra Version Tracking (VT) optimization — design / feasibility report

**Date:** 2026-06-09
**Context:** RB3 decomp drives Ghidra VT headless to port Bank 5 DWARF markup (types,
signatures, comments, labels) onto the Bank 8 target. Two PowerPC programs,
~40,000 functions each, run via `analyzeHeadless` + our own postScript
`tools/ghidra/RB3AutoVersionTrackingScript.java` → `AutoVersionTrackingTask`.
We maintain a Ghidra fork, so fixes can land either upstream-in-VT **or** in our
postScript/correlator.

**Live symptom:** single thread pinned at 100% of one core for 90+ min, stuck in
the Duplicate-Function apply phase in a near-O(n²) loop. `jstack` shows
`VTAssociationDB.equals()` → `getSourceAddress()` → `AddressMapDB.decodeAddress()`
(a `synchronized` method) being called from `HashSet.add`/`addAll` inside
`AutoVersionTrackingTask.getAllRelatedAssociations`. `jstat`: CPU-bound, not
GC-bound (old gen ~24%, 0 full GCs, 38s GC / 90 min). 40 GB RSS is just
`-Xmx48G` committed-but-unreclaimed pages; live set is small.

All file:line citations below are against the tree at `/home/free/code/milohax/ghidra`.

---

## 1. Executive summary

There are two compounding problems, and the cheap fix kills the live one:

1. **Quadratic *number* of associations.** The Duplicate-Function correlator
   emits one association per `(source, dest)` pair *within each identical-code
   hash group*. For RB3, the Bank 5 / Bank 8 programs contain large groups of
   byte-identical tiny thunks/stubs, so a group of `k` identical functions on
   each side produces ~`k²` associations
   (`MatchFunctions.matchFunctions`, MatchFunctions.java:88-95).

2. **Each association comparison is needlessly expensive.** `VTAssociationDB`'s
   `equals`/`hashCode` decode the stored address keys back into `Address`
   objects through `AddressMapDB.decodeAddress`, which is `synchronized`
   (AddressMapDB.java:495,509). The apply phase builds `HashSet<VTAssociationDB>`
   over the quadratic association set
   (`getAllRelatedAssociations`, AutoVersionTrackingTask.java:825-847), so every
   `HashSet.add` triggers ≥1 lock-taking DB decode. Quadratic set-build ×
   per-element locked decode = the pinned core.

**Recommended path (in order):**

- **Tier 1 (do first — S, ~1-2 hr, very low risk):** make
  `VTAssociationDB.equals`/`hashCode` compare the association's **primitive DB
  record key** (`getKey()`) instead of decoded addresses, and dedup
  `getAllRelatedAssociations` on `long` keys. This removes the
  `decodeAddress`/lock round-trip from the inner loop entirely. Expected: the
  pinned-core phase goes from hours to seconds-to-minutes for the *same* number
  of associations. This is the live fix. Can land in our fork; a *partial*
  workaround can even live in the postScript (see §3.4).
- **Tier 1b (do alongside — S, low risk):** cap/skip the pathological
  duplicate-function groups (the real `k²` source). Trim huge identical-thunk
  groups before they ever become associations. This attacks the `n` itself, not
  just the per-comparison cost. Lives cleanly in our fork's correlator or our
  postScript options.
- **Tier 2 (bigger win, M-L):** parallelize the **correlation** phase
  (stateless, pure, embarrassingly parallel function hashing/scoring over
  immutable snapshots) using Ghidra's existing `ConcurrentQ`, while keeping the
  **commit** phase single-threaded (the program/session DB is single-writer,
  lock-serialized, transactional — that serialization is *fundamental*, not
  incidental). Hashing is already the cheap part for *exact* correlators; the
  real CPU win for parallelism is the **reference/LSH correlator** scoring.
- **Tier 3 (speculative, mostly "no"):** GPU only plausibly helps the
  reference-correlator's bulk cosine-similarity / LSH over feature vectors
  (VTAbstractReferenceProgramCorrelator.java:126-156). For 40k functions a good
  multicore CPU pass is simpler and almost certainly sufficient; GPU offload is
  not worth it. Exact-byte/instruction hashing, graph propagation, and DB
  commits are *not* GPU-amenable.

**The single most important first step:** the Tier 1 `equals`/`hashCode` change.
It is surgical, behavior-preserving, and directly un-pins the core that is
burning right now.

---

## 2. Root cause of the O(n²) (with code)

### 2.1 The quadratic association *count* originates in the correlator

`FunctionMatchProgramCorrelator` (the duplicate-function correlator's engine)
calls `MatchFunctions.matchFunctions(...)`
(FunctionMatchProgramCorrelator.java:55-57). That routine hashes every function,
buckets identical hashes, and for the non-one-to-one case emits the full cross
product of each bucket:

```java
// MatchFunctions.java:86-96  (matchFunctions)
if ((includeOneToOne && aProgAddrs.size() == 1 && bProgAddrs.size() == 1) ||
    (includeNonOneToOne && !(aProgAddrs.size() == 1 && bProgAddrs.size() == 1))) {
    for (Address aAddr : aProgAddrs) {
        for (Address bAddr : bProgAddrs) {
            MatchedFunctions functionMatch =
                new MatchedFunctions(aProgram, bProgram, aAddr, bAddr,
                    aProgAddrs.size(), bProgAddrs.size(), "Code Only Match");
            functionMatches.add(functionMatch);   // a×b matches per hash bucket
        }
    }
}
```

For a bucket of `k` byte-identical thunks on each side this is `k²` matches → `k²`
associations created in the session DB. RB3's Bank 5/Bank 8 have many such
buckets (compiler-emitted thunks, `__sinit` stubs, trivial getters), so the
association table is dominated by these dense groups.

### 2.2 The apply phase rebuilds those groups into HashSets, repeatedly

`applyDuplicateFunctionMatches` iterates every duplicate match
(AutoVersionTrackingTask.java:745-815). For each not-yet-processed match it calls
`getAllRelatedAssociations`, which builds a `HashSet<VTAssociation>` of the whole
related group and then, *for every member*, unions in that member's related set
again:

```java
// AutoVersionTrackingTask.java:825-847  (getAllRelatedAssociations)
Collection<VTAssociation> relatedAssociations =
    vtAssocManager.getRelatedAssociationsBySourceAndDestinationAddress(source, destination);
Set<VTAssociation> allRelatedAssociations = new HashSet<>(relatedAssociations);   // (A)
for (VTAssociation association : relatedAssociations) {
    monitor.checkCancelled();
    allRelatedAssociations.addAll(                                                // (B)
        vtAssocManager.getRelatedAssociationsBySourceAndDestinationAddress(
            association.getSourceAddress(), association.getDestinationAddress()));
}
return allRelatedAssociations;
```

For a group of size `g` this does `g` related-lookups and inserts `O(g²)`
elements into the `HashSet` (each `getRelatedAssociations…` returns ~the whole
group). And `applyDuplicateFunctionMatches` calls this once per group, so the
overall apply cost is `Σ g²` over groups — which for a few dense thunk buckets is
the dominant term. (The `processedSrcDestPairs.addAll(...)` at
AutoVersionTrackingTask.java:792 prevents re-processing a group, so it's `Σ g²`,
not `Σ g³` — but `Σ g²` is already the killer.)

### 2.3 Each HashSet insert pays a locked DB decode

`HashSet.add` → `HashMap.putVal` → `VTAssociationDB.equals` / `hashCode`. Both go
through the decoded `Address`:

```java
// VTAssociationDB.java:177-180  (hashCode)
public int hashCode() {
    return getSourceAddress().hashCode() + getDestinationAddress().hashCode();
}

// VTAssociationDB.java:206-219  (equals)
public boolean equals(Object obj) {
    ...
    VTAssociation other = (VTAssociation) obj;
    return getSourceAddress().equals(other.getSourceAddress()) &&
        getDestinationAddress().equals(other.getDestinationAddress());
}
```

`getSourceAddress()` / `getDestinationAddress()` each acquire the session lock and
decode through the address map:

```java
// VTAssociationDB.java:94-105  (getSourceAddress)
public Address getSourceAddress() {
    associationDBM.lock.acquire();
    try {
        checkIsValid();
        return associationDBM
            .getSourceAddressFromLong(record.getLongValue(SOURCE_ADDRESS_COL.column()));
    }
    finally {
        associationDBM.lock.release();
    }
}
```

`getSourceAddressFromLong` → `VTSessionDB.getSourceAddressFromLong`
(VTSessionDB.java:571-578) → `addressMap.decodeAddress(value)` →
`AddressMapDB.decodeAddress`, which is **`synchronized`**:

```java
// AddressMapDB.java:494-497 and 508-509
@Override
public synchronized Address decodeAddress(long value) {
    return decodeAddress(value, true);
}
...
public synchronized Address decodeAddress(long value, boolean useMemorySegmentation) {
```

So the cost model of a single `HashSet.add` in the hot loop is:
`equals` (worst case across a bucket of mutually-equal hashCodes — and these
thunks frequently collide in `hashCode` since it's just summed offsets) →
multiple `getSourceAddress`/`getDestinationAddress` → multiple
`lock.acquire()` + `synchronized decodeAddress` + `Address` allocation +
`normalize` (AddressMapDB.java:524-536). Multiply by `O(g²)` inserts per group
and the whole run lives on one core in `decodeAddress`. That is exactly the
`jstack`.

### 2.4 Why the data we need is already cheap

Critically, **the comparison never needs a decoded `Address`.** The information
that uniquely identifies an association is already present as cheap primitives on
the object:

- The **DB record key** is a unique `long` per association:
  `DatabaseObject.key` / `getKey()` (DatabaseObject.java:31,57). Two
  `VTAssociationDB` instances are the same association iff they have the same
  record key (the table is keyed on it; `AssociationDatabaseManager`'s cache is
  keyed on it — AssociationDatabaseManager.java:340-355). The cache
  (`DBObjectCache`) already returns the *same* object for a given key in most
  paths, but `equals` must still be correct for distinct instances.
- The **source/dest address keys** are stored as `LongField` columns
  (VTAssociationTableDBAdapter.java:35-37) and read with
  `record.getLongValue(col)` — a plain in-memory array access with **no lock and
  no decode** (DBRecord.java:276-277). Two associations are equal iff these two
  longs match (this is the encoded form of the address pair; equal encoded keys
  ⇔ equal addresses within one program's address map).

Either of these gives a correct, lock-free identity. The current code throws that
away and reconstructs `Address` objects through a synchronized map just to
compare them.

---

## 3. Tier 1 — the surgical O(n²) fix

### 3.1 The change

Make `VTAssociationDB.equals`/`hashCode` compare the cheap primitives instead of
decoded addresses. Two equally-correct options:

**Option A — compare the DB record key (simplest, strongest identity).**

```java
@Override
public int hashCode() {
    return Long.hashCode(key);   // DatabaseObject.key, the unique association record id
}

@Override
public boolean equals(Object obj) {
    if (obj == this) return true;
    if (!(obj instanceof VTAssociationDB)) return false;
    VTAssociationDB other = (VTAssociationDB) obj;
    // same session/manager + same record key == same association
    return associationDBM == other.associationDBM && key == other.key;
}
```

Caveat to verify: the existing `equals` accepts any `VTAssociation` (interface),
not just `VTAssociationDB` (VTAssociationDB.java:213). In headless AutoVT every
association in these sets is a `VTAssociationDB` from one manager, so narrowing to
`VTAssociationDB` is safe for our run. If upstream wants to keep cross-impl
equality, use **Option B**.

**Option B — compare the stored source/dest address *longs* (keeps address
semantics, still lock-free).**

```java
private long sourceKey() { return record.getLongValue(SOURCE_ADDRESS_COL.column()); }
private long destKey()   { return record.getLongValue(DESTINATION_ADDRESS_COL.column()); }

@Override
public int hashCode() {
    long h = sourceKey() * 31 + destKey();
    return Long.hashCode(h);
}

@Override
public boolean equals(Object obj) {
    if (obj == this) return true;
    if (!(obj instanceof VTAssociationDB)) return false;
    VTAssociationDB other = (VTAssociationDB) obj;
    return sourceKey() == other.sourceKey() && destKey() == other.destKey();
}
```

`getLongValue` is a non-locking in-memory read (DBRecord.java:276-277). Note
`record` can be refreshed (VTAssociationDB.java:82-92); reading it under the
existing object lifecycle is fine for the apply phase, which is single-threaded.
To be fully defensive you could wrap the two reads in `checkIsValid()` like the
accessors do, but that's only needed if invalidation can race — it can't in the
serial apply phase.

**Recommendation:** Option A (record key) is the cleanest and has the best hash
distribution (record keys are dense, unlike summed address offsets which collide
heavily across thunks — and hash collisions are precisely what makes the current
`equals` fire so often). Behavior is preserved: within one session, "same record
key" ⇔ "same (source,dest) association," which is the invariant the table
enforces.

### 3.2 Also dedup `getAllRelatedAssociations` without object hashing

Even with cheap `equals`, the cleanest structural fix is to stop building a
`HashSet<VTAssociation>` at all and dedup on primitive keys. Rewrite
`getAllRelatedAssociations` (AutoVersionTrackingTask.java:825-847) to accumulate
into a `Set<Long>` of association record keys (or a `LongHashSet` from
`ghidra.util` if you want to avoid boxing), resolving back to associations once at
the end. Pseudocode:

```java
private Set<VTAssociation> getAllRelatedAssociations(Address source, Address dest,
        TaskMonitor monitor) throws CancelledException {
    VTAssociationManager mgr = session.getAssociationManager();
    Map<Long, VTAssociation> byKey = new HashMap<>();          // key = ((VTAssociationDB)a).getKey()
    Deque<VTAssociation> seed = new ArrayDeque<>(
        mgr.getRelatedAssociationsBySourceAndDestinationAddress(source, dest));
    for (VTAssociation a : seed) byKey.putIfAbsent(((VTAssociationDB) a).getKey(), a);
    for (VTAssociation a : new ArrayList<>(byKey.values())) {
        monitor.checkCancelled();
        for (VTAssociation r : mgr.getRelatedAssociationsBySourceAndDestinationAddress(
                a.getSourceAddress(), a.getDestinationAddress())) {
            byKey.putIfAbsent(((VTAssociationDB) r).getKey(), r);
        }
    }
    return new LinkedHashSet<>(byKey.values());
}
```

With Tier 1's `hashCode`/`equals` fix this is already fast; doing the `Long`
dedup additionally guarantees no `VTAssociationDB.equals` is ever called from the
set build, which is the belt-and-suspenders version.

Note the *same* pattern (`new HashSet<>(getRelatedAssociations…)`) recurs in
`hasAcceptedRelatedAssociation` (AutoVersionTrackingTask.java:701-729) and in
`AssociationDatabaseManager.getRelatedAssociations` /
`blockRelatedAssociations` / `unblockRelatedAssociations` /
`computeBlockedStatus` (AssociationDatabaseManager.java:507-574), which build
`HashSet<VTAssociationDB>` during *accept*. Those also benefit automatically from
the Tier 1 `equals`/`hashCode` fix (they call `getRelatedAssociations`, which
inserts into a `HashSet<VTAssociationDB>` at line 560,567). So the one
`equals`/`hashCode` edit fixes multiple call sites at once.

### 3.3 Expected complexity / speedup

- **Per-comparison cost:** drops from "acquire lock + `synchronized`
  `decodeAddress` + `Address` alloc + normalize, ×2" to "two `long` field reads
  and a primitive compare." That is roughly a 100×-1000× constant-factor
  reduction *per `equals`/`hashCode` call*, and — more importantly — removes all
  lock acquisition from the inner loop.
- **Hash distribution:** Option A's record-key hash eliminates the
  thunk-collision storm (the current summed-offset `hashCode` collides across
  the very thunks that dominate the buckets, turning each `add` into a linear
  `equals` scan of the bucket — so the *current* effective cost per insert is
  itself `O(bucket)`; Option A makes it true `O(1)` amortized).
- **Net:** the `Σ g²` insert work stays `Σ g²` *element operations* but each is
  now nanoseconds and lock-free, so the phase goes from the observed
  hours-on-one-core to seconds-to-minutes. Combined with Tier 1b (below) the
  `g²` term shrinks too.

This is the live fix. It is behavior-preserving (same set contents, same matches,
same markup) and touches only comparison semantics.

### 3.4 Where it lives: fork vs postScript

- **`equals`/`hashCode` change → fork** (VTAssociationDB.java). It cannot be done
  from the postScript because it's the identity of a core VT DB class. This is
  the right place; it's a tiny, defensible upstream-quality patch (arguably worth
  a PR to upstream Ghidra — the current implementation is a latent O(n²) for
  *any* large duplicate-heavy program pair, not just ours).
- **Pure-postScript partial workaround (no fork rebuild needed, if you want to
  validate the thesis fast):** in our `RB3AutoVersionTrackingScript`, *don't run*
  `AutoVersionTrackingTask`'s duplicate-function apply on the pathological
  groups. Concretely: set `RUN_DUPE_FUNCTION_OPTION = false`
  (RB3AutoVersionTrackingScript.java:223) to skip the duplicate-function
  correlator+apply entirely, or raise `DUPE_FUNCTION_CORRELATOR_MIN_LEN_OPTION`
  (currently 10, line 229) to a value that excludes the tiny identical thunks
  that form the dense buckets. The duplicate-function phase only *disambiguates*
  matches that the exact correlators left ambiguous; for type/signature porting
  the exact-symbol + exact-instruction correlators already carry the vast
  majority of the markup. This is a one-line config change we can ship today to
  unblock the run while the fork patch is built. **Tradeoff:** we lose the
  operand-based tie-breaking for genuinely-duplicate medium functions, i.e. a
  small number of ambiguous matches won't get auto-applied. For our use
  (porting DWARF types, keeping CW names) that's an acceptable, recoverable loss.

### 3.5 Tier 1b — attack `n` itself (the duplicate-group blowup)

The deepest fix is to not generate `k²` associations for a bucket of `k`
identical thunks in the first place. Options, in increasing invasiveness:

1. **Config (postScript, zero code):** raise the duplicate min-length so trivial
   thunks/stubs are excluded from the duplicate correlator (see §3.4). Thunks are
   already skipped by `MatchFunctions` (`!func.isThunk()`, MatchFunctions.java:58,
   68) — but compiler stubs that *aren't* flagged as thunks still form buckets,
   and a higher min-length removes most.
2. **Cap bucket cross-product (fork, in the correlator):** in
   `MatchFunctions.matchFunctions` (MatchFunctions.java:86-96), when
   `aProgAddrs.size() * bProgAddrs.size()` exceeds a threshold (e.g. a few
   hundred), skip emitting the cross product (these are inherently ambiguous and
   unresolvable by operand comparison anyway — every member has identical
   instructions). This caps `g` and turns `Σ g²` into a bounded constant per
   bucket. Low risk because such buckets produce *no* uniquely-resolvable match
   by construction.

Tier 1 (cheap comparisons) + Tier 1b option 1 (config) together are the
pragmatic, ship-today combination; add Tier 1b option 2 in the fork for
robustness on future program pairs.

---

## 4. Tier 2 — concurrency with memory-safe / actor-style structures

### 4.1 The fundamental constraint: the DB is single-writer

Ghidra's DB layer is transactional and single-writer by design. The session DB
(`VTSessionDB`) and both programs' DBs (`ProgramDB`) serialize all access through
a `ghidra.util.Lock` (`AssociationDatabaseManager.lock`, shared from
`session.getLock()` — AssociationDatabaseManager.java:46,69), and the address map
itself synchronizes (AddressMapDB.java:495). Every write — `createMatchSet`,
`addMatch`, `setAccepted`, `updateAssociationRecord`, and every markup apply via
`ApplyMarkupItemTask` — runs inside one VT transaction opened in
`AutoVersionTrackingTask.run` (`session.startTransaction`,
AutoVersionTrackingTask.java:115). **This serialization is fundamental, not
incidental.** You cannot have N threads concurrently mutating the program/session
DB; the transaction + B-tree + address-map invariants forbid it. So the
fan-in/commit phase is *necessarily* single-threaded.

The leverage is that the *expensive* work — correlation (hashing/scoring function
pairs) and markup *computation* (reading instructions, deciding what to apply) —
is mostly **read-only over the two programs** and can be done off-thread on
immutable snapshots, with only the final DB mutation serialized.

### 4.2 What is embarrassingly parallel vs inherently serial

| Phase | Where | Parallelizable? | Notes |
|---|---|---|---|
| Function hashing (exact byte/instr/mnemonic) | `MatchFunctions.matchFunctions` hashing loops (MatchFunctions.java:55-71) | **Yes** — pure per-function hash | Read-only over program; hash is a pure function of bytes/instructions. Today serial. |
| Bucketing identical hashes | MatchFunctions.java:79-97 | Partially | Map merge; fan-out hashes, fan-in into a concurrent map or merge per-thread maps. |
| Reference/LSH correlator scoring | `VTAbstractReferenceProgramCorrelator` cosine-similarity over `LSHCosineVectorAccum` (lines 126-156, 178+) | **Yes — biggest CPU win** | Nested loop over dest×src feature vectors computing cosine scores. Pure math over precomputed vectors. |
| Operand-map build for dup disambiguation | `createFunctionsMap` / `mapFunctionScalarAndAddressOperands` (AutoVersionTrackingTask.java:979-1294) | **Yes** | Read-only per function; produces plain `Map<Long,Map<Integer,Object>>` value objects. |
| Creating match sets / associations | `matchSet.addMatch`, `getOrCreateAssociationDB` | **No** | DB writes under the session lock + address-map (AssociationDatabaseManager.java:201-230). |
| Accept + block-related + apply markup | `setAssociationAccepted`, `ApplyMarkupItemTask.run` (AutoVersionTrackingTask.java:651-665, 1351-1387) | **No** | Mutates both DBs transactionally; ordering matters (accept blocks conflicting associations). |

### 4.3 The actor/functional split: stateless snapshot → serialized commit

The design the user wants maps cleanly onto Ghidra's existing machinery:

**Phase P (parallel, pure, no DB, no lock):**
Extract an **immutable snapshot** of each function up front, on the calling
thread (single DB read pass), into plain value objects:

```
FunctionSnapshot {
    long entryKey;          // encoded address key (already what the DB stores) — NOT an Address
    long bodyHash;          // FunctionHasher result (byte/instr/mnemonic)
    int  bodyLength;
    long[] refKeysOut;      // outgoing reference target address-keys as longs
    int[]  mnemonicHistogram / feature vector;   // for similarity/LSH
    // operand fingerprint for dup disambiguation, as primitives only
}
```

Workers operate **only** on `FunctionSnapshot[]` — pure functions, no `Program`,
no `Address`, no `DBRecord`, no lock. They emit **candidate** results as plain
value objects:

```
CandidateMatch { long srcEntryKey; long dstEntryKey; double similarity, confidence;
                 int srcLen, dstLen; VTAssociationType type; }
```

Because workers touch *no* shared mutable state and *no* DB, there are **no race
conditions** to reason about — the snapshot is read-only and the outputs are
disjoint per task. This is the "stateless, functional, almost-actor" model: each
task is a pure function `FunctionSnapshot → List<CandidateMatch>`.

**Phase C (serial commit):**
A single thread drains the `CandidateMatch` value objects and performs all DB
mutation in deterministic order inside the existing transaction:
`createMatchSet` → `addMatch` → (for dup phase) accept/block/apply markup. This
is the only phase that takes the lock, and it does so from one thread, so there is
zero contention. Determinism (sorting candidates by `(srcEntryKey, dstEntryKey)`
before commit) keeps results reproducible run-to-run, which matters for a markup
port we re-run.

### 4.4 Reuse Ghidra's own ConcurrentQ (don't invent a framework)

Ghidra's auto-analysis already parallelizes exactly this fan-out/fan-in shape via
`generic.concurrent.ConcurrentQ` + `GThreadPool`, and the decompiler wraps it in
`ParallelDecompiler` /`DecompilerConcurrentQ`
(ParallelDecompiler.java:94-99 uses `GThreadPool.getSharedThreadPool(...)` +
`DecompilerConcurrentQ(callback, threadPool, ...)` then `queue.process(functions,
resultsConsumer)`). The VT module currently uses **none** of this (grep for
`ConcurrentQ` in `Features/VersionTracking/src/main` returns nothing).

The reuse recipe for Phase P:

- Build a `ConcurrentQ<FunctionSnapshot, List<CandidateMatch>>` via
  `ConcurrentQBuilder`
  (`Ghidra/Framework/Generic/.../generic/concurrent/ConcurrentQBuilder.java`),
  backed by `GThreadPool.getSharedThreadPool(...)`.
- The `QCallback` is the pure scorer. Results stream to a single consumer that
  appends to a thread-safe queue of `CandidateMatch`.
- Phase C then commits sequentially. No worker ever calls `addMatch` /
  `setAccepted` / `decodeAddress`.

This mirrors `AutoAnalysisManager`'s model (analyzers submit per-function work to
a shared pool; results applied under program lock) and inherits its
cancellation/monitor plumbing (`TaskMonitor` threading through `ConcurrentQ`).

### 4.5 Race conditions a naive parallelization would hit (and how the design avoids them)

1. **Concurrent DB writes / B-tree + address-map corruption.** If workers called
   `addMatch`/`setAccepted` directly, they'd race on the session B-tree and the
   `synchronized` address map. *Avoided:* workers emit value objects only; all
   writes are in serial Phase C.
2. **`AddressMapDB.createBaseAddress`/`getKey` mutating shared state.** Encoding a
   *new* address can mutate the base-address table (AddressMapDB.java:480-491).
   *Avoided:* snapshots pre-encode all needed keys (`getKey(addr, false)`) on the
   calling thread before fan-out; workers compare existing `long`s only and never
   encode.
3. **Accept-order dependence.** `setAssociationAccepted` *blocks* conflicting
   related associations (AssociationDatabaseManager.java:480-497,507-524); the
   set of applied matches depends on the order accepts happen. *Avoided:* keep
   accept/apply entirely in serial Phase C with a deterministic candidate order;
   parallelism is only for the score computation that *proposes* matches.
4. **`DBObjectCache` / lazy `record` refresh races.** `VTAssociationDB.refresh`
   can swap `record` (VTAssociationDB.java:82-92) and the cache holds size-10
   soft refs (AssociationDatabaseManager.java:70). *Avoided:* workers never touch
   `VTAssociationDB`/`DBRecord`; they hold immutable snapshots.

### 4.6 Effort / risk

- **P-correlation for the reference/LSH correlator (highest ROI for parallelism):
  M.** The scoring loop (VTAbstractReferenceProgramCorrelator.java:126-156) is
  already structured as nested loops over precomputed vectors; wrapping the outer
  loop in a `ConcurrentQ` with a results consumer is mechanical. Risk: medium —
  must ensure the vector maps are fully built (read-only) before fan-out, and
  collect `VTMatchInfo` into a list committed serially.
- **P-hashing for exact correlators: M, but lower ROI.** Exact-byte/instruction
  hashing is already fast relative to the *apply* phase; parallelizing it helps
  total wall-clock modestly. Worth doing after Tier 1 if hashing shows up in
  profiles.
- **Operand-map build parallelization (dup phase): S-M.** `createFunctionsMap`
  (AutoVersionTrackingTask.java:979-1021) is read-only per function and returns
  plain maps; easy to fan out. But once Tier 1 removes the `decodeAddress`
  bottleneck, this phase is unlikely to dominate — measure first.
- **Overall risk:** the architecture is low-risk *because* it preserves the
  serial commit; the only correctness surface is "did the snapshot capture
  everything the scorer needs." Keeping commit serial means we never fight the
  transaction model.

**Sequencing:** ship Tier 1 first (it's the live fix and may make Tier 2
unnecessary for *our* run). Then, if total wall-clock is still dominated by
correlation (likely the reference correlator on 40k functions), parallelize Phase
P starting with the reference/LSH scorer.

---

## 5. Tier 3 — GPU / extreme parallelism (honest assessment)

**Short answer: not worth it here.** GPU offload is a poor fit for almost all of
VT, and the one part that *is* data-parallel (similarity scoring) is small enough
at 40k functions that multicore CPU wins on simplicity.

What a GPU *could* in principle accelerate:
- **Bulk cosine-similarity / LSH over feature vectors** in the reference
  correlator (`LSHCosineVectorAccum`, dot products over dest×src vectors,
  VTAbstractReferenceProgramCorrelator.java:126-156). This is the classic
  GPU-amenable kernel (dense/sparse matrix-vector). At 40k×40k candidate pairs a
  GPU would chew through it. **But:** LSH exists precisely to *avoid* the full
  cross product — it buckets candidates so you only score near-neighbors, which
  collapses the work to roughly `O(n × neighbors)`, well within CPU reach.
- **Bulk fingerprint/n-gram hashing** (mnemonic histograms). Also GPU-friendly in
  theory, but the per-function hash is cheap and the bottleneck is reading the
  program (DB I/O on the CPU), not arithmetic.

What a GPU *cannot* help (the majority):
- **Exact byte/instruction matching** — it's hashing + hash-map bucketing, not
  arithmetic throughput; dominated by DB reads.
- **Reference-graph propagation** — pointer-chasing over the call/data-reference
  graph (VTAbstractReferenceProgramCorrelator.java:403-570). Irregular,
  data-dependent, latency-bound; the GPU-hostile case.
- **The entire apply/commit path** — transactional DB writes, address decode,
  markup application. Inherently serial and I/O-bound.
- **The live bug** — it's lock contention on a `synchronized decodeAddress` in a
  HashSet build. A GPU does nothing for that; Tier 1 does.

**Data-size reality:** 40k functions is "big for a single core, trivial for a
thread pool." The cross-product blowup is in the *number of associations*
(addressed by Tier 1/1b), not in arithmetic intensity. There is no dense numeric
kernel large enough to amortize GPU transfer + JNI/JCuda/OpenCL integration cost
against. **Recommendation: do not pursue GPU.** If similarity scoring ever
becomes the wall-clock bottleneck after Tier 1+2, the right next step is better
LSH bucketing and CPU SIMD, not a GPU.

---

## 6. Prioritized recommendation

1. **Now / unblock the live run (postScript, zero rebuild):** in
   `RB3AutoVersionTrackingScript.buildOptions()`, raise
   `DUPE_FUNCTION_CORRELATOR_MIN_LEN_OPTION` (currently 10,
   RB3AutoVersionTrackingScript.java:229) to exclude the tiny identical thunks,
   or set `RUN_DUPE_FUNCTION_OPTION=false` (line 223) to skip the pathological
   phase entirely. Recoverable, low-stakes for a type/signature port. (§3.4)
2. **Tier 1 — the real fix (fork, S, ~1-2 hr, very low risk):** change
   `VTAssociationDB.hashCode`/`equals` (VTAssociationDB.java:177-219) to compare
   the primitive record key (Option A) — lock-free, decode-free, better hash
   distribution. Optionally dedup `getAllRelatedAssociations`
   (AutoVersionTrackingTask.java:825-847) on `long` keys. This un-pins the core
   and is arguably worth upstreaming. (§3.1-3.3)
3. **Tier 1b — cap the blowup (fork, S):** bound the bucket cross-product in
   `MatchFunctions.matchFunctions` (MatchFunctions.java:86-96) so dense
   identical-stub groups don't create `k²` associations. Defensive for future
   program pairs. (§3.5)
4. **Tier 2 — parallelize correlation if still slow (M-L):** stateless
   snapshot → `ConcurrentQ` fan-out → serial commit, starting with the
   reference/LSH scorer (VTAbstractReferenceProgramCorrelator.java:126-156),
   reusing `GThreadPool`/`ConcurrentQ` as `ParallelDecompiler` does. Keep all DB
   writes single-threaded. (§4)
5. **Tier 3 — GPU: do not pursue.** No kernel large enough to justify it; the
   bottleneck is lock contention and association count, not arithmetic. (§5)

### Uncertainties / things to verify before landing

- **Confirm the dominant bucket sizes empirically.** Add a one-off log of the
  per-hash bucket `a×b` in `MatchFunctions.matchFunctions` (or query the
  association count after the dup correlator) to confirm the `Σ g²` thesis and
  pick the Tier 1b threshold. I inferred the dense-thunk groups from the
  cross-product code + the `jstack`, but haven't measured the actual
  distribution on these two ELFs.
- **`equals` interface contract.** The current `equals` accepts any
  `VTAssociation` (VTAssociationDB.java:213). Narrowing to `VTAssociationDB`
  (Options A/B) is safe for our headless run (all instances are `VTAssociationDB`
  from one manager) but would be a behavior change for any hypothetical mixed-impl
  caller. For an upstream PR, prefer Option B (compares the stored address longs)
  to preserve "equal addresses ⇒ equal," or keep the interface check and fall
  back to address compare only when `other` isn't a `VTAssociationDB`.
- **Record-key stability across refresh.** `getKey()` is stable for the lifetime
  of an association (it's the table key); `record` can be swapped by `refresh`
  but `key` does not change (DatabaseObject.java:31). Safe for Option A.
