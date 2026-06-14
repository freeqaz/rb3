# verify-ui-trio — Wave-3 independent verification of diff-grid / fret-held / highway-centering

**Verifier:** opus (wave-3, adversarial) · 2026-06-11 · **Ports:** 8851–8859
**Build verified:** the COMPOSED master binary `native/build-native/rb3-native`
(built 21:00:38, after the last wave-2 landing `cca1869a` @ 21:00:13).
**Engine pin:** `469c550` (matches `milo-native-engine` HEAD). **Evidence:** `/tmp/rp3-ui-trio/`

## Composition sanity (all three fixes present in the COMPOSED tree)

- **diff-grid** — `src/system/rndobj/Text.cpp:1188-1218` has the `topZ`/`botZ`
  icon re-center (`std::strstr(matName,"icon")`, shift by `0.5*(f6 - f6Raw)`);
  `Font.h` `RawCellDiff()` present. Landed `4d69205d`.
- **highway-offset** — `native/src/rb3_native_settings.h:36` `float camRotX = 0.0f;`
  (was `-4.0f`). Landed `5248158d`.
- **fret-held** — engine `standard_wgsl.inc:814-816` emissive `emissiveTint`
  white-fallback for near-black base; `Rnd_Wgpu_RB3.cpp:4798-4805`
  `gem_smasher_glow.mat` ×2.0 emissive boost + `RB3_FRET_GLOW_OFF` opt-out.
  Engine `8874e77`, reachable in pinned `469c550`.

---

## CHECK 1 — DIFF-GRID (song select, instrument icons centered on dot rows) — **PASS**

**Capture:** `song-select-capture.py --port 8851 --depths 16` → `diff-grid/native_depth_16.png`
(China Grove highlighted; the per-instrument difficulty grid shows in the right panel).

**Method:** detected the clean dot-row centers (bass y=453.7, drums y=495.5,
vocals y=578.2) by blob-centroiding the bright dot spheres, then measured each
instrument-icon disk center in a symmetric ±24px window anchored on that dot row.

| Row | dot-row y | icon disk center y | **delta** |
|---|---|---|---|
| bass | 453.7 | 453.7 | **+0.0 px** |
| drums | 495.5 | 496.4 | **+0.9 px** |
| vocals | 578.2 | 579.8 | **+1.6 px** |

All within ~2px — icons are vertically centered on their dot rows, matching the
retail relationship in `yt_qRagnZCIMzk_song_select_diff_ratings.png` (icon center
== dot row on every row; see `diff-grid/ref_grid_zoom.png`). The impl doc's
BEFORE was a uniform ~22–28px downward offset; that is gone.

- **Icon SHAPE preserved (criterion 2):** bass glyph ring is round
  (`diff-grid/leftcol_zoom4x.png` shows clean circles, not squished half-height
  ovals) — the wide-atlas CellDiff correction is NOT regressed.
- **Devil/expert bars + dots still render (criterion 4):** the red devil bars and
  all difficulty dots are present (`diff-grid/grid_zoom.png`, `annotated_align.png`).
- **Normal text unshifted (criterion 1b/3):** the song list ("Before I Forget",
  "Bohemian Rhapsody"…) renders cleanly with no vertical artifact; gameplay HUD
  score text ("2,500") is correctly positioned (`hud_text_check.png`). The change
  is gated to `"icon"`-material fonts, so normal text is structurally untouched —
  confirmed visually in the composed build, no interaction regression observed.

**Evidence:** `diff-grid/native_depth_16.png`, `grid_zoom.png`,
`leftcol_zoom4x.png`, `annotated_align.png`, `ref_grid_zoom.png`, `hud_text_check.png`.

**Caveat (pre-existing, out of scope):** the right panel is partially obscured by
the "FRIEND RANKINGS" overlay text + a flat grey album-art box — the scout/impl
flagged both as separate panel-state/album-art z-order problems, NOT the diff-grid
fix. The icon-vs-dot alignment is cleanly measurable around them and is correct.

---

## CHECK 2 — FRET-HELD (held fret lights the now-bar smasher per-slot color) — **PASS**

The real-input held-fret glow is an additive overlay that is hard to capture with
naive timing (and autohit lights smashers independently). I made it deterministic
with a no-autohit **A/B**: hold one fret continuously, capture ~14 frames, measure
the held-slot smasher-region brightness vs a nothing-held baseline, with the fix
ON (default) vs `RB3_FRET_GLOW_OFF=1`.

**Per-color held-glow on the held slot (fix ON):**

| Color | bit | baseline | held max | **delta** |
|---|---|---|---|---|
| green | 1 | 71.9 | 110.8 | **+38.9** |
| red | 5 | 75.2 | 119.8 | **+44.6** |
| yellow | 4 | 88.1 | 130.6 | **+42.6** |
| blue | 6 | 70.6 | 112.2 | **+41.6** |
| orange | 7 | 86.7 | 140.0 | **+53.3** |

