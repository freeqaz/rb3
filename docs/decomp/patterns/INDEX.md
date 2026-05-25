# RB3 Pattern Reference Index

Quick reference for the MetroWorks CodeWarrior codegen patterns documented from RB3 decomp work. Targets the Wii build (`mwcceppc 4.3.172`, `-O4,p -inline noauto -ipa file -sdata 2 -sdata2 2`).

This index mirrors the structure of [DC3's pattern catalog](../../../../dc3-decomp/docs/decomp/patterns/INDEX.md) so cross-references work both ways between the two projects. Filenames match where the categories overlap; per-file content is MWCC-specific for RB3.

## Pattern Files

| File | Topic |
|------|-------|
| [fixable-declarations.md](fixable-declarations.md) | Declaration order, register hoisting, pre-load before loop, function definition order, `CopyFrom` vs `operator=`, `return *this;`, stlport accessor inlining, `__less<T>` specialization, inline container helpers, `ObjPtr<T>.mPtr` direct access, struct-copy → field-access, pre-declare-for-callee-saved, pre-loop iterator hoist, pointer-select after function call |
| [fixable-bool-mask.md](fixable-bool-mask.md) | Bool materialization (`IsLocal()` vs `!IsNet()`, compound bools, condition inversion) |
| [fixable-control-flow.md](fixable-control-flow.md) | `switch` vs if/else, early return inversion, `do-while` vs `while`, loop unrolling, nested scopes, STL `__find` / `vector::erase` patterns, early-return AutoTimer dtor dedup, split int addition, mid-loop break, pair-local stack materialization, restore missing MILO_ASSERTs |
| [fixable-fsel-fma.md](fixable-fsel-fma.md) | Float expression splitting, `fmadds` scheduling, `#pragma fp_contract on/off`, manual Cross expansion as scheduling barrier |
| [fixable-casting.md](fixable-casting.md) | `(float)x` vs `(float)(long long)x`, `(int)` for arithmetic shift, enum return type, float truthiness `fcmpu` ordering, packed-alpha `(float)(int)((unsigned)x>>24)`, `x % 4` rotate trick, `unsigned long` for `mulhwu` magic constant, `unsigned int` for `cmplw` |
| [fixable-comparison.md](fixable-comparison.md) | `!=` vs `<` for loop comparisons |
| [fixable-operators.md](fixable-operators.md) | Arg eval order (right-to-left), commutative `fmuls` operand order, `.Set()` vs constructor assignment, `std::max` literal-first quirk, `lwzu` via reference-cast auto-increment, `Symbol::operator==(const char*)` vs `streq`, `!streq` vs `if (strcmp)`, MILO_WARN arg order affects `MakeString<>` template, format-string tweaks shift string pool, splitting BinStream `>>` chains |
| [fixable-macros.md](fixable-macros.md) | `#pragma pool_data`, `#pragma dont_inline`, static `Message` guards, `__declspec(noinline)`, per-file `#pragma ipa on`, TU-local conditional inline macros, `_Temporary_buffer<T*, T>` allocator specialization (sort-template cascade unlock), comparator template specialization for `__introsort_loop` / heap sorts |
| [fixable-struct-layout.md](fixable-struct-layout.md) | `virtual` vs non-`virtual` vtable layout; redundant virtual override declaration (link-time undefined symbol) |
| [fixable-copy-ctor.md](fixable-copy-ctor.md) | Explicit copy constructor blocks small-struct return ABI; TU-local inline `Hmx::Object` copy ctor; member function pointer to force out-of-line emission |
| [verifiable-icf.md](verifiable-icf.md) | Linker ICF risks — verify before treating as unfixable |
| [quazal-ddl-pattern.md](quazal-ddl-pattern.md) | `_DDL_X::Extract`/`::Add` are static members — porting MISSING Quazal DDL units to Matching |
| [wave-session-2026-05-18.md](wave-session-2026-05-18.md) | Session notes — param-slot reuse, `bool`→`int` returns, double-precision lowering, .cpp ordering controls inlining, MemcardMgr bitfield+dead-null pattern, header-edit blockers |
| [wave-session-2026-05-23.md](wave-session-2026-05-23.md) | Session notes — `_Temporary_buffer` allocator specialization (sort-template cascade), `lwzu` reference-cast idiom, packed-alpha cast sequence, `ObjPtr.mPtr`, struct→fields, `_vector_sized.c` reserve A/B dead-end, recurring header blockers (Mtx.h, Timer.h) |

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
