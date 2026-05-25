# Fixable: Float Scheduling, fsel, FMA

Patterns around floating-point code generation: `fneg` scheduling, fused multiply-add, and expression splitting.

## Expression Splitting Controls fmadds/fneg Scheduling

Splitting a compound float expression into two statements controls when CW applies `fneg` and whether it generates `fmadds` (fused multiply-add) vs separate `fmuls`+`fadds`.

```cpp
// Generates fneg before multiply:
float result = -fabsf(ry*ry) * slewSpeed * ry;
// vs generates fneg after:
float result = -(fabsf(ry*ry) * ry * slewSpeed);
```

**Example:** In `FreeCamera::Poll`, `slewY = -fabsf(ry*ry) * slewSpeed * ry` matched the target's early-negate pattern.

## #pragma fp_contract on

Enables fused multiply-add (`fmadds`/`fmsubs`) generation. Required when the target uses fused operations.

**Example:** In `QuatSpline`, `#pragma fp_contract on` was needed for the Catmull-Rom formula to generate fused operations.

## #pragma fp_contract off to Suppress `fmsubs`

When the target emits separate `fsubs` + `fmuls` instructions but ours fuses them into a single `fmsubs`, wrap the function in `#pragma fp_contract off`:

```cpp
#pragma fp_contract off
void Spotlight::CalculateDirection(RndTransformable *t, Hmx::Matrix3 &m) {
    // ... Cross() call inlines without fmsubs fusion ...
}
#pragma fp_contract on
```

The `off` directive disables the fusion specifically for the enclosed function while leaving surrounding TU code unaffected.

**Precondition:** target must have **zero** `fmsubs`/`fmadds` in the function. `off` strips fusion uniformly — if target uses fusion in any slot, the pragma regresses. Verified 2026-05-25: `math/Rot::RotateAboutZ` 90.3% → 69.7% from a blind apply.

**Examples:**
- `Spotlight::CalculateDirection` 88.1% → 93.4%.
- `MakeRotMatrix(Vec3, Vec3, Matrix3)` 87.6% → 94.0% via manual 6-product Cross expansion (`yz, yx, zx, zy, xy, xz`) + a `0.0f` load between the muls and subtractions as a scheduling barrier (the `0.0f` is consumed by the subsequent `Normalize` zero-check).

## Manual Cross Expansion Forces Pre-Compute-All-Products

When the target schedules all 6 cross-product multiplications BEFORE any subtractions (instead of interleaving), a `Cross(a, b, c)` call may fuse mid-computation. Replace with explicit named intermediates:

```cpp
// Cross(a, b, c) — CW may fuse fmuls + fsubs into fmsubs mid-cross-product:
Cross(m.y, v2, m.x);

// Forces compute-all-muls-then-subtract pattern:
float yz = m.y.y * v2.z;
float yx = m.y.z * v2.x;
float zx = m.y.x * v2.y;
float zy = m.y.z * v2.y;
float xy = m.y.x * v2.z;
float xz = m.y.y * v2.x;
m.x.Set(yz - zy, zx - xz, xy - yx);
```

All 6 `fmul` results are held in registers before any `fsubs` runs, matching the target's instruction schedule.

**Example:** `MakeRotMatrix(Vec3, Vec3, Matrix3)` 87.6% → 94.0% (combined with `0.0f` scheduling barrier).

## See Also

- [fixable-casting.md](fixable-casting.md) — float conversion (avoid `(long long)` cast), enum return type, truthiness fcmpu operand order
