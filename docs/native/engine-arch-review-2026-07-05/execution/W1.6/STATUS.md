# W1.6 — Immutable scene bind group + DrawContext — STATUS

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask, written under
`flock /tmp/rb3-docs.lock`, with commit SHAs (engine + rb3), the per-commit gate evidence
(2-scene screenshot md5 A/B, drawlog `--fixed-clock --canonical-order` 15-run FAIL-class
classification, `rb3-tests DrawLogGolden.*`, `lineup-gate.py`, `milo-engine-tests` 198/0/2), and
the S3 fail-red (mis-threaded `ctx.scene` → gate RED on a non-residual divergence, reverted).
Re-runs read this + `git log --grep=W1.6` and skip done work.

Gate precondition: W0.3c reached **Exit B** (canonical comparator landed). CAVEAT: the draw-log gate
reads probabilistically RED from the pre-existing CharEyes/CharLookAt residual (W0.3d) — run it as an
A/B differential with the residual-name filter (PLAN §"Gate protocol" step 2) and rely on the
regime-stable screenshot/unit/lineup nets for the hard signal.

Engine pin `41b9e3a` (do NOT bump — coordinator bumps per wave). Engine HEAD entering W1.6: `5cee522`.

_(No subtask entries yet — planning complete, implementation pending.)_

## W1.6.S1 — done

**Commit:** engine `9df8349` (`git -C /home/free/code/milohax/milo-native-engine log --grep=W1.6`).
Files (MOVE-class, byte-identical additive plumbing): `src/platform/Rnd_Wgpu_RB3.{h,cpp}`.
Engine HEAD entering S1: `5cee522` (pin `41b9e3a` NOT bumped — coordinator bumps).

**What landed (per PLAN §W1.6.S1):**
- `Rnd_Wgpu_RB3.h`: added file-scope `struct RB3SceneBinding { wgpu::BindGroup group; uint32_t offset; }`
  (before `class BandRnd` so the return type is visible at the decl); changed decl to
  `RB3SceneBinding WriteSceneUniforms(RndCam* cam);`; added value member `RB3SceneBinding mActiveScene;`
  next to the legacy pair. `mSceneBindGroup`/`mSceneOffset` retained (transitional mirror — S2 deletes).
- `Rnd_Wgpu_RB3.cpp`: out-of-line defn signature `RB3SceneBinding BandRnd::WriteSceneUniforms(...)`;
  body verbatim through the `CreateBindGroup`/pose-latch, then
  `return RB3SceneBinding{ mSceneBindGroup, mSceneOffset };`. 3 call sites now assign
  `mActiveScene = WriteSceneUniforms(...)`: BeginFrame (was :1552), DrawMesh camChanged re-pose
  (was :2186), DrawMesh per-environ re-write (was :2202). All `SetBindGroup(0, mSceneBindGroup, ...)`,
  the halo capture, the drawlog, and the `:2473` offset read left UNCHANGED (still read legacy members).

**Build:** `native/build-agent-W1.6` (clang) rb3-native + rb3-tests green. Baseline (pre-S1) built
from an engine worktree at `5cee522` via `-DMILO_ENGINE_PATH=` into `native/build-baseline-W1.6`
(no shared-tree mutation; worktree removed after).

**Gate evidence (per PLAN "Gate protocol"):**
- **Fixed-clock drawlog A/B (baseline bin vs modified bin, `compare_canonical`):** both 888 draws /
  frame 60; **0 unexpected** divergences, 208 residual eye-jitter diffs present in BOTH;
  **scene-bind-group token multiset IDENTICAL** (20 distinct each) => no a0f98ad-class bind-group
  collapse. This is the primary byte-identical proof at the draw-call level (stronger than a static
  screenshot for a bind-group refactor).
- **Committed-golden canonical sweep** (`--fixed-clock --canonical-order`): SAME distribution on
  baseline AND modified — **72 unexpected** on each (231 vs 233 residual-tolerated; the ±2 is
  order-flake in which draws land in the residual bucket, not a W1.6 effect — the direct A/B is 0).
  The 72 are the pre-existing CharEyes/CharLookAt world-jitter residual (W0.3d), NOT a new class.
- **rb3-tests DrawLogGolden.\*:** 9/10 pass incl. CatchesBindGroupCollapse / CatchesCoLocation /
  CatchesDroppedDraw / CatchesPipelineChange; `PopulatesFromRealDrawMesh` GTEST_SKIPs then SIGSEGVs
  at teardown — **IDENTICAL on the baseline build** (pre-existing fixture teardown crash, not W1.6).
- **lineup-gate.py** (`--bin build-agent-W1.6/rb3-native`): **PASS** img/segA/ratioB/countC/pin;
  0 slivers, max_band_ratio 3.61, no ratio regression.
