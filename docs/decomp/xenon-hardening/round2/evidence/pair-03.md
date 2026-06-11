# Pair 03 — verification evidence

**Claimed identity:** Wii `UpdateState__13OvershellSlotFv`  ==  Xenon `0x825c1698`

| field | value |
|---|---|
| pair_id | 03 |
| stratum | BSIM>=30 |
| match_type | `BSIM` |
| BSim sim×conf | 47.199 |
| BSim similarity / confidence | 0.433 / 109.004 |
| TU (Wii) | `OvershellSlot.o` |
| Wii symbol (demangled) | `OvershellSlot::UpdateState(...)` |
| Wii addr (Bank 8) | `0x803290c0` |
| Xenon addr | `0x825c1698` |
| Xenon func name | `Function_825C1698` (stripped binary — name is auto-generated) |
| Wii body size | 585 asm lines (lines 6731-7314 in `build/SZBE69_B8/asm/band3/meta_band/OvershellSlot.s`) |
| Xenon body size | 2132 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (22 total, 14 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x823cd938` | `FUN_823cd938` | `MainPerformer__4BandCFv` |
| `0x826e0908` | `Function_826E0908` | _(unmatched / Function_)_ |
| `0x82643300` | `Function_82643300` | `InCharEditFlow__18OvershellSlotStateFv` |
| `0x82725d28` | `Function_82725D28` | `__as__8DataNodeFRC8DataNode` |
| `0x827362a0` | `Function_827362A0` | `SetTrackType__8BandUserF9TrackType` |
| `0x82804da8` | `Function_82804DA8` | _(unmatched / Function_)_ |
| `0x8279b788` | `??0Symbol@@QAA@PBD@Z` | `Enter__11RndPollableFv` |
| `0x82664dd0` | `Function_82664DD0` | `GetUserFromSlot__11BandUserMgrCFi` |
| `0x82260570` | `Function_82260570` | `MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc` |
| `0x82642c90` | `Function_82642C90` | _(unmatched / Function_)_ |
| `0x828050f8` | `Function_828050F8` | `MsToTick__Ff` |
| `0x825bed40` | `Function_825BED40` | _(unmatched / Function_)_ |
| `0x82803f28` | `FUN_82803f28` | `__nw__FUl` |
| `0x82642300` | `Function_82642300` | `HandleMsg__18OvershellSlotStateFRC7Message` |
| `0x8259a258` | `Function_8259A258` | _(unmatched / Function_)_ |
| `0x82642740` | `Function_82642740` | _(unmatched / Function_)_ |
| `0x8259ace8` | `Function_8259ACE8` | _(unmatched / Function_)_ |
| `0x82726008` | `Function_82726008` | `GetObj__8DataNodeCFPC9DataArray` |
| `0x82261bc0` | `Function_82261BC0` | _(unmatched / Function_)_ |
| `0x82665a98` | `Function_82665A98` | `Array__9DataArrayCFi` |
| `0x825c0680` | `Function_825C0680` | `ShowEnterFlowPrompt__13OvershellSlotF20OvershellSlotStateID` |
| `0x825c0050` | `Function_825C0050` | `GenerateCurrentState__13OvershellSlotFv` |

## Referenced strings (Xenon side, 4)

- `'overshell_up.cue'`
- `'overshell_down.cue'`
- `'enter'`
- `'swap_user'`

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

/* WARNING: Removing unreachable block (ram,0x825c18f0) */

void Function_825C1698(void)

{
  bool bVar1;
  int iVar3;
  int *piVar4;
  undefined8 uVar2;
  int iVar5;
  int iVar6;
  undefined4 *puVar7;
  char cVar9;
  char cVar10;
  int iVar8;
  double dVar11;
  undefined1 auStack_70 [8];
  undefined1 local_68 [8];
  undefined4 local_60;
  uint local_5c;
  undefined4 local_58;
  uint local_54;
  
  iVar3 = FUN_82803f28();
  piVar4 = (int *)Function_82664DD0(*(undefined4 *)(iVar3 + 0x38),*(undefined4 *)(iVar3 + 0x40));
  uVar2 = Function_825C0050(iVar3);
  if (*(int *)(iVar3 + 0x2c) != 0) {
    iVar5 = FUN_823cd938();
    iVar6 = FUN_823cd938(uVar2);
    if (iVar6 == iVar5) goto LAB_825c1990;
  }
  bVar1 = false;
  if (*(int *)(iVar3 + 0x2c) != 0) {
    if ((DAT_82dce544 & 1) == 0) {
      DAT_82dce544 = DAT_82dce544 | 1;
      puVar7 = (undefined4 *)__0Symbol__QAA_PBD_Z(auStack_70,0xffffffff82049358);
      Function_82261BC0(0xffffffff82dce53c,*puVar7);
      Function_828050F8(0xffffffff82c0e430);
    }
    Function_82642300(&local_60,*(undefined4 *)(iVar3 + 0x2c),0xffffffff82dce53c);
    if ((local_5c & 0x10) != 0) {
      Function_82260570(local_60);
    }
    if ((piVar4 == (int *)0x0) ||
       (cVar9 = (**(code **)(*(int *)((int)piVar4 + *(int *)(piVar4[1] + 4) + 4) + 0x5c))
                          ((int)piVar4 + *(int *)(piVar4[1] + 4) + 4), cVar9 != '\0')) {
      cVar9 = Function_82642C90(*(undefined4 *)(iVar3 + 0x2c));
      dVar11 = (double)DAT_82000d6c;
      if ((cVar9 != '\0') && (cVar9 = Function_82642C90(uVar2), cVar9 == '\0')) {
        Function_826E0908(dVar11,dVar11,dVar11,DAT_82dd1f94,0xffffffff820b2968);
      }
      cVar9 = Function_82642C90(*(undefined4 *)(iVar3 + 0x2c));
      if ((cVar9 == '\0') && (cVar9 = Function_82642C90(uVar2), cVar9 != '\0')) {
        Function_826E0908(dVar11,dVar11,dVar11,DAT_82dd1f94,0xffffffff820b2954);
      }
    }
    cVar9 = Function_82642740(*(undefined4 *)(iVar3 + 0x2c));
    cVar10 = Function_82642740(uVar2);
    if (cVar9 != cVar10) {
      bVar1 = true;
    }
  }
  *(int *)(iVar3 + 0x2c) = (int)uVar2;
  if (bVar1) {
    if ((DAT_82dce544 & 2) == 0) {
      DAT_82dce544 = DAT_82dce544 | 2;
      uVar2 = Function_82664DD0(*(undefined4 *)(iVar3 + 0x38),*(undefined4 *)(iVar3 + 0x40));
      Function_8259ACE8(0xffffffff82dce534,uVar2);
      Function_828050F8(0xffffffff82c0e410);
    }
    Function_82664DD0(*(undefined4 *)(iVar3 + 0x38),*(undefined4 *)(iVar3 + 0x40));
    Function_82725D28((ulonglong)*DAT_82dce538 + 0x10,local_68);
    iVar5 = *(int *)(*(int *)(*(int *)(iVar3 + 0x34) + 4) + 4) + *(int *)(iVar3 + 0x34);
    (**(code **)(*(int *)(iVar5 + 4) + 0x38))(iVar5 + 4,DAT_82dce538,1);
  }
  if ((DAT_82dce544 & 4) == 0) {
    DAT_82dce544 = DAT_82dce544 | 4;
    puVar7 = (undefined4 *)__0Symbol__QAA_PBD_Z(auStack_70,0xffffffff8203b97c);
    Function_82261BC0(0xffffffff82dce52c,*puVar7);
    Function_828050F8(0xffffffff82c0e3f0);
  }
  Function_82642300(&local_58,*(undefined4 *)(iVar3 + 0x2c),0xffffffff82dce52c);
  if ((local_54 & 0x10) != 0) {
    Function_82260570(local_58);
  }
LAB_825c1990:
  if ((piVar4 != (int *)0x0) &&
     (cVar9 = (**(code **)(*(int *)((int)piVar4 + *(int *)(piVar4[1] + 4) + 4) + 0x5c))
                        ((int)piVar4 + *(int *)(piVar4[1] + 4) + 4), cVar9 != '\0')) {
    iVar5 = (**(code **)(*piVar4 + 0x18))(piVar4);
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if ((iVar6 == 0x2f) &&
       (iVar6 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
       cVar9 = (**(code **)(*(int *)(iVar6 + 4) + 0x18))(iVar6 + 4), cVar9 == '\0')) {
      Function_825BED40(iVar3,0x2d);
    }
    cVar9 = Function_82643300(*(undefined4 *)(iVar3 + 0x2c));
    if ((cVar9 != '\0') &&
       (iVar6 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
       cVar9 = (**(code **)(*(int *)(iVar6 + 4) + 0x18))(iVar6 + 4), cVar9 == '\0')) {
      Function_825BED40(iVar3,6);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if ((iVar6 == 0x18) &&
       (iVar6 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
       cVar9 = (**(code **)(*(int *)(iVar6 + 4) + 0x18))(iVar6 + 4), cVar9 == '\0')) {
      Function_825BED40(iVar3,6);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if ((iVar6 == 0x33) &&
       (iVar6 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
       cVar9 = (**(code **)(*(int *)(iVar6 + 4) + 0x18))(iVar6 + 4), cVar9 != '\0')) {
      Function_825BED40(iVar3,6);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if (((iVar6 == 0x10) &&
        (iVar6 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
        cVar9 = (**(code **)(*(int *)(iVar6 + 4) + 0x14))(iVar6 + 4), cVar9 != '\0')) &&
       (iVar6 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
       cVar9 = (**(code **)(*(int *)(iVar6 + 4) + 8))(iVar6 + 4), cVar9 == '\0')) {
      Function_825BED40(iVar3,0x11);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if ((iVar6 == 0x18) && (cVar9 = (**(code **)(**(int **)(iVar3 + 0x3c) + 0x24))(), cVar9 == '\0')
       ) {
      Function_825BED40(iVar3,0x30);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if ((iVar6 == 0x17) && (cVar9 = (**(code **)(**(int **)(iVar3 + 0x3c) + 0x24))(), cVar9 == '\0')
       ) {
      Function_825BED40(iVar3,0x31);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if ((iVar6 == 0x3b) && (cVar9 = (**(code **)(**(int **)(iVar3 + 0x3c) + 0x24))(), cVar9 == '\0')
       ) {
      Function_825BED40(iVar3,0x3c);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if ((iVar6 == 0x1e) && (iVar6 = (**(code **)(**(int **)(iVar3 + 0xb4) + 0x28))(), iVar6 == 0)) {
      Function_825BED40(iVar3,0x48);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if (((iVar6 == 0x13) &&
        (iVar6 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
        cVar9 = (**(code **)(*(int *)(iVar6 + 4) + 8))(iVar6 + 4), cVar9 != '\0')) &&
       (cVar9 = Function_82665A98(*(undefined4 *)(iVar3 + 0x38)), cVar9 == '\0')) {
      Function_825BED40(iVar3,0x14);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if (((iVar6 == 0x33) &&
        (iVar6 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
        cVar9 = (**(code **)(*(int *)(iVar6 + 4) + 8))(iVar6 + 4), cVar9 != '\0')) &&
       (cVar9 = Function_82665A98(*(undefined4 *)(iVar3 + 0x38)), cVar9 == '\0')) {
      Function_825C0680(iVar3,0x18);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if ((iVar6 == 0x36) &&
       (cVar9 = Function_8259A258(*(undefined4 *)(iVar3 + 0x34),*(undefined4 *)(iVar3 + 0x90)),
       cVar9 != '\0')) {
      Function_825BED40(iVar3,0x42);
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if ((iVar6 == 0x1a) && (cVar9 = (**(code **)(**(int **)(iVar3 + 0x3c) + 0x24))(), cVar9 != '\0')
       ) {
      Function_825BED40(iVar3,7);
    }
    iVar6 = *(int *)(*(int *)(*(int *)(iVar3 + 0x3c) + 0x54) + 0x28);
    if (iVar6 == 0) {
      iVar6 = 0;
    }
    else {
      iVar6 = *(int *)(*(int *)(iVar6 + 4) + 8) + iVar6 + 4;
    }
    iVar8 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if (iVar8 == 0x41) {
      if (iVar5 == 0) {
        iVar8 = 0;
      }
      else {
        iVar8 = *(int *)(*(int *)(iVar5 + 4) + 8) + iVar5 + 4;
      }
      if (iVar6 != iVar8) {
        Function_825BED40(iVar3,5);
        *(undefined1 *)(iVar3 + 0x61) = 1;
      }
    }
    iVar6 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if ((iVar6 == 0x37) &&
       (iVar5 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
       cVar9 = (**(code **)(*(int *)(iVar5 + 4) + 0x18))(iVar5 + 4), cVar9 == '\0')) {
      Function_825BED40(iVar3,5);
      *(undefined1 *)(iVar3 + 0x61) = 1;
    }
    iVar5 = FUN_823cd938(*(undefined4 *)(iVar3 + 0x2c));
    if (iVar5 == 0x20) {
      puVar7 = (undefined4 *)__0Symbol__QAA_PBD_Z(auStack_70,0xffffffff820b2948);
      uVar2 = Function_827362A0(*(undefined4 *)(iVar3 + 0x2c),*puVar7,1);
      uVar2 = Function_82726008(uVar2,0);
      iVar5 = Function_82804DA8(uVar2,0,0xffffffff82c349c4,0xffffffff82c41e80,0);
      iVar6 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5;
      cVar9 = (**(code **)(*(int *)(iVar6 + 4) + 0x10))(iVar6 + 4);
      if (((cVar9 == '\0') ||
          (iVar6 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
          cVar9 = (**(code **)(*(int *)(iVar6 + 4) + 4))(iVar6 + 4), cVar9 == '\0')) ||
         (iVar5 = *(int *)(*(int *)(iVar5 + 4) + 0xc) + iVar5,
         cVar9 = (**(code **)(*(int *)(iVar5 + 4) + 0xc))(iVar5 + 4), cVar9 != '\0')) {
        Function_825BED40(iVar3,0x1f);
      }
    }
  }
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct BandUser {
    /* 0x0 */ DataArray *unk0;                      /* inferred */
    /* 0x4 */ void *unk4;                           /* inferred */
} BandUser;                                         /* size >= 0x8 */

typedef struct DataArray {
    /* 0x0 */ void *unk0;                           /* inferred */
    /* 0x4 */ char pad4[6];                         /* maybe part of unk0[2]? */
    /* 0xA */ s16 unkA;                             /* inferred */
} DataArray;                                        /* size >= 0xC */

typedef struct OvershellAllowingInputChangedMsg {
    /* 0x0 */ char pad0[4];
    /* 0x4 */ DataArray *unk4;                      /* inferred */
} OvershellAllowingInputChangedMsg;                 /* size >= 0x8 */

typedef struct OvershellPanel {
    /* 0x00 */ char pad0[0x54];
    /* 0x54 */ ? unk54;                             /* inferred */
    /* 0x54 */ char pad54[1];
} OvershellPanel;                                   /* size >= 0x55 */

typedef struct OvershellSlot {
    /* 0x00 */ char pad0[0x20];
    /* 0x20 */ OvershellSlotState *unk20;           /* inferred */
    /* 0x24 */ char pad24[0xC];                     /* maybe part of unk20[4]? */
    /* 0x30 */ OvershellPanel *unk30;               /* inferred */
    /* 0x34 */ BandUserMgr *unk34;                  /* inferred */
    /* 0x38 */ void *unk38;                         /* inferred */
    /* 0x3C */ s32 unk3C;                           /* inferred */
    /* 0x40 */ char pad40[0x44];                    /* maybe part of unk3C[0x12]? */
    /* 0x84 */ CharData *unk84;                     /* inferred */
} OvershellSlot;                                    /* size >= 0x88 */

s32 AllLocalUsersInSessionAreGuests__11BandUserMgrCFv(BandUserMgr *this); /* extern */
u32 AllowsInputToShell__18OvershellSlotStateFv(OvershellSlotState *this); /* extern */
s32 DoesAnySlotHaveChar__14OvershellPanelCFP8CharData(OvershellPanel *this, CharData *arg0); /* extern */
? GetObj__8DataNodeCFPC9DataArray(DataNode *this, DataArray *arg0); /* extern */
s32 GetStateID__18OvershellSlotStateCFv(OvershellSlotState *this); /* extern */
BandUser *GetUserFromSlot__11BandUserMgrCFi(BandUserMgr *this, s32 arg0); /* extern */
? HandleMsg__18OvershellSlotStateFRC7Message(OvershellSlotState *this, Message *arg0); /* extern */
s32 HasTransitionEvent__10UIEventMgrCF6Symbol(UIEventMgr *this, Symbol arg0); /* extern */
s32 InCharEditFlow__18OvershellSlotStateFv(OvershellSlotState *this); /* extern */
s32 IsPrimaryProfileCritical__10ProfileMgrFPC9LocalUser(ProfileMgr *this, LocalUser *arg0); /* extern */
DataNode *Node__9DataArrayFi(DataArray *this, s32 arg0); /* extern */
? Play__5SynthFPCcfff(Synth *this, s8 *arg0, f32 arg1, f32 arg2, f32 arg3); /* extern */
DataNode *Property__Q23Hmx6ObjectCF6Symbolb(Hmx::Object *this, Symbol arg0, s32 arg1); /* extern */
s32 RetractedPosition__18OvershellSlotStateFv(OvershellSlotState *this); /* extern */
? __as__8DataNodeFRC8DataNode(DataNode *this, DataNode *arg0); /* extern */
void *__ct__32OvershellAllowingInputChangedMsgFP8BandUser(OvershellAllowingInputChangedMsg *this, BandUser *arg0); /* extern */
void *__ct__6SymbolFPCc(Symbol *this, s8 *arg0);    /* extern */
void *__dt__32OvershellAllowingInputChangedMsgFv(OvershellAllowingInputChangedMsg *this, s16 destroyFlag); /* extern */
void *__dt__9DataArrayFv(DataArray *this, s16 destroyFlag); /* extern */
void *__dynamic_cast(?, struct RTTI *, struct RTTI *, ?); /* extern */
? __register_global_object(OvershellAllowingInputChangedMsg *, void *(*)(OvershellAllowingInputChangedMsg *, s16), ? *); /* extern */
? CancelLinkingCode__13OvershellSlotFv(OvershellSlot *this); /* static */
OvershellSlotState *GenerateCurrentState__13OvershellSlotFv(OvershellSlot *this); /* static */
? LeaveOptions__13OvershellSlotFv(OvershellSlot *this); /* static */
? ShowState__13OvershellSlotF20OvershellSlotStateID(OvershellSlot *this, OvershellSlotStateID arg0); /* static */
extern f32 @F_00000000;
extern u8 @GUARD@UpdateState__13OvershellSlotFv@allowingInputMsg;
extern ProfileMgr TheProfileMgr;
extern void *TheServer;
extern Synth *TheSynth;
extern UIEventMgr *TheUIEventMgr;
extern struct RTTI __RTTI__13LocalBandUser;
extern struct RTTI __RTTI__Q23Hmx6Object;
extern ? enter_msg;
extern ? exit_msg;
static ? @stringBase0;                              /* unable to generate initializer: unknown type */
static ? @56469;
static OvershellAllowingInputChangedMsg @LOCAL@UpdateState__13OvershellSlotFv@allowingInputMsg;

/* OvershellSlot::UpdateState (void) */
void UpdateState__13OvershellSlotFv(OvershellSlot *this, ? arg_sp0) {
    OvershellSlotState sp30;
    s32 sp2C;
    DataArray *sp28;
    OvershellSlotState sp20;
    Symbol sp1C;
    Symbol sp18;
    Symbol sp14;
    Symbol sp10;
    Symbol spC;
    Symbol sp8;
    ? *temp_r3_2;
    ? *var_r28;
    BandUser *temp_r31;
    BandUser *var_r3;
    DataArray *temp_r3;
    DataArray *temp_r3_18;
    DataArray *temp_r3_3;
    OvershellSlotState *temp_r0;
    OvershellSlotState *temp_r30;
    OvershellSlotState *temp_r4;
    s16 temp_r0_2;
    s16 temp_r0_3;
    s16 temp_r0_4;
    s32 temp_r28;
    s32 temp_r28_3;
    s32 temp_r3_5;
    s32 var_r27;
    u32 temp_r28_2;
    void *temp_r30_2;
    void *temp_r3_10;
    void *temp_r3_11;
    void *temp_r3_12;
    void *temp_r3_13;
    void *temp_r3_14;
    void *temp_r3_15;
    void *temp_r3_16;
    void *temp_r3_17;
    void *temp_r3_19;
    void *temp_r3_20;
    void *temp_r3_21;
    void *temp_r3_22;
    void *temp_r3_23;
    void *temp_r3_4;
    void *temp_r3_6;
    void *temp_r3_7;
    void *temp_r3_8;
    void *temp_r3_9;
    void *var_r0;

    temp_r31 = GetUserFromSlot__11BandUserMgrCFi(this->unk34, this->unk3C);
    temp_r0 = this->unk20;
    temp_r30 = GenerateCurrentState__13OvershellSlotFv(this);
    if ((temp_r0 == NULL) || (temp_r28 = GetStateID__18OvershellSlotStateCFv(temp_r0), ((GetStateID__18OvershellSlotStateCFv(temp_r30) == temp_r28) == 0))) {
        temp_r4 = this->unk20;
        var_r27 = 0;
        if (temp_r4 != NULL) {
            HandleMsg__18OvershellSlotStateFRC7Message(&sp30, (Message *) temp_r4);
            if (sp34 & 0x10) {
                temp_r0_2 = ((DataArray *) sp30)->unkA - 1;
                ((DataArray *) sp30)->unkA = temp_r0_2;
                if (temp_r0_2 == 0) {
                    __dt__9DataArrayFv((DataArray *) sp30, 1);
                }
            }
            if ((temp_r31 == NULL) || (temp_r3 = temp_r31->unk0, ((temp_r3->unk0->unk64(temp_r3) == 0) == 0))) {
                if ((RetractedPosition__18OvershellSlotStateFv(this->unk20) != 0) && (RetractedPosition__18OvershellSlotStateFv(temp_r30) == 0)) {
                    Play__5SynthFPCcfff(TheSynth, "setup_providers\0user_name.lbl\0OvershellSlot.cpp\0mUserNameLabel\0mState\0Unknown user entry state %i\n\0pUser\0pLocUser\0pUser->IsLocal()\0reconnect_controller.lbl\0overshell_reconnect_controller\0wii_error_remote_extension_x\0skip_choose_part\0forced_part\0trackType != kTrackVocals\0trackType != kTrackDrum\0track == kTrackVocals || !harmony\0track == kTrackDrum || !proDrums\0cymBit != 0\0skip_choose_diff_prompt\0difficulty\0pUser->GetTrackType() != kTrackNone\0required_song_options_chosen\0kick_user\0pUserToKick != NULL\0mSwappableProfilesProvider\0pLocalUser\0swap_user\0mState->GetStateID() == kState_LinkingCode\0\0code\0pUser->GetControllerType() == kControllerVocals\0pUser->GetControllerType() != kControllerVocals\0in_track_mode\0slot_view\0GetUser()->IsLocal()\0pause_menu_quit_token\0mSlotOverrideFlow == kOverrideFlow_None\0type != kOverrideFlow_None\0mSlotOverrideFlow == type\0overshell_up.cue\0overshell_down.cue\0go_to_wiiprofilecreator\0update_controller_type\0update_user_name\0update_local_status\0update_restart_allowed\0update_sign_in_continue\0update_pad_num\0update_remote_feedback\0update_remote_status\0update_show_vocal_bg\0update_song_difficulty_ranking\0update_lefty_and_static_toggle\0set_difficulty_restriction\0update_mics\0update_online_enabled\0!InGame()\0mCharForEdit != NULL\0illegal attempt made to delete guest character\n\0pProfile\0illegal attempt made to rename guest character to %s\n\0pUser && pUser->IsLocal()\0button_error.cue\0overshell_back.cue\0on_start\0Local user %s cannot join\0play_instr_sfx_local\0slider.cue\0overshell_select.cue\0mCharProvider\0update_char_provider\0update_users_provider\0update_profiles_provider\0update_part_select_provider\0update_character_portrait\0update_char_more_options\0slot state %i is not an Enter Flow Prompt\n\0%s(%d): %s unhandled msg: %s" + 0x359, @F_00000000, @F_00000000, @F_00000000);
                }
                if ((RetractedPosition__18OvershellSlotStateFv(this->unk20) == 0) && (RetractedPosition__18OvershellSlotStateFv(temp_r30) != 0)) {
                    Play__5SynthFPCcfff(TheSynth, "setup_providers\0user_name.lbl\0OvershellSlot.cpp\0mUserNameLabel\0mState\0Unknown user entry state %i\n\0pUser\0pLocUser\0pUser->IsLocal()\0reconnect_controller.lbl\0overshell_reconnect_controller\0wii_error_remote_extension_x\0skip_choose_part\0forced_part\0trackType != kTrackVocals\0trackType != kTrackDrum\0track == kTrackVocals || !harmony\0track == kTrackDrum || !proDrums\0cymBit != 0\0skip_choose_diff_prompt\0difficulty\0pUser->GetTrackType() != kTrackNone\0required_song_options_chosen\0kick_user\0pUserToKick != NULL\0mSwappableProfilesProvider\0pLocalUser\0swap_user\0mState->GetStateID() == kState_LinkingCode\0\0code\0pUser->GetControllerType() == kControllerVocals\0pUser->GetControllerType() != kControllerVocals\0in_track_mode\0slot_view\0GetUser()->IsLocal()\0pause_menu_quit_token\0mSlotOverrideFlow == kOverrideFlow_None\0type != kOverrideFlow_None\0mSlotOverrideFlow == type\0overshell_up.cue\0overshell_down.cue\0go_to_wiiprofilecreator\0update_controller_type\0update_user_name\0update_local_status\0update_restart_allowed\0update_sign_in_continue\0update_pad_num\0update_remote_feedback\0update_remote_status\0update_show_vocal_bg\0update_song_difficulty_ranking\0update_lefty_and_static_toggle\0set_difficulty_restriction\0update_mics\0update_online_enabled\0!InGame()\0mCharForEdit != NULL\0illegal attempt made to delete guest character\n\0pProfile\0illegal attempt made to rename guest character to %s\n\0pUser && pUser->IsLocal()\0button_error.cue\0overshell_back.cue\0on_start\0Local user %s cannot join\0play_instr_sfx_local\0slider.cue\0overshell_select.cue\0mCharProvider\0update_char_provider\0update_users_provider\0update_profiles_provider\0update_part_select_provider\0update_character_portrait\0update_char_more_options\0slot state %i is not an Enter Flow Prompt\n\0%s(%d): %s unhandled msg: %s" + 0x36A, @F_00000000, @F_00000000, @F_00000000);
                }
            }
            temp_r28_2 = AllowsInputToShell__18OvershellSlotStateFv(temp_r30);
            if (AllowsInputToShell__18OvershellSlotStateFv(this->unk20) != temp_r28_2) {
                var_r27 = 1;
            }
        }
        this->unk20 = temp_r30;
        if (var_r27 != 0) {
            if ((s8) @GUARD@UpdateState__13OvershellSlotFv@allowingInputMsg == 0) {
                __ct__32OvershellAllowingInputChangedMsgFP8BandUser(&@LOCAL@UpdateState__13OvershellSlotFv@allowingInputMsg, GetUserFromSlot__11BandUserMgrCFi(this->unk34, this->unk3C));
                __register_global_object(&@LOCAL@UpdateState__13OvershellSlotFv@allowingInputMsg, __dt__32OvershellAllowingInputChangedMsgFv, &@56469);
                @GUARD@UpdateState__13OvershellSlotFv@allowingInputMsg = 1;
            }
            var_r3 = GetUserFromSlot__11BandUserMgrCFi(this->unk34, this->unk3C);
            if (var_r3 != NULL) {
                var_r3 = (BandUser *) var_r3->unk0;
            }
            sp28 = (DataArray *) var_r3;
            sp2C = 4;
            __as__8DataNodeFRC8DataNode(Node__9DataArrayFi(@LOCAL@UpdateState__13OvershellSlotFv@allowingInputMsg.unk4, 2), (DataNode *) &sp28);
            if (sp2C & 0x10) {
                temp_r0_3 = sp28->unkA - 1;
                sp28->unkA = temp_r0_3;
                if (temp_r0_3 == 0) {
                    __dt__9DataArrayFv(sp28, 1);
                }
            }
            temp_r3_2 = &this->unk30->unk54;
            temp_r3_2->unk4->unk20(temp_r3_2, @LOCAL@UpdateState__13OvershellSlotFv@allowingInputMsg.unk4, 1);
        }
        HandleMsg__18OvershellSlotStateFRC7Message(&sp20, (Message *) this->unk20);
        if (sp24 & 0x10) {
            temp_r0_4 = ((DataArray *) sp20)->unkA - 1;
            ((DataArray *) sp20)->unkA = temp_r0_4;
            if (temp_r0_4 == 0) {
                __dt__9DataArrayFv((DataArray *) sp20, 1);
            }
        }
    }
    if (temp_r31 != NULL) {
        temp_r3_3 = temp_r31->unk0;
        if (temp_r3_3->unk0->unk64(temp_r3_3) != 0) {
            temp_r30_2 = temp_r31->unk4->unk28(temp_r31);
            if (GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x2F) {
                temp_r3_4 = temp_r30_2->unk4;
                temp_r28_3 = temp_r3_4->unk4->unk2C(temp_r3_4) == 0;
                temp_r3_5 = temp_r28_3 | (TheServer->unk4->unk38(TheServer, &TheServer) == 0);
                if (((u32) (-temp_r3_5 | temp_r3_5) >> 0x1FU) != 0) {
                    CancelLinkingCode__13OvershellSlotFv(this);
                    ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 0x2D);
                }
            }
            if (InCharEditFlow__18OvershellSlotStateFv(this->unk20) != 0) {
                temp_r3_6 = temp_r30_2->unk4;
                if (temp_r3_6->unk4->unk2C(temp_r3_6) == 0) {
                    ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 6);
                }
            }
            if (GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x18) {
                temp_r3_7 = temp_r30_2->unk4;
                if (temp_r3_7->unk4->unk2C(temp_r3_7) == 0) {
                    ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 6);
                }
            }
            if (GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x33) {
                temp_r3_8 = temp_r30_2->unk4;
                if (temp_r3_8->unk4->unk2C(temp_r3_8) != 0) {
                    ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 6);
                }
            }
            if (GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x10) {
                temp_r3_9 = temp_r30_2->unk4;
                if (temp_r3_9->unk4->unk28(temp_r3_9) != 0) {
                    temp_r3_10 = temp_r30_2->unk4;
... [truncated 66 of 286 m2c lines]
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# OvershellSlot::UpdateState()
.fn UpdateState__13OvershellSlotFv, global
/* 803290C0 0031DAE0  94 21 FF B0 */	stwu r1, -0x50(r1)
/* 803290C4 0031DAE4  7C 08 02 A6 */	mflr r0
/* 803290C8 0031DAE8  90 01 00 54 */	stw r0, 0x54(r1)
/* 803290CC 0031DAEC  39 61 00 50 */	addi r11, r1, 0x50
/* 803290D0 0031DAF0  48 71 02 61 */	bl _savegpr_27
/* 803290D4 0031DAF4  7C 7D 1B 78 */	mr r29, r3
/* 803290D8 0031DAF8  80 63 00 34 */	lwz r3, 0x34(r3)
/* 803290DC 0031DAFC  80 9D 00 3C */	lwz r4, 0x3c(r29)
/* 803290E0 0031DB00  4B E3 EF 31 */	bl GetUserFromSlot__11BandUserMgrCFi
/* 803290E4 0031DB04  7C 7F 1B 78 */	mr r31, r3
/* 803290E8 0031DB08  7F A3 EB 78 */	mr r3, r29
/* 803290EC 0031DB0C  4B FF B6 85 */	bl GenerateCurrentState__13OvershellSlotFv
/* 803290F0 0031DB10  80 1D 00 20 */	lwz r0, 0x20(r29)
/* 803290F4 0031DB14  7C 7E 1B 78 */	mr r30, r3
/* 803290F8 0031DB18  2C 00 00 00 */	cmpwi r0, 0x0
/* 803290FC 0031DB1C  41 82 00 20 */	beq .L_8032911C
/* 80329100 0031DB20  7C 03 03 78 */	mr r3, r0
/* 80329104 0031DB24  48 00 8A 2D */	bl GetStateID__18OvershellSlotStateCFv
/* 80329108 0031DB28  7C 7C 1B 78 */	mr r28, r3
/* 8032910C 0031DB2C  7F C3 F3 78 */	mr r3, r30
/* 80329110 0031DB30  48 00 8A 21 */	bl GetStateID__18OvershellSlotStateCFv
/* 80329114 0031DB34  7C 03 E0 00 */	cmpw r3, r28
/* 80329118 0031DB38  41 82 02 4C */	beq .L_80329364
.L_8032911C:
/* 8032911C 0031DB3C  80 9D 00 20 */	lwz r4, 0x20(r29)
/* 80329120 0031DB40  3B 60 00 00 */	li r27, 0x0
/* 80329124 0031DB44  2C 04 00 00 */	cmpwi r4, 0x0
/* 80329128 0031DB48  41 82 01 14 */	beq .L_8032923C
/* 8032912C 0031DB4C  3C A0 80 D1 */	lis r5, exit_msg@ha
/* 80329130 0031DB50  38 61 00 30 */	addi r3, r1, 0x30
/* 80329134 0031DB54  38 A5 E6 10 */	addi r5, r5, exit_msg@l
/* 80329138 0031DB58  48 00 94 49 */	bl HandleMsg__18OvershellSlotStateFRC7Message
/* 8032913C 0031DB5C  80 01 00 34 */	lwz r0, 0x34(r1)
/* 80329140 0031DB60  54 00 06 F7 */	rlwinm. r0, r0, 0, 27, 27
/* 80329144 0031DB64  41 82 00 24 */	beq .L_80329168
/* 80329148 0031DB68  80 61 00 30 */	lwz r3, 0x30(r1)
/* 8032914C 0031DB6C  A8 83 00 0A */	lha r4, 0xa(r3)
/* 80329150 0031DB70  38 04 FF FF */	subi r0, r4, 0x1
/* 80329154 0031DB74  B0 03 00 0A */	sth r0, 0xa(r3)
/* 80329158 0031DB78  7C 00 07 35 */	extsh. r0, r0
/* 8032915C 0031DB7C  40 82 00 0C */	bne .L_80329168
/* 80329160 0031DB80  38 80 00 01 */	li r4, 0x1
/* 80329164 0031DB84  48 12 5D 8D */	bl __dt__9DataArrayFv
.L_80329168:
/* 80329168 0031DB88  2C 1F 00 00 */	cmpwi r31, 0x0
/* 8032916C 0031DB8C  41 82 00 20 */	beq .L_8032918C
/* 80329170 0031DB90  80 7F 00 00 */	lwz r3, 0x0(r31)
/* 80329174 0031DB94  81 83 00 00 */	lwz r12, 0x0(r3)
/* 80329178 0031DB98  81 8C 00 64 */	lwz r12, 0x64(r12)
/* 8032917C 0031DB9C  7D 89 03 A6 */	mtctr r12
/* 80329180 0031DBA0  4E 80 04 21 */	bctrl
/* 80329184 0031DBA4  2C 03 00 00 */	cmpwi r3, 0x0
/* 80329188 0031DBA8  41 82 00 94 */	beq .L_8032921C
.L_8032918C:
/* 8032918C 0031DBAC  80 7D 00 20 */	lwz r3, 0x20(r29)
/* 80329190 0031DBB0  48 00 8E C1 */	bl RetractedPosition__18OvershellSlotStateFv
/* 80329194 0031DBB4  2C 03 00 00 */	cmpwi r3, 0x0
/* 80329198 0031DBB8  41 82 00 3C */	beq .L_803291D4
/* 8032919C 0031DBBC  7F C3 F3 78 */	mr r3, r30
/* 803291A0 0031DBC0  48 00 8E B1 */	bl RetractedPosition__18OvershellSlotStateFv
/* 803291A4 0031DBC4  2C 03 00 00 */	cmpwi r3, 0x0
/* 803291A8 0031DBC8  40 82 00 2C */	bne .L_803291D4
/* 803291AC 0031DBCC  3C 60 80 B3 */	lis r3, "@F_00000000"@ha
/* 803291B0 0031DBD0  3C 80 80 BA */	lis r4, "@stringBase0"@ha
/* 803291B4 0031DBD4  C0 23 4B AC */	lfs f1, "@F_00000000"@l(r3)
/* 803291B8 0031DBD8  3C A0 80 D2 */	lis r5, TheSynth@ha
/* 803291BC 0031DBDC  38 84 CA 30 */	addi r4, r4, "@stringBase0"@l
/* 803291C0 0031DBE0  80 65 03 D0 */	lwz r3, TheSynth@l(r5)
/* 803291C4 0031DBE4  FC 40 08 90 */	fmr f2, f1
/* 803291C8 0031DBE8  38 84 03 59 */	addi r4, r4, 0x359
/* 803291CC 0031DBEC  FC 60 08 90 */	fmr f3, f1
/* 803291D0 0031DBF0  48 69 5F 91 */	bl Play__5SynthFPCcfff
.L_803291D4:
/* 803291D4 0031DBF4  80 7D 00 20 */	lwz r3, 0x20(r29)
/* 803291D8 0031DBF8  48 00 8E 79 */	bl RetractedPosition__18OvershellSlotStateFv
/* 803291DC 0031DBFC  2C 03 00 00 */	cmpwi r3, 0x0
/* 803291E0 0031DC00  40 82 00 3C */	bne .L_8032921C
/* 803291E4 0031DC04  7F C3 F3 78 */	mr r3, r30
/* 803291E8 0031DC08  48 00 8E 69 */	bl RetractedPosition__18OvershellSlotStateFv
/* 803291EC 0031DC0C  2C 03 00 00 */	cmpwi r3, 0x0
/* 803291F0 0031DC10  41 82 00 2C */	beq .L_8032921C
/* 803291F4 0031DC14  3C 60 80 B3 */	lis r3, "@F_00000000"@ha
/* 803291F8 0031DC18  3C 80 80 BA */	lis r4, "@stringBase0"@ha
/* 803291FC 0031DC1C  C0 23 4B AC */	lfs f1, "@F_00000000"@l(r3)
/* 80329200 0031DC20  3C A0 80 D2 */	lis r5, TheSynth@ha
/* 80329204 0031DC24  38 84 CA 30 */	addi r4, r4, "@stringBase0"@l
/* 80329208 0031DC28  80 65 03 D0 */	lwz r3, TheSynth@l(r5)
/* 8032920C 0031DC2C  FC 40 08 90 */	fmr f2, f1
/* 80329210 0031DC30  38 84 03 6A */	addi r4, r4, 0x36a
/* 80329214 0031DC34  FC 60 08 90 */	fmr f3, f1
/* 80329218 0031DC38  48 69 5F 49 */	bl Play__5SynthFPCcfff
.L_8032921C:
/* 8032921C 0031DC3C  7F C3 F3 78 */	mr r3, r30
/* 80329220 0031DC40  48 00 8B 61 */	bl AllowsInputToShell__18OvershellSlotStateFv
/* 80329224 0031DC44  7C 7C 1B 78 */	mr r28, r3
/* 80329228 0031DC48  80 7D 00 20 */	lwz r3, 0x20(r29)
/* 8032922C 0031DC4C  48 00 8B 55 */	bl AllowsInputToShell__18OvershellSlotStateFv
/* 80329230 0031DC50  7C 03 E0 40 */	cmplw r3, r28
/* 80329234 0031DC54  41 82 00 08 */	beq .L_8032923C
/* 80329238 0031DC58  3B 60 00 01 */	li r27, 0x1
.L_8032923C:
/* 8032923C 0031DC5C  2C 1B 00 00 */	cmpwi r27, 0x0
/* 80329240 0031DC60  93 DD 00 20 */	stw r30, 0x20(r29)
/* 80329244 0031DC64  41 82 00 E0 */	beq .L_80329324
/* 80329248 0031DC68  88 0D 96 ED */	lbz r0, "@GUARD@UpdateState__13OvershellSlotFv@allowingInputMsg"@sda21(r0)
/* 8032924C 0031DC6C  7C 00 07 75 */	extsb. r0, r0
/* 80329250 0031DC70  40 82 00 40 */	bne .L_80329290
/* 80329254 0031DC74  80 7D 00 34 */	lwz r3, 0x34(r29)
/* 80329258 0031DC78  80 9D 00 3C */	lwz r4, 0x3c(r29)
/* 8032925C 0031DC7C  4B E3 ED B5 */	bl GetUserFromSlot__11BandUserMgrCFi
/* 80329260 0031DC80  3F 80 80 C9 */	lis r28, "@LOCAL@UpdateState__13OvershellSlotFv@allowingInputMsg"@ha
/* 80329264 0031DC84  7C 64 1B 78 */	mr r4, r3
/* 80329268 0031DC88  38 7C 08 38 */	addi r3, r28, "@LOCAL@UpdateState__13OvershellSlotFv@allowingInputMsg"@l
/* 8032926C 0031DC8C  4B FE ED 25 */	bl __ct__32OvershellAllowingInputChangedMsgFP8BandUser
/* 80329270 0031DC90  3C 80 80 28 */	lis r4, __dt__32OvershellAllowingInputChangedMsgFv@ha
/* 80329274 0031DC94  3C A0 80 C9 */	lis r5, "@56469"@ha
/* 80329278 0031DC98  38 7C 08 38 */	addi r3, r28, "@LOCAL@UpdateState__13OvershellSlotFv@allowingInputMsg"@l
/* 8032927C 0031DC9C  38 84 2C 80 */	addi r4, r4, __dt__32OvershellAllowingInputChangedMsgFv@l
/* 80329280 0031DCA0  38 A5 06 D0 */	addi r5, r5, "@56469"@l
/* 80329284 0031DCA4  48 70 F3 E9 */	bl __register_global_object
/* 80329288 0031DCA8  38 00 00 01 */	li r0, 0x1
/* 8032928C 0031DCAC  98 0D 96 ED */	stb r0, "@GUARD@UpdateState__13OvershellSlotFv@allowingInputMsg"@sda21(r0)
.L_80329290:
/* 80329290 0031DCB0  80 7D 00 34 */	lwz r3, 0x34(r29)
/* 80329294 0031DCB4  80 9D 00 3C */	lwz r4, 0x3c(r29)
/* 80329298 0031DCB8  4B E3 ED 79 */	bl GetUserFromSlot__11BandUserMgrCFi
/* 8032929C 0031DCBC  2C 03 00 00 */	cmpwi r3, 0x0
/* 803292A0 0031DCC0  41 82 00 08 */	beq .L_803292A8
/* 803292A4 0031DCC4  80 63 00 00 */	lwz r3, 0x0(r3)
.L_803292A8:
/* 803292A8 0031DCC8  3C 80 80 C9 */	lis r4, "@LOCAL@UpdateState__13OvershellSlotFv@allowingInputMsg"@ha
/* 803292AC 0031DCCC  38 00 00 04 */	li r0, 0x4
/* 803292B0 0031DCD0  38 84 08 38 */	addi r4, r4, "@LOCAL@UpdateState__13OvershellSlotFv@allowingInputMsg"@l
/* 803292B4 0031DCD4  90 61 00 28 */	stw r3, 0x28(r1)
/* 803292B8 0031DCD8  80 64 00 04 */	lwz r3, 0x4(r4)
/* 803292BC 0031DCDC  38 80 00 02 */	li r4, 0x2
/* 803292C0 0031DCE0  90 01 00 2C */	stw r0, 0x2c(r1)
/* 803292C4 0031DCE4  48 12 46 DD */	bl Node__9DataArrayFi
/* 803292C8 0031DCE8  38 81 00 28 */	addi r4, r1, 0x28
/* 803292CC 0031DCEC  48 13 61 15 */	bl __as__8DataNodeFRC8DataNode
/* 803292D0 0031DCF0  80 01 00 2C */	lwz r0, 0x2c(r1)
/* 803292D4 0031DCF4  54 00 06 F7 */	rlwinm. r0, r0, 0, 27, 27
/* 803292D8 0031DCF8  41 82 00 24 */	beq .L_803292FC
/* 803292DC 0031DCFC  80 61 00 28 */	lwz r3, 0x28(r1)
/* 803292E0 0031DD00  A8 83 00 0A */	lha r4, 0xa(r3)
/* 803292E4 0031DD04  38 04 FF FF */	subi r0, r4, 0x1
/* 803292E8 0031DD08  B0 03 00 0A */	sth r0, 0xa(r3)
/* 803292EC 0031DD0C  7C 00 07 35 */	extsh. r0, r0
... [truncated 435 of 585 asm lines]
```
