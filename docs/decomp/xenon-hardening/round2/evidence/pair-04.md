# Pair 04 — verification evidence

**Claimed identity:** Wii `Poll__7NetSyncFv`  ==  Xenon `0x82586258`

| field | value |
|---|---|
| pair_id | 04 |
| stratum | BSIM>=30 |
| match_type | `BSIM` |
| BSim sim×conf | 40.168 |
| BSim similarity / confidence | 0.746 / 53.844 |
| TU (Wii) | `NetSync.o` |
| Wii symbol (demangled) | `NetSync::Poll(...)` |
| Wii addr (Bank 8) | `0x8030a270` |
| Xenon addr | `0x82586258` |
| Xenon func name | `Function_82586258` (stripped binary — name is auto-generated) |
| Wii body size | 62 asm lines (lines 1330-1390 in `build/SZBE69_B8/asm/band3/meta_band/NetSync.s`) |
| Xenon body size | 224 bytes |

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
| `0x82593d30` | `Function_82593D30` | `DispatchRMCCall__Q26Quazal17_DOC_VoiceChannelFRCQ26Quazal19CallMethodOperation` |
| `0x82592570` | `FUN_82592570` | `InLock__11LockStepMgrCFv` |
| `0x82585328` | `Function_82585328` | `AttemptTransition__7NetSyncFP8UIScreeni` |

## Referenced strings (Xenon side, 0)

_(none)_

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

void Function_82586258(int param_1)