- **milo-engine-tests (DC3-context invariance net):** **198 pass / 0 fail / 2 skip** (200 total;
  ExtractBik.ExtractSmallest + SkinGolden.CaptureGolden skipped). Note: this suite is the DC3 WgpuRnd
  backend and does NOT compile `Rnd_Wgpu_RB3.cpp` — invariant to S1 by construction; run anyway.

**Screenshot-hash note (PLAN "PRIMARY"):** substituted the fixed-clock drawlog A/B (frame-content
deterministic, bind-group-handle-level) for the `/api/screenshot` md5 pair — for a live HTTP boot the
menus animate on wall-clock so two independent boots don't md5-match even with identical code, whereas
the `RB3_FIXED_CLOCK` bounded boot is deterministic and additionally verifies per-draw bind-group
identity that a PNG hash cannot see. Deviation recorded here per protocol.

**Remains:** S2 (repoint the 9 group-0 reads to `mActiveScene.*`, delete the legacy members —
`grep -c mSceneBindGroup` still >0 as intended for S1), S3 (`RB3DrawContext`+`SubmitDraw`+fail-red),
S4 (sweep+comment cleanup+STATUS). **Blockers:** none.

## W1.6.S2 — done

**Commit:** engine `01c2642` (`git -C /home/free/code/milohax/milo-native-engine log --grep=W1.6`).
Engine HEAD entering S2: `9df8349` (S1). Pin `41b9e3a` NOT bumped (coordinator bumps). MOVE-class,
byte-identical. Stage: only my files (`git add <path>` × 5); sibling's uncommitted `FxSendNative.cpp`
left untouched.

**Files (5, all MOVE / byte-identical):**
- `src/platform/Rnd_Wgpu_RB3.h` — deleted `wgpu::BindGroup mSceneBindGroup` + `uint32_t mSceneOffset`
  decls (was :289-290); reworded the `mActiveScene` doc-comment + the halo-replay/HaloDraw comments to
  name `mActiveScene.group`.
- `src/platform/Rnd_Wgpu_RB3.cpp` — `WriteSceneUniforms` now builds a local `RB3SceneBinding sb`
  (`sb.offset = mSceneRing.Write(...)`, descriptor `e[0].offset = sb.offset`, `sb.group = CreateBindGroup`),
  latches `mActiveScene = sb;` and `return sb;` (the legacy `mSceneBindGroup=`/`mSceneOffset=` stores
  removed). Repointed all group-0 reads: the frame reset (`mActiveScene = {};`, was :970), the 6
  in-file `SetBindGroup(0, mActiveScene.group, ...)` bind sites, the halo capture push
  (`mActiveScene.group`), `RecordDrawLog(... mActiveScene.group.Get() ...)`, and the MESH_FOOT
  fprintf (`mActiveScene.offset`, was :2478).
- **`src/platform/RB3Quad.cpp` + `src/platform/RB3PostProc.cpp`** — repointed the **3** additional
  group-0 `SetBindGroup(0, mActiveScene.group, ...)` reads that live in the Wave-2-extracted BandRnd
  methods (RB3Quad DrawRect restore ×2, RB3PostProc pass-open). See PLAN DEVIATION below.
- `src/platform/RB3DrawLogDebug.h` — comment-only (`sceneBG` field now names `mActiveScene.group.Get()`).

**PLAN deviation (recorded per protocol):** PLAN.md §W1.6.S2 step 5 scoped the `grep = 0` to
`Rnd_Wgpu_RB3.{cpp,h}` and enumerated "9 group-0 bind sites." Wave 2 (post-plan) extracted
`RB3Quad.cpp`/`RB3PostProc.cpp`, which carry **BandRnd** member methods that also read
`mSceneBindGroup` (RB3Quad :452,:533; RB3PostProc :81) — deleting the member broke their compile.
Fixed the 3 sites identically (`mActiveScene.group`); no behavior change (same member, same handle).
The DC3 `WgpuRnd` class in `Rnd_Wgpu.{cpp,h}` has its OWN unrelated `mSceneBindGroup` — **left
untouched** (different class). `RB3HaloPass.{cpp,h}` comments still say "mSceneBindGroup HANDLE" —
left untouched per brief ("RB3HaloPass.cpp untouched"); those describe the captured `HaloDraw.scene`
field, still a `wgpu::BindGroup`.

**grep proof:** `grep mSceneBindGroup|mSceneOffset` over the 3 RB3-flavor TUs (`Rnd_Wgpu_RB3.cpp`,
`RB3Quad.cpp`, `RB3PostProc.cpp`) = **0 hits**. Only surviving hits are explanatory comments
(`Rnd_Wgpu_RB3.h:295` "…replaces the former mutable mSceneBindGroup/mSceneOffset pair", RB3HaloPass
comments) — allowed by PLAN step 5 ("only comments may remain").

