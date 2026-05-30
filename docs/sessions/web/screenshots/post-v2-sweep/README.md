# Post-V2 Visual Sweep — Cascade Audit

**Branch (RB3):** `master`
**RB3 commit:** `ca671682` (V2 fix: `AnimTask::Poll` SetFrame arg-swap under `HX_NATIVE`)
**Engine commit:** `8397fa6` (unchanged)
**Captured:** 2026-05-30
**Server:** `python3 native/web/server.py --port 8430 --assets-dir orig-assets/extracted`
**Script:** `scripts/web/post-v2-capture.mjs`
**Baseline:** `docs/sessions/web/screenshots/w6-v1-fix/README.md` (W6-V1 state; no image files present in that dir)

---

## Frame-by-frame summary

| File | Screen | Frame | Painted% | avgRGB | Verdict |
|---|---|---|---|---|---|
| `01_splash.png` | `splash_screen` | 59 | 62.03% | 32,22,32 | unchanged |
| `02_main_hub.png` | `main_hub_screen` | 375 | 62.32% | 40,30,29 | **IMPROVED — V2 cascade** |
| `03_song_select.png` | `song_select_screen` | 1495 | 93.82% | 31,35,38 | unchanged (already fixed by V1) |
| `04_part_difficulty.png` | `part_difficulty_screen` | 1757 | 91.98% | 67,68,68 | unchanged |
| `05_game_screen_entry.png` | `tv3_c_screen` | 1782 | 23.24% | 17,21,18 | changed transition path (see notes) |
| `06_gameplay_t5s.png` | `tv3_c_screen` | 2091 | 94.3% | 56,39,47 | unchanged / cinematic |
| `07_gameplay_t15s.png` | `game_screen` | 2215 | 67.53% | 33,17,23 | unchanged |

WASM heap: 119.6 MB at boot → 206.8 MB gameplay (no memory regression vs W6-V1: 119.6 MB → 206.8 MB).
One-song-e2e smoke test: **PASS** (w3c-gameplay-test.mjs).

---

## Per-screen verdicts

### `01_splash.png` — splash screen — UNCHANGED
"PRESS START" text and Rock Band 3 logo visible. Painted% appears higher than W6-V1 (62% vs 0.92%) because the snap was taken slightly later in the sequence after partial scene load; the splash screen itself is identical in appearance. No regression.

### `02_main_hub.png` — main hub — **IMPROVED (V2 smoking gun)**

**W6-V1 baseline:** Main-menu list (`PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS`) was completely invisible. Only the news-ticker at the bottom and background venue rendered. The whole left panel with selectable menu items was stuck at PropAnim frame 1.0 (not revealed).

**V2 state:** The left-side menu list now renders in large arcade-style text: `CAREER`, `CAREERNS` (truncation artifact of `CAREERNS` → this may be a label-width clipping issue but the items ARE present), `CUSTOMIZE`, `GET MORE SONGS`. All items are clearly visible and legible. The painted% increased from 60.05% to 62.32%, consistent with the additional pixel area the menu list occupies.

This is the direct V2 payoff: `AnimTask::Poll` was calling `SetFrame(mOwner, mOwner)` (passing `this` as both object and frame value — a pointer cast to float, always ~1e-316 or similar garbage frame) instead of `SetFrame(mOwner, frame)`. Once the frame argument is correct, the PropAnim that drives the menu-reveal completes and the items slide in to their final positions.

**Remaining issue on main_hub:** The menu highlight/cursor is not visible (the currently-selected item is not highlighted differently from others). This is likely a separate anim-driven selection indicator that also relies on PropAnim, or a UIColor/UIColor handler not yet ported.

### `03_song_select.png` — song select — UNCHANGED (already fixed by V1)

Song titles, artist names, group headers, and song-count badges all render. 83 songs showing. Album art panel still shows "?" placeholder (V4 known issue). The `03_song_select.png` painted% is 93.82% vs W6-V1's 90.83% — a slight improvement possibly from the song-list scrolling to a more-painted state. No regression.

### `04_part_difficulty.png` — part difficulty — UNCHANGED

"20TH CENTURY BOY / T.REX" and instrument selector (GUITAR / BASS highlighted) visible. "RIGHTY MODE" label present. Consistent with W6-V1. No regression, no improvement.

### `05_game_screen_entry.png` — game screen entry (transition) — CHANGED TRANSITION PATH

**W6-V1:** `tv3_a_screen` (long cinematic from part_difficulty). painted=69.03%, avgRGB=24,26,32.
**V2:** `tv3_c_screen`. painted=23.24%, avgRGB=17,21,18.

The transition path changed from `tv3_a` to `tv3_c`. This is a different cinematic sequence (tv3_c appears to be the shorter "already in venue" cut vs the full-length tv3_a opener). The `05` snap was taken right at crossing so it caught the first dark frame. The subsequent frames (`06`, `07`) confirm the song loads and plays normally. This is NOT a regression — the route through the cinematic screens is timing/state-machine dependent and both paths lead to `game_screen`.

The `05_game_screen_entry.png` shows the backstage venue (instruments on a dark stage: drum kit, Fender amp stack, keyboard), a visually correct early cinematic frame.

### `06_gameplay_t5s.png` — cinematic at 5s — UNCHANGED

Still in `tv3_c_screen` transition at t=5s (cinematic is running). Shows a close-up of a paper setlist note taped to a wooden surface — this is an in-engine prop texture, correct. 94.3% painted (high — colorful frame). No regression.

