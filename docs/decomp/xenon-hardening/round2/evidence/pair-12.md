# Pair 12 — verification evidence

**Claimed identity:** Wii `UpdateGameCymbalLanes__9GemPlayerFv`  ==  Xenon `0x8269ea00`

| field | value |
|---|---|
| pair_id | 12 |
| stratum | BSIM 20-30 |
| match_type | `BSIM` |
| BSim sim×conf | 26.925 |
| BSim similarity / confidence | 0.496 / 54.285 |
| TU (Wii) | `GemPlayer.o` |
| Wii symbol (demangled) | `GemPlayer::UpdateGameCymbalLanes(...)` |
| Wii addr (Bank 8) | `0x8019c600` |
| Xenon addr | `0x8269ea00` |
| Xenon func name | `Function_8269EA00` (stripped binary — name is auto-generated) |
| Wii body size | 108 asm lines (lines 12894-13000 in `build/SZBE69_B8/asm/band3/game/GemPlayer.s`) |
| Xenon body size | 348 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (8 total, 8 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x82803f30` | `FUN_82803f30` | `FUN_80a3937c` |
| `0x82563f98` | `FUN_82563f98` | `Current__13MetaPerformerFv` |
| `0x8274b8b0` | `FUN_8274b8b0` | `GetUsingRealDrums__8SongDataCFv` |
| `0x8274b8b8` | `FUN_8274b8b8` | `GetTrackType__8BandUserCFv` |
| `0x825116e0` | `Function_825116E0` | `UserHasGHDrums__FP9LocalUser` |
| `0x8266d140` | `Function_8266D140` | `GetGameplayOptions__8BandUserFv` |
| `0x827abe80` | `FUN_827abe80` | `GetResponseMessage__Q26Quazal10RMCContextFv` |
| `0x82753450` | `FUN_82753450` | `SetUseDiscoUnflip__8SongDataFb` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_8269EA00(void)

{
  char cVar1;
  undefined4 uVar2;
  int *piVar3;
  int iVar4;
  char cVar6;
  int *piVar5;
  undefined8 uVar7;
  
  piVar3 = (int *)FUN_82803f30();
  iVar4 = FUN_827abe80(piVar3[0x98]);
  if (iVar4 != 0) {
    return;
  }
  cVar6 = (**(code **)(*piVar3 + 0x1c))(piVar3);
  if (cVar6 == '\0') {
    iVar4 = (**(code **)(*(int *)piVar3[0x98] + 0x18))();
    if (iVar4 == 0) {
      iVar4 = 0;
    }
    else {
      iVar4 = *(int *)(*(int *)(iVar4 + 4) + 0xc) + iVar4 + 4;
    }
    cVar6 = Function_825116E0(iVar4);
    if (cVar6 != '\0') {
      piVar5 = (int *)Function_8266D140(piVar3[0x98]);
      cVar6 = (**(code **)(*piVar5 + 0x10))();
      uVar7 = 1;
      if (cVar6 == '\0') goto LAB_8269eaa8;
    }
  }
  uVar7 = 0;
LAB_8269eaa8:
  uVar2 = *(undefined4 *)(DAT_82dd0c98 + 4);
  cVar6 = FUN_8274b8b0(uVar2);
  if (cVar6 == '\0') {
    uVar7 = 0;
    piVar3[0xc5] = 0;
  }
  else {
    iVar4 = (**(code **)(*(int *)piVar3[0x98] + 0x10))();
    piVar3[0xc5] = iVar4;
    cVar6 = *(char *)(DAT_82dd09e8 + 0x31);
    cVar1 = *(char *)(DAT_82dd09e8 + 0x32);
    iVar4 = FUN_82563f98();
    if ((*(char *)(iVar4 + 0x379) == '\0') && (cVar6 == '\0')) {
      if (cVar1 != '\0') {
        piVar3[0xc5] = 0;
      }
    }
    else {
      piVar3[0xc5] = 0x1c;
    }
    if ((piVar3[0xc5] & 4U) != 0) {
      uVar7 = 1;
    }
  }
  FUN_82753450(uVar2,uVar7);
  FUN_8274b8b8(uVar2,piVar3[0xc5]);
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct BandUser {
    /* 0x0 */ char pad0[4];
    /* 0x4 */ void *unk4;                           /* inferred */
} BandUser;                                         /* size >= 0x8 */

typedef struct GemPlayer {
    /* 0x000 */ char pad0[4];
    /* 0x004 */ void *unk4;                         /* inferred */
    /* 0x008 */ char pad8[0x228];                   /* maybe part of unk4[0x8B]? */
    /* 0x230 */ BandUser *unk230;                   /* inferred */
    /* 0x234 */ char pad234[0xAC];                  /* maybe part of unk230[0x2C]? */
    /* 0x2E0 */ u32 unk2E0;                         /* inferred */
} GemPlayer;                                        /* size >= 0x2E4 */

typedef struct LocalUser {
    /* 0x0 */ char pad0[4];
    /* 0x4 */ LocalUser *unk4;                      /* inferred */
} LocalUser;                                        /* size >= 0x8 */

typedef struct MetaPerformer {
    /* 0x00 */ char pad0[0x29];
    /* 0x29 */ u8 unk29;                            /* inferred */
    /* 0x2A */ u8 unk2A;                            /* inferred */
} MetaPerformer;                                    /* size >= 0x2B */

void *Current__13MetaPerformerFv(MetaPerformer *this); /* extern */
void **GetGameplayOptions__8BandUserFv(BandUser *this); /* extern */
s32 GetTrackType__8BandUserCFv(BandUser *this);     /* extern */
s32 GetUsingRealDrums__8SongDataCFv(SongData *this); /* extern */
? SetGameCymbalLanes__8SongDataFUi(SongData *this, u32 arg0); /* extern */
? SetUseDiscoUnflip__8SongDataFb(SongData *this, s32 arg0); /* extern */
s32 UserHasGHDrums__FP9LocalUser(LocalUser *arg0);  /* extern */
extern MetaPerformer *TheGame;
extern void *TheSongDB;

/* GemPlayer::UpdateGameCymbalLanes (void) */
void UpdateGameCymbalLanes__9GemPlayerFv(GemPlayer *this, ? arg_sp0) {
    BandUser *temp_r3;
    BandUser *temp_r3_2;
    LocalUser *var_r3;
    SongData *temp_r27;
    s32 var_r28;
    s32 var_r31;
    u8 temp_r28;
    u8 temp_r29;

    if (GetTrackType__8BandUserCFv(this->unk230) == 0) {
        var_r31 = 0;
        var_r28 = 0;
        if (this->unk4->unk2C(this) == 0) {
            temp_r3 = this->unk230;
            var_r3 = temp_r3->unk4->unk28(temp_r3);
            if (var_r3 != NULL) {
                var_r3 = var_r3->unk4;
            }
            if (UserHasGHDrums__FP9LocalUser(var_r3) != 0) {
                var_r28 = 1;
            }
        }
        if ((var_r28 != 0) && ((*GetGameplayOptions__8BandUserFv(this->unk230))->unk1C() == 0)) {
            var_r31 = 1;
        }
        temp_r27 = TheSongDB->unk4;
        if (GetUsingRealDrums__8SongDataCFv(temp_r27) != 0) {
            temp_r3_2 = this->unk230;
            this->unk2E0 = temp_r3_2->unk4->unk24(temp_r3_2);
            temp_r28 = TheGame->unk29;
            temp_r29 = TheGame->unk2A;
            if ((s32) Current__13MetaPerformerFv(TheGame)->unk35D != 0) {
                this->unk2E0 = 0x1C;
            } else if ((s32) temp_r28 != 0) {
                this->unk2E0 = 0x1C;
            } else if ((s32) temp_r29 != 0) {
                this->unk2E0 = 0;
            }
            if (this->unk2E0 & 4) {
                var_r31 = 1;
            }
        } else {
            this->unk2E0 = 0;
            var_r31 = 0;
        }
        SetUseDiscoUnflip__8SongDataFb(temp_r27, var_r31);
        SetGameCymbalLanes__8SongDataFUi(temp_r27, this->unk2E0);
    }
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# GemPlayer::UpdateGameCymbalLanes()
.fn UpdateGameCymbalLanes__9GemPlayerFv, global
/* 8019C600 00191020  94 21 FF E0 */	stwu r1, -0x20(r1)
/* 8019C604 00191024  7C 08 02 A6 */	mflr r0
/* 8019C608 00191028  90 01 00 24 */	stw r0, 0x24(r1)
/* 8019C60C 0019102C  39 61 00 20 */	addi r11, r1, 0x20
/* 8019C610 00191030  48 89 CD 21 */	bl _savegpr_27
/* 8019C614 00191034  7C 7E 1B 78 */	mr r30, r3
/* 8019C618 00191038  80 63 02 30 */	lwz r3, 0x230(r3)
/* 8019C61C 0019103C  4B FC 5E 75 */	bl GetTrackType__8BandUserCFv
/* 8019C620 00191040  2C 03 00 00 */	cmpwi r3, 0x0
/* 8019C624 00191044  40 82 01 44 */	bne .L_8019C768
/* 8019C628 00191048  81 9E 00 04 */	lwz r12, 0x4(r30)
/* 8019C62C 0019104C  7F C3 F3 78 */	mr r3, r30
/* 8019C630 00191050  3B E0 00 00 */	li r31, 0x0
/* 8019C634 00191054  3B 80 00 00 */	li r28, 0x0
/* 8019C638 00191058  81 8C 00 2C */	lwz r12, 0x2c(r12)
/* 8019C63C 0019105C  7D 89 03 A6 */	mtctr r12
/* 8019C640 00191060  4E 80 04 21 */	bctrl
/* 8019C644 00191064  7C 60 00 34 */	cntlzw r0, r3
/* 8019C648 00191068  54 00 D9 7F */	srwi. r0, r0, 5
/* 8019C64C 0019106C  41 82 00 34 */	beq .L_8019C680
/* 8019C650 00191070  80 7E 02 30 */	lwz r3, 0x230(r30)
/* 8019C654 00191074  81 83 00 04 */	lwz r12, 0x4(r3)
/* 8019C658 00191078  81 8C 00 28 */	lwz r12, 0x28(r12)
/* 8019C65C 0019107C  7D 89 03 A6 */	mtctr r12
/* 8019C660 00191080  4E 80 04 21 */	bctrl
/* 8019C664 00191084  2C 03 00 00 */	cmpwi r3, 0x0
/* 8019C668 00191088  41 82 00 08 */	beq .L_8019C670
/* 8019C66C 0019108C  80 63 00 04 */	lwz r3, 0x4(r3)
.L_8019C670:
/* 8019C670 00191090  48 29 3A 81 */	bl UserHasGHDrums__FP9LocalUser
/* 8019C674 00191094  2C 03 00 00 */	cmpwi r3, 0x0
/* 8019C678 00191098  41 82 00 08 */	beq .L_8019C680
/* 8019C67C 0019109C  3B 80 00 01 */	li r28, 0x1
.L_8019C680:
/* 8019C680 001910A0  2C 1C 00 00 */	cmpwi r28, 0x0
/* 8019C684 001910A4  41 82 00 28 */	beq .L_8019C6AC
/* 8019C688 001910A8  80 7E 02 30 */	lwz r3, 0x230(r30)
/* 8019C68C 001910AC  4B FC 66 15 */	bl GetGameplayOptions__8BandUserFv
/* 8019C690 001910B0  81 83 00 00 */	lwz r12, 0x0(r3)
/* 8019C694 001910B4  81 8C 00 1C */	lwz r12, 0x1c(r12)
/* 8019C698 001910B8  7D 89 03 A6 */	mtctr r12
/* 8019C69C 001910BC  4E 80 04 21 */	bctrl
/* 8019C6A0 001910C0  2C 03 00 00 */	cmpwi r3, 0x0
/* 8019C6A4 001910C4  40 82 00 08 */	bne .L_8019C6AC
/* 8019C6A8 001910C8  3B E0 00 01 */	li r31, 0x1
.L_8019C6AC:
/* 8019C6AC 001910CC  3C 60 80 C9 */	lis r3, TheSongDB@ha
/* 8019C6B0 001910D0  80 63 F0 48 */	lwz r3, TheSongDB@l(r3)
/* 8019C6B4 001910D4  83 63 00 04 */	lwz r27, 0x4(r3)
/* 8019C6B8 001910D8  7F 63 DB 78 */	mr r3, r27
/* 8019C6BC 001910DC  48 4B 62 05 */	bl GetUsingRealDrums__8SongDataCFv
/* 8019C6C0 001910E0  2C 03 00 00 */	cmpwi r3, 0x0
/* 8019C6C4 001910E4  41 82 00 80 */	beq .L_8019C744
/* 8019C6C8 001910E8  80 7E 02 30 */	lwz r3, 0x230(r30)
/* 8019C6CC 001910EC  81 83 00 04 */	lwz r12, 0x4(r3)
/* 8019C6D0 001910F0  81 8C 00 24 */	lwz r12, 0x24(r12)
/* 8019C6D4 001910F4  7D 89 03 A6 */	mtctr r12
/* 8019C6D8 001910F8  4E 80 04 21 */	bctrl
/* 8019C6DC 001910FC  90 7E 02 E0 */	stw r3, 0x2e0(r30)
/* 8019C6E0 00191100  3C 60 80 C9 */	lis r3, TheGame@ha
/* 8019C6E4 00191104  80 63 EB 18 */	lwz r3, TheGame@l(r3)
/* 8019C6E8 00191108  8B 83 00 29 */	lbz r28, 0x29(r3)
/* 8019C6EC 0019110C  8B A3 00 2A */	lbz r29, 0x2a(r3)
/* 8019C6F0 00191110  48 15 03 E1 */	bl Current__13MetaPerformerFv
/* 8019C6F4 00191114  88 03 03 5D */	lbz r0, 0x35d(r3)
/* 8019C6F8 00191118  2C 00 00 00 */	cmpwi r0, 0x0
/* 8019C6FC 0019111C  41 82 00 10 */	beq .L_8019C70C
/* 8019C700 00191120  38 00 00 1C */	li r0, 0x1c
/* 8019C704 00191124  90 1E 02 E0 */	stw r0, 0x2e0(r30)
/* 8019C708 00191128  48 00 00 28 */	b .L_8019C730
.L_8019C70C:
/* 8019C70C 0019112C  2C 1C 00 00 */	cmpwi r28, 0x0
/* 8019C710 00191130  41 82 00 10 */	beq .L_8019C720
/* 8019C714 00191134  38 00 00 1C */	li r0, 0x1c
/* 8019C718 00191138  90 1E 02 E0 */	stw r0, 0x2e0(r30)
/* 8019C71C 0019113C  48 00 00 14 */	b .L_8019C730
.L_8019C720:
/* 8019C720 00191140  2C 1D 00 00 */	cmpwi r29, 0x0
/* 8019C724 00191144  41 82 00 0C */	beq .L_8019C730
/* 8019C728 00191148  38 00 00 00 */	li r0, 0x0
/* 8019C72C 0019114C  90 1E 02 E0 */	stw r0, 0x2e0(r30)
.L_8019C730:
/* 8019C730 00191150  80 1E 02 E0 */	lwz r0, 0x2e0(r30)
/* 8019C734 00191154  54 00 07 7B */	rlwinm. r0, r0, 0, 29, 29
/* 8019C738 00191158  41 82 00 18 */	beq .L_8019C750
/* 8019C73C 0019115C  3B E0 00 01 */	li r31, 0x1
/* 8019C740 00191160  48 00 00 10 */	b .L_8019C750
.L_8019C744:
/* 8019C744 00191164  38 00 00 00 */	li r0, 0x0
/* 8019C748 00191168  90 1E 02 E0 */	stw r0, 0x2e0(r30)
/* 8019C74C 0019116C  3B E0 00 00 */	li r31, 0x0
.L_8019C750:
/* 8019C750 00191170  7F 63 DB 78 */	mr r3, r27
/* 8019C754 00191174  7F E4 FB 78 */	mr r4, r31
/* 8019C758 00191178  48 4B 61 79 */	bl SetUseDiscoUnflip__8SongDataFb
/* 8019C75C 0019117C  80 9E 02 E0 */	lwz r4, 0x2e0(r30)
/* 8019C760 00191180  7F 63 DB 78 */	mr r3, r27
/* 8019C764 00191184  48 4B 61 8D */	bl SetGameCymbalLanes__8SongDataFUi
.L_8019C768:
/* 8019C768 00191188  39 61 00 20 */	addi r11, r1, 0x20
/* 8019C76C 0019118C  48 89 CC 11 */	bl _restgpr_27
/* 8019C770 00191190  80 01 00 24 */	lwz r0, 0x24(r1)
/* 8019C774 00191194  7C 08 03 A6 */	mtlr r0
/* 8019C778 00191198  38 21 00 20 */	addi r1, r1, 0x20
/* 8019C77C 0019119C  4E 80 00 20 */	blr
.endfn UpdateGameCymbalLanes__9GemPlayerFv
```
