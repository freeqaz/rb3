# Pair 22 — verification evidence

**Claimed identity:** Wii `SetPrimaryMetaScore__16LocalBandMachineFi`  ==  Xenon `0x825a8520`

| field | value |
|---|---|
| pair_id | 22 |
| stratum | ExactInstr |
| match_type | `ExactInstructionsFunctionHasher` |
| BSim sim×conf | n/a (non-BSim) |
| TU (Wii) | `BandMachine.o` |
| Wii symbol (demangled) | `LocalBandMachine::SetPrimaryMetaScore(...)` |
| Wii addr (Bank 8) | `0x802d6050` |
| Xenon addr | `0x825a8520` |
| Xenon func name | `FUN_825a8520` (stripped binary — name is auto-generated) |
| Wii body size | 11 asm lines (lines 1135-1144 in `build/SZBE69_B8/asm/band3/meta_band/BandMachine.s`) |
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

## Xenon callees (1 total, 1 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x825aa600` | `Function_825AA600` | `SyncLocalMachine__14BandMachineMgrFUc` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void FUN_825a8520(int param_1,int param_2)

{
  if (param_2 == *(int *)(param_1 + 0x6c)) {
    return;
  }
  *(int *)(param_1 + 0x6c) = param_2;
  Function_825AA600(*(undefined4 *)(param_1 + 0x70),4);
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct LocalBandMachine {
    /* 0x00 */ char pad0[0x78];
    /* 0x78 */ s32 unk78;                           /* inferred */
    /* 0x7C */ LocalBandMachine *unk7C;             /* inferred */
} LocalBandMachine;                                 /* size >= 0x80 */

/* LocalBandMachine::SetPrimaryMetaScore (int) */
LocalBandMachine *SetPrimaryMetaScore__16LocalBandMachineFi(LocalBandMachine *this, s32 arg0) {
    if (arg0 != (s32) this->unk78) {
        this->unk78 = arg0;
        return this->unk7C;
    }
    return this;
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# LocalBandMachine::SetPrimaryMetaScore(int)
.fn SetPrimaryMetaScore__16LocalBandMachineFi, global
/* 802D6050 002CAA70  80 03 00 78 */	lwz r0, 0x78(r3)
/* 802D6054 002CAA74  7C 04 00 00 */	cmpw r4, r0
/* 802D6058 002CAA78  4D 82 00 20 */	beqlr
/* 802D605C 002CAA7C  90 83 00 78 */	stw r4, 0x78(r3)
/* 802D6060 002CAA80  38 80 00 04 */	li r4, 0x4
/* 802D6064 002CAA84  80 63 00 7C */	lwz r3, 0x7c(r3)
/* 802D6068 002CAA88  48 00 1E 58 */	b SyncLocalMachine__14BandMachineMgrFUc
/* 802D606C 002CAA8C  4E 80 00 20 */	blr
.endfn SetPrimaryMetaScore__16LocalBandMachineFi
```
