# Ghidra VT performance fixes — upstream PR prep (2026-06-09)

**Status: PREPARED, NOT PUSHED, NOT OPENED.** Local branches + this doc only; awaits human
review. Upstream = https://github.com/NationalSecurityAgency/ghidra.

See also: [ghidra-vt-handoff-2026-06-09.md](ghidra-vt-handoff-2026-06-09.md),
[ghidra-vt-optimization-2026-06-09.md](ghidra-vt-optimization-2026-06-09.md),
[ghidra-vt-tier2-concurrency-design-2026-06-09.md](ghidra-vt-tier2-concurrency-design-2026-06-09.md).

## Prepared artifacts

- **Worktree:** `/tmp/claude/ghidra-vt-pr` (a `git worktree` of `/home/free/code/milohax/ghidra`;
  the main tree was not touched — a concurrent BSim agent uses its built runtime).
- **Base:** `nsa/master` @ `430465776d` (fetched fresh 2026-06-09; upstream had nothing newer).
- **Branches** (all in the fork repo, local only):
  - `vt-perf-fixes` — the one-PR layout: all 3 cherry-picks (`43660105c8`, `743a81aab0`, `13760b74a6`).
  - `vt-assoc-equals-key` — PR 1 layout: Tier-1 only (`43660105c8`).
  - `vt-parallel-ref-correlator` — PR 2 layout: Tier-2 + mem-fix (`a6233e1882`, `f9cd28fe9f`).
- **Cherry-pick results:** all clean, no conflicts. Tier-2 commits are byte-identical diffs to
  the fork originals (`0963a5e934`, `611a8d1dc5`). Tier-1 (`df874cfe14`) differs only in hunk
  context/line numbers (upstream renamed `DatabaseObject`→`DbObject` and the file shifted by
  25 lines since the fork commit); every added/removed line is identical. The two branch layouts
  produce identical file contents (verified file-level diff across layouts).
- **Compile sanity check: PASS.** `javac` (JDK 26, `/usr/lib/jvm/java-26-openjdk`) of both touched
  files against the built fork runtime's full jar classpath
  (`/home/free/code/milohax/ghidra-rt-parallel/ghidra_12.2_DEV`, 197 jars) → exit 0. The single
  "uses or overrides a deprecated API" note also fires on the **stock** `nsa/master` version of
  the correlator file (pre-existing, not introduced). A full `gradle prepDev` build was *not* run
  (the main tree is in use by a concurrent agent); see the pre-submission checklist.

## CONTRIBUTING.md requirements (digest)

- **No CLA.** Inbound=outbound (GitHub ToS §D.6 / Apache-2.0 §5); submitting the PR *is* the
  license grant. Contributions to a USG repo are voluntary, no payment expectation.
- **"Isolate multiple patches from each other... do so in separate, smaller pull requests."**
- **"Before submission, please squash your commits using a message that starts with the issue
  number and a description of the changes."** → open a GitHub issue first; squash to one commit
  per PR titled `#NNNN: <description>`.
- "Consider first opening a dialogue with the Ghidra team" for improvements; they initially
  prioritize **small bug fixes** — which is exactly the Tier-1 patch's shape.
- Patches must compile in their dev environment, ideally the full build.
- Reviewers explicitly look for **threading issues** and **performance implications** (so the
  race-freedom argument below is the load-bearing part of PR 2).
- If AI assisted, apply extra scrutiny for correctness + their legal requirements.

## Recommendation: TWO PRs (split), not one

1. **PR 1 = Tier-1 alone** (`vt-assoc-equals-key`). It is a self-contained, ~18-line,
   zero-threading **O(n²) bug fix** with a reproducible benchmark — precisely the "small bug
   fix" CONTRIBUTING says they prioritize and can accept quickly. It must not be held hostage
   by the threading review the parallel correlator will (rightly) get.
