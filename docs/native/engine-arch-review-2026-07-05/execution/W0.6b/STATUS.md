# W0.6b — STATUS

Item: classify the 91 game-root `Unknown` NativeCompat flags into
`probe|workaround|feature|perf`. Lane B (parallel to Lane A). Engine pin `609efb7`.

Planner (Opus) seeded this file + `PLAN.md` + `census-snapshot.json`. Implementers append one
`## <subtask-id> — done|partial|blocked` section per subtask under `flock /tmp/rb3-docs.lock`,
with the staging-file path / commit SHA. S4 appends the final 91-row flag→class→reason table +
verify log.

Fixed facts (planner-verified 2026-07-06):
- 91 game-root flags, all `FlagClass::Unknown`, none in `classification.json` (join-verified).
- Partition: S1 = 23 char/skinning behavior; S2 = 24 ui/render/audio/loader/synth/world behavior +
  perf/value + `MILO_HEADLESS`; S3 = 44 probes. 23+24+44 = 91, zero overlap (see PLAN.md).
- Scanner already covers `rb3/src/system` (`a537c2a3`); `check` already exits 0 at 318 flags — the
  gap is classification, not scanning.
- D2: NO `gen` regen by this lane; coordinator regens `gen.inc`/ledger once at wave end.

## (pending implementers)

## W0.6b.S1 — done
Staging: `execution/W0.6b/classified-S1.json` (23 rows). Commit `92a53bcb`.
All 23 flags re-scanned (fresh `native_compat_census.py scan`), confirmed present + `Unknown`,
classified by reading each call site (READ-ONLY on src/). Verify: parses; count==23; keys==S1 set;
classes all valid. Histogram: 15 workaround / 6 probe / 1 perf / 1 feature.

DEVIATION (site-verified, per PLAN "verify at call site, do NOT classify from the name"): the
name-guidance lumped the whole `RB3_NO_*` family as opt-out workarounds-default-ON, but the call
sites split into TWO kinds:
- **probe** (default-OFF debug/bisection *disable* of a faithful subsystem that runs by default,
  `g=getenv?1:0; if(g)return`): `RB3_NO_CLIP` (CharDriver::Poll), `RB3_NO_DEFORM`
  (SetDeformation), `RB3_NO_FACE` (CharFaceServo+CharHair::Poll), `RB3_NO_IK` (all 10 IK Poll
  sites), `RB3_NO_POSEMESHES` (CharBonesMeshes::PoseMeshes). Absent = faithful; setting = study.
- **workaround** (opt-out of a native hack that IS default-ON): `RB3_NO_SKEL_REBIND`,
  `RB3_NO_CROWD_REBIND`, `RB3_NO_HEAD_REBIND`, `RB3_NO_INST_REBIND`.
- **workaround / faithfulStatus:live** (native now does the FAITHFUL load by default; flag is an
  escape-hatch opt-out that re-enables a legacy hack): `RB3_NO_DEFORM_LOAD` (deforms load),
  `RB3_NO_HEAD_SHAPER` (head shapes load, CharLoad5b byte-correct).

Other calls: `RB3_SKIN_RTT`=feature (gates broken engine RTT composite; default-OFF direct-bind
bypass ships, C8 memory); `RB3_SKIN_NOCACHE`=perf (cache disable, measurement); `RB3_SKIN_TIMING`=
probe (timing print); `RB3_SKEL_REBIND_FULL`/`_CALCOFF`/`SET_SKEL_REBIND`=workaround default-OFF
study/variant knobs (each selects a rebind behavior when set); `RB3_MESH_FREE`=workaround default-ON
(native keeps mesh CPU data); `RB3_WALKON_SNAP_OFF`=workaround default-ON; `RB3_BOUND_REBAKE`/
`RB3_HANDS_BIND_FIX`=workaround default-OFF experimental; `RB3_INST_STRINGS_MODE`=workaround
read=value (rigid/rebake mode); `RB3_SKIN_FIX_OFF`=workaround default-ON (coupled to RB3_SKIN_RTT).

No src/ edits. classification.json NOT touched (S4 merges). No gen regen.

## W0.6b.S2 — done
Staging: `execution/W0.6b/classified-S2.json` (24 rows). Commit `969b78f8`.
All 24 flags re-scanned (fresh `native_compat_census.py scan`), confirmed present + `Unknown`,
classified by reading each call site (READ-ONLY on src/). Verify: parses; count==24; keys==S2 set;
classes all valid; required fields present. Histogram: 14 workaround / 8 perf / 2 feature (0 probe).

Per-flag (class — call site — reason):
- `MILO_HEADLESS` = **feature** default-off (UI.cpp:519 + engine AudioDevice.cpp:175/Rnd_Wgpu.cpp:253
  + main_native): headless runtime mode; skips window/audio/GPU device, fakes 1/30s UI clock. Shared
  root (engine/game/glue) — one row covers all.
