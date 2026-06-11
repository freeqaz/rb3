# Pair 19 — verification evidence

**Claimed identity:** Wii `SaveGlobalOptions__10ProfileMgrFR23FixedSizeSaveableStream`  ==  Xenon `0x82532b48`

| field | value |
|---|---|
| pair_id | 19 |
| stratum | BSIM 15-20 |
| match_type | `BSIM` |
| BSim sim×conf | 17.482 |
| BSim similarity / confidence | 0.739 / 23.656 |
| TU (Wii) | `ProfileMgr.o` |
| Wii symbol (demangled) | `ProfileMgr::SaveGlobalOptions(...)` |
| Wii addr (Bank 8) | `0x8034aa60` |
| Xenon addr | `0x82532b48` |
| Xenon func name | `Function_82532B48` (stripped binary — name is auto-generated) |
| Wii body size | 209 asm lines (lines 2373-2580 in `build/SZBE69_B8/asm/band3/meta_band/ProfileMgr.s`) |
| Xenon body size | 532 bytes |

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
| `0x827a0108` | `Function_827A0108` | `WriteEndian__9BinStreamFPCvi` |
| `0x8279ffc8` | `?Write@BinStream@@QAAXPBXH@Z` | `Read__9BinStreamFPvi` |
| `0x82571cc8` | `Function_82571CC8` | `Save__11ModifierMgrFR23FixedSizeSaveableStream` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_82532B48(int param_1,undefined8 param_2)

