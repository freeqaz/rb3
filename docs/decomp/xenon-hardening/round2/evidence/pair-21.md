# Pair 21 — verification evidence

**Claimed identity:** Wii `CreateController__14ChordbookPanelFv`  ==  Xenon `0x826966f0`

| field | value |
|---|---|
| pair_id | 21 |
| stratum | BSIM 15-20 |
| match_type | `BSIM` |
| BSim sim×conf | 17.353 |
| BSim similarity / confidence | 0.504 / 34.431 |
| TU (Wii) | `ChordbookPanel.o` |
| Wii symbol (demangled) | `ChordbookPanel::CreateController(...)` |
| Wii addr (Bank 8) | `0x8016c4c0` |
| Xenon addr | `0x826966f0` |
| Xenon func name | `Function_826966F0` (stripped binary — name is auto-generated) |
| Wii body size | 103 asm lines (lines 2136-2237 in `build/SZBE69_B8/asm/band3/game/ChordbookPanel.s`) |
| Xenon body size | 232 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (6 total, 5 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x8279b788` | `??0Symbol@@QAA@PBD@Z` | `Enter__11RndPollableFv` |
| `0x82803f34` | `FUN_82803f34` | `memset` |
| `0x8276aef0` | `Function_8276AEF0` | `NewController__FP4UserPC9DataArrayP23BeatMatchControllerSinkbb9TrackType` |
| `0x8266d140` | `Function_8266D140` | `GetGameplayOptions__8BandUserFv` |
| `0x824fce58` | `Function_824FCE58` | `SystemConfig__F6Symbol6Symbol6Symbol` |
| `0x8266adb0` | `Function_8266ADB0` | _(unmatched / Function_)_ |

## Referenced strings (Xenon side, 2)

- `'controller'`
- `'beatmatcher'`

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_826966F0(void)

{
  int iVar1;
  int iVar4;
  undefined4 *puVar5;
  undefined4 *puVar6;
  undefined8 uVar2;
  int *piVar7;
  undefined8 uVar3;
  undefined4 uVar8;
  int iVar9;
  undefined4 local_40;
  undefined1 auStack_3c [4];
  undefined1 auStack_38 [56];
  
  iVar4 = FUN_82803f34();
  puVar5 = *(undefined4 **)(iVar4 + 0x6c);
  if (puVar5 != (undefined4 *)0x0) {
    (**(code **)*puVar5)(puVar5,1);
  }
  iVar9 = 0;
  *(undefined4 *)(iVar4 + 0x6c) = 0;
  iVar1 = *(int *)(*(int *)(iVar4 + 0x44) + 0x260);
  Function_8266ADB0(&local_40,DAT_82dd0cb8,iVar1);
  puVar5 = (undefined4 *)__0Symbol__QAA_PBD_Z(auStack_3c,0xffffffff820e5804);
  puVar6 = (undefined4 *)__0Symbol__QAA_PBD_Z(auStack_38,0xffffffff820e57f8);
  uVar2 = Function_824FCE58(*puVar6,*puVar5,local_40);
  if (iVar1 != 0) {
    iVar9 = *(int *)(*(int *)(iVar1 + 4) + 4) + iVar1 + 4;
  }
  piVar7 = (int *)Function_8266D140(iVar1);
  uVar3 = (**(code **)(*piVar7 + 0x10))();
  uVar8 = Function_8276AEF0(iVar9,uVar2,iVar4 + 0x3c,0,uVar3,10);
  *(undefined4 *)(iVar4 + 0x6c) = uVar8;
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct BandUser {
    /* 0x0 */ User *unk0;                           /* inferred */
} BandUser;                                         /* size >= 0x4 */

typedef struct ChordbookPanel {
    /* 0x00 */ char pad0[0x38];
    /* 0x38 */ BeatMatchControllerSink unk38;       /* inferred */
    /* 0x38 */ char pad38[8];
    /* 0x40 */ void *unk40;                         /* inferred */
    /* 0x44 */ char pad44[0x24];                    /* maybe part of unk40[0xA]? */
    /* 0x68 */ void **unk68;                        /* inferred */
} ChordbookPanel;                                   /* size >= 0x6C */

? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
s32 GetController__10GameConfigCFP8BandUser(GameConfig *this, BandUser *arg0); /* extern */
void **GetGameplayOptions__8BandUserFv(BandUser *this); /* extern */
s8 *MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(s8 *arg0, s8 *arg1, s32 arg2, s8 *arg3); /* extern */
void **NewController__FP4UserPC9DataArrayP23BeatMatchControllerSinkbb9TrackType(User *arg0, DataArray *arg1, BeatMatchControllerSink *arg2, s32 arg3, s32 arg4, TrackType arg5); /* extern */
DataArray *SystemConfig__F6Symbol6Symbol6Symbol(Symbol arg0, Symbol arg1, Symbol arg2); /* extern */
void *__ct__6SymbolFPCc(Symbol *this, s8 *arg0);    /* extern */
extern Debug TheDebug;
extern GameConfig *TheGameConfig;
extern s8 *kAssertStr;
static ? @stringBase0;                              /* unable to generate initializer: unknown type */

/* ChordbookPanel::CreateController (void) */
void CreateController__14ChordbookPanelFv(ChordbookPanel *this, ? arg_sp0) {
    Symbol sp10;
    Symbol spC;
    s32 sp8;
    ? *temp_r29;
    ? *temp_r6;
    ? *temp_r6_2;
    BandUser *temp_r28;
    BandUser *var_r30;
    ChordbookPanel *var_r29;
    DataArray *temp_r27;
    void **temp_r0;
    void *temp_r30;

    temp_r0 = this->unk68;
    if (temp_r0 != NULL) {
        (*temp_r0)->unk8(temp_r0, 1);
    }
    this->unk68 = NULL;
    if ((void *) this->unk40 == NULL) {
        temp_r6 = "chord_legend\0fret_%02d.lbl\0button_skip.lbl\0button_confirm.lbl\0button_cancel.lbl\0skip_chordbook.lbl\0rg_chordbook_skip\0skip_verify.lbl\0rg_chordbook_skip_verify\0confirm.lbl\0help_confirm\0cancel.lbl\0help_cancel\0song_name.lbl\0artist_name.lbl\0difficulty.lbl\0ChordbookPanel.cpp\0pTrainingMgr\0user\0pause_track\0rg_trainer_panel\0track_graphics\0gem\0gems\0real_guitar\0chord\0normal\0chord_fret\0chord_label\0progress_meter\0skip_chordbook.trig\0skip_chordbook_cancel.trig\0success.trig\0player\0mNumSteps <= kMaxSteps\0set_step_progress\0step_progress_%02d.anim\0string < kNumStrings\0fret < kNumFrets\0set_finger_fret\0strum_string\0strum_used_string\0%d invalid finger number, should be 1 thru 4\0no string label for string number %d\0mGemPlayer\0beatmatcher\0controller\0idx < mNumChords\0set_chord_fret\0step_%02d_text.lbl\0set_step_text\0step_%02d.lbl\0num_steps.anim\0string != -1\0fret != -1\0set_string_used\0set_string_unused\0update_progress\0string_%02d.lbl\0lefty_flip.anim\0%s(%d): %s unhandled msg: %s";
        Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r6 + 0xFB, 0x374, temp_r6 + 0x2BF));
    }
    temp_r28 = this->unk40->unk230;
    if (temp_r28 == NULL) {
        temp_r6_2 = "chord_legend\0fret_%02d.lbl\0button_skip.lbl\0button_confirm.lbl\0button_cancel.lbl\0skip_chordbook.lbl\0rg_chordbook_skip\0skip_verify.lbl\0rg_chordbook_skip_verify\0confirm.lbl\0help_confirm\0cancel.lbl\0help_cancel\0song_name.lbl\0artist_name.lbl\0difficulty.lbl\0ChordbookPanel.cpp\0pTrainingMgr\0user\0pause_track\0rg_trainer_panel\0track_graphics\0gem\0gems\0real_guitar\0chord\0normal\0chord_fret\0chord_label\0progress_meter\0skip_chordbook.trig\0skip_chordbook_cancel.trig\0success.trig\0player\0mNumSteps <= kMaxSteps\0set_step_progress\0step_progress_%02d.anim\0string < kNumStrings\0fret < kNumFrets\0set_finger_fret\0strum_string\0strum_used_string\0%d invalid finger number, should be 1 thru 4\0no string label for string number %d\0mGemPlayer\0beatmatcher\0controller\0idx < mNumChords\0set_chord_fret\0step_%02d_text.lbl\0set_step_text\0step_%02d.lbl\0num_steps.anim\0string != -1\0fret != -1\0set_string_used\0set_string_unused\0update_progress\0string_%02d.lbl\0lefty_flip.anim\0%s(%d): %s unhandled msg: %s";
        Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r6_2 + 0xFB, 0x377, temp_r6_2 + 0x11B));
    }
    temp_r29 = "chord_legend\0fret_%02d.lbl\0button_skip.lbl\0button_confirm.lbl\0button_cancel.lbl\0skip_chordbook.lbl\0rg_chordbook_skip\0skip_verify.lbl\0rg_chordbook_skip_verify\0confirm.lbl\0help_confirm\0cancel.lbl\0help_cancel\0song_name.lbl\0artist_name.lbl\0difficulty.lbl\0ChordbookPanel.cpp\0pTrainingMgr\0user\0pause_track\0rg_trainer_panel\0track_graphics\0gem\0gems\0real_guitar\0chord\0normal\0chord_fret\0chord_label\0progress_meter\0skip_chordbook.trig\0skip_chordbook_cancel.trig\0success.trig\0player\0mNumSteps <= kMaxSteps\0set_step_progress\0step_progress_%02d.anim\0string < kNumStrings\0fret < kNumFrets\0set_finger_fret\0strum_string\0strum_used_string\0%d invalid finger number, should be 1 thru 4\0no string label for string number %d\0mGemPlayer\0beatmatcher\0controller\0idx < mNumChords\0set_chord_fret\0step_%02d_text.lbl\0set_step_text\0step_%02d.lbl\0num_steps.anim\0string != -1\0fret != -1\0set_string_used\0set_string_unused\0update_progress\0string_%02d.lbl\0lefty_flip.anim\0%s(%d): %s unhandled msg: %s";
    sp8 = GetController__10GameConfigCFP8BandUser(TheGameConfig, this->unk40->unk230);
    temp_r30 = __ct__6SymbolFPCc(&spC, temp_r29 + 0x2D6);
    temp_r27 = SystemConfig__F6Symbol6Symbol6Symbol((Symbol) __ct__6SymbolFPCc(&sp10, temp_r29 + 0x2CA), (Symbol) temp_r30, (Symbol) &sp8);
    var_r29 = this;
    if (this != NULL) {
        var_r29 = (ChordbookPanel *) &this->unk38;
    }
    var_r30 = temp_r28;
    if (temp_r28 != NULL) {
        var_r30 = (BandUser *) temp_r28->unk0;
    }
    this->unk68 = NewController__FP4UserPC9DataArrayP23BeatMatchControllerSinkbb9TrackType((User *) var_r30, temp_r27, (BeatMatchControllerSink *) var_r29, 0, (*GetGameplayOptions__8BandUserFv(temp_r28))->unk1C(), (TrackType) 0xA);
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# ChordbookPanel::CreateController()
.fn CreateController__14ChordbookPanelFv, global
/* 8016C4C0 00160EE0  94 21 FF D0 */	stwu r1, -0x30(r1)
/* 8016C4C4 00160EE4  7C 08 02 A6 */	mflr r0
/* 8016C4C8 00160EE8  90 01 00 34 */	stw r0, 0x34(r1)
/* 8016C4CC 00160EEC  39 61 00 30 */	addi r11, r1, 0x30
/* 8016C4D0 00160EF0  48 8C CE 61 */	bl _savegpr_27
/* 8016C4D4 00160EF4  80 03 00 68 */	lwz r0, 0x68(r3)
/* 8016C4D8 00160EF8  7C 7F 1B 78 */	mr r31, r3
/* 8016C4DC 00160EFC  2C 00 00 00 */	cmpwi r0, 0x0
/* 8016C4E0 00160F00  41 82 00 1C */	beq .L_8016C4FC
/* 8016C4E4 00160F04  7C 03 03 78 */	mr r3, r0
/* 8016C4E8 00160F08  38 80 00 01 */	li r4, 0x1
/* 8016C4EC 00160F0C  81 83 00 00 */	lwz r12, 0x0(r3)
/* 8016C4F0 00160F10  81 8C 00 08 */	lwz r12, 0x8(r12)
/* 8016C4F4 00160F14  7D 89 03 A6 */	mtctr r12
/* 8016C4F8 00160F18  4E 80 04 21 */	bctrl
.L_8016C4FC:
/* 8016C4FC 00160F1C  80 1F 00 40 */	lwz r0, 0x40(r31)
/* 8016C500 00160F20  38 60 00 00 */	li r3, 0x0
/* 8016C504 00160F24  90 7F 00 68 */	stw r3, 0x68(r31)
/* 8016C508 00160F28  2C 00 00 00 */	cmpwi r0, 0x0
/* 8016C50C 00160F2C  40 82 00 34 */	bne .L_8016C540
/* 8016C510 00160F30  3C 60 80 BB */	lis r3, kAssertStr@ha
/* 8016C514 00160F34  3C C0 80 B8 */	lis r6, "@stringBase0"@ha
/* 8016C518 00160F38  38 C6 B1 A4 */	addi r6, r6, "@stringBase0"@l
/* 8016C51C 00160F3C  80 63 28 58 */	lwz r3, kAssertStr@l(r3)
/* 8016C520 00160F40  38 86 00 FB */	addi r4, r6, 0xfb
/* 8016C524 00160F44  38 A0 03 74 */	li r5, 0x374
/* 8016C528 00160F48  38 C6 02 BF */	addi r6, r6, 0x2bf
/* 8016C52C 00160F4C  4B EA 4B 15 */	bl "MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc"
/* 8016C530 00160F50  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 8016C534 00160F54  7C 64 1B 78 */	mr r4, r3
/* 8016C538 00160F58  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 8016C53C 00160F5C  48 2B 24 85 */	bl Fail__5DebugFPCc
.L_8016C540:
/* 8016C540 00160F60  80 7F 00 40 */	lwz r3, 0x40(r31)
/* 8016C544 00160F64  83 83 02 30 */	lwz r28, 0x230(r3)
/* 8016C548 00160F68  2C 1C 00 00 */	cmpwi r28, 0x0
/* 8016C54C 00160F6C  40 82 00 34 */	bne .L_8016C580
/* 8016C550 00160F70  3C 60 80 BB */	lis r3, kAssertStr@ha
/* 8016C554 00160F74  3C C0 80 B8 */	lis r6, "@stringBase0"@ha
/* 8016C558 00160F78  38 C6 B1 A4 */	addi r6, r6, "@stringBase0"@l
/* 8016C55C 00160F7C  80 63 28 58 */	lwz r3, kAssertStr@l(r3)
/* 8016C560 00160F80  38 86 00 FB */	addi r4, r6, 0xfb
/* 8016C564 00160F84  38 A0 03 77 */	li r5, 0x377
/* 8016C568 00160F88  38 C6 01 1B */	addi r6, r6, 0x11b
/* 8016C56C 00160F8C  4B EA 4A D5 */	bl "MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc"
/* 8016C570 00160F90  3C A0 80 C9 */	lis r5, TheDebug@ha
/* 8016C574 00160F94  7C 64 1B 78 */	mr r4, r3
/* 8016C578 00160F98  38 65 73 D8 */	addi r3, r5, TheDebug@l
/* 8016C57C 00160F9C  48 2B 24 45 */	bl Fail__5DebugFPCc
.L_8016C580:
/* 8016C580 00160FA0  80 9F 00 40 */	lwz r4, 0x40(r31)
/* 8016C584 00160FA4  3C 60 80 C9 */	lis r3, TheGameConfig@ha
/* 8016C588 00160FA8  80 63 EB 60 */	lwz r3, TheGameConfig@l(r3)
/* 8016C58C 00160FAC  80 84 02 30 */	lwz r4, 0x230(r4)
/* 8016C590 00160FB0  48 01 94 A1 */	bl GetController__10GameConfigCFP8BandUser
/* 8016C594 00160FB4  3F A0 80 B8 */	lis r29, "@stringBase0"@ha
/* 8016C598 00160FB8  90 61 00 08 */	stw r3, 0x8(r1)
/* 8016C59C 00160FBC  3B BD B1 A4 */	addi r29, r29, "@stringBase0"@l
/* 8016C5A0 00160FC0  38 61 00 0C */	addi r3, r1, 0xc
/* 8016C5A4 00160FC4  38 9D 02 D6 */	addi r4, r29, 0x2d6
/* 8016C5A8 00160FC8  48 35 0C 19 */	bl __ct__6SymbolFPCc
/* 8016C5AC 00160FCC  7C 7E 1B 78 */	mr r30, r3
/* 8016C5B0 00160FD0  38 61 00 10 */	addi r3, r1, 0x10
/* 8016C5B4 00160FD4  38 9D 02 CA */	addi r4, r29, 0x2ca
/* 8016C5B8 00160FD8  48 35 0C 09 */	bl __ct__6SymbolFPCc
/* 8016C5BC 00160FDC  7F C4 F3 78 */	mr r4, r30
/* 8016C5C0 00160FE0  38 A1 00 08 */	addi r5, r1, 0x8
/* 8016C5C4 00160FE4  48 2D 6C CD */	bl SystemConfig__F6Symbol6Symbol6Symbol
/* 8016C5C8 00160FE8  2C 1F 00 00 */	cmpwi r31, 0x0
/* 8016C5CC 00160FEC  7C 7B 1B 78 */	mr r27, r3
/* 8016C5D0 00160FF0  7F FD FB 78 */	mr r29, r31
/* 8016C5D4 00160FF4  41 82 00 08 */	beq .L_8016C5DC
/* 8016C5D8 00160FF8  3B BF 00 38 */	addi r29, r31, 0x38
.L_8016C5DC:
/* 8016C5DC 00160FFC  2C 1C 00 00 */	cmpwi r28, 0x0
/* 8016C5E0 00161000  7F 9E E3 78 */	mr r30, r28
/* 8016C5E4 00161004  41 82 00 08 */	beq .L_8016C5EC
/* 8016C5E8 00161008  83 DC 00 00 */	lwz r30, 0x0(r28)
.L_8016C5EC:
/* 8016C5EC 0016100C  7F 83 E3 78 */	mr r3, r28
/* 8016C5F0 00161010  4B FF 66 B1 */	bl GetGameplayOptions__8BandUserFv
/* 8016C5F4 00161014  81 83 00 00 */	lwz r12, 0x0(r3)
/* 8016C5F8 00161018  81 8C 00 1C */	lwz r12, 0x1c(r12)
/* 8016C5FC 0016101C  7D 89 03 A6 */	mtctr r12
/* 8016C600 00161020  4E 80 04 21 */	bctrl
/* 8016C604 00161024  7C 67 1B 78 */	mr r7, r3
/* 8016C608 00161028  7F C3 F3 78 */	mr r3, r30
/* 8016C60C 0016102C  7F 64 DB 78 */	mr r4, r27
/* 8016C610 00161030  7F A5 EB 78 */	mr r5, r29
/* 8016C614 00161034  38 C0 00 00 */	li r6, 0x0
/* 8016C618 00161038  39 00 00 0A */	li r8, 0xa
/* 8016C61C 0016103C  48 4B 1B 75 */	bl NewController__FP4UserPC9DataArrayP23BeatMatchControllerSinkbb9TrackType
/* 8016C620 00161040  90 7F 00 68 */	stw r3, 0x68(r31)
/* 8016C624 00161044  39 61 00 30 */	addi r11, r1, 0x30
/* 8016C628 00161048  48 8C CD 55 */	bl _restgpr_27
/* 8016C62C 0016104C  80 01 00 34 */	lwz r0, 0x34(r1)
/* 8016C630 00161050  7C 08 03 A6 */	mtlr r0
/* 8016C634 00161054  38 21 00 30 */	addi r1, r1, 0x30
/* 8016C638 00161058  4E 80 00 20 */	blr
.endfn CreateController__14ChordbookPanelFv
```
