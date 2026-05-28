# SCORE_HUD — Always-on gameplay scoring HUD (G_SCORE)

**Date:** 2026-05-28 (Opus implementation subagent, V25).
**Goal:** make the always-on gameplay scoring HUD render like retail — the running
**score digits**, the **star-progress meter**, and the **note-streak multiplier**
(x1/x2/x3/x4). The note highway, gems, strike plate, venue, and band already render;
the score/star elements did not.

## Outcome (TL;DR)

The **score digits and the star-progress meter now render** during gameplay, in the
BandScoreboard widget (a rounded plate showing the score digit(s) plus the star meter
disc below it). The score correctly reads **0** in this headless no-hit `nofail` run.
The fix was a **visibility/timing** fix, not a wiring fix — the score data path was
already correct.

Screenshots: `docs/sessions/native/screenshots/v25-score-hud/`
- `07_f1100.png`, `10_f1400.png` — full gameplay frames with the score widget top-right.
- `zoom_score_widget_f1400.png` — zoomed crop: the scoreboard plate with the `0` digit
  and the star-meter disc.
- `01_f0600.png`..`05_f0920.png` — the cinematic song-intro window (HUD correctly OFF,
  per retail — see below).

## Root cause (file:line)

The roadmap's leading hypothesis — that `TrackPanel::TrackerDisplayReset` is a weak
no-op stub in `native/src/band3_link_stubs.s:157` shadowing the real impl at
`src/band3/bandtrack/TrackPanel.cpp:526` — is a **red herring**. Verified via `nm`:

```
W _ZN10TrackPanel19TrackerDisplayResetEv     <- weak stub (NON-const overload)
T _ZNK10TrackPanel19TrackerDisplayResetEv    <- real impl (CONST, strong, links)
```

The weak stub is for a *non-const* `TrackerDisplayReset()` overload that **no caller
invokes** (declared at `TrackPanel.h:72`, never defined); the real **const**
`TrackerDisplayReset() const` (`TrackPanel.h:87`, def `TrackPanel.cpp:526`) is a
distinct mangled symbol and already strongly linked. The stub does not shadow anything.
`band3_link_stubs.s` was left untouched.

**Actual root cause — the HUD master draw group was hidden during early play.**
Runtime diagnostics (K8_DBG) proved the score data path is fully wired and the
scoreboard object exists/draws:

- `TrackPanel::Poll()` runs every gameplay frame (`gate=0`, `mScoreboard != null`,
  `mTracks.size=1`), feeding `mScoreboard->SetScore(GetAccumulatedScore())` and
  `SetNumStars(...)` (`TrackPanel.cpp:606-618`) and `dir->SetMultiplier(...)`
  (`TrackPanel.cpp:623`).
- `BandScoreboard` renders digits by mesh-geometry swap, not format-string substitution
  (`BandScoreboard::SetScore`, `src/system/bandobj/BandScoreboard.cpp:15`): `num0.mesh`
  showing=1, geometry = `0_source.mesh`. Confirmed showing=1 at runtime.
- The scoreboard subgroup `scoreboard.grp` was showing=1.

…but the **master HUD group `draw_order.grp` was showing=0** during the first seconds of
play. `draw_order.grp` is the only thing the per-frame draw path walks for the HUD; it
is toggled solely by `TrackPanelDirBase::SetShowing(bool)`
(`src/system/bandobj/TrackPanelDirBase.cpp:198` → `Find<RndGroup>("draw_order.grp")->SetShowing(b)`).

The gameplay-entry sequence is:
1. `GamePanel::Enter()` (`src/band3/game/GamePanel.cpp:237`) → `Game::Reset` →
   `TrackPanelDir::Reset()` which **ends with `SetShowing(false)`**
   (`src/system/bandobj/TrackPanelDir.cpp:520`) — HUD intentionally hidden for the intro.
2. The HUD is meant to be turned back **on** by the milo-driven `play_intro` message
   → `TrackPanelDirBase`/`TrackPanelDir::PlayIntro()` → `SetShowing(!mPerformanceMode)`
   (`TrackPanelDir.cpp:555`; handler at `TrackPanelDirBase.cpp:271`) at the end of the
   cinematic song-intro.

In the native port that `play_intro` message fires **inconsistently / late** (its intro
camshot/anim timeline is the same proxy/anim machinery the venue work is still bringing
up). So `draw_order.grp` could stay hidden for the first seconds of play, and in some
runs never come on. (Why the highway/gems still draw: they render through the per-track
`TrackDir::DrawShowing` path, not through `draw_order.grp` — so they were unaffected.)

## The fix (file:line)

`src/band3/game/GamePanel.cpp` — `GamePanel::StartGame()` (additive `#ifdef HX_NATIVE`):
after the song actually starts playing (`mGameState = kGamePlaying`), deterministically
show the HUD master group:

```cpp
#ifdef HX_NATIVE
    if (TrackPanelDirBase *tpd = GetTrackPanelDir())
        tpd->SetShowing(true);
#endif
```

