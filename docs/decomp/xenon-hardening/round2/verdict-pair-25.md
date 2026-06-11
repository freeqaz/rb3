# Verdict — Pair 25

**Claim:** Wii `GetFrameMatchType__6SingerFv` (`0x801d96b0`, Bank8) == Xenon `FUN_826d99b0` (`0x826d99b0`)
**Match type:** ExactInstructionsFunctionHasher (leaf, no callees, no strings)

## Verdict: CORRECT (confidence: high)

## Side-by-side

Wii target asm (build/SZBE69_B8/asm/band3/game/Singer.s:3823-3836, confirmed against build tree):
```
lwz r0, 0x70(r3)      ; this->field_0x70
cmpwi r0, -0x1        ; sentinel == -1 ?
beq  .L (return 4)
lwz r3, 0x0(r3)       ; this->field_0x00 (object ptr)
slwi r0, r0, 2        ; field_0x70 * 4
lwz r3, 0x358(r3)     ; obj->array_base @ 0x358
lwzx r3, r3, r0       ; array[field_0x70]
lwz r3, 0x98(r3)      ; element->field_0x98
blr
.L: li r3, 0x4 ; blr  ; return 4
```

Xenon pseudo-C (from evidence pack):
```c
undefined4 FUN_826d99b0(int *param_1) {
  if (param_1[0x1c] != -1)   // 0x1c*4 = byte 0x70 — SAME field
    return *(*(*(*param_1 + 0x390) + param_1[0x1c]*4) + 0x9c);
  return 4;                  // SAME sentinel constant
}
```

## Decisive evidence

Both functions share the entire distinctive skeleton:
1. **Same sentinel field at byte offset 0x70** (Wii `0x70(r3)`; Xenon `param_1[0x1c]` = 0x1c*4 = 0x70).
2. **Same sentinel value -1** with an early-out.
3. **Same default return constant `4`** on the -1 branch.
4. **Same triple-indirection array lookup**: deref `this`, add an array-base offset, index by `field*4`, then load a field of the element.
5. **Same arity** (single pointer = `this`), same leaf control flow (one branch, two returns).

The only differences are exactly the two predicted toolchain struct-layout offsets: array base 0x358 (Wii) vs 0x390 (Xenon), and final field 0x98 vs 0x9c. The substrate notes (caveat 1) explicitly instruct discounting such offset deltas. Everything that is toolchain-invariant — the 0x70 field, the -1 sentinel, the `4` default, the `field*4` index, and the indirection depth — agrees exactly. The body is also fully consistent with the demangled name `Singer::GetFrameMatchType()` (a tiny getter returning an enum/int "match type" with `4` as a no-match default). No callees/strings to corroborate, but the constant pool + field offset + control flow are far more distinctive than a generic stub.

## For the next agent
Strong leaf-function match; no follow-up needed. ExactInstructionsFunctionHasher fired correctly here despite cross-compiler offset differences because the pre-relocation instruction shape (sentinel check + indexed lookup + dual return) is identical.
