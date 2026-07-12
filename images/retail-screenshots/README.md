# RB3 UI Reference Screenshots

Ground-truth captures of the real **Rock Band 3** game, for validating the
native/web port's rendering of the main menu, song select, and gameplay against
how the shipped game actually looks. Drop new references here
(`images/retail-screenshots/`). This is a small, curated set kept deliberately
*outside* the gigabyte-scale `orig-assets/` tree (which is gitignored) so these
stay safely committable.

**Platform note:** the build we target is the **Wii** debug binary (`SZBE69_B8`).
RB3's menu / song-select / HUD *layout* is identical across Wii / Xbox 360 / PS3
(same Milo UI scenes) — only output resolution and anti-aliasing differ (Wii is
480p; 360/PS3 are 720p). So 360/PS3 captures are valid layout references; Wii
captures are the most faithful for color/resolution. Each file below is tagged
with the platform it came from.

These are third-party copyrighted screenshots kept solely as internal visual
references for this reimplementation effort (fair use). Not game assets.

---

## Main menu / hub  ← was the biggest reference gap

| File | Platform | Res | Shows |
|---|---|---|---|
| `yt_mhKNp9uAT48_menu_hub.png` | 360/PS3 | 1280×720 | Top-level "Rock City" hub: PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS over the animated neon-street scene; friends/leaderboard panel top-right; CONNECT CONTROLLER footer. |
| `yt_mhKNp9uAT48_menu_playnow_submenu.png` | 360/PS3 | 1280×720 | Same hub with PLAY NOW expanded → QUICKPLAY / START A ROAD CHALLENGE (submenu fan-out behaviour). |

Source: YouTube `mhKNp9uAT48` — "Let's play rock band 3" (VikingNurse), frame @ ~12s / ~14s.

## Song select / song library  ← Wii-native

Primary set — clean **Wii** captures (`qRagnZCIMzk`):

| File | Platform | Res | Shows |
|---|---|---|---|
| `yt_qRagnZCIMzk_song_select_list.png` | **Wii** | 1280×720 | Music Library — scrollable song list sorted by name, header "VIEWING n OF 587 SONGS", album-art panel right, action bar bottom. |
| `yt_qRagnZCIMzk_song_select_album_art.png` | **Wii** | 1280×720 | Music Library — a song highlighted with its large album art shown; setlist/shuffle entries at the list top. |
| `yt_qRagnZCIMzk_song_select_diff_ratings.png` | **Wii** | 1280×720 | Music Library — right panel showing per-instrument difficulty star ratings (guitar / bass / drums / vox / keys) + NO PART / NO REVIEW states. |
| `yt_qRagnZCIMzk_song_select_filter_panel.png` | **Wii** | 1280×720 | Sort/Filter overlay — Sort By, Instrument Scores, Genres, Decades, Pro support, and Song Source counts (DLC / Guitar Hero / Rock Band 1-3 / RBN). |

Secondary — camera-of-TV **Wii** captures (`qSRJ8HHPXzM`), kept for the setlist view:

