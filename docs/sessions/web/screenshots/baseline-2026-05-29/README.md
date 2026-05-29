# W4 Baseline Screenshots — 2026-05-29

**Commit:** `6cfb0a7d` (master HEAD — W3c complete, one song e2e in browser)
**Engine pin:** `5fda7f0`
**Captured:** 2026-05-29T22:36:26Z
**Build:** emcc 5.0.2, default flags (debug `-O0 -g2`)
**Server:** `native/web/server.py --port 8430 --assets-dir orig-assets/extracted`
**Script:** `scripts/web/w4-baseline-capture.mjs`

These replace the stale `docs/sessions/native/screenshots/song-load-2026-05-27/` shots,
which pre-date the tv3 vignette and CamShotFrame::Interp fixes.

---

## Frame-by-frame summary

| File | Screen | Frame | Painted% | avgRGB | Notes |
|------|--------|-------|----------|--------|-------|
| `01_splash.png` | `splash_screen` | 45 | 0.57% | 0,0,0 | "PRESS START" text visible, background black (no intro movie) |
| `02_main_hub.png` | `main_hub_screen` | 254 | 59.35% | 30,21,22 | Main hub scene partially rendered; neon signs, MUSIC label, crowd visible |
| `03_song_select.png` | `song_select_screen` | 1186 | 90.44% | 26,30,31 | Music Library list visible with song rows, album art placeholder (? icon) |
| `04_part_difficulty.png` | `part_difficulty_screen` | 1465 | 93.48% | 67,69,69 | "20TH CENTURY BOY T. REX" + venue art shown, GUITAR/BASS selector |
| `05_game_screen_entry.png` | `tv3_b_screen` (transition) | 1489 | 5.61% | 2,1,3 | tv3 cinematic intro playing: microphone stand close-up shot |
| `06_gameplay_t5s.png` | `tv3_b_screen` (still) | 1662 | 0.62% | 0,0,0 | Near-black frame mid-transition (vignette fading in) |
| `07_gameplay_t15s.png` | `game_screen` | 1820 | 71.74% | 37,18,24 | Gem track rendering with guitar highway, crowd, stage visible |

---

## Comparison against `images/retail-screenshots/`

### `02_main_hub.png` vs `yt_mhKNp9uAT48_menu_hub.png`
- **Present in web:** Neon signs, MUSIC label, basic scene geometry rendered with lighting.
- **Missing vs native:**
  - The "PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS" main menu text is absent — the web hub scene renders the venue 3D scene but the HUD/overlay menu labels are not visible.
  - No player character visible at bottom of screen (native shows "AltairEspen" avatar).
  - Top HUD elements (fan count bar, star rating) absent.
  - **Verdict: main_hub 3D scene renders but UI overlay (text, menu list) is missing.**

### `03_song_select.png` vs `yt_qRagnZCIMzk_song_select_list.png`
- **Present in web:** MUSIC LIBRARY header, song list rows with separators, score columns (0/10, 0/5, 0/30), bottom button bar.
- **Missing vs native:**
  - Song titles are blank in the list rows (white-on-dark rows without song name text). Native shows "Killer Queen", "More Than a Feeling", etc.
  - Album art panel: web shows a placeholder "?" icon; native shows the real album art (Rock Band logo or cover art).
  - No artist names next to song titles.
  - **Verdict: song list structure is correct, but song name text is not rendering (font/text pipeline issue). Album art fetch not working.**

### `04_part_difficulty.png` vs native refs
- **Present in web:** Song title "20TH CENTURY BOY T. REX" renders correctly in the header. Venue background photo (subway/graffiti wall) renders. GUITAR/BASS selector label visible.
- **Missing vs native:**
  - Instrument difficulty dot indicators not visible.
  - Player profile slot row shows mostly blank.
  - **Verdict: best-looking screen — title + venue render correctly.**

### `05_game_screen_entry.png` + `07_gameplay_t15s.png` vs `yt_qRagnZCIMzk_gameplay_guitar.png`
- **Present in web:** tv3 cinematic transition plays (microphone close-up). Gem highway renders in gameplay with gem track, fret lines, crowd, stage lighting.
- **Missing vs native:**
  - HUD overlay absent: no score display (native shows "78 250"), no streak counter, no star power indicator, no energy bar.
  - No overdrive effects visible.
  - Song progress bar not shown.
  - **Verdict: 3D gem highway renders but all HUD elements are absent.**

---

## Visual deficits summary (ranked by impact)

1. **Menu text / song list text not rendering** — song names invisible in song_select. Likely font texture or text material not loading. Highest user-facing impact.
2. **Main hub menu labels absent** — "PLAY NOW / CAREER" etc. not showing over the 3D scene. Same root cause as (1).
3. **Gameplay HUD missing** — score, streak, star power, progress bar all absent during gameplay. Critical for playability.
4. **Album art not loading** — placeholder "?" shown instead of real cover art. Asset fetch or texture decode gap.
5. **Splash background black** — intro movie not playing (expected — BinkVideo is stubbed). Low priority.

---

## Memory / heap observations

- **WASM heap at boot:** 119.6MB (125,435,904 bytes)
- **WASM heap during gameplay:** 206.8MB (216,858,624 bytes) — within the 256MB W4 target
- **heap_boot probe via:** `Module.HEAPU8.buffer.byteLength` (Emscripten standard)
- **`performance.memory` JS heap:** not sampled (Chromium's `performance.memory` requires `--enable-precise-memory-info`; the initScript hook did not fire)
- **Gap:** no per-frame heap growth curve captured. W4d.0 requires a DevTools Memory recording during a full playthrough.
