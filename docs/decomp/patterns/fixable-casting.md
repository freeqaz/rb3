# Fixable: Casting

Patterns where the choice of explicit cast affects the generated code path.

## (float)x vs (float)(long long)x

Never cast through `long long` for float conversion — it calls `__cvt_sll_flt` library function instead of using `fctiwz`/`fcfid` directly. Prefer the direct `(float)x` form.

## (int) Cast for Signed Arithmetic Shift

`(int)unsignedVal >> shift` generates `sraw` (sign-extending arithmetic right shift), while `unsignedVal >> shift` generates `srw` (logical zero-fill shift). Match whichever the target uses.

**Example:** In `DecodeDxtColor`, `((int)rowPtr[4] >> shift)` generated `sraw` matching the target's DXT color index extraction.

## Enum Return Type Prevents Arithmetic Bool Optimization

When two code paths return consecutive integers (e.g., 3 and 4), CW with `-O4,p` applies `base + !!(bool)` arithmetic (`neg/or/srwi/addi`). Returning a proper enum type instead of `int` disables this optimization, generating branches instead.

**Example:** In `Tour::GetMode`, changing return type from `int` to `TourMode` enum with named constants (`kMetaTour_KnownRemote=3`, `kMetaTour_BrowsingRemote=4`) generated `cmpwi/li/beq/li` branches instead of `3 + !!IsLocal()` arithmetic.

## Truthiness Test vs Explicit Comparison Flips fcmpu Operand Order

`if (floatVar)` and `if (floatVar != 0.0f)` generate `fcmpu` with different operand orderings. The truthiness form puts the variable first; the explicit comparison puts zero first.

**Example:** In `Intersect(Ray)`, `if (dot)` generated `fcmpu cr0, f9, f0` while `if (dot != 0.0f)` generated `fcmpu cr0, f0, f9`.
