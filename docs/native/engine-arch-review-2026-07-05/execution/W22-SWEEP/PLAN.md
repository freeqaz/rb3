# W22-SWEEP — PLAN (fresh native-vs-retail visual discovery + triage)

**Lane:** SWEEP (Wave 22). **Scope:** scripts + docs ONLY. NO source edits, NO fixes.
**Mission:** capture native at the canonical screens, diff vs retail GT, produce a RANKED
(severity × confidence) Wave-23 visual menu. Convert eyeball sightings into named
mesh/bone/owner targets (T2 `uidump_query.py --roi`) where the gap is a world/skinned element.

## Ground truth (A8 trust table)
- **Gameplay primary:** Wii `yt_qRagnZCIMzk_gameplay_{guitar,drums,drums_starpower}` (1280×720).
  `drums_starpower` = FILLED 5-star oracle.
- **main_hub:** `yt_mhKNp9uAT48_menu_hub.png` (360/PS3, valid layout).
- **song_select:** `yt_qRagnZCIMzk_song_select_{list,album_art,diff_ratings,filter_panel}` (Wii).
- **part/diff select:** NO retail GT exists (README "Still missing"). Native captured, triaged
  by internal consistency + layout plausibility only, confidence capped LOW.
- **camera-of-TV** `yt_qSRJ8HHPXzM_*` = geometry-distorted, NEVER pixel-diff (reference only).
- **wikipedia/fandom** = layout-tertiary.
- All YouTube-compressed → diff LAYOUT / POSITION / PRESENCE, not color calibration.

## Exclusions (A7 — do NOT re-report)
1. **Ledger default-ON rows** (72): C8 skin (BLACK_HEAD/SKIN/COMPOSE/CHAR_REAL_LIGHT/RTT),
   venue (VENUE_LIGHT/PP_CHROMA/TRACK_LIGHT/HIGHWAY_BLOOM), crowd rebind, mesh-cache, all
   the ui/hub/scrollbar/setlist/review/rowfix/refraction fixes, walkon-snap, mitten, etc.
2. **Fixed 2026-07-02:** `512a1bde` (hub-ticker Y-anchor + song_select album-art overlap),
   `9a7c40eb` (album-art whole-assembly move). → memory findings #1, #4, #5 are FIXED.
3. **Memory don't-re-find:** C8 skin family, venue exposure, missing online chrome (offline).
4. **CLOSED / other-lane:** hands-finger family; FOREARM-FLOAT (Lane FOREARM); score-HUD +
   star-meter (Lane HUD owns memory findings #2 + #3). Do NOT re-report these.

## Method (per screen)
1. Boot native headless (`RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free port, frame-settle, pgid cleanup).
2. Capture PNG(s) via `/api/screenshot`.
3. Align to retail GT (aspect/scale — both 1280×720 targets so direct overlay for Wii shots).
4. Identify LAYOUT / POSITION / PRESENCE gaps (not color).
5. For WORLD/skinned gaps: `uidump_query.py --roi X,Y,W,H` on a RUNNING instance
   (`RB3_DRAWLOG_PROV=1`) → NAME owning mesh/bone/owner.
6. Dedup against exclusions; rank severity × confidence.

## Screens + harness
| Screen | Harness | Retail GT |
|---|---|---|
| main_hub | boot, settle at main_hub_screen | `yt_mhKNp9uAT48_menu_hub.png` |
| song_select | `song-select-capture.py` (depths 0,8,16) | `yt_qRagnZCIMzk_song_select_*` |
| part/diff | `keyboard-to-gameplay.py` intermediate capture | none (LOW conf) |
| gameplay | `keyboard-to-gameplay.py` → game_screen, screenshot | `yt_qRagnZCIMzk_gameplay_guitar` |

## Deliverables
- `PLAN.md` (this), `STATUS.md` (ranked Wave-23 menu), `evidence/` (native caps +
  retail-vs-native crops + uidump ROI queries), checkpoint.
- Each menu item: screen, description, severity, confidence, crop path, ROI provenance
  (mesh/bone/owner) where applicable, suggested first discriminator.

## Ranking rubric
- **Severity:** HIGH = large/central/systematic visual wrongness; MED = noticeable localized;
  LOW = minor/edge/subtle.
- **Confidence:** HIGH = clear native gap vs trustworthy Wii GT; MED = plausible but GT-era or
  capture-artifact uncertainty; LOW = no GT / internal-consistency judgment only.