- `RB3_APPLY_HANDLER_FIX_OFF` = **workaround** default-ON (TrackPanelDir.cpp:294): single-player
  scoreboard right/left.grp x-translation neutralization; `if(!getenv)` → opt-out.
- `RB3_BILLBOARD_OFF` = **workaround** default-ON (MultiMesh.cpp:30, truthy): native DrawShowing
  billboard branch active by default; flag disables it.
- `RB3_CAM_FALLBACK_OFF` = **workaround** default-ON (BandDirector.cpp:386): voidcut last-good-cam
  fallback; `!=0` opt-out.
- `RB3_LOADER_BUDGET_MS` / `_MIN_YIELD_MS` / `_READAHEAD` / `_YIELD_MS` = **perf** default-off (value):
  Loader/StandardStream numeric budget/yield/read-ahead knobs (defaults 8ms/16ms/6/16ms).
- `RB3_MENU_VOID_FIX_OFF` = **workaround** default-ON (Draw.cpp:70): menu-void mesh cull fix; opt-out.
- `RB3_METAMUSIC_SYNC` = **workaround** default-OFF opt-in (MetaMusic.cpp:381, truthy): restores the
  original eager blocking PostLoad path (native default = async).
- `RB3_NO_CROWD_INTRO` = **workaround** default-ON (CrowdAudio.cpp:39): native crowd/venue_intro mogg
  bridge; `if(getenv)return nullptr` opt-out.
- `RB3_PREWARM_NEXT` = **perf** default-off (UIScreen.cpp:275, value): prewarm from:to screen-pair
  spec string; asset-prewarm scheduling only.
- `RB3_PREWARM_SCREENS` = **perf** default-off native / on web (UIScreen/UIPanel, truthy): UI screen
  asset prewarm+adopt; adopt site has no correctness dependency.
- `RB3_REFRACTION_FIX_OFF` = **workaround** default-ON (Draw.cpp:101): song_select
  bottom_square_refraction cull fix; opt-out.
- `RB3_RESYNC_YIELD_OFF` = **workaround** default-ON (StandardStream.cpp:568, truthy): stream resync
  yield; opt-out disables.
- `RB3_REVIEW_LIGHTER_FIX_OFF` = **workaround** default-ON (ReviewDisplay.cpp:84): lighter-slot show
  fix (hide on zero score); opt-out.
- `RB3_SCROLLBAR_FIX_OFF` = **workaround** default-ON (ScrollbarDisplay.cpp:210): content-aware draw
  gate; =1 restores exact Wii over-draw gate.
- `RB3_STREAM_BUF_SECS` = **perf** default-off (StandardStream.cpp:165, value): min buffer depth in
  seconds (default 4, capped by ~9.1s ring). Anti-underrun value knob.
- `RB3_STREAM_PREPLAY_CAP_OFF` = **workaround** default-ON (StreamReceiver.cpp:79, truthy): pre-play
  ring-fill write cap; opt-out.
- `RB3_TV3_PLAY_OFF` = **workaround** default-ON (Dir.cpp:151): vignette_transition (tv3) WorldDir
  force-play; `if(!sOff)b=true` opt-out.
- `RB3_VENUE_FRUSTUM_CULL` = **perf** default-OFF opt-in (Draw.cpp:201, truthy): world.cam venue
  sphere frustum cull; pure draw-skip.
- `RB3_VENUE_SYNC` = **workaround** default-ON (BandDirector.cpp:36, truthy '0'): native forces
  synchronous venue load (correct ordering); =0 opts into experimental async (unsafe).
- `RB3_WEB_OFFMAIN_MIX` = **feature** default-ON (engine AudioDevice_Web.cpp:673 + StreamReceiver.cpp:46,
  truthy '0'): off-main-thread audio decode/mix mode (web survives freezes; native sizes ring deeper).
  Port audio-architecture toggle, not a fidelity stand-in. Shared root (engine/game).
- `VENUE_CAM_LOCK` = **workaround** default-ON (BandDirector.cpp:351): native bridge points venue
  WorldDir mCam at director's active shot cam each frame; =1 reverts to static cam. Gates real
  behavior (not a probe despite the plain name).

DEVIATIONS from PLAN name-guidance (all site-verified):
- `RB3_PREWARM_NEXT` classified **perf** (value spec string), not workaround — PLAN allowed
  "workaround if correctness dependency"; the adopt/prewarm sites only schedule assets, no draw/logic
  change, so perf per definition.
- `RB3_WEB_OFFMAIN_MIX` classified **feature** (not workaround): off-main-thread audio mixing is an
  intended port architecture toggle, not a stand-in for a Wii-faithful path (default-ON, improves
  robustness). PLAN said "feature or workaround" — chose feature.
- `VENUE_CAM_LOCK` classified **workaround** (not probe): despite the non-`_OFF` name it gates a real
  camera-bridge behavior default-ON; opt-out=1. PLAN flagged this ambiguity ("debug camera lock print
  = probe") — the site is behavior, not a print.

No src/ edits. classification.json NOT touched (S4 merges). No gen regen.
