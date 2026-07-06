# W0.3c — Draw-submission-order determinism (fix, or canonical-order comparator)

**Lane A, item 1 (engine).** Unblocks W1.6 (DrawContext state-leak fix), which requires a
*deterministic* draw-log gate. Planner: Opus. Status: PLAN written, implementation pending.

## Objective

Complete the W0.3 / W0.3b safety net so the draw-log can gate W1.6. Today:

- draw **COUNT** is deterministic under `--fixed-clock` (888× exact — W0.3b.S2 closed the
  count-jitter root cause: wall-clock boot RNG seed).
- draw **SUBMISSION ORDER** is **~33% flaky** run-to-run: the W0.3b VERIFY 15-run sweep saw
  `PASS PASS PASS PASS PASS FAIL FAIL FAIL FAIL FAIL PASS PASS PASS PASS PASS` (5/15 FAIL),
  each FAIL reporting 336/354 **unexpected** (non-residual) divergences that are **mesh-identity
  swaps** — `skinned`/`pipe`/`idx`/`tris`/`verts` all differ at the diverging index, i.e. two draws
  changed *places*, not one draw's transform jittering. Byte-identical binaries disagree run-to-run
  even under `setarch -R` (ASLR off) + frozen clock + fixed seed + forced loader drain. This is a
  live **SYS-3** (order-dependent-state) instance the harness exposes.

Reach **Exit A** (root-cause + fix → exact-order golden green 15/15) if a bounded, render-neutral
fix is found within the timebox; otherwise reach **Exit B** (deterministic order-insensitive
canonical-order comparator) which unblocks W1.6 regardless. **Exit B (S3) is landed unconditionally**
so the riskiest downstream item (W1.6) never launches on a probabilistic gate — see Exit criteria.

**Timebox:** ~2 agent-sessions of diagnosis (WAVE3_REVIEW amendment C1).

## Faithful-reference citations (current tree — re-grepped post-Wave-2; all line numbers verified)

Engine `milo-native-engine` @ pin-adjacent HEAD `41b9e3a`:

- **PRIMARY LEAD — non-stable transparent sort.** `src/platform/TransparentQueue.cpp`:
  - `DeferredDraw` struct (`:23-28`): `{ RndMesh* mesh; float distSq; RndCam* cam; RndEnviron* env; }`
    — **no insertion-order field**.
  - `FlushTransparentDraws()` (`:106-146`) sorts with a **non-stable `std::sort`** on a
    **strict `a.distSq > b.distSq`** comparator (`:120-124`), then submits in sorted order via
    `DrawMeshImmediate` (`:132`). Two consequences, both order-nondeterministic:
    (i) *ties* (equal `distSq`) have unspecified relative order that depends on the vector's input
    order (which depends on traversal/allocation order); (ii) *near-ties* flip whenever `distSq`
    jitters across the comparison boundary. The W0.3b.S3 residual is exactly **26 eye/face draws with
    ~2u world-jitter** (`splash_screen.fixedclock-residual.json`, eps 3.0) — that jitter feeds
    `distSq` and flips the order of near-equal-depth transparent draws. **This is the most likely
    amplifier turning the bounded ~2u world residual into full mesh-identity swaps** (one adjacent
    swap = ~13 field divergences × the swapped draws ≈ the 336/354 the verifier saw).
  - Same non-stable pattern in `FlushTextDraws()` (`:88-104`) — text queue is **not** sorted (submits
    in queue order) so it is order-stable *iff* its input order is; note it as a secondary check site.
- **SECONDARY LEAD — allocation-sequence divergence (opaque draws).** Opaque draws submit in
  scene-graph **traversal order** (no deferred queue), so any opaque mesh-identity swap means the
  *object-list order* or a pointer-order-dependent iteration varied. ASLR is off yet skinned body
  draws still swap → the divergence is **allocation-SEQUENCE**, not load-address, per WAVE3_REVIEW C1
  lead (b). Suspect upstream **async loader / worker-thread interleaving**: real pthread worker in
  `src/platform/ThreadCall_Native.cpp` (`WorkerMain`, 12-slot ring; `#ifndef __EMSCRIPTEN__`).
  Loader drain (W0.3b.S2, `Loader.cpp`) forces *completion*, not completion *order*.