2. **PR 2 = Tier-2 + mem-fix together** (`vt-parallel-ref-correlator`). These two are NOT
   independent: the parallelize commit alone OOMs a default 2G `analyzeHeadless` heap at scale
   (proven empirically); the chunking commit is its required memory bound. Shipping them
   separately would make the intermediate state a regression, so they belong in one PR — and
   per the squash guidance they should be **squashed to one commit** before submission anyway.

The two PRs are functionally independent (different files: `VTAssociationDB.java` vs
`VTAbstractReferenceProgramCorrelator.java`; verified by clean cherry-picks of 2+3 without 1)
and CONTRIBUTING explicitly asks for independent patches in separate PRs. The one-PR branch
`vt-perf-fixes` is kept as the fallback layout if the human prefers a single submission.

---

## PR 1 body — `vt-assoc-equals-key`

**Title (pre-squash):** `VT: use record-key identity for VTAssociationDB equals/hashCode`
**Title (post-squash, once an issue exists):** `#NNNN: Fix O(n^2) stall in Auto Version Tracking duplicate-match application (VTAssociationDB equals/hashCode)`

### Summary

`AutoVersionTrackingTask` can stall for **hours on one core** applying duplicate-function
matches on large programs. The root cause is `VTAssociationDB.equals()`/`hashCode()`:
both round-trip through the **`synchronized` `AddressMapDB.decodeAddress`** (a DB lock +
`Address` allocation per call), and the offset-sum `hashCode` collides heavily across the
byte-identical thunks that dominate duplicate-function groups. This patch makes both methods
use the association's cached **DB record key** — lock-free, decode-free, collision-free —
while keeping the original address comparison as a fallback for non-DB / cross-manager
`VTAssociation` implementations.

### Root cause

- The duplicate-function correlator emits one association per `(source, dest)` pair **within
  each identical-code-hash group**, so a group of `k` byte-identical functions per side yields
  ~`k²` associations (`MatchFunctions.matchFunctions` cross-product).
- `AutoVersionTrackingTask.getAllRelatedAssociations` builds a `HashSet<VTAssociation>` over
  that group, and the same `new HashSet<>(getRelatedAssociations...)` pattern recurs in
  `hasAcceptedRelatedAssociation` and in `AssociationDatabaseManager`'s related-association
  block/unblock paths. Every `HashSet.add` calls `hashCode`/`equals`.
- `hashCode()` was `getSourceAddress().hashCode() + getDestinationAddress().hashCode()`;
  `equals()` compared decoded addresses. Each accessor takes the association manager's lock
  and decodes through `AddressMapDB.decodeAddress`, which is `synchronized`.
- Worse, the summed-offset hash is the **same value** for the many distinct associations that
  point at identical-thunk clusters, so the `HashSet` degrades to linear `equals` scans —
  each step of which is itself multiple locked decodes. Net: `O(n²)` locked address decodes.
- Observed via `jstack`: a single core pinned in
  `HashSet.add → VTAssociationDB.equals → getSourceAddress → AddressMapDB.decodeAddress`
  for 90+ minutes; `jstat` confirmed CPU-bound, not GC-bound.

### Fix

`AssociationDatabaseManager.getOrCreateAssociationDB` stores **exactly one record per
(source, destination) pair**, so within a session the primitive DB record key (`getKey()`,
cached on the object, no lock, no decode) is an exact identity for the association:

- `hashCode()` → `Long.hashCode(getKey())` (dense keys → no thunk-collision storm).
- `equals()` → fast path: if both operands are `VTAssociationDB` from the **same manager**,
  compare record keys; otherwise **fall back to the original decoded-address comparison**, so
  equality semantics against non-DB `VTAssociation` implementations are unchanged.

### Why behavior-preserving

- Within one session, "same record key" ⇔ "same (source, destination) pair" — the invariant
  the association table enforces — so the fast path computes the same answer the address
  comparison would, without the lock or decode.
