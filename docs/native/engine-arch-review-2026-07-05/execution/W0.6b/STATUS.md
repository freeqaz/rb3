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

## W0.6b.S3 — done
Staging: `execution/W0.6b/classified-S3.json` (44 rows). Commit `c081175b`.
All 44 flags re-scanned (fresh `native_compat_census.py scan --json`), confirmed present + still
absent from `classification.json` (site-verified read, not name-based). Verify: parses; count==44;
keys==S3 set exactly; classes all valid (`probe`); required fields (`class`/`owner`/`faithfulStatus`)
present on every row.

Result: **all 44 classify as `probe`, default-off** — every call site was read and confirmed to only
print/collect (`fprintf`/`MILO_LOG`) or, in one case, dump raw PCM to disk (`RB3_DUMP_STEMS`, a
side-channel file write with no effect on the mix path). None gate rendering, physics, load order, or
any other shipped behavior. No reclassification to `workaround` was needed.

One near-miss checked closely: `MENU_VOID_SKIP` (`Draw.cpp:69`) DOES change what gets drawn (an
opt-in comma-substring skip-draw list), which looks workaround-shaped. Kept as `probe` per PLAN
guidance — it is a name-substring A/B research tool for isolating which drawable paints a given
hub-backdrop region, not a shipped fix; the actual shipped fixes in the same function
(worldcenter-occluder skip, `bottom_square_refraction` skip) are unconditional and already covered by
S2's `RB3_MENU_VOID_FIX_OFF`/`RB3_REFRACTION_FIX_OFF` rows. Same category as S1's `RB3_NO_CLIP`-style
bisection-disable probes.

Five flags are dual-site with independent, unrelated print statements sharing one env-var name
(coincidental reuse, not shared state): `CAM_DBG` (TrackDir.cpp highway-cam pose dump + engine
Rnd_Wgpu_RB3.cpp pipeline-prewarm timing), `CHAR_DBG` (engine RB3MaterialBinder.cpp outfit-texture
report + BandDirector.cpp LoadCharacters re-run log), `RB3_PREWARM_DBG` (game UIScreen.cpp prewarm
schedule log + engine Rnd_Wgpu_RB3.cpp A5 pipeline-warm timing), `RB3_STATS_DBG` (File.cpp MetaPerformer
integrity bisect + Mesh.cpp VertVector stomp-ring recorder), `VENUE_DBG` (BandDirector.cpp, 5 sites,
all logging the same EnterVenue force-load bridge; shares its last two lines with `CAMDIR_DBG` and
`CHAR_DBG`, which are themselves aliases of the same VENUE_DBG log calls, not separate gates). One
sidecar row covers all sites/roots for each, as instructed.

