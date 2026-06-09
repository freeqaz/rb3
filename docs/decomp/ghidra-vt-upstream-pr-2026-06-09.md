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
- **Branches** (all in the fork repo, local only; SHAs are post-review-fix — see the
  changelog section at the bottom for the old→new mapping):
  - `vt-perf-fixes` — the one-PR layout: all 3 cherry-picks (`fb3a5e94f3`, `67c4040982`, `670d599c59`).
  - `vt-assoc-equals-key` — PR 1 layout: Tier-1 only (`236c3263da`).
  - `vt-parallel-ref-correlator` — PR 2 layout: Tier-2 + mem-fix (`369a37441f`, `314bfa567c`).
- **Cherry-pick results:** all clean, no conflicts. The original picks were byte-identical
  (Tier-2) / line-identical (Tier-1, vs fork `df874cfe14`, modulo the upstream
  `DatabaseObject`→`DbObject` rename and a 25-line shift) to the fork originals
  (`df874cfe14`, `0963a5e934`, `611a8d1dc5`); the branches have since been **amended** with the
  adversarial-review fixes (see changelog), so they now deliberately differ from the fork
  commits. The two branch layouts still produce identical file contents (re-verified file-level
  diff across layouts after the amendments). **These fixes must be ported back to fork master
  before the next `/opt` jar rebuild** — the deployed jar has the cosmetic double-driven
  progress bug too.
- **Compile sanity check: PASS (re-run after the review fixes).** `javac` (JDK 26,
  `/usr/lib/jvm/java-26-openjdk`) of both touched files against the built fork runtime's full
  jar classpath (`/home/free/code/milohax/ghidra-rt-parallel/ghidra_12.2_DEV`, 204 jars at
  re-check time) → exit 0, for the final state of all three branches **and** for PR 2's
  intermediate commit (`369a37441f`) so the branch is bisectable. The single "uses or overrides
  a deprecated API" note also fires on the **stock** `nsa/master` version of the correlator file
  (pre-existing, not introduced). A full `gradle prepDev` build and the VT test suite were *not*
  run (the machine is in use by concurrent agents); see the pre-submission checklist.

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
keeping the original address comparison for non-DB `VTAssociation` implementations and for
deleted instances, and making `VTAssociationDB`s from different managers compare unequal
(record keys are session-scoped; cross-session associations have no meaningful equality).

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
- `equals()` → fast path: if both operands are `VTAssociationDB` from the **same manager**
  and **both are live** (cheap `isDeleted(lock)` check — two volatile reads on the hot path),
  compare record keys.
- `equals()` → two `VTAssociationDB`s from **different managers** are never equal: record keys
  are session-scoped, associations have no meaningful cross-session equality, and answering by
  address there would be inconsistent with the key-based `hashCode` (an equals/hashCode
  contract violation).
- `equals()` → same-manager pairs where either side is **deleted** fall back to the original
  decoded-address comparison (a deleted object reads its frozen record), because a deleted
  association's record key can be reused by a later association — see "deleted-key reuse"
  below.
- `equals()` → non-DB `VTAssociation` implementations keep the **original decoded-address
  comparison**, so equality semantics against them are unchanged.

### Why behavior-preserving

- Within one session, "same record key" ⇔ "same (source, destination) pair" for live objects —
  the invariant the association table enforces — so the fast path computes the same answer the
  address comparison would, without the lock or decode.
