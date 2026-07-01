# Pair 07 — verification evidence

**Claimed identity:** Wii `ProcessStaticLyrics__10VocalTrackFbP5LyricRfRfRP5LyricRP5LyricRfbP10LyricPlate`  ==  Xenon `0x82b6e140`

| field | value |
|---|---|
| pair_id | 07 |
| stratum | BSIM 20-30 |
| match_type | `BSIM` |
| BSim sim×conf | 21.823 |
| BSim similarity / confidence | 0.505 / 43.214 |
| TU (Wii) | `VocalTrack.o` |
| Wii symbol (demangled) | `VocalTrack::ProcessStaticLyrics(...)` |
| Wii addr (Bank 8) | `0x80157d00` |
| Xenon addr | `0x82b6e140` |
| Xenon func name | `Function_82B6E140` (stripped binary — name is auto-generated) |
| Wii body size | 98 asm lines (lines 11317-11413 in `build/SZBE69_B8/asm/band3/bandtrack/VocalTrack.s`) |
| Xenon body size | 312 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (4 total, 4 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x82b7cad0` | `FUN_82b7cad0` | `EstimateLyricWidth__10LyricPlateFPC5Lyric` |
| `0x82b7cab8` | `FUN_82b7cab8` | `SetChunkEnd__5LyricFb` |
| `0x82b7cac8` | `FUN_82b7cac8` | `SetAfterMidPhraseLyricShift__5LyricFb` |
| `0x82803f2c` | `FUN_82803f2c` | `FUN_80a39378` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_82B6E140(undefined8 param_1,ulonglong param_2,undefined8 param_3,float *param_4,
                      float *param_5,int *param_6,int *param_7,float *param_8)

{
  float fVar1;
  int iVar2;
  double dVar3;
  double dVar4;
  double dVar5;
  char in_stack_00000057;
  undefined4 in_stack_0000005c;
  
  iVar2 = FUN_82803f2c();
  if ((param_2 & 0xff) != 0) {
    if (in_stack_00000057 != '\0') {
      fVar1 = *param_5;
      *param_4 = fVar1;
      *param_8 = fVar1;
      *param_6 = 0;
      *param_7 = 0;
    }
    dVar5 = (double)(*(float *)(*(int *)(iVar2 + 0x98) + 0x480) -
                    *(float *)(*(int *)(iVar2 + 0x98) + 0x47c));
    dVar4 = (double)(float)(dVar5 * (double)DAT_8200108c);
    dVar3 = (double)FUN_82b7cad0(in_stack_0000005c,param_3);
    fVar1 = *param_5;
    *param_5 = (float)(dVar3 + (double)fVar1);
    dVar3 = (double)((float)(dVar3 + (double)fVar1) - *param_4);
    iVar2 = (int)param_3;
    if (*param_6 != 0) {
      if (*param_7 == 0) {
        *param_7 = iVar2;
      }
      if (*param_6 != 0) {
        if (dVar5 < dVar3) {
          FUN_82b7cab8(*param_6,1);
          *param_6 = 0;
          *param_4 = *param_8;
          dVar3 = (double)(*param_5 - *param_4);
          FUN_82b7cac8(*param_7,1);
          *param_7 = 0;
        }
        if (*param_6 != 0) {
          return;
        }
      }
    }
    if ((dVar4 < dVar3) && (*(char *)(iVar2 + 0x4c) != '\0')) {
      *param_6 = iVar2;
      *param_8 = *param_5;
    }
  }
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct Lyric {
    /* 0x00 */ char pad0[0x3C];
    /* 0x3C */ u8 unk3C;                            /* inferred */
} Lyric;                                            /* size >= 0x3D */

typedef struct VocalTrack {
    /* 0x00 */ char pad0[0x88];
    /* 0x88 */ void *unk88;                         /* inferred */
} VocalTrack;                                       /* size >= 0x8C */

f32 EstimateLyricWidth__10LyricPlateFPC5Lyric(LyricPlate *this, Lyric *arg0); /* extern */
? SetAfterMidPhraseLyricShift__5LyricFb(Lyric *this, s32 arg0); /* extern */
? SetChunkEnd__5LyricFb(Lyric *this, s32 arg0);     /* extern */
extern f32 @F_0000003f;

/* VocalTrack::ProcessStaticLyrics (bool, Lyric *, float &, float &, Lyric * &, Lyric * &, float &, bool, LyricPlate *) */
void ProcessStaticLyrics__10VocalTrackFbP5LyricRfRfRP5LyricRP5LyricRfbP10LyricPlate(VocalTrack *this, s32 arg0, Lyric *arg1, f32 *arg2, f32 *arg3, Lyric **arg4, Lyric **arg5, f32 *arg6, s32 arg7, LyricPlate *arg8, u8 arg_sp8, LyricPlate *arg_spC) {
    Lyric *temp_r3;
    f32 temp_f0;
    f32 temp_f1;
    f32 temp_f1_2;
    f32 temp_f30;
    f32 temp_f31;
    f32 var_f29;
    void *temp_r5;

    if (arg0 != 0) {
        if ((s32) arg_sp8 != 0) {
            temp_f0 = *arg3;
            *arg2 = temp_f0;
            *arg6 = temp_f0;
            *arg4 = NULL;
            *arg5 = NULL;
        }
        temp_r5 = this->unk88;
        temp_f31 = temp_r5->unk42C - temp_r5->unk428;
        temp_f30 = @F_0000003f * temp_f31;
        temp_f1 = *arg3 + EstimateLyricWidth__10LyricPlateFPC5Lyric(arg_spC, arg1);
        *arg3 = temp_f1;
        var_f29 = temp_f1 - *arg2;
        if (((Lyric *) *arg4 != NULL) && ((Lyric *) *arg5 == NULL)) {
            *arg5 = arg1;
        }
        temp_r3 = *arg4;
        if ((temp_r3 != NULL) && (var_f29 > temp_f31)) {
            SetChunkEnd__5LyricFb(temp_r3, 1);
            *arg4 = NULL;
            temp_f1_2 = *arg6;
            *arg2 = temp_f1_2;
            var_f29 = *arg3 - temp_f1_2;
            SetAfterMidPhraseLyricShift__5LyricFb(*arg5, 1);
            *arg5 = NULL;
        }
        if (((Lyric *) *arg4 == NULL) && (var_f29 > temp_f30) && ((s32) arg1->unk3C != 0)) {
            *arg4 = arg1;
            *arg6 = *arg3;
        }
    }
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# VocalTrack::ProcessStaticLyrics(bool, Lyric*, float&, float&, Lyric*&, Lyric*&, float&, bool, LyricPlate*)
.fn ProcessStaticLyrics__10VocalTrackFbP5LyricRfRfRP5LyricRP5LyricRfbP10LyricPlate, global
/* 80157D00 0014C720  94 21 FF A0 */	stwu r1, -0x60(r1)
/* 80157D04 0014C724  7C 08 02 A6 */	mflr r0
/* 80157D08 0014C728  90 01 00 64 */	stw r0, 0x64(r1)
/* 80157D0C 0014C72C  39 61 00 30 */	addi r11, r1, 0x30
/* 80157D10 0014C730  DB E1 00 50 */	stfd f31, 0x50(r1)
/* 80157D14 0014C734  F3 E1 00 58 */	psq_st f31, 0x58(r1), 0, qr0
/* 80157D18 0014C738  DB C1 00 40 */	stfd f30, 0x40(r1)
/* 80157D1C 0014C73C  F3 C1 00 48 */	psq_st f30, 0x48(r1), 0, qr0
/* 80157D20 0014C740  DB A1 00 30 */	stfd f29, 0x30(r1)
/* 80157D24 0014C744  F3 A1 00 38 */	psq_st f29, 0x38(r1), 0, qr0
/* 80157D28 0014C748  48 8E 16 01 */	bl _savegpr_25
/* 80157D2C 0014C74C  2C 04 00 00 */	cmpwi r4, 0x0
/* 80157D30 0014C750  88 01 00 6B */	lbz r0, 0x6b(r1)
/* 80157D34 0014C754  80 81 00 6C */	lwz r4, 0x6c(r1)
/* 80157D38 0014C758  7C B9 2B 78 */	mr r25, r5
/* 80157D3C 0014C75C  7C DA 33 78 */	mr r26, r6
/* 80157D40 0014C760  7C FB 3B 78 */	mr r27, r7
/* 80157D44 0014C764  7D 1C 43 78 */	mr r28, r8
/* 80157D48 0014C768  7D 3D 4B 78 */	mr r29, r9
/* 80157D4C 0014C76C  7D 5E 53 78 */	mr r30, r10
/* 80157D50 0014C770  41 82 00 EC */	beq .L_80157E3C
/* 80157D54 0014C774  2C 00 00 00 */	cmpwi r0, 0x0
/* 80157D58 0014C778  41 82 00 1C */	beq .L_80157D74
/* 80157D5C 0014C77C  C0 07 00 00 */	lfs f0, 0x0(r7)
/* 80157D60 0014C780  38 00 00 00 */	li r0, 0x0
/* 80157D64 0014C784  D0 06 00 00 */	stfs f0, 0x0(r6)
/* 80157D68 0014C788  D0 0A 00 00 */	stfs f0, 0x0(r10)
/* 80157D6C 0014C78C  90 08 00 00 */	stw r0, 0x0(r8)
/* 80157D70 0014C790  90 09 00 00 */	stw r0, 0x0(r9)
.L_80157D74:
/* 80157D74 0014C794  80 A3 00 88 */	lwz r5, 0x88(r3)
/* 80157D78 0014C798  3C 60 80 B3 */	lis r3, "@F_0000003f"@ha
/* 80157D7C 0014C79C  C0 03 4B E4 */	lfs f0, "@F_0000003f"@l(r3)
/* 80157D80 0014C7A0  7C 83 23 78 */	mr r3, r4
/* 80157D84 0014C7A4  C0 45 04 2C */	lfs f2, 0x42c(r5)
/* 80157D88 0014C7A8  7F 24 CB 78 */	mr r4, r25
/* 80157D8C 0014C7AC  C0 25 04 28 */	lfs f1, 0x428(r5)
/* 80157D90 0014C7B0  EF E2 08 28 */	fsubs f31, f2, f1
/* 80157D94 0014C7B4  EF C0 07 F2 */	fmuls f30, f0, f31
/* 80157D98 0014C7B8  4B FE A8 B9 */	bl EstimateLyricWidth__10LyricPlateFPC5Lyric
/* 80157D9C 0014C7BC  C0 1B 00 00 */	lfs f0, 0x0(r27)
/* 80157DA0 0014C7C0  EC 20 08 2A */	fadds f1, f0, f1
/* 80157DA4 0014C7C4  D0 3B 00 00 */	stfs f1, 0x0(r27)
/* 80157DA8 0014C7C8  C0 1A 00 00 */	lfs f0, 0x0(r26)
/* 80157DAC 0014C7CC  80 1C 00 00 */	lwz r0, 0x0(r28)
/* 80157DB0 0014C7D0  EF A1 00 28 */	fsubs f29, f1, f0
/* 80157DB4 0014C7D4  2C 00 00 00 */	cmpwi r0, 0x0
/* 80157DB8 0014C7D8  41 82 00 14 */	beq .L_80157DCC
/* 80157DBC 0014C7DC  80 1D 00 00 */	lwz r0, 0x0(r29)
/* 80157DC0 0014C7E0  2C 00 00 00 */	cmpwi r0, 0x0
/* 80157DC4 0014C7E4  40 82 00 08 */	bne .L_80157DCC
/* 80157DC8 0014C7E8  93 3D 00 00 */	stw r25, 0x0(r29)
.L_80157DCC:
/* 80157DCC 0014C7EC  80 7C 00 00 */	lwz r3, 0x0(r28)
/* 80157DD0 0014C7F0  2C 03 00 00 */	cmpwi r3, 0x0
/* 80157DD4 0014C7F4  41 82 00 3C */	beq .L_80157E10
/* 80157DD8 0014C7F8  FC 1D F8 40 */	fcmpo cr0, f29, f31
/* 80157DDC 0014C7FC  40 81 00 34 */	ble .L_80157E10
/* 80157DE0 0014C800  38 80 00 01 */	li r4, 0x1
/* 80157DE4 0014C804  4B FE AF 2D */	bl SetChunkEnd__5LyricFb
/* 80157DE8 0014C808  3B E0 00 00 */	li r31, 0x0
/* 80157DEC 0014C80C  93 FC 00 00 */	stw r31, 0x0(r28)
/* 80157DF0 0014C810  38 80 00 01 */	li r4, 0x1
/* 80157DF4 0014C814  C0 3E 00 00 */	lfs f1, 0x0(r30)
/* 80157DF8 0014C818  D0 3A 00 00 */	stfs f1, 0x0(r26)
/* 80157DFC 0014C81C  C0 1B 00 00 */	lfs f0, 0x0(r27)
/* 80157E00 0014C820  80 7D 00 00 */	lwz r3, 0x0(r29)
/* 80157E04 0014C824  EF A0 08 28 */	fsubs f29, f0, f1
/* 80157E08 0014C828  4B FE AF 29 */	bl SetAfterMidPhraseLyricShift__5LyricFb
/* 80157E0C 0014C82C  93 FD 00 00 */	stw r31, 0x0(r29)
.L_80157E10:
/* 80157E10 0014C830  80 1C 00 00 */	lwz r0, 0x0(r28)
/* 80157E14 0014C834  2C 00 00 00 */	cmpwi r0, 0x0
/* 80157E18 0014C838  40 82 00 24 */	bne .L_80157E3C
/* 80157E1C 0014C83C  FC 1D F0 40 */	fcmpo cr0, f29, f30
/* 80157E20 0014C840  40 81 00 1C */	ble .L_80157E3C
/* 80157E24 0014C844  88 19 00 3C */	lbz r0, 0x3c(r25)
/* 80157E28 0014C848  2C 00 00 00 */	cmpwi r0, 0x0
/* 80157E2C 0014C84C  41 82 00 10 */	beq .L_80157E3C
/* 80157E30 0014C850  93 3C 00 00 */	stw r25, 0x0(r28)
/* 80157E34 0014C854  C0 1B 00 00 */	lfs f0, 0x0(r27)
/* 80157E38 0014C858  D0 1E 00 00 */	stfs f0, 0x0(r30)
.L_80157E3C:
/* 80157E3C 0014C85C  39 61 00 30 */	addi r11, r1, 0x30
/* 80157E40 0014C860  E3 E1 00 58 */	psq_l f31, 0x58(r1), 0, qr0
/* 80157E44 0014C864  CB E1 00 50 */	lfd f31, 0x50(r1)
/* 80157E48 0014C868  E3 C1 00 48 */	psq_l f30, 0x48(r1), 0, qr0
/* 80157E4C 0014C86C  CB C1 00 40 */	lfd f30, 0x40(r1)
/* 80157E50 0014C870  E3 A1 00 38 */	psq_l f29, 0x38(r1), 0, qr0
/* 80157E54 0014C874  CB A1 00 30 */	lfd f29, 0x30(r1)
/* 80157E58 0014C878  48 8E 15 1D */	bl _restgpr_25
/* 80157E5C 0014C87C  80 01 00 64 */	lwz r0, 0x64(r1)
/* 80157E60 0014C880  7C 08 03 A6 */	mtlr r0
/* 80157E64 0014C884  38 21 00 60 */	addi r1, r1, 0x60
/* 80157E68 0014C888  4E 80 00 20 */	blr
.endfn ProcessStaticLyrics__10VocalTrackFbP5LyricRfRfRP5LyricRP5LyricRfbP10LyricPlate
```
