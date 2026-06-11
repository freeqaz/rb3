# Pair 26 — verification evidence

**Claimed identity:** Wii `TambourineGems__17TambourineManagerCFv`  ==  Xenon `0x826dbaa8`

| field | value |
|---|---|
| pair_id | 26 |
| stratum | ExactInstr |
| match_type | `ExactInstructionsFunctionHasher` |
| BSim sim×conf | n/a (non-BSim) |
| TU (Wii) | `TambourineManager.o` |
| Wii symbol (demangled) | `TambourineManager const::TambourineGems(...)` |
| Wii addr (Bank 8) | `0x801ebda0` |
| Xenon addr | `0x826dbaa8` |
| Xenon func name | `FUN_826dbaa8` (stripped binary — name is auto-generated) |
| Wii body size | 9 asm lines (lines 643-650 in `build/SZBE69_B8/asm/band3/game/TambourineManager.s`) |
| Xenon body size | 24 bytes |

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

longlong FUN_826dbaa8(int param_1)

{
  return (ulonglong)*(uint *)(**(int **)(*(int *)(param_1 + 0x28) + 0x390) + 8) + 0x24;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct TambourineManager {
    /* 0x00 */ char pad0[0x1C];
    /* 0x1C */ void *unk1C;                         /* inferred */
} TambourineManager;                                /* size >= 0x20 */

/* TambourineManager::TambourineGems (void) const */
s32 TambourineGems__17TambourineManagerCFv(TambourineManager *this) {
    return (*this->unk1C->unk358)->unk8 + 0x18;
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# TambourineManager::TambourineGems() const
.fn TambourineGems__17TambourineManagerCFv, global
/* 801EBDA0 001E07C0  80 63 00 1C */	lwz r3, 0x1c(r3)
/* 801EBDA4 001E07C4  80 63 03 58 */	lwz r3, 0x358(r3)
/* 801EBDA8 001E07C8  80 63 00 00 */	lwz r3, 0x0(r3)
/* 801EBDAC 001E07CC  80 63 00 08 */	lwz r3, 0x8(r3)
/* 801EBDB0 001E07D0  38 63 00 18 */	addi r3, r3, 0x18
/* 801EBDB4 001E07D4  4E 80 00 20 */	blr
.endfn TambourineGems__17TambourineManagerCFv
```
