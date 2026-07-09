# W26-GLOW — now-bar / combo-multiplier glow (SWEEP S5) — PLAN

**Lane:** GLOW (Opus). **Owns:** `native/src/rb3_render_hook.cpp` + scripts ONLY.
**Charter:** WAVE26_KICKOFF A7 + WAVE26_REVIEW Q5. Confirm-then-fix, light lane.

## The bug (S5, LOW-MED)
The now-bar / combo-multiplier ring reads plain on native — no lit "Nx" glow vs retail.
Retail GT (`images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png`): a strong
radiating **blue halo** around the bottom-center "4x" combo ring, plus a pink/magenta
lit glow on the now-bar strike line.

## In-tree root cause the review pinned (A7 / Q5)
The native halo/bloom pass **deliberately excludes** `gem_smasher_glow` (the now-bar
plate) from the additive halo unless `RB3_SMASHER_HALO=1`
(`native/src/rb3_render_hook.cpp:285-309`, comment B6 → engine `RB3HaloPass.cpp:68-76`).
Two mechanisms are in play:
1. `kHighwaySmasher` material emissive boost (×1.25) — engine `RB3MaterialBinder.cpp:589`,
   already ALWAYS-ON (opt-out `RB3_FRET_GLOW_OFF`). This lights the plate itself.
2. The additive **halo bloom** radiating around it — engine `RB3HaloPass` / `IsHaloSourceMat`
   — gated OFF for gem_smasher_glow via the hook's `QueryHaloPolicy` (RB3_SMASHER_HALO).
   This is the radiating ring in the GT.

## Discriminator (STEP 1, A7 — checkpoint verdict BEFORE any fix code)
1. Build a DRIVEN-COMBO gameplay state via natural `autohit` (proven harness,
   `song-end-test.py:61` / `capture_song_gameplay.py:168`). NOTE: direct `set_multiplier`
   only updates the meter; ≥4x PeakState/glow needs natural streak build via
   `BandTrack::SetStreak` → `GemTrackDir::PeakState`. So autoplay-and-wait for ≥4x.
2. A/B `RB3_SMASHER_HALO=1` vs OFF on a driven-combo (≥4x) frame. Screenshot both.
3. Verdict branches:
   - **HALO closes S5** (glow appears, matches GT, no over-bloom, no highway/HUD regress):
     GLOW is a FLAG-DEFAULT decision. Recommend the flip (coordinator decides E1). NO new
     fix code.
   - **HALO does NOT close it** (glow still missing/wrong): root-cause the actual combo-ring
     emissive/material (cf P1 gem bloom-halo `RB3_HIGHWAY_BLOOM_OFF`), fix flag-first default-OFF.
   - **already faithful on current build**: NO-FIX + defer.

## Gates (E1 + regression)
- E1 vs retail GT (`yt_qRagnZCIMzk_gameplay_*`, STRUCTURAL — combo ring shows lit Nx glow at ≥4x).
- drawlog-792 flag-OFF byte-identical (no default flip → OFF path unchanged).
- batch_objdiff == baseline (no engine src touched unless a fix is needed; if halo-flag-flip,
  no code change at all).
- No highway / HUD / gem-bloom regression (shipped RB3_HIGHWAY_BLOOM defaults intact); halo
  must not over-bloom to the giant-white-sphere failure the ×2 boost caused (wave-4 note).

## Harness
`scripts/native/glow_ab_capture.py` (NEW, lane-owned): boot headless, nav to gameplay via the
canonical part:/diff: sequence, autohit, poll for ≥4x streak (via `/api/dta/eval` on the
BandTrack streak / PeakState), then screenshot with RB3_SMASHER_HALO ON and OFF.
