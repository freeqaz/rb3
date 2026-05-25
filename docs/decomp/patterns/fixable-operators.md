# Fixable: Operators & Argument Order

Patterns around operator overload selection, commutative operations, and argument evaluation.

## CW Evaluates Function Arguments Right-to-Left

When calling a function with multiple arguments, CW evaluates the rightmost argument first. This affects which vtable calls happen in which order.

```cpp
// CW evaluates MinBlur() first, then MaxBlur(), then BlurDepth()
TheDOFProc->Set(BlurDepth(), MaxBlur(), MinBlur());
```

**Example:** In `FreeCamera::Poll`, reordering DOFProc::Set arguments to `BlurDepth(), MaxBlur(), MinBlur()` matched the target's vtable dispatch order.

## Operand Order in Commutative Operations

`a * b` and `b * a` can generate different register assignments for `fmuls`. Match the target's operand order.

**Example:** In `PatchPanel::Poll`, `unk60 * mScaleVelX` vs `mScaleVelX * unk60` fixed an OFFSET_SWAP.

## Direct .Set() vs Constructor Assignment

`vec.Set(x, y, z)` avoids a temporary on the stack that constructor assignment `vec = Vector3(x, y, z)` creates.

**Example:** In `CharIKFingers`, `.Set(0.3f, -6.0f, 0.4f)` instead of `= Vector3(...)` fixed the stack layout.

## `memset`/`memcpy` on Tiny Buffers → Typed Stores

`memset(buf, 0, N)` or `memcpy(&dst, &src, sizeof(T))` on small (≤16B) stack buffers emits a `bl memset`/`bl memcpy` call. When the target uses inline `sth`/`stb`/`psq_st` stores instead, replace with typed C: `*(short*)buf = 0; buf[2] = 0;` or `T dst = src;`.

**Wins:** `AppLabel::SetPitch` 87.7→95.6% (memset on 3-byte buf); `UtilDrawCigar` 60.6→64.7% (memcpy of 0x40-byte Transform).

## std::max with Literal First Arg Creates Anonymous Static

`std::max(0.0f, expr)` with the literal as the first `const float&` argument causes CW to allocate an anonymous static for the literal and generate a pointer-select pattern (compare, then load via pointer to either the static or a stack spill).

**Example:** In `Player::SubtractEnergy`, `SetEnergy(std::max(0.0f, mBandEnergy - f))` generated the correct stack-spill + pointer-select pattern with full stack frame, while a ternary `(x > 0 ? x : 0)` generated different codegen.

## `lwzu` via Reference-Cast Auto-Increment

When the target reads a packed byte sequence with `lwzu` (load-word-with-update — load + advance pointer in one instruction), the C source must use the reference-cast auto-increment idiom:

```cpp
static unsigned char sKey[] = { 0x7a, 0x4d, 0x60, 0x7c, 0xFF };
const unsigned char *p = sKey;
unsigned int word = *((unsigned int *&)p)++;  // emits lwzu
```

Equivalent-looking forms emit TWO instructions instead:

```cpp
unsigned int word = *(unsigned int*)p;  p += 4;          // lwz + addi
unsigned int word = *((unsigned int*)p);  p += 4;        // lwz + addi
```

The exact `*((unsigned int *&)p)++` shape is what makes MWCC emit `lwzu`.

**Example:** `Synth::returnMasterKey` 94.5% → 99.9%.

## `Symbol::operator==(const char*)` over `streq(symbol.Str(), ...)`

`Symbol::operator==(const char*)` generates the `addic.` + strcmp + `cntlzw` null-guard pattern the target uses. `streq(symbol.Str(), "lit")` generates a different sequence (extra `.Str()` accessor inline + different comparison shape).

```cpp
// Mismatch:
if (streq(plat.Str(), "pc")) ...

// Match:
if (plat == "pc") ...
```

**Example:** `BandWardrobe::GetPrefab` 85.2% → 99.2%.

## `!streq(a, b)` over `if (strcmp(a, b))` for bool branches

`!streq(...)` materializes `(strcmp == 0)` as a bool via `cntlzw+srwi.`, then `!` inverts to `bne`. `if (strcmp(a, b))` emits `beq` with different surrounding scheduling.

```cpp
// Mismatch — beq:
if (strcmp(Dir()->Name(), "main")) { ... }

// Match — bne via bool materialization:
if (!streq(Dir()->Name(), "main")) { ... }
```

**Example:** `OutfitConfig::InMilo` 94.2% → 99.5%.

## MILO_WARN/MILO_LOG Argument Order Affects `MakeString<>` Template

The order of `%s`/`%d` args in MILO_WARN binds to a specific `MakeString<Args...>` template instantiation. Swapping arg order changes the template and the register assignment for the first/second arg.

```cpp
// Wrong arg order — different MakeString template:
MILO_WARN("This mesh (%s // %s)", Name(), Dir()->GetPathName());

// Correct — matches target's MakeString<PCc,PCc> arg order + r4/r5 assignment:
MILO_WARN("This mesh (%s // %s)", Dir()->GetPathName(), Name());
```

**Example:** `RndMesh::SkinVertex` 98.4% → 100% from this single swap.

## Format-String Tweaks Shift String Pool

A missing `\n` or a typo in any MILO_WARN/MILO_LOG/MILO_ASSERT format string shifts `@stringBase0` offsets for **every** function in the TU. When seeing systematic address-relocation mismatches across all functions in a unit, suspect a format-string difference vs the target.

**Example:** `CameraShot::LensSym_to_FOV` 84.0% → 84.1% fixed by adding a missing `\n` to a MILO_WARN format string in *sister* function `CamShotFrame::Interp`.

## Splitting BinStream `>>` Chains at Register-Reuse Points

`bs >> a >> b >> c` chains the result of each `operator>>` through a register. When the target reloads `bs` from its cached r30 mid-chain (instead of threading the prior result through r28), split the chain at that boundary:

```cpp
// Mismatching r28↔r30 swaps in the middle:
bs >> mGradientMap >> mGradientMapOpacity >> mRefractMap >> mRefractOpacity;

// Matching — second bs starts fresh from r30:
bs >> mGradientMap;
bs >> mGradientMapOpacity >> mRefractMap >> mRefractOpacity;
```

**Example:** `RndPostProc::LoadRev` 98.4% → 100% (combined with other fixes).