- **EXONERATED (keep as pattern reference, do not re-chase as direct cause).**
  `src/platform/RB3MeshCache.h:119,142` `std::unordered_map<RndMesh*,…> sMeshGpu` / `sGeomSyncGen`
  — verifier confirmed point-queried only (`sMeshGpu[mesh]`/`.find`), never iterated for submission.
  Indicative of the *class* of bug (pointer-keyed unordered containers), not the instance.
- **Comparator to extend (Exit B) + residual to leave UNCHANGED.**
  `rb3/scripts/native/drawlog-golden.py`: `compare_drawlogs()` (`:373`), scalar key set
  `SCALAR_FIELDS` (`:356`, includes `pipe,blend,zmode,layout,fmt,hasDepth,alphaCut,alphaWrite,
  skinned,idx,tris,verts,name`), `SHARE_STREAMS` bind-group-collapse check (`:362` +
  `:395-405`), `world_elem_ok()` tolerance (`:365`), and the residual-exception layer
  `compare_fixed_clock()` (`:218`) + `load_residual()` (`:138`). The `--fail-red-audit` harness is at
  `:477-490`.
- **Do NOT edit `src/App.cpp`** (WAVE3_KICKOFF D2 + W0.3b already placed the fixed-clock seams there;
  reuse `RB3FixedClockActive()`/`RB3_FIXED_CLOCK`).

## Root-cause hypothesis (to confirm/refute in S1, not assume)

The order flake is **two mechanisms**, in decreasing suspected weight:

1. **Transparent-sort instability** (`TransparentQueue.cpp`): near-equal-`distSq` transparent draws
   (the jittering CharEyes/face meshes + coincident-depth quads) swap under the non-stable
   `std::sort`. **Bounded and render-neutral to fix** (back-to-front order of equal-depth alpha
   draws is visually identical) → candidate Exit-A fix.
2. **Allocation-sequence divergence** (async loader / `ThreadCall` worker interleaving) feeding
   pointer values into an opaque-traversal / skin-pose iteration. **Deep, possibly multi-site, NOT
   assumed fixable in the timebox** → if this dominates, go Exit B and file W0.3d.

S1 must **attribute** the flake between these (and any third source) before S2 commits to a fix.

## Subtasks

### W0.3c.S1 — Diagnose: attribute where submission order first diverges (model: opus)

**Goal.** Across ≥15 fresh `--fixed-clock` boots, dump submission order at 2-3 pipeline stages,
bisect where the order *first* diverges, and attribute the flake to mechanism 1 (transparent sort),
mechanism 2 (allocation/traversal order), or another. Produce a written GO/NO-GO for a bounded
Exit-A fix. **This is the timebox-critical subtask.**

**Exact files.**
- Engine: `src/platform/TransparentQueue.cpp` (add a default-OFF probe: dump pre-sort and post-sort
  order — mesh ptr, name-hash, `distSq`, insertion-seq — to stderr under a new flag).
- Engine: `src/platform/Mesh_Wgpu.cpp` (probe at `RecordDrawCall` / `DrawMeshImmediate` for the final
  submission-order stream, i.e. opaque + flushed transparent interleaved) **only if** the existing
  `RB3_DRAWLOG` ring is insufficient to see opaque-vs-transparent ordering; prefer reading order from
  the existing JSON dump first and add the pre/post-sort probe only in `TransparentQueue.cpp`.
- Engine: `src/platform/NativeCompatFlags.classification.json` (register the new probe flag) +
  regenerated `src/platform/NativeCompatFlags.gen.inc`.
- rb3: `docs/.../NATIVE_COMPAT_LEDGER.md` (regen via census tool) + this dir's `STATUS.md`.
- rb3 (diagnostic driver, no engine change): a throwaway analysis under `scripts/native/` is
  permitted but **prefer** an ad-hoc shell/python loop recorded in STATUS over a committed script.

