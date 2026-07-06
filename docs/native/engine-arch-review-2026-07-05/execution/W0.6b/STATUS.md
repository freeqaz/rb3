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

## W0.6b.S4 — done
Merge: engine commit `1fd2bfc` (`git -C milo-native-engine log --oneline -1`), engine pin advanced
past `609efb7` (Lane A already appended `RB3_DRAWSORT_DETERMINISTIC_OFF` at `e26d8c2`; this lane
appended its own block strictly after that, append-only, one flock'd write).

**Merge (step 1).** Loaded `classified-S1.json` (23 rows + `_note`), `classified-S2.json` (24 rows,
no `_note`), `classified-S3.json` (45 rows incl. `_note` = 44 flags). Stripped `_note`; union =
91 keys, 0 duplicates, 0 overlap with the then-current `classification.json` (92 keys: 89
pre-Wave-5 + `RB3_PLACEMENT_CONTRACT`/`RB3_FIXED_CLOCK`/`RB3_FIXED_CLOCK_DT_MS`/`RB3_DRAWLOG`/
`RB3_DRAWLOG_DUMP`/`RB3_DRAWORDER_TRACE`/`RB3_DRAWSORT_DETERMINISTIC_OFF` already appended by
Lane A / W3.1a before this lane ran). Inserted all 91 rows **after** the existing last row
(`RB3_DRAWSORT_DETERMINISTIC_OFF`) under one `_w06b_section` marker key (JSON has no `//` comment
syntax, so the PLAN's comment-separator instruction was realized as a blank-line-delimited
`"_w06b_section": "..."` marker key instead — same existing-file convention used by `_comment`/
`_schema` at the top of the file; **deviation** noted here per instructions). Applied under
`flock /tmp/milo-engine-classjson.lock` via a single atomic read-modify-write (added a trailing
comma to the prior last row, appended the 91 rows + section marker, re-closed the object). `git diff`
confirms exactly one existing line touched (the added comma) plus pure additions — no existing row
reordered or edited.

**Validate (step 2).** `python3 -c "import json;json.load(open('src/platform/NativeCompatFlags.classification.json'))"` — parses clean. Post-merge key count: 184 (92 prior + 91 new + 1
`_w06b_section` marker).

**Temp-gen verify (step 3).** `python3 scripts/analysis/native_compat_census.py gen --gen-inc-out
/tmp/w06b-gen.inc --ledger-out /tmp/w06b-ledger.md` (never touches committed paths) →
`319 rows (138 unclassified) -> /tmp/w06b-gen.inc`. Cross-checked against a fresh
`native_compat_census.py scan --json /tmp/w06b-fresh-scan.json` (`319 distinct flags, 435 call
sites`): fresh scan's game-root set (`'game' in roots`) = **exactly 91 flags**, and it is byte-set-
identical to the S1∪S2∪S3 classified set (0 symmetric difference). Intersection of that 91-flag
game-root set with `/tmp/w06b-gen.inc`'s `FlagClass::Unknown` rows (138 total, all engine/glue-only)
= **∅ (0)** — zero game-root `Unknown` rows remain, the hard exit criterion. The 138 remaining
`Unknown` rows are engine/glue-scoped and out of W0.6b's scope (not classified here).

**Selftest (step 4).** `python3 scripts/analysis/native_compat_census.py --selftest` →
**14/14 PASS** (tool logic un-regressed by the sidecar edit, as expected — selftest is hermetic).

**Check (step 5, expected non-zero pre-coordinator-regen).**
`python3 scripts/analysis/native_compat_census.py check` → **exit 1** (non-zero, expected per D2):
```
check: FAIL — 1 getenv flag(s) not in registry (.../NativeCompatFlags.gen.inc):
  - RB3_DRAWSORT_DETERMINISTIC_OFF
check: FAIL — .../NativeCompatFlags.gen.inc is stale (regen would differ). Run `gen`.
check: FAIL — .../NATIVE_COMPAT_LEDGER.md is stale (regen would differ). Run `gen`.
```
Reason: `check` diffs the fresh scan against the **committed** `NativeCompatFlags.gen.inc`, which
this lane deliberately does not regenerate (D2 — three Wave-5 lanes append to `classification.json`;
only the coordinator runs the single reconciling `gen` at wave end). The one reported missing flag
(`RB3_DRAWSORT_DETERMINISTIC_OFF`) is Lane A's W0.3d-fix addition, already in the sidecar but not yet
in the committed `.gen.inc` — orthogonal to this lane's 91 rows. This non-zero exit is the documented
expected state, not a lane failure; left for the coordinator's wave-end regen.

**Commit (step 6).** Under `flock /tmp/milo-engine-git.lock`: staged only
`src/platform/NativeCompatFlags.classification.json` (confirmed via `git status --porcelain` before
add — sibling `FxSendNative.cpp` had unrelated unstaged WIP, left untouched) and committed
`W0.6b: classify 91 game-root NativeCompat flags (probe/workaround/feature/perf)` → engine commit
`1fd2bfc`. `git show --stat` / `git diff HEAD~1 HEAD --stat` confirm only the sidecar file changed
(97 insertions, 1 deletion i.e. the one comma).

**Exit criteria checklist:**
1. Zero game-root `Unknown` rows in temp gen — **met** (0/91 intersect).
2. `--selftest` 14/14 — **met**.
3. `classification.json` parses, 92+91 rows (+1 section marker), append-only — **met**.
4. This STATUS.md section + 91-row table below — **met**.
5. No committed `gen.inc`/ledger regen by this lane — **met** (only `/tmp/w06b-gen.inc` +
   `/tmp/w06b-ledger.md` written).

No scope creep: `src/band3` untouched/unclassified (accepted per PLAN scope boundary); no `src/`
edits; `Rnd_Wgpu_RB3.cpp` / `src/App.cpp` / `FxSendNative.cpp` untouched.

### W0.6b — 91-row flag → class → owner → reason table (S1 char/skinning, S2 ui/render/audio/loader/synth/world, S3 probes)

| Flag | Class | Owner | Reason (one line) |
|---|---|---|---|
| RB3_BOUND_REBAKE | workaround | skinning | not-live: opt-in bound-mesh bind-side rebake, default-OFF (those meshes stay on the engine clamp/V24 guard; C8 pose-pipeline root-cause is the faithful fix) ... |
| RB3_HANDS_BIND_FIX | workaround | skinning | not-live: experimental hands/fingers rest-basis bind fix, default-OFF (W2.2 measured no benefit, NOT flipped; see char-skinning-deform memory) [BandCharacter... |
| RB3_INST_STRINGS_MODE | workaround | skinning | not-live: selects instrument-strings native rebind mode (rigid=default / rebake=A/B); the inst-strings rebind itself is a native stand-in [BandCharacter.cpp:... |
| RB3_MESH_FREE | workaround | render/mesh | not-live: native keeps the mesh CPU copy by default (prevents rigid eyes/teeth vanishing); opt-out RB3_MESH_FREE=1 restores the faithful free for A/B [Mesh.c... |
| RB3_NO_CLIP | probe | char | n/a: debug bisection disable of CharDriver::Poll (clip driving); default-OFF, Poll runs normally when absent [CharDriver.cpp:341] |
| RB3_NO_CROWD_REBIND | workaround | render/crowd | not-live: crowd char-bone rebind-to-own-skeleton default-ON (W2.3 retained/load-bearing, ~24x shard-drop when disabled) [Crowd.cpp:932] |
| RB3_NO_DEFORM | probe | skinning | n/a: debug bisection disable of BandCharacter::SetDeformation; default-OFF, runs when absent [BandCharacter.cpp:2335] |
| RB3_NO_DEFORM_LOAD | workaround | char/deform | live: native loads gender deforms by default (blocker fixed; converges to original); opt-out RB3_NO_DEFORM_LOAD reverts to the deferred-load hack [BandCharDe... |
| RB3_NO_FACE | probe | char | n/a: debug bisection disable of CharFaceServo::Poll + CharHair::Poll; default-OFF, both run when absent [CharFaceServo.cpp:60, CharHair.cpp:465] |
| RB3_NO_HEAD_REBIND | workaround | skinning | not-live: head/hands rest-capture rebind (NativeCaptureRestPoseAfterDeform + RebindHeadHandsAtRest) default-ON (W2.2 net win; rebake-OFF head guard-DROPs 9.5... |
| RB3_NO_HEAD_SHAPER | workaround | char/headshaper | live: head shapes load by default on native (byte-correct LE serialization, CharLoad5b gtest); opt-out RB3_NO_HEAD_SHAPER=1 reverts to disabled [BandHeadShap... |
| RB3_NO_IK | probe | char | n/a: debug bisection disable of all Char IK Poll (hand/foot/head/fore-twist/neck/upper-twist/lookat/fingers/midi/slider); default-OFF, IK runs when absent [C... |
| RB3_NO_INST_REBIND | workaround | skinning | not-live: instrument-strings rest-basis rebind (RebindInstStringsToRestBasis) default-ON [BandCharacter.cpp:1540] |
| RB3_NO_POSEMESHES | probe | skinning | n/a: Q1 decisive-test disable of CharBonesMeshes::PoseMeshes (channel->LocalXfm writeback); default-OFF [CharBonesMeshes.cpp:103] |
| RB3_NO_SKEL_REBIND | workaround | skinning | not-live: outfit-bone rebind-to-own-skeleton (RebindOutfitBonesToOwnSkeleton) default-ON (char-skinning-deform fix acd9c19a; W0.5 fail-red control) [BandChar... |
| RB3_SKEL_REBIND_CALCOFF | workaround | skinning | not-live: A/B variant of the outfit rebind (SetBone calcOffset=true); shipped default is calcOffset=false [BandCharacter.cpp:1110] |
| RB3_SKEL_REBIND_FULL | workaround | skinning | not-live: KNOWN-BROKEN full-body rebind (shards thin geo), default-OFF study/W0.1 fail-red control; shipped rebind is torso-only [BandCharacter.cpp:1084] |
| RB3_SKIN_FIX_OFF | workaround | render/c8-faces | not-live: skin RT-recolor compose rebind (only active when RB3_SKIN_RTT is also set); opt-out RB3_SKIN_FIX_OFF=1 (see project_c8_faces memory) [OutfitConfig.... |
| RB3_SKIN_NOCACHE | perf | skinning | n/a: A/B disable of the per-member skinned-mesh cache (measurement only; default cached path is a perf optimization, see incremental-load-perf memory) [BandC... |
| RB3_SKIN_RTT | feature | render/c8-faces | not-live: gates the engine skin-RTT composite path (broken on web); default-OFF ships the direct-bind diff x skin-tone bypass (see project_c8_faces memory 26... |
| RB3_SKIN_TIMING | probe | skinning | n/a: timing print summing skinned-mesh cache rebuild vs cache-hit cost [BandCharacter.cpp:846] |
| RB3_WALKON_SNAP_OFF | workaround | bandobj/walkon | not-live: walk-on count-in pose snap default-ON (avoids frozen stale-vignette pose; see walkon-countin-pose memory 67e87ae1) [BandCharacter.cpp:59] |
| SET_SKEL_REBIND | workaround | skinning | not-live: superseded experimental whole-subtree rebind (SetBone), study-only default-OFF; shipped fix is the renderer-side SKEL_REBAKE static-pose offset [Ba... |
| MILO_HEADLESS | feature | platform/headless | n/a: headless runtime mode (skips window/audio/GPU device init; UI.cpp fakes a fixed 1/30s UI clock). Real port toggle, not a fidelity stand-in. |
| RB3_APPLY_HANDLER_FIX_OFF | workaround | bandobj/trackpanel | not-live: single-player scoreboard/applause right/left.grp x-translation neutralization (K9 apply-handler fix) default-ON; =1 reverts to raw V22 |
| RB3_BILLBOARD_OFF | workaround | render/billboard | not-live: native RndMultiMesh::DrawShowing kFastBillboardXYZ branch default-ON; opt-out disables the native billboard branch |
| RB3_CAM_FALLBACK_OFF | workaround | camera/voidcut | not-live: BandDirector voidcut last-good-cam fallback (avoids dropping to void) default-ON; =1 reverts to raw V22 follow |
| RB3_LOADER_BUDGET_MS | perf | load/perf | n/a: Loader frame-drain budget in ms (default 8; huge value restores unbudgeted drain-to-completion). See project_incremental_load_perf memory. |
| RB3_LOADER_MIN_YIELD_MS | perf | load/perf | n/a: min yield interval for synchronous loader/stream drain in ms (default 16; 0 restores per-slice yield). See project_incremental_load_perf memory. |
| RB3_LOADER_READAHEAD | perf | load/perf | n/a: loader pipeline read-ahead depth (default 6; 0 disables). HX_NATIVE-only. See project_incremental_load_perf memory. |
| RB3_LOADER_YIELD_MS | perf | load/perf | n/a: loader spin yield interval in ms (default 16, clamped >= budget). Boot-time-neutral tunable. See project_incremental_load_perf memory. |
| RB3_MENU_VOID_FIX_OFF | workaround | render/menu-void | not-live: menu-void mesh cull fix default-ON; =set reverts to baseline draw |
| RB3_METAMUSIC_SYNC | workaround | synth/metamusic | not-live: native async MetaMusic PostLoad default; =1 opt-in restores the original eager (blocking) PostLoad+wiring path |
| RB3_NO_CROWD_INTRO | workaround | audio/crowd | not-live: native crowd_intro/venue_intro BinkClip mogg bridge default-ON; =set disables the intro synth bridge |
| RB3_PREWARM_NEXT | perf | load/perf | n/a: from:to screen prewarm-pair spec string (default main_hub_screen:song_select_screen). Asset prewarm scheduling, no draw/logic change. See project_increm... |
| RB3_PREWARM_SCREENS | perf | load/perf | n/a: UI screen/panel asset prewarm+adopt (web default-ON opt-out via '0'; native default-OFF opt-in). No correctness dependency at the adopt site. See projec... |
| RB3_REFRACTION_FIX_OFF | workaround | render/refraction | not-live: song_select bottom_square_refraction cull fix default-ON; =set restores baseline draw |
| RB3_RESYNC_YIELD_OFF | workaround | synth/stream | not-live: StandardStream resync yield default-ON; opt-out disables the resync yield |
| RB3_REVIEW_LIGHTER_FIX_OFF | workaround | ui/review | not-live: ReviewDisplay lighter-slot show fix (hide slots on zero score) default-ON; =1 restores baseline |
| RB3_SCROLLBAR_FIX_OFF | workaround | ui/scrollbar | not-live: ScrollbarDisplay content-aware draw gate default-ON; =1 restores the exact Wii over-draw gate |
| RB3_STREAM_BUF_SECS | perf | synth/stream | n/a: StandardStream min buffer depth in seconds (default 4, capped by 16-chunk ~9.1s ring). Anti-underrun value knob. |
| RB3_STREAM_PREPLAY_CAP_OFF | workaround | synth/stream | not-live: StreamReceiver pre-play write cap (fill ring before Play, clock reads 0) default-ON; =1 opts out |
| RB3_TV3_PLAY_OFF | workaround | world/transition | not-live: vignette_transition (tv3) WorldDir force-play default-ON (bounds sequencer to authored sub-shot durations); =set escapes |
| RB3_VENUE_FRUSTUM_CULL | perf | render/cull | n/a: opt-in world.cam venue frustum sphere cull (default-OFF); pure draw-skip optimization, confirmed Draw.cpp:201 |
| RB3_VENUE_SYNC | workaround | load/venue | not-live: native forces synchronous venue load (correct ordering) default-ON; =0 opts into experimental async (unsafe until Enter() is split into a multi-fra... |
| RB3_WEB_OFFMAIN_MIX | feature | audio/web | n/a: off-main-thread audio decode/mix mode (web survives main-thread freezes; native uses it to size the decode-ahead ring deeper) default-ON; =0 keeps the p... |
| VENUE_CAM_LOCK | workaround | camera/venue | not-live: native bridge points the venue WorldDir mCam at the director's active shot cam each frame default-ON; =1 reverts to the static cam |
| BAND_ANIM_BONE | probe | char/anim | n/a: names the bone BAND_ANIM_PROBE samples (defaults to bone_R-upperArm.mesh); only read when BAND_ANIM_PROBE is active, never alters Poll() [BandCharacter.... |
| BAND_ANIM_PROBE | probe | char/anim | n/a: per-frame trace of the band animation chain (driver presence, playing clip, named bone worldPos pre/post Character::Poll) to localize why the on-stage s... |
| BONE_CLEAR_DBG | probe | skinning | n/a: logs any CopyBones(0) that wipes a non-empty bone list off a mesh, to catch post-load bone clears [Mesh.cpp:1156] |
| BONE_LOAD_DBG | probe | skinning | n/a: reports how many bones a mesh loaded from the file vs. how many resolved null, before RemoveInvalidBones strips them [Mesh.cpp:1000] |
| CAMDIR_DBG | probe | render/camera | n/a: logs the re-run HarvestDircuts() call once venue+song.anim are both live (propAnim/venueDir pointers); render-inert, aliases VENUE_DBG's same line [Band... |
| CAM_DBG | probe | render/camera | n/a: dumps game.cam pose (pos/fwd/up/fov/near/far) during highway draw at TrackDir.cpp:280 (game.cam is only current at this scope point); also gates a separ... |
| CBM_DBG | probe | skinning | n/a: V38 instrumentation — dumps decoded SCALE channel + pre-divide matrix row lengths per scale-bone, localizing the crowd/extras 0.53-det Y-squash [CharBon... |
| CBM_DBG2 | probe | skinning | n/a: V38 probe — dumps quat magnitude pre-Normalize and post-QUAT/ROT LocalXfm determinant for a substring-matched bone name (or '*') [CharBonesMeshes.cpp:11... |
| CBS_DBG | probe | skinning | n/a: dumps the cached packed-buffer layout (offsets, sizes, compression) vs bytes actually consumed, to verify against the DC3 Save contiguous layout [CharBo... |
| CHARDRV_PROBE | probe | char/anim | n/a: confirms CharDriver::Poll runs and whether a clip is playing/applying, substring-matched by ClipType (or '*'); two sites (pre- and post-apply) [CharDriv... |
| CHAR_DBG | probe | render/char-material | n/a: two independent print sites sharing a name — engine RB3MaterialBinder.cpp reports whether a skinned outfit mesh resolved a diffuse texture (untextured-b... |
| CLOCK_DBG | probe | ui/hud | n/a: logs TrackDir::DrawShowing's real-time delta and y-per-second scroll multiplier for a track-panel HUD overlay [TrackDir.cpp:302] |
| CROWD_REBIND_PROBE | probe | render/crowd | n/a: verbose companion to RebindCrowdCharBonesToOwnSkeleton (the default-ON RB3_NO_CROWD_REBIND workaround) — read-only diagnostic flag, does not itself chan... |
| GAME_DBG | probe | bandobj/flow | n/a: logs BandDirector::OnFileLoaded's sym/dir args and ReadyForMidiParsers' native venue-deferred gate inputs/result; the gate's own cond is computed indepe... |
| GEM_DBG | probe | render/highway | n/a: dumps GemTrackDir::UpdateSurfaceTexture's mesh/mat/tex name state after the (unconditional) SetDiffuseTex call [GemTrackDir.cpp:393] |
| HEAD_REBIND_PROBE | probe | skinning | n/a: verbose companion to RebindHeadHandsAtRest (the default-ON RB3_NO_HEAD_REBIND-gated rest-capture rebind); read-only, does not alter the rebind [BandChar... |
| IK_TGT_DBG | probe | char/ik | n/a: reports IK hand world position + each target's name/pos/distance/parent chain for far (>50u) targets; investigative tool for the residual crowd hand-IK ... |
| INST_REBIND_PROBE | probe | skinning | n/a: verbose companion to the instrument-strings rebind (RB3_NO_INST_REBIND/RB3_INST_STRINGS_MODE); read-only [BandCharacter.cpp:1551] |
| K9_APPLY_DBG | probe | ui/hud | n/a: dumps the milo 'apply' handler dispatch state (config object name/class/typedef apply-array, objects/visibles/xfms arrays) on TrackPanelDirBase::SetConf... |
| MENU_VOID_DBG2 | probe | render/ui | n/a: render-inert one-shot-per-name material dump for hub-backdrop drawables (sky/dome/moon/cloud/star/night/bgbuilding/fog-keyed by default, or every mesh w... |
| MENU_VOID_SKIP | probe | render/ui | n/a: comma/space-separated substring skip-draw list — an opt-in A/B research tool for isolating which drawable paints a given hub-backdrop screen region (not... |
| MESH_BONE_DBG | probe | skinning | n/a: dumps each loaded RndBone's bind-offset determinant + row lengths for substring-matched mesh names, checking whether the crowd-body det-0.53 squash is a... |
| MILO_LOCALE_DBG | probe | utl/locale | n/a: traces Localize() token/format resolution for diagnosing format-string/token leaks [Locale.cpp:296] |
| MILO_SETTOKEN_DBG | probe | ui/label | n/a: traces UILabel::SetTokenFmtImp — symbol, localized format string, and final SetDisplayText input [UILabel.cpp:805] |
| PART_INIT_DBG | probe | render/particles | n/a: dumps InitParticle's emit branch, raw direction and mesh state before speed scale for substring-matched particle systems, diagnosing runaway street-fog ... |
| PART_MOVE_DBG | probe | render/particles | n/a: per-call MoveParticles trace (dt/frameSpan, relative-frame force rows, relative xfm state, first particle velocity) for substring-matched systems, pinpo... |
| RB3_DUMP_STEMS | probe | audio/stems | n/a: opt-in raw PCM stem dump to disk (one .s16 file per channel under the given directory); purely a side-channel file write, does not alter the audio mix/p... |
| RB3_HAIR_DBG | probe | char/hair | n/a: reports per-strand collide-hookup counts (points hooked, CharCollides reachable) to quantify free-hanging strands passing through the skull/shoulders un... |
| RB3_METAMUSIC_DBG | probe | synth/metamusic | n/a: logs completion of the deferred stream-FX wiring (6 eq.send dirs) after RB3_METAMUSIC_SYNC's async PostLoad path drains [MetaMusic.cpp:415] |
| RB3_NOTIFY_ALL | probe | os/debug | n/a: console-verbosity toggle — restores every repeat of an engine NOTIFY diagnostic instead of the default per-message dedup; affects only diagnostic consol... |
| RB3_PLACEMENT_PROBE | probe | render/placement | n/a: dumps each crowd instance's spXfm translation used by the W2.1 RB3_PLACEMENT_CONTRACT placement oracle; Wii-compile-inert, no behavior change [Crowd.cpp... |
| RB3_PP_PROBE | probe | bandobj/patchmesh | n/a: dumps BandPatchMesh::WorkVerts::SetMeshVerts entry state (vert/face counts, max face index, OOB check) added while triaging the C8 head-invisible regres... |
| RB3_PREWARM_DBG | probe | load/perf | n/a: two independent print sites sharing a name — game UIScreen.cpp logs the song_select-prewarm screen-pair schedule/CheckIsLoaded gate; engine Rnd_Wgpu_RB3... |
| RB3_READAHEAD_DEBUG | probe | load/perf | n/a: reports loader queueDepth/kicked counts to show whether dependency milos enqueue deep enough for read-ahead to matter; zero cost when off, does not chan... |
| RB3_SKINFIX_DBG | probe | skinning | n/a: logs every MatSwap touched by the outfit skin-diffuse rebind loop and what it rebinds, added while triaging the C8 head-invisible regression [OutfitConf... |
| RB3_STATS_DBG | probe | debug/stomp-watch | n/a: two independent stomp-watch bisection sites sharing a name — File.cpp checks MetaPerformer integrity on every file open (web song-end wedge diagnosis); ... |
| RELOAD_PROBE | probe | bandobj/flow | n/a: answers 'what reloads the band mid-song' by logging every RecomposePatches/MiloReload/start_load-DTA/in_closet-propsync/CharCache/BandWardrobe reload ca... |
| SERVO_PROBE | probe | char/anim | n/a: confirms CharServoBone::Poll runs and reports a mid-chain bone's LocalXfm before/after PoseMeshes, substring-matched by ClipType (or '*') [CharServoBone... |
| SKEL_REBIND_PROBE | probe | skinning | n/a: verbose companion to RebindOutfitBonesToOwnSkeleton (RB3_NO_SKEL_REBIND) and the SET_SKEL_REBIND study variant — counts rebound/same/null-own bones; rea... |
| SKEL_REBIND_SKINPOS | probe | skinning | n/a: post-rebind verification metric — reports the worst \|skinWorld-boneWorld\| bone-relative delta after RebindOutfitBonesToOwnSkeleton, the correctness me... |
| STRIDE_PROBE | probe | skinning | n/a: dumps mesh name/vert count/per-vert size/version/skinned-flag when reading the platform-compressed vertex blob, verifying the native compressed-vert rea... |
| UISCREEN_DBG | probe | ui/hub | n/a: logs UI transition-state changes (UI.cpp) and screen/panel Exiting()/CheckIsLoaded() blocked-panel state (UIScreen.cpp, UIPanel.cpp), dedup'd per state/... |
| VENUE_DBG | probe | bandobj/venue | n/a: logs the native venue force-load bridge in BandDirector::EnterVenue (override symbol, chosen venue, sync/async mode, wardrobe/venueDir/venueName state, ... |
| VOIDCUT_DBG | probe | render/camera | n/a: logs each shot's void-vs-keep verdict for the RB3_CAM_FALLBACK_OFF-gated sticky last-good-cam fallback (voidcut avoidance); read-only, does not affect t... |