`read` mode: 11 flags are `value` (bone/clip-type/system-name substrings or a raw path string:
`BAND_ANIM_BONE`, `BAND_ANIM_PROBE`, `CBM_DBG2`, `CHARDRV_PROBE`, `MENU_VOID_DBG2`, `MENU_VOID_SKIP`,
`MESH_BONE_DBG`, `PART_INIT_DBG`, `PART_MOVE_DBG`, `RB3_DUMP_STEMS`, `SERVO_PROBE`); 1 is `truthy`
(`RB3_READAHEAD_DEBUG`, matches the scanner's own guess); the remaining 32 are `presence`.

No src/ edits (read-only). `classification.json` NOT touched (S4 merges). No `gen` regen run.

### W0.6b.S3 flag to class to one-line reason (44 rows, all class=probe, default=off)

| Flag | Owner | Reason |
|---|---|---|
| BAND_ANIM_BONE | char/anim | names the bone BAND_ANIM_PROBE samples |
| BAND_ANIM_PROBE | char/anim | per-frame band-skeleton animation-chain trace |
| BONE_CLEAR_DBG | skinning | logs CopyBones(0) wiping a non-empty bone list |
| BONE_LOAD_DBG | skinning | loaded-vs-null-resolved bone counts pre-strip |
| CAMDIR_DBG | render/camera | logs HarvestDircuts re-run (aliases VENUE_DBG) |
| CAM_DBG | render/camera | highway game.cam pose dump + engine pipeline-warm timing (2 unrelated sites) |
| CBM_DBG | skinning | dumps decoded SCALE channel + row lengths per bone |
| CBM_DBG2 | skinning | dumps quat magnitude / post-rotate det per bone |
| CBS_DBG | skinning | dumps cached packed-buffer layout vs bytes consumed |
| CHARDRV_PROBE | char/anim | confirms CharDriver::Poll runs + clip/apply state |
| CHAR_DBG | render/char-material | outfit diffuse-tex report + LoadCharacters log (2 unrelated sites) |
| CLOCK_DBG | ui/hud | logs TrackDir::DrawShowing real-time/scroll state |
| CROWD_REBIND_PROBE | render/crowd | verbose companion to the default-ON crowd rebind |
| GAME_DBG | bandobj/flow | logs OnFileLoaded + venue-deferred gate inputs |
| GEM_DBG | render/highway | dumps GemTrackDir surface mesh/mat/tex names |
| HEAD_REBIND_PROBE | skinning | verbose companion to the default-ON head/hands rebind |
| IK_TGT_DBG | char/ik | reports far (over 50u) IK hand-target distances |
| INST_REBIND_PROBE | skinning | verbose companion to the instrument-strings rebind |
| K9_APPLY_DBG | ui/hud | dumps milo apply handler dispatch state |
| MENU_VOID_DBG2 | render/ui | render-inert hub-backdrop material dump |
| MENU_VOID_SKIP | render/ui | opt-in skip-draw A/B research list (not a shipped fix) |
| MESH_BONE_DBG | skinning | dumps bind-offset determinant per matched mesh |
| MILO_LOCALE_DBG | utl/locale | traces Localize() token/format resolution |
| MILO_SETTOKEN_DBG | ui/label | traces UILabel::SetTokenFmtImp resolution |
| PART_INIT_DBG | render/particles | dumps InitParticle emit branch + direction |
| PART_MOVE_DBG | render/particles | per-call MoveParticles force/velocity trace |
| RB3_DUMP_STEMS | audio/stems | opt-in raw PCM stem dump to disk (side-channel write) |
| RB3_HAIR_DBG | char/hair | reports per-strand collide-hookup counts |
| RB3_METAMUSIC_DBG | synth/metamusic | logs deferred stream-FX wiring completion |
| RB3_NOTIFY_ALL | os/debug | console-verbosity toggle (restores NOTIFY dedup) |
| RB3_PLACEMENT_PROBE | render/placement | dumps crowd instance spXfm for the placement oracle |
| RB3_PP_PROBE | bandobj/patchmesh | dumps BandPatchMesh WorkVerts entry state |
| RB3_PREWARM_DBG | load/perf | prewarm schedule log + pipeline-warm timing (2 unrelated sites) |
| RB3_READAHEAD_DEBUG | load/perf | reports loader queueDepth/kicked counts |
| RB3_SKINFIX_DBG | skinning | logs outfit skin-diffuse MatSwap rebinds |
| RB3_STATS_DBG | debug/stomp-watch | MetaPerformer integrity bisect + VertVector stomp ring (2 unrelated sites) |
| RELOAD_PROBE | bandobj/flow | logs every mid-song band-reload call site |
| SERVO_PROBE | char/anim | confirms CharServoBone::Poll + bone LocalXfm change |
| SKEL_REBIND_PROBE | skinning | verbose companion to the outfit skeleton rebind |
| SKEL_REBIND_SKINPOS | skinning | worst bone-relative skin-position delta metric |
| STRIDE_PROBE | skinning | dumps compressed-vertex blob read parameters |
| UISCREEN_DBG | ui/hub | logs UI transition-state + panel exiting/blocked state |
| VENUE_DBG | bandobj/venue | logs the native EnterVenue force-load bridge (5 sites) |
| VOIDCUT_DBG | render/camera | logs the sticky last-good-cam voidcut fallback verdict |
