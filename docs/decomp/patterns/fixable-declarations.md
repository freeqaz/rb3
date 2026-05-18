# Fixable: Declaration & Register Allocation Patterns

Patterns where source-level declaration choices control which registers MetroWorks CodeWarrior assigns to which variables.

## Variable Declaration Order

The order you declare local variables directly influences which callee-saved registers (r25-r31, f28-f31) CW assigns them to. Variables declared earlier tend to get higher-numbered registers.

```cpp
// To get varA in r31 and varB in r30:
int varA = ...;  // gets r31
int varB = ...;  // gets r30
```

**Example:** In `OvershellPanel::Poll`, declaring `inSession` FIRST gives it r29, matching the target.

## int vs unsigned int Changes Register Priority

CW gives `unsigned int` loop counters lower register priority than `int`. Changing a variable's signedness can shift the entire register coloring.

**Example:** In `FitText`, changing `unsigned int ellipsisLen` to `int ellipsisLen` let `mText` reclaim r31 as the function-wide cache, fixing 8 register swap mismatches.

## Intermediate Variables Force Instruction Ordering

Creating a named local for a sub-expression forces CW to compute and store it at that point, controlling instruction scheduling.

```cpp
// Target loads mult before accumulation:
int *pMult = (mult < maxMult) ? &mult : &pData->mMaxMultiplier;
int multiplier = *pMult;  // forces early load
pData->mMaxPts += score;
pData->mMaxStreakPts += (float)multiplier * score;
```

**Example:** In `FreeCamera::Poll`, `float slewHalf = 0.5f;` before other declarations forced the constant load before `ry`, matching target register ordering.

## Local Pointer Cache for Register Hoisting

Caching a member pointer in a local variable causes CW to hoist it into a callee-saved register, matching patterns where the target pre-loads a pointer.

```cpp
// Instead of repeated this->mText:
RndText *t = mText;  // cached in callee-saved register
t->DeferUpdateText();
// ... use t throughout ...
t->ResolveUpdateText();
```

**Example:** In `FitText`, `RndText *t = mText` in `kFitJust` fixed 3 delete mismatches. In `GemPlayer::Pass`, `GemStatus *status = mGemStatus` matched the target's register hoisting.

## Pre-Loading Member Before Loop for Register Hoisting

Caching a member access in a local variable before a loop forces CW to hoist the load before the loop entry, matching patterns where the target pre-loads a value into a callee-saved register.

**Example:** In `Locale::FindDataIndex`, `const char *sStr = s.mStr` before the `while` loop caused CW to hoist the load before entering, fixing r9/r10/r11 register swap mismatches (91% → 100%).

## Function Definition Order Affects String Pool

The order functions appear in a .cpp file determines string literal pool offsets. Reordering function definitions can fix `@stringBase0` address mismatches.

**Example:** In `NetCacheMgr`, moving `NetCacheMgrInit()` before the constructor put `"NetCacheMgr.cpp"` at offset 0 in the string pool, matching the target.

## CopyFrom() vs operator= for Vector Copy

`operator=` on a vector compiles to a compact external call. `CopyFrom()` (which calls `clear() + reserve() + insert()`) triggers CW IPA to inline the full copy, producing much more code that matches the target.

**Example:** In `RestoreGems`, `mixes.CopyFrom(backup_mixes)` generated the correct 1308-byte inlined copy vs 172 bytes from `operator=`.

## Inline Accessor vs Direct Field Access

Using `Children()` inline accessor vs a local reference `ObjPtrList &children = mChildren` can produce different register allocation.

**Example:** In `SfxSeq::Load`, using `Children().clear()` instead of `children.clear()` fixed an r23/r24 register swap.

## `return *this;` in `operator=`

When a user-defined `operator=` body lacks `return *this;`, CW's register allocator doesn't pin `this` to r3 (since r3 doesn't need to hold the return value at exit), and it picks r4 for member-pointer comparisons. This cascades into r3↔r4 swaps throughout the function.

**Example:** `FileMerger::Merger::operator=` was 64.6% (1904 bytes). Adding `return *this;` to the header definition fixed 274 real "replace" instructions — function jumped to **100%** in one edit (commit f1998277).

**How to scan:** `grep -rn "::operator=" src/ | grep -v "return"` finds candidate definitions missing the trailing return. Some may be intentionally `void` — confirm the diff first.

## stlport Direct Member Access vs Trivial Accessor

In `src/system/stlport/stl/_vector_sized.c`, MWCC with `-inline noauto -ipa file` does NOT inline `_M_start()` even when declared `inline`. This consumes an extra callee-saved register (`_savegpr_25` instead of `_savegpr_26`), shifts frame size, and cascades register allocation throughout `_Vector_impl::operator=`.

**Fix:** Replace `__x._M_start()` / `this->_M_start()` with direct member access `__x._M_ptr._M_data` / `this->_M_ptr._M_data` (the data pointer sits at offset 0, matching the target's `lwz r5, 0x0(rN)`).

**Impact (commit ba24c86e):** Single change fixed 6 `__as__` Vector_impl template instantiations to 100% in one shot:
- OutfitConfig::MatSwap, ::Piercing (previously AT_LIMIT), CharHair::Strand, CharIKHead::Point: all 68.5% → 100%
- Overlay, MeshAO variants: 97-98% → 100%
- Pattern applies to any trivial container accessor (`_M_start`, `_M_finish`, `_M_end_of_storage`).

## Explicit `__less<T>` Template Specialization

When `stlpmtx_std::sort<T*>` matches ~72-75% and the diff shows inlined `li r5, 0` / `li r5, 1` / `stb` / `lbz` bool materialization at the call site, CW is inlining the `__less<T>` factory function. The target uses a proper `bl __less<T>` call.

**Fix:** In the `.cpp` (NOT the header), add:
```cpp
namespace stlpmtx_std {
    template <> inline less<T> __less<T>(T*) { return less<T>(); }
}
```

**Example (commit 0135fc1f):** `sort<GameGem*>` 72.6% → 100%, `sort<CameraManager::Category*>` 74.9% → 100%. Headers don't work — the symbol needs ODR-uniqueness from the implementation file.

## Inline Container Helper Methods in Headers

When a container's dtor or accessor matches ~40-50% and the diff shows an out-of-line `bl Method` followed by sequence-mismatched epilogue, add an `inline` body to the helper method in the header.

**Wins:**
- `qChain<T>::erase(iterator, iterator)` inline body in `qChain.h` → dtor 42.6% → 98.9% for BandwidthCounter + SystemSetting (commit 9ebc5ef7).
- `qVector<T>(size_type n, const T& val)` ctor + explicit default ctor in `qStd.h` → `Quazal::Key::Key` 41.3% → 100%.

When `__dt__` or `__ct__` for a container-using type matches in the 40-50% range, check whether the helper container method is currently out-of-line.
