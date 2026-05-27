# makestring_wrap_literal

**Pattern file**: `scripts/permuter/patterns/makestring_wrap_literal.py`
**Status**: B2 (new; proven by hand on `RndText::UpdateScrolling` 69.5% -> 71.9% in one edit).

## What

Wrap a bare string literal on the right of a `TheDebug << "..."` write in
`MakeString(...)`:

```cpp
TheDebug << "   width: " << mWidth << "\n";
// becomes
TheDebug << MakeString("   width: ") << mWidth << "\n";
```

Runtime behaviour is unchanged — single-arg `MakeString(const char *)` returns
its argument verbatim. Only the **caller's stack frame** changes.

## Why this moves match%

`utl/MakeString.h` declares one `MakeString` overload as `inline`:

```cpp
inline const char *MakeString(const char *c);
```

Its body instantiates a `FormatString` local, which holds `char mFmtBuf[2048]`
plus a handful of pointer fields — roughly **0x82C bytes** of stack. When the
target source called `MakeString("lit")` and our reimplementation just passes
the bare literal, the **caller's** stack frame is short by ~0x82C bytes
(the multi-arg `MakeString<...>` templates are NOT inline, so the FormatString
lives in the callee's frame for those).

The asm tell is a matched-opcode mismatch on prologue/epilogue stack-pointer
arithmetic (`stwu r1, r1, -0x830` vs `stwu r1, r1, -4`) and a cascade of
offset shifts on every other stack slot.

## When the pattern fires

- The diagnosis shows a `stwu` / `addi r1` / `subi r1` mismatch where target
  vs base immediates differ by ~0x800-0x880.
- AST has at least one `TheDebug << "..."` site (cheap pre-check via
  `_has_candidate_site`).
- Variants: one per call site, capped at 8 per function (regions-aware).

## Cross-reference

- MEMORY: `feedback_makestring_inline_constant.md`
- Header: `src/system/utl/MakeString.h` (single-arg inline)
- Related: `fixable-macros.md` (`MILO_ASSERT` stringification pool shifts)

## Repo footprint

10 hit functions (~50 sites) at time of authoring — see
`python -m scripts.permuter.pattern_scan --patterns makestring_wrap_literal`.
