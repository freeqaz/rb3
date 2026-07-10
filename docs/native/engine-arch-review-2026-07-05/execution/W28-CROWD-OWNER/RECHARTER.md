# W28-CROWD-OWNER — RE-CHARTER (Lever B) → hand-off to W29

**Status:** Lever B accepted. This document is the Lever-B deliverable per WAVE28
acceptance A6(iii): it names the REAL main_hub walkers, their NATIVE state, and the
ACCEPTANCE TARGET SET for subsequent waves, with the same evidence rigor as STEP-0.
All claims trace to `evidence/raw/step0-combined.log.gz` (grep the probe tags).

## Why the bug was mis-scoped for five waves (W23→W28)

Every prior wave measured the crowd that plays `crowd1-5.clp` and freezes at beat 2.433
and assumed it was main_hub's crowd. It is **not**. The STEP-0 ownership dump proves:

- The 8 walker figures are **shared character proxies** —
  `char/crowd/crowd_{male,female}0N.milo` (each drives a `main.drv` CharDriver). They are
  NOT owned by any one vignette; a vignette binds its own clip set onto them.
- During **splash**, they are bound to the **sv8 CITYSCAPE** clip set
  (`world/vignette/shell/sv8/a/cityscape/cityscape_clips.milo`) and play `crowd1-5.clp`.
- At the splash→main_hub transition (beat 2.433), `sv8_panel` unloads (**faithful** —
  the splash backdrop is supposed to unload), destroying `cityscape_clips`; the drivers
  then **correctly rebind** to the **sv3 STREETSLOMO** clip set
  (`world/vignette/shell/sv3/a/streetslomo/streetslomo_clips.milo`), which is main_hub's
  resident backdrop (`main_hub.dta:744` panels = `sv3_panel …`).

So the "freeze" everyone chased was the SPLASH crowd faithfully dying. It was never a
binding bug, a panel-residency bug, a load-merge bug, or a teardown bug.

## The REAL main_hub walkers

| Property | Value | Evidence |
|---|---|---|
| Char dirs | `char/crowd/crowd_{male,female}0N.milo` × 8 (male01-04, female01-04) | CHARDRV_CLIPSWAP `drvPath` |
| Driver | `main.drv` (CharDriver), `mClipType = 'crowd'` | CHARDRV_ENTER |
| Bound clip set on main_hub | `clips` in `world/vignette/shell/sv3/a/streetslomo/streetslomo_clips.milo` (resident) | CHARDRV_CLIPSWAP beat=2.433 |
| Walk clips (what SHOULD play) | `player0_f, player0_m, player1_f, player1_m, player2_f, player2_m, player3_f, player3_m` | streetslomo_clips.milo runtime NOTIFY + sv3_a raw strings |
| mDefaultClip | serialized `''` → NULL (FAITHFUL; no driver-level auto-replay by design) | CHARDRV_DEFCLIP × 8 |
| Vignette dir | `sv3_a` → `streetslomo_ao` (PanelDir) | PANELDBG PanelDir::Enter |

## Their NATIVE state (the actual gap)

After the correct rebind to `streetslomo_clips`, the drivers sit **idle**:

- **Zero `CHARDRV_PLAY` after beat 2.433** — nothing issues `play_clip` on the crowd
  drivers for any `playerN_*` streetslomo walk clip.
- `PanelDir::Enter dir=streetslomo_ao` and `dir=sv3_a` fire with **`nTriggers=0`** — the
  scene has no `UITrigger` in `mTriggers` to restart the walk (matches W27 item 4).
- `mDefaultClip == NULL` (faithful) → the beat-2.433 re-Enter cannot auto-play.
- Net: `animating = 0`, the crowd is bound-but-undriven.

The missing driver is the **streetslomo scene's own start mechanism** — a
`vignette_start.trig` / `ns_start.eventanm` (or the FileMerger `player0-3` proxy binding
seen at `C13_PROBE` lines 206-209) that on Wii issues the `play_clip` for the
`playerN_*` clips when streetslomo enters. That mechanism is in the **world / vignette /
eventanm / trigger layer**, NOT in CharDriver/CharClip. That is why no CharDriver clip-
binding lever could ever have fixed it.

## W29 ACCEPTANCE TARGET SET (for the census + future gates)

"Main_hub crowd walkers animate" means, precisely:

1. **Target drivers:** the 8 `char/crowd/crowd_{male,female}0N` `main.drv` CharDrivers,
   while `main_hub_screen` is active and their `mClips` resolves to
   `streetslomo_clips.milo` (assert the PathName, not just the count).
2. **Animating criterion:** each target driver has `CHARDRV_PLAY` of a `playerN_{f,m}`
   clip AFTER beat 2.433 and `FirstPlaying() != NULL` (`animating > 0`) sustained on
   main_hub — NOT the transient cityscape `crowd1-5` plays at beat 0.
3. **Do NOT** count the splash/cityscape crowd (crowd1-5, sv8) as main_hub walkers; it
   correctly animates during splash and correctly dies at the transition. Any census that
   measures `animating` at/around beat 2.433 without pinning `mClips==streetslomo_clips`
   is measuring the wrong crowd (the W23 ambiguity that caused this supersession chain).

## Recommended W29 charter (scene-trigger lane, NOT a CharDriver lane)

Investigate and drive the streetslomo walk trigger:

- Enumerate how the **cityscape (sv8)** crowd gets its `crowd1-5.clp` `play_clip` at
  beat 0 (the working reference) — the scene object / eventanm / FileMerger proxy path —
  then find streetslomo's equivalent and why it does not fire natively (`nTriggers=0`).
- Likely surfaces: `world/` vignette scene load, `PanelDir`/`WorldDir` eventanm
  execution, the `player0-3` FileMerger proxies (`C13_PROBE`), or the milo
  `.trig`/`.eventanm` objects inside `sv3_a/streetslomo`. Owner: world/vignette lane.
- Flag name reserved but UNUSED here: `RB3_HUB_CROWD_CLIPBIND` is the WRONG vocabulary
  for the real fix (it is not a bind fix). W29 should choose a trigger-layer name.

### Folded-in deferred thread: verts=0 / near-black

W25-W27 noted the crowd meshes render with `verts=0` (near-black / no geometry) — a
SEPARATE material/geometry discriminator that was never reached because `animating>0`
was never achieved. It stays deferred: it can only be evaluated once the walkers are
actually driven (a moving-but-invisible walker vs a still-but-visible one are different
symptoms). W29 must reach `animating>0` FIRST, then re-open verts=0 as a follow-on.

## What this lane did NOT do (scope honesty)

- No fix code, no flag, no default flip, no pin bump, no census/sidecar/golden edits.
- Did not touch the protected `Crowd.cpp:884-1000` gameplay oracle, the RndMesh loader,
  or the hands/FOREARM families.
- Probe additions only (CharDriver CLIPSWAP/DEFCLIP, UIScreen/UIPanel beat stamps) — all
  `#ifdef HX_NATIVE`, env-gated, Wii `.o` byte-identical (batch_objdiff == baseline).
