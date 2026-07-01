# Pair 06 — verification evidence

**Claimed identity:** Wii `Poll__7TrackerFf`  ==  Xenon `0x826b1f50`

| field | value |
|---|---|
| pair_id | 06 |
| stratum | BSIM>=30 |
| match_type | `BSIM` |
| BSim sim×conf | 40.282 |
| BSim similarity / confidence | 1.0 / 40.282 |
| TU (Wii) | `Tracker.o` |
| Wii symbol (demangled) | `Tracker::Poll(...)` |
| Wii addr (Bank 8) | `0x801eeae0` |
| Xenon addr | `0x826b1f50` |
| Xenon func name | `Function_826B1F50` (stripped binary — name is auto-generated) |
| Wii body size | 44 asm lines (lines 840-882 in `build/SZBE69_B8/asm/band3/game/Tracker.s`) |
| Xenon body size | 176 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (5 total, 4 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x826b1c78` | `Function_826B1C78` | `SetupDisplays__7TrackerFv` |
| `0x82b5fe98` | `Function_82B5FE98` | `SetSuppressPlayerFeedback__10TrackPanelFb` |
| `0x826b1b48` | `Function_826B1B48` | _(unmatched / Function_)_ |
| `0x82b5e878` | `FUN_82b5e878` | `GetTrackPanel__Fv` |
| `0x82b5fe00` | `Function_82B5FE00` | `SetSuppressTambourineDisplay__10TrackPanelFb` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_826B1F50(undefined8 param_1,int *param_2)

{
  undefined1 uVar1;
  undefined8 uVar2;
  
  if (*(char *)(param_2 + 1) != '\0') {
    Function_826B1B48(param_2,0xffffffffffffffff);
    (**(code **)(*param_2 + 0x30))(param_1,param_2);
    *(undefined1 *)(param_2 + 1) = 0;
    uVar1 = *(undefined1 *)((int)param_2 + 0x2d);
    uVar2 = FUN_82b5e878();
    Function_82B5FE00(uVar2,uVar1);
    uVar1 = *(undefined1 *)((int)param_2 + 0x2e);
    uVar2 = FUN_82b5e878();
    Function_82B5FE98(uVar2,uVar1);
    Function_826B1C78(param_2);
  }
  (**(code **)(*param_2 + 0x34))(param_1,param_2);
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct Tracker {
    /* 0x00 */ void *unk0;                          /* inferred */
    /* 0x04 */ u8 unk4;                             /* inferred */
    /* 0x05 */ char pad5[0x24];                     /* maybe part of unk4[0x25]? */
    /* 0x29 */ u8 unk29;                            /* inferred */
    /* 0x2A */ u8 unk2A;                            /* inferred */
} Tracker;                                          /* size >= 0x2B */

TrackPanel *GetTrackPanel__Fv();                    /* extern */
? SetSuppressPlayerFeedback__10TrackPanelFb(TrackPanel *this, u8 arg0); /* extern */
? SetSuppressTambourineDisplay__10TrackPanelFb(TrackPanel *this, u8 arg0); /* extern */
? ReachedTargetLevel__7TrackerFi(Tracker *this, s32 arg0); /* static */
? SetupDisplays__7TrackerFv(Tracker *this);         /* static */

/* Tracker::Poll (float) */
void Poll__7TrackerFf(Tracker *this, f32 arg0) {
    if ((s32) this->unk4 != 0) {
        ReachedTargetLevel__7TrackerFi(this, -1);
        this->unk0->unk38(this, arg0);
        this->unk4 = 0;
        SetSuppressTambourineDisplay__10TrackPanelFb(GetTrackPanel__Fv(), this->unk29);
        SetSuppressPlayerFeedback__10TrackPanelFb(GetTrackPanel__Fv(), this->unk2A);
        SetupDisplays__7TrackerFv(this);
    }
    this->unk0->unk3C(this, arg0);
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# Tracker::Poll(float)
.fn Poll__7TrackerFf, global
/* 801EEAE0 001E3500  94 21 FF E0 */	stwu r1, -0x20(r1)
/* 801EEAE4 001E3504  7C 08 02 A6 */	mflr r0
/* 801EEAE8 001E3508  90 01 00 24 */	stw r0, 0x24(r1)
/* 801EEAEC 001E350C  DB E1 00 18 */	stfd f31, 0x18(r1)
/* 801EEAF0 001E3510  FF E0 08 90 */	fmr f31, f1
/* 801EEAF4 001E3514  93 E1 00 14 */	stw r31, 0x14(r1)
/* 801EEAF8 001E3518  7C 7F 1B 78 */	mr r31, r3
/* 801EEAFC 001E351C  88 03 00 04 */	lbz r0, 0x4(r3)
/* 801EEB00 001E3520  2C 00 00 00 */	cmpwi r0, 0x0
/* 801EEB04 001E3524  41 82 00 4C */	beq .L_801EEB50
/* 801EEB08 001E3528  38 80 FF FF */	li r4, -0x1
/* 801EEB0C 001E352C  48 00 04 15 */	bl ReachedTargetLevel__7TrackerFi
/* 801EEB10 001E3530  81 9F 00 00 */	lwz r12, 0x0(r31)
/* 801EEB14 001E3534  FC 20 F8 90 */	fmr f1, f31
/* 801EEB18 001E3538  7F E3 FB 78 */	mr r3, r31
/* 801EEB1C 001E353C  81 8C 00 38 */	lwz r12, 0x38(r12)
/* 801EEB20 001E3540  7D 89 03 A6 */	mtctr r12
/* 801EEB24 001E3544  4E 80 04 21 */	bctrl
/* 801EEB28 001E3548  38 00 00 00 */	li r0, 0x0
/* 801EEB2C 001E354C  98 1F 00 04 */	stb r0, 0x4(r31)
/* 801EEB30 001E3550  4B F5 9B 11 */	bl GetTrackPanel__Fv
/* 801EEB34 001E3554  88 9F 00 29 */	lbz r4, 0x29(r31)
/* 801EEB38 001E3558  4B F5 D6 59 */	bl SetSuppressTambourineDisplay__10TrackPanelFb
/* 801EEB3C 001E355C  4B F5 9B 05 */	bl GetTrackPanel__Fv
/* 801EEB40 001E3560  88 9F 00 2A */	lbz r4, 0x2a(r31)
/* 801EEB44 001E3564  4B F5 D6 FD */	bl SetSuppressPlayerFeedback__10TrackPanelFb
/* 801EEB48 001E3568  7F E3 FB 78 */	mr r3, r31
/* 801EEB4C 001E356C  48 00 05 85 */	bl SetupDisplays__7TrackerFv
.L_801EEB50:
/* 801EEB50 001E3570  81 9F 00 00 */	lwz r12, 0x0(r31)
/* 801EEB54 001E3574  FC 20 F8 90 */	fmr f1, f31
/* 801EEB58 001E3578  7F E3 FB 78 */	mr r3, r31
/* 801EEB5C 001E357C  81 8C 00 3C */	lwz r12, 0x3c(r12)
/* 801EEB60 001E3580  7D 89 03 A6 */	mtctr r12
/* 801EEB64 001E3584  4E 80 04 21 */	bctrl
/* 801EEB68 001E3588  80 01 00 24 */	lwz r0, 0x24(r1)
/* 801EEB6C 001E358C  CB E1 00 18 */	lfd f31, 0x18(r1)
/* 801EEB70 001E3590  83 E1 00 14 */	lwz r31, 0x14(r1)
/* 801EEB74 001E3594  7C 08 03 A6 */	mtlr r0
/* 801EEB78 001E3598  38 21 00 20 */	addi r1, r1, 0x20
/* 801EEB7C 001E359C  4E 80 00 20 */	blr
.endfn Poll__7TrackerFf
```
