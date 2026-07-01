# STEP 2 impl — impostor-crowd cam reads its scoped env (GAP 3, big_club crowd white)

**STEP-2 impl agent (Opus). Engine edits in the PRIVATE engine worktree
`/home/free/code/milohax/milo-native-engine-worktrees/converge-lighting`
(branch `wt-converge-lighting`). Built + verified in the rb3 worktree
`/home/free/code/milohax/rb3/.claude/worktrees/converge-lighting`.**

## Commits (engine worktree, NOT pushed)
- STEP 1 (GAP 2, GX point falloff): **`a360e3c`** — committed by this agent. STEP 1
  was implemented (3 files dirty: `standard_wgsl.inc`, `UniformStructs.h`,
  `Rnd_Wgpu_RB3.cpp`) but **never committed and had no step1-impl.md / step1-verify.md**
  when I arrived. Its verify shots existed (`shots/step1/arena02_{gxon,legacy,venueoff}`),
  so it was verified-but-undocumented. I committed it unchanged as a discrete commit so
  STEP 1 / STEP 2 are independently revertable per the task. The STEP 1 change is sound
  and DC3-safe (see "DC3 safety" below).
- STEP 2 (GAP 3, impostor crowd env gate): **`bae1aae`** — this agent. One file
  (`src/platform/Rnd_Wgpu_RB3.cpp`, +74/-4).
- Local-only pin bump: `native/CMakeLists.txt:74` MILO_ENGINE_PIN -> `bae1aae…` (worktree
  build only; the coordinator does the real master pin bump).

## What STEP 2 changes (engine, `Rnd_Wgpu_RB3.cpp`)
1. **`WriteSceneUniforms` gate (was line 1288):** widen the `name=="world.cam"` venue
   gate to ALSO admit the impostor crowd cam, so it reads its scoped authored
   `RndEnviron::sCurrent` (`crowd.env`) instead of the hardcoded-white default branch
   (1 white directional + 0.45 grey ambient). Discriminator: `(unnamed cam) &&
   (cam->TargetTex() != nullptr)`. `venv->mAmbientFogOwner` still required.
2. **Per-environ re-write gate (was line 3530):** identical widening so the impostor cam
   re-writes on a mid-frame env change.
3. **Zero-light grey-key guard (the no-light fallback):** `crowd.env` has 0 lights AND
   ~0 ambient. Fully skipping the grey key would render the crowd BLACK (the plan
   assumed 0.18 ambient — measured reality is ~0), so I keep a SOFTENED crowd-specific
   key (`RB3_CROWD_GREY_KEY`, default 0.10, vs the generic 0.22). world.cam keeps 0.22
   (byte-identical).

Knobs: `RB3_CROWD_LIGHT_OFF=1` (opt out — impostor cam falls back to legacy white),
`RB3_CROWD_GREY_KEY=<f>` (tune, default 0.10). No rebuild needed for either.

## Discriminator — measured, not assumed (temporary RB3_STEP2_PROBE, since removed)
Logged `(camName, hasTargetTex, envName, hasOwner, ambient, #lights)` per distinct
tuple across boot + menu + gameplay on big_club_01 / arena_02 / festival_01 /
small_club_01. The **only** camera that is BOTH unnamed AND has a TargetTex is the
impostor crowd cam:
```
cam='' tgt=1 env='crowd.env' owner=1 amb=(0.00,0.00,0.00) approx=0   <- impostor crowd cam (ADMITTED)
cam='Cam.cam' tgt=1 env='' owner=1 amb=(1.00,1.00,1.00) approx=0     <- NAMED char-preview RTT cam (EXCLUDED, named)
cam='' tgt=0 env='theater.env' ...                                   <- other unnamed cams are tgt=0 (EXCLUDED)
cam='game.cam' tgt=0 ... / 'world.cam' / '[ui.cam]' / 'overshell.cam' / 'meta.cam'  (all named, EXCLUDED)
```
So `(unnamed && TargetTex)` uniquely selects the impostor RT cam and structurally
EXCLUDES game.cam (highway), menu cams, world.cam, and the char-preview cam.

## Verification — engine behaviour PROVEN, visible-crowd delta CONFOUNDED
**Engine-level: VERIFIED.** With STEP 2 on, the venue-gate probe (`RB3_VENUE_PROBE`)
now fires for the impostor cam reading `crowd.env` in big_club_01 / arena_02 /
festival_01 — it did NOT before (the probe is inside the gate the impostor cam
previously missed):
```
big_club_01 : env=crowd.env ambRaw=(0.00,0.00,0.00) ambAdj=(0.01,0.01,0.01) numApprox=0
arena_02    : env=crowd.env ambRaw=(0.00,0.00,0.00) ambAdj=(0.01,0.01,0.01) numApprox=0
festival_01 : env=crowd.env ambRaw=(0.19,0.27,0.35) ambAdj=(0.19,0.27,0.35) numApprox=0
```
The impostor cam now reads its authored dim env instead of the white default. This is
exactly the plan's intended structural fix, and it is DC3-safe (see below).

