# Fixable: Macros, Pragmas, and Static Guards

Patterns controlled by compiler pragmas, macro layout, or static-variable initialization.

## #pragma pool_data off

Prevents CW's IPA from pre-loading the BSS segment base address into a callee-saved register at function entry. Critical when the target doesn't have this optimization.

**Example:** In `SetDiskError`, `#pragma pool_data off` prevented IPA from hoisting BSS base to r31, which was causing a 4-register spill cascade.

## #pragma dont_inline on/off

Controls whether CW inlines functions within the pragma scope. Be careful — `dont_inline on` can cause `MessageTimer` constructors to not inline, drastically shrinking function size.

## Static Message Guards

Function-local `static Message msg(...)` generates guard variables (`_GUARD_FuncName@msg`). These add initialization checks on every call. The guard pattern must match between source and target.

## __declspec(noinline) to Defeat IPA Inlining

When `-ipa file` causes CW to inline a helper that the target left as a `bl` call, mark the helper with `__declspec(noinline)`.

**Symptom in objdiff:** caller is ~70-80%, the diff shows N extra instructions inline at the call site (often inside a switch case), and the helper itself may still be 100% but never actually called.

**Examples:**
- `__declspec(noinline)` on `nandComposePerm` → `nandGetStatus` 74.9% → 100%, `nandGetStatusCallback` 18.5% → 100%.
- `__declspec(noinline)` on `UnsetRun` → `OSSuspendThread` 74.6% → 100%, plus side effects on `SetEffectivePriority`.

Use when ~5-25 extra inline instructions in a partial caller match the body of a helper defined in the same TU. The target was likely built with the helper in a different TU or with different cflags.

## #pragma ipa on (Per-File)

When a group-level `-ipa file` cflag would cause regressions in some TUs, enable IPA per-file via `#pragma ipa on` at the top of the affected `.cpp`.

**Examples:**
- `network/Platform/BandwidthCounter.cpp` + `ProfilingUnit.cpp` — per-file pragma; group-level on `network/Platform` regressed `StringStream`.
- `system/speex/libspeex/ltp.c` — group-level on `system/speex` regressed `bits.c`.
- `system/speex/libspeex/nb_celp.c` — fixed `nb_decode` 87.9% → 100% and `nb_encode` 98.77% → 99.9%.

## TU-Local Conditional Inline Macro

When a stlport (or container) method needs to be **inline in one TU** to match the target but **out-of-line in another TU** to preserve another match.

1. In the header (e.g. `_list.c`):
   ```cpp
   #ifndef _STLP_LIST_CLEAR_INLINE
   #define _STLP_LIST_CLEAR_INLINE
   #endif

   _STLP_LIST_CLEAR_INLINE void _List_base::clear() { /* body */ }
   ```

2. In the TU that needs the inline version (e.g. `StreamTable.cpp`):
   ```cpp
   #define _STLP_LIST_CLEAR_INLINE inline
   #include "stl/_list.h"
   ```

Other TUs leave the macro undefined → they see the non-inline version and continue calling `bl _List_base::clear`.

**Example:** `StreamTable::~StreamTable` 41.6% → 100% (commit 3aa722cf) without regressing 17 previously-100% functions in `SessionSearcher_RV.cpp`.