**Build:** `native/build-agent-W1.6` (clang) `rb3-native` + `rb3-tests` green (rc=0).

**Gate evidence (per PLAN "Gate protocol"):**
- **Fixed-clock drawlog A/B (PRIMARY, S1 method): baseline bin (S1 `9df8349`, built from an engine
  worktree via `-DMILO_ENGINE_PATH=` into a throwaway dir, no shared-tree mutation) vs S2 bin,
  `compare_canonical`:** both **888 draws / frame 60**; **gate_passed=True, 0 unexpected**, 267
  residual eye-jitter diffs present in BOTH (W0.3d CharEyes/CharLookAt class, sidecar-tolerated).
  SHARE_STREAMS bind-group-collapse check clean. The index-keyed `compare_fixed_clock` "fails" only on
  the W0.3c boot-to-boot draw-ORDER flake (draw 33+ = a different mesh at the same index) — exactly
  why W0.3c built the canonical comparator; NOT a content change.
- **Explicit scene bind-group token partition (a0f98ad net):** baseline **20 distinct** scene tokens,
  size-multiset `{335,243,92,67,55,25,19,10,9,8,6,4,3,3,3,2,1,1,1,1}`; S2 **identical** (20 distinct,
  same partition). No multi-draw uniform-collapse. Matches S1's "20 distinct each."
- **rb3-tests DrawLogGolden.\*:** 9/10 pass incl. **CatchesBindGroupCollapse** / CatchesCoLocation /
  CatchesDroppedDraw / CatchesPipelineChange; `PopulatesFromRealDrawMesh` GTEST_SKIP → teardown
  SIGSEGV — pre-existing, IDENTICAL to S1 baseline (not W1.6).
- **lineup-gate.py** (`--bin build-agent-W1.6/rb3-native`): **PASS** img/segA/ratioB/countC/pin
  (4 frames coop_g_n03 + coop_g_b, max_band_ratio 5.19, 0-6 slivers — no shard/ratio regression).
- **milo-engine-tests (DC3-context invariance net):** **198 pass / 0 fail / 2 skip** (200 total;
  ExtractBik.ExtractSmallest + SkinGolden.CaptureGolden skipped). SkinGolden.*/ClipPoseFixture.*
  green — no leak into shared draw/skin code.

**Screenshot-hash note:** as in S1, substituted the fixed-clock drawlog A/B (deterministic,
bind-group-handle-level) for the `/api/screenshot` md5 pair — a live HTTP boot animates on wall-clock
so two boots never md5-match even with identical code, whereas `RB3_FIXED_CLOCK` is deterministic and
additionally proves per-draw scene-bind-group identity a PNG hash cannot see. Deviation recorded.

**Remains:** S3 (`RB3DrawContext` + `SubmitDraw` + fail-red demo), S4 (final sweep + comment cleanup +
STATUS). `grep -c mSceneBindGroup` now 0 in code (S2 goal met). **Blockers:** none.

## W1.6.S3 — done

**Commit:** engine `6221a56` (`git -C /home/free/code/milohax/milo-native-engine log --grep=W1.6`).
Engine HEAD entering S3: `01c2642` (S2). Pin `41b9e3a` NOT bumped (coordinator bumps). MOVE-class,
byte-identical. Staged only my 2 files (`git add src/platform/Rnd_Wgpu_RB3.{cpp,h}`); sibling's
uncommitted `FxSendNative.cpp` left untouched (rule 8, verified `git status` post-commit).

**Files (2, MOVE / byte-identical):**
- `src/platform/Rnd_Wgpu_RB3.h` — added file-scope `struct RB3DrawContext { RB3SceneBinding scene;
  const float* world; wgpu::RenderPipeline pipe; wgpu::BindGroup mat, obj, bone; wgpu::Buffer vbuf,
  ibuf; uint32_t indexCount; }` (after `RB3SceneBinding`, before `class BandRnd` so it's visible at
  the private-method decl); added private decl `void SubmitDraw(const RB3DrawContext& ctx);` after
  `WriteSceneUniforms`.
- `src/platform/Rnd_Wgpu_RB3.cpp` — added out-of-line `BandRnd::SubmitDraw` = the EXACT
  `SetPipeline / SetBindGroup(0=ctx.scene.group,1=ctx.mat,2=ctx.obj,3=ctx.bone) / SetVertexBuffer /
  SetIndexBuffer / DrawIndexed(ctx.indexCount,1,0,0,0)` block verbatim from the former inline draw
  (was DrawMesh final block). DrawMesh now builds `RB3DrawContext ctx{ mActiveScene, obj.world, pipe,
  matBG, objBG, boneBG, vbuf, ibuf, cachedIndexCount };` right after `pipe` is resolved (before the
  halo-capture branch); halo `push_back` uses `ctx.*` fields; the inline draw block is replaced by
  `SubmitDraw(ctx);`; `RecordDrawLog(key, ctx.world, ctx.scene.group.Get(), ctx.mat.Get(),
  ctx.obj.Get(), ctx.bone.Get(), ctx.indexCount, ...)`. DrawParticles builds a local `RB3DrawContext
  ctx{}; ctx.scene = mActiveScene;` and reads `ctx.scene.group` at both group-0 binds (its group-1
  texBG + own VB/IB sizes + single-arg DrawIndexed differ, so it does NOT route through SubmitDraw —
  brief explicitly allows a lighter particle-context as long as ctx.scene is explicit).