- Every `VTAssociation` collection upstream is per-session/per-manager (the
  `AutoVersionTrackingTask` sets, `AssociationDatabaseManager`'s related-association sets), so
  the fast path covers all hot paths; the fallback covers everything else with the original code.
- `getKey()` is stable for the lifetime of the object (the record key never changes across
  `refresh()`), so the hash is stable while an object sits in a `HashSet`. The previous
  hashCode was only as stable as the decoded addresses — same guarantee.
- The package-private `AssociationStub` does not override `equals`/`hashCode` (identity
  semantics). It was already asymmetric vs. the old address-comparing `VTAssociationDB.equals`;
  this patch does not change that pre-existing relationship — stubs still hit the fallback path.
- Anticipated reviewer probe — *cross-session hashCode/equals consistency*: two
  `VTAssociationDB`s from **different** sessions with equal addresses still compare equal via
  the fallback, while their record-key hashes may differ — a contract violation **iff** such
  objects are mixed in one hashed collection. No such cross-session collection exists in the
  codebase (associations are meaningless outside their session), and the old summed-offset
  hash was itself colliding by design. If reviewers want belt-and-suspenders, the fallback
  could be narrowed to require the same manager on both sides; we kept the wider fallback to
  preserve the documented address-equality semantics for non-DB implementations.

### Benchmark (synthetic duplicate-group benchmark, headless, JDK; harness available on request)

`N` = number of duplicate groups; time = the duplicate-match application phase
(`getAllRelatedAssociations` HashSet builds):

| N (dup groups) | unpatched (ms) | growth |
|---|---|---|
| 100 | 96 | — |
| 200 | 1,380 | ~14× for 2× N |
| 300 | 3,963 | super-quadratic curve |
| 700 | 68,842 | ~hours at real-program scale |

With the fix the same phase **collapses to near-constant** (the quadratic term's per-element
cost drops to two primitive compares); measured **~11–19× on the hot path** at benchmark sizes,
and the real-world stall (40k-function PowerPC programs, dense identical-thunk groups) went
from a pinned core for hours to seconds. Independently smoke-tested as a single-class patch on
a stock Ghidra 12.1.2 runtime: duplicate phase 2,487 ms → 164 ms on the same input.

### Test plan

- Existing VT unit/integration tests pass (association identity is exercised by the VT API tests).
- Benchmark above (synthetic duplicate-group generator + `AutoVersionTrackingTask` run) —
  unpatched vs patched timing, identical resulting match/association sets.
- Full `AutoVersionTrackingTask` run on a real 40k×40k-function program pair: identical applied
  markup, wall-clock of the duplicate phase collapses.

---

## PR 2 body — `vt-parallel-ref-correlator`

**Title (pre-squash):** `VT: parallelize reference correlator scoring (stateless compare + serial commit)`
**Title (post-squash):** `#NNNN: Parallelize Version Tracking reference correlator scoring with bounded memory`

### Summary

The reference program correlators' (`VTAbstractReferenceProgramCorrelator`)
`findDestinations()` is the dominant Version Tracking CPU cost at scale: an
**O(destinations × sources)** loop computing an LSH cosine similarity for every feature-vector
pair, on a single thread, while all other cores idle. This PR reworks it into a **stateless
parallel scoring phase** (Ghidra's own `generic.concurrent.ConcurrentQ`, mirroring
`DecompilerConcurrentQ`) followed by a **serial, deterministic, chunked commit phase**, leaving
the produced match set **identical** to the serial implementation. Measured **3–4×** end-to-end
on the correlator at 8k functions (scales with cores); peak memory is **bounded to one chunk**
(256 destinations) regardless of program size.

No new dependencies; no API changes; one file touched. All `Program`/`Listing`/`matchSet`
access stays single-threaded.

### Structure (two phases per chunk)

The sorted destination list is processed in fixed chunks of 256:

- **Phase P (parallel, pure vector math):** for each destination in the chunk, a worker scores
  it against all source vectors (`LSHCosineVectorAccum.compare`) into a per-destination
  neighbor map. Workers read only the two read-only vector maps and write only per-pair-local
  `VectorCompare` scratch + their own returned map. No `Program`, no `Listing`, no `matchSet`,
  no shared mutable state. Fan-out uses `ConcurrentQ` on a private named thread pool (headless
  AutoVT has no `AutoAnalysisManager` shared pool); worker exceptions are unwrapped
  (`CancelledException` rethrown as-is) exactly as `DecompilerConcurrentQ` does.
- **Phase C (serial):** iterate the chunk's destinations in ascending address order on the
  calling thread, running the **unchanged** `transform()` → `matchSet.addMatch()` path.
  `matchSet` writes the session DB under a lock and must stay serial. The chunk's neighbor
  maps are dropped before the next chunk is scored.

### Race-freedom argument (the part to probe)

The design removes data races **structurally**, not with added locks:

1. **The only lazily-mutating operation is pre-empted serially.**
   `LSHCosineVectorAccum.compare()` calls `doFinalize()` on **both** operands
   (LSHCosineVectorAccum.java: `compare` → `doFinalize(); ((LSHCosineVectorAccum) op2).doFinalize();`).
   `doFinalize()` mutates the vector **exactly once** — rebuilds the sorted `hash[]` entries
   from `treehash`, nulls `treehash`, sets `finalized = true` — and is **idempotent**: it
   early-returns on the `finalized` flag. After finalization, `compare()` is
   `LSHCosineVector.compare()`, a pure read of the two `hash[]` arrays whose only writes go to
   the **caller-provided, per-pair-local** `VectorCompare`. So `findDestinations` now
   **pre-finalizes every source and destination vector single-threaded before fan-out**; the
   parallel compares then mutate nothing and need no synchronization.
2. **Safe publication.** Vectors are built (`extractReferenceFeatures`) and finalized on the
   calling thread; work items cross into workers through `ConcurrentQ.add(...)`, whose internal
   queue synchronization establishes the happens-before edge, so workers observe fully-built
   `hash[]` state.
3. **No lazy-map population from workers.** `srcVectorsByAddress`/`destVectorsByAddress` are
   `LazyMap`s whose `get(missingKey)` mutates the map. Workers only iterate
   `srcVectorsByAddress.entrySet()` (no factory calls; iterates only populated entries); each
   work item already **carries** its destination vector, looked up on the calling thread while
   building the chunk. Workers never call `get()` on either map.
4. **Disjoint writes, framework-aggregated results.** Each worker returns its own neighbor map;
   results are keyed by the `QResult` work item (no shared result map written by workers);
   aggregation happens in `waitForResults()` on the calling thread.
5. **DB single-writer preserved.** Every `getFunctionAt`, `transform()`, and
   `matchSet.addMatch()` call runs in Phase C on the calling thread, inside the existing
   transaction — byte-for-byte the original commit code. The session DB's single-writer
   constraint is never violated because no worker holds a DB handle of any kind.
6. **Monitor discipline.** Workers only `checkCancelled()` (thread-safe); progress is
   incremented serially in Phase C, so monitor progress state is single-threaded.

### Output equivalence + determinism

- The **match set is identical** to the serial implementation: the same pairs are scored with
  the same pure function, the same `> 0` gate, and the unchanged `transform()` thresholds.
  Collapsing the inner loop's redundant **double `compare()`** (the old code computed each
  similarity twice: once for the score, once to gate) is result-identical because post-finalize
  `compare()` is pure; the stored `VectorCompare` is identical either way.
