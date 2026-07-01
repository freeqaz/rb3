# Pair 24 — verification evidence

**Claimed identity:** Wii `GlobalOptionsNeedsSave__10ProfileMgrFv`  ==  Xenon `0x82532198`

| field | value |
|---|---|
| pair_id | 24 |
| stratum | ExactInstr |
| match_type | `ExactInstructionsFunctionHasher` |
| BSim sim×conf | n/a (non-BSim) |
| TU (Wii) | `ProfileMgr.o` |
| Wii symbol (demangled) | `ProfileMgr::GlobalOptionsNeedsSave(...)` |
| Wii addr (Bank 8) | `0x8034aa10` |
| Xenon addr | `0x82532198` |
| Xenon func name | `FUN_82532198` (stripped binary — name is auto-generated) |
| Wii body size | 11 asm lines (lines 2342-2351 in `build/SZBE69_B8/asm/band3/meta_band/ProfileMgr.s`) |
| Xenon body size | 28 bytes |

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

undefined1 FUN_82532198(int param_1)

{
  if (*(int *)(param_1 + 0x34) != 1) {
    return 0;
  }
  return *(undefined1 *)(param_1 + 0x38);
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct ProfileMgr {
    /* 0x000 */ char pad0[0x554];
    /* 0x554 */ s32 unk554;                         /* inferred */
    /* 0x558 */ u8 unk558;                          /* inferred */
} ProfileMgr;                                       /* size >= 0x559 */

/* ProfileMgr::GlobalOptionsNeedsSave (void) */
u8 GlobalOptionsNeedsSave__10ProfileMgrFv(ProfileMgr *this) {
    if ((s32) this->unk554 != 1) {
        return 0U;
    }
    return this->unk558;
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# ProfileMgr::GlobalOptionsNeedsSave()
.fn GlobalOptionsNeedsSave__10ProfileMgrFv, global
/* 8034AA10 0033F430  80 03 05 54 */	lwz r0, 0x554(r3)
/* 8034AA14 0033F434  2C 00 00 01 */	cmpwi r0, 0x1
/* 8034AA18 0033F438  41 82 00 0C */	beq .L_8034AA24
/* 8034AA1C 0033F43C  38 60 00 00 */	li r3, 0x0
/* 8034AA20 0033F440  4E 80 00 20 */	blr
.L_8034AA24:
/* 8034AA24 0033F444  88 63 05 58 */	lbz r3, 0x558(r3)
/* 8034AA28 0033F448  4E 80 00 20 */	blr
.endfn GlobalOptionsNeedsSave__10ProfileMgrFv
```
