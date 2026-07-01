# Pair 25 — verification evidence

**Claimed identity:** Wii `GetFrameMatchType__6SingerFv`  ==  Xenon `0x826d99b0`

| field | value |
|---|---|
| pair_id | 25 |
| stratum | ExactInstr |
| match_type | `ExactInstructionsFunctionHasher` |
| BSim sim×conf | n/a (non-BSim) |
| TU (Wii) | `Singer.o` |
| Wii symbol (demangled) | `Singer::GetFrameMatchType(...)` |
| Wii addr (Bank 8) | `0x801d96b0` |
| Xenon addr | `0x826d99b0` |
| Xenon func name | `FUN_826d99b0` (stripped binary — name is auto-generated) |
| Wii body size | 15 asm lines (lines 3823-3836 in `build/SZBE69_B8/asm/band3/game/Singer.s`) |
| Xenon body size | 44 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (0 total, 0 resolved to a matched Wii symbol)

_(no direct callees — leaf function)_

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

undefined4 FUN_826d99b0(int *param_1)

{
  if (param_1[0x1c] != -1) {
    return *(undefined4 *)(*(int *)(*(int *)(*param_1 + 0x390) + param_1[0x1c] * 4) + 0x9c);
  }
  return 4;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct Singer {
    /* 0x00 */ void *unk0;                          /* inferred */
    /* 0x04 */ char pad4[0x6C];                     /* maybe part of unk0[0x1C]? */
    /* 0x70 */ s32 unk70;                           /* inferred */
} Singer;                                           /* size >= 0x74 */

/* Singer::GetFrameMatchType (void) */
s32 GetFrameMatchType__6SingerFv(Singer *this) {
    s32 temp_r0;

    temp_r0 = this->unk70;
    if (temp_r0 != -1) {
        return (*(this->unk0->unk358 + (temp_r0 * 4)))->unk98;
    }
    return 4;
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# Singer::GetFrameMatchType()
.fn GetFrameMatchType__6SingerFv, global
/* 801D96B0 001CE0D0  80 03 00 70 */	lwz r0, 0x70(r3)
/* 801D96B4 001CE0D4  2C 00 FF FF */	cmpwi r0, -0x1
/* 801D96B8 001CE0D8  41 82 00 1C */	beq .L_801D96D4
/* 801D96BC 001CE0DC  80 63 00 00 */	lwz r3, 0x0(r3)
/* 801D96C0 001CE0E0  54 00 10 3A */	slwi r0, r0, 2
/* 801D96C4 001CE0E4  80 63 03 58 */	lwz r3, 0x358(r3)
/* 801D96C8 001CE0E8  7C 63 00 2E */	lwzx r3, r3, r0
/* 801D96CC 001CE0EC  80 63 00 98 */	lwz r3, 0x98(r3)
/* 801D96D0 001CE0F0  4E 80 00 20 */	blr
.L_801D96D4:
/* 801D96D4 001CE0F4  38 60 00 04 */	li r3, 0x4
/* 801D96D8 001CE0F8  4E 80 00 20 */	blr
.endfn GetFrameMatchType__6SingerFv
```
