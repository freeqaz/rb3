# W7 — Main-hub cursor + gameplay HUD bars investigation

> **STATUS: INVESTIGATION ONLY — no code shipped.** The user-supplied hypothesis (same root cause as W6-V1: silent `dynamic_cast<App*>` HX_NATIVE early-returns) did NOT pan out. Verified via comprehensive grep + visual re-analysis of the post-V2 screenshots. Real root causes are different mechanisms; documented here for handoff.

**Parent plan:** [`PLAN.md`](PLAN.md) → [`W6_VISUAL_POLISH.md`](W6_VISUAL_POLISH.md).
**Audit anchor:** rb3 master `cb0bb31e`. Engine main `8397fa6`. W6 V1/V2/V3-digits already landed.
**Capture:** `docs/sessions/web/screenshots/w7-cursor-hud/` (re-built worktree at SHA `cb0bb31e` confirms same post-V2 visual state).

---

## Symptom recap

### 1. Main-hub "cursor invisible"

**Re-analysis of `02_main_hub.png` (zoomed): the cursor IS visible.** PLAY NOW renders at the top with a green/yellow highlight box around it (focused-state color from the milo's UIColor table). The other 4 items (CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS) render in dim silver/white (unfocused color).

**The actual problem is sibling vertical spacing** — CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS overlap each other (4 lines crammed into ~2 line-heights of vertical space). PLAY NOW is at the top correctly; the unfocused 4 are stacked. See `/tmp` scratch zoom output (or re-zoom `docs/sessions/web/screenshots/w7-cursor-hud/02_main_hub.png` to confirm).

This is NOT a W6-V1-class silent-cast bug. The labels are populated (text_token PROPSYNC writes their text at milo load time, no per-frame binder). The unfocused-state color IS being applied (silver/white). What's missing is the per-button **positioning** that the milo's per-item reveal anim is supposed to drive into final stagger pose.

### 2. Gameplay HUD missing streak / SP / energy bars

**Re-analysis of `07_gameplay_t15s.png` (zoomed): the meter geometry IS present, just empty.**

- Top-center horizontal score bar: a small "0" digit renders (BandScoreboard's `mNumMeshes[0]` showing when `mScore == 0`). The bar backdrop is there. The streak counter, ×multiplier, and 5 multiplier-dot icons are absent.
- Left-side vertical meter: the **empty silver bar geometry** for the StreakMeter / OverdriveMeter is present at the correct screen position. The colored fill (green energy fill in the retail reference) is missing — meaning the fill PropAnims (`extend_anim.grp::SetFrame(energy_pct, 1.0f)` in `OverdriveMeter::SetEnergy`, `meter_wipe.anim` in StreakMeter) likely aren't being driven, OR the fill mesh material is rendering with zero alpha.

The HUD digits (BandScoreboard `mNumMeshes[1..7]` via `mesh->SetGeomOwner(mSrcMeshes[digit])`) only render once `score > 0`. The capture is at t=15s with no actual gem hits (the auto-hit watcher may not be wired for the BandScoreboard input path on the web build, OR the score is genuinely staying at 0 because no gems are being struck).

---

## What I verified (the W6-V1 hypothesis test)

**Comprehensive sweep for "silent `dynamic_cast<X*>` HX_NATIVE early-return" pattern across `src/`:**

Python regex sweep of every `#ifdef HX_NATIVE ... #endif` block in `src/`, looking for blocks that contain `if (!varname) return;` (or `return false;`/`return X;`) where `varname` was just assigned from a `dynamic_cast<T*>`:

| Site | Cast type | Verdict |
| --- | --- | --- |
| `src/system/bandobj/TrackPanelDir.cpp:309` | `RndTransformable*` | benign — inside a lambda helper; the `return` bails out of the lambda only (the V31 `neutralizeAnchor` helper). Not a silent UI drop. |
| `src/system/rndobj/Draw.cpp` (MenuVoidDrawHook) | `RndMesh*` | benign — helper return value. Not a silent UI drop. |

**Plus the 4 already-patched W6-V1 sites:** `MusicLibrary.cpp:1049`, `SongSetlistProvider.cpp:18`, `AppScoreDisplay.cpp:8`, `ViewSetting.cpp:386` (+ engine-side `UILabel.cpp:944` set_score_or_stars handler).

**Result: zero new W6-V1-pattern sites.** The W6-V1 sweep was complete. The 24 other `dynamic_cast<AppLabel*>` sites in `src/band3/` use the unguarded `MILO_ASSERT(cast, line); cast->Method();` form — if the cast failed on native, `MILO_ASSERT` would log via `Debug::Fail` (which on `HX_WEB` is non-fatal — see `src/system/os/Debug.cpp:121`) and then `cast->Method()` would null-deref + crash. Since the web build does NOT crash on the affected screens, the cast is succeeding for those 24 sites. They are NOT broken.

---

## Why the cursor + HUD-bars are NOT a W6-V1-class miss

| Failure | W6-V1 class? | Why not |
| --- | --- | --- |
| Cursor invisible | NO | Visible on inspection — PLAY NOW has the focus-color highlight. Issue is sibling label *positioning*, not focus visibility. |
| Cursor positioning (siblings stacked) | NO | Hub menu buttons are static milo objects positioned by the milo's PropAnim reveal/stagger. Not a dynamic_cast path. |
| HUD streak/SP/energy fill missing | NO | Meters are RndDir + RndPropAnim + EventTrigger driven. No AppLabel cast. Likely (a) `SetShowing(true)` gated on `HasPlayer()` is failing, or (b) the fill anims aren't ticking, or (c) the score genuinely stays at 0 so nothing updates. |
| HUD score digits absent | NO (already addressed) | The W6 V3-digits engine fix (`set_score_or_stars` handler in `UILabel::Handle`) handles plain-UILabel score widgets on song_select. The in-game scoreboard uses **mesh-based** digits (BandScoreboard with `mesh->SetGeomOwner(mSrcMeshes[digit])`), not UILabel. Different mechanism. |

---

## Probe ideas for the actual root causes (handoff to W7-real)

### Cursor sibling-stacking

1. Probe `MainHubPanel::Enter()` + first 30 frames of `Poll()` to log every PropAnim currently registered on each `mb_*.btn` (transform, alpha, frame). Compare to expected reveal-anim end-pose vs current pose.
2. Compare the post-V2 PropAnim driving the buttons' x/z positions vs the milo's authored per-button rest pose. If the reveal anim ends at frame 80 but the buttons are reading frame 0 transforms, AnimTask might be re-targeting the same anim object across multiple buttons (one anim driving all 5 buttons; only the last one applied "wins").
3. Look for any second AnimTask in `none_to_main.trg` that's supposed to stagger the buttons. The V2 probe (`docs/sessions/web/screenshots/w6-v2-probe/v2-dbg-init.log`) showed ONE anim: `01_main_button_reveal.anim`. If retail used more, those didn't transcribe.

### HUD bar fill

1. Add `getenv("HUD_DBG")` probe to `StreakMeter::SetMultiplier` / `OverdriveMeter::SetEnergy` to log whether they're being called during gameplay. Also `BandTrack::ResetStreakMeter` / `Reset()` to confirm `mParent->HasPlayer()` returns true.
2. Add probe to `BandScoreboard::SetScore` to log the score values being pushed. If always 0, the gameplay engine isn't actually scoring (separate issue, possibly auto-hit watcher gap).
3. Investigate the `extend_anim.grp` / `meter_wipe.anim` PropAnims: are they being added to AnimTask polling, and is the fill mesh material set to render with non-zero alpha?

### HUD top-strip (multiplier + dots + streak counter)

1. The 5 multiplier-dot icons are individual mesh elements `multi-meter_anim.tnm`. Check if `mMultiMeterAnim->SetFrame(...)` is being called.
2. `mMultiplierLabel` (`multiplier.lbl` BandLabel) is set via `UpdateMultiplierText`. Confirm whether `MultiplierChanged()` is firing on band multiplier transitions during gameplay.

---

## Recommendation

**Do NOT dispatch a W7 fix on this pass.** The W6-V1 family is exhausted (4 broken sites all patched). The remaining cursor + HUD-bar issues require:

- Engine-side probes (or rb3 HUD-side probes) to pin which mechanism is failing.
- Not a uniform "apply HX_NATIVE fallback" pattern — each meter / button-stagger is its own targeted fix.

The user-supplied hypothesis ("same root cause family as W6-V1, more sites with different cast types") is FALSE based on the grep sweep. There are no additional broken-cast sites.

**Next concrete step:** start an instrumentation-only probe (mirroring the W6-V2 probe approach: env-gated `MILO_LOG` in `BandTrack::Reset` + `BandScoreboard::SetScore` + `StreakMeter::SetMultiplier`) to confirm whether the meters are getting their refresh calls or not. THEN decide if the fix is rb3-side (HasPlayer gate / score path) or engine-side (PropAnim ticking / mesh material).

---

## Files read during investigation

- `docs/plans/web-port/W6_V1_MISSING_TEXT.md`, `W6_VISUAL_POLISH.md`
- `docs/sessions/web/screenshots/post-v2-sweep/README.md`
- `src/system/ui/UIComponent.{cpp,h}`, `PanelDir.{cpp,h}`, `UIPanel.cpp`, `UIButton.cpp`, `UILabel.cpp`, `UILabelDir.cpp`, `UIList.cpp`, `UIListDir.cpp`, `UIListHighlight.cpp`, `UIListWidget.cpp`
- `src/system/bandobj/BandScoreboard.cpp`, `BandTrack.cpp`, `StreakMeter.cpp`, `OverdriveMeter.cpp`, `TrackPanelDir.cpp`, `TrackPanelDirBase.cpp`
- `src/band3/bandtrack/TrackPanel.cpp`, `Track.cpp`
- `src/band3/meta_band/MainHubPanel.{cpp,h}`, every `dynamic_cast<App*>` site in `src/band3/`
- `src/system/rndobj/Anim.cpp` (AnimTask::Poll — confirmed V2 fix is in place)
- `src/system/os/Debug.{cpp,h}` (HX_WEB fail-is-nonfatal semantics)
- `orig-assets/extracted/ui/main/main_hub.dta` (hub state machine + per-state triggers)
- `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`, `yt_qRagnZCIMzk_gameplay_guitar.png`

---

## Risk to decomp match

**ZERO.** No source changes shipped.
