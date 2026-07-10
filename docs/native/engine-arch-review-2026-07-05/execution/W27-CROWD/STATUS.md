# W27-CROWD — STATUS

## Headline

**Panel-residency REFUTED. The W26 root cause (splash->main_hub UI panel-unload tears
down the streetslomo `CharClipSet`) does NOT reproduce on this build.** Native runtime
tracing proves `sv3_panel` is **RESIDENT** across splash->main_hub — the interstitial->
regular-panel refcount handshake works exactly as on Wii (`mLoadRefs` 1->2->1, never
hits 0) — and the crowd `CharClipSet` is **never torn down** (`CHARDRV_CLEAR`=0).

The real mechanism is a **char-layer clip-REPLAY divergence, not a ui/world teardown**:
the 8 crowd `CharDriver`s exist, are polled, showing, and driven, but their walk clip
plays **once** (a one-shot scene animation during splash), ends by beat ~2.433, and is
**never replayed** — `mDefaultClip` is NULL on every crowd driver and the panel Enter
does not re-fire the walk trigger. Result: `animating=0`.

**Fixed? NO — honest REFUTATION + corrected root cause + hand-off.** No faithful fix
exists in this lane's owned ui/world/interstitial files: the ui layer is already
correct. The fix belongs in the char clip/driver layer (protected / not-owned this
lane). This supersedes BOTH W25 (merge/bank-swap) AND W26 (panel-unload teardown).

## STEP 0 verdict (checkpointed before any fix — /tmp/wave27-checkpoints/CROWD-step0.json)

`verdict = resident` (sv3_panel resident; real divergence is char-layer clip-replay).
`chosen_lever = NONE-IN-GRANT`. `flag = RB3_HUB_CROWD_REFIRE` reserved, NOT used.

### Evidence (all probes HX_NATIVE + env-gated `RB3_CROWD_PANEL_DBG`, byte-inert)

1. **Refcount handshake WORKS (panel resident).** `sv3_panel`:
   `CheckLoad refs->1` (splash `LoadInterstitials`), `CheckLoad refs->2` (main_hub
   `UIScreen::LoadPanels`), `CheckUnload refs->1` (splash `UnloadInterstitials`).
   splash's `UnloadPanels` unloads only `splash_panel`+`sv8_panel` (NOT sv3).
   **sv3_panel stays resident at refs=1 under main_hub — never hits 0.**
   (evidence/panel-refcount-trace.log lines 338/455/468/555.)
2. **ZERO teardown.** `CHARDRV_PROBE=crowd`: 0 `CHARDRV_CLEAR` events — `Clear()` never
   wipes a live clip on any crowd driver. W26's "nclips 11->8, crowd clips DeleteClip'd
   by the panel teardown" does NOT occur.
3. **Walk plays once, then freezes (no replay).** Each of the 8 crowd drivers gets 2
   `CharDriver::Enter` (beat 0 during splash; beat 2.433 at the transition). `mDefaultClip
   = NULL` at BOTH enters on ALL drivers. The walk clip plays during splash (`mFirst` set)
   and ENDS naturally by beat 2.433 — the beat-2.433 re-Enter shows `mFirstAtEntry=(nil)`
   BEFORE its `Clear()`, i.e. the clip **already ended**, it was NOT killed by the Enter.
   `CHARDRV_DIE` (mFirst set->null) fires for all 8 at pollFrame=72 beat=2.433. Because
   `mDefaultClip` is null, the re-Enter does not replay. (evidence/chardriver-state.log.)
   → **The beat-2.433 event W26 read as a "teardown" is actually the walk clip ending +
   a missed driver-level replay, not a panel/clip-set deletion.**
4. **Panel Enter does not re-fire the walk.** `PanelDir::Enter` fires for `sv3_a` and
   `streetslomo_ao` at the transition with **nTriggers=0** — `vignette_start.trig` (the
   scene/eventanm object that starts the walk) is not a `UITrigger` in `mTriggers`, so
   panel Enter cannot restart the crowd. (evidence/paneldir-census.log.)
5. **Census:** `crowd_chars=8 showing=8 polled=8 driven=8 animating=0 onscreen=8`.
   Drivers all present/polled/shown; none playing. (Also mesh `verts=0` — a separate
   near-black/no-geometry issue, out of scope; deferred material discriminator never
   reached since `animating>0` was never achieved.)
6. **DTA confirms Wii-GT (data-side).** splash panels = `meta sv8_panel splash_panel`
   (splash.dta:171, NO sv3); main_hub panels = `meta sv3_panel main_hub_panel
   accomplishments_status_panel` (main_hub.dta:744); `splash_screen` interstitial -> `sv3`
   (config/vignettes.dta:126-129). The Wii-GT residency hypothesis is confirmed by native
   runtime trace, not just the data.

### Root cause (corrected)

The hub crowd walk is a **one-shot scene animation** (`vignette_start.trig` /
`ns_start.eventanm` inside `sv3_a`/streetslomo) that plays on scene load during splash
and ends ~beat 2.433. On Wii the walk persists (a looping walk-cycle clip and/or a
driver `mDefaultClip`/starve-replay). Natively the crowd `CharDriver`s have
`mDefaultClip=NULL` and the clip does not loop/replay, and the panel Enter does not
re-fire the trigger (`nTriggers=0`). Net: the crowd freezes. **This is a char/world
`CharDriver`+`CharClipSet` clip-config/replay divergence, NOT ui panel-residency.**
(A secondary `clips` split — some drivers on an 11-clip set, some on an 8-clip set,
both named `clips` — is a proxy/char-layer detail that does not change the conclusion.)

