# verify-venue-wash — wave-3 adjudication (composed build, engine pin 469c550)

**Question.** Is the gameplay venue "pink wash" in the orchestrator's smoke frame
(`/tmp/rp-smoke/07_playing.png`, Antibodies @ small_club, songMs≈21.5s) an
authored stage-lighting moment, or an engine lighting blowout?

**VERDICT: FAIL — it is a lighting BLOWOUT (soft-clip white-out of an *authored*
red/pink lighting moment), with the root cause isolated to the unbounded
lighting sum in the shared standard shader.** The underlying *moment* is real
and authored; its render saturates to white and destroys texture, which retail
GX hardware cannot do. The wave-2 changes (outer-halo bloom, unlit+emissive-all-
cams, mesh-cache) are individually **exonerated** — the amplifier predates wave
2 (the P4 venue-light path + the non-venue white-flood fallback).

## Method

8 headless runs of the exact smoke recipe (`scripts/native/keyboard-to-gameplay.py
--diff hard`, same binary `native/build-native/rb3-native` built 21:00, ports
8841-8848), all hitting the same song window: **Antibodies, small_club venue
(`ludvista_small_club` resources), songMs 0 → ~23.5s**. Venue-region pixel
metrics (central highway + HUD excluded) via `/tmp/rp3-venue-wash/measure.py`:
mean luminance, RGB, saturation, `clipW` (% pixels ≥250 in ALL channels),
`clipAny` (% pixels ≥250 in any channel).

| run | env | intro-window result (worst frame) |
|---|---|---|
| `/tmp/rp-smoke` (orchestrator) | default | **WASHED** — 06: lum 0.99, clipAny 98.4%; 07: (253,218,240) clipAny 97.0% |
| `base` | default | dark red, no wash (07: (78,9,8), clipAny 0.4%) |
| `probe` | default + RB3_VENUE_PROBE | dark red, no wash |
| `rep1` | default + probe | dark red, no wash |
| `rep2` | default + probe | dark red, no wash |
| `rep3` | default + probe | **partial wash** — 06 (CORK club room + crowd): (148,72,86) clipW 13.0%, clipAny 25.9% |
| `rep4` | default | **WASHED** — 07: (247,212,219) lum 0.876, **clipAny 93.2%** (same wall+hex-window shot as smoke) |
| `rep5` | default | **WASHED** — 07/b00/b01: lum 0.64-0.75, clipAny 63.8-78.9% (club room + crowd blown pink-white) |
| `venue_off` | RB3_VENUE_LIGHT_OFF=1 | **WASHED WARM** — 07: (244,207,193) clipAny 88.7% (white-flood fallback also over-drives) |

All runs PASS the harness; every run snaps back to a normal dark venue (lum
0.18-0.26, clipAny <2%) at the **same boundary, songMs ≈22.7→23.5s**.

## Findings

1. **The moment is authored.** The window boundary (~23.5s) is identical across
   all 8 runs — a scripted venue-lighting keyframe, not noise. In the 4 runs
   that rendered it correctly-ish, the same window is a *moody dark-red wash
   with full texture detail* (greenroom dartboard readable at (70,10,16),
   sat 0.93) — exactly the "heavy color wash" retail does.
2. **The white-pink frame is a blowout, not the authored look.** Washed frames
   have **63-98% of venue pixels ≥250** and texture detail destroyed; the
   correctly-rendered frames of the *same moment* clip ≤2%. "Authored wash
   keeps texture" fails decisively.
3. **Reproduced 3/7 default-env runs (smoke, rep4, rep5).** The variance is the
   venue's random camera-shot selection: **close-up shots** (camera near
   lit surfaces, inside point-light ranges) blow out; **wide shots** render
   dark red. rep5/burst_00 shows it position-dependently in one frame: room +
   crowd blown white-pink, foreground characters still dark red.
4. **The wash goes through the venue-light path** (not only the fallback):
   default-env washes are **pink** (B>G, e.g. (247,212,219)) matching the
   venue's pink/magenta lights (`main_crowd.lit` (0.93,0.69,0.99),
   `theaterpurp`, silhouette whites — see VENUE_PROBE dumps in
   `/tmp/rb3-kbd2game-8843.log`, `-8846.log`), while the
   `RB3_VENUE_LIGHT_OFF` wash is **warm** (G>B, (244,207,193)) — the plain
   white-flood signature. Both paths over-drive; the shipped one is the pink one.

## Root cause (engine, pre-wave-2)

