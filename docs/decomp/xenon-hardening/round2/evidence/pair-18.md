# Pair 18 — verification evidence

**Claimed identity:** Wii `SetLoadedPrefabChar__8BandUserFi`  ==  Xenon `0x8266ecb0`

| field | value |
|---|---|
| pair_id | 18 |
| stratum | BSIM 15-20 |
| match_type | `BSIM` |
| BSim sim×conf | 16.152 |
| BSim similarity / confidence | 1.0 / 16.152 |
| TU (Wii) | `BandUser.o` |
| Wii symbol (demangled) | `BandUser::SetLoadedPrefabChar(...)` |
| Wii addr (Bank 8) | `0x80162c30` |
| Xenon addr | `0x8266ecb0` |
| Xenon func name | `Function_8266ECB0` (stripped binary — name is auto-generated) |
| Wii body size | 22 asm lines (lines 1207-1227 in `build/SZBE69_B8/asm/band3/game/BandUser.s`) |
| Xenon body size | 76 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (3 total, 3 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x8266eaf8` | `Function_8266EAF8` | `SetChar__8BandUserFP8CharData` |
| `0x82540840` | `FUN_82540840` | `GetPrefabMgr__9PrefabMgrFv` |
| `0x826fbef0` | `FUN_826fbef0` | `TrackName__8SongDataCFi` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_8266ECB0(undefined8 param_1,undefined8 param_2)

{
  undefined8 uVar1;
  
  uVar1 = FUN_82540840();
  uVar1 = FUN_826fbef0(uVar1,param_2);
  Function_8266EAF8(param_1,uVar1);
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
CharData *GetDefaultPrefab__9PrefabMgrCFi(PrefabMgr *this, s32 arg0); /* extern */
PrefabMgr *GetPrefabMgr__9PrefabMgrFv(PrefabMgr *this); /* extern */
? SetChar__8BandUserFP8CharData(BandUser *this, CharData *arg0); /* static */

/* BandUser::SetLoadedPrefabChar (int) */
void SetLoadedPrefabChar__8BandUserFi(BandUser *this, s32 arg0) {
    SetChar__8BandUserFP8CharData(this, GetDefaultPrefab__9PrefabMgrCFi(GetPrefabMgr__9PrefabMgrFv((PrefabMgr *) this), arg0));
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# BandUser::SetLoadedPrefabChar(int)
.fn SetLoadedPrefabChar__8BandUserFi, global
/* 80162C30 00157650  94 21 FF F0 */	stwu r1, -0x10(r1)
/* 80162C34 00157654  7C 08 02 A6 */	mflr r0
/* 80162C38 00157658  90 01 00 14 */	stw r0, 0x14(r1)
/* 80162C3C 0015765C  93 E1 00 0C */	stw r31, 0xc(r1)
/* 80162C40 00157660  7C 9F 23 78 */	mr r31, r4
/* 80162C44 00157664  93 C1 00 08 */	stw r30, 0x8(r1)
/* 80162C48 00157668  7C 7E 1B 78 */	mr r30, r3
/* 80162C4C 0015766C  48 1E 22 C5 */	bl GetPrefabMgr__9PrefabMgrFv
/* 80162C50 00157670  7F E4 FB 78 */	mr r4, r31
/* 80162C54 00157674  48 1E 3C 9D */	bl GetDefaultPrefab__9PrefabMgrCFi
/* 80162C58 00157678  7C 64 1B 78 */	mr r4, r3
/* 80162C5C 0015767C  7F C3 F3 78 */	mr r3, r30
/* 80162C60 00157680  4B FF FD E1 */	bl SetChar__8BandUserFP8CharData
/* 80162C64 00157684  80 01 00 14 */	lwz r0, 0x14(r1)
/* 80162C68 00157688  83 E1 00 0C */	lwz r31, 0xc(r1)
/* 80162C6C 0015768C  83 C1 00 08 */	lwz r30, 0x8(r1)
/* 80162C70 00157690  7C 08 03 A6 */	mtlr r0
/* 80162C74 00157694  38 21 00 10 */	addi r1, r1, 0x10
/* 80162C78 00157698  4E 80 00 20 */	blr
.endfn SetLoadedPrefabChar__8BandUserFi
```