**Opt-out negative control (`RB3_FRET_GLOW_OFF=1`, hold green):** baseline 59.8 →
held max 64.1 = **+4.2** (noise floor) — i.e. with the fix disabled the held
smasher does NOT glow. Side-by-side proof:
`fret-held/green_held_ON_crop.png` (green smasher bright/lit) vs
`green_held_OFF_crop.png` (green smasher same flat color as the others).

**Correct per-slot color + only the held slot lights:**
`fret-held/held_all_colors_montage.png` (rows green/red/yellow/blue/orange) shows
each held fret lighting its OWN button in its OWN color while the other four stay
dim. **Off-when-released** is established by both the baseline (no glow) and the
opt-out (no glow at all).

**Verdict:** the composed shader fix (`emissiveTint` white fallback) + the ×2.0
glow boost render the held-fret smasher glow correctly per slot, and the opt-out
suppresses it — the fix is genuinely responsible.

**Evidence:** `fret-held/held_all_colors_montage.png`, `green_held_ON_crop.png`,
`green_held_OFF_crop.png`, plus `{green,red,yellow,blue,orange}-on/` and
`green-off/` frame sets.

---

## CHECK 3 — HIGHWAY CENTERING (now-bar center ≈ 0.50W, head-on) — **PASS**

**Capture:** `keyboard-to-gameplay.py --port 8853 --game-burst 18` → `glow-on/burst_*.png`.

**Now-bar center (fret-button span):** the clean, settled frames read
center-frac = **0.500** consistently; the fret span is `[439,841]`, perfectly
symmetric about screen-center 640 (left arm 201 == right arm 201, **asym = 0px**).

**Highway-surface center per row (skew check, far→near, burst_10):**

| y (far→near) | 340 | 370 | 400 | 430 | 490 | 520 | 550 | 580 |
|---|---|---|---|---|---|---|---|---|
| center frac | 0.502 | 0.499 | 0.499 | 0.501 | 0.500 | 0.500 | 0.500 | 0.505 |

mean **0.502 ± 0.005** — flat at center, no far→near drift = head-on (the old bug
drifted 0.50→0.56–0.58 rightward).

**Negative control (`CAM_ROTX=-4`, the old broken default):** now-bar center
shifts to **0.566** (clean frames burst_07/08/09), asym **+167px** (pushed right)
— exactly the scout's measured right-shift. Visual: `hwy_before_after.png`
(top = -4, highway right of the red centerline; bottom = default 0, highway on
the centerline). This proves the camRotX=0 default is responsible for the fix.

**Evidence:** `glow-on/burst_10.png`, `hwy_before_after.png`, `hwy-cam-4/burst_*.png`.

---

## OVERALL VERDICT: **PASS** (all three checks)

All three fixes compose cleanly and are confirmed in the merged binary with
quantitative measurements AND negative controls (opt-out for fret-held, CAM_ROTX
env for highway). No interaction regressions observed: the diff-grid Text.cpp
change does not shift normal song-list/HUD text; the three fixes touch independent
surfaces (UI glyph geometry / native camera setting / engine smasher emissive).

## Residual gaps (on these three issues)

- **diff-grid:** the "FRIEND RANKINGS" overlay + grey album-art box still obscure
  part of the difficulty grid in song select — pre-existing, separate, out of
  scope for diff-grid (flagged by scout/impl; queued as a wave-3+ follow-up).
- **fret-held:** the glow is coupled to the `sTrackLight && game.cam` block, so
  `RB3_TRACK_LIGHT_OFF=1` also suppresses it (documented, expected). No issue.
- **highway-centering:** none. The deeper fix (porting the milo `1_player_<aspect>`
  apply-handler so `set_track_offset 0` runs natively, removing the
  CAMERA_FRAME_FIX hack) is noted in the impl doc as optional follow-up; the
  1-line default is correct for single-player. Multiplayer fan-out is gated on
  `gHxNativeNumUsedGemTracks==1` and was not exercised headless (single-agent).

## New issues noticed in passing (off-topic)

- **(minor) Highway "blue surface" mask is noisy across some frames** — the bluish
  highway-surface color overlaps venue/background blue, so a naive B>R mask catches
  off-highway pixels on a few frames (std jumps to ~0.046 vs the 0.005 single-clean-
  frame). Not a render bug — just a note for whoever automates highway-centering
  measurement: prefer the now-bar fret-span center (deterministic 0.500) over the
  surface-blue mask.
- **(observed, already tracked)** Song-select right panel: the flat grey album-art
  box (RGB ~177) and FRIEND-RANKINGS overlay obscuring the grid — already in the
  PLAN.md wave-3+ follow-up list; re-confirming it is still present on the composed
  build.
- No new regressions or crashes seen during the ~9 boot-to-gameplay / song-select
  runs across ports 8851–8859.
