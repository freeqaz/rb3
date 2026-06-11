# Pair 30 — verification evidence

**Claimed identity:** Wii `GetRankData__11SingerStatsCFi`  ==  Xenon `0x826798b0`

| field | value |
|---|---|
| pair_id | 30 |
| stratum | Implied |
| match_type | `Implied Match` |
| BSim sim×conf | n/a (non-BSim) |
| TU (Wii) | `Stats.o` |
| Wii symbol (demangled) | `SingerStats const::GetRankData(...)` |
| Wii addr (Bank 8) | `0x801e68a0` |
| Xenon addr | `0x826798b0` |
| Xenon func name | `FUN_826798b0` (stripped binary — name is auto-generated) |
| Wii body size | 7 asm lines (lines 4701-4706 in `build/SZBE69_B8/asm/band3/game/Stats.s`) |
| Xenon body size | 16 bytes |

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

longlong FUN_826798b0(uint *param_1,ulonglong param_2)

{
  return (param_2 & 0x1fffffff) * 8 + (ulonglong)*param_1;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct SingerStats {
    /* 0x0 */ s32 unk0;                             /* inferred */
} SingerStats;                                      /* size >= 0x4 */

/* SingerStats::GetRankData (int) const */
s32 GetRankData__11SingerStatsCFi(SingerStats *this, s32 arg0) {
    return this->unk0 + (arg0 * 8);
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# SingerStats::GetRankData(int) const
.fn GetRankData__11SingerStatsCFi, global
/* 801E68A0 001DB2C0  80 63 00 00 */	lwz r3, 0x0(r3)
/* 801E68A4 001DB2C4  54 80 18 38 */	slwi r0, r4, 3
/* 801E68A8 001DB2C8  7C 63 02 14 */	add r3, r3, r0
/* 801E68AC 001DB2CC  4E 80 00 20 */	blr
.endfn GetRankData__11SingerStatsCFi
```
