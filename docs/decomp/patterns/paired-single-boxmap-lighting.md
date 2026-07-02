# Paired-Single BoxMapLighting Kernel

## Target

The three `BoxMapLighting::ApplyLight` overloads, e.g.:

```text
ApplyLight__14BoxMapLightingCFPQ23Hmx5ColorRC54BoxLightArray<Q214BoxMapLighting16LightParams_Spot,50>RC7Vector3
BoxMapLighting::ApplyLight(Hmx::Color *, const BoxLightArray<LightParams_Spot, 50> &, const Vector3 &) const
```

Unit: `main/system/rndobj/BoxMap`. Target size for the spot-array overload:
`0x2f8` bytes.

`run_objdiff` reports 0% / 32.4% / 45.4% for the three overloads because the
targets are **hand-written Gekko paired-single lighting kernels**, while our
source compiles as ordinary scalar C++. This is not a scheduling gap the permuter
can close — see the proof below.

## PROVEN: the Bank-8 ApplyLight bodies are hand-written paired-single asm

This is no longer a hypothesis. The Bank-5 DWARF carries the smoking gun (each
item independently verified in the `f1-boxmap-applylight` sweep pass, landed as
`79c0845d`):

1. **The dual-body globals.** The `BoxMap.cpp` CU has two globals
   `g_testApplyLightWiiAsm` (decl_line 14) and `g_testLightRefactor` (line 15)
   — the classic Harmonix asm/C toggle pair (`/tmp/b5_dwarf.txt` @
   `<4ba0b0>`/`<4ba0d6>`).
2. **The kernel has two sibling bodies.** The Bank-5 kernel
   `ApplyLight(Hmx::Color*, const Vector3&, const Hmx::Color&) const`
   (`0x809b0660`, decl_line 168) contains **two sibling lexical blocks**:
   - an **asm-path** block whose only locals are `float* pColor` / `pDirection` /
     `pResult` (lines 181–183) — the exact operand-setup idiom of the landed
     inline asm in `src/system/math/Vec.h` `Distance`;
   - a **pure-scalar C** block with `fWeight_{XP,XN,YP,YN,ZP,ZN}_Sqr`,
     `fSrc_R/G/B`, `fRes_{XP..ZN}_{R,G,B}` locals (lines 289–327) — fully
     unrolled, RGB only, no alpha.

   i.e. the original source is `if (g_testApplyLightWiiAsm) { asm } else { C }`.
   **Bank 8 kept only the asm path** (Bank-5 kernel = 1220B two-path; Bank 8 =
   312B asm-only). `bank_divergence.py` classifies it **MISLEADING** (m_ratio
   0.41) — the Bank-5 body is the wrong era to read as the target.

## Compile-probe results (mwcceppc 4.3.172)

Same flags as the BoxMap build rule, scratch TUs, disassembled with `dtk`. The
binary's strings tables show **no `__PS_*` intrinsic functions** — only the
inline-assembler mnemonic tables and the `__vec2x32float__` type.

**Expressible from C++** (`typedef __vec2x32float__ psq;`):

| Form | C++ spelling |
|---|---|
| `psq_l` / `psq_st` | deref/copy of a `psq`, incl. a cast `float*` and immediate offsets |
| `psq_lx` / `psq_stx` | indexed `psq` load/store |
| `ps_add` / `ps_sub` / `ps_mul` / `ps_div` | the arithmetic operators on `psq` |
| `ps_madd` | `*a * s + *c` — **only** under `#pragma fp_contract on` |
| `ps_merge00 x,x` | float→pair broadcast (same source twice) |

**NOT expressible from any C++ spelling** (must be inline asm):

- two-source `ps_merge00` / `ps_merge10` / `ps_merge11` (swizzle from two pairs)
- `ps_sel` (lane select)
- `ps_madds0` / `ps_madds1`, `ps_muls0` / `ps_muls1` (scalar-lane multiply-add)
- `ps_sum0` / `ps_sum1` (cross-lane reduction)
- `ps_neg` — **unary minus on the paired type ICEs the compiler**
  (`PCodeUtilities.c:974`)
- non-`qr0` quantized loads (`qr1` etc.)
- `__vec2x32float__` has no aggregate initializer (`{a,b}` → error 10174) and no
  element indexing (`pair[0]` → error 10377).

## The targets require exactly the inexpressible forms

Per-overload, the required-but-inexpressible operations:

- **Directional**: 10× two-source `ps_merge`, 3× `ps_sel`, 4× `ps_madds0/1`,
  plus the hand idiom `ps_sub f0,f0,f0` (zero-from-garbage; the compiler would
  instead load `0.0f` from the pool).