| File | Platform | Res | Shows |
|---|---|---|---|
| `yt_qSRJ8HHPXzM_song_select_wii.png` | **Wii** | 1280×720 (camera-of-TV) | Song library list (Before I Forget [Slipknot] highlighted …), album-art + difficulty panel, PLAY SONG / VIEW MORE INFO / NEXT HEADING. |
| `yt_qSRJ8HHPXzM_setlists_wii.png` | **Wii** | 1280×720 (camera-of-TV) | "Built-in Setlists" view (Drummer's Delight / Guitar Shredders / … / The Endless Setlist III) with the Harmonix-83-songs preview panel. |

## Gameplay (note highway)

| File | Platform | Res | Shows |
|---|---|---|---|
| `yt_qRagnZCIMzk_gameplay_guitar.png` | **Wii** | 1280×720 | Guitar note highway (5-lane, HOPO gems), score + streak HUD top-right. |
| `yt_qRagnZCIMzk_gameplay_drums.png` | **Wii** | 1280×720 | Drums note highway (4 pads + bass pedal), song-progress banner, venue background. |
| `yt_qRagnZCIMzk_gameplay_drums_starpower.png` | **Wii** | 1280×720 | Drums highway with Star Power active (blue glow on the gem field), 5-star HUD. |
| `gameplay_highway_wikipedia.jpg` | 360/PS3 | 400×225 | Classic 4-lane simultaneous play (keys + drums + guitar + vocals lyric ribbon), score top-right. Low-res but the canonical composed-HUD shot. |
| `fandom_gameplay_guitar.png` | 360/PS3 | 1200×675 | Single guitar track highway, gems + chord name ("C5"), score/streak HUD. |
| `fandom_gameplay_drums.png` | 360/PS3 | 1219×681 | Single drums track highway, score/streak HUD. |

## Other UI screens

| File | Platform | Res | Shows |
|---|---|---|---|
| `band_challenges_menu_wikipedia.jpg` | 360/PS3 | 400×225 | Post-song results / Band High Score screen (per-player % + solo scores + CONTINUE / RESTART). |
| `title_screen_360_tcrf.png` | 360 | 1920×1080 | Title screen ("Rock Band 3" logo). |

## Extracted UI assets — exact pixels (Wii)

Sprite sheets ripped from the Wii game files (The Spriters Resource). These are
the authoritative source for individual UI element pixels (not composed screens).

| File | Res | Shows |
|---|---|---|
| `ui_menu_icons_wii_spriters.png` | 535×328 | Menu icons. |
| `ui_buttons_wii_spriters.png` | 281×205 | Button glyphs. |
| `ui_instrument_icons_wii_spriters.png` | 530×598 | Instrument icons. |
| `ui_album_art_wii_spriters.png` | 1432×1042 | Default / placeholder album art. |
| `ui_savedata_banner_wii_spriters.png` | 256×163 | Save-data icon & banner. |

## Still missing (no good reference yet)

- **Difficulty / instrument-select screen** (per-player instrument + difficulty picker) — not present in any source checked; song-select → gameplay is a direct white-flash load in the videos available.
- A clean (non-camera, higher-res) **results / score** screen — only the low-res `band_challenges_menu_wikipedia.jpg` so far.

---

## Sources

- The Spriters Resource — Rock Band 3 (Wii): https://www.spriters-resource.com/wii/rockband3/
- Wikipedia "Rock Band 3" (fair-use in-game screenshots): https://en.wikipedia.org/wiki/Rock_Band_3
- The Cutting Room Floor — Rock Band 3: https://tcrf.net/Rock_Band_3
- Rock Band Wiki (Fandom): https://rockband.fandom.com/wiki/Rock_Band_3
- YouTube `mhKNp9uAT48` (VikingNurse, "Let's play rock band 3") — main-menu hub (360/PS3).
- YouTube `qSRJ8HHPXzM` ("rock band 3 long play") — Wii longplay, camera-of-TV (song select / setlists).
- YouTube `qRagnZCIMzk` (ImaDireWolf, "Rock Band 3 WII - CUSTOM SOURCES …") — clean Wii capture (song select ×4, gameplay ×3).

## How these were captured

Video frames were pulled with `yt-dlp` (≤720p, video-only) + `ffmpeg`
(`-ss <t> -frames:v 1 -q:v 1`). Contact sheets
(`-vf "fps=1/N,scale=320:-1,tile=CxR"`) were used to locate each screen, then a
clean settled frame extracted. Static images were downloaded via `curl`/Python
from the sources above. Web galleries checked that had NO usable in-game shots:
MobyGames (cover art only), GameFAQs / Giant Bomb (bot-blocked), Nintendo Life
(promo art + carousel only).

---

## RB3DX-on-Xenia captures  ← first-boot flow (photosensitivity / title / calibration) + main_hub-reached evidence

New in `xenia/`. Headless reference frames captured from **Rock Band 3 Deluxe
(RB3DX)** running under our **Xenia Xbox-360 emulator fork** (branch
`headless-vulkan-linux`, HEAD `36d4528a5` + uncommitted in-tree diagnostics),
`--gpu=vulkan` headless, 1280×720. These ground-truth screens the native/web
port previously had NO retail pair for: the first-boot **calibration flow** and
the RB3DX **photosensitivity / title** shells.

**How reached:** the boot advances `intro_movie_screen → splash_screen (animated
Deluxe title) → first_time_calibration → cal_welcome_screen → cal_audio_screen`
with time-scripted input; screen labels come from the emulator's read-only
`--rb3dx_ui_probe` sampler (reads `TheBandUI`→current `UIScreen` name each frame).
The `main_hub_screen` frame was reached with the `--rb3dx_skip_calibration`
diagnostic (primes `mHasSeenFirstTimeCalibration=1` so first boot routes past the
uncompletable headless A/V-latency calibration straight to the hub).

**Scanline caveat (important):** Xenia's headless frame readback raw-copies the
**tiled** resolved frontbuffer, so any **3D-resolved scene** captures with a
fine green/magenta 8-pixel scanline scramble (a 32-byte Xenos micro-tile
readback artifact, NOT a render bug — the underlying scene is correct). **2D /
menu-composited surfaces read clean.** Hence the calibration dialogs,
photosensitivity text, and overshell chrome are crisp, while the animated
title 3D backdrop and the `main_hub` night-street scene are scrambled. The
`*_main_hub_loading_artifact.png` frame is included as **evidence that
`main_hub_screen` is reached** (the "ROCK BAND 3 DELUXE" hub load + Player1
overshell are visible through the scanlines) — it is NOT a clean color reference.
A clean headless main_hub / gameplay capture is not currently possible: the hub
3D scene load deterministically crashes the guest (SIGSEGV at guest PC
`0x82BCEFE4`, ~30 s, 2/2 boots), so song-select and gameplay are unreached.

| File | Screen | Clean? | Shows |
|---|---|---|---|
| `xenia/xenia_rb3dx_photosensitivity_warning.png` | (boot splash) | ✅ clean | RB3DX bilingual (EN/ES) photosensitivity warning + animated diamond logo. |
| `xenia/xenia_rb3dx_splash_deluxe_title.png` | `splash_screen` | ⚠ 3D artifact | Animated "ROCK BAND 3 DELUXE" title shell (+ `RB3DX 1.1.0-nightly` build string); 3D backdrop scrambled. |
| `xenia/xenia_rb3dx_first_time_calibration.png` | `first_time_calibration` | ✅ clean | First-boot "Want to Calibrate your system?" dialog — CALIBRATE SYSTEM / CONTINUE, over the Player1 overshell + CONNECT CONTROLLER footer. |
| `xenia/xenia_rb3dx_cal_welcome.png` | `cal_welcome_screen` | ✅ clean | "CALIBRATE SYSTEM" intro — the three option icons (auto / TV / numeric) + filigree header band. |
| `xenia/xenia_rb3dx_cal_audio.png` | `cal_audio_screen` | ✅ mostly | "AUDIO CALIBRATION" — "CURRENT DELAY IS 0 MS" (headless stalls here: interactive A/V-latency test can't complete with null audio). |
| `xenia/xenia_rb3dx_main_hub_loading_artifact.png` | `main_hub_screen` | ❌ artifact | main_hub reached (hub load + Player1 overshell visible through the scanline scramble). Evidence-only. |

**Labeled detail crops** (zoomed, for agents reading UI element detail):
`xenia_rb3dx_crop_photosensitivity_en.png`, `..._crop_deluxe_diamond_logo.png`,
`..._crop_firsttime_dialog.png`, `..._crop_firsttime_buttons.png`,
`..._crop_overshell_player1.png`, `..._crop_calwelcome_header.png`,
`..._crop_calwelcome_icons.png`, `..._crop_calaudio_header.png`,
`..._crop_calaudio_delay.png`.

Source: RB3DX `default.xex` (TitleID `0x45410914` v0.0.5.1) on Xenia fork
`headless-vulkan-linux`, headless Vulkan, captured 2026-07 for the native/web
port. Third-party copyrighted game frames kept solely as internal visual
references (fair use); not game assets.
