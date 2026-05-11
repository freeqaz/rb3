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
