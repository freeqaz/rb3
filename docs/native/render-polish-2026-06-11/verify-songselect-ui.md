# verify-songselect-ui — independent adversarial review (wave-5)

**Verdict:** CONFIRM
**Reviewer:** opus (independent), 2026-06-15  ·  **Ports used:** 9321–9324
**Build reviewed:** composed master `c6e6048d` / engine pin `15ce606`
(`native/build-native/rb3-native`, built Jun 15 09:07; no source newer than the binary)
**Implementer doc reviewed:** `task-songselect-ui-impl.md` (commit `9096f309`, ex-`a9e39150`)

---

## SUMMARY

All three song-select clutter fixes CONFIRMED on the composed build with my own
evidence (not the implementer's). The wave-2 diff-grid icon-centering is intact,
the online mini-leaderboard show-path is correctly preserved (gated to
`IsReady() && HasRows()`), and the menu-contrast ambient-floor lowering composes
cleanly through the full menu → song-select → gameplay sweep with no new
crush-dark or blowout.

Evidence under `/tmp/rp5rev-songselect-ui/` (trimmed to 17M).

---

## FIX-BY-FIX (my own evidence)

### (1) Garbage header digits — CONFIRM
- **Screenshot:** `after/native_depth_{00,08,16,30}.png` + zoom `header_zoom_d08.png`
  — header reads exactly "VIEWING ALL 83 SONGS, SORTED BY SONG NAME" with NO
  trailing garbage digits at any depth.
- **Direct engine probe** (`/api/dta/eval` on my live instance, port 9323):
  `{music_library header_career_stars}` = **0** (was 1843121372 pre-fix);
  `{music_library header_possible_stars}` = **410** (legit, 82×5, untouched).
- **Before baseline** (implementer's `/tmp/rp5-songselect-ui/before/native_depth_08.png`,
  re-inspected by me): header clearly shows "...SONG NAME**1056964736**". Decisive
  A/B — the defect existed and is gone.
- Source confirmed present: `SongStatusMgr.cpp:462-475` zeroes
  `mCachedTotalScores/DiscScores/Stars[11]` under `#ifdef HX_NATIVE` in the ctor.

### (2) FRIEND RANKINGS overlay — CONFIRM
- **Screenshot:** none of my AFTER captures (depths 0/8/16/30) show a FRIEND
  RANKINGS panel; the difficulty grid renders clean (`diffgrid_zoom_d08.png`:
  "NO REVIEW" + per-instrument dot rows).
- **Before baseline:** the implementer's pre-fix `before/native_depth_08.png`
  clearly shows the "FRIEND RANKINGS" overlay obscuring the lower-right grid.
  Decisive A/B.
- **Online-correctness preserved (code review):** `SetMiniLeaderboardGroupShowing(true)`
  is reached ONLY inside `SongSelectPanel::Poll` under the guard
  `unk48 && unk48->IsReady() && unk48->HasRows()` (SongSelectPanel.cpp:140-150).
  Offline my probe shows `leaderboard.mld is_ready=0 has_rows=0`, so that path
  never fires. The hide-paths (`FinishLoad`, `Restart/CancelLeaderboardTimer`)
  re-hide. The fix is online-correct, not just offline-suppressing — the legit
  online case (online + rows ready) would still reveal the panel exactly when the
  Wii rotation timer would.

### (3) Grey album-art box — CONFIRM
- **Screenshot:** `albumart_zoom_d08.png` — a highlighted real song (The Beautiful
  People) shows the "ROCK BAND" blank-art placeholder, NOT a flat grey box.
- **Before baseline:** implementer's `before/native_depth_08.png` shows a flat
  light-grey box at that position. Decisive A/B.
- The depth-0 "?" placeholder (header/RANDOM-SONG node) is the correct authored
  no-art texture (`song_select_random_keep.png`), not a defect.
- Source confirmed: `BandSongMgr.cpp:357-362` `::stat()`-checks the resolved
  `_keep.png` and falls back to `ui/image/blank_album_art_keep.png` (HX_NATIVE).

---

## REGRESSION / INTERACTION SWEEP — interactionsOk = true

Full nav sweep on the composed build (all wave-4 + wave-5 brightness fixes):

- **Wave-2 diff-grid icon centering — intact.** `diffgrid_zoom_d08.png`: instrument
  icons vertically centered on their dot rows; grid no longer obscured by the
  overlay these defects used to cover.
- **Menu hub** (`sweep_hub.png`): venue art (BABOON NEST) vibrant, meanL 112,
  0.00% white-clip, 6.1% black — NOT crushed by the menu-contrast ambient-floor
  lowering. The floor-lower deepens shadows for contrast without flattening the
  hub to black.
- **Song select** (`after/*`): meanL ~80, 0.00% clip, ~8.5% black — readable,
  not crushed.
- **Gameplay, lit venue** (`gp/06_game_screen.png`, `07_playing.png`,
  burst series): boot→gameplay green, song playing (songMs 43330), 0 crashes/
  asserts. Highway centered/head-on, gems + sustain tails render, band standing
  & dressed, venue stage-lit. Burst brightness: white-clip 0.19-0.41% (well under
  the wave-4 4.3% healthy ceiling — no blowout); black% 14-46% on normal scenes,
  82% only on the dimmest authored stage-light phase where the highway/gems stay
  fully visible (not crushed). First game frame (06) is a fully-drawn lit venue
  with NO pink/red clear-color wash — the wave-5 first-frame-flash fix composes.

The three brightness-shifting fixes (venue soft-clip, menu fog, menu-contrast
floor) plus the song-select clutter fixes compose with no interaction regression.

---

## NOTES / RESIDUALS (none block CONFIRM)

- `/api/dta/eval`'s `{song_select_panel find live_lb.grp}` returns a NULL object
  (the leaderboard sub-tree isn't reachable via that eval path in the 360-ARK
  extract — matches the `FinishLoad` "absent from the extract" comment), so the
  `showing=0` it reports is a null-deref artifact, NOT a clean read of the group
  state. I did NOT rely on it; the screenshot A/B (overlay present pre-fix →
  absent post-fix) is the authoritative FRIEND-RANKINGS evidence. (`{exists ...}`
  on the null also tripped the known deferred `/api/dta/eval` Color/sub-property
  SIGSEGV — caught inside the handler, non-fatal, server kept running.)
- Wii byte-identity: all 5 edits are `#ifdef HX_NATIVE`; I did not re-run objdiff
  (implementer reports every touched fn 100% except Poll 98.84393 == master,
  consistent with HX_NATIVE gating). The native screenshots + commit being on
  master are sufficient for this visual review's scope.

---

## EVIDENCE INDEX (`/tmp/rp5rev-songselect-ui/`)

| Path | What |
|---|---|
| `after/native_depth_{00,08,16,30}.png` | my AFTER song-select captures, 4 depths |
| `header_zoom_d08.png` | header: "...SORTED BY SONG NAME" — no digits |
| `albumart_zoom_d08.png` | ROCK BAND blank placeholder (not grey box) |
| `diffgrid_zoom_d08.png` | clean diff grid, wave-2 icon centering intact |
| `sweep_hub.png` | main hub lit (not crushed) |
| `gp/06_game_screen.png`, `gp/07_playing.png` | lit-venue gameplay (no blowout/crush) |

(BEFORE baseline = implementer's `/tmp/rp5-songselect-ui/before/native_depth_08.png`,
re-inspected; shows all 3 defects.)