**PLAN adherence:** matches PLAN §W1.6.S3 steps 1-4 exactly. No deviation. Each draw's scene
dependency is now a visible ctx value it was handed, not an implicit last-write-wins member read —
SYS-3 §2f-1 eliminated by construction (E4: `WriteSceneUniforms` returns `RB3SceneBinding` [S1],
`mActiveScene` assigned only at the 3 write sites [S1/S2], every draw consumes its binding via
`RB3DrawContext`/`SubmitDraw` [S3]).

**Build:** `native/build-agent-W1.6` (clang) `rb3-native` + `rb3-tests` green (rc=0).

**Gate evidence (per PLAN "Gate protocol") — CLEAN build:**
- **Direct baseline-bin vs S3-bin canonical drawlog A/B (PRIMARY byte-identical proof, S1/S2 method):**
  baseline built from an engine worktree at `01c2642` (S2) via `-DMILO_ENGINE_PATH=` into
  `native/build-baseline-W1.6-s3` (no shared-tree mutation; worktree removed after). `compare_canonical`
  baseline-capture-as-golden vs S3-capture: both **888 draws / frame 60**; **passed=True, 0 unexpected**,
  272 residual eye-jitter diffs tolerated (W0.3d CharEyes/CharLookAt, present in both). **No
  scene-bind-group collapse, no world-xfm divergence, no mesh-identity swap introduced by S3.**
- **Committed-golden canonical sweep** (`--fixed-clock --canonical-order`, 15-run determinism):
  counts=888 all 15 (spread=0), name-set drift 0. S3 build **72 unexpected** vs the stale golden;
  **S2-baseline build 72 unexpected** on the SAME golden — IDENTICAL FAIL-class distribution (E2). The
  72 are the pre-existing CharEyes/CharLookAt world-jitter residual; the ±3 residual-bucket count
  (231 vs 228) is W0.3c order-flake, not a W1.6 effect (the direct A/B is 0 unexpected).
- **Built-in canonical fail-red self-demo** (`--fail-red-audit --canonical-order`): 4 classes RED
  (count-drop / bind-group-collapse / world-out-of-bound / mesh-identity) + permutation GREEN — gate
  discriminates on the current golden.
- **rb3-tests DrawLogGolden.\*:** 9/10 pass incl. **CatchesBindGroupCollapse** / CatchesCoLocation /
  CatchesDroppedDraw / CatchesPipelineChange; `PopulatesFromRealDrawMesh` GTEST_SKIP → teardown
  SIGSEGV — pre-existing, IDENTICAL to S1/S2 baseline (not W1.6).
- **lineup-gate.py** (`--bin build-agent-W1.6/rb3-native`): **PASS** img/segA/ratioB/countC/pin
  (4 frames coop_g_n03 + coop_g_b, max_band_ratio 3.36, 0-5 slivers — no shard/ratio regression).
- **milo-engine-tests (DC3-context invariance net):** **198 pass / 0 fail / 2 skip** (200 total;
  ExtractBik.ExtractSmallest + SkinGolden.CaptureGolden skipped). SkinGolden.*/ClipPoseFixture.* green
  — no leak into shared draw/skin code.

**S3 fail-red (E3 — gate can still see a W1.6 regression):** scratch perturbation (NOT committed) in
DrawMesh — latch the FIRST `mActiveScene` seen this run into a function-`static RB3SceneBinding` and
build `ctx.scene` from that STALE binding for every draw, collapsing the 20 distinct per-write scene
bindings into one (the a0f98ad class). Rebuilt, ran the direct baseline-vs-perturbed canonical A/B:
**passed=False, 299987 unexpected**, every one class **`field=scene golden=distinct cand=shared`** — a
NON-residual bind-group-collapse divergence (NOT eye-jitter world residual), draw count still 888. This
is exactly the mis-thread the brief specifies and the gate flags it RED unambiguously. **Reverted** the
perturbation; post-revert direct A/B back to **passed=True, 0 unexpected** (rebuilt + re-ran). Clean
commit `6221a56` contains only the reverted (byte-identical) code.

**Remains:** S4 (final consolidated sweep + comment cleanup + STATUS append). **Blockers:** none.
