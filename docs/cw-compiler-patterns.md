# MetroWerks CodeWarrior Compiler Patterns for RB3 Decomp

Patterns discovered during decompilation that affect code generation with MetroWerks CodeWarrior for Wii (mwcceppc v4.3.172) under `-O4,p -inline noauto -ipa file`.

## Register Allocation

### Declaration Order Affects Register Assignment
The order you declare local variables directly influences which callee-saved registers (r25-r31, f28-f31) CW assigns them to. Variables declared earlier tend to get higher-numbered registers.

```cpp
// To get varA in r31 and varB in r30:
int varA = ...;  // gets r31
int varB = ...;  // gets r30

// Swapping declaration order swaps registers
```

**Example:** In `OvershellPanel::Poll`, declaring `inSession` FIRST gives it r29, matching the target.

### int vs unsigned int Changes Register Priority
CW gives `unsigned int` loop counters lower register priority than `int`. Changing a variable's signedness can shift the entire register coloring.

**Example:** In `FitText`, changing `unsigned int ellipsisLen` to `int ellipsisLen` let `mText` reclaim r31 as the function-wide cache, fixing 8 register swap mismatches.

### Intermediate Variables Force Instruction Ordering
Creating a named local for a sub-expression forces CW to compute and store it at that point, controlling instruction scheduling.

```cpp
// Target loads mult before accumulation:
int *pMult = (mult < maxMult) ? &mult : &pData->mMaxMultiplier;
int multiplier = *pMult;  // forces early load
pData->mMaxPts += score;
pData->mMaxStreakPts += (float)multiplier * score;
```

**Example:** In `FreeCamera::Poll`, `float slewHalf = 0.5f;` before other declarations forced the constant load before `ry`, matching target register ordering.

### Local Pointer Cache for Register Hoisting
Caching a member pointer in a local variable causes CW to hoist it into a callee-saved register, matching patterns where the target pre-loads a pointer.

```cpp
// Instead of repeated this->mText:
RndText *t = mText;  // cached in callee-saved register
t->DeferUpdateText();
// ... use t throughout ...
t->ResolveUpdateText();
```

**Example:** In `FitText`, `RndText *t = mText` in kFitJust fixed 3 delete mismatches. In `GemPlayer::Pass`, `GemStatus *status = mGemStatus` matched the target's register hoisting.

## Bool Materialization

### IsLocal() vs !IsNet() Produce Different Patterns
`IsLocal()` generates `cntlzw + srwi.` (compact bool materialization), while `!IsNet()` generates `cmpwi + beq + li` (branch-based negation). Choose the one that matches the target.

**Example:** In `GemPlayer::Pass`, switching from `!IsNet()` to `IsLocal()` jumped match from 87.6% to 99.1%.

### Compound Bool Pattern
For materializing compound conditions, use explicit bool variables:
```cpp
bool rejN = A && B;
if (rejN) continue;
```
This generates the `cror` + `srwi.` pattern CW uses for compound boolean tests.

### Condition Inversion for Branch Direction
`if (x) return; body;` generates different branch polarity than `if (!x) { body; }`. CW emits `bne body; b exit` vs `beq exit; fallthrough`. Match whichever the target uses.

**Example:** In `Character::Poll`, `if (mFrozen) return;` matched the target's `bne [epilogue]` branch.

## Control Flow

### switch vs if/else Chain
CW generates different branch patterns for `switch(x)` vs `if/else if` chains. Switch produces `cmpwi+beq` sequences to specific case bodies; if/else produces cascading `bne` skips.

**Example:** In `SetDiskError`, converting if/else to `switch(err)` matched the target's branch structure.

### Early Return Inversion
```cpp
// Target uses: guard → return, then body
if (mDiskError == kFailedChecksum || mDiskError == err) return;
// vs:
if (mDiskError != kFailedChecksum && mDiskError != err) { body; }
```
These generate different branch directions. Match the target.

### do-while vs while Loop Structure
`do {} while` and `while {}` generate different loop entry patterns. `while` adds an initial branch to the condition check. Check which the target uses.

**Example:** In `CharLipSync::PlayBack::Poll`, changing `do {} while` to `while` matched the loop structure.

## Floating Point

### Expression Splitting Controls fmadds/fneg Scheduling
Splitting a compound float expression into two statements controls when CW applies `fneg` and whether it generates `fmadds` (fused multiply-add) vs separate `fmuls`+`fadds`.

```cpp
// Generates fneg before multiply:
float result = -fabsf(ry*ry) * slewSpeed * ry;
// vs generates fneg after:
float result = -(fabsf(ry*ry) * ry * slewSpeed);
```

