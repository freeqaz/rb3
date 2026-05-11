# Fixable: Macros, Pragmas, and Static Guards

Patterns controlled by compiler pragmas, macro layout, or static-variable initialization.

## #pragma pool_data off

Prevents CW's IPA from pre-loading the BSS segment base address into a callee-saved register at function entry. Critical when the target doesn't have this optimization.

**Example:** In `SetDiskError`, `#pragma pool_data off` prevented IPA from hoisting BSS base to r31, which was causing a 4-register spill cascade.

## #pragma dont_inline on/off

Controls whether CW inlines functions within the pragma scope. Be careful — `dont_inline on` can cause `MessageTimer` constructors to not inline, drastically shrinking function size.

## Static Message Guards

Function-local `static Message msg(...)` generates guard variables (`_GUARD_FuncName@msg`). These add initialization checks on every call. The guard pattern must match between source and target.
