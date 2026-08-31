# RB3 Pattern Reference Index

Quick reference for the MetroWorks CodeWarrior codegen patterns documented from RB3 decomp work. Targets the Wii build (`mwcceppc 4.3.172`, `-O4,p -inline noauto -ipa file -sdata 2 -sdata2 2`).

This index mirrors the structure of [DC3's pattern catalog](../../../../dc3-decomp/docs/decomp/patterns/INDEX.md) so cross-references work both ways between the two projects. Filenames match where the categories overlap; per-file content is MWCC-specific for RB3.

## Read these first

| File | Topic |
|------|-------|
| [at-limit-mwcc.md](at-limit-mwcc.md) | **Source-immune vs permuter-class triage.** Which patterns can be hand-edited, which need the permuter, which are truly at-limit. Read before declaring any function "stuck". |
| [permuter-roi.md](permuter-roi.md) | **When to dispatch the source permuter.** Categories of mismatch the permuter cracks (regswaps, FPR scheduling, bool materialization, stack-slot inversion). Hand-edit alternatives are listed inline. ⚠ Its register-allocation-cascades section is **corrected** by `fixable-liveness.md`. |
| [fixable-liveness.md](fixable-liveness.md) | **Register swaps are symptoms, not causes.** Scoping moves stack slots; liveness and scheduling move registers. Carries RB3's own two measurements of that boundary (Wave E1's 11 declaration-axis zeroes; `harmful-avoid.md`'s −6.5% live-range lever), the ABI argument that makes the volatile/callee-saved split *decidable*, four candidate levers imported from DC3 and **labelled unverified on MWCC**, and the three-part floor-evidence standard. Read before opening a regswap residual. |
| [harmful-avoid.md](harmful-avoid.md) | Anti-patterns — source shapes that look reasonable but actively hurt match%. Read before writing new code in shared headers. |

## Pattern Files

| File | Topic |
|------|-------|
| [fixable-liveness.md](fixable-liveness.md) | Live-range shortening, call-through-the-cached-local, schedule-then-polarity, scope-into-block, name-the-call-argument-temporaries; the volatile-vs-callee-saved routing rule; the byte-identical routing signal; three-part floor evidence. **Provenance is labelled per claim** — MWCC-measured vs ABI consequence vs MSVC-measured-and-unverified-here. |
| [fixable-declarations.md](fixable-declarations.md) | Declaration order, register hoisting, pre-load before loop, function definition order, `CopyFrom` vs `operator=`, `return *this;`, stlport accessor inlining, `__less<T>` specialization, inline container helpers, `ObjPtr<T>.mPtr` direct access, struct-copy → field-access, pre-declare-for-callee-saved, pre-loop iterator hoist, pointer-select after function call |
| [fixable-bool-mask.md](fixable-bool-mask.md) | Bool materialization (`IsLocal()` vs `!IsNet()`, compound bools, condition inversion) |
| [fixable-control-flow.md](fixable-control-flow.md) | `switch` vs if/else, early return inversion, `do-while` vs `while`, loop unrolling, nested scopes, STL `__find` / `vector::erase` patterns, early-return AutoTimer dtor dedup, split int addition, mid-loop break, pair-local stack materialization, restore missing MILO_ASSERTs |
| [fixable-fsel-fma.md](fixable-fsel-fma.md) | Float expression splitting, `fmadds` scheduling, `#pragma fp_contract on/off`, manual Cross expansion as scheduling barrier |
| [fixable-casting.md](fixable-casting.md) | `(float)x` vs `(float)(long long)x`, `(int)` for arithmetic shift, enum return type, float truthiness `fcmpu` ordering, packed-alpha `(float)(int)((unsigned)x>>24)`, `x % 4` rotate trick, `unsigned long` for `mulhwu` magic constant, `unsigned int` for `cmplw` |
| [fixable-comparison.md](fixable-comparison.md) | `!=` vs `<` for loop comparisons |
| [fixable-operators.md](fixable-operators.md) | Arg eval order (right-to-left), commutative `fmuls` operand order, `.Set()` vs constructor assignment, `std::max` literal-first quirk, `lwzu` via reference-cast auto-increment, `Symbol::operator==(const char*)` vs `streq`, `!streq` vs `if (strcmp)`, MILO_WARN arg order affects `MakeString<>` template, format-string tweaks shift string pool, splitting BinStream `>>` chains |
| [fixable-macros.md](fixable-macros.md) | `#pragma pool_data`, `#pragma dont_inline`, static `Message` guards, `__declspec(noinline)`, per-file `#pragma ipa on`, TU-local conditional inline macros, `_Temporary_buffer<T*, T>` allocator specialization (sort-template cascade unlock), comparator template specialization for `__introsort_loop` / heap sorts, `MILO_ASSERT` `#cond` stringification pool-shift (extract inline expressions to locals + rename to share pool slots) |
| [fixable-struct-layout.md](fixable-struct-layout.md) | `virtual` vs non-`virtual` vtable layout; redundant virtual override declaration (link-time undefined symbol); redundant **non-virtual** member redeclaration (name-hiding "phantom stub" — silent no-op on native/web, e.g. `BandCharacter::NameToDrumVenue` drum-gear 404) |
| [fixable-copy-ctor.md](fixable-copy-ctor.md) | Explicit copy constructor blocks small-struct return ABI; TU-local inline `Hmx::Object` copy ctor; member function pointer to force out-of-line emission |
| [fixable-inline-boundary.md](fixable-inline-boundary.md) | Inlining decisions — `__declspec(noinline)`, `DECOMP_FORCEBLOCK`, qualified call to bypass vtable + force inline, TU-local conditional inline macros |
| [dwarf-flat-member-retype.md](dwarf-flat-member-retype.md) | Flat `float unkN` members that are really `Vector2`/`Vector3`/`Color`/`Transform` aggregates — the `lfs`/`stfs`-vs-`lwz`/`stw` struct-copy signature (block-copy vs member-copy), the Bank-5 DWARF retype workflow, and `scripts/analysis/flat_member_retype_scan.py`. Base rate: 1 true retype in ~35 structs — run the tool before hand-auditing. |
| [paired-single-boxmap-lighting.md](paired-single-boxmap-lighting.md) | `BoxMapLighting::ApplyLight` spot kernel — **PROVEN hand-written paired-single asm in the original** (Bank-5 dual-body `g_testApplyLightWiiAsm` globals); the expressible/inexpressible Gekko op lists (incl. `ps_neg` ICE); source-immune `at_limit` by construction; the `0.28f` vs `1/255` epsilon finding |
| [verifiable-icf.md](verifiable-icf.md) | Linker ICF — verifying when calls have been folded by the linker (vs a real source-level mismatch). |
| [quazal-ddl-pattern.md](quazal-ddl-pattern.md) | `_DDL_X::Extract`/`::Add` are static members — porting MISSING Quazal DDL units to Matching |
| [wave-session-2026-05-18.md](wave-session-2026-05-18.md) | Session notes — param-slot reuse, `bool`→`int` returns, double-precision lowering, .cpp ordering controls inlining, MemcardMgr bitfield+dead-null pattern, header-edit blockers |
| [wave-session-2026-05-23.md](wave-session-2026-05-23.md) | Session notes — `_Temporary_buffer` allocator specialization (sort-template cascade), `lwzu` reference-cast idiom, packed-alpha cast sequence, `ObjPtr.mPtr`, struct→fields, `_vector_sized.c` reserve A/B dead-end, recurring header blockers (Mtx.h, Timer.h) |

## Quick Decision Tree

```
Match% < 50%
  → Implementation is missing or wrong. Run /analyze-function and review Ghidra
    pseudo-C + m2c output. Check /dc3-pair for a sister-project reference.
    DO NOT run the permuter yet — it can only optimize a roughly-correct skeleton.

Match% 50-80%
  → Structural issues. Check control flow (switch vs if/else, early-return
    inversion) and variable declaration order. Hand-edit work, not permuter work.

Match% 80-95%
  → Fine-tuning. Look at comparison patterns, casting, operator selection,
    operand order. Run /compare-asm for instruction-by-instruction diff.
    Permuter starts paying off here for residual cascades.

Match% 95-99%
  → Last-mile work. Most remaining diffs are permuter-class: register-allocation
    cascades, FPR scheduling, bool materialization, stack-slot inversions.
    Run the source permuter (see permuter-roi.md) before hand-editing 10+ lines.
    Hand-edit fallbacks: bool materialization, register hoisting, function
    definition order, fcmpu operand order. Use /stack-layout for offset cascades.

Match% 99%+ but not 100%
  → Run objdiff with --verdict and read verdict.classification:
      AT_LIMIT + source-immune patterns only -> accept and document
        (ICF, anon-namespace hash, address-relocation noise — see at-limit-mwcc.md)
      MAYBE_FIXABLE + permuter-class patterns -> run the permuter first
      anything else -> hand-edit per the verdict's suggestions

  Do NOT mark AT_LIMIT until either (a) all detected patterns are source-immune,
  or (b) a full permuter sweep on the function returned 0 improvements.
```

## Decision shortcuts

| Question | Answer |
|----------|--------|
| What does "register swap" mean in the verdict? | A **symptom**. Route by register class first: volatile-only ⇒ scheduling; callee-saved or mixed ⇒ liveness across a call. See [fixable-liveness.md](fixable-liveness.md). Still worth a cheap permuter pass — [permuter-roi.md#register-allocation-cascades](permuter-roi.md#register-allocation-cascades) — but do **not** read a zero-gain declaration-axis sweep as floor evidence. |
| A permuter sweep returned zero improvements — is that a floor? | Only if the variants were **byte-identical** *and* they were on the right axis. Byte-identical on the declaration axis for a register residual means the axis is wrong. See [fixable-liveness.md](fixable-liveness.md#2-byte-identical-is-not-no-improvement--it-is-a-routing-signal). |
| Does the DC3/rb3-xenon "EH funclet score wobble" note apply here? | **No.** RB3 builds `-Cpp_exceptions off` and has zero `__unwind$`/`__catch$` symbols — the whole class is absent on Wii. |
| objdiff says "rarely_hand_fixable" — am I stuck? | No. Check the pattern type: source-immune = accept, anything else = run the permuter. See [at-limit-mwcc.md](at-limit-mwcc.md). |
| When to run the source permuter? | See [permuter-roi.md](permuter-roi.md). Short answer: high-match (≥85%) functions with regswaps, FPR/scheduling cascades, bool/clrlwi noise, or OFFSET_SWAP. |
| When to truly mark AT_LIMIT? | See [at-limit-mwcc.md#when-to-mark-at-limit](at-limit-mwcc.md#when-to-mark-at-limit). |
| objdiff shows a run of `lfs`/`stfs` vs `lwz`/`stw` replaces in a struct copy? | Run `scripts/analysis/flat_member_retype_scan.py` and see [dwarf-flat-member-retype.md](dwarf-flat-member-retype.md) — a flat `float unkN` block is probably a hidden `Vector2`/`Vector3`/`Color` aggregate; **retype from the Bank-5 DWARF, don't permute**. (`lwz`/`stw` on BOTH sides = already member-faithful → permuter-class.) |

## Measurement instruments

- [two-objdiff-entry-points-two-rulers.md](two-objdiff-entry-points-two-rulers.md) —
  **`objdiff-cli diff` and `objdiff-cli report generate` were two different
  rulers, and the per-function one read LOW.** The two entry points carry
  different hardcoded base configs; `ppc.calculatePoolRelocations` synthesizes
  "fake" pool relocations that a dtk-carved target obj and an mwcc base obj do
  not reconstruct identically, and `reloc_eq` charges the target-only ones under
  every ruler except `none`. **151 functions / 224,892 bytes** on this binary —
  the largest of the three sibling repos — always with `diff` low; two of them
  read exactly 100.0 in `report.json`. Fixed by pinning all four divergent keys
  in `objdiff.json`'s `options`; guard `scripts/verify_ruler_agreement.py`,
  wired into the build ahead of `REPORT`.

## Cross-Project Reference

DC3's catalog at `/home/free/code/milohax/dc3-decomp/docs/decomp/patterns/` has many MSVC-specific patterns that don't apply to MWCC, but the categorization is mostly portable. When a new RB3 pattern is discovered:

1. Check DC3's catalog for a matching MSVC analogue — the mechanism is often parallel even when the codegen surface differs.
2. File it under the matching `fixable-*.md` here. If it doesn't fit any existing file and a similar DC3 file exists, prefer creating an RB3 file with the same name over inventing a new schema.