{
  undefined1 local_20 [4];
  undefined4 local_1c;
  
  local_1c = 7;
  Function_827A0108(param_2,&local_1c,4);
  local_1c = *(undefined4 *)(param_1 + 0x58);
  Function_827A0108(param_2,&local_1c,4);
  local_1c = *(undefined4 *)(param_1 + 0x5c);
  Function_827A0108(param_2,&local_1c,4);
  local_1c = *(undefined4 *)(param_1 + 0x3c);
  Function_827A0108(param_2,&local_1c,4);
  local_1c = *(undefined4 *)(param_1 + 0x40);
  Function_827A0108(param_2,&local_1c,4);
  local_1c = *(undefined4 *)(param_1 + 0x44);
  Function_827A0108(param_2,&local_1c,4);
  local_1c = *(undefined4 *)(param_1 + 0x4c);
  Function_827A0108(param_2,&local_1c,4);
  local_20[0] = *(undefined1 *)(param_1 + 0x60);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_1c = *(undefined4 *)(param_1 + 0x48);
  Function_827A0108(param_2,&local_1c,4);
  local_20[0] = *(undefined1 *)(param_1 + 0x61);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_1c = *(undefined4 *)(param_1 + 100);
  Function_827A0108(param_2,&local_1c,4);
  local_20[0] = *(undefined1 *)(param_1 + 0x68);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_20[0] = *(undefined1 *)(param_1 + 0x69);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_1c = *(undefined4 *)(param_1 + 0x50);
  Function_827A0108(param_2,&local_1c,4);
  local_20[0] = *(undefined1 *)(param_1 + 0x54);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_20[0] = *(undefined1 *)(param_1 + 0x55);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_1c = *(undefined4 *)(param_1 + 0x98);
  Function_827A0108(param_2,&local_1c,4);
  local_20[0] = *(undefined1 *)(param_1 + 0x6a);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  local_20[0] = *(undefined1 *)(param_1 + 0x6b);
  _Write_BinStream__QAAXPBXH_Z(param_2,local_20,1);
  Function_82571CC8(DAT_82dcd660,param_2);
  *(undefined1 *)(param_1 + 0x38) = 0;
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct ProfileMgr {
    /* 0x000 */ char pad0[0x558];
    /* 0x558 */ s8 unk558;                          /* inferred */
    /* 0x559 */ char pad559[3];                     /* maybe part of unk558[4]? */
    /* 0x55C */ s32 unk55C;                         /* inferred */
    /* 0x560 */ s32 unk560;                         /* inferred */
    /* 0x564 */ s32 unk564;                         /* inferred */
    /* 0x568 */ s32 unk568;                         /* inferred */
    /* 0x56C */ s32 unk56C;                         /* inferred */
    /* 0x570 */ s32 unk570;                         /* inferred */
    /* 0x574 */ u8 unk574;                          /* inferred */
    /* 0x575 */ u8 unk575;                          /* inferred */
    /* 0x576 */ char pad576[2];                     /* maybe part of unk575[3]? */
    /* 0x578 */ f32 unk578;                         /* inferred */
    /* 0x57C */ f32 unk57C;                         /* inferred */
    /* 0x580 */ u8 unk580;                          /* inferred */
    /* 0x581 */ u8 unk581;                          /* inferred */
    /* 0x582 */ char pad582[2];                     /* maybe part of unk581[3]? */
    /* 0x584 */ s32 unk584;                         /* inferred */
    /* 0x588 */ u8 unk588;                          /* inferred */
    /* 0x589 */ u8 unk589;                          /* inferred */
    /* 0x58A */ u8 unk58A;                          /* inferred */
    /* 0x58B */ u8 unk58B;                          /* inferred */
    /* 0x58C */ char pad58C[0x18];                  /* maybe part of unk58B[0x19]? */
    /* 0x5A4 */ u8 unk5A4;                          /* inferred */
    /* 0x5A5 */ char pad5A5[3];                     /* maybe part of unk5A4[4]? */
    /* 0x5A8 */ s32 unk5A8;                         /* inferred */
    /* 0x5AC */ s32 unk5AC;                         /* inferred */
    /* 0x5B0 */ u8 unk5B0;                          /* inferred */
    /* 0x5B1 */ u8 unk5B1;                          /* inferred */
    /* 0x5B2 */ char pad5B2[1];
    /* 0x5B3 */ u8 unk5B3;                          /* inferred */
    /* 0x5B4 */ u8 unk5B4;                          /* inferred */
    /* 0x5B5 */ char pad5B5[3];                     /* maybe part of unk5B4[4]? */
    /* 0x5B8 */ s32 unk5B8;                         /* inferred */
    /* 0x5BC */ char pad5BC[0x10];                  /* maybe part of unk5B8[5]? */
    /* 0x5CC */ s32 unk5CC;                         /* inferred */
} ProfileMgr;                                       /* size >= 0x5D0 */

? Save__11ModifierMgrFR23FixedSizeSaveableStream(ModifierMgr *this, FixedSizeSaveableStream *arg0); /* extern */
? WriteEndian__9BinStreamFPCvi(BinStream *this, void *arg0, s32 arg1); /* extern */
? Write__9BinStreamFPCvi(BinStream *this, void *arg0, s32 arg1); /* extern */
void *__ct__6StringFPCc(String *this, s8 *arg0);    /* extern */
void *__dt__6StringFv(String *this, s16 destroyFlag); /* extern */
? __ls__9BinStreamFRC6String(BinStream *this, String *arg0); /* extern */
extern ModifierMgr *TheModifierMgr;
static ? @stringBase0;                              /* unable to generate initializer: unknown type */

/* ProfileMgr::SaveGlobalOptions (FixedSizeSaveableStream &) */
void SaveGlobalOptions__10ProfileMgrFR23FixedSizeSaveableStream(ProfileMgr *this, FixedSizeSaveableStream *arg0) {
    String sp58;
    s32 sp54;
    s32 sp50;
    s32 sp4C;
    f32 sp48;
    f32 sp44;
    s32 sp40;
    s32 sp3C;
    s32 sp38;
    s32 sp34;
    s32 sp30;
    s32 sp2C;
    s32 sp28;
    s32 sp24;
    s32 sp20;
    s32 sp1C;
    s32 sp18;
    u8 sp15;
    u8 sp14;
    u8 sp13;
    u8 sp12;
    u8 sp11;
    u8 sp10;
    u8 spF;
    u8 spE;
    u8 spD;
    u8 spC;
    u8 spB;
    s8 spA;
    u8 sp9;
    u8 sp8;

    sp4C = 0x20007;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp4C, 4);
    sp48 = this->unk578;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp48, 4);
    sp44 = this->unk57C;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp44, 4);
    sp40 = this->unk55C;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp40, 4);
    sp3C = this->unk560;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp3C, 4);
    sp38 = this->unk564;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp38, 4);
    sp34 = this->unk56C;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp34, 4);
    sp15 = this->unk580;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp15, 1);
    sp30 = this->unk568;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp30, 4);
    sp14 = this->unk581;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp14, 1);
    sp2C = this->unk584;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp2C, 4);
    sp13 = this->unk588;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp13, 1);
    sp12 = this->unk589;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp12, 1);
    sp11 = this->unk5A4;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp11, 1);
    sp28 = this->unk5A8;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp28, 4);
    sp24 = this->unk5AC;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp24, 4);
    sp10 = this->unk5B0;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp10, 1);
    spF = this->unk5B1;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spF, 1);
    sp20 = this->unk570;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp20, 4);
    spE = this->unk574;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spE, 1);
    spD = this->unk575;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spD, 1);
    sp1C = this->unk5CC;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp1C, 4);
    spC = this->unk58A;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spC, 1);
    spB = this->unk58B;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spB, 1);
    Save__11ModifierMgrFR23FixedSizeSaveableStream(TheModifierMgr, arg0);
    __ct__6StringFPCc(&sp58, "profile_mgr\0signin_changed\0ProfileMgr.cpp\0profile\0padNum != -1\0pUser\0netServer\0mGlobalOptionsSaveState != kMetaProfileUnchanged\0\0%s can't load new %s version %d > %d\0%s can't load new %s alt version %d > %d\0sound\0slider\0slider_voicechat\0mSliderConfig\00 <= ixVol && ixVol < mSliderConfig->Size() - 1\0Mic:%s\n\0ratio_sliders\0background_music_level.fade\0crowd_level.fade\0sfx_level.fade\0instrument_level.fade\0bass_boost.send\0wet_gain\0dry_gain\0mVoiceChatSliderConfig\00 <= mVoiceChatVolume && mVoiceChatVolume < mVoiceChatSliderConfig->Size() - 1\0crowd_audio.volume\0error_message\0init\0passive_message_use_headphones\0passive_message_dont_use_headphones\00\0PlatformAudioLatency:%f\n\0PlatformVideoLatency:%f\n\0( 0) <= (type) && (type) < ( kJoypadNumTypes)\0( 0) <= (ctx) && (ctx) < ( kNumLagContexts)\0print_mic_gains\0Mic gain is %f\n\0Mic gain is forced to %f (would be %f if not forcing)\n\0mic\0Mic gain forcing: %s\n\0INACTIVE\0ACTIVE\0Mic output gain forcing: %s - output gain for mic %d is %f dB\n\0pProfile\0campaign\0tour\0qp_career_songinfo\0qp_coop\0user\0pMachineMgr\0%s(%d): %s unhandled msg: %s" + 0x80);
    __ls__9BinStreamFRC6String((BinStream *) arg0, &sp58);
    __dt__6StringFv(&sp58, -1);
    spA = 0;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spA, 1);
    sp54 = 0;
    sp50 = 0;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp50, 8);
    sp9 = this->unk5B3;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp9, 1);
    sp8 = this->unk5B4;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp8, 1);
    sp18 = this->unk5B8;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp18, 4);
    this->unk558 = 0;
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# ProfileMgr::SaveGlobalOptions(FixedSizeSaveableStream&)
.fn SaveGlobalOptions__10ProfileMgrFR23FixedSizeSaveableStream, global
/* 8034AA60 0033F480  94 21 FF 80 */	stwu r1, -0x80(r1)
/* 8034AA64 0033F484  7C 08 02 A6 */	mflr r0
/* 8034AA68 0033F488  3C A0 00 02 */	lis r5, 0x2
/* 8034AA6C 0033F48C  90 01 00 84 */	stw r0, 0x84(r1)
/* 8034AA70 0033F490  38 05 00 07 */	addi r0, r5, 0x7
/* 8034AA74 0033F494  38 A0 00 04 */	li r5, 0x4
/* 8034AA78 0033F498  93 E1 00 7C */	stw r31, 0x7c(r1)
/* 8034AA7C 0033F49C  7C 9F 23 78 */	mr r31, r4
/* 8034AA80 0033F4A0  38 81 00 4C */	addi r4, r1, 0x4c
/* 8034AA84 0033F4A4  93 C1 00 78 */	stw r30, 0x78(r1)
/* 8034AA88 0033F4A8  7C 7E 1B 78 */	mr r30, r3
/* 8034AA8C 0033F4AC  7F E3 FB 78 */	mr r3, r31
/* 8034AA90 0033F4B0  93 A1 00 74 */	stw r29, 0x74(r1)
/* 8034AA94 0033F4B4  90 01 00 4C */	stw r0, 0x4c(r1)
/* 8034AA98 0033F4B8  48 13 EB A9 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AA9C 0033F4BC  C0 1E 05 78 */	lfs f0, 0x578(r30)
/* 8034AAA0 0033F4C0  7F E3 FB 78 */	mr r3, r31
/* 8034AAA4 0033F4C4  D0 01 00 48 */	stfs f0, 0x48(r1)
/* 8034AAA8 0033F4C8  38 81 00 48 */	addi r4, r1, 0x48
/* 8034AAAC 0033F4CC  38 A0 00 04 */	li r5, 0x4
/* 8034AAB0 0033F4D0  48 13 EB 91 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AAB4 0033F4D4  C0 1E 05 7C */	lfs f0, 0x57c(r30)
/* 8034AAB8 0033F4D8  7F E3 FB 78 */	mr r3, r31
/* 8034AABC 0033F4DC  D0 01 00 44 */	stfs f0, 0x44(r1)
/* 8034AAC0 0033F4E0  38 81 00 44 */	addi r4, r1, 0x44
/* 8034AAC4 0033F4E4  38 A0 00 04 */	li r5, 0x4
/* 8034AAC8 0033F4E8  48 13 EB 79 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AACC 0033F4EC  80 1E 05 5C */	lwz r0, 0x55c(r30)
/* 8034AAD0 0033F4F0  7F E3 FB 78 */	mr r3, r31
/* 8034AAD4 0033F4F4  90 01 00 40 */	stw r0, 0x40(r1)
/* 8034AAD8 0033F4F8  38 81 00 40 */	addi r4, r1, 0x40
/* 8034AADC 0033F4FC  38 A0 00 04 */	li r5, 0x4
/* 8034AAE0 0033F500  48 13 EB 61 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AAE4 0033F504  80 1E 05 60 */	lwz r0, 0x560(r30)
/* 8034AAE8 0033F508  7F E3 FB 78 */	mr r3, r31
/* 8034AAEC 0033F50C  90 01 00 3C */	stw r0, 0x3c(r1)
/* 8034AAF0 0033F510  38 81 00 3C */	addi r4, r1, 0x3c
/* 8034AAF4 0033F514  38 A0 00 04 */	li r5, 0x4
/* 8034AAF8 0033F518  48 13 EB 49 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AAFC 0033F51C  80 1E 05 64 */	lwz r0, 0x564(r30)
/* 8034AB00 0033F520  7F E3 FB 78 */	mr r3, r31
/* 8034AB04 0033F524  90 01 00 38 */	stw r0, 0x38(r1)
/* 8034AB08 0033F528  38 81 00 38 */	addi r4, r1, 0x38
/* 8034AB0C 0033F52C  38 A0 00 04 */	li r5, 0x4
/* 8034AB10 0033F530  48 13 EB 31 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AB14 0033F534  80 1E 05 6C */	lwz r0, 0x56c(r30)
/* 8034AB18 0033F538  7F E3 FB 78 */	mr r3, r31
/* 8034AB1C 0033F53C  90 01 00 34 */	stw r0, 0x34(r1)
/* 8034AB20 0033F540  38 81 00 34 */	addi r4, r1, 0x34
/* 8034AB24 0033F544  38 A0 00 04 */	li r5, 0x4
/* 8034AB28 0033F548  48 13 EB 19 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AB2C 0033F54C  88 1E 05 80 */	lbz r0, 0x580(r30)
/* 8034AB30 0033F550  7F E3 FB 78 */	mr r3, r31
/* 8034AB34 0033F554  98 01 00 15 */	stb r0, 0x15(r1)
/* 8034AB38 0033F558  38 81 00 15 */	addi r4, r1, 0x15
/* 8034AB3C 0033F55C  38 A0 00 01 */	li r5, 0x1
/* 8034AB40 0033F560  48 13 E7 71 */	bl Write__9BinStreamFPCvi
/* 8034AB44 0033F564  80 1E 05 68 */	lwz r0, 0x568(r30)
/* 8034AB48 0033F568  7F E3 FB 78 */	mr r3, r31
/* 8034AB4C 0033F56C  90 01 00 30 */	stw r0, 0x30(r1)
/* 8034AB50 0033F570  38 81 00 30 */	addi r4, r1, 0x30
/* 8034AB54 0033F574  38 A0 00 04 */	li r5, 0x4
/* 8034AB58 0033F578  48 13 EA E9 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AB5C 0033F57C  88 1E 05 81 */	lbz r0, 0x581(r30)
/* 8034AB60 0033F580  7F E3 FB 78 */	mr r3, r31
/* 8034AB64 0033F584  98 01 00 14 */	stb r0, 0x14(r1)
/* 8034AB68 0033F588  38 81 00 14 */	addi r4, r1, 0x14
/* 8034AB6C 0033F58C  38 A0 00 01 */	li r5, 0x1
/* 8034AB70 0033F590  48 13 E7 41 */	bl Write__9BinStreamFPCvi
/* 8034AB74 0033F594  80 1E 05 84 */	lwz r0, 0x584(r30)
/* 8034AB78 0033F598  7F E3 FB 78 */	mr r3, r31
/* 8034AB7C 0033F59C  90 01 00 2C */	stw r0, 0x2c(r1)
/* 8034AB80 0033F5A0  38 81 00 2C */	addi r4, r1, 0x2c
/* 8034AB84 0033F5A4  38 A0 00 04 */	li r5, 0x4
/* 8034AB88 0033F5A8  48 13 EA B9 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AB8C 0033F5AC  88 1E 05 88 */	lbz r0, 0x588(r30)
/* 8034AB90 0033F5B0  7F E3 FB 78 */	mr r3, r31
/* 8034AB94 0033F5B4  98 01 00 13 */	stb r0, 0x13(r1)
/* 8034AB98 0033F5B8  38 81 00 13 */	addi r4, r1, 0x13
/* 8034AB9C 0033F5BC  38 A0 00 01 */	li r5, 0x1
/* 8034ABA0 0033F5C0  48 13 E7 11 */	bl Write__9BinStreamFPCvi
/* 8034ABA4 0033F5C4  88 1E 05 89 */	lbz r0, 0x589(r30)
/* 8034ABA8 0033F5C8  7F E3 FB 78 */	mr r3, r31
/* 8034ABAC 0033F5CC  98 01 00 12 */	stb r0, 0x12(r1)
/* 8034ABB0 0033F5D0  38 81 00 12 */	addi r4, r1, 0x12
/* 8034ABB4 0033F5D4  38 A0 00 01 */	li r5, 0x1
/* 8034ABB8 0033F5D8  48 13 E6 F9 */	bl Write__9BinStreamFPCvi
/* 8034ABBC 0033F5DC  88 1E 05 A4 */	lbz r0, 0x5a4(r30)
/* 8034ABC0 0033F5E0  7F E3 FB 78 */	mr r3, r31
/* 8034ABC4 0033F5E4  98 01 00 11 */	stb r0, 0x11(r1)
/* 8034ABC8 0033F5E8  38 81 00 11 */	addi r4, r1, 0x11
/* 8034ABCC 0033F5EC  38 A0 00 01 */	li r5, 0x1
/* 8034ABD0 0033F5F0  48 13 E6 E1 */	bl Write__9BinStreamFPCvi
/* 8034ABD4 0033F5F4  80 1E 05 A8 */	lwz r0, 0x5a8(r30)
/* 8034ABD8 0033F5F8  7F E3 FB 78 */	mr r3, r31
/* 8034ABDC 0033F5FC  90 01 00 28 */	stw r0, 0x28(r1)
/* 8034ABE0 0033F600  38 81 00 28 */	addi r4, r1, 0x28
/* 8034ABE4 0033F604  38 A0 00 04 */	li r5, 0x4
/* 8034ABE8 0033F608  48 13 EA 59 */	bl WriteEndian__9BinStreamFPCvi
/* 8034ABEC 0033F60C  80 1E 05 AC */	lwz r0, 0x5ac(r30)
/* 8034ABF0 0033F610  7F E3 FB 78 */	mr r3, r31
/* 8034ABF4 0033F614  90 01 00 24 */	stw r0, 0x24(r1)
/* 8034ABF8 0033F618  38 81 00 24 */	addi r4, r1, 0x24
/* 8034ABFC 0033F61C  38 A0 00 04 */	li r5, 0x4
/* 8034AC00 0033F620  48 13 EA 41 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AC04 0033F624  88 1E 05 B0 */	lbz r0, 0x5b0(r30)
/* 8034AC08 0033F628  7F E3 FB 78 */	mr r3, r31
/* 8034AC0C 0033F62C  98 01 00 10 */	stb r0, 0x10(r1)
/* 8034AC10 0033F630  38 81 00 10 */	addi r4, r1, 0x10
/* 8034AC14 0033F634  38 A0 00 01 */	li r5, 0x1
/* 8034AC18 0033F638  48 13 E6 99 */	bl Write__9BinStreamFPCvi
/* 8034AC1C 0033F63C  88 1E 05 B1 */	lbz r0, 0x5b1(r30)
/* 8034AC20 0033F640  7F E3 FB 78 */	mr r3, r31
/* 8034AC24 0033F644  98 01 00 0F */	stb r0, 0xf(r1)
/* 8034AC28 0033F648  38 81 00 0F */	addi r4, r1, 0xf
/* 8034AC2C 0033F64C  38 A0 00 01 */	li r5, 0x1
/* 8034AC30 0033F650  48 13 E6 81 */	bl Write__9BinStreamFPCvi
/* 8034AC34 0033F654  80 1E 05 70 */	lwz r0, 0x570(r30)
/* 8034AC38 0033F658  7F E3 FB 78 */	mr r3, r31
/* 8034AC3C 0033F65C  90 01 00 20 */	stw r0, 0x20(r1)
/* 8034AC40 0033F660  38 81 00 20 */	addi r4, r1, 0x20
/* 8034AC44 0033F664  38 A0 00 04 */	li r5, 0x4
/* 8034AC48 0033F668  48 13 E9 F9 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AC4C 0033F66C  88 1E 05 74 */	lbz r0, 0x574(r30)
/* 8034AC50 0033F670  7F E3 FB 78 */	mr r3, r31
/* 8034AC54 0033F674  98 01 00 0E */	stb r0, 0xe(r1)
/* 8034AC58 0033F678  38 81 00 0E */	addi r4, r1, 0xe
/* 8034AC5C 0033F67C  38 A0 00 01 */	li r5, 0x1
/* 8034AC60 0033F680  48 13 E6 51 */	bl Write__9BinStreamFPCvi
/* 8034AC64 0033F684  88 1E 05 75 */	lbz r0, 0x575(r30)
/* 8034AC68 0033F688  7F E3 FB 78 */	mr r3, r31
/* 8034AC6C 0033F68C  98 01 00 0D */	stb r0, 0xd(r1)
/* 8034AC70 0033F690  38 81 00 0D */	addi r4, r1, 0xd
/* 8034AC74 0033F694  38 A0 00 01 */	li r5, 0x1
/* 8034AC78 0033F698  48 13 E6 39 */	bl Write__9BinStreamFPCvi
/* 8034AC7C 0033F69C  80 1E 05 CC */	lwz r0, 0x5cc(r30)
/* 8034AC80 0033F6A0  7F E3 FB 78 */	mr r3, r31
/* 8034AC84 0033F6A4  90 01 00 1C */	stw r0, 0x1c(r1)
/* 8034AC88 0033F6A8  38 81 00 1C */	addi r4, r1, 0x1c
/* 8034AC8C 0033F6AC  38 A0 00 04 */	li r5, 0x4
/* 8034AC90 0033F6B0  48 13 E9 B1 */	bl WriteEndian__9BinStreamFPCvi
/* 8034AC94 0033F6B4  88 1E 05 8A */	lbz r0, 0x58a(r30)
/* 8034AC98 0033F6B8  7F E3 FB 78 */	mr r3, r31
/* 8034AC9C 0033F6BC  98 01 00 0C */	stb r0, 0xc(r1)
/* 8034ACA0 0033F6C0  38 81 00 0C */	addi r4, r1, 0xc
/* 8034ACA4 0033F6C4  38 A0 00 01 */	li r5, 0x1
/* 8034ACA8 0033F6C8  48 13 E6 09 */	bl Write__9BinStreamFPCvi
/* 8034ACAC 0033F6CC  88 1E 05 8B */	lbz r0, 0x58b(r30)
... [truncated 59 of 209 asm lines]
```
