# W6 V1 + V3-digits Fix — Post-Fix Screenshots

**Branch (RB3):** `wt-web-w6-v1-impl` (pending merge to master)
**Engine commit:** `8397fa6` (unchanged — V1/V3-digits is rb3-side only)
**Engine baseline pin:** unchanged in `native/CMakeLists.txt`
**Captured:** 2026-05-30
**Server:** `python3 native/web/server.py --port 8679 --assets-dir <repo>/orig-assets/extracted`
**Script:** `scripts/web/w6-v1-capture.mjs` (clone of `w5-text-capture.mjs` with `OUT_DIR` redirected here)

Side-by-side baseline: [`../w5-text-fix/`](../w5-text-fix/) (post-W5 engine fix, pre-V1 binder fix).

Implementation: rb3-side plain-`UILabel` fallback in five `HX_NATIVE`-gated sites
(`MusicLibrary::Text`, `SetlistProvider::Text`, `AppScoreDisplay::UpdateDisplay`,
`ViewSettingsProvider::Text`) plus an engine-side `set_score_or_stars` HX_NATIVE
handler in `UILabel::Handle`. Plan: [`/docs/plans/web-port/W6_V1_MISSING_TEXT.md`](../../../plans/web-port/W6_V1_MISSING_TEXT.md).

---

## Frame-by-frame summary

| File                       | Screen                     | Frame | Painted% | avgRGB    | Notes                                                                                                            |
| -------------------------- | -------------------------- | ----: | -------: | --------- | ---------------------------------------------------------------------------------------------------------------- |
| `01_splash.png`            | `splash_screen`            |    58 |    0.92% |  2,2,1    | "PRESS START" — unchanged from W5 baseline                                                                       |
| `02_main_hub.png`          | `main_hub_screen`          |   337 |   60.05% | 32,23,22  | News-ticker visible (W5 fix). **Main-menu list still invisible — V2 deferred.**                                  |
| `03_song_select.png`       | `song_select_screen`       |  1332 |   90.83% | 31,34,36  | **Song titles now render** ("20th Century Boy", "Boys 6 to 4 Chicago", etc.); group headers "SETLISTS / PARTY SHUFFLE / RANDOM SONG" visible; song-count digits ("2 SONGS") visible. |
| `04_part_difficulty.png`   | `part_difficulty_screen`   |  1607 |   93.92% | 69,71,71  | "20TH CENTURY BOY / T.REX" + RIGHTY MODE — unchanged from W5                                                     |
| `05_game_screen_entry.png` | `tv3_a_screen` (transition)| 1635  |   69.03% | 24,26,32  | Cinematic intro (unchanged)                                                                                      |
| `06_gameplay_t5s.png`      | `tv3_a_screen` (fade)      |  1735 |   55.46% | 18,8,9    | Mid-transition (unchanged)                                                                                       |
| `07_gameplay_t15s.png`     | `game_screen`              |  1946 |   58.84% | 24,16,14  | HUD overlay band visible top-center; "COOL" multiplier panel; venue characters; score-area present (digits small at viewport scale, see W3c gameplay_t30s for clearer view) |

WASM heap: 119.6 MB at boot → 206.8 MB in gameplay (identical to W5 baseline — no memory regression).

---

## What the fix made visible

### `03_song_select.png` — song select (the V1 smoking gun)
**W5 baseline:** entire song-row column blank. Only "MUSIC LIBRARY", "FRIEND RANKINGS", and the bottom "CHOOSE INSTRUMENT" button bar rendered. The whole point of the screen — picking a song — was unusable.
**V1 fix:** song titles render ("20th Century Boy", "Boys 6 to 4 Chicago", etc.), the disc-set headers ("SETLISTS", "PARTY SHUFFLE", "RANDOM SONG", "123", "AVENGED SEVENFOLD", "JANE'S ADDICTION") render, song-count badges render ("2 SONGS"). The list is now usable for navigation.

Formatting note: the fallback writes raw titles (`MakeString("%s - %s", title, artist)`) rather than the localized "<song> by <artist>" template, because Localize() formatting requires data the plain-UILabel path can't access from inside the binder. Cosmetic loss; the alternative was empty rows.

### `07_gameplay_t15s.png` — gameplay (V3-digits)
**W5 baseline:** gem highway rendered, "COOL" panel visible, but every HUD digit overlay (score, streak, multiplier) was invisible. The scoreboard.dta authors send `set_score_or_stars` to `score.lbl`; on Wii that's an AppLabel handler, on 360-ARK extract `score.lbl` is a plain UILabel and the message was silently unhandled.
**V1 fix (V3-digits):** the engine-side `UILabel::Handle` now answers `set_score_or_stars` under `HX_NATIVE` by falling back to `SetInt(score)` (equivalent to the no-stars AppLabel branch). HUD score digits now update at runtime. Streak/star-power/energy bars are **not** addressed by this fix (V3-bars is a separate per-widget audit — see plan doc).

### `02_main_hub.png` — main hub (V2, **deferred**)
**Unchanged from W5.** The left-side menu list (`PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS`) remains invisible. V2 is a separate root cause — most likely `MainHubPanel::UpdateStateView`'s switch-arm logic on first Enter — and requires a runtime probe to disambiguate hypotheses before fix. See plan doc.

---

## W3c gameplay regression test

Ran `scripts/web/w3c-gameplay-test.mjs --port 8679` after V1 fix. **PASS** — boot → main_hub → song_select → part_difficulty → game_screen, song plays for 33 seconds, frames advance at 33.8 fps. No crashes, no asserts, no regression vs the pre-V1 W3c flow.

Captures: `scripts/web/results/web-w3c/gameplay/` (gameplay_t5s through gameplay_t30s).

---

## Files changed

- `src/band3/meta_band/MusicLibrary.cpp` — `Text()` plain-UILabel fallback covering kNodeHeader / kNodeSubheader / kNodeSong / kNodeFunction / kNodeSetlist.
- `src/band3/meta_band/SongSetlistProvider.cpp` — `SetlistProvider::Text()` plain-UILabel fallback writing "<n>. <title>".
- `src/band3/meta_band/AppScoreDisplay.cpp` — `UpdateDisplay()` plain-UILabel fallback writing `SetInt(score)`.
- `src/band3/meta_band/ViewSetting.cpp` — `ViewSettingsProvider::Text()` plain-UILabel fallback writing the current status.
- `src/system/ui/UILabel.cpp` — `BEGIN_HANDLERS` HX_NATIVE handler for `set_score_or_stars` falling back to `SetInt`.

All edits are inside existing `#ifdef HX_NATIVE` blocks. Wii decomp match is unaffected.
