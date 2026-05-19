# RB3 Pattern Reference Index

Quick reference for the MetroWorks CodeWarrior codegen patterns documented from RB3 decomp work. Targets the Wii build (`mwcceppc 4.3.172`, `-O4,p -inline noauto -ipa file -sdata 2 -sdata2 2`).

This index mirrors the structure of [DC3's pattern catalog](../../../../dc3-decomp/docs/decomp/patterns/INDEX.md) so cross-references work both ways between the two projects. Filenames match where the categories overlap; per-file content is MWCC-specific for RB3.

## Pattern Files

| File | Topic |
|------|-------|
| [fixable-declarations.md](fixable-declarations.md) | Declaration order, register hoisting, pre-load before loop, function definition order, `CopyFrom` vs `operator=`, `return *this;`, stlport accessor inlining, `__less<T>` specialization, inline container helpers in headers |
| [fixable-bool-mask.md](fixable-bool-mask.md) | Bool materialization (`IsLocal()` vs `!IsNet()`, compound bools, condition inversion) |
| [fixable-control-flow.md](fixable-control-flow.md) | `switch` vs if/else, early return inversion, `do-while` vs `while`, loop unrolling, nested scopes, STL `__find` / `vector::erase` patterns |
| [fixable-fsel-fma.md](fixable-fsel-fma.md) | Float expression splitting, `fmadds` scheduling, `#pragma fp_contract` |
| [fixable-casting.md](fixable-casting.md) | `(float)x` vs `(float)(long long)x`, `(int)` for arithmetic shift, enum return type, float truthiness `fcmpu` ordering |
| [fixable-comparison.md](fixable-comparison.md) | `!=` vs `<` for loop comparisons |
| [fixable-operators.md](fixable-operators.md) | Arg eval order (right-to-left), commutative `fmuls` operand order, `.Set()` vs constructor assignment, `std::max` literal-first quirk |
| [fixable-macros.md](fixable-macros.md) | `#pragma pool_data`, `#pragma dont_inline`, static `Message` guards, `__declspec(noinline)`, per-file `#pragma ipa on`, TU-local conditional inline macros |
| [fixable-struct-layout.md](fixable-struct-layout.md) | `virtual` vs non-`virtual` vtable layout |
| [fixable-copy-ctor.md](fixable-copy-ctor.md) | Explicit copy constructor blocks small-struct return ABI; TU-local inline `Hmx::Object` copy ctor; member function pointer to force out-of-line emission |
| [verifiable-icf.md](verifiable-icf.md) | Linker ICF risks — verify before treating as unfixable |
| [quazal-ddl-pattern.md](quazal-ddl-pattern.md) | `_DDL_X::Extract`/`::Add` are static members — porting MISSING Quazal DDL units to Matching |
| [wave-session-2026-05-18.md](wave-session-2026-05-18.md) | Session notes — param-slot reuse, `bool`→`int` returns, double-precision lowering, .cpp ordering controls inlining, MemcardMgr bitfield+dead-null pattern, header-edit blockers |

## Quick Decision Tree

```
Match% < 50%
  → Implementation is missing or wrong. Run /analyze-function and review Ghidra
    pseudo-C + m2c output. Check /dc3-pair for a sister-project reference.

Match% 50-80%
  → Structural issues. Check control flow (switch vs if/else, early-return
    inversion) and variable declaration order.

Match% 80-95%
  → Fine-tuning. Look at comparison patterns, casting, operator selection,
    operand order. Run /compare-asm for instruction-by-instruction diff.

Match% 95-99%
  → Last-mile work. Check bool materialization, register hoisting, function
    definition order, fcmpu operand order. Use /stack-layout if offset mismatches
    dominate.

Match% 99%+ but not 100%
  → Often an ICF/LINKER_MERGED artifact or unfixable scheduler artifact. Verify
    via objdiff's symbol address before marking AT_LIMIT.
```

## Cross-Project Reference

DC3's catalog at `/home/free/code/milohax/dc3-decomp/docs/decomp/patterns/` has many MSVC-specific patterns that don't apply to MWCC, but the categorization is mostly portable. When a new RB3 pattern is discovered:

1. Check DC3's catalog for a matching MSVC analogue — the mechanism is often parallel even when the codegen surface differs.
2. File it under the matching `fixable-*.md` here. If it doesn't fit any existing file and a similar DC3 file exists, prefer creating an RB3 file with the same name over inventing a new schema.