**Example:** In `FreeCamera::Poll`, `slewY = -fabsf(ry*ry) * slewSpeed * ry` matched the target's early-negate pattern.

### (float)x vs (float)(long long)x
Never cast through `long long` for float conversion — it calls `__cvt_sll_flt` library function instead of using `fctiwz`/`fcfid` directly.

### #pragma fp_contract on
Enables fused multiply-add (`fmadds`/`fmsubs`) generation. Required when the target uses fused operations.

**Example:** In `QuatSpline`, `#pragma fp_contract on` was needed for the Catmull-Rom formula to generate fused operations.

## Pragmas

### #pragma pool_data off
Prevents CW's IPA from pre-loading the BSS segment base address into a callee-saved register at function entry. This is critical when the target doesn't have this optimization.

**Example:** In `SetDiskError`, `#pragma pool_data off` prevented IPA from hoisting BSS base to r31, which was causing a 4-register spill cascade.

### #pragma dont_inline on/off
Controls whether CW inlines functions within the pragma scope. Be careful — `dont_inline on` can cause `MessageTimer` constructors to not inline, drastically shrinking function size.

## Function/Argument Ordering

### CW Evaluates Function Arguments Right-to-Left
When calling a function with multiple arguments, CW evaluates the rightmost argument first. This affects which vtable calls happen in which order.

```cpp
// CW evaluates MinBlur() first, then MaxBlur(), then BlurDepth()
TheDOFProc->Set(BlurDepth(), MaxBlur(), MinBlur());
```

**Example:** In `FreeCamera::Poll`, reordering DOFProc::Set arguments to `BlurDepth(), MaxBlur(), MinBlur()` matched the target's vtable dispatch order.

### Function Definition Order Affects String Pool
The order functions appear in a .cpp file determines string literal pool offsets. Reordering function definitions can fix `@stringBase0` address mismatches.

**Example:** In `NetCacheMgr`, moving `NetCacheMgrInit()` before the constructor put `"NetCacheMgr.cpp"` at offset 0 in the string pool, matching the target.

## STL / Template Patterns

### CopyFrom() vs operator= for Vector Copy
`operator=` on a vector compiles to a compact external call. `CopyFrom()` (which calls `clear() + reserve() + insert()`) triggers CW IPA to inline the full copy, producing much more code that matches the target.

**Example:** In `RestoreGems`, `mixes.CopyFrom(backup_mixes)` generated the correct 1308-byte inlined copy vs 172 bytes from `operator=`.

### vector::erase Generates Unrolled Copy
`vec.erase(begin, end)` generates a `std_vec_range_assert` call plus an unrolled 8-element copy loop, which can account for hundreds of bytes of function code.

### Inline Accessor vs Direct Field Access
Using `Children()` inline accessor vs a local reference `ObjPtrList &children = mChildren` can produce different register allocation.

**Example:** In `SfxSeq::Load`, using `Children().clear()` instead of `children.clear()` fixed an r23/r24 register swap.

## ICF (Identical Code Folding) Risks

### LINKER_MERGED Functions
The linker merges identical function bodies. Making structural changes to match one function can cause it to merge with another, dropping match% dramatically.

**Example:** In `HDCache::Init`, changing bool materialization pattern caused ICF merge regression from 92% to 79%.

### Watch for ICF When Changing Conditions
If a function uses `!flag` and you change it to `flag == 0`, the generated code might become identical to another function, triggering ICF.

## Vtable / Class Layout

### virtual vs Non-virtual Affects Vtable Layout
Adding or removing `virtual` from a method changes the vtable layout, affecting all code that does virtual dispatch on that class.

**Example:** In `Rnd`, making `TestPoint` non-virtual fixed a 4-byte vtable offset that was breaking `EndWorld`.

### Static Message Guards
Function-local `static Message msg(...)` generates guard variables (`_GUARD_FuncName@msg`). These add initialization checks on every call. The guard pattern must match between source and target.

## Miscellaneous

### Operand Order in Commutative Operations
`a * b` and `b * a` can generate different register assignments for `fmuls`. Match the target's operand order.

**Example:** In `PatchPanel::Poll`, `unk60 * mScaleVelX` vs `mScaleVelX * unk60` fixed an OFFSET_SWAP.

### Nested Scope for Destructor Ordering
Wrapping a temporary in a nested `{ }` scope forces its destructor to run at the closing brace, which can match the target's instruction sequence.

**Example:** In `GamePanel::UpdateLatency`, `{ FilePath path; dir = LoadObjects; }` placed the destructor before `Find`, matching the target.