**Step-by-step.**
1. Add a probe flag `RB3_DRAWORDER_TRACE` (class `probe`, default off, presence-read) that, in
   `FlushTransparentDraws()`, prints the queue **before** and **after** the sort (index, `mesh`
   pointer, `RB3DebugHashName(mesh->Name())` or the same name-hash the drawlog uses, `distSq`, and a
   per-flush monotonic insertion-seq). Register it in the classification sidecar; regen `.gen.inc`
   and the ledger; `python3 scripts/analysis/native_compat_census.py check` must exit 0 and
   `--selftest` pass. This probe is **inert** (default-off, additive) — no fail-red needed (probe
   class), but confirm off-path output is byte-identical (run with/without the flag → identical
   drawlog JSON).
2. Build `rb3-native` in the OWN dir (`native/build-agent-W0.3c`, clang/clang++).
3. Run the `--fixed-clock` capture **≥15 times** (`RB3_FIXED_CLOCK=1 RB3_DRAWLOG=1
   RB3_DRAWORDER_TRACE=1 MILO_MAX_FRAMES=60`, under `setarch -R`), capturing both the drawlog JSON
   and the pre/post-sort trace each run. For each run, record: the ordered mesh-name-hash sequence of
   (a) opaque submissions, (b) transparent pre-sort, (c) transparent post-sort.
4. **Bisect the divergence:** compare the 15 runs pairwise per stage. Determine the earliest stage at
   which the sequence differs. Log **mesh pointer values** at that stage to confirm/refute
   allocation-order divergence (pointers differ run-to-run under ASLR-off ⇒ allocation-sequence
   varies).
5. **Attribute:** quantify what fraction of the ~336/354 unexpected divergences are (i) transparent
   post-sort swaps of near-equal-`distSq` draws (mechanism 1), (ii) opaque-traversal / pre-sort input
   swaps (mechanism 2). Cross-reference the swapped indices against the 26 residual eye/face draws.
6. Write the attribution + GO/NO-GO to `STATUS.md`: **GO** if a bounded, render-neutral fix
   (e.g. total-order comparator + insertion-seq tiebreak + sub-eps `distSq` quantization in
   `TransparentQueue.cpp`) plausibly eliminates ≥ the transparent share AND the residual opaque share
   is either zero or itself trivially stabilizable; **NO-GO** if opaque allocation-order divergence
   dominates and needs multi-site engine work beyond the timebox (then S2 is skipped → Exit B only,
   and file W0.3d).

**Verification.**
- `python3 scripts/analysis/native_compat_census.py --selftest` → all PASS; `… check` → exit 0.
- `cmake --build native/build-agent-W0.3c --target rb3-native -j8` → `Built target`.
- Off-path identity: one capture with `RB3_DRAWORDER_TRACE` unset vs set → **identical** drawlog JSON
  (`md5sum` equal); the probe only adds stderr.
- STATUS carries the 15-run per-stage sequences (or a digest), the earliest-divergence stage, the
  pointer-order evidence, the mechanism attribution, and the GO/NO-GO verdict.

### W0.3c.S2 — Exit-A fix: deterministic transparent submission order (model: opus) — **CONDITIONAL on S1 = GO**

**Goal.** Land the bounded, render-neutral determinism fix S1 identified so the exact-order
`--fixed-clock` golden is green **15/15**. Most-likely shape (confirm against S1): give `DeferredDraw`
a monotonic **insertion-seq**, replace the non-stable `std::sort` with a **total-order** comparator
`(bucket(distSq), insertionSeq)` where `bucket()` quantizes `distSq` to a grid coarser than the
residual jitter (so sub-eps `distSq` wobble can no longer reorder), tiebreaking by insertion-seq —
OR `std::stable_sort` on `distSq` + insertion-seq if S1 shows ties, not near-tie flips, dominate.
**Do NOT** tiebreak on raw pointer values (that is the very nondeterminism being removed).

**Behavior-changing subtask — required staging (WAVE3_KICKOFF, hard rule 1):**
- This is a **CHANGE** commit (never mixed with a MOVE). It alters submission order *only for
  draws whose visual result is order-independent* (equal/near-equal-depth alpha draws in a
  back-to-front pass); document why the change is render-neutral (alpha compositing of coincident-
  depth fragments) and prove it with the lineup + 2-scene screenshot gate below.
