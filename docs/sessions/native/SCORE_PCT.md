# SCORE_PCT — the center-screen `%d%%` gameplay label (V29)

**Date:** 2026-05-28 (Opus implementation subagent, V29).
**Goal:** fix the literal `%d%%` rendered dead-center during gameplay (a percentage
label whose `%d` int substitution never ran). SUCCESS = the label shows the
substituted number (e.g. `0%`), not the raw `%d%%`.

## Outcome (TL;DR)

The center label is **`solo_percent.lbl`** (the solo-overlay percentage readout that
lives in the `player_feedback` milo). After the fix it renders the substituted
**`0%`** instead of the raw `%d%%`. `0%` is the correct value for this headless,
no-note-hit `nofail` run.

Screenshots: `docs/sessions/native/screenshots/v29-score-pct/`
- `BEFORE_seedoff_HUDforced_f490.png` — raw `%d%%` dead-center (seed disabled, HUD forced visible).
- `AFTER_center_label_0pct_HUDforced.png` — same center position now shows `0%`.
- `SIDEBYSIDE_before_after.png` — the two side by side.
- `BEFORE_center_label_pctdpct.png` — independent V28 capture (band camera) also showing the raw `%d%%`.

## Which label it actually is (V14b was on the right object, wrong conclusion)

The literal text comes from the locale token **`solo_percent_fmt` → `"%d%%"`**
(`ui/locale/eng/locale_keep.dta:22900`). Several tokens resolve to `%d%%`
(`eg_percent_format`, `endgame_player_noteshit_fmt`, `me_percent_format`,
`solo_percent_fmt`); a runtime trace (`MILO_SETTOKEN_DBG=1`, the existing
`UILabel::SetTokenFmtImp` debug) pinned it to `solo_percent_fmt` on a label named
**`solo_percent.lbl`**, `showing=1`, with `da2sz=0` (no format args) →
`out="%d%%"`.

So V14b was correct that this is `solo_percent.lbl` — but **wrong** that it is
"correctly hidden because the test song has no solo." It is in fact **on-screen
during normal (non-solo) gameplay** whenever the gameplay HUD master group
(`draw_order.grp`) is visible.

## Root cause (file:line)

`solo_percent.lbl` bakes its default `text_token = solo_percent_fmt` into
`ui/track/gen/player_feedback.milo_xbox` (verified via `strings`). When the milo
loads, `UILabel`'s `text_token` SYNC_PROP path runs
`UILabel::SetTextToken(Symbol)` (`src/system/ui/UILabel.cpp:425`), which calls
`SetTokenFmtImp(token, 0, 0, 0, /*rawFmt=*/true)` with **no format args**. With no
args the `%d` is never substituted and the label displays the raw localized format
string `"%d%%"`.

In retail this is never seen because the **solo overlay is hidden** until
`solo_start.trig` animates it in — and `set_percent` (`player_feedback.dta:120-125`)
supplies the int via `set_token_fmt solo_percent_fmt $val` at that moment. The
overlay is hidden again by `reset.trig` / `hide_solo.anim`
(`BandTrack::SoloHide`, `BandTrack.cpp:630`).

Natively the overlay's reset/hide PropAnim path does **not** reliably push the
label off-screen (the same proxy/anim machinery the venue/camera work is still
bringing up). So when the HUD master group `draw_order.grp` is showing — which the
V25 SCORE_HUD work force-enables in `GamePanel::StartGame` (uncommitted working-tree
change) — the always-loaded `solo_percent.lbl` draws its un-substituted `%d%%`
center-screen.

(The substitution machinery itself was never broken: `me_percent_format` /
digit substitution renders fine, as V14b/V25 already proved. This was a
wiring/args gap on this one always-loaded label.)

## The fix (file:line)

`src/system/bandobj/BandTrack.cpp` — `BandTrack::Reset()` (additive `#ifdef HX_NATIVE`,
inside the existing `if (mPlayerFeedback)` block, right after
`mPlayerFeedback->HandleType(reset_msg)`):

```cpp
#ifdef HX_NATIVE
    if (UILabel *pctLabel =
            mPlayerFeedback->Find<UILabel>("solo_percent.lbl", false))
        pctLabel->SetTokenFmt(me_percent_format, 0);
#endif
```

`Reset()` runs on every gameplay (re)entry. Seeding a *substituted* `0%`
(`me_percent_format` → `"%d%%"` with int arg `0` → `"0%"`) guarantees the label
never holds the raw format string regardless of whether the hide-overlay anim ran.
This mirrors the existing V14b seed in `BandTrack::SoloStart()` (`~:446`) and the
live `SoloHit()` push (`~:738`), just on the always-run reset path.

`me_percent_format` and `solo_percent_fmt` both localize to `"%d%%"`, so the
displayed glyphs are identical; `me_percent_format` is reused for consistency with
the existing seeds.

## What renders now

- During gameplay the center label shows **`0%`** (substituted), not `%d%%`.
- Trace proof (`MILO_SETTOKEN_DBG=1`, full 24000-frame run): the raw
  `solo_percent_fmt → "%d%%"` renders only at **load** (5×, before frame 0 draws);
  the `me_percent_format → "0%"` seed then fires during gameplay (6×) and is the
  **last** value the label holds (final render at log line 41375 = `out="0%"`).
- Visual proof: with the HUD master group forced visible, the label that previously
  showed `%d%%` dead-center now shows `0%` (`SIDEBYSIDE_before_after.png`).

## Note on visibility / why this is the right layer to fix

The deeper issue is that `solo_percent.lbl`'s solo overlay isn't deterministically
hidden during non-solo play (anim machinery), and separately the HUD master group
`draw_order.grp` visibility races (the V25 SCORE_HUD finding — `draw_order.grp`
toggles off after `GamePanel::StartGame` force-shows it, so in many runs the whole
HUD, including the score digits AND this label, isn't drawn at all). Fixing the
*value* at `Reset()` is the correct, minimal, regression-safe fix for THIS task
(one label's value-substitution wiring): whether or not the overlay/HUD is visible,
the label can only ever show the substituted number, never the format string. The
overlay-hide-anim and `draw_order.grp` visibility race are pre-existing, separately
tracked rendering issues and were intentionally left untouched.

## Regression status

- Clean build (`cmake --build native/build-native -j`), no warnings introduced.
- Full reproducer (24000 frames, `MILO_AUDIO=1`, `nofail`, `track:guitar`): exit 0,
  no FATAL/SEGV/assert/crash; `Game::mLoadState = kReady` (audio loaded, gameplay
  loop runs) — boot / song-select / song-load unchanged.
- No change to highway / gems / venue / band / camera / score-plate / star-meter.
  The fix touches only `solo_percent.lbl`'s text.
- All temporary diagnostics (`MILO_SETTOKEN_DBG` name/showing addition in
  `UILabel.cpp`; `RB3_FORCE_PCT` / `RB3_DISABLE_PCT_SEED` harness gates) were
  reverted; `UILabel.cpp` is byte-identical to its committed state and the only
  remaining change is the `BandTrack::Reset()` seed.

## Files

- Fix: `src/system/bandobj/BandTrack.cpp` (`BandTrack::Reset()`, ~line 141).
- Data refs: `ui/track/gen/player_feedback.milo_xbox` (bakes the default text_token),
  `ui/track/player_feedback.dta:120-125` (`set_percent` → `set_token_fmt`),
  `ui/locale/eng/locale_keep.dta:22900` (`solo_percent_fmt → "%d%%"`).
