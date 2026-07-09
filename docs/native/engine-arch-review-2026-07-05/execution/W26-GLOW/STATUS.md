# W26-GLOW — STATUS

## Headline
**RB3_SMASHER_HALO does NOT close S5 — but S5 is already faithful WITHOUT it.**
The now-bar / combo-multiplier "Nx" glow renders correctly on the current DEFAULT
build at ≥4x. **Verdict: NO-FIX + DEFER. Do NOT flip RB3_SMASHER_HALO (stays OFF).
No code change.**

## Discriminator (A7 / Q5) — driven-combo A/B, checkpointed before any fix code
Built a genuine ≥4x streak via natural `autohit` (proven harness; direct
`set_multiplier` was correctly noted to only move the meter, not fire PeakState),
dwelled to songMs ≈ 44–50k, and A/B-captured `RB3_SMASHER_HALO=1` vs OFF.
Also captured a 2x negative-control pair.

### Architectural finding (the S5 report conflated three distinct elements)
At ≥4x the retail "lit Nx glow" is composed of THREE elements, only one of which
the flag touches:
1. **Combo-multiplier "Nx" ring** — `StreakMeter` HUD, `mPeakStateTrig` → PropAnim in
   `streak_meter.milo` (`src/system/bandobj/StreakMeter.cpp:144`). Drawn in the UI/HUD
   cam, **NOT game.cam** → `RB3_SMASHER_HALO` has ZERO effect on it. This is the blue
   radiating ring in the GT.
2. **SP peak-state blue track overlay** — `peakstate_plane` (`kHighwayPeakstate`,
   engine `RB3MaterialBinder.cpp:602`, color ×2, **always-on**). The blue filigree over
   the whole lane at ≥4x.
3. **Now-bar strike-plate bloom** — `gem_smasher_glow.mat`, game.cam halo pass, halo
   **excluded** unless `RB3_SMASHER_HALO=1` (`native/src/rb3_render_hook.cpp:302-320`,
   comment B6 → engine `RB3HaloPass.cpp:68-76`). Excluded by default because enabling it
   caused the wave-2 "giant white sphere" over-bloom (20th Century Boy ~49.5s).

### Result (evidence/ring_montage_GT_OFF_ON.png, GT | OFF | ON)
- **OFF (default):** the "4x" ring shows the **same strong blue radiating glow** as the
  retail GT. Elements 1+2+3(base) all render. **S5's "missing" glow is present.**
- **ON:** the "4x" ring glow is **identical** to OFF (confirming it is HUD-driven, not
  flag-driven). ON only adds an extra additive bloom to the **now-bar strike plate**
  (evidence/nowbar_montage_OFF_ON.png) — a step toward the documented over-bloom, with
  **no improvement** to the combo-ring match.

**Why S5 read "plain":** the sweep frame was below 4x. At 1x/2x the ring is correctly
plain silver (retail is plain there too — evidence halo_*_2x_*.png). The peak glow is a
≥4x behavior and requires driven combo to observe. This is a capture-state artifact, not
a rendering gap.

## Gates
| Gate | Result |
|---|---|
| E1 vs retail GT (structural, combo ring lit Nx glow at ≥4x) | **PASS** by default — OFF ring glow matches GT (evidence/E1_GT_vs_native_OFF.png) |
| drawlog-792 flag-OFF byte-identical | **PASS (trivial)** — no code change; flag default-OFF and gameplay-only, never touches the menu/splash golden |
| batch_objdiff == baseline | **PASS (trivial)** — no src/system engine unit touched |
| No highway / HUD / gem-bloom regression | **PASS** — no code change; shipped RB3_HIGHWAY_BLOOM / FRET_GLOW / peakstate defaults intact |

## What RB3_SMASHER_HALO would cost if flipped (documented for the coordinator)
Flipping it ON adds a now-bar strike bloom halo that (a) does not improve the S5
combo-ring match (already faithful) and (b) reintroduces the wave-2 over-bloom failure
mode the exclusion was added to prevent. Recommendation: **leave OFF**.

## Deliverables
- Harness: `scripts/native/glow_ab_capture.py` (natural autohit → game_screen → dwell for
  ≥4x → screenshot; runs once per flag value).
- Evidence: `evidence/halo_{OFF,ON}_{0..4}.png` (≥4x), `evidence/halo_{OFF,ON}_2x_{0..3}.png`
  (2x negative control), `evidence/ring_montage_GT_OFF_ON.png`,
  `evidence/nowbar_montage_OFF_ON.png`, `evidence/E1_GT_vs_native_OFF.png`.
- Checkpoint: `/tmp/wave26-checkpoints/GLOW.json`.
- Code: **NONE** (rb3_render_hook.cpp untouched).
