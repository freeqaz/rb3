# WHITE-fix — Wave 9 Lane B PLAN

**Item:** engaged-venue WHITE over-exposure (Wave-8 disclosed residual; raw sub-knee, luma
~0.709). Composite family; files freed after Wave-8.

## Subtasks

- **B.S1 (opus) — FORCE-REPRODUCE + DISCRIMINATE.** (1) a deterministic (or ≥50%-rate) WHITE
  reproducer (hot-input condition); (2) the `RB3_PP_OFF` scene-vs-composite discriminator with
  per-tonal-band numbers. Deliver `STATUS.md` (recipe + verdict + fix design). If the verdict
  is scene-side, STAGE the `Rnd_Wgpu_RB3.cpp` patch under `execution/WHITE-fix/` (fence: Lane A
  single-writes that TU) — do not edit it. **DONE** → verdict SCENE-SIDE; staged patch at
  `staged-patch-scene-side.md`.
- **B.S2 (Wave 10, gated on coordinator accepting scene-side landing) — LAND.** Apply the
  staged luma-preserving venue highlight compression behind `RB3_VENUE_WHITE_GUARD`
  (default-OFF); paired continuous fail-red on the flood reproducer + a re-captured natural hot
  boot; flag-OFF drawlog 792; lineup PASS; DC3 behavioral-zero-blast sign-off (shared shader).

## Files touched (this stage)
`execution/WHITE-fix/{STATUS.md,PLAN.md,staged-patch-scene-side.md,white_discriminate.py,
analyze_disc.py,montage_discriminator.png,measure/*.json}`. No engine/source edits (analysis
+ staged patch only). Build dir `native/build-agent-WHITE-fix` (own, clang Debug).

## Verify
`white_discriminate.py` reproduces WHITE deterministically (flood 2/5 strict, 5/5 over-exposed)
+ natural 1/5; `analyze_disc.py` discriminator → SCENE-SIDE on the paired flood arm.
