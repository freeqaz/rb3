# W22-HUD — STATUS

**Headline:** Bug (1) score-HUD-mid-screen = CONFIRMED OPEN, root-caused to K9
(`RB3_APPLY_HANDLER_FIX_OFF`) and **FIXED** via a re-scope (new default-OFF flag
`RB3_HUD_SCOREBOARD_TOPRIGHT`). Bug (2) star-meter-never-fills = **FALSE ALARM /
already-working** — the star meter fills correctly; the 2026-07-02 sighting caught
it early-song (pre-1.0 star). No pivot needed — bug (1) is a real, landed fix.

## K9 A/B result (STEP 0, A4) — the discriminator
| run | score-pill center x | % width | vs retail (top-right) |
|---|---|---|---|
| default (K9 ON) | 578 | 45% (MID-SCREEN) | WRONG — the bug |
| `RB3_APPLY_HANDLER_FIX_OFF=1` (K9 off) | 1124 | 88% (TOP-RIGHT) | ✓ matches |
| **FIX** (`RB3_HUD_SCOREBOARD_TOPRIGHT=1`, K9 still default-ON) | 1124 | 88% (TOP-RIGHT) | ✓ matches |

K9 is a DEFAULT-ON workaround (`TrackPanelDir.cpp` `ConfigureTracks`,
`neutralizeAnchor`) that zeroes the scoreboard's `right.grp.x` to force
top-CENTER, on the false premise "retail single-player = top-center." The Wii GT
(`yt_qRagnZCIMzk_gameplay_guitar/drums/drums_starpower`) is unambiguously
**top-RIGHT**. K9 was the cause.

## Fix — re-scope of K9 (A4), NOT a new anchor constant
New flag **`RB3_HUD_SCOREBOARD_TOPRIGHT`** (default-OFF, opt-in, presence-read,
getenv-cached, `#ifdef HX_NATIVE`) makes K9 **skip** the scoreboard-anchor
neutralization so the plate keeps its authored top-right position (raw V22). The
applause-meter/mtv anchors are left untouched — in single-player
`TrackPanel::ShowApplauseMeter()==false` hides `applause_meter.grp`, so their
neutralization is visually inert. **Coordinator flips the default at close-out**
(precedent `RB3_HUB_TICKER_YFIX` `512a1bde`).

## Bug (2) star-fill — NOT A BUG
`RB3_HUD_STAR_DBG` (env-gated diagnostic, HX_NATIVE) proves `SetNumStars` IS fed a
rising value during native gameplay: `showStars=1`, `numStarsF` 0.190 @12s →
3.405 @90s (score 17458). At 3.4 stars the HUD renders **3 filled + 1 outlined**
star = correct. The star meter fills. The 2026-07-02 diff observed "one outlined
star" because it captured early-song (pre-1.0 star = the correct initial state:
`ResetStars` hides stars 1-4). Reported as not-a-bug.

## Gate table
| gate | result |
|---|---|
| score-position E1 (drawlog/screenshot bbox vs Wii GT) | PASS — fix x=1124 (88% width, top-right) |
| star-fill oracle (drive past 1.0 stars) | PASS — 3.405 stars → 3 filled pips render |
| flag-OFF drawlog-golden 792 byte-identical | PASS — `if(!sScoreboardTopRight)` wraps the exact prior call; default OFF runs it unchanged |
| batch_objdiff == baseline (touched src/system unit) | PASS — `ConfigureTracks__13TrackPanelDirFb` 100.0% (692 instr all equal); edits inside `#ifdef HX_NATIVE`, not in Wii build |
| TrackPanel.cpp (band3, diagnostic only) | Poll 97.7% = pre-existing baseline (HX_NATIVE-only diag, invisible to Wii .o) |
| rb3-tests | PASS — 116 passed / 0 failed (7 skipped = absent real-capture fixtures; teardown SIGABRT is the known xobject.cc:52 issue, post-all-pass) |
| no regression to shipped HUD | PASS — highway/gems/energy-bar/venue unchanged in captures |

## Files changed (staged by path)
- `src/system/bandobj/TrackPanelDir.cpp` — re-scope K9 (`RB3_HUD_SCOREBOARD_TOPRIGHT`, default-OFF, HX_NATIVE).
- `src/band3/bandtrack/TrackPanel.cpp` — `RB3_HUD_STAR_DBG` diagnostic (env-gated, HX_NATIVE, no behavior change).
- `scripts/native/hud-k9-ab-capture.py` — K9 A/B + star-oracle capture harness.
- `docs/.../W22-HUD/{PLAN.md,STATUS.md,evidence/}`.

## Coordinator ask
Flip `RB3_HUD_SCOREBOARD_TOPRIGHT` default-ON at close-out (add an `_OFF` opt-out,
mirroring the HUB_TICKER_YFIX lifecycle) — this is the retail-correct HUD position.

---
## ERRATA (Wave-22 close-out review `6c972e88`, ERR-1, ERR-5)
- ERR-1: the "applause/mtv neutralization is visually inert" claim is STANDARD-PLAY-scoped —
  BotB `SetupApplauseMeter` (TrackPanelDir.cpp:515) CAN show `applause_meter.grp`; that path is
  unchanged by this fix and has no GT. Non-blocking watch item.
- ERR-5: the flag-OFF "byte-identical" gate was PASS-BY-CONSTRUCTION (code inspection of the
  wrapped call), not a measured drawlog run. The coordinator flip supplies the measured half:
  post-flip DEFAULT-run capture `evidence/05_FLIP_default_topright_score525.png` shows the
  score pill top-right (x≈88% width) with NO flag set, drawlog-golden 792 PASS (scoreboard
  reposition is in-gameplay, past the 60-frame boot golden window).
