# Lane V handoff — rb3-viewer standalone .milo asset renderer

Date: 2026-07-02. Implements PLAN.md "Lane V" per scout-rb3-infra.md §6 Option A.

## What shipped

A headless `--viewer` / `RB3_VIEWER=1` mode of `rb3-native` that loads one `.milo`
against the real engine + BandRnd (WGPU) and writes a PNG — the DC3-milo-viewer
analog for RB3, for debugging asset-render bugs without a full game boot.

Files (all mine; committed together):
- `native/src/rb3_viewer.cpp` — the mode: `RunViewer()`, CLI parse, char/hair
  factory set, `--subdir` dep loading, `--sim` CharHair settling, `--hide` /
  `--only-showing` draw filter, camera CLI, `--list` census, readback+PNG+`_exit`.
- `native/src/main_native.cpp` — dispatch branch (argv `--viewer` OR
  `RB3_VIEWER=1`), next to the `RB3_RENDER_MESH` branch.
- `native/CMakeLists.txt` — `rb3_viewer.cpp` added to the `rb3-native` source list.
- `native/src/rb3_render_mesh.{cpp,h}` — **extended (not forked)** with two
  exported helpers `ViewerComputeBounds()` + `ViewerMakeCamera()` and a factored
  `BuildFramingCam()` the existing `SynthesizeCamera` now also calls.
  RB3_RENDER_MESH behavior is byte-for-byte unchanged (verified — see below).
- `scripts/native/render-asset.py` — thin wrapper (build-if-stale, run headless,
  print PNG path + census).
- `docs/native/asset-viewer-2026-07-02/VIEWER.md` — usage doc.

## Init spine (exactly per scout §6)

InitGpu(W,H,headless) BEFORE chdir → chdir(RB3_DATA) → `kPlatformXBox` →
SetSystemArgs → SystemPreInit(`band_preinit_keep.dta`) →
SystemInit(`band_keep.dta`) → `gBandRnd.PreInitRender()` (rndobj factories +
Tex/Text/Dir aliases; InjectTypeDefStubs runs inside LoadMiloAndWalk / the load
path) → `RB3RegisterGameObjectFactories()` → char factory set. `--list` skips
only InitGpu (PreInitRender is pure factory registration, still required).

Char factory set: the `test_charload5b.cpp` `RegisterCharLoadFactories` list
(RndMeshAnim/MeshDeform/TexBlender/TexBlendController/MatAnim/TransAnim/PropAnim/
EventTrigger + CharClipSet/CharClip/CharCollide/CharLipSync/CharInterest/
CharFaceServo/CharWeightSetter/CharServoBone/CharHair/BandFaceDeform) plus
`RndAmbientOcclusion::Init()` and `OutfitConfig::Init()`. `Character` is NOT
registered; `CharInit()`/`BandInit()` are NOT called wholesale (overlay traps).

## Bug found + fixed during bring-up (not in the scout)

`OutfitConfig::Init()` (bandobj/OutfitConfig.cpp) does
`sBandCharDesc = Hmx::Object::New<BandCharDesc>()`. If `BandCharDesc`'s factory
isn't registered, that hits "Unknown class BandCharDesc" → MILO_FAIL → null
static → later `double free / SIGABRT`. Fix: call `BandCharDesc::Register()`
(lightweight factory only) BEFORE `OutfitConfig::Init()`. Do NOT call
`BandCharDesc::Init()` — its full init also `ReloadPrefabs()` + loads
`deform.milo`, unneeded for a static asset render.

Second bring-up bug: an early `--list` version skipped `PreInitRender` to avoid
GPU work and registered only a handful of rndobj factories → Unknown-class stream
desync → heap corruption abort (trap #4). Fixed by always calling
`PreInitRender` (it does no GPU work) and skipping only InitGpu for `--list`.

## Acceptance — ALL PASS (evidence in /tmp/rb3-viewer-accept/)

1. `char/main/hair/female/gen/female_hair_long_resource.milo_xbox` →
   `female_hair_long.png`. **ZERO `Can't make` NOTIFYs.** non-clear pixels
   68187/307200 (22.2%) — **pixel-identical to the scout baseline**
   `probe-female-hair-long-render-mesh.png` (`cmp` clean visually; same %). The
   added char factories did NOT regress the static draw.
   (Cross-milo `NOTIFY: ... couldn't find hair_shared_spec.tex / bone_hair.mesh`
   are the expected non-fatal fallbacks, §3.5 — not `Can't make`.)
2. `char/main/hair/male/gen/male_hair_crazyhawk_resource.milo_xbox` renders
   static (`crazyhawk_static.png`, 0 `Can't make`, coherent spiky mohawk geo)
   AND with `--sim 30` (`crazyhawk_sim30_prefix.png`). The sim RAN:
   `--sim 30 steps over 1 CharHair object(s)`, 30 steps × 1 hair polled, no crash.
3. rb3-native builds green. `RB3_RENDER_MESH=1` still works: smoke render of the
   same female hair → 68187/307200 (22.2%), identical framing/output as before,
   and it still shows its ORIGINAL 3 `Can't make` (AmbientOcclusion/OutfitConfig/
   CharHair) because RENDER_MESH deliberately doesn't register char factories —
   confirming that path is unchanged.

## Important note on --sim visual result (for Lane G / Fable review)

`crazyhawk_sim30_prefix.png` is **byte-identical to `crazyhawk_static.png`.** The
sim executes (proven by the poll log) but the render does not reflect it, for two
reasons:
1. **CharHair.cpp is ALREADY MODIFIED ON DISK by Lane H** during my run —
   `git diff --stat src/system/char/CharHair.cpp` = `1 file changed, 19
   insertions(+), 19 deletions(-)` (the SimulateInternal brace/CFG fix). So my
   capture reflects the **Lane-H-FIXED** CharHair, NOT the pre-fix broken source.
   I did not touch or revert it.
2. **More fundamentally, the viewer draws the strand mesh un-skinned.** `--sim`
   moves `bone_hair_*` transforms via `CharHair::Poll()`, but the drawn
   `*_resource.mesh` is a *skinned* mesh rendered at rest via `DrawMesh` — the
   viewer runs no skinning/`PoseMeshes` pass, and the standalone resource milo's
   shared `bone_hair.mesh` is absent (the `couldn't find bone_hair.mesh`
   NOTIFYs). So bone motion isn't visible in the frame regardless of the CFG fix.

Net: the "collapse proves the sim runs" acceptance is met via the console poll
trace, not a visual delta. Visually reflecting hair sim needs the skeleton +
skinning path (a follow-up beyond Lane V's loader scope). Lane G's in-game
closeup gate (`band-closeup-capture.py`) remains the authoritative visual check
for Lane H's fix.

## Commit

`tools(native): rb3-viewer standalone .milo asset renderer` — SHA recorded in
StructuredOutput. Staged ONLY my files (rb3_viewer.cpp, main_native.cpp,
CMakeLists.txt, rb3_render_mesh.{cpp,h}, render-asset.py, VIEWER.md,
laneV-viewer.md). Did NOT stage `src/system/char/CharHair.cpp` (Lane H's).