- **Default staging.** A pure-determinism reorder of a back-to-front alpha sort is render-neutral by
  construction, so it may land **default-ON** *only if* the S1 render-neutrality argument holds AND
  the visual gate is green. If S1 leaves any doubt, gate behind a new registered flag
  `RB3_DETERMINISTIC_TRANSPARENT_SORT` (class `feature`, default off), flip default in a **separate
  one-line commit** gated on the visual + 15-run evidence (mirrors W2.2's B1 anti-revert staging).
  Register the flag in the W0.6 registry at introduction (`census check` exit 0).
- **Fail-red demonstration (required).** Prove the exact-order gate would catch a real order
  regression: with the fix in place, temporarily perturb the comparator to re-introduce a swap (e.g.
  `MILO_TRANSPARENT_SORT_BREAK=1` flips two equal-bucket draws) and show `--fixed-clock`
  `compare_drawlogs`/`--fail-red-audit` goes **RED**; revert. Record the RED output in STATUS.

**Exact files.**
- Engine: `src/platform/TransparentQueue.cpp` (+ `src/platform/TransparentQueue.h` if `DeferredDraw`
  or the queue API needs the insertion-seq field exposed).
- Engine: `src/platform/NativeCompatFlags.classification.json` + regen `.gen.inc` (if a flag is added).
- rb3: `docs/.../NATIVE_COMPAT_LEDGER.md` regen; `STATUS.md`; possibly re-capture
  `native/tests/goldens/drawlog/splash_screen.json` **only if** the fix legitimately changes the
  canonical order of equal-depth draws (record the justification — a determinism reorder is NOT a
  behavior change, so a re-capture must be argued and the residual sidecar must **shrink or stay
  equal**, never grow).
- **AVOID** editing `src/platform/Rnd_Wgpu_RB3.cpp` here (W1.6's file; keep S2 self-contained in
  `TransparentQueue.*`). If S1 proves an opaque skin-pose iteration site in `Rnd_Wgpu_RB3.cpp` must
  change, STOP and record it as W0.3d instead of editing the W1.6 hotspot in this lane.

**Verification (Exit A bar).**
- `scripts/native/drawlog-golden.py --fixed-clock` green **15/15** fresh boots (the 3-run bar is
  statistically worthless at 33% flake — WAVE3_REVIEW C1).
- `--fixed-clock --fail-red-audit` still **RED**; residual sidecar **unchanged or shrunk** (never
  widened to swallow order swaps — verifier's explicit warning, W0.3b STATUS).
- Visual gate: `scripts/native/lineup-gate.py` PASS + 2-scene `/api/screenshot` A/B (pre/post fix)
  reviewer-judged unchanged.
- `rb3-tests --gtest_filter='*DrawLog*'` unchanged (9 pass / 1 skip).
- Engine suite invariance: `milo-engine-tests` **198 pass / 0 fail / 2 skip** (DC3-context; a red
  there = the change leaked into shared render math → STOP).
- `MILO_ENGINE_PIN` **unchanged** (coordinator bumps per wave).

### W0.3c.S3 — Exit-B: order-insensitive canonical multiset comparator mode (model: sonnet) — **UNCONDITIONAL**

**Goal.** Land a deterministic, order-insensitive comparator mode in `drawlog-golden.py` so W1.6 has a
**non-probabilistic** gate whether or not S2 reaches Exit A. Keeps every defect class W1.6 risks:
exact count, bind-group-collapse (a0f98ad-class multi-draw uniform collapse), world-xfm change,
pipeline/blend/count change, mesh add/remove. Insensitive **only** to the one known, real bug —
submission order. This is *not* widening the residual sidecar (categorically different: the residual
bounds a per-draw world-jitter magnitude on 26 named draws; the multiset makes the *comparison*
order-agnostic while keeping every value check).

**Exact files.**
- rb3: `scripts/native/drawlog-golden.py` (new `--canonical-order` mode + `compare_canonical()`;
  legacy modes byte-identical).
- rb3: `STATUS.md`. (No golden re-capture — reuses the committed `splash_screen.json`.)

**Design (precise — the meat of the multiset comparator).**
1. **Count gate (exact, kept):** `len(golden.draws) == len(candidate.draws)` else FAIL.
2. **Multiset membership by scalar-key (order- AND world-jitter-insensitive):** key each draw by the
   full exact scalar tuple from `SCALAR_FIELDS` **excluding `world`** — i.e.
   `(name, pipe, blend, zmode, layout, fmt, hasDepth, alphaCut, alphaWrite, skinned, idx, tris,
   verts)` (this is a *superset* of the brief's `(name, pipeline, blend, counts)` — strictly
   stronger). Golden `Counter(keys)` must equal candidate `Counter(keys)`. Catches: draw
   appear/disappear, count change, pipeline/blend change, mesh-identity substitution — all
   order-insensitively. Report exact bucket deltas on mismatch.
   *Rationale for excluding `world` from the membership key:* the residual permits ~2u world jitter on
   26 draws; folding a quantized `world` into the membership key would make those keys wobble
   run-to-run and defeat the multiset. World is verified in step 3 instead.
3. **World-xfm verification on matched pairs (tolerance + residual, kept):** within each scalar-key
   bucket, establish a golden↔candidate correspondence (bucket size 1 → trivial; size >1 → greedy
   match by minimum summed `|world[e]|` delta). For each matched pair apply the **existing**
   `world_elem_ok()` tolerance; reuse the **unchanged** residual sidecar (`load_residual`) to tolerate
   the 26 known eye/face draws (matched by name-hash, not index). Any out-of-bound world delta on a
   non-residual draw → FAIL.
4. **Bind-group-collapse check (kept, order-insensitive):** using the step-3 correspondence to map
   golden index → candidate index, run the `SHARE_STREAMS` pairwise sharing check
   (`scene`/`mat`/`obj`/`bone`) on **corresponding** pairs. Preserves the a0f98ad multi-draw uniform-
   collapse detection under reordering. (If a bucket's correspondence is ambiguous, report a
   diagnostic but still fail on any sharing-structure change.)
5. **Legacy untouched:** `compare_drawlogs`, `compare_fixed_clock`, and all existing modes are
   byte-identical; `--canonical-order` is a new dispatch branch.

**Fail-red demonstration (required, all four classes).** With `--canonical-order`, prove RED on:
(a) count change (drop a draw); (b) bind-group collapse (make two `obj`-distinct draws share);
(c) world-xfm out-of-bound on a non-residual draw (perturb by ≫ eps); (d) mesh-identity change
(swap a `name`/`pipe`/`verts` on one draw). And prove GREEN on a pure order permutation of a golden
against itself (shuffle draw order → still passes). Record all in STATUS.

**Verification (Exit B bar).**
- `scripts/native/drawlog-golden.py --fixed-clock --canonical-order` green **15/15** fresh boots.
- The four fail-red classes above each go RED; the pure-permutation case stays GREEN.
- Legacy modes unchanged: `--fixed-clock` (probabilistic, retained as diagnostic) and
  `--fail-red-audit` behave exactly as before on the same inputs.
- `rb3-tests --gtest_filter='*DrawLog*'` unchanged (9 pass / 1 skip; `drawlog_compare.h` untouched).

## Exit criteria (measurable)

Layered; **W1.6 launches iff the item reaches Exit A OR Exit B** (WAVE3_REVIEW C2).

- **L0 (S1, mandatory):** ≥15-run per-stage submission-order attribution written to STATUS with the
  earliest-divergence stage, pointer-order evidence, and a GO/NO-GO verdict for a bounded fix. Probe
  flag registered (`census check` exit 0), off-path drawlog byte-identical.
- **Exit A (S2, if S1=GO):** `--fixed-clock` exact-order golden green **15/15** fresh boots;
  `--fail-red-audit` still RED; residual sidecar unchanged or shrunk; lineup + 2-scene screenshot
  reviewer-unchanged; `milo-engine-tests` 198/0/2; fail-red demo (comparator-break) RED recorded;
  `MILO_ENGINE_PIN` unchanged. → order root-cause CLOSED.
- **Exit B (S3, unconditional):** `--canonical-order` mode green **15/15** fresh boots; all four
  fail-red classes RED + pure-permutation GREEN; exact-count, bind-group-collapse, world-xfm(+residual
  unchanged) all retained; legacy modes byte-identical. → deterministic gate for W1.6 exists even if
  Exit A not reached. If Exit A not reached, **file the order root-cause as W0.3d for Wave 4.**
- **CharEyes/CharLookAt look-at determinism is a STRETCH, NOT an exit** (it would delete the bounded
  26-draw residual sidecar). Do not let it gate W0.3c.

## Files touched (exact repo-relative — coordinator cross-diffs lanes)

Engine (`/home/free/code/milohax/milo-native-engine`):
- `src/platform/TransparentQueue.cpp` (S1 probe, S2 fix)
- `src/platform/TransparentQueue.h` (S2, only if `DeferredDraw`/API needs the insertion-seq field)
- `src/platform/Mesh_Wgpu.cpp` (S1 probe — ONLY if the existing `RB3_DRAWLOG` dump can't separate
  opaque vs transparent order; prefer not to touch)
- `src/platform/NativeCompatFlags.classification.json` + `src/platform/NativeCompatFlags.gen.inc`
  (S1 probe flag; S2 feature flag if staged)

rb3 (`/home/free/code/milohax/rb3`):
- `scripts/native/drawlog-golden.py` (S3 canonical-order mode; S2 has no script edit unless a
  justified golden re-capture)
- `native/tests/goldens/drawlog/splash_screen.json` (S2 ONLY if a justified equal-depth re-capture;
  default: untouched)
- `native/tests/goldens/drawlog/splash_screen.fixedclock-residual.json` (may SHRINK under S2; never
  grow — verifier warning)
- `docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md` (regen for any new flag)
- `docs/native/engine-arch-review-2026-07-05/execution/W0.3c/STATUS.md` (append under
  `flock /tmp/rb3-docs.lock`)

**Explicitly NOT touched:** `src/App.cpp` (D2 — reuse W0.3b seams); `src/platform/Rnd_Wgpu_RB3.cpp`
(W1.6's hotspot — S2 stays in `TransparentQueue.*`; if an opaque site there needs changing, defer to
W0.3d); `native/build-native`, `native/build-web*`, `build-coord-*`.

## Risks / conflicts

- **Lane A sequencing (W0.3c → W1.6) on the engine backend.** W1.6 rewrites
  `WriteSceneUniforms`/`mSceneBindGroup` in `src/platform/Rnd_Wgpu_RB3.cpp`. W0.3c is deliberately
  scoped to `src/platform/TransparentQueue.*` + the flag sidecars to keep the two **file-disjoint**.
  They run sequentially in the same lane (same agent), so no concurrent-edit hazard — but if S2's
  diagnosis forces an `Rnd_Wgpu_RB3.cpp` skin-pose iteration change, **do not** make it here (it
  would collide with W1.6's rewrite region); record it as W0.3d.
- **W2.2 (Lane B, rb3-only, engine READ-ONLY)** — no overlap; it lives in the char/skeleton load
  path and touches no engine `.cpp` (uses `mNativeBonesRebound`/`RB3_GUARD_EXEMPT_REBOUND=1` seams).
- **W2.5 (Lane C, rb3-only, game-side waypoint assert)** — no overlap.
- **W3.1 was DEFERRED to Wave 4** (E1: collides with W1.6 at `WriteSceneUniforms`), so no Lane-C
  render collision this wave.
- **Exit-A render-neutrality risk.** A determinism reorder of the back-to-front alpha sort is
  render-neutral *only* for coincident/near-coincident-depth alpha draws. If S1 finds swaps between
  meaningfully different depths (would change compositing), S2 must gate behind the default-OFF flag
  and NOT flip default — worst case is an unflipped flag + Exit B, never a visual regression.
- **Statistical-power risk.** 3-run checks are worthless at 33% flake; every Exit-A/B green claim
  requires **15** fresh boots (WAVE3_REVIEW C1). Bake the 15-run loop into every verification.
- **Residual-sidecar discipline.** Do NOT fold order swaps into `splash_screen.fixedclock-residual.json`
  (verifier's explicit warning) — order is a different bug class; Exit B handles it by comparison
  design, not by widening tolerance.
