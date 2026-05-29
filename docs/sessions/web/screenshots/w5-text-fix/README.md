# W5 Text-Rendering Fix — Post-Fix Screenshots

**Commit (RB3):** `wt-web-w5-text-impl` branch (pending merge)
**Engine commit:** `8397fa6` on `w5-text-mesh-fix` branch (pending merge to engine/main)
**Engine baseline pin:** `5fda7f0` (unchanged in `native/CMakeLists.txt` — bump deferred to orchestrator integration)
**Captured:** 2026-05-29
**Server:** `native/web/server.py --port 8434 --assets-dir orig-assets/extracted`
**Script:** `scripts/web/w5-text-capture.mjs` (mirrors `w4-baseline-capture.mjs` with `OUT_DIR` redirected here)

Side-by-side baseline: [`../baseline-2026-05-29/`](../baseline-2026-05-29/).

The fix is engine-only — see commit `8397fa6` on `milo-native-engine` branch
`w5-text-mesh-fix`. Implementation plan: [`/docs/plans/web-port/W5_TEXT_RENDERING.md`](../../../plans/web-port/W5_TEXT_RENDERING.md).

---

## Frame-by-frame summary

| File | Screen | Frame | Painted% | avgRGB | Notes |
|------|--------|-------|----------|--------|-------|
| `01_splash.png`             | `splash_screen`         | 46   | 0.92%  | 2,2,1    | "PRESS START" — comparable to baseline (no movie) |
| `02_main_hub.png`           | `main_hub_screen`       | 314  | 59.56% | 31,22,23 | News-ticker text now visible bottom-left ("Receive an alert when ...") |
| `03_song_select.png`        | `song_select_screen`    | 1242 | 90.69% | 28,32,33 | "FRIEND RANKINGS" appears top-right; all 4 button labels readable bottom |
| `04_part_difficulty.png`    | `part_difficulty_screen`| 1512 | 93.97% | 69,71,71 | "RIGHTY MODE" + difficulty-dot labels legible (faint in baseline) |
| `05_game_screen_entry.png`  | `tv3_b_screen` (transition) | 1537 | 5.59% | 2,1,3 | Cinematic intro (same as baseline) |
| `06_gameplay_t5s.png`       | `tv3_b_screen` (still)  | 1711 | 0.61%  | 0,1,1    | Near-black vignette fade (same as baseline) |
| `07_gameplay_t15s.png`      | `game_screen`           | 1843 | 68.25% | 34,17,23 | HUD "COOL" panel top-left + side-strip text now visible |

WASM heap: 119.6 MB at boot → 206.8 MB in gameplay (identical to baseline — no memory regression).

---

## What the fix made visible

### `02_main_hub.png` — main hub
Baseline: lower-third of screen was dim purple geometry with no text overlays.
W5-fix: bottom-of-screen news-ticker reads "Receive an alert when (player) gets information about ... Band 3" plus the small bottom-left action button labels are now legible. The "PALACE" sign over the venue arch is sharper because the text-mesh path now picks up the alpha-mask correctly.

### `03_song_select.png` — song select
Baseline: completely blank song-name column; "CONNECT CONTROLLER" / "CHOOSE INSTRUMENT" button bar at the bottom was invisible (black-on-dark).
W5-fix: "FRIEND RANKINGS" label resolves over the friends panel. All 4 bottom button labels ("CHOOSE INSTRUMENT" x4) are now plainly readable. The song-row text column is still faint — see Phase 3 below.

### `04_part_difficulty.png` — part difficulty
Baseline: "20TH CENTURY BOY" / "T. REX" header was already legible (the only screen whose font happened to have a non-alpha-only diffuse atlas — see plan doc). "GUITAR" / "BASS" labels visible.
W5-fix: same header + selector labels, *plus* "RIGHTY MODE" panel label and the difficulty-dot labels are now visible inside the player slot row (they were missing in baseline).

### `07_gameplay_t15s.png` — gameplay HUD
Baseline: gem highway rendered but every HUD overlay (score, streak, star power, energy, progress) was invisible.
W5-fix: gem highway still renders, "COOL" panel appears top-left (the multiplier ring with the freestyle prompt), and a vertical strip of text appears on the far-left edge (label likely "LAST PROGRESS" — partly clipped at the canvas edge in this 1280x720 viewport). Score / streak / star-power numerals are still not legible — see Phase 3 below.

### Splash / transitions (`01`, `05`, `06`)
No change vs baseline — these frames have no text-mesh content (or are mid-fade).

---

## Visual regressions

None observed. The `paintedPct` deltas are within capture-to-capture noise:
- `01_splash`: 0.57 → 0.92 (+0.35; "PRESS START" rendered slightly differently — both legible)
- `02_main_hub`: 59.35 → 59.56 (+0.21)
- `03_song_select`: 90.44 → 90.69 (+0.25)
- `04_part_difficulty`: 93.48 → 93.97 (+0.49)
- `07_gameplay_t15s`: 71.74 → 68.25 (-3.49 — the highway happened to be at a different beat/camera angle this run; no visible regression in the frame, just less brightly-lit gems on screen at the moment of capture)

Per the W5 plan's risk analysis: the text-mesh predicate (`!mesh->Name()[0]`) is true only for the un-named RndText sub-meshes; every gameplay/scene mesh has a non-empty Name(). So the change cannot affect non-text geometry.

---

## Phase 3 — residual dimness (deferred follow-up)

A handful of text labels still render dim grey rather than crisp white:
- Song-row titles in `03_song_select.png` (faint vs background)
- Score / streak digits in `07_gameplay_t15s.png` (not visible at the centre/top of the HUD where the native ref shows large "78 250"-style numerals)
- Some news-ticker text on `02_main_hub.png`

This is the Phase 3 material-colour issue called out in `W5_TEXT_RENDERING.md`
(line ~170): the diffuse-as-alpha glyph mask is now correct, but the colour
the text gets multiplied by is dark grey instead of white. Likely root cause:
the UIColor "default"/"focused" style resolution in `UIListLabel` / `UILabel`,
or the `mAltStyle` / `style.color` path in `RndText`, is picking the wrong
slot (e.g. disabled-grey instead of focused-white). Engine-side
`MaterialUniforms.color[]` is just forwarding whatever the material gives it.

Investigation queue (not in this fix):
1. Add a probe in `BandRnd::DrawMesh` to dump `(mesh->Name() == "", mat->mColor)`
   for the first frame so we can see what colour values the text materials
   carry on each screen.
2. Cross-check with DC3: does its `Mesh_Wgpu.cpp` text path do any colour
   override beyond what `BuildMaterialParams(mat, isTextMesh)` does?
3. Walk RB3's `UIListLabel::SetState` to confirm the right Style is being
   picked for the active list row.