{
  int *piVar1;
  char cVar2;
  
  if (*(int *)(PTR_DAT_82c41b48 + 0x10) == 0) {
    if ((*(int *)(param_1 + 0x2c) != 0) && (*(int *)(PTR_DAT_82c41b48 + 0x2c) != 0)) {
      Function_82585328(param_1,*(int *)(param_1 + 0x2c),*(undefined4 *)(param_1 + 0x30));
    }
  }
  else {
    piVar1 = *(int **)(PTR_DAT_82c41b48 + 0x2c);
    if ((*(int **)(PTR_DAT_82c41b48 + 0x30) != (int *)0x0) &&
       (cVar2 = (**(code **)(**(int **)(PTR_DAT_82c41b48 + 0x30) + 0x60))(), cVar2 == '\0')) {
      return;
    }
    if ((piVar1 != (int *)0x0) && (cVar2 = (**(code **)(*piVar1 + 0x7c))(piVar1), cVar2 != '\0')) {
      return;
    }
    cVar2 = FUN_82592570(*(undefined4 *)(param_1 + 0x38));
    if ((cVar2 != '\0') && (*(char *)(*(int *)(param_1 + 0x38) + 0x38) == '\0')) {
      Function_82593D30(*(int *)(param_1 + 0x38),1);
    }
  }
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct LockStepMgr {
    /* 0x00 */ char pad0[0x28];
    /* 0x28 */ u8 unk28;                            /* inferred */
} LockStepMgr;                                      /* size >= 0x29 */

typedef struct NetSync {
    /* 0x00 */ char pad0[0x20];
    /* 0x20 */ UIScreen *unk20;                     /* inferred */
    /* 0x24 */ s32 unk24;                           /* inferred */
    /* 0x28 */ char pad28[4];
    /* 0x2C */ LockStepMgr *unk2C;                  /* inferred */
} NetSync;                                          /* size >= 0x30 */

s32 InLock__11LockStepMgrCFv(LockStepMgr *this);    /* extern */
? RespondToLock__11LockStepMgrFb(LockStepMgr *this, s32 arg0); /* extern */
? AttemptTransition__7NetSyncFP8UIScreeni(NetSync *this, UIScreen *arg0, s32 arg1); /* static */
extern void *TheUI;

/* NetSync::Poll (void) */
void Poll__7NetSyncFv(NetSync *this) {
    LockStepMgr *temp_r3_2;
    UIScreen *temp_r4;
    void **temp_r31;
    void **temp_r3;

    if ((s32) TheUI->unk8 != 0) {
        temp_r3 = TheUI->unk24;
        temp_r31 = TheUI->unk20;
        if (((temp_r3 == NULL) || ((*temp_r3)->unk68() != 0)) && ((temp_r31 == NULL) || ((*temp_r31)->unk84(temp_r31) == 0)) && (InLock__11LockStepMgrCFv(this->unk2C) != 0)) {
            temp_r3_2 = this->unk2C;
            if ((s32) temp_r3_2->unk28 == 0) {
                RespondToLock__11LockStepMgrFb(temp_r3_2, 1);
            }
        }
    } else {
        temp_r4 = this->unk20;
        if ((temp_r4 != NULL) && ((void **) TheUI->unk20 != NULL)) {
            AttemptTransition__7NetSyncFP8UIScreeni(this, temp_r4, this->unk24);
        }
    }
}
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# NetSync::Poll()
.fn Poll__7NetSyncFv, global
/* 8030A270 002FEC90  94 21 FF F0 */	stwu r1, -0x10(r1)
/* 8030A274 002FEC94  7C 08 02 A6 */	mflr r0
/* 8030A278 002FEC98  3C 80 80 C9 */	lis r4, TheUI@ha
/* 8030A27C 002FEC9C  90 01 00 14 */	stw r0, 0x14(r1)
/* 8030A280 002FECA0  93 E1 00 0C */	stw r31, 0xc(r1)
/* 8030A284 002FECA4  93 C1 00 08 */	stw r30, 0x8(r1)
/* 8030A288 002FECA8  7C 7E 1B 78 */	mr r30, r3
/* 8030A28C 002FECAC  80 A4 F9 30 */	lwz r5, TheUI@l(r4)
/* 8030A290 002FECB0  80 05 00 08 */	lwz r0, 0x8(r5)
/* 8030A294 002FECB4  2C 00 00 00 */	cmpwi r0, 0x0
/* 8030A298 002FECB8  41 82 00 7C */	beq .L_8030A314
/* 8030A29C 002FECBC  80 65 00 24 */	lwz r3, 0x24(r5)
/* 8030A2A0 002FECC0  83 E5 00 20 */	lwz r31, 0x20(r5)
/* 8030A2A4 002FECC4  2C 03 00 00 */	cmpwi r3, 0x0
/* 8030A2A8 002FECC8  41 82 00 1C */	beq .L_8030A2C4
/* 8030A2AC 002FECCC  81 83 00 00 */	lwz r12, 0x0(r3)
/* 8030A2B0 002FECD0  81 8C 00 68 */	lwz r12, 0x68(r12)
/* 8030A2B4 002FECD4  7D 89 03 A6 */	mtctr r12
/* 8030A2B8 002FECD8  4E 80 04 21 */	bctrl
/* 8030A2BC 002FECDC  2C 03 00 00 */	cmpwi r3, 0x0
/* 8030A2C0 002FECE0  41 82 00 74 */	beq .L_8030A334
.L_8030A2C4:
/* 8030A2C4 002FECE4  2C 1F 00 00 */	cmpwi r31, 0x0
/* 8030A2C8 002FECE8  41 82 00 20 */	beq .L_8030A2E8
/* 8030A2CC 002FECEC  81 9F 00 00 */	lwz r12, 0x0(r31)
/* 8030A2D0 002FECF0  7F E3 FB 78 */	mr r3, r31
/* 8030A2D4 002FECF4  81 8C 00 84 */	lwz r12, 0x84(r12)
/* 8030A2D8 002FECF8  7D 89 03 A6 */	mtctr r12
/* 8030A2DC 002FECFC  4E 80 04 21 */	bctrl
/* 8030A2E0 002FED00  2C 03 00 00 */	cmpwi r3, 0x0
/* 8030A2E4 002FED04  40 82 00 50 */	bne .L_8030A334
.L_8030A2E8:
/* 8030A2E8 002FED08  80 7E 00 2C */	lwz r3, 0x2c(r30)
/* 8030A2EC 002FED0C  4B FC 89 35 */	bl InLock__11LockStepMgrCFv
/* 8030A2F0 002FED10  2C 03 00 00 */	cmpwi r3, 0x0
/* 8030A2F4 002FED14  41 82 00 40 */	beq .L_8030A334
/* 8030A2F8 002FED18  80 7E 00 2C */	lwz r3, 0x2c(r30)
/* 8030A2FC 002FED1C  88 03 00 28 */	lbz r0, 0x28(r3)
/* 8030A300 002FED20  2C 00 00 00 */	cmpwi r0, 0x0
/* 8030A304 002FED24  40 82 00 30 */	bne .L_8030A334
/* 8030A308 002FED28  38 80 00 01 */	li r4, 0x1
/* 8030A30C 002FED2C  4B FC 8C E5 */	bl RespondToLock__11LockStepMgrFb
/* 8030A310 002FED30  48 00 00 24 */	b .L_8030A334
.L_8030A314:
/* 8030A314 002FED34  80 83 00 20 */	lwz r4, 0x20(r3)
/* 8030A318 002FED38  2C 04 00 00 */	cmpwi r4, 0x0
/* 8030A31C 002FED3C  41 82 00 18 */	beq .L_8030A334
/* 8030A320 002FED40  80 05 00 20 */	lwz r0, 0x20(r5)
/* 8030A324 002FED44  2C 00 00 00 */	cmpwi r0, 0x0
/* 8030A328 002FED48  41 82 00 0C */	beq .L_8030A334
/* 8030A32C 002FED4C  80 A3 00 24 */	lwz r5, 0x24(r3)
/* 8030A330 002FED50  48 00 00 21 */	bl AttemptTransition__7NetSyncFP8UIScreeni
.L_8030A334:
/* 8030A334 002FED54  80 01 00 14 */	lwz r0, 0x14(r1)
/* 8030A338 002FED58  83 E1 00 0C */	lwz r31, 0xc(r1)
/* 8030A33C 002FED5C  83 C1 00 08 */	lwz r30, 0x8(r1)
/* 8030A340 002FED60  7C 08 03 A6 */	mtlr r0
/* 8030A344 002FED64  38 21 00 10 */	addi r1, r1, 0x10
/* 8030A348 002FED68  4E 80 00 20 */	blr
.endfn Poll__7NetSyncFv
```