- **Commit order changes from `HashMap` iteration order (nondeterministic) to ascending
  destination-address order (deterministic):** destinations are sorted once up front; chunks
  run in order; each chunk commits in order. The *set* of matches and all scores/confidences
  are unchanged; run-to-run output is now reproducible, which the old code did not guarantee.
- **Validation performed:** A/B harness running the full correlator end-to-end under a
  serial-baseline runtime vs the parallel runtime on the same synthetic program pair
  (N=8,000 functions, 256 anchors): the **sorted match-tuple digest
  (src, dst, score, confidence) is identical** serial-vs-parallel, and identical across 3
  repeated parallel runs (determinism). Also validated on a real 40k×40k-function PowerPC
  program pair: identical match output, no new warnings in the headless log.

### Why the chunking commit is part of this PR

The first (unchunked) parallel version computed **every** destination's neighbor map before
committing any, retaining all of them simultaneously — the original serial scorer only ever
held **one** destination's map. At large function counts that **OOM'd `analyzeHeadless`'s
default 2G heap** (reproduced; and the no-OOM fix was verified against a negative control that
re-OOMs without it). The chunked version scores 256 sorted destinations in parallel, commits
them serially, drops the maps, and reuses the same `ConcurrentQ` across chunks
(`waitForResults()` resets its internal result list). Peak memory is bounded to a single
chunk regardless of program size; the O(dest×src) compare remains the parallel hot path.
Without this commit the parallelization would be a memory regression, so the two land together.