`StartGame()` is invoked from `GamePanel::Poll()` (`:320`) the moment the song clock
crosses 0 — exactly the retail "song begins → HUD appears" point. This mirrors the state
`PlayIntro()`'s `SetShowing(!mPerformanceMode)` sets and is **idempotent** if the milo
`play_intro` message also fires later. It does NOT force the HUD on during the cinematic
intro (frames before song-start stay HUD-off, matching retail).

Verified after the fix: `draw_order.grp` flips to show=1 at song-start (~frame 1000 in
the reproducer) and stays on for the rest of the song; the score plate + star disc render
in every post-intro frame sampled (f1000..f1500).

## What renders now

| HUD element | State | Notes |
|---|---|---|
| **Score digits** | **RENDERS** | BandScoreboard plate, reads `0` (correct — `nofail`, no note-hits in headless). Geometry-swap digit meshes draw. |
| **Star-progress meter** | **RENDERS** | The star-meter disc below the score plate; 0 stars filled (correct for a no-hit run). |
| **Band-score multiplier popup** (`star_meter_multiplier`) | hidden (correct) | `TrackPanelDir::SetMultiplier` only shows it when `mult > 1 && GetNumPlayers() > 1` (`TrackPanelDir.cpp:565`). Single-player at x1 → correctly hidden; this is retail behavior, not a gap. |
| **Per-track streak meter** (x1..x4 at strike plate) | base x1 state | Driven by note-hits (`BandTrack::SetStreak`, `BandTrack.cpp:317`); no hits in headless → stays at x1 base. Renders with the highway/strike-plate track path. |

## What remains

1. **HUD position vs retail.** The scoreboard renders **top-right** in the native port;
   retail RB3 places the band scoreboard **top-center**. This is the same
   camera-framing / screen-rect issue documented in the venue/camera work
   (`VENUE_RENDER.md`, the `CAMERA_FRAME_FIX` HX_NATIVE block at
   `TrackPanelDir.cpp:278-309`): the milo `N_player_<aspect>` configuration object's
   `apply` handler script doesn't fully execute natively, so HUD/track layout keeps the
   authored multi-player default offset. **Out of scope here (regression bar:
   don't disturb venue/camera).** Fixing the config `apply` handler will recenter both
   the highway and the HUD.
2. **`play_intro` reliability.** The proper milo-driven `play_intro` → `PlayIntro()` is
   still racy/late in the native port (root in the intro camshot/anim proxy path, owned
   by the venue work). The `StartGame()` force-show makes the HUD deterministic
   regardless, but the underlying anim-timeline gap is a venue/camera follow-up.
3. **Live non-zero score** requires real note-hit input; the headless `nofail` player
   never hits, so the displayed score legitimately stays 0. To see the digits tick up,
   a synthetic note-hit input verb (cf. G_FX) is needed.

## Shared-file state left

- **`native/src/band3_link_stubs.s` — NOT TOUCHED by this work.** (The diff present in
  the tree is another agent's `BandPatchMesh` removal, "Increment A 2026-05-28", a
  venue/character bring-up — unrelated.) The `TrackerDisplayReset` weak stub at
  `:157-158` was deliberately **left in place** (it is a dead non-const overload, not a
  shadow of the real impl — removing it is a no-op).
- **`src/band3/game/GamePanel.cpp`** — additive `#ifdef HX_NATIVE` block in
  `StartGame()` (the production fix). Matched-fork; `#else`/non-native path unchanged.
- **`src/band3/bandtrack/TrackPanel.cpp`** — additive `#ifdef HX_NATIVE` K8_DBG-gated
  diagnostic in `Poll()` (scoreboard / draw_order.grp / num-mesh showing-state +
  world-pos dump). Default no-op (gated on `K8_DBG` env). Extends the pre-existing K8
  block. Includes guarded `rndobj/Group.h`/`Mesh.h`.
- **`src/system/bandobj/TrackPanelDirBase.cpp`** — additive `#ifdef HX_NATIVE`
  K8_DBG-gated `MILO_LOG` trace in `SetShowing()` (logs every call's bool + group ptr +
  perfMode). Default no-op. Guarded `<cstdlib>`/`os/Debug.h` includes.

All three matched-fork edits are additive HX_NATIVE; re-read + re-apply if the permuter
shifts them. No `Co-Authored-By`. Not committed/staged.

## Reproducer

```
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=1600 \
  MILO_SCREENSHOT_DIR=/home/free/code/milohax/rb3/docs/sessions/native/screenshots/v25-score-hud \
  MILO_SCREENSHOT_FRAMES="600,680,760,840,920,1000,1100,1200,1300,1400,1500" \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
  /home/free/code/milohax/rb3/native/build-native/rb3-native
```

Add `K8_DBG=1` to dump the scoreboard / `draw_order.grp` showing-state diagnostics.
KNOWN FLAKE: the fixed-frame input occasionally races early asset load and the run
crashes/never leaves the menu (SIGSEGV during App construction, or every frame draws 0
meshes) — re-run until a gameplay run (non-zero mesh frames, K8 `SB showing` lines).
Gameplay HUD appears ~frame 1000 (song-clock start); intro frames before that are
correctly HUD-off.