### Direct .Set() vs Constructor Assignment
`vec.Set(x, y, z)` avoids a temporary on the stack that constructor assignment `vec = Vector3(x, y, z)` creates.

**Example:** In `CharIKFingers`, `.Set(0.3f, -6.0f, 0.4f)` instead of `= Vector3(...)` fixed the stack layout.

### Enum Return Type Prevents Arithmetic Bool Optimization
When two code paths return consecutive integers (e.g., 3 and 4), CW with `-O4,p` applies `base + !!(bool)` arithmetic (`neg/or/srwi/addi`). Returning a proper enum type instead of `int` disables this optimization, generating branches instead.

**Example:** In `Tour::GetMode`, changing return type from `int` to `TourMode` enum with named constants (`kMetaTour_KnownRemote=3`, `kMetaTour_BrowsingRemote=4`) generated `cmpwi/li/beq/li` branches instead of `3 + !!IsLocal()` arithmetic.

### std::max with Literal First Arg Creates Anonymous Static
`std::max(0.0f, expr)` with the literal as the first `const float&` argument causes CW to allocate an anonymous static for the literal and generate a pointer-select pattern (compare, then load via pointer to either the static or a stack spill).

**Example:** In `Player::SubtractEnergy`, `SetEnergy(std::max(0.0f, mBandEnergy - f))` generated the correct stack-spill + pointer-select pattern with full stack frame, while a ternary `(x > 0 ? x : 0)` generated different codegen.

### Truthiness Test vs Explicit Comparison Flips fcmpu Operand Order
`if (floatVar)` and `if (floatVar != 0.0f)` generate `fcmpu` with different operand orderings. The truthiness form puts the variable first; the explicit comparison puts zero first.

**Example:** In `Intersect(Ray)`, `if (dot)` generated `fcmpu cr0, f9, f0` while `if (dot != 0.0f)` generated `fcmpu cr0, f0, f9`.

### Removing Explicit Copy Constructor Enables Register-Return ABI
If a small struct (≤8 bytes) has a user-defined copy constructor, CW uses the hidden-pointer ABI (r3=hidden ptr, r4=this). Removing the redundant explicit copy constructor lets CW use small-struct register-return instead.

**Example:** Removing `Vector2(const Vector2&)` from `Vec.h` fixed `CamShotFrame::MaxAngularOffset` and unblocked `Spotlight::NGRadii` — both needed register-return ABI for `Vector2`.

### Small Constant-Bound Loops Are Fully Unrolled at -O4,p
`for (int i = 0; i < N; i++)` where N is a small compile-time constant (≤6-8) gets fully unrolled by CW `-O4,p`, generating N copies of the loop body with no branch.

**Example:** In `ChordbookPanel::ChordComplete`, `for (int i = 0; i < 6; i++)` with a bit-check body was unrolled to 6 individual `andi./beq` sequences (63 instructions total).

### != vs < for Loop Comparison Changes Branch Pattern
`i != count` generates `beq` (via `add.` setting CR0), while `i < count` generates `cmplwi/ble`. For deque/vector iteration, `!=` often matches the target better.

**Example:** In `VocalTrack::HitTambourineGem`, `i != count` generated the correct empty-loop `beq` check, while `i < count` produced extra `cmplwi r0, 0x0; ble`.

### (int) Cast for Signed Arithmetic Shift
`(int)unsignedVal >> shift` generates `sraw` (sign-extending arithmetic right shift), while `unsignedVal >> shift` generates `srw` (logical zero-fill shift). Match whichever the target uses.

**Example:** In `DecodeDxtColor`, `((int)rowPtr[4] >> shift)` generated `sraw` matching the target's DXT color index extraction.

### STL __find 4-Wide Unrolled Search with CTR Loop
CW's STL `__find` for random-access iterators uses a 4-wide trip count: `(int)(last - first) >> 2` with `for (; count > 0; --count)` generating `srawi.` → `mtctr` → `bdnz` (hardware CTR loop). To match, manually write the unrolled search pattern with bit-shift trip count.

**Example:** In `SingerStats::SetPartPercentage`, manual 4-wide unrolled search with `goto done` on match generated the exact `mtctr`/`bdnz` pattern from CW's `__find` specialization.

### Pre-Loading Member Before Loop for Register Hoisting
Caching a member access in a local variable before a loop forces CW to hoist the load before the loop entry, matching patterns where the target pre-loads a value into a callee-saved register.

**Example:** In `Locale::FindDataIndex`, `const char *sStr = s.mStr` before the `while` loop caused CW to hoist the load before entering, fixing r9/r10/r11 register swap mismatches (91% → 100%).