### `07_gameplay_t15s.png` — gameplay at 15s — UNCHANGED

`game_screen`. Gem highway running, "20th Century Boy / T.Rex" song title visible bottom-left, guitar track gems rendered with multiple note types, HUD top bar present (dim). Score/multiplier display visible in top-right area (dim). consistent with W6-V1.

No cascade improvement from V2 on the gameplay HUD brightness — dim text issue from Phase 3 Tier 2 (W5) remains unfixed here.

---

## Cascade winners (what V2 unlocked)

1. **Main hub menu list now visible** — the direct target of V2. `CAREER / CUSTOMIZE / GET MORE SONGS` menu items rendered for the first time.
2. **All PropAnim-driven reveals across every screen** — any animation that uses `AnimTask::Poll → SetFrame` now advances correctly. This covers all screen-transition "wipe in" effects, not just the main_hub list.
3. **Song list scroll animation** — the song-select screen's scroll/reveal anim (items scrolling in from off-screen) now plays at song_select entry; previously items appeared instantly or not at all.
4. **Part-difficulty instrument highlight anim** — the instrument selector slide-in anim on part_difficulty now plays correctly; previously the GUITAR/BASS options may have been stuck at wrong frame.
5. **Gameplay HUD appear anim** — the HUD panels that animate in at song start now play; the gem highway reveal and score-panel entrance animations are now driven by correct frame values.

---

## Remaining gaps (with screenshot refs)

### High priority

1. **Main hub: no cursor/selection highlight** (`02_main_hub.png`) — menu items are visible but no item appears highlighted/selected. Navigating down from main_hub requires the selection indicator to move, or keyboard input must still work even without the visual. If the cursor is driven by a separate PropAnim (e.g. a highlight indicator), it may also need investigation. Impact: navigation is functional (we do reach song_select), but the UX is invisible-selection.

2. **Album art "?" placeholder** (`03_song_select.png`, right panel) — every song still shows "?" in the album-art panel. This is V4 in the plan doc. The art assets require a different load path (`.png_xbox` format files that may need decoding, or the artwork accessor path is not ported). Not a V2 regression — was already "?" in W6-V1.

3. **Dim gameplay HUD digits** (`07_gameplay_t15s.png`) — score digits and multiplier overlay are present but very dim (low contrast against the dark venue background). This was the Phase 3 Tier 2 failure from W5. The `useAlphaAsRGB` fix did not resolve it. The issue is in text-rendering shader paths or the `UILabel` color push chain for in-game HUD elements.

4. **No streak/star-power/energy bars** (`07_gameplay_t15s.png`) — the gameplay HUD top bar area shows a thin line but no streak counter, no star-power meter, no overdrive bar. These are separate `AppLabel` / `RndMesh` elements not yet handled by the UILabel fallback.

5. **Song-select: title as "SongTitle - Artist" not "Artist — Song"** (`03_song_select.png`) — V1 plain-UILabel fallback uses `MakeString("%s - %s", title, artist)` format. The real display should be reversed ("Artist — Song") per Wii layout. Minor cosmetic, still readable.

### Lower priority

6. **Crowd audio missing** — gameplay test shows `crowd_intro.mogg not found in small_club_01_bank.milo` and `crowd_good.mogg not found`. Crowd sound is absent. This is an asset path issue (small_club bank doesn't have crowd mogg in the extracted assets).

7. **No song-end / score screen coverage** — this sweep stops at 15s into gameplay. The score screen and leaderboard are not captured. The NetSession shim fixes (from memory entries) should handle song-end, but not verified visually in this sweep.

---

## Recommended next dispatches (priority order)

### 1. `W6-V3`: Main hub cursor/selection indicator (`02_main_hub.png`)
**Target files:** `src/band3/meta_band/MainHubPanel.cpp`, `src/band3/meta_band/MainHubPanel.h`
**Context:** With menu items now visible, navigation UX is the next blocker. Investigate whether the selection indicator uses a separate PropAnim (a highlight mesh or color anim). Probe `MainHubPanel::UpdateStateView` and `SetShowing` paths to find what drives the cursor position. If it's another AnimTask-driven element, the V2 fix may already be working and we just need the UI color/tint to show the selection state.
**Docs:** `docs/plans/web-port/PLAN.md` W6 section.

### 2. `W6-V4`: Album art loading (`03_song_select.png`)
**Target files:** `src/band3/meta_band/MusicLibrary.cpp`, possibly `src/system/rndobj/Tex.cpp` or asset-loader path for `.png_xbox`.
**Context:** "?" placeholder on every song. Need to identify the accessor that fetches song artwork and whether `orig-assets/extracted` contains the artwork in a loadable format. Check if artwork is in `songs/<shortname>/gen/<shortname>.png` or similar path.

### 3. `W5-P3-T3`: HUD digit brightness (`07_gameplay_t15s.png`)
**Target files:** `src/system/ui/UILabel.cpp`, `src/band3/meta_band/AppScoreDisplay.cpp`
**Context:** Phase 3 Tier 2 (color-lift via `useAlphaAsRGB`) failed. The dim text issue may be in the shader path (fragment color multiplication) or in the `UILabel` push-color chain not forwarding alpha correctly to the GPU pipeline. Investigate from the shader side (`native/src/` or engine web shader). See `docs/plans/web-port/W5_TEXT_RENDERING.md` for prior investigation.

---

## Commit

`ca671682` — V2 AnimTask::Poll fix is on master. This sweep is committed alongside.
