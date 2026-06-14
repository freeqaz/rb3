# verify-menu-fog — Wave-4 composed-build verification (menu street-fog thinning)

Independent adversarial review (Opus), 2026-06-14. Read-only judgment of the
**composed wave-4 build** (`native/build-native/rb3-native`, rb3 master
`9c1f4449`, engine pin `58254f7` — verified `git rev-parse HEAD` in the engine
repo matches the pin exactly; binary + `Rnd_Wgpu_RB3.cpp.o` both built 21:41).
Target fix: engine `58254f7`, `BandRnd::DrawParticles` — fold material register
color into the per-vertex color + 0.35 haze-alpha scale + near-camera fade,
gated to `matColor.a < 0.999f`, no-op for `matColor==1`.

My own evidence: `/tmp/rp4rev-menu-fog/` (`cap.py` frugal hub-loop capture +
per-frame metric CSV, `flamescan.py` smasher-region brightness, `greenscan.py`/
`measure.py` from the wave-3 reference dirs). Ports 9121–9124, all instances
torn down. I did NOT trust the implementer's numbers — every figure below is my
own re-capture.

## VERDICT: CONFIRM

The fix does exactly what it claims and the no-regression guard is sound:

1. **The green-grey full-frame wash floor is GONE across the camera loop.**
   My own A/B with the fix's opt-out envs (`RB3_PART_HAZE_OFF=1
   RB3_PART_MATCOLOR_OFF=1 RB3_PART_NEARFADE_OFF=1`):

   | build | soft-green mean | max | p90 | frames >8% | frames >20% |
   |---|---|---|---|---|---|
   | **FIX ON** (default) | 2.21% | **4.02%** | 3.65% | **0 / 56** | **0 / 56** |
   | FIX OFF (3 opt-outs) | 4.09% | **36.33%** | 16.55% | 8 / 56 | 5 / 56 |

   FIX OFF reproduces the wave-3 wash exactly (worst shots 18–36% soft-green,
   `fix_off/off_f02700.jpg` = a thick olive haze film smothering the BARBER SHOP
   storefront and band). FIX ON: the worst frame across the whole loop is 4.02%
   soft-green — **inside the retail band (3.4–3.8%)** — and zero frames exceed
   8%. Same scene FIX ON (`fix_on/on_f02700.jpg`) shows crisp BARBER/TIGER/MUSIC
   neon, readable brick storefronts and band figures, dark background, thin warm
   haze. Matches `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png` far
   better than FIX OFF. The implementer's hide-test number (39.8%/61.7% → 5.3%)
   corroborates independently.

2. **No-regression CONFIRMED on gameplay particle FX (the critical test).**
   Boot-to-gameplay A/B (`keyboard-to-gameplay.py`, autohit-driven hit-flames,
   same song/venue/diff both runs), smasher/now-bar region brightness over the
   burst (`flamescan.py`):

   | | smasher p99 (mean) | brightfrac (mean) | max lum |
   |---|---|---|---|
   | FIX ON  | 0.970 | 12.79% | 1.000 |
   | FIX OFF | 0.972 | 12.13% | 1.000 |

   Statistically identical — flames fire at full intensity (max 1.000) in both.
   This is the direct empirical proof of the `matColor.a < 0.999f` guard: the
   A1 hit-flames / now-bar / gem-glow FX use `matColor==1` and take **none** of
   the haze thinning. Gameplay frames (`gp_on/burst_38.jpg`, `gp_off/burst_38.jpg`,
   `gp_on/07_playing.png`) show highway/gems/now-bar/band/venue all correctly lit,
   no washout, in both builds. 0 crashes / asserts in either run.

3. **Mechanism verified against source.** I read `git show 58254f7` —
   the diff matches the doc: `mcr/mcg/mcb/mca` from `mat->GetColor()`, folded
   into `cr/cg/cb/ca = p->col * mc`; `isHaze = !off && !matcoloroff && mca <
   0.999f`; haze scale + near-camera-forward-dot fade applied only when `isHaze`.
   The matColor fold is a true no-op for `(1,1,1,1)` (multiply by 1). All three
   opt-out envs are present in the binary (`strings | grep RB3_PART_*`).

4. **Interaction with the other wave-4 fixes: CLEAN (interactionsOk).** Swept
   menu hub → song select → gameplay (lit venue). The venue is **normally lit,
   not blown out** (venue soft-clip `1abd595` composing correctly), gems are
   **saturated and the now-bar is lit, no white-sphere** (fret-sphere `20b38a7`
   composing correctly), and the menu fog is thinned. The three brightness-
   shifting wave-4 fixes do not collide: fog thinning is CPU per-vertex color on
   `matColor<1` particles only; venue soft-clip is a shader lighting-sum rolloff;
   fret-sphere is a halo-source exclusion + bloom-boost tweak — disjoint code
   paths. Song select renders normally (no fog, no emissive washout).

## Wii match-neutrality (verified)

rb3 pin-bump commit `9660215b` touches only `native/CMakeLists.txt` (the engine
SHA), no `src/` → Wii byte-identical by construction. The engine change is in
`src/platform/Rnd_Wgpu_RB3.cpp` — a native-only WebGPU renderer file never
compiled for the Wii target. report.json unchanged (81.865%). CONFIRMED.

## Residuals (out of scope for THIS fix, pre-existing, NOT regressions)

- **Hub contrast still ~2.6:1, not retail ~10:1.** My loop-wide 3×3 contrast is
  2.62 (ON) vs 2.57 (OFF) — the fog fix does NOT move contrast and the doc never
  claimed it would. The remaining gap is the wave-3 "Fix 3" floor-lighting lever
  (the `ue=1` venue heuristic — sidewalk/brick never reach retail's deep blacks),
  explicitly deferred and independent of the fog. The fog fix correctly removes
  the *wash*; the *dark-cell floor* is a separate open lever.
- **Subtlety (honest tradeoff, in the fix's favor):** FIX ON raises the *median*
  thin-haze floor slightly (2.32% vs OFF's 0.66%) because matColor folds the
  red-tinted fog in as a faint uniform warm haze — but it eliminates the wash
  *spikes* (OFF p90 16.5% / max 36% → ON p90 3.65% / max 4.0%). This is the
  correct direction: retail has a uniform ~3.4–3.8% green-neon floor with no
  spikes, which is exactly the ON distribution.
- Song-select garbage digits after "SORTED BY SONG NAME" (`1843121372`), FRIEND
  RANKINGS overlay, grey album-art box — all pre-existing, already in PLAN.

## Evidence index (`/tmp/rp4rev-menu-fog/`)

| file(s) | what |
|---|---|
| `fix_on/on_f*.jpg` + `on_metrics.csv` | 56-frame default-config hub loop + per-frame metrics |
| `fix_off/off_f*.jpg` + `off_metrics.csv` | 56-frame fix-disabled hub loop (3 opt-out envs) |
| `fix_off/off_f02700.jpg` vs `fix_on/on_f02700.jpg` | the decisive wash vs thin-haze same-scene pair |
| `gp_on/` `gp_off/` (burst_*.png) | boot-to-gameplay A/B (hit-flame FX) |
| `cap.py` / `flamescan.py` / `greenscan.py` / `measure.py` | harnesses |

---

## Wave-3 section (preserved — `verify-menu-hub.md` was the prior doc)

The wave-3 composed-build verification of the menu-lighting + neon-slab/Part.cpp
fixes (verdict PARTIAL: saturated green slab fixed, but the Part.cpp sim fix
revived the street-fog into a green-grey wash, contrast win didn't reproduce —
max 3.9:1 vs retail ~10:1) lives in **`verify-menu-hub.md`** in this directory
(left in place, not overwritten). That PARTIAL is precisely what wave-4's
menu-fog fix (this doc) addresses — and CONFIRMS resolved for the wash; the
contrast residual remains the separate floor-lighting lever.