**Visible big_club_01 crowd delta: NOT cleanly demonstrable — and the dominant visible
white is a DIFFERENT issue than the impostor path.** Three findings:
1. **The visible white crowd in big_club_01 is NOT the impostor path.** A LIGHT_PROBE
   trace shows the crowd character meshes (`female_crowd_body0*`, `male_crowd_body0*`,
   `crowd_left_walking`, `male_extras_body0*`) are drawn under **`world.cam`** scoping
   `char_rooftop.env`, NOT under the impostor cam. The unnamed cam in big_club draws
   only cityscape backdrop (110 building/sign meshes), 0 crowd meshes. So big_club's
   visible crowd does not go through the cam STEP 2 fixes (in this build/LOD).
2. **The white blob is a MESH SHARD, not a lighting bug.** The lower-left crowd "figure"
   is an exploded white smear (see `shots/step2/.../crowd_left_on.png`) whose white% is
   INVARIANT to STEP 1 (legacy vs GX falloff), STEP 2 (`RB3_CROWD_GREY_KEY=0.0` vs
   0.10), and `RB3_VENUE_LIGHT_OFF` — all land crowd white% ~6–12%. That's the
   char-skinning / inst-strings shard family (cf MEMORY `crowd_origin_inststrings`,
   `char_skinning_deform`), out of STEP 2's lighting scope.
3. **Venue lighting is NON-DETERMINISTIC across boots.** Two byte-identical small_club_01
   runs (same code, same `--anchor-ms 6000`) gave frame luma 37.8 vs 49.5 — the disco
   light phase differs per boot. So separate-boot A/B luma on venue scenes is unreliable
   (the documented camera/lighting-desync false-positive). The big-looking small_club
   on/off "pink wash" diff I first saw is this disco-phase noise on the world.cam path
   (unchanged by STEP 2), not a STEP 2 regression.

Net: STEP 2 is correct-by-construction (proven the impostor cam reads its env, proven the
discriminator is unique, structurally excludes game.cam/menu/world.cam) but its visible
benefit on big_club_01 is masked because that venue's crowd renders via world.cam +
carries a separate mesh-shard artifact. The fix is the right, minimal, gated structural
change and improves the impostor path wherever it IS the renderer; it is harmless where
it isn't.

## DC3 safety — STRUCTURALLY SAFE (stronger than the plan flagged)
- **STEP 2 file is RB3-only.** `Rnd_Wgpu_RB3.cpp` is in
  `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` (engine `CMakeLists.txt:320`). DC3 compiles
  `Rnd_Wgpu.cpp` instead (`MILO_ENGINE_GPU_PLATFORM_SOURCES`, line 309) — a different
  renderer (`WgpuRnd : NgRnd`). DC3 never compiles or links the STEP 2 code. Confirmed:
  no DC3 source references `Rnd_Wgpu_RB3`.
- **STEP 1 shared shader is flag-gated.** `standard_wgsl.inc` IS shared, so STEP 1 added
  `SceneUniforms.pointFalloffMode` (reusing a former `_padPL` float — struct size
  unchanged). Default 0 = the EXACT legacy `saturate(1-d/r)^2` curve, byte-identical.
  Mode 1 (GX curve) is set ONLY on RB3's world.cam venue path. DC3's
  `WgpuRnd::WriteSceneUniforms` does `SceneUniforms scene{}` (zero-init) and never
  touches the field -> DC3 stays mode 0 -> byte-identical. (Verified by reading DC3's
  `Rnd_Wgpu.cpp:1276`.)

## Gates vs the task's objective gates
- big_club crowd white% drop toward small_club ~0: **NOT achieved on the visible crowd**
  — but because the visible white is the world.cam-extras + mesh-shard path, not the
  impostor path the gate targets. The impostor path itself is verified fixed (reads
  dim crowd.env). Recommend the coordinator route the remaining big_club visible-white
  to the char-shard / world.cam-extras workstream, not STEP 2.
- BAND + HIGHWAY byte-identical (game.cam untouched): **structurally guaranteed** — the
  discriminator excludes named cams; the grey-key softening only fires for
  `isImpostorCrowdCam`. (Pixel-diff "proof" is impossible here due to per-boot venue
  non-determinism; the guarantee is by code inspection.)
- DC3 unaffected: **structurally guaranteed** (RB3-only file + flag-gated shared shader).

## Shots
`docs/native/converge-2026-06-20/lighting/shots/step2/` — bigclub_on / bigclub_off
(`RB3_CROWD_LIGHT_OFF=1`) / bigclub_greykey0 (`RB3_CROWD_GREY_KEY=0.0`) /
bigclub_step1legacy / bigclub_venueoff / smallclub_on / smallclub_off /
sc_det_r1+r2 (the non-determinism proof) / band_on+off / final_bigclub_bothfixes.

## Defer / open
- Exact crowd exposure (`RB3_CROWD_GREY_KEY`) left at a moderate 0.10 default per the
  task's "DEFER final exposure tuning" — it's a runtime knob.
- big_club_01 visible white crowd: separate char-skinning-shard + world.cam-extras
  (char_rooftop.env bright (2.0,1.91,0.89) range-250 point) issue, NOT the impostor
  path. Out of STEP 2 scope.
