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

## See Also

- [fixable-casting.md](fixable-casting.md) — float conversion (avoid `(long long)` cast), enum return type, truthiness fcmpu operand order
