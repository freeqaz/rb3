# W22-HUD — PLAN

Lane: gameplay score-HUD position + star-meter fill (Wave 22, hands CLOSED).
Charter: WAVE22_KICKOFF.md + A1–A9 (WAVE22_REVIEW.md). GT: Wii
`yt_qRagnZCIMzk_gameplay_{guitar,drums,drums_starpower}`.

## Ground truth (Wii, 1280×720)
- Score pill: **top-RIGHT** (right-justified number, x≈1100–1210).
- Star row: **top-RIGHT directly below the score pill**; 5 pips; they **DO fill**
  (guitar shot 4/5 filled, drums_starpower 5/5 filled).

## Bugs (2026-07-02 visual diff) — status after confirm
1. **Score HUD mid-screen** — CONFIRMED OPEN. Root-caused to K9
   (`RB3_APPLY_HANDLER_FIX_OFF`, `TrackPanelDir.cpp:294` `neutralizeAnchor`), a
   DEFAULT-ON workaround that zeroes `right.grp.x` for single-player to force the
   scoreboard to top-CENTER. K9's premise ("retail single-player = top-center")
   is WRONG — retail is top-RIGHT. K9 A/B (STEP 0, A4):
   - default (K9 ON): score-pill center x=578 (**45% width = mid-screen**)
   - `RB3_APPLY_HANDLER_FIX_OFF=1` (K9 off): x=1124 (**88% width = top-right**) ✓ matches retail
2. **Star meter never fills** — **FALSE ALARM / already-working.** `RB3_HUD_STAR_DBG`
   probe shows `SetNumStars` IS fed a rising value (showStars=1; numStarsF 0.190 @12s
   → 3.405 @90s), and at 3.4 stars the HUD renders **3 filled + 1 outlined** star =
   correct. The 2026-07-02 sighting caught the HUD early-song (pre-1.0 star = a single
   outlined star = the correct initial state). No fix needed; reported as not-a-bug.

## Fix (bug 1) — re-scope K9, NOT a new anchor constant (A4)
Applause-meter is hidden in single-player (`TrackPanel::ShowApplauseMeter()==false`),
so K9's `left.grp`/applause neutralization is visually inert — the only user-visible
effect of K9 is the scoreboard mis-centering. Fix = a new **default-OFF** flag
`RB3_HUD_SCOREBOARD_TOPRIGHT` (opt-in, HX_NATIVE, presence-read, getenv-cached) that
makes K9 **skip** the scoreboard-anchor neutralization so the scoreboard keeps its
authored top-right position (the raw V22 layout). Precedent: `RB3_HUB_TICKER_YFIX`
(`512a1bde`) — lane lands default-OFF, coordinator flips default-ON at close-out.

## Gates (A6)
- score-position E1: drawlog-bbox / screenshot-bbox — scoreboard center in the
  right-third/top-band vs the Wii guitar shot. ✓ (x=1124 → 88% width)
- star-fill oracle: drive past 1.0 stars → fill draws appear. ✓ (3.4 stars → 3 filled)
- flag-OFF drawlog-golden 792 byte-identical.
- `batch_objdiff == baseline` on touched `src/system/bandobj` units (Wii-matched).
- `rb3-tests` 116/0.
- No regression to the shipped HUD.

## Files
- `src/system/bandobj/TrackPanelDir.cpp` — re-scope K9 (HX_NATIVE, flag-gated).
- `src/band3/bandtrack/TrackPanel.cpp` — `RB3_HUD_STAR_DBG` diagnostic (env-gated, no behavior change).
- `scripts/native/hud-k9-ab-capture.py` — A/B + star-oracle harness.
