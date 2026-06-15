# Verify: score-detail / star-breakdown screen (wave-5) — INDEPENDENT REVIEW

**Verdict: CONFIRM_WITH_RESIDUALS**

Independent adversarial review of the wave-5 `score-detail` fix on the composed
master build (rb3 `c6e6048d`, engine pin `15ce606`,
`native/build-native/rb3-native` built 2026-06-15 09:07). The blocker the task set
out to remove — the endgame popup never auto-advancing to the score/star breakdown
— is genuinely fixed for BOTH guitar and vocals, the Wii match claim
(`PassiveMessageQueue::Poll` 98.8%→100%) is objectively confirmed at the asm level,
and no brightness/interaction regression from the composed wave-4/5 fixes was seen.
The "residuals" are pre-documented, out-of-scope follow-ups (zero score values
under autohit; an over-press `load_nextsong` SIGSEGV), not defects in this fix.

Evidence under `/tmp/rp5rev-score-detail/`.

## What the fix is (verified against landed source)

- `src/band3/meta_band/PassiveMessenger.cpp:32-33` — `PassiveMessageQueue::Poll`:
  `bool running = mTimer.Running(); if (!running && ...)`. Landed in `f04b9f99`,
  matching the doc's claimed `running &&` → `!running &&` flip.
- `src/band3/meta_band/PassiveMessenger.h:50-52` — `PassiveMessage::AddAnim`'s
  `MILO_ASSERT(mMeterAnimValue >= 0, 0x4A)` is wrapped in `#ifndef HX_NATIVE`,
  exactly as claimed.

## Claim 1 — Wii match improvement (Poll 98.8%→100%): CONFIRMED (two ways)

- `build/SZBE69_B8/report.json` (regenerated 09:07:47, AFTER the `f04b9f99` land at
  09:07:21): `Poll__19PassiveMessageQueueFv` = **100.000%**;
  `GetAndPreProcessFirstMessage__19PassiveMessageQueueFv` = **98.940%** (unchanged —
  this is where `AddAnim` inlines, so its stability proves the `#ifndef HX_NATIVE`
  gate is Wii-neutral); `HasMessages` = 100.000%; `Poll__16PassiveMessengerFv` =
  100.000%; `AddMessage` = 100.000%.
- `objdiff-cli diff` on the live composed-build object: `fuzzy/normalized/raw =
  100.0`, `diff_score.score = 0` (of 21600), `target_size == base_size == 864`.
  The flip is byte-faithful to Bank 8. The match-IMPROVING exception is legitimate.

## Claim 2 — popup auto-advances to the score-detail screen (no input): CONFIRMED

Independent probe on the COMPOSED MASTER binary (`/tmp/rp5rev-score-detail/rev_probe.py`,
points at the main-repo binary, NOT the implementer's worktree), full song →
`{game jump 600000}` → game-over → **no confirm input**:

| track | screen chain (auto-advance) | passive_has_messages | stable | crash |
|---|---|---|---|---|
| guitar (`9342`) | `game_screen` → `coop_endgame_popups_screen` (f2660) → `coop_endgame_screen` (f3291) | **0** (drained) | 15s / 1787 frames | none |
| vocals (`9343`) | `game_screen` → `coop_endgame_popups_screen` (f4251) → `coop_endgame_screen` (f4773) | **0** | 15s / 1642 frames | none |

The hand-off happens with NO manual input — exactly the behavior the inverted
timer test used to block (the implementer's BEFORE: stuck 90s/10k frames,
`passive_has_messages=1` forever). `passive_has_messages` flipping to 0 is the
direct fingerprint of the drain now working.

Also reproduced with the OFFICIAL gate
`scripts/native/song-end-test.py --port 9341 --require-endgame`: **PASS** — it
observed the popup advance to `coop_endgame_screen` and hold STABLE 25s / 2885
frames advanced, no abort. (This gate previously only ever saw
`coop_endgame_popups_screen`.)

## Claim 3 — score-detail screen RENDERS (stars / per-player scores): CONFIRMED

Own screenshots (NOT the implementer's), captured on the composed master build:
- `/tmp/rp5rev-score-detail/guitar_score_detail.png` — per-player widget renders:
  "20TH CENTURY BOY" title, the 5-star row, "0% / EXPERT", "SOLO SCORE / GO /
  Previous Best", CONTINUE / RESTART, CONNECT-CONTROLLER slots.
- `/tmp/rp5rev-score-detail/vocals_score_detail.png` — same per-player widget,
  vocals layout. No null labels, no missing widget, no cast crash.
- `num_stars=0 score=0` are the documented autohit-harness artifact (nofail+autohit
  yields a 0 score); the WIDGETS/labels populate + render correctly. Engine logs
  (`engine-9342.log` / `engine-9343.log`) are clean of `SIGABRT`/`SIGSEGV`/
  `mMeterAnimValue`/`OSFatal`/`Error:`/`assert` — the Layer-2 meter assert no longer
  fires (the `#ifndef HX_NATIVE` gate works). The only NOTIFY noise is benign
  `unhandled msg: clear_pics` (the now-correctly-draining Poll's empty-queue branch).

## Interaction sweep (menu-contrast floor lowering — ALL venues): NO REGRESSION

`scripts/native/keyboard-to-gameplay.py --port 9344 --diff hard --game-burst 8`:
boot → main_hub → song_select → part_difficulty → gameplay PASS, song playing,
no crash. Brightness analysis of the captures (`/tmp/rp5rev-score-detail/sweep/`):

- `01_song_select` mean=83.4, white%=0.0, black%=0.3 — clean, readable; header
  reads "VIEWING ALL 83 SONGS, SORTED BY SONG NAME" with NO garbage digits
  (songselect-ui fix intact), album-art placeholder present.
- `02/03/04 part_difficulty` mean 85-92, white%=0.0 — menus not crushed, not blown.
- `06_game_screen`/`burst_*` gameplay mean 27-35, white% 0.3-0.4 (no blowout); the
  elevated black% (22-39%) is the genuinely dark TV3 club venue surround — the note
  highway + colored gems + band chars + venue backdrop all render lit (verified
  visually in `burst_06.png`). The menu-contrast floor lowering did NOT crush the
  gameplay venue.

`interactionsOk = true`.

## Residuals (pre-documented, out of scope — NOT defects of this fix)

1. **Zero score values under autohit** — harness artifact; the widgets render, only
   the numbers are 0. To validate non-zero numbers a real-note-hit harness is needed.
2. **Over-press `load_nextsong_screen` SIGSEGV** — pressing Confirm on the score
   screen in quickplay (no next song) null-derefs in the next-song-load path. This is
   NOT score-detail rendering; my auto-advance probes never press Confirm and never
   hit it. Worth a dedicated "continue from score screen" look (wave-6).
3. **Endgame crowd/venue greenish-dark tint** in the score-detail backdrop — same
   class as the crowd-render + venue-light residuals (wave-5 backlog: endgame tint
   adjudicated "authored disco color-wheel, not a bug").

## Bottom line

The success criterion — the score/star-breakdown screen is now REACHABLE +
auto-advances + RENDERS for guitar and vocals with no abort/crash — is met and
independently reproduced on the composed master build. The Wii match claim is
objectively true (objdiff 100.0, diff_score 0). No new issues; no regression from
the other wave-4/5 brightness fixes. Verdict **CONFIRM_WITH_RESIDUALS** (the
residuals are the documented wave-6 follow-ups, not problems with this change).