- **Spot-array loop** (all of the above, plus): `qr1` quantized loads,
  `frsqrte` + a manual Newton refinement, scalar `fsel` clamps, `ps_sum0/1`
  distance reductions, all 12 face-color accumulator pairs kept register-resident
  across the loop (saves `f14`–`f31`), and a `psq_l 0x18` read that **straddles a
  struct boundary** (`{mColor.alpha, mTipPosition.x}`) — pure hand-coding.

Register allocation also **differs between the standalone Directional copy and
its inlined copies** in Point/array, which is the fingerprint of the original
using **symbolic-register asm** (`register __vec2x32float__` + `asm {}` inside a
macro or inline function, compiler-allocated per site) — the established Harmonix
idiom already visible in `src/system/bandobj/InlineHelp.cpp`,
`src/system/math/Vec.h`, `src/system/math/Rot.cpp`, and `src/system/rndobj/Env.cpp`.

## Verdict: source-immune `at_limit` by construction

Matching these three bodies from portable C++ is **impossible by construction**:
no reordering of scalar C++ changes the opcode *class* of ~70% of the
instructions. This is source-immune `at_limit` (NOT permuter-class), recorded in
`decomp.db`. Mark the three ApplyLight symbols so future sweeps skip them (they
otherwise keep surfacing as "0% / 32% / 45% big fish").

### Refuted attempts (do not repeat)

- **Shared-inline restructure** (one `static inline ApplyLightToFaces` called by
  all three, faithful to the target's inlined-kernel structure and Bank 5's
  kernel signature): compiles, is *more* faithful, but **scores worse** — Point
  45.4 → 28.4 (the compact `bl` aligns better against the PS tail than ~45 scalar
  instrs; the fuzzy metric punishes the insert/delete storm). Reverted.
- **`CacheData` hand-permutes**: two principled reorders toward the target op
  order scored 91.66 and 89.65 vs a 92.02 baseline — **nonmonotonic**, classic
  permuter-class. Restore the baseline text; if anyone grinds it, use the source
  permuter, not hands.
- **Honest C-level `__vec2x32float__` kernel** (only the expressible ops):
  abandoned pre-implementation — without merges/sel/madds the dataflow must be
  restructured so radically that any alignment gain is speculative (~50%
  ceiling), while requiring `#ifdef __MWERKS__` gating plus a native scalar
  duplicate. Bad trade for the port.

## The 0.28f-vs-1/255 finding

The previously-rejected `0.28f` epsilon was **half right**. Two distinct skip
constants coexist in the target pool:

| Constant | Big-endian word | Value | Display name | Where |
|---|---|---|---|---|
| Spot-array loop skip test (`r+g+b <`) | `0x3e8f5c29` | **`0.28f`** | `@F_295c8f3e` | skip spots dimmer than sum 0.28 |
| `CacheData` per-channel test | `0x3b808081` | **`1/255`** | `@F_8180803b` | per-channel epsilon |

The prior agent's `0.28f` was correct **for the array function**; the sin would
have been touching `CacheData`'s `1/255` (which is intact). `79c0845d` landed
`static const float kColorEpsilon = 0.28f;` (fn-local, with a pool-provenance
comment) as a **behavioral fidelity fix for the native port** — retail skips
spots whose channel sum is below 0.28, and our old code processed them.

> **This replaces the stale claim** in prior revisions of this doc that "the
> current C++ uses `0.003921569f`." The array loop's cutoff is `0.28f`
> (`0x3e8f5c29`); `1/255` (`0.003921569f`, `0x3b808081`) is `CacheData`'s
> separate per-channel constant, not the array skip test.

## DC3 is not a drop-in reference

DC3 has a related `BoxMapLighting` but it is structurally different: DC3 queues
light directions/colors into temporary global buffers, then `ApplyQueuedLights`
folds those buffers into the output colors. RB3 Bank 8 has the spot overload as a
direct paired-single output-color kernel. Use DC3 for naming and broad intent,
not for this code shape.

## Standing-rule note

Documenting the asm provenance is required and fine. **Transcribing the disasm
into an `ASM_BLOCK` remains a REJECTED fake match** (see
[at-limit-mwcc.md](at-limit-mwcc.md); the standing no-fake-asm rule). If the
project ever *relaxes* the no-asm rule specifically for provably-asm-original
functions — the `Vec.h` / `Rot.cpp` / `Env.cpp` inline-asm precedent already
exists in-tree — **this family is the top candidate**: +1712 target bytes across
three functions, and the Bank-5 DWARF hands you the exact operand setup
(`pColor`/`pDirection`/`pResult`).

## See also

- [at-limit-mwcc.md](at-limit-mwcc.md) — source-immune vs permuter-class triage.
- [fixable-fsel-fma.md](fixable-fsel-fma.md) — the *expressible* float
  scheduling controls (`fp_contract`, manual Cross expansion) that this kernel is
  beyond.