`milo-native-engine/src/gfx/standard_wgsl.inc` `fs_main` (~:798):

```wgsl
finalColor = baseColor.rgb * (ambientLight + totalLighting.diffuse * shadowFactor) + ...
```

`(ambient + Σ diffuse)` is **unbounded**. The CPU side
(`src/platform/Rnd_Wgpu_RB3.cpp` `WriteSceneUniforms` :1195-1273) clamps each
light to 1.5 (dir) / 1.8 (point) but feeds up to 4+4 of them + ambient — sums of
2-7× are reachable, and at close range `falloff = (1-d/range)² ≈ 1` across the
whole frame. `compressHighlights` (tanh shoulder, knee 0.9, :575) then maps any
over-range input asymptotically to ~1.0 → **≥250 "soft-clipped white"** with
texture detail gone. On Wii, GX **saturates the rasterized channel color at 1.0
before the texture multiply** — lighting can tint but can never push output
above the texture's own brightness, so this moment reads as a saturated red/pink
room on retail, never a white-out. The non-venue fallback (1.0 white dir + 0.45
ambient = 1.45×, :1267-1272) over-drives pale textures the same way.

**Fix lead (one line, GX-faithful):** clamp the lit color before the texture
multiply — `min(ambientLight + totalLighting.diffuse * shadowFactor, vec3f(1.0))`
(at minimum under the venue path / world.cam). With the clamp, max compose input
is 1.0 → `compressHighlights(1.0) ≈ 0.95` → max ~243: the ≥250 clip class is
*eliminated by construction* while the authored dark-red look (already <1.0)
is untouched. Follow-up tuning (the open "per-env venue exposure" item) can then
decide if 1.5/1.8 per-light clamps are still wanted.

## Wave-2 interaction check (the composed-build question)

- **gem-colors outer-halo bloom (`70636b5`)** — game.cam-gated; the highway is
  dark and crisp in every washed frame. Not involved.
- **menu-lighting unlit (`7acc22a`)** — unlit/prelit materials *bypass* the
  lighting sum entirely (shader :795-796); they cannot wash from lights. Not
  involved.
- **emissive-all-cams (`7acc22a`)** — material-static; the wash is moment-
  windowed (0→23.5s) and shot-dependent, and the same scenes render normal-dark
  after 23.5s with emissive active. Not involved.
- The blowout amplifier (venue light path + fallback flood + unclamped shader
  sum) **predates wave 2**; this song/venue's hot authored moment is what
  exposed it.

## Evidence index

- Smoke (washed): `/tmp/rp-smoke/{06_game_screen,07_playing,burst_00,burst_01}.png`
- Reproductions (default env): `/tmp/rp3-venue-wash/rep4/07_playing.png`
  (93.2% clipAny, same shot as smoke), `/tmp/rp3-venue-wash/rep5/{07_playing,burst_00,burst_01}.png`
- Authored look, same moment: `/tmp/rp3-venue-wash/{base,probe,rep1,rep2}/0{6,7}*.png`
  (dark red, texture detail), `/tmp/rp3-venue-wash/rep3/06_game_screen.png` (partial)
- A/B: `/tmp/rp3-venue-wash/venue_off/` (RB3_VENUE_LIGHT_OFF — warm wash)
- Metrics tool: `/tmp/rp3-venue-wash/measure.py`; harness logs
  `/tmp/rp3-venue-wash/*.harness.log`, engine logs `/tmp/rb3-kbd2game-884{1..8}.log`
  (VENUE_PROBE light dumps in 8843/8846)
- Retail ground truth (dark venues): `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png`,
  `fandom_gameplay_guitar.png`

## Residuals / follow-ups

- Land the lighting clamp (engine `standard_wgsl.inc`), then re-run this window
  ~5× — pass = no intro frame with venue clipAny >5%, dark-red moment preserved.
- Even the non-clipped dark-red render is unvalidated against retail for THIS
  song/venue. **REFERENCE NEEDED:** retail/Xenia capture of Antibodies intro
  (0:00-0:30) in small_club.
- Open per-env venue exposure tuning (pre-existing) folds into the same pass.
- `RB3_VENUE_LIGHT_OFF` fallback also clips (1.45× flood) — same clamp fixes it.

## New issues noticed in passing

- None new. Crowd renders populated + distributed in rep3/rep5 frames (wave-2
  crowd fix visibly working in the composed build). No asserts/crashes in any
  of the 8 runs' logs. The known endgame `netServer` crash was not hit (runs
  end before song end).
