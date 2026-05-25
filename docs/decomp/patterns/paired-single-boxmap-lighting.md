# Paired-Single BoxMapLighting Kernel

## Target

Function:

```text
ApplyLight__14BoxMapLightingCFPQ23Hmx5ColorRC54BoxLightArray<Q214BoxMapLighting16LightParams_Spot,50>RC7Vector3
BoxMapLighting::ApplyLight(Hmx::Color *, const BoxLightArray<LightParams_Spot, 50> &, const Vector3 &) const
```

Unit: `main/system/rndobj/BoxMap`

Target size: `0x2f8` bytes.

Current scalar source is a semantic approximation, not a binary-shaped
decompilation. `run_objdiff` reports 0% for this overload because the target is
a hand-shaped Gekko paired-single lighting kernel, while the source compiles as
ordinary scalar code with a `sqrt` call and a call to the directional overload.

## What The Target Does

The target implementation:

- Saves `f14` through `f31` and uses a `0x130` stack frame.
- Loads the six input `Hmx::Color` entries into paired-single accumulators.
- Iterates `arr.mArray` at stride `0x50`, with `mNumElements` at offset
  `0xfa0`.
- Skips a spot when `red + green + blue < 0.28f`.
- Computes direction and attenuation using paired-single math and `frsqrte`.
  There is no `sqrt` library call and no Newton refinement sequence in this
  function.
- Expands the directional box-map contribution inline instead of calling
  `ApplyLight(color, dl)`.
- Writes all six accumulated colors back with `psq_st`.

Representative target-only instructions:

```asm
psq_l      f4, 0x0(r6), 0, qr0
psq_l      f5, 0x8(r6), 0, qr1
ps_merge00 f8, f0, f2
ps_sum1    f27, f29, f28, f29
frsqrte    f6, f1
ps_sel     f25, f25, f25, f24
ps_madds0  f9, f3, f25, f9
ps_madds1  f13, f3, f1, f13
```

The current C++ does not have the same cutoff either: it uses
`0.003921569f`, while this target compares the RGB sum against `0.28f`.

## DC3 Is Not A Drop-In Reference

DC3 has a related `BoxMapLighting` implementation, but it is structurally
different. DC3 queues light directions and colors into temporary global buffers,
then `ApplyQueuedLights` folds those buffers into the output colors. RB3 Bank 8
has this spot overload as a direct paired-single output-color kernel.

Use DC3 for naming and broad intent, not for this exact code shape.

## Compiler Findings

The RB3 build already uses the relevant Gekko compiler settings:

```text
-proc gekko -fp hardware -fp_contract on -O4,p -inline noauto -ipa file
```

`mwcceppc -help` shows:

- `-vector` is Altivec vector support, not a Gekko paired-single auto-vectorizer.
- `-fp_contract` controls fused multiply-add generation and is already on here.
- `-use_fsel` can request scalar `fsel`, but it does not expose `ps_sel`.
- No compiler option was found that turns scalar `Vector3` or `Hmx::Color` math
  into this paired-single kernel.

The paired-single C++ type does work:

```cpp
typedef __vec2x32float__ psq;

void probe_square_sum(float *dst, const float *a, const float *b) {
    psq va = *(const psq *)a;
    psq vb = *(const psq *)b;
    psq sq = va * va;
    *(psq *)dst = sq + vb * vb;
}
```

This compiles to paired-single operations:

```asm
psq_lx f0, r0, r4, 0, qr0
psq_lx f1, r0, r5, 0, qr0
ps_mul f2, f0, f0
ps_mul f0, f1, f1
ps_add f0, f2, f0
psq_stx f0, r0, r3, 0, qr0
```

However, this is not enough for the BoxMap kernel. The compiler surface found
so far has these limits:

- `__vec2x32float__` has no aggregate initializer in this MWCC mode.
  `psq pair = { a, b };` gives error `10174 illegal initialization`.
- `__vec2x32float__` has no element indexing.
  `pair[0]` and `pair[1]` give error `10377 illegal operands`.
- Common-looking builtins are not available:
  `__builtin_ps_merge00`, `__builtin_ps_sum0`, and `__builtin_ps_sel` are
  undefined.
- Cast loads such as `*(const psq *)&v.z` emit ordinary `psq_l ..., qr0`, not
  the target's quantized single-lane load shapes.
- No pure C++ expression tested emitted `ps_merge00`, `ps_merge10`,
  `ps_merge11`, `ps_sum0`, `ps_sum1`, or `ps_sel` on demand.

Existing RB3 source uses inline asm for this exact missing surface area in
files such as `src/system/math/Rot.cpp`, `src/system/math/Vec.h`,
`src/system/rndobj/Part.cpp`, and several `src/system/rndwii/*` files.

## Practical Conclusion

A fully matching version of this overload without inline asm is unlikely with
the current MWCC toolchain. `__vec2x32float__` C++ can express paired arithmetic
and basic paired loads/stores, but this function needs explicit swizzles,
reductions, lane selects, reciprocal-square-root scheduling, and quantized load
forms that are not exposed as ordinary C++.

Reasonable future paths:

- Keep the scalar C++ as a readable nonmatching implementation.
- Rewrite with `__vec2x32float__` C++ only if the goal is partial code-shape
  improvement, not a final match.
- Use a small inline-asm or assembly kernel if exact matching becomes more
  important than avoiding asm.
- If adding a portability abstraction, wrap the paired-single asm behind a
  helper layer with portable scalar fallbacks, but treat that as inline asm for
  matching purposes.

Do not spend time trying `-vector on` or alternate optimization spelling as the
main fix. The missing part is not enabled by a known flag; it is the lack of a
C++ intrinsic surface for the target paired-single instructions.
