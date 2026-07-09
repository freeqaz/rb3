# W26-CROWD — STATUS

## Headline

**Discriminator: NEITHER dup NOR legit merge. The A1 FileMerger hypothesis is
REFUTED.** The beat-2.433 crowd-clip kill is a **UI panel-unload teardown** during
the splash→main_hub screen transition, not a `FileMerger::Merger::Clear`.

**Fixed? NO — honest partial (narrowed, out-of-lane-surface).** The real fix is in
`ui/UIScreen`+`UIPanel` / world / the vignette DTA, OUTSIDE this lane's owned files
(`FileMerger.cpp` + `CharDriver.cpp`). No CharDriver/FileMerger-scoped recovery
exists: the crowd clips are freed exactly once and `streetslomo_clips.milo` is
never reloaded. This is the W25-model outcome — a precise root cause + a clean
hand-off, with byte-inert instrumentation and the E-C3 cleanup landed.

## Discriminator verdict (checkpointed before any fix — /tmp/wave26-checkpoints/CROWD.json)

Two probes, both HX_NATIVE + env-gated (Wii byte-identical):

1. **`FMERGE_PROBE` (FileMerger.cpp `AppendLoader`/`NotifyFileLoaded`)** — logs
   every FileMerger merge/clear with owning path + old/new FilePaths. Result: all
   182 events at boot are the **band-players wardrobe merger**
   (`char/main/main.milo`: torso/legs/head/hands/…). **ZERO** touch
   crowd/streetslomo/sv3. The last FileMerger activity is ~19 frames (237 log
   lines) BEFORE the crowd kill, with no `Merger::Clear` in between.
   → A1's "second load through the same Merger slot" mechanism does not occur.

2. **`CHARDRV_BT` (backtrace at `CharDriver::Replace`)** — symbolized deleter chain
   for `crowd4.clp`/etc at beat 2.433 (frame 72):
   ```
   UIManager::Poll → BandScreen::Enter → UIScreen::Enter → UIScreen::UnloadPanels
     → UIPanel::CheckUnload → UIPanel::Unload → WorldDir::~WorldDir
       → (nested ObjectDir/PanelDir/RndDir) → CharClipSet::~CharClipSet
         → CharClip::~CharClip → Hmx::Object::~Object → Replace(clip,NULL)
           → CharDriver::Replace → DeleteClip → mFirst=NULL
   ```
   The `streetslomo` crowd `CharClipSet` lives inside a **panel WorldDir that is
   UNLOADED** on the splash→main_hub transition. NOT a merge.

**Bank-state correction to W25:** post-kill the drivers KEEP their shared `clips`
ObjPtr (`clipsName='clips'`) — it is NOT swapped to a "player-only bank". Rather
`nclips` drops **11→8**: the 5 `crowd*.clp/clip` objects are the ones DELETED by
the panel teardown, leaving 8 non-crowd clips. So `mClips->FindObject('crowdN.clp')`
returns null and the `RB3_CROWD_CLIP_KEEP` re-arm cannot fire (re-tested on this
build, flag-ON: still `animating=0`, no re-Play). `streetslomo_clips.milo` loads
exactly once at boot (8 `mDoNotCompress` events) and is **never reloaded** → no
surviving or resurrected crowd bank anywhere to re-point to. The W25 cross-bank UAF
wall was a symptom of exactly this: there is no live crowd bank to reach.

## Why no in-lane fix (honest)

A2's "fix the bank resolution, let the flag re-Play" is BLOCKED: the clip objects
are genuinely freed and never reloaded, so there is no crowd-bearing bank for a
`CharDriver`/`FileMerger` re-arm to resolve against. Preventing `DeleteClip` dangles
`mFirst` (UAF next Poll — W25 proved this). The teardown originates in
`UIScreen::UnloadPanels`/`UIPanel::Unload` (ui/ files) tearing down a shared
vignette `WorldDir` — none of that is in `FileMerger.cpp` or `CharDriver.cpp`.

## Hand-off (engine/ui-side; coordinator to route to a W27 ui/world lane)

The native divergence: on the splash→main_hub transition, native **unloads the
`streetslomo` vignette panel that owns the hub crowd's clip set** (and does not
re-trigger the walk), where Wii keeps the hub crowd walking. Candidate fixes
(all outside CROWD's owned files):
- Keep the `streetslomo` vignette WorldDir/panel resident (or `mAlwaysLoad`) across
  the transition so its `CharClipSet` is not torn down; OR
- reload `streetslomo_clips.milo` under main_hub and re-fire `play_clip`
  (`vignette_start.trig`) on the 8 crowd drivers after the transition settles.
Sites: `ui/UIScreen.cpp:575` `UnloadPanels`, `ui/UIPanel.cpp` `Unload`/`CheckUnload`,
the main_hub screen's panel definition (`mAlwaysLoad`/referenced), and the hub
vignette DTA that fires the initial `play_clip`.

## What landed (this lane, default-OFF / byte-inert)

- `FileMerger.cpp`: `FMERGE_PROBE` discriminator logging (AppendLoader + the
  NotifyFileLoaded kill site). HX_NATIVE + env-gated.
- `CharDriver.cpp`: `CHARDRV_BT` backtrace probe at `Replace`; corrected the
  root-cause comment (was: FileMerger merge/bank-swap → now: UI panel-unload
  teardown); **E-C3 cleanup** — `gCrowdKeep` entry pruned in `~CharDriver`
  (stale-key alias fix when flag-ON).
- `RB3_CROWD_CLIP_KEEP` stays **default-OFF**. **E-C2 ruling:** it remains
  prophylactic scaffolding — in the observed repro it recovers ZERO drivers (no
  surviving crowd bank). It should be REMOVED once (a) the W27 ui/world fix lands and
  keeps/reloads the streetslomo crowd, OR (b) the coordinator confirms the crowd is
  fixed by other means. It is harmless while default-OFF; not promoting it.

## Gates

| Gate | Result |
|---|---|
| discriminator checkpointed before fix | **DONE** (`/tmp/wave26-checkpoints/CROWD.json`, verdict = panel-unload, A1 refuted) |
| `batch_objdiff` (CharDriver::Poll) | **PASS** 93.54% == report.json baseline 93.54499% (UNCHANGED) |
| `batch_objdiff` (Replace / AppendLoader / NotifyFileLoaded) | **PASS** 100% each — Wii byte-identical (all edits HX_NATIVE) |
| drawlog-golden `--fixed-clock --canonical-order` (flag-OFF) | **PASS 792** draws; EXIT 0; all divergences `field=world` crowd-pose jitter, run-to-run varying (inherent, not my change) |
| rb3-tests | **PASS 116 / 0 fail** (7 skipped) |
| WorldCrowd A/B (flag-ON, gameplay) | **PASS (safe)** — game_screen + song-end game-over reached, crash-free, ZERO crowd CharDriver events in gameplay → provably dormant |
| crowd census `animating>0` + 8 lit isolate figures | **NOT MET** — recovery impossible in-lane (crowd clips freed once, never reloaded; no bank to re-arm). Root-caused + handed off. |
| near-black material discriminator | **moot/deferred** — never reached `animating>0` (same as W25) |

## Files

- `src/system/char/FileMerger.cpp` — FMERGE_PROBE discriminator logging (HX_NATIVE, env-gated, inert).
- `src/system/char/CharDriver.cpp` — CHARDRV_BT backtrace probe + corrected comment + E-C3 gCrowdKeep prune.
- `docs/.../W26-CROWD/{PLAN.md, STATUS.md, evidence/*}`.
- Commit `5b7aabc5` (recon + E-C3).
