# Pair 23 — verification evidence

**Claimed identity:** Wii `PitchNote__5LyricCFv`  ==  Xenon `0x82b7d078`

| field | value |
|---|---|
| pair_id | 23 |
| stratum | ExactInstr |
| match_type | `ExactInstructionsFunctionHasher` |
| BSim sim×conf | n/a (non-BSim) |
| TU (Wii) | `Lyric.o` |
| Wii symbol (demangled) | `Lyric const::PitchNote(...)` |
| Wii addr (Bank 8) | `0x80142cf0` |
| Xenon addr | `0x82b7d078` |
| Xenon func name | `FUN_82b7d078` (stripped binary — name is auto-generated) |
| Wii body size | 9 asm lines (lines 1357-1364 in `build/SZBE69_B8/asm/band3/bandtrack/Lyric.s`) |
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

bool FUN_82b7d078(int param_1)

{
  return *(char *)(**(int **)(param_1 + 0x3c) + 0x2a) == '\0';
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct Lyric {
    /* 0x00 */ char pad0[0x30];
    /* 0x30 */ void **unk30;                        /* inferred */
} Lyric;                                            /* size >= 0x34 */

/* Lyric::PitchNote (void) const */
s32 PitchNote__5LyricCFv(Lyric *this) {
    return (*this->unk30)->unk2A == 0;
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# Lyric::PitchNote() const
.fn PitchNote__5LyricCFv, global
/* 80142CF0 00137710  80 63 00 30 */	lwz r3, 0x30(r3)
/* 80142CF4 00137714  80 63 00 00 */	lwz r3, 0x0(r3)
/* 80142CF8 00137718  88 03 00 2A */	lbz r0, 0x2a(r3)
/* 80142CFC 0013771C  7C 00 00 34 */	cntlzw r0, r0
/* 80142D00 00137720  54 03 D9 7E */	srwi r3, r0, 5
/* 80142D04 00137724  4E 80 00 20 */	blr
.endfn PitchNote__5LyricCFv
```