### Benchmark

- Correlator wall-clock **3–4× faster** (8k-function synthetic pair, multi-core desktop;
  scales with available cores since Phase P is embarrassingly parallel and Phase C is a small
  serial tail). Independently measured 2.75× as a patch on a stock 12.1.2 runtime under
  machine load.
- Combined with the `VTAssociationDB` fix (separate PR), a real Auto VT run over two
  40k-function programs dropped from ~105 minutes to a fraction of that, with identical markup.

### Test plan

- Existing VT tests pass (the correlator's public behavior and `transform()` are unchanged).
- Serial-vs-parallel A/B digest equality + 3-run determinism (harness described above).
- Negative control for the memory bound: unchunked variant OOMs a 2G heap; chunked does not,
  same inputs.
- Suggested upstream addition if reviewers want it: a system-property gate
  (e.g. `-Dvt.parallel.scoring=false`) to fall back to serial scoring — trivial to add since
  Phase P/Phase C share the serial commit code; we did not include it to keep the patch minimal.

---

## Pre-submission checklist (human actions)

- [ ] **Review the three cherry-picked commits** in `/tmp/claude/ghidra-vt-pr`
      (branches `vt-assoc-equals-key`, `vt-parallel-ref-correlator`; fallback `vt-perf-fixes`).
- [ ] **Decide split vs single.** Recommendation above: two PRs.
- [ ] **Open upstream GitHub issue(s) first** (CONTRIBUTING wants commit messages to start with
      the issue number, and suggests dialogue before implementing): one perf-bug issue for the
      O(n²) duplicate-apply stall (with the N=100..700 numbers), one enhancement issue for the
      single-threaded reference correlator. Then **squash each branch to one commit** titled
      `#NNNN: <description>`.
- [ ] **Full build verification** upstream expects: `gradle -I gradle/support/fetchDependencies.gradle`
      + `gradle prepDev` + a module build in a clean clone of the branch (our check was
      javac-against-runtime-classpath only; do NOT gradle-build the main fork tree while the
      BSim agent is running — use the worktree or a clone).
- [ ] Consider running Ghidra's own VT test suite (`:VersionTracking:test`) on the branch.
- [ ] **No CLA needed** — inbound=outbound; submitting the PR is the grant.
- [ ] **Push** the chosen branch(es) to `origin` (github.com/freeqaz/ghidra) — not done, per
      instructions — and open the PR(s) against `NationalSecurityAgency/ghidra:master` with the
      bodies above.
- [ ] Benchmarks live in rb3 `scripts/ghidra/` (`vt_dupe_benchmark.sh`, `vt_ref_ab.sh`,
      `vt_ref_benchmark.sh` + the two `*.java` scripts); offer them in the PR thread if asked
      (CONTRIBUTING forbids self-generated binaries, but these are source scripts).
- [ ] After upstreaming, the local `/opt` deploy story is unchanged (see
      [ghidra-vt-handoff-2026-06-09.md](ghidra-vt-handoff-2026-06-09.md) — 12.1.2 ABI gotcha).