- Every `VTAssociation` collection upstream is per-session/per-manager (the
  `AutoVersionTrackingTask` sets, `AssociationDatabaseManager`'s related-association sets), so
  the fast path covers all hot paths; the fallback covers everything else with the original code.
- `getKey()` is stable for the lifetime of the object (the record key never changes across
  `refresh()`), so the hash is stable while an object sits in a `HashSet`. The previous
  hashCode was only as stable as the decoded addresses — same guarantee.
- The package-private `AssociationStub` does not override `equals`/`hashCode` (identity
  semantics). It was already asymmetric vs. the old address-comparing `VTAssociationDB.equals`
  (`stub.equals(db)` is identity-false while `db.equals(stub)` can be address-true); this patch
  does not change that pre-existing relationship — stubs still hit the address-comparison path,
  and no in-tree call site mixes stubs into hashed collections of DB associations (verified by
  grep).
- *Cross-session hashCode/equals consistency*: under the **old** code, two `VTAssociationDB`s
  from different sessions with equal addresses compared equal; keeping that with a key-based
  `hashCode` would be a strict equals/hashCode contract violation (equal objects, different
  hashes). The submitted behavior therefore **narrows** equals: cross-manager
  `VTAssociationDB` pairs are never equal. This is safe because no cross-session collection of
  associations exists in the codebase (associations are meaningless outside their session) —
  and it is the principled answer to give a reviewer, rather than a latent contract violation.
- *Deleted-key reuse*: `db.Table.getKey()` hands out `getMaxKey() + 1`, so deleting the
  highest-keyed association and creating a new one **reuses the key**; a client still holding
  the stale (deleted, frozen-record) instance must not compare equal to the unrelated new
  association. The fast path therefore requires both operands live (`isDeleted(lock)` — two
  volatile reads when valid, and at worst the same lock+refresh the old equals already paid via
  its address getters); deleted instances fall through to the original address comparison
  against their frozen record, which preserves the old code's answer exactly (including the
  resurrect-via-undo case, where old pair == new pair ⇒ still equal). `hashCode` stays
  key-based for deleted objects — a hash collision with the key's new owner is permitted by
  the contract, since equals answers false.

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

