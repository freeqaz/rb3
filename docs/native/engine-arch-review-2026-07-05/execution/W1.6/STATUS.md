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
