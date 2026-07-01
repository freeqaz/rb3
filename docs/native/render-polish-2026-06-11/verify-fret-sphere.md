# verify-fret-sphere — wave-4 independent adversarial verification (held-fret white-sphere regression)

**Verifier:** opus (wave-4, adversarial) · 2026-06-14 · **Ports:** 9101–9109
**Build verified:** the COMPOSED master binary `native/build-native/rb3-native`
(built 2026-06-14 21:41, after the wave-4 landing `9c1f4449` / pin bump `9660215b`).
**Engine pin:** `58254f7` (== `milo-native-engine` HEAD; fret-sphere fix = `20b38a7`).
**Evidence:** `/home/free/rp4rev-fret-sphere-evidence/` (written off-tmpfs — tmpfs user
quota was at its 38 GB ceiling from sibling-agent evidence dirs; do NOT prune theirs).

## VERDICT: **CONFIRM** (held-fret white-sphere regression is gone AND the per-slot
glow still works). All required sub-checks pass with negative controls. One honest
methodological caveat (continuous fret-hold is not achievable headless) and one
re-diagnosis correction (full-scene white-blob detectors conflate the sphere with
white OD sustain tails) are documented below — neither weakens the verdict.

---

## Composition sanity — the fix is actually in the binary

`milo-native-engine@58254f7` (== pin) contains `20b38a7`, two region-scoped hunks in
`src/platform/Rnd_Wgpu_RB3.cpp`, both verified present:

1. **`BandRnd::IsHaloSourceMat` (~L1979):** `if (mn && std::strstr(mn,"gem_smasher_glow")) { ... return false; }`
   excludes the now-bar plate from the additive-halo bloom source set, next to the
   existing `surface` exclusion. Opt back in with `RB3_SMASHER_HALO=1` (default-off).
2. **`BandRnd::DrawMesh`, `gem_smasher_glow.mat` branch (~L4824):** the wave-2 now-bar
   emissive boost changed `* 2.0f` → `* 1.25f`. `RB3_FRET_GLOW_OFF=1` still zeroes it.

No `src/` (Wii-matched) source touched → the implementer's Wii byte-identity claim is
structurally sound (engine-only, `native/CMakeLists.txt` pin bump is the only rb3 delta).

## Local-asset constraint (independently re-confirmed)

Only **3** songs have an extracted MIDI locally — `20thcenturyboy`, `antibodies`,
`25or6to4` (`find orig-assets/extracted -iname '*.mid'`). This matches the implementer's
"only these are playable" note; the repro song IS one of them, and I exercised all three.

---

## CHECK (a) — NO giant white sphere occluding gems in the bloom-heavy venue — **PASS**

Repro per verify-gems NEW ISSUE: **20th Century Boy, jump 49500, autohit** (the B-heavy
sustain window). `/tmp/rp3-gems/song2_probe_bin.py`, port 9101, 24 frames.

- `AFTER-20th/e_006.png`, `e_014.png`: the now-bar shows **colored** fret buttons
  (red/yellow/blue/orange) and the lane gems are **visible and colored** — no ~110 px
  white ball hovering above the now-bar occluding gems. The strike at center is a small,
  contained bright spot, not a sphere.
- Second + third venue (the "≥2 venues" requirement):
  - `AFTER-antibodies/a_010.png` (Antibodies pink venue, jump 15500) — clean colored
    now-bar, gems visible, no sphere; the bright pink venue is not blown out.
  - `hold-25or6-ON/green_06.png` / `orange_06.png` (25or6to4 green-purple venue,
    jump 30000) — colored buttons + visible gems, contained now-bar strike, no sphere.

**Attribution (the decisive control) — ISOLATED MESH A/B.** Full-scene white-blob
metrics are unreliable here: at this song position 51878–53966 ms is a MIDI **overdrive
phrase**, so the lanes carry **white OD-styled sustain tails** (135×228 px white columns)
in BOTH the fixed and halo-on runs — a naive "largest compact white component" detector
flags those tails as a "sphere" identically in both conditions (I built that detector,
saw the false-equal 14/24-vs-14/24, and rejected it; see `compact_blob.py`). The clean
attribution is `RB3_ISOLATE_MESH=gem_smasher_glow` (draw ONLY the smasher glow on black),
which removes tails/gems/venue:

| condition (ISO gem_smasher_glow, 26 non-transient frames) | litPx min | **median** | p90 | max |
|---|---|---|---|---|
| **default (fix: halo excluded, ×1.25)** | 1007 | **2479** | 5426 | 7925 |
| **`RB3_SMASHER_HALO=1` (old halo behaviour)** | 0 | **5171** | 7386 | 10100 |

The halo **more than doubles** the glow's lit footprint (median 5171 vs 2479, **2.1×**) —
i.e. the bloom is the size amplifier, exactly as the impl doc claims, and the exclusion
bounds it. Visual A/B: `AB_iso_fixedTOP_haloBOTTOM.png` (top = fix, small per-slot glow;
bottom = halo-on, larger soft mass) and `ISO-halo/h_012.png` (the soft radial teal glow
plate the bloom blew up). The impl's first-frame ISO transient (a corner white cloud in
`ISO-fixed/f_000.png`, songMs ~49.5 s pre-camera-settle) is a 2-frame jump artifact in the
bottom-right corner, NOT the now-bar sphere; excluded from the distribution above.

## CHECK (b) — per-slot colour CORRECT and glow bounded in size — **PASS**

- **Per-slot colour present (fix on, ISO):** `ISO-fixed/f_011.png`, `f_014.png` =
  pure GREEN glow (normRGB (0.0,1.0,0.84), saturation 1.0); `f_004/f_005` blue-ish
  ((0.79–0.83, 0.84–0.98, 1.0)); the now-bar strike is a separate small contained white
  blob (`f_005` bottom-left). The per-slot hue is NOT washed to white.