- Done: targeted `javac` compile check of the patched file against a built runtime classpath
  (exit 0; the only compiler note is pre-existing). The full `:VersionTracking:test` suite is
  queued to run before submission (a gated pre-submission task exists for it) — it has **not**
  been run on this branch yet and the PR must not claim otherwise until it has.
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
parallel scoring phase** (Ghidra's own `generic.concurrent.ConcurrentQ`, following the in-tree
`BSimProgramCorrelatorMatching` pattern) followed by a **serial, deterministic, chunked commit
phase**, leaving the produced match set **identical** to the serial implementation (the session
DB's match *insertion order* changes to sorted-address order — see Output equivalence).
Measured **3–4×** end-to-end
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
  AutoVT has no `AutoAnalysisManager` shared pool). A failed worker's exception is rethrown by
  `QResult.getResult()` (it arrives wrapped in `Future.get()`'s `ExecutionException`) and
  surfaces wrapped as a `RuntimeException`; an item cancelled in flight yields a **null**
  result, which Phase C skips — the same null-result handling as
  `BSimProgramCorrelatorMatching`.
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
6. **Monitor discipline.** The monitor has exactly **one** progress driver: the queue's own
   `QMonitorAdapter` (default `jobsReportProgress=false` mode), which sets progress to the
   queue's cumulative completed-task count after each scored destination — the same pattern as
   the in-tree `BSimProgramCorrelatorMatching`. That count is never reset across the reused
   queue, so progress runs monotonically 0→N against the up-front `initialize(N)`. The worker
   callback never touches its `TaskMonitor` parameter at all, and Phase C only calls
   `checkCancelled()` — it does **not** increment progress (an earlier draft double-drove the
   monitor from both the adapter and the commit loop; fixed per review).

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

- Done: targeted `javac` compile check of the patched file (final state **and** the
  intermediate commit) against a built runtime classpath (exit 0; the only compiler note is
  pre-existing on stock `nsa/master`). The full `:VersionTracking:test` suite (plus
  `:Generic:test` for the `ConcurrentQ`-adjacent usage) is queued to run before submission (a
  gated pre-submission task exists for it) — it has **not** been run on this branch yet and the
  PR must not claim otherwise until it has.
- Serial-vs-parallel A/B digest equality + 3-run determinism (harness described above).
- Negative control for the memory bound: unchunked variant OOMs a 2G heap; chunked does not,
  same inputs.
- Suggested upstream addition if reviewers want it: a system-property gate
  (e.g. `-Dvt.parallel.scoring=false`) to fall back to serial scoring — trivial to add since
  Phase P/Phase C share the serial commit code; we did not include it to keep the patch minimal.

---

## Pre-submission checklist (human actions)

- [x] **Adversarial review** of the branches — done 2026-06-09
      ([ghidra-vt-upstream-pr-review-2026-06-09.md](ghidra-vt-upstream-pr-review-2026-06-09.md));
      all MAJOR findings + the cheap MINORs applied, see the changelog section below.
- [ ] **Review the three amended commits** in `/tmp/claude/ghidra-vt-pr`
      (branches `vt-assoc-equals-key`, `vt-parallel-ref-correlator`; fallback `vt-perf-fixes`).
- [ ] **Decide split vs single.** Recommendation above: two PRs.
- [ ] **Open upstream GitHub issue(s) first** (CONTRIBUTING wants commit messages to start with
      the issue number, and suggests dialogue before implementing): one perf-bug issue for the
      O(n²) duplicate-apply stall (with the N=100..700 numbers), one enhancement issue for the
      single-threaded reference correlator. Then retitle the PR-1 commit and **squash PR-2's two
      commits to one** per CONTRIBUTING, titled `#NNNN: <description>` (the amended messages are
      written so the squash message can be assembled from them; keep the corrected
      monitor/digest wording, not the old "byte-identical" claim).
- [ ] **Run the VT test suite** (`:VersionTracking:test`, plus `:Generic:test` for the
      ConcurrentQ-adjacent change) on the branch **before** opening the PRs — the PR bodies'
      test plans now explicitly say this is queued, not done; flip that wording once it has run.
      This is the gated task the bodies reference.
- [ ] **Full build verification** upstream expects: `gradle -I gradle/support/fetchDependencies.gradle`
      + `gradle prepDev` + a module build in a clean clone of the branch (our check was
      javac-against-runtime-classpath only; do NOT gradle-build the main fork tree while the
      BSim agent is running — use the worktree or a clone).
- [ ] **No CLA needed** — inbound=outbound; submitting the PR is the grant.
- [ ] **Push** the chosen branch(es) to `origin` (github.com/freeqaz/ghidra) — not done, per
      instructions — and open the PR(s) against `NationalSecurityAgency/ghidra:master` with the
      bodies above.
- [ ] **Port these same fixes back to fork master before the next `/opt` jar rebuild** — the
      deployed `/opt` jar was built from the pre-review fork commits and has the cosmetic
      double-driven-progress bug (and the wider equals fallback) too.
- [ ] Benchmarks live in rb3 `scripts/ghidra/` (`vt_dupe_benchmark.sh`, `vt_ref_ab.sh`,
      `vt_ref_benchmark.sh` + the two `*.java` scripts); offer them in the PR thread if asked
      (CONTRIBUTING forbids self-generated binaries, but these are source scripts).
- [ ] After upstreaming, the local `/opt` deploy story is unchanged (see
      [ghidra-vt-handoff-2026-06-09.md](ghidra-vt-handoff-2026-06-09.md) — 12.1.2 ABI gotcha).

---

## Review fixes applied (2026-06-09)

Adversarial-review findings
([ghidra-vt-upstream-pr-review-2026-06-09.md](ghidra-vt-upstream-pr-review-2026-06-09.md))
applied by amending the local branches. Old → new commit SHAs:

| Branch | Old | New |
|---|---|---|
| `vt-assoc-equals-key` | `43660105c8` | `236c3263da` |
| `vt-parallel-ref-correlator` (parallelize) | `a6233e1882` | `369a37441f` |
| `vt-parallel-ref-correlator` (chunk/mem) | `f9cd28fe9f` | `314bfa567c` |
| `vt-perf-fixes` (combined, re-cherry-picked) | `43660105c8`/`743a81aab0`/`13760b74a6` | `fb3a5e94f3`/`67c4040982`/`670d599c59` |

Finding → fix:

- **MAJOR-1 (monitor double-driven, false threading comment)** → Phase C no longer calls
  `incrementProgress(1)`; the queue's `QMonitorAdapter` is the **sole** progress driver
  (default `jobsReportProgress=false` mode: `taskEnded` sets progress to the cumulative
  completed count, monotonic 0→N across the reused queue against `initialize(N)`). This is the
  in-tree precedent: `BSimProgramCorrelatorMatching.discoverPotentialMatches` initializes the
  monitor to the destination count, hands it to the `ConcurrentQBuilder`, and never manually
  increments during the parallel phase. The in-code comment and PR body §Monitor discipline now
  describe exactly this. Applied to **both** PR-2 commits so the intermediate state is honest
  too. (`369a37441f`, `314bfa567c`)
- **MAJOR-2 (cross-manager equals contract violation)** → equals narrowed as the submitted
  behavior: different-manager `VTAssociationDB` pairs → `false`; same-manager + both live →
  record-key equality; non-DB `VTAssociation` (e.g. `AssociationStub`, which keeps identity
  semantics — asymmetry pre-existing and unchanged) → original address comparison.
  (`236c3263da`)
- **MAJOR-3 (overclaimed "tests pass")** → both PR-body test plans now state the honest state:
  targeted javac compile-check done; `:VersionTracking:test` queued as a gated pre-submission
  task, not yet run. (doc only)
- **MINOR-1 (deleted-key reuse)** → cheap guard added: the equals fast path requires both
  operands live via `isDeleted(lock)` (two volatile reads on valid objects — cannot regress the
  hot path, which is `hashCode`-dominated anyway; at worst it pays the same lock+refresh the old
  equals paid in its address getters). Deleted instances fall through to the original
  frozen-record address comparison, preserving old semantics exactly (incl. resurrect-via-undo).
  (`236c3263da`)
- **MINOR-3 (cancelled QResult → null neighbor map)** → BSim-style null guard at the
  consumption point: Phase C skips a destination whose neighbor map is null (only possible for
  an item cancelled in flight; `checkCancelled()` still throws first, the guard removes the
  invisible load-bearing invariant). (`314bfa567c`)
- **MINOR-4 (stale/overstated commit messages)** → parallelize-commit message now flags its
  `ConcurrentHashMap` aggregation as replaced by the follow-up commit's per-chunk `QResult`
  collection; chunk-commit message replaces "byte-identical" with the supportable claim —
  identical accepted-match set + deterministic sorted match-tuple digest, with the session DB's
  insertion order differing from serial. (`369a37441f`, `314bfa567c`)
- **MINOR-5 (inaccurate "mirroring DecompilerConcurrentQ" comment)** → comment (and commit-1
  message) rewritten to the actual mechanism: worker exceptions arrive wrapped via
  `Future.get()`'s `ExecutionException` and are rethrown wrapped as `RuntimeException`; null
  results for cancelled items. No claim about `DecompilerConcurrentQ`. (`369a37441f`,
  `314bfa567c`)
- **NIT-1 (import style / line length)** → the four `generic.concurrent` single imports
  collapsed to `import generic.concurrent.*;` (matching `DecompilerConcurrentQ`,
  `BSimProgramCorrelatorMatching`, `ConstantPropagationAnalyzer` and the file's own wildcard
  convention); the two >120-column nested-generics lines wrapped. (`369a37441f`, `314bfa567c`)

Not applied (disclosed instead): **MINOR-2** (pre-existing TOCTOU in
`getOrCreateAssociationDB`, not introduced here, all `addMatch` paths single-threaded),
**MINOR-6** (benchmark reproducibility — harness scripts offered on request),
**NIT-2** (pre-existing `ConcurrentQ.dispose()` listener wart shared by all clients),
**NIT-3** (comment heft — comments were trimmed while editing, remaining length is reviewer's
discretion).

Verification after the amendments: all three layouts re-verified content-identical per file
(`git diff` across branches empty for each touched file); `git diff --check` clean; javac
compile-check re-run PASS (exit 0) for both files on the combined branch **and** for PR-2's
intermediate commit.
