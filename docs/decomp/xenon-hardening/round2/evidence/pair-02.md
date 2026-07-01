# Pair 02 — verification evidence

**Claimed identity:** Wii `SaveForEndGame__5StatsCFR9BinStream`  ==  Xenon `0x82679b40`

| field | value |
|---|---|
| pair_id | 02 |
| stratum | BSIM>=30 |
| match_type | `BSIM` |
| BSim sim×conf | 41.09 |
| BSim similarity / confidence | 1.0 / 41.09 |
| TU (Wii) | `Stats.o` |
| Wii symbol (demangled) | `Stats const::SaveForEndGame(...)` |
| Wii addr (Bank 8) | `0x801e3a20` |
| Xenon addr | `0x82679b40` |
| Xenon func name | `Function_82679B40` (stripped binary — name is auto-generated) |
| Wii body size | 259 asm lines (lines 1193-1450 in `build/SZBE69_B8/asm/band3/game/Stats.s`) |
| Xenon body size | 1024 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (5 total, 5 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x827a0108` | `Function_827A0108` | `WriteEndian__9BinStreamFPCvi` |
| `0x826796d0` | `Function_826796D0` | `__ls<Q25Stats11SectionInfo,Us>__FR9BinStreamRCQ211stlpmtx_std85vector<Q25Stats11SectionInfo,Us,Q211stlpmtx_std35StlNodeAlloc<Q25Stats11SectionInfo>>_R9BinStream` |
| `0x8279ffc8` | `?Write@BinStream@@QAAXPBXH@Z` | `Read__9BinStreamFPvi` |
| `0x8228cb00` | `Function_8228CB00` | `__ls<Q25Stats10StreakInfo,Us>__FR9BinStreamRCQ211stlpmtx_std83vector<Q25Stats10StreakInfo,Us,Q211stlpmtx_std34StlNodeAlloc<Q25Stats10StreakInfo>>_R9BinStream` |
| `0x826799d0` | `Function_826799D0` | `SaveSingerStats__5StatsCFR9BinStream` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_82679B40(undefined4 *param_1,undefined8 param_2)

{
  undefined1 local_20 [4];
  undefined4 local_1c;
  
  local_1c = *param_1;
  Function_827A0108(param_2,&local_1c,4);
  Function_8228CB00(param_2,param_1 + 0x34);
  local_1c = param_1[4];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[5];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[6];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x71];
  Function_827A0108(param_2,&local_1c,4);
  local_20[0] = *(undefined1 *)(param_1 + 7);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_1c = param_1[0x42];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[9];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0xb];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[10];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[8];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0xe];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x13];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x14];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x15];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x16];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x11];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x12];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x25];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x26];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x28];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x27];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x29];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x2a];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x2b];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x2c];
  Function_827A0108(param_2,&local_1c,4);
  local_20[0] = *(undefined1 *)(param_1 + 0x2d);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_20[0] = *(undefined1 *)((int)param_1 + 0xb5);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_1c = param_1[100];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x65];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x66];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x67];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x2e];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x2f];
  Function_827A0108(param_2,&local_1c,4);
  local_20[0] = *(undefined1 *)(param_1 + 0x30);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_1c = param_1[0x31];
  Function_827A0108(param_2,&local_1c,4);
  Function_826796D0(param_2,param_1 + 0x7a);
  local_1c = param_1[0x7d];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x7e];
  Function_827A0108(param_2,&local_1c,4);
  local_1c = param_1[0x7f];
  Function_827A0108(param_2,&local_1c,4);
  Function_826799D0(param_1,param_2);
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct Stats {
    /* 0x000 */ s32 unk0;                           /* inferred */
    /* 0x004 */ char pad4[0xC];                     /* maybe part of unk0[4]? */
    /* 0x010 */ s32 unk10;                          /* inferred */
    /* 0x014 */ s32 unk14;                          /* inferred */
    /* 0x018 */ f32 unk18;                          /* inferred */
    /* 0x01C */ u8 unk1C;                           /* inferred */
    /* 0x01D */ char pad1D[3];                      /* maybe part of unk1C[4]? */
    /* 0x020 */ s32 unk20;                          /* inferred */
    /* 0x024 */ s32 unk24;                          /* inferred */
    /* 0x028 */ s32 unk28;                          /* inferred */
    /* 0x02C */ s32 unk2C;                          /* inferred */
    /* 0x030 */ char pad30[8];                      /* maybe part of unk2C[3]? */
    /* 0x038 */ s32 unk38;                          /* inferred */
    /* 0x03C */ char pad3C[8];                      /* maybe part of unk38[3]? */
    /* 0x044 */ s32 unk44;                          /* inferred */
    /* 0x048 */ s32 unk48;                          /* inferred */
    /* 0x04C */ s32 unk4C;                          /* inferred */
    /* 0x050 */ s32 unk50;                          /* inferred */
    /* 0x054 */ s32 unk54;                          /* inferred */
    /* 0x058 */ s32 unk58;                          /* inferred */
    /* 0x05C */ char pad5C[0x2C];                   /* maybe part of unk58[0xC]? */
    /* 0x088 */ s32 unk88;                          /* inferred */
    /* 0x08C */ s32 unk8C;                          /* inferred */
    /* 0x090 */ s32 unk90;                          /* inferred */
    /* 0x094 */ f32 unk94;                          /* inferred */
    /* 0x098 */ f32 unk98;                          /* inferred */
    /* 0x09C */ f32 unk9C;                          /* inferred */
    /* 0x0A0 */ f32 unkA0;                          /* inferred */
    /* 0x0A4 */ s32 unkA4;                          /* inferred */
    /* 0x0A8 */ u8 unkA8;                           /* inferred */
    /* 0x0A9 */ u8 unkA9;                           /* inferred */
    /* 0x0AA */ char padAA[2];                      /* maybe part of unkA9[3]? */
    /* 0x0AC */ f32 unkAC;                          /* inferred */
    /* 0x0B0 */ s32 unkB0;                          /* inferred */
    /* 0x0B4 */ u8 unkB4;                           /* inferred */
    /* 0x0B5 */ char padB5[3];                      /* maybe part of unkB4[4]? */
    /* 0x0B8 */ f32 unkB8;                          /* inferred */
    /* 0x0BC */ char padBC[8];                      /* maybe part of unkB8[3]? */
    /* 0x0C4 */ stlpmtx_std::vector<Stats::StreakInfo, short unsigned, stlpmtx_std::StlNodeAlloc<Stats::StreakInfo>> unkC4; /* inferred */
    /* 0x0C4 */ char padC4[0x28];
    /* 0x0EC */ s32 unkEC;                          /* inferred */
    /* 0x0F0 */ char padF0[0x70];                   /* maybe part of unkEC[0x1D]? */
    /* 0x160 */ s32 unk160;                         /* inferred */
    /* 0x164 */ s32 unk164;                         /* inferred */
    /* 0x168 */ s32 unk168;                         /* inferred */
    /* 0x16C */ s32 unk16C;                         /* inferred */
    /* 0x170 */ char pad170[0x24];                  /* maybe part of unk16C[0xA]? */
    /* 0x194 */ f32 unk194;                         /* inferred */
    /* 0x198 */ char pad198[0x20];                  /* maybe part of unk194[9]? */
    /* 0x1B8 */ stlpmtx_std::vector<Stats::SectionInfo, short unsigned, stlpmtx_std::StlNodeAlloc<Stats::SectionInfo>> unk1B8; /* inferred */
    /* 0x1B8 */ char pad1B8[8];
    /* 0x1C0 */ f32 unk1C0;                         /* inferred */
    /* 0x1C4 */ f32 unk1C4;                         /* inferred */
    /* 0x1C8 */ f32 unk1C8;                         /* inferred */
} Stats;                                            /* size >= 0x1CC */

? WriteEndian__9BinStreamFPCvi(BinStream *this, void *arg0, s32 arg1); /* extern */
? Write__9BinStreamFPCvi(BinStream *this, void *arg0, s32 arg1); /* extern */
? SaveSingerStats__5StatsCFR9BinStream(Stats *this, BinStream *arg0); /* static */
BinStream *__ls<Q25Stats10StreakInfo,Us>__FR9BinStreamRCQ211stlpmtx_std83vector<Q25Stats10StreakInfo,Us,Q211stlpmtx_std34StlNodeAlloc<Q25Stats10StreakInfo>>_R9BinStream(BinStream *arg0, stlpmtx_std::vector<Stats::StreakInfo, short unsigned, stlpmtx_std::StlNodeAlloc<Stats::StreakInfo>> *arg1); /* static */
BinStream *__ls<Q25Stats11SectionInfo,Us>__FR9BinStreamRCQ211stlpmtx_std85vector<Q25Stats11SectionInfo,Us,Q211stlpmtx_std35StlNodeAlloc<Q25Stats11SectionInfo>>_R9BinStream(BinStream *arg0, stlpmtx_std::vector<Stats::SectionInfo, short unsigned, stlpmtx_std::StlNodeAlloc<Stats::SectionInfo>> *arg1); /* static */

/* Stats::SaveForEndGame (BinStream &) const */
void SaveForEndGame__5StatsCFR9BinStream(Stats *this, BinStream *arg0) {
    s32 sp94;
    s32 sp90;
    s32 sp8C;
    f32 sp88;
    f32 sp84;
    s32 sp80;
    s32 sp7C;
    s32 sp78;
    s32 sp74;
    s32 sp70;
    s32 sp6C;
    s32 sp68;
    s32 sp64;
    s32 sp60;
    s32 sp5C;
    s32 sp58;
    s32 sp54;
    s32 sp50;
    s32 sp4C;
    f32 sp48;
    s32 sp44;
    f32 sp40;
    f32 sp3C;
    f32 sp38;
    s32 sp34;
    s32 sp30;
    s32 sp2C;
    s32 sp28;
    s32 sp24;
    f32 sp20;
    s32 sp1C;
    f32 sp18;
    f32 sp14;
    f32 sp10;
    f32 spC;
    u8 spB;
    u8 spA;
    u8 sp9;
    u8 sp8;

    sp94 = this->unk0;
    WriteEndian__9BinStreamFPCvi(arg0, &sp94, 4);
    __ls<Q25Stats10StreakInfo,Us>__FR9BinStreamRCQ211stlpmtx_std83vector<Q25Stats10StreakInfo,Us,Q211stlpmtx_std34StlNodeAlloc<Q25Stats10StreakInfo>>_R9BinStream(arg0, &this->unkC4);
    sp90 = this->unk10;
    WriteEndian__9BinStreamFPCvi(arg0, &sp90, 4);
    sp8C = this->unk14;
    WriteEndian__9BinStreamFPCvi(arg0, &sp8C, 4);
    sp88 = this->unk18;
    WriteEndian__9BinStreamFPCvi(arg0, &sp88, 4);
    sp84 = this->unk194;
    WriteEndian__9BinStreamFPCvi(arg0, &sp84, 4);
    spB = this->unk1C;
    Write__9BinStreamFPCvi(arg0, &spB, 1);
    sp80 = this->unkEC;
    WriteEndian__9BinStreamFPCvi(arg0, &sp80, 4);
    sp7C = this->unk24;
    WriteEndian__9BinStreamFPCvi(arg0, &sp7C, 4);
    sp78 = this->unk2C;
    WriteEndian__9BinStreamFPCvi(arg0, &sp78, 4);
    sp74 = this->unk28;
    WriteEndian__9BinStreamFPCvi(arg0, &sp74, 4);
    sp70 = this->unk20;
    WriteEndian__9BinStreamFPCvi(arg0, &sp70, 4);
    sp6C = this->unk38;
    WriteEndian__9BinStreamFPCvi(arg0, &sp6C, 4);
    sp68 = this->unk4C;
    WriteEndian__9BinStreamFPCvi(arg0, &sp68, 4);
    sp64 = this->unk50;
    WriteEndian__9BinStreamFPCvi(arg0, &sp64, 4);
    sp60 = this->unk54;
    WriteEndian__9BinStreamFPCvi(arg0, &sp60, 4);
    sp5C = this->unk58;
    WriteEndian__9BinStreamFPCvi(arg0, &sp5C, 4);
    sp58 = this->unk44;
    WriteEndian__9BinStreamFPCvi(arg0, &sp58, 4);
    sp54 = this->unk48;
    WriteEndian__9BinStreamFPCvi(arg0, &sp54, 4);
    sp50 = this->unk88;
    WriteEndian__9BinStreamFPCvi(arg0, &sp50, 4);
    sp4C = this->unk8C;
    WriteEndian__9BinStreamFPCvi(arg0, &sp4C, 4);
    sp48 = this->unk94;
    WriteEndian__9BinStreamFPCvi(arg0, &sp48, 4);
    sp44 = this->unk90;
    WriteEndian__9BinStreamFPCvi(arg0, &sp44, 4);
    sp40 = this->unk98;
    WriteEndian__9BinStreamFPCvi(arg0, &sp40, 4);
    sp3C = this->unk9C;
    WriteEndian__9BinStreamFPCvi(arg0, &sp3C, 4);
    sp38 = this->unkA0;
    WriteEndian__9BinStreamFPCvi(arg0, &sp38, 4);
    sp34 = this->unkA4;
    WriteEndian__9BinStreamFPCvi(arg0, &sp34, 4);
    spA = this->unkA8;
    Write__9BinStreamFPCvi(arg0, &spA, 1);
    sp9 = this->unkA9;
    Write__9BinStreamFPCvi(arg0, &sp9, 1);
    sp30 = this->unk160;
    WriteEndian__9BinStreamFPCvi(arg0, &sp30, 4);
    sp2C = this->unk164;
    WriteEndian__9BinStreamFPCvi(arg0, &sp2C, 4);
    sp28 = this->unk168;
    WriteEndian__9BinStreamFPCvi(arg0, &sp28, 4);
    sp24 = this->unk16C;
    WriteEndian__9BinStreamFPCvi(arg0, &sp24, 4);
    sp20 = this->unkAC;
    WriteEndian__9BinStreamFPCvi(arg0, &sp20, 4);
    sp1C = this->unkB0;
    WriteEndian__9BinStreamFPCvi(arg0, &sp1C, 4);
    sp8 = this->unkB4;
    Write__9BinStreamFPCvi(arg0, &sp8, 1);
    sp18 = this->unkB8;
    WriteEndian__9BinStreamFPCvi(arg0, &sp18, 4);
    __ls<Q25Stats11SectionInfo,Us>__FR9BinStreamRCQ211stlpmtx_std85vector<Q25Stats11SectionInfo,Us,Q211stlpmtx_std35StlNodeAlloc<Q25Stats11SectionInfo>>_R9BinStream(arg0, &this->unk1B8);
    sp14 = this->unk1C0;
    WriteEndian__9BinStreamFPCvi(arg0, &sp14, 4);
    sp10 = this->unk1C4;
    WriteEndian__9BinStreamFPCvi(arg0, &sp10, 4);
    spC = this->unk1C8;
    WriteEndian__9BinStreamFPCvi(arg0, &spC, 4);
    SaveSingerStats__5StatsCFR9BinStream(this, arg0);
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# Stats::SaveForEndGame(BinStream&) const
.fn SaveForEndGame__5StatsCFR9BinStream, global
/* 801E3A20 001D8440  94 21 FF 60 */	stwu r1, -0xa0(r1)
/* 801E3A24 001D8444  7C 08 02 A6 */	mflr r0
/* 801E3A28 001D8448  38 A0 00 04 */	li r5, 0x4
/* 801E3A2C 001D844C  90 01 00 A4 */	stw r0, 0xa4(r1)
/* 801E3A30 001D8450  93 E1 00 9C */	stw r31, 0x9c(r1)
/* 801E3A34 001D8454  7C 9F 23 78 */	mr r31, r4
/* 801E3A38 001D8458  38 81 00 94 */	addi r4, r1, 0x94
/* 801E3A3C 001D845C  93 C1 00 98 */	stw r30, 0x98(r1)
/* 801E3A40 001D8460  7C 7E 1B 78 */	mr r30, r3
/* 801E3A44 001D8464  80 03 00 00 */	lwz r0, 0x0(r3)
/* 801E3A48 001D8468  7F E3 FB 78 */	mr r3, r31
/* 801E3A4C 001D846C  90 01 00 94 */	stw r0, 0x94(r1)
/* 801E3A50 001D8470  48 2A 5B F1 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3A54 001D8474  7F E3 FB 78 */	mr r3, r31
/* 801E3A58 001D8478  38 9E 00 C4 */	addi r4, r30, 0xc4
/* 801E3A5C 001D847C  48 00 04 55 */	bl "__ls<Q25Stats10StreakInfo,Us>__FR9BinStreamRCQ211stlpmtx_std83vector<Q25Stats10StreakInfo,Us,Q211stlpmtx_std34StlNodeAlloc<Q25Stats10StreakInfo>>_R9BinStream"
/* 801E3A60 001D8480  80 1E 00 10 */	lwz r0, 0x10(r30)
/* 801E3A64 001D8484  7F E3 FB 78 */	mr r3, r31
/* 801E3A68 001D8488  90 01 00 90 */	stw r0, 0x90(r1)
/* 801E3A6C 001D848C  38 81 00 90 */	addi r4, r1, 0x90
/* 801E3A70 001D8490  38 A0 00 04 */	li r5, 0x4
/* 801E3A74 001D8494  48 2A 5B CD */	bl WriteEndian__9BinStreamFPCvi
/* 801E3A78 001D8498  80 1E 00 14 */	lwz r0, 0x14(r30)
/* 801E3A7C 001D849C  7F E3 FB 78 */	mr r3, r31
/* 801E3A80 001D84A0  90 01 00 8C */	stw r0, 0x8c(r1)
/* 801E3A84 001D84A4  38 81 00 8C */	addi r4, r1, 0x8c
/* 801E3A88 001D84A8  38 A0 00 04 */	li r5, 0x4
/* 801E3A8C 001D84AC  48 2A 5B B5 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3A90 001D84B0  C0 1E 00 18 */	lfs f0, 0x18(r30)
/* 801E3A94 001D84B4  7F E3 FB 78 */	mr r3, r31
/* 801E3A98 001D84B8  D0 01 00 88 */	stfs f0, 0x88(r1)
/* 801E3A9C 001D84BC  38 81 00 88 */	addi r4, r1, 0x88
/* 801E3AA0 001D84C0  38 A0 00 04 */	li r5, 0x4
/* 801E3AA4 001D84C4  48 2A 5B 9D */	bl WriteEndian__9BinStreamFPCvi
/* 801E3AA8 001D84C8  C0 1E 01 94 */	lfs f0, 0x194(r30)
/* 801E3AAC 001D84CC  7F E3 FB 78 */	mr r3, r31
/* 801E3AB0 001D84D0  D0 01 00 84 */	stfs f0, 0x84(r1)
/* 801E3AB4 001D84D4  38 81 00 84 */	addi r4, r1, 0x84
/* 801E3AB8 001D84D8  38 A0 00 04 */	li r5, 0x4
/* 801E3ABC 001D84DC  48 2A 5B 85 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3AC0 001D84E0  88 1E 00 1C */	lbz r0, 0x1c(r30)
/* 801E3AC4 001D84E4  7F E3 FB 78 */	mr r3, r31
/* 801E3AC8 001D84E8  98 01 00 0B */	stb r0, 0xb(r1)
/* 801E3ACC 001D84EC  38 81 00 0B */	addi r4, r1, 0xb
/* 801E3AD0 001D84F0  38 A0 00 01 */	li r5, 0x1
/* 801E3AD4 001D84F4  48 2A 57 DD */	bl Write__9BinStreamFPCvi
/* 801E3AD8 001D84F8  80 1E 00 EC */	lwz r0, 0xec(r30)
/* 801E3ADC 001D84FC  7F E3 FB 78 */	mr r3, r31
/* 801E3AE0 001D8500  90 01 00 80 */	stw r0, 0x80(r1)
/* 801E3AE4 001D8504  38 81 00 80 */	addi r4, r1, 0x80
/* 801E3AE8 001D8508  38 A0 00 04 */	li r5, 0x4
/* 801E3AEC 001D850C  48 2A 5B 55 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3AF0 001D8510  80 1E 00 24 */	lwz r0, 0x24(r30)
/* 801E3AF4 001D8514  7F E3 FB 78 */	mr r3, r31
/* 801E3AF8 001D8518  90 01 00 7C */	stw r0, 0x7c(r1)
/* 801E3AFC 001D851C  38 81 00 7C */	addi r4, r1, 0x7c
/* 801E3B00 001D8520  38 A0 00 04 */	li r5, 0x4
/* 801E3B04 001D8524  48 2A 5B 3D */	bl WriteEndian__9BinStreamFPCvi
/* 801E3B08 001D8528  80 1E 00 2C */	lwz r0, 0x2c(r30)
/* 801E3B0C 001D852C  7F E3 FB 78 */	mr r3, r31
/* 801E3B10 001D8530  90 01 00 78 */	stw r0, 0x78(r1)
/* 801E3B14 001D8534  38 81 00 78 */	addi r4, r1, 0x78
/* 801E3B18 001D8538  38 A0 00 04 */	li r5, 0x4
/* 801E3B1C 001D853C  48 2A 5B 25 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3B20 001D8540  80 1E 00 28 */	lwz r0, 0x28(r30)
/* 801E3B24 001D8544  7F E3 FB 78 */	mr r3, r31
/* 801E3B28 001D8548  90 01 00 74 */	stw r0, 0x74(r1)
/* 801E3B2C 001D854C  38 81 00 74 */	addi r4, r1, 0x74
/* 801E3B30 001D8550  38 A0 00 04 */	li r5, 0x4
/* 801E3B34 001D8554  48 2A 5B 0D */	bl WriteEndian__9BinStreamFPCvi
/* 801E3B38 001D8558  80 1E 00 20 */	lwz r0, 0x20(r30)
/* 801E3B3C 001D855C  7F E3 FB 78 */	mr r3, r31
/* 801E3B40 001D8560  90 01 00 70 */	stw r0, 0x70(r1)
/* 801E3B44 001D8564  38 81 00 70 */	addi r4, r1, 0x70
/* 801E3B48 001D8568  38 A0 00 04 */	li r5, 0x4
/* 801E3B4C 001D856C  48 2A 5A F5 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3B50 001D8570  80 1E 00 38 */	lwz r0, 0x38(r30)
/* 801E3B54 001D8574  7F E3 FB 78 */	mr r3, r31
/* 801E3B58 001D8578  90 01 00 6C */	stw r0, 0x6c(r1)
/* 801E3B5C 001D857C  38 81 00 6C */	addi r4, r1, 0x6c
/* 801E3B60 001D8580  38 A0 00 04 */	li r5, 0x4
/* 801E3B64 001D8584  48 2A 5A DD */	bl WriteEndian__9BinStreamFPCvi
/* 801E3B68 001D8588  80 1E 00 4C */	lwz r0, 0x4c(r30)
/* 801E3B6C 001D858C  7F E3 FB 78 */	mr r3, r31
/* 801E3B70 001D8590  90 01 00 68 */	stw r0, 0x68(r1)
/* 801E3B74 001D8594  38 81 00 68 */	addi r4, r1, 0x68
/* 801E3B78 001D8598  38 A0 00 04 */	li r5, 0x4
/* 801E3B7C 001D859C  48 2A 5A C5 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3B80 001D85A0  80 1E 00 50 */	lwz r0, 0x50(r30)
/* 801E3B84 001D85A4  7F E3 FB 78 */	mr r3, r31
/* 801E3B88 001D85A8  90 01 00 64 */	stw r0, 0x64(r1)
/* 801E3B8C 001D85AC  38 81 00 64 */	addi r4, r1, 0x64
/* 801E3B90 001D85B0  38 A0 00 04 */	li r5, 0x4
/* 801E3B94 001D85B4  48 2A 5A AD */	bl WriteEndian__9BinStreamFPCvi
/* 801E3B98 001D85B8  80 1E 00 54 */	lwz r0, 0x54(r30)
/* 801E3B9C 001D85BC  7F E3 FB 78 */	mr r3, r31
/* 801E3BA0 001D85C0  90 01 00 60 */	stw r0, 0x60(r1)
/* 801E3BA4 001D85C4  38 81 00 60 */	addi r4, r1, 0x60
/* 801E3BA8 001D85C8  38 A0 00 04 */	li r5, 0x4
/* 801E3BAC 001D85CC  48 2A 5A 95 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3BB0 001D85D0  80 1E 00 58 */	lwz r0, 0x58(r30)
/* 801E3BB4 001D85D4  7F E3 FB 78 */	mr r3, r31
/* 801E3BB8 001D85D8  90 01 00 5C */	stw r0, 0x5c(r1)
/* 801E3BBC 001D85DC  38 81 00 5C */	addi r4, r1, 0x5c
/* 801E3BC0 001D85E0  38 A0 00 04 */	li r5, 0x4
/* 801E3BC4 001D85E4  48 2A 5A 7D */	bl WriteEndian__9BinStreamFPCvi
/* 801E3BC8 001D85E8  80 1E 00 44 */	lwz r0, 0x44(r30)
/* 801E3BCC 001D85EC  7F E3 FB 78 */	mr r3, r31
/* 801E3BD0 001D85F0  90 01 00 58 */	stw r0, 0x58(r1)
/* 801E3BD4 001D85F4  38 81 00 58 */	addi r4, r1, 0x58
/* 801E3BD8 001D85F8  38 A0 00 04 */	li r5, 0x4
/* 801E3BDC 001D85FC  48 2A 5A 65 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3BE0 001D8600  80 1E 00 48 */	lwz r0, 0x48(r30)
/* 801E3BE4 001D8604  7F E3 FB 78 */	mr r3, r31
/* 801E3BE8 001D8608  90 01 00 54 */	stw r0, 0x54(r1)
/* 801E3BEC 001D860C  38 81 00 54 */	addi r4, r1, 0x54
/* 801E3BF0 001D8610  38 A0 00 04 */	li r5, 0x4
/* 801E3BF4 001D8614  48 2A 5A 4D */	bl WriteEndian__9BinStreamFPCvi
/* 801E3BF8 001D8618  80 1E 00 88 */	lwz r0, 0x88(r30)
/* 801E3BFC 001D861C  7F E3 FB 78 */	mr r3, r31
/* 801E3C00 001D8620  90 01 00 50 */	stw r0, 0x50(r1)
/* 801E3C04 001D8624  38 81 00 50 */	addi r4, r1, 0x50
/* 801E3C08 001D8628  38 A0 00 04 */	li r5, 0x4
/* 801E3C0C 001D862C  48 2A 5A 35 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3C10 001D8630  80 1E 00 8C */	lwz r0, 0x8c(r30)
/* 801E3C14 001D8634  7F E3 FB 78 */	mr r3, r31
/* 801E3C18 001D8638  90 01 00 4C */	stw r0, 0x4c(r1)
/* 801E3C1C 001D863C  38 81 00 4C */	addi r4, r1, 0x4c
/* 801E3C20 001D8640  38 A0 00 04 */	li r5, 0x4
/* 801E3C24 001D8644  48 2A 5A 1D */	bl WriteEndian__9BinStreamFPCvi
/* 801E3C28 001D8648  C0 1E 00 94 */	lfs f0, 0x94(r30)
/* 801E3C2C 001D864C  7F E3 FB 78 */	mr r3, r31
/* 801E3C30 001D8650  D0 01 00 48 */	stfs f0, 0x48(r1)
/* 801E3C34 001D8654  38 81 00 48 */	addi r4, r1, 0x48
/* 801E3C38 001D8658  38 A0 00 04 */	li r5, 0x4
/* 801E3C3C 001D865C  48 2A 5A 05 */	bl WriteEndian__9BinStreamFPCvi
/* 801E3C40 001D8660  80 1E 00 90 */	lwz r0, 0x90(r30)
/* 801E3C44 001D8664  7F E3 FB 78 */	mr r3, r31
/* 801E3C48 001D8668  90 01 00 44 */	stw r0, 0x44(r1)
/* 801E3C4C 001D866C  38 81 00 44 */	addi r4, r1, 0x44
/* 801E3C50 001D8670  38 A0 00 04 */	li r5, 0x4
/* 801E3C54 001D8674  48 2A 59 ED */	bl WriteEndian__9BinStreamFPCvi
/* 801E3C58 001D8678  C0 1E 00 98 */	lfs f0, 0x98(r30)
/* 801E3C5C 001D867C  7F E3 FB 78 */	mr r3, r31
/* 801E3C60 001D8680  D0 01 00 40 */	stfs f0, 0x40(r1)
/* 801E3C64 001D8684  38 81 00 40 */	addi r4, r1, 0x40
/* 801E3C68 001D8688  38 A0 00 04 */	li r5, 0x4
/* 801E3C6C 001D868C  48 2A 59 D5 */	bl WriteEndian__9BinStreamFPCvi
... [truncated 109 of 259 asm lines]
```