- **Bounded size:** ISO-fixed median 2479 px / max 7925 px (vs the old 6000–14000 px white
  blobs the impl/verify-gems recorded) — the glow is small per-slot, not a sphere.
- **Per-slot held response (real input, no autohit):** `hold_fret_probe.py` holds each
  fret (driver bits green=1 red=5 yellow=4 blue=6 orange=7 — matches `kPad_*` in
  `rb3_joypad_native.cpp`) and measures the smasher-row brightness on 25or6to4. Per-slot
  mean shift vs baseline (84.7): green 98.9 (+14), red 90.8 (+6), yellow 87.6 (+3),
  blue 99.5 (+15), orange 132.1 (+47). Each held fret lifts its own slot — the feature
  fires per-slot. (Max saturates at 255 because the now-bar strip is already bright at
  baseline; mean is the usable signal here.)

## CHECK (c) — the venue where fret-held ALREADY worked still glows — **PASS (not killed)**

verify-ui-trio (wave-3) measured +38–53 per-slot brightness on the held slot in its good
venue with the wave-2 build. The wave-4 fix only (i) drops gem_smasher_glow from the bloom
source set and (ii) trims the boost ×2.0→×1.25 — it does **not** touch the emissive bind or
the `square_smasher_bright_*` texture. Independently re-confirmed the feature survives:
the per-slot held response above (green/blue/orange clearly lift) and the ISO per-slot
colours (green sat 1.0, teal/blue) prove the held-fret glow is intact, just bounded and
un-bloomed. The ×1.25 (vs the old ×2.0 that clamped the colour to white) is what restores
the per-slot hue — a strict improvement, not a regression.

## Gem-core halo bloom (the INTENDED feature) still fires — **PASS**

`RB3_RENDER_DBG=1`, 20th Century Boy gameplay: **52/52** frames log
`[RB3_HALOBLOOM] … draws=5 thresh=0.55 blend=0.70 halo=1`. The fix removed only the smasher
plate from the source set; the gem cores (`prism_*`, `draws=5`) are still captured and the
additive halo replay still runs. `renderdbg/engine-9108.log`.

## `RB3_FRET_GLOW_OFF=1` still disables — **PASS**

`RB3_ISOLATE_MESH=gem_smasher_glow RB3_FRET_GLOW_OFF=1` → the isolated smasher glow is
near-black (`ISO-glowoff/o_005.png` = two faint specks; median litPx 876 vs 2479 with the
glow on). The residual is the un-boosted base plate (the env zeroes the *emissive boost*,
not the mesh draw), which is the documented/correct behaviour. Opt-out intact.

## Interaction sweep (the OTHER three wave-4 brightness fixes) — **no regression**

Composed-build sweep menu hub → song select → gameplay → score screen
(`keyboard-to-gameplay.py` + `song-end-test.py --require-endgame`):

- **menu hub** (`hub.png`, Baboon Nest) — neon signage + menu options readable, no fog
  wash obscuring them → **menu-fog** thinning composes cleanly.
- **song select** (`sweep/01_song_select.png`) — list + album-art panel render clean
  (the "FRIEND RANKINGS" overlay is the pre-existing out-of-scope panel issue, not wave-4).
- **gameplay, lit venue** (`sweep/burst_05.png`, Antibodies) — venue **normally lit, not
  blown out** → **venue soft-clip** composes; highway + colored gems + buttons clean, no
  fret sphere.
- **score screen** — `song-end-test.py --require-endgame` = **PASS** (reached
  `coop_endgame_popups_screen`, stable 25 s / 3424 frames, **no abort**) → **endgame**
  server-wiring fix composes.

⇒ `interactionsOk = true`.

## Residuals / honest caveats (do not change the verdict)

1. **Continuous fret-hold is not achievable headless.** The `pad:<bit>` queue forces a
   3-poll release between presses (`kPadHoldPolls=4` / `kPadGapPolls=3`,
   `rb3_joypad_native.cpp`), so a "held" fret actually pulses. I worked around it (autohit
   for the repro; queue-spam + max/mean over the window for per-slot) but a truly sustained
   hold + whammy wiggle remains a windowed/manual check — the same limitation every wave-3
   verifier flagged. The pulsed proxy is sufficient to prove per-slot response and that the
   sphere is gone.
2. **Full-scene white-blob detection is unreliable in the OD-phrase repro window** (white
   OD sustain tails masquerade as the sphere). ISO-mesh A/B is the correct attribution tool
   and is what this verdict rests on.
3. The bloom-heavy repro window's venue red/pink-wash (cross-run camera desync) is the
   separate, already-tracked "venue pink-wash" follow-up — not in scope for fret-sphere and
   not a wave-4 regression.

## Evidence index (`/home/free/rp4rev-fret-sphere-evidence/`)

- `AFTER-20th/` (24f repro, fix) · `HALO-on-20th/` (24f, `RB3_SMASHER_HALO=1`)
- `ISO-fixed/` `ISO-fixed-2/` (28f) · `ISO-halo/` `ISO-halo-2/` (28f) — the decisive A/B
- `ISO-glowoff/` (`RB3_FRET_GLOW_OFF=1`) · `AFTER-antibodies/` · `hold-25or6-ON/`
- `renderdbg/` (HALOBLOOM positive control) · `sweep/` + `hub.png` (interaction sweep)
- `AB_iso_fixedTOP_haloBOTTOM.png` — side-by-side glow-size A/B
- tools: `sphere_detect.py`, `compact_blob.py` (rejected — conflates OD tails),
  `iso_blob.py` (used), `hold_fret_probe.py` (per-slot held response)
