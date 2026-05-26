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

## Packed Alpha Extraction — `(float)(int)((unsigned)x >> 24)`

Extracting the high byte of a packed RGBA `int` and converting to `float` is sensitive to the precise cast sequence:

```cpp
// 'packed' declared as int — must use unsigned cast then int cast then float cast:
float alpha = (float)(int)((unsigned)packed >> 24);  // srwi + xoris + lfd/fsub
```

The sequence preserves:
- `srwi` (logical right shift) — comes from `(unsigned)packed >> 24`
- `xoris` + `lfd` + `fsub` signed-int → float conversion — comes from the `(int)(...)` then `(float)` chain

Alternative forms produce different code:
- `(float)((unsigned)packed >> 24)` → calls unsigned→float helper
- `(float)(packed >> 24)` → `srawi` (arithmetic shift) instead of `srwi`
- `(float)(int)(packed >> 24)` → arithmetic shift again

**Example:** `VocalTrackDir::ApplyFontStyle` 76% → 99.9% — applied to three packed colors per call (along with int packing via `(int)((color.alpha*255)<<24 | ...)`).

## Modulo Rotate Trick — `x % 4` vs subtract form

MWCC `-O4,p` recognizes `signed_int % literal_pow2` and emits a 5-instruction rotate sequence (`slwi+srwi+subf+rotlwi+add`) that handles signed input. The subtract form does NOT trigger this optimization:

```cpp
// 5-instruction rotate trick — matches target:
int xRemainder = x % 4;

// 2 mul/sub instructions — mismatches:
int xRemainder = x - (x / 4) * 4;
int xRemainder = x - xQuotient * 4;

// Bitmask — works only for unsigned (negative inputs differ):
int xRemainder = x & 3;
```

**Example:** `RndBitmap::DxtColor` 87.1% → 93.8%.

## `unsigned long` for Multiply that Triggers `mulhwu` Magic Constant

When dividing by a constant (e.g. for `value / 6`), CW generates a `mulhwu` (multiply-high-unsigned) magic-constant sequence for `unsigned` types and a `mulhw` (signed) sequence for `int`. If the target uses `mulhwu` + `0xaaab` (the unsigned divide-by-6 magic), the source value must be `unsigned`:

```cpp
// Mismatching mulhw (signed):
int byteCount = numEntries * 6;

// Matching mulhwu (unsigned, 0xaaab magic constant for /6):
unsigned long byteCount = numEntries * 6UL;
```

The choice also affects `MakeString<>` template selection downstream — `int` becomes `<i>`, `unsigned long` becomes `<Ul>`.

**Example:** `StoreRedemptionsTable::Load` 94.8% → 99.5%.

## `unsigned int diff` over `int diff` for `cmplw`

Loop differences and span lengths often need `cmplw` (unsigned compare) instead of `cmpw` (signed). The cast on the difference forces the unsigned variant:

```cpp
// Mismatching cmpw:
int diff = dataEnd - dataStart;

// Matching cmplw:
unsigned int diff = (unsigned int)(dataEnd - dataStart);
```

**Example:** also `StoreRedemptionsTable::Load`.

## `(T)(float)floor(...)` Forces `frsp` Before `fctiwz`

When converting a `double`-returning math call (`floor`, `ceil`, `round`, `sqrt`, etc.) directly to a small integral type, MWCC may skip the single-precision rounding step. Target sometimes wants an explicit `frsp` before `fctiwz`/`fcfid`. Add an explicit `(float)` cast (or a `float` intermediate variable):

```cpp
// no frsp emitted:
x = (char)floor(Clamp(-127.0f, 127.0f, 0.5f + q.x * 127.0f));

// frsp emitted before fctiwz:
x = (char)(float)floor(Clamp(-127.0f, 127.0f, 0.5f + q.x * 127.0f));
```

Both shapes — inline `(T)(float)floor(...)` cast and `float f = floor(...); x = f;` intermediate — produce identical code (the same TGT-only `frsp` insert is recovered).

**Example:** `CharBonesSamples::Relativize` 80.6% → 81.4% via `ByteQuat::Set` (4 components) + `ShortVector3::ToShort`. **Always blast-radius-check first** — `ShortQuat::Set` is used across many TUs and the same trick regressed via IPA cascade (81.4→69.4%).

The source permuter pattern `math_return_cast` automates `return (T)math_call(...)` → `return (T)(float)math_call(...)`. The assignment shape `x = (T)floor(...)` and the `void`-return shape via member-assignment is covered when the same `cast_expression` AST node appears inside an assignment too (the pattern walks any cast_expression in the function body).