## Why no in-lane fix (honest)

The owned ui/world/interstitial surface (`UIScreen`/`UIPanel`/`PanelDir`/`UI`/
`BandScreen`/`InterstitialMgr`) is **already correct**: the refcount handshake keeps sv3
resident, there is no teardown, and the panel machinery cycles cleanly with no leak (A7).
Neither lever applies: lever A (restore residency) is moot — already resident; lever B
(reload + re-fire) is moot for the reload part (no unload to reverse) and the re-fire part
requires the protected `CharDriver::Play` API + char-layer knowledge of the walk clip — a
non-faithful hack that risks the drawlog gate. Per anti-overclaim discipline, no ui-side
workaround was forced.

## Hand-off (char/world clip-config lane — coordinator to route W28)

The faithful fix is in the char clip/driver layer (protected / not-owned this lane):
- `CharDriver` `mDefaultClip` resolution on the crowd proxy drivers
  (`CharDriver.cpp:923` `mDefaultClip.Load(bs, false, mClips)` resolving NULL); OR
- the walk clip's loop/hold flag in `CharClip` parse (one-shot vs looping walk cycle); OR
- `mDefaultPlayStarved` / the starve-replay path (`CharDriver.cpp:623-624`, `defStarved=0`).
Also investigate the 11-clip vs 8-clip proxy `clips` split (FileMerger/proxy binding).

**E-C2:** `RB3_CROWD_CLIP_KEEP` was ALWAYS a no-op on this build (no teardown ever occurs;
`CHARDRV_CLEAR`=0), so it recovers zero drivers. **Confirmed removable** (+ its E-C3
`gCrowdKeep` prune) — coordinator decides at close-out.

## What landed (this lane, default-OFF / byte-inert)

Diagnostic probes only (all `#ifdef HX_NATIVE`, env-gated `RB3_CROWD_PANEL_DBG`,
0 behavioral change with the env unset; 7/7 touched decomp fns 100% objdiff):
- `src/system/ui/UIPanel.cpp` — `CheckLoad`/`CheckUnload` per-panel `mLoadRefs` trace.
- `src/system/ui/UIScreen.cpp` — `LoadPanels`/`UnloadPanels` call-site markers.
- `src/system/ui/PanelDir.cpp` — `Enter` PanelDir + trigger-list trace.
- `src/band3/meta_band/BandScreen.cpp` — `LoadInterstitials`/`UnloadInterstitials` trace.
NO flag added (no lever). NO default flips. NO pin bump.

## Gates

| Gate | Result |
|---|---|
| STEP 0 checkpoint before fix | **DONE** (`/tmp/wave27-checkpoints/CROWD-step0.json`; verdict=resident, panel-residency REFUTED) |
| batch_objdiff (7 touched decomp fns) | **PASS** — `CheckLoad/CheckUnload__7UIPanel`, `Load/UnloadPanels__8UIScreen`, `Enter__8PanelDir`, `Load/UnloadInterstitials__10BandScreen` all **100.0% raw+fuzzy** (byte-identical to baseline) |
| drawlog-golden `--fixed-clock --canonical-order` (flag-OFF) | **PRE-EXISTING FAIL, not mine.** My build: count=792, 70-72 unexpected `field=world` pose divergences, varying run-to-run. **Clean-HEAD worktree baseline (no my changes) is statistically IDENTICAL: count=792, 71-72 `field=world`, varying** — the gate is flaky at HEAD from inherent crowd-pose jitter (ALL divergences `field=world`, count stable, ZERO structural/bind-collapse). My probes are byte-inert (objdiff 100% + getenv-gated). W26's "PASS 792" no longer holds at current HEAD, independent of this lane. |
| rb3-tests | **PASS 116 / 0 fail** (7 skipped; trailing SIGSEGV is the known post-test teardown) |
| boot A/B (no flag — stability) | **PASS** — splash->main_hub->song_select all reached crash-free across every probe boot; no asserts |
| A7 revisit (hub->song_select->hub) | **PASS (memory-sanity).** sv3_panel refcount: `1->2->1` (resident@hub#1), `->0 UNLOAD` across hub->song_select (Wii-GT expected — song_select uses sv4), `->1` reload on return. **No monotonic growth / no leak.** Census `animating=0` at both hub visits (freeze survives the reload path — consistent with the char-layer root cause). |
| A10 prewarm rail (`RB3_PREWARM_SCREENS=1`) | **PASS** — boots to main_hub, prewarm adoption fires (main_hub -> song_select, 6 panels), no crash/assert; prewarm invariants intact |
| **Acceptance** (crowd census `animating>0` + 8 lit figures) | **NOT MET** — no faithful in-lane fix (panel-residency refuted; fix is char-layer). Root-caused + handed off. |
| near-black material discriminator | **moot/deferred** — `animating>0` never reached (same as W25/W26) |

## Files

- `src/system/ui/UIPanel.cpp` — CheckLoad/CheckUnload `mLoadRefs` probe (HX_NATIVE, byte-inert).
- `src/system/ui/UIScreen.cpp` — Load/UnloadPanels call-site probe.
- `src/system/ui/PanelDir.cpp` — Enter PanelDir/trigger probe.
- `src/band3/meta_band/BandScreen.cpp` — Load/UnloadInterstitials probe.
- `docs/.../W27-CROWD/{PLAN.md, STATUS.md, evidence/*}`.
