# Pair 11 — verification evidence

**Claimed identity:** Wii `ResolveSlotStates__14OvershellPanelFv`  ==  Xenon `0x8259df90`

| field | value |
|---|---|
| pair_id | 11 |
| stratum | BSIM 20-30 |
| match_type | `BSIM` |
| BSim sim×conf | 23.996 |
| BSim similarity / confidence | 0.331 / 72.496 |
| TU (Wii) | `OvershellPanel.o` |
| Wii symbol (demangled) | `OvershellPanel::ResolveSlotStates(...)` |
| Wii addr (Bank 8) | `0x8031c470` |
| Xenon addr | `0x8259df90` |
| Xenon func name | `Function_8259DF90` (stripped binary — name is auto-generated) |
| Wii body size | 517 asm lines (lines 5369-5884 in `build/SZBE69_B8/asm/band3/meta_band/OvershellPanel.s`) |
| Xenon body size | 1448 bytes |

## How to read this

The Wii symbol is **ground truth** (from the CodeWarrior linker map). The Xenon
function is from a **stripped** XEX — its name `Function_<addr>` is meaningless.
The claim is that these two functions are the **same source function** compiled by
two different toolchains (Wii MWCC/Gekko PPC vs Xbox360 MSVC/Xenon PPC). Judge
whether the *control-flow shape, constant pool, field offsets, and especially the
resolved callees* are consistent with that claim. The 'resolved callee' column
below maps each Xenon callee to its matched Wii symbol where a match exists —
**agreeing callee names are the strongest cross-compiler signal.**

## Xenon callees (38 total, 20 resolved to a matched Wii symbol)

| xenon callee addr | xenon name | resolved Wii symbol (via matches.json) |
|---|---|---|
| `0x82642eb0` | `Function_82642EB0` | _(unmatched / Function_)_ |
| `0x82642a70` | `Function_82642A70` | _(unmatched / Function_)_ |
| `0x8259cda0` | `Function_8259CDA0` | `ResolveReadyToPlayStates__14OvershellPanelFv` |
| `0x825bfa88` | `Function_825BFA88` | _(unmatched / Function_)_ |
| `0x827362a0` | `Function_827362A0` | `SetTrackType__8BandUserF9TrackType` |
| `0x82725d28` | `Function_82725D28` | `__as__8DataNodeFRC8DataNode` |
| `0x8259a370` | `Function_8259A370` | _(unmatched / Function_)_ |
| `0x825bed30` | `FUN_825bed30` | `GetUser__13OvershellSlotCFv` |
| `0x8266d3d8` | `FUN_8266d3d8` | `PostSave__13WiiProfileMgrFv` |
| `0x82735fe0` | `Function_82735FE0` | _(unmatched / Function_)_ |
| `0x82804da8` | `Function_82804DA8` | _(unmatched / Function_)_ |
| `0x825bf690` | `Function_825BF690` | _(unmatched / Function_)_ |
| `0x8259c790` | `Function_8259C790` | `NHTTPi_free` |
| `0x8259a4c0` | `Function_8259A4C0` | _(unmatched / Function_)_ |
| `0x823f5f60` | `FUN_823f5f60` | `OSGetArenaHi` |
| `0x825bfa20` | `Function_825BFA20` | `SetOverrideFlowReturnState__13OvershellSlotF20OvershellSlotStateID` |
| `0x8279b788` | `??0Symbol@@QAA@PBD@Z` | `Enter__11RndPollableFv` |
| `0x8254ef10` | `FUN_8254ef10` | `GetGfxMode__Fv` |
| `0x82260570` | `Function_82260570` | `MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc` |
| `0x82803f10` | `FUN_82803f10` | `FUN_80a3935c` |
| `0x825bfb80` | `FUN_825bfb80` | `InOverrideFlow__13OvershellSlotCF21OvershellOverrideFlow` |
| `0x825c0580` | `FUN_825c0580` | _(unmatched / Function_)_ |
| `0x823cd938` | `FUN_823cd938` | `MainPerformer__4BandCFv` |
| `0x825beec0` | `Function_825BEEC0` | _(unmatched / Function_)_ |
| `0x827a9b78` | `FUN_827a9b78` | `GetControllerType__8BandUserCFv` |
| `0x82642960` | `Function_82642960` | _(unmatched / Function_)_ |
| `0x825bef60` | `Function_825BEF60` | `ShowSongOptions__13OvershellSlotFv` |
| `0x8259b2f0` | `Function_8259B2F0` | _(unmatched / Function_)_ |
| `0x828050f8` | `Function_828050F8` | `MsToTick__Ff` |
| `0x825bed40` | `Function_825BED40` | _(unmatched / Function_)_ |
| `0x825beda0` | `Function_825BEDA0` | `LeaveOptions__13OvershellSlotFv` |
| `0x825c5eb8` | `Function_825C5EB8` | `AttemptRemoveUser__13OvershellSlotFv` |
| `0x82726008` | `Function_82726008` | `GetObj__8DataNodeCFPC9DataArray` |
| `0x826430e0` | `Function_826430E0` | _(unmatched / Function_)_ |
| `0x8259d948` | `Function_8259D948` | _(unmatched / Function_)_ |
| `0x826431f0` | `Function_826431F0` | _(unmatched / Function_)_ |
| `0x82261ca8` | `Function_82261CA8` | _(unmatched / Function_)_ |
| `0x82642850` | `Function_82642850` | _(unmatched / Function_)_ |

## Referenced strings (Xenon side, 2)

- `'kick_user'`
- `'hide_connect_controller_mesh'`

## Xenon pseudo-C (Ghidra, fork 12.2, PowerPC:BE:64:Xenon)

```c

/* WARNING: Removing unreachable block (ram,0x8259e488) */
/* WARNING: Removing unreachable block (ram,0x8259e4f8) */

void Function_8259DF90(void)

{
  int iVar1;
  int iVar3;
  int iVar4;
  char cVar9;
  int *piVar5;
  int iVar6;
  undefined4 *puVar7;
  int iVar8;
  undefined8 uVar2;
  undefined4 uVar10;
  undefined4 uVar11;
  uint uVar12;
  int iVar13;
  int *piVar14;
  undefined4 local_a0 [2];
  undefined1 auStack_98 [8];
  undefined1 local_90 [8];
  undefined1 local_88 [8];
  undefined4 local_80;
  uint local_7c;
  
  iVar3 = FUN_82803f10();
  Function_8259D948();
  Function_8259CDA0(iVar3);
  Function_8259C790(iVar3);
  Function_8259A370(iVar3);
  Function_8259A4C0(iVar3);
  iVar1 = *(int *)(iVar3 + 0x74);
  piVar14 = (int *)(iVar3 + 0x74);
  uVar12 = 0;
  if (*(int *)(iVar3 + 0x78) - iVar1 >> 2 != 0) {
    iVar13 = 0;
    do {
      iVar1 = *(int *)(iVar13 + iVar1);
      iVar4 = FUN_825bed30(iVar1);
      if (iVar4 == 0) {
LAB_8259e3f4:
        FUN_823f5f60(iVar1);
        FUN_823cd938();
      }
      else {
        iVar4 = FUN_825bed30(iVar1);
        iVar4 = *(int *)(*(int *)(iVar4 + 4) + 4) + iVar4;
        cVar9 = (**(code **)(*(int *)(iVar4 + 4) + 0x5c))(iVar4 + 4);
        if (cVar9 == '\0') goto LAB_8259e3f4;
        piVar5 = (int *)FUN_825bed30(iVar1);
        iVar4 = (**(code **)(*piVar5 + 0x18))();
        FUN_823f5f60(iVar1);
        iVar6 = FUN_823cd938();
        if (iVar6 == 0x32) {
          piVar5 = (int *)FUN_825bed30(iVar1);
          (**(code **)(*(int *)((int)piVar5 + *(int *)(piVar5[1] + 4) + 4) + 0x5c))
                    ((int)piVar5 + *(int *)(piVar5[1] + 4) + 4);
          puVar7 = (undefined4 *)(**(code **)(*piVar5 + 0x18))(piVar5);
          iVar6 = (**(code **)*puVar7)();
          iVar8 = FUN_827a9b78(piVar5);
          if (iVar6 == iVar8) {
            Function_825BFA88(iVar1);
          }
          else if ((*(int *)(iVar3 + 0x90) == 1) || (*(int *)(iVar3 + 0x90) == 2)) {
            Function_825C5EB8(iVar1);
          }
        }
        uVar10 = *(undefined4 *)(*(int *)(*(int *)(iVar4 + 4) + 8) + iVar4 + 0x24);
        FUN_823f5f60(iVar1);
        cVar9 = Function_82642850();
        if (cVar9 == '\0') {
          cVar9 = Function_8259B2F0(iVar3,iVar4,local_a0);
          if (cVar9 == '\0') {
            iVar6 = FUN_8254ef10();
            FUN_823f5f60(iVar1);
            cVar9 = Function_826431F0();
            if (((cVar9 == '\0') || (iVar6 == 0)) || (*(int *)(iVar6 + 0x18) != iVar4)) {
              if (((*(int *)(iVar3 + 0x8c) == 1) || (*(char *)(iVar3 + 0x94) != '\0')) ||
                 (cVar9 = FUN_825bfb80(*(undefined4 *)(iVar13 + *piVar14),1), cVar9 == '\0')) {
                FUN_823f5f60(iVar1);
                cVar9 = Function_82642EB0();
                if (cVar9 == '\0') {
                  cVar9 = FUN_825bfb80(iVar1,1);
                  if (cVar9 != '\0') {
                    uVar11 = uVar10;
                    if (*(char *)(iVar1 + 0x69) != '\0') {
                      uVar11 = 5;
                    }
                    Function_825BFA20(iVar1,uVar11);
                    Function_825BEF60(iVar1);
                  }
                }
                else {
                  cVar9 = FUN_825bfb80(iVar1,1);
                  if (cVar9 == '\0') {
                    Function_825BFA88(iVar1);
                  }
                }
                if (*(char *)(iVar1 + 0x6a) == '\0') {
                  FUN_823f5f60(iVar1);
                  cVar9 = Function_826430E0();
                  if (cVar9 == '\0') {
                    cVar9 = FUN_825bfb80(iVar1,2);
                    if (cVar9 != '\0') {
                      Function_825BFA20(iVar1,uVar10);
                      Function_825BED40(iVar1,0x14);
                    }
                  }
                  else {
                    cVar9 = FUN_825bfb80(iVar1,2);
                    if (cVar9 == '\0') {
                      Function_825BFA88(iVar1);
                    }
                  }
                  FUN_823f5f60(iVar1);
                  cVar9 = Function_82642960();
                  if ((cVar9 != '\0') &&
                     (cVar9 = (**(code **)(**(int **)(iVar3 + 0xa4) + 0x20))(), cVar9 == '\0')) {
                    Function_825BEDA0(iVar1);
                  }
                  FUN_823f5f60(iVar1);
                  cVar9 = Function_82642A70();
                  if ((cVar9 != '\0') &&
                     (cVar9 = (**(code **)(**(int **)(iVar3 + 0xa4) + 0x24))(), cVar9 != '\0')) {
                    Function_825BEEC0(iVar1);
                  }
                  FUN_823f5f60(iVar1);
                  iVar4 = FUN_823cd938();
                  if (iVar4 == 0x29) {
                    puVar7 = (undefined4 *)__0Symbol__QAA_PBD_Z(auStack_98,0xffffffff820aa338);
                    uVar10 = *puVar7;
                    uVar2 = FUN_823f5f60(iVar1);
                    uVar2 = Function_827362A0(uVar2,uVar10,1);
                    uVar2 = Function_82726008(uVar2,0);
                    iVar4 = Function_82804DA8(uVar2,0,0xffffffff82c349c4,0xffffffff82c3d3d0,0);
                    if (iVar4 == 0) {
                      iVar4 = 0;
                    }
                    else {
                      iVar4 = *(int *)(*(int *)(iVar4 + 4) + 4) + iVar4 + 4;
                    }
                    cVar9 = (**(code **)(**(int **)(iVar3 + 0xa4) + 0x30))
                                      (*(int **)(iVar3 + 0xa4),iVar4);
                    if (cVar9 == '\0') {
                      Function_825BF690(iVar1);
                    }
                  }
                  FUN_823f5f60(iVar1);
                  iVar4 = FUN_823cd938();
                  if ((iVar4 == 0x1e) &&
                     (cVar9 = (**(code **)(**(int **)(iVar3 + 0xa4) + 0x3c))(), cVar9 == '\0')) {
                    Function_825BEEC0(iVar1);
                  }
                }
              }
              else {
                FUN_825c0580(*(undefined4 *)(iVar13 + *piVar14),1,1);
              }
              goto LAB_8259e40c;
            }
            uVar10 = 0x49;
          }
          else {
            FUN_8266d3d8(iVar4);
            uVar10 = local_a0[0];
          }
          Function_825BED40(iVar1,uVar10);
        }
      }
LAB_8259e40c:
      iVar1 = *piVar14;
      uVar12 = uVar12 + 1;
      iVar13 = iVar13 + 4;
    } while (uVar12 < (uint)(*(int *)(iVar3 + 0x78) - iVar1 >> 2));
  }
  if ((DAT_82dcde40 & 1) == 0) {
    DAT_82dcde40 = DAT_82dcde40 | 1;
    puVar7 = (undefined4 *)__0Symbol__QAA_PBD_Z(auStack_98,0xffffffff820aa318);
    Function_82261CA8(0xffffffff82dcde38,*puVar7,local_90);
    Function_828050F8(0xffffffff82c0e070);
  }
  Function_82725D28((ulonglong)*DAT_82dcde3c + 0x10,local_88);
  Function_82735FE0(&local_80,*(int *)(*(int *)(iVar3 + 4) + 4) + iVar3 + 4,DAT_82dcde3c);
  if ((local_7c & 0x10) != 0) {
    Function_82260570(local_80);
  }
  return;
}


```

## Wii m2c decompilation (target Bank-8 asm → C, `m2c --target ppc`)

```c
typedef struct BandUser {
    /* 0x0 */ void **unk0;                          /* inferred */
    /* 0x4 */ void *unk4;                           /* inferred */
} BandUser;                                         /* size >= 0x8 */

typedef struct DataArray {
    /* 0x0 */ char pad0[0xA];
    /* 0xA */ s16 unkA;                             /* inferred */
} DataArray;                                        /* size >= 0xC */

typedef struct LocalBandUser {
    /* 0x0 */ void *unk0;                           /* inferred */
    /* 0x4 */ void *unk4;                           /* inferred */
} LocalBandUser;                                    /* size >= 0x8 */

typedef struct OvershellPanel {
    /* 0x00 */ DataArray *unk0;                     /* inferred */
    /* 0x04 */ char pad4[0x6C];                     /* maybe part of unk0[0x1C]? */
    /* 0x70 */ s32 unk70;                           /* inferred */
    /* 0x74 */ u16 unk74;                           /* inferred */
    /* 0x76 */ char pad76[0xA];                     /* maybe part of unk74[6]? */
    /* 0x80 */ s32 unk80;                           /* inferred */
    /* 0x84 */ s32 unk84;                           /* inferred */
    /* 0x88 */ u8 unk88;                            /* inferred */
    /* 0x89 */ char pad89[0xB];                     /* maybe part of unk88[0xC]? */
    /* 0x94 */ void *unk94;                         /* inferred */
} OvershellPanel;                                   /* size >= 0x98 */

typedef struct OvershellSlot {
    /* 0x00 */ char pad0[0x5D];
    /* 0x5D */ u8 unk5D;                            /* inferred */
    /* 0x5E */ u8 unk5E;                            /* inferred */
} OvershellSlot;                                    /* size >= 0x5F */

? AttemptRemoveUser__13OvershellSlotFv(OvershellSlot *this); /* extern */
? EndOverrideFlow__13OvershellSlotF21OvershellOverrideFlowb(OvershellSlot *this, OvershellOverrideFlow arg0, s32 arg1); /* extern */
? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
void *GetClosetMgr__9ClosetMgrFv(ClosetMgr *this);  /* extern */
s32 GetControllerType__8BandUserCFv(BandUser *this); /* extern */
s32 GetIndexForPad__13WiiProfileMgrCFi(WiiProfileMgr *this, s32 arg0); /* extern */
? GetObj__8DataNodeCFPC9DataArray(DataNode *this, DataArray *arg0); /* extern */
s32 GetStateID__18OvershellSlotStateCFv(OvershellSlotState *this); /* extern */
OvershellSlotState *GetState__13OvershellSlotFv(OvershellSlot *this); /* extern */
BandUser *GetUser__13OvershellSlotCFv(OvershellSlot *this); /* extern */
? HandleType__Q23Hmx6ObjectFP9DataArray(Hmx::Object *this, DataArray *arg0); /* extern */
s32 InChooseCharFlow__18OvershellSlotStateFv(OvershellSlotState *this); /* extern */
s32 InOverrideFlow__13OvershellSlotCF21OvershellOverrideFlow(OvershellSlot *this, OvershellOverrideFlow arg0); /* extern */
s32 InRegisterOnlineFlow__18OvershellSlotStateFv(OvershellSlotState *this); /* extern */
s32 InSongSettingsFlow__18OvershellSlotStateFv(OvershellSlotState *this); /* extern */
s32 IsIndexLoaded__13WiiProfileMgrCFi(WiiProfileMgr *this, s32 arg0); /* extern */
s32 IsIndexLocked__13WiiProfileMgrCFi(WiiProfileMgr *this, s32 arg0); /* extern */
s32 IsPadAGuest__13WiiProfileMgrCFi(WiiProfileMgr *this, s32 arg0); /* extern */
? LeaveKickConfirmation__13OvershellSlotFv(OvershellSlot *this); /* extern */
? LeaveOptions__13OvershellSlotFv(OvershellSlot *this); /* extern */
? LeaveWaitWii__13OvershellSlotFv(OvershellSlot *this); /* extern */
s8 *MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(s8 *arg0, s8 *arg1, s32 arg2, s8 *arg3); /* extern */
DataNode *Node__9DataArrayFi(DataArray *this, s32 arg0); /* extern */
s32 PreventsOverride__18OvershellSlotStateFv(OvershellSlotState *this); /* extern */
DataNode *Property__Q23Hmx6ObjectCF6Symbolb(Hmx::Object *this, Symbol arg0, s32 arg1); /* extern */
s32 RequiresOnlineSession__18OvershellSlotStateFv(OvershellSlotState *this); /* extern */
s32 RequiresRemoteUsers__18OvershellSlotStateFv(OvershellSlotState *this); /* extern */
? RevertToOverrideFlowReturnState__13OvershellSlotFv(OvershellSlot *this); /* extern */
? SetHasSeenRealGuitarPrompt__13LocalBandUserFv(LocalBandUser *this); /* extern */
? SetOverrideFlowReturnState__13OvershellSlotF20OvershellSlotStateID(OvershellSlot *this, OvershellSlotStateID arg0); /* extern */
? ShowOnlineOptions__13OvershellSlotFv(OvershellSlot *this); /* extern */
? ShowSongOptions__13OvershellSlotFv(OvershellSlot *this); /* extern */
? ShowState__13OvershellSlotF20OvershellSlotStateID(OvershellSlot *this, OvershellSlotStateID arg0); /* extern */
? ShowWaitWii__13OvershellSlotFv(OvershellSlot *this); /* extern */
DataArray *_PoolAlloc__Fii8PoolType(s32 arg0, s32 arg1, PoolType arg2); /* extern */
? __as__8DataNodeFRC8DataNode(DataNode *this, DataNode *arg0); /* extern */
void *__ct__6SymbolFPCc(Symbol *this, s8 *arg0);    /* extern */
DataArray *__ct__9DataArrayFi(DataArray *this, s32 arg0); /* extern */
void *__dt__7MessageFv(Message *this, s16 destroyFlag); /* extern */
void *__dt__9DataArrayFv(DataArray *this, s16 destroyFlag); /* extern */
? *__dynamic_cast(?, struct RTTI *, struct RTTI *, ?); /* extern */
? __register_global_object(? *, void *(*)(Message *, s16), ? *); /* extern */
? ResolveAutoSignInStates__14OvershellPanelFv(OvershellPanel *this); /* static */
? ResolvePartWaitStates__14OvershellPanelFv(OvershellPanel *this); /* static */
? ResolveReadyToPlayStates__14OvershellPanelFv(OvershellPanel *this); /* static */
? ResolveSignInWaitStates__14OvershellPanelFv(OvershellPanel *this); /* static */
ClosetMgr *ShouldSeeRealGuitarPrompt__14OvershellPanelFP13LocalBandUserR20OvershellSlotStateID(OvershellPanel *this, LocalBandUser *arg0, OvershellSlotStateID *arg1); /* static */
extern u8 @GUARD@ResolveSlotStates__14OvershellPanelFv@msgHideConnectControllerMesh;
extern Debug TheDebug;
extern WiiProfileMgr TheWiiProfileMgr;
extern struct RTTI __RTTI__8BandUser;
extern struct RTTI __RTTI__Q23Hmx6Object;
extern DataArray *hide_connect_controller_mesh;
extern s8 *kAssertStr;
static ? @stringBase0;                              /* unable to generate initializer: unknown type */
static ? @57005;
static ? @LOCAL@ResolveSlotStates__14OvershellPanelFv@msgHideConnectControllerMesh;

/* OvershellPanel::ResolveSlotStates (void) */
void ResolveSlotStates__14OvershellPanelFv(OvershellPanel *this, ? arg_sp0) {
    s32 sp2C;
    DataArray *sp28;
    s32 sp24;
    DataArray *sp20;
    Hmx::Object sp18;
    s32 sp14;
    DataArray *sp10;
    OvershellSlotStateID spC;
    Symbol sp8;
    ? *temp_r3;
    ? *temp_r3_11;
    ? *var_r18;
    BandUser *temp_r3_3;
    ClosetMgr *temp_r3_5;
    DataArray *temp_r22;
    DataArray *var_r3;
    DataArray *var_r4_2;
    LocalBandUser *temp_r24;
    OvershellSlot *temp_r27;
    s16 temp_r0;
    s16 temp_r0_2;
    s16 temp_r0_3;
    s16 temp_r0_4;
    s32 temp_r18;
    s32 temp_r28;
    s32 temp_r3_14;
    s32 temp_r3_8;
    s32 var_r0;
    s32 var_r22;
    s32 var_r26;
    s32 var_r4;
    u32 var_r25;
    void **temp_r3_2;
    void **temp_r3_4;
    void *temp_r18_2;
    void *temp_r24_2;
    void *temp_r3_10;
    void *temp_r3_12;
    void *temp_r3_13;
    void *temp_r3_6;
    void *temp_r3_7;
    void *temp_r3_9;

    ResolvePartWaitStates__14OvershellPanelFv(this);
    ResolveReadyToPlayStates__14OvershellPanelFv(this);
    ResolveSignInWaitStates__14OvershellPanelFv(this);
    ResolveAutoSignInStates__14OvershellPanelFv(this);
    temp_r3 = "overshell\0OvershellPanel.cpp\0TheSessionMgr\0TheBandUserMgr\0!InOverrideFlow(kOverrideFlow_SongSettings)\0mActiveStatus != kOvershellInactive\0user != NULL\0mActiveStatus == kOvershellInactive\0!user->IsParticipating()\0InOverrideFlow(kOverrideFlow_None)\0type != kOverrideFlow_None\0InOverrideFlow(type)\0!playableTracks.empty()\0!resolvingUsers.empty()\0pSlot\0autohide_msg\0move_slots\0update\0mSlotPriorities.size() == mSlots.size()\0prevent_in_game_drop_in\0user\0IsAutoVocalsAllowed()\0!TheModifierMgr->IsModifierActive(mod_auto_vocals)\0pUser->IsLocal()\0kick_user\0pUser\0first_time_real_guitar_prompt_reqs\0second_time_real_guitar_prompt_reqs\0player_panels\0type\0slots\0valid_controllers\0validControllers\0normal\0auto_vocals\0joining_priority\0slot%i\0p\0InOverrideFlow(kOverrideFlow_RegisterOnline)\0u\0IsLoaded()\0slot\0pSlotCur\0show_net_error\0!mSlots.empty()\0no_slot_button_down_msg\0remove_user_on_disconnect_in_song\0init\0required\0%s(%d): %s unhandled msg: %s\0vector";
    var_r26 = 0;
    var_r25 = 0U;
    var_r22 = 0;
loop_65:
    if (var_r25 < (u16) this->unk74) {
        temp_r27 = *(this->unk70 + var_r22);
        if ((GetUser__13OvershellSlotCFv(temp_r27) != NULL) && (temp_r3_2 = GetUser__13OvershellSlotCFv(temp_r27)->unk0, (((*temp_r3_2)->unk64(temp_r3_2) == 0) == 0))) {
            temp_r24 = GetUser__13OvershellSlotCFv(temp_r27)->unk4->unk28();
            if (GetStateID__18OvershellSlotStateCFv(GetState__13OvershellSlotFv(temp_r27)) == 0x32) {
                temp_r3_3 = GetUser__13OvershellSlotCFv(temp_r27);
                temp_r3_4 = temp_r3_3->unk0;
                if ((*temp_r3_4)->unk64(temp_r3_4) == 0) {
                    Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r3 + 0xA, 0x58B, temp_r3 + 0x20A));
                }
                temp_r18 = GetControllerType__8BandUserCFv(temp_r3_3);
                if (temp_r3_3->unk4->unk28(temp_r3_3)->unk8->unk28() == temp_r18) {
                    RevertToOverrideFlowReturnState__13OvershellSlotFv(temp_r27);
                } else if ((u32) (this->unk84 - 1) <= 1U) {
                    AttemptRemoveUser__13OvershellSlotFv(temp_r27);
                }
            }
            temp_r28 = temp_r24->unk0->unk20;
            if (PreventsOverride__18OvershellSlotStateFv(GetState__13OvershellSlotFv(temp_r27)) == 0) {
                temp_r3_5 = ShouldSeeRealGuitarPrompt__14OvershellPanelFP13LocalBandUserR20OvershellSlotStateID(this, temp_r24, &spC);
                if (temp_r3_5 != NULL) {
                    SetHasSeenRealGuitarPrompt__13LocalBandUserFv(temp_r24);
                    ShowState__13OvershellSlotF20OvershellSlotStateID(temp_r27, (OvershellSlotStateID) (s32) spC);
                } else {
                    temp_r18_2 = GetClosetMgr__9ClosetMgrFv(temp_r3_5);
                    if ((InChooseCharFlow__18OvershellSlotStateFv(GetState__13OvershellSlotFv(temp_r27)) != 0) && (temp_r18_2 != NULL) && ((u32) temp_r18_2->unk1C == temp_r24)) {
                        ShowState__13OvershellSlotF20OvershellSlotStateID(temp_r27, (OvershellSlotStateID) 0x49);
                    } else if (((s32) this->unk80 != 1) && ((s32) this->unk88 == 0) && (InOverrideFlow__13OvershellSlotCF21OvershellOverrideFlow(*(this->unk70 + var_r22), (OvershellOverrideFlow) 1) != 0)) {
                        EndOverrideFlow__13OvershellSlotF21OvershellOverrideFlowb(*(this->unk70 + var_r22), (OvershellOverrideFlow) 1, 1);
                    } else if (GetStateID__18OvershellSlotStateCFv(GetState__13OvershellSlotFv(temp_r27)) < 0xC8) {
                        temp_r3_6 = temp_r24->unk4;
                        if (IsPadAGuest__13WiiProfileMgrCFi(&TheWiiProfileMgr, temp_r3_6->unk4->unk10(temp_r3_6)) == 0) {
                            temp_r3_7 = temp_r24->unk4;
                            temp_r3_8 = GetIndexForPad__13WiiProfileMgrCFi(&TheWiiProfileMgr, temp_r3_7->unk4->unk10(temp_r3_7));
                            if (temp_r3_8 >= 0) {
                                if ((IsIndexLoaded__13WiiProfileMgrCFi(&TheWiiProfileMgr, temp_r3_8) == 0) || (IsIndexLocked__13WiiProfileMgrCFi(&TheWiiProfileMgr, temp_r3_8) != 0)) {
                                    if (GetStateID__18OvershellSlotStateCFv(GetState__13OvershellSlotFv(temp_r27)) != 0x81) {
                                        ShowWaitWii__13OvershellSlotFv(temp_r27);
                                    }
                                } else {
                                    goto block_31;
                                }
                            }
                        } else {
block_31:
                            if (GetStateID__18OvershellSlotStateCFv(GetState__13OvershellSlotFv(temp_r27)) == 0x81) {
                                LeaveWaitWii__13OvershellSlotFv(temp_r27);
                            }
                            if (InSongSettingsFlow__18OvershellSlotStateFv(GetState__13OvershellSlotFv(temp_r27)) != 0) {
                                if (InOverrideFlow__13OvershellSlotCF21OvershellOverrideFlow(temp_r27, (OvershellOverrideFlow) 1) == 0) {
                                    RevertToOverrideFlowReturnState__13OvershellSlotFv(temp_r27);
                                }
                            } else if (InOverrideFlow__13OvershellSlotCF21OvershellOverrideFlow(temp_r27, (OvershellOverrideFlow) 1) != 0) {
                                var_r4 = temp_r28;
                                if ((s32) temp_r27->unk5D != 0) {
                                    var_r4 = 5;
                                }
                                SetOverrideFlowReturnState__13OvershellSlotF20OvershellSlotStateID(temp_r27, (OvershellSlotStateID) var_r4);
                                ShowSongOptions__13OvershellSlotFv(temp_r27);
                            }
                            if ((s32) temp_r27->unk5E == 0) {
                                if (InRegisterOnlineFlow__18OvershellSlotStateFv(GetState__13OvershellSlotFv(temp_r27)) != 0) {
                                    if (InOverrideFlow__13OvershellSlotCF21OvershellOverrideFlow(temp_r27, (OvershellOverrideFlow) 2) == 0) {
                                        RevertToOverrideFlowReturnState__13OvershellSlotFv(temp_r27);
                                    }
                                } else if (InOverrideFlow__13OvershellSlotCF21OvershellOverrideFlow(temp_r27, (OvershellOverrideFlow) 2) != 0) {
                                    if (InRegisterOnlineFlow__18OvershellSlotStateFv(GetState__13OvershellSlotFv(temp_r27)) == 0) {
                                        SetOverrideFlowReturnState__13OvershellSlotF20OvershellSlotStateID(temp_r27, (OvershellSlotStateID) temp_r28);
                                    }
                                    ShowState__13OvershellSlotF20OvershellSlotStateID(temp_r27, (OvershellSlotStateID) 0x8B);
                                }
                                if (RequiresOnlineSession__18OvershellSlotStateFv(GetState__13OvershellSlotFv(temp_r27)) != 0) {
                                    temp_r3_9 = this->unk94;
                                    if (temp_r3_9->unk4->unk4C(temp_r3_9) == 0) {
... [truncated 104 of 324 m2c lines]
```

## Wii target asm (Bank 8, ground-truth body)

```asm
# OvershellPanel::ResolveSlotStates()
.fn ResolveSlotStates__14OvershellPanelFv, global
/* 8031C470 00310E90  94 21 FF 90 */	stwu r1, -0x70(r1)
/* 8031C474 00310E94  7C 08 02 A6 */	mflr r0
/* 8031C478 00310E98  90 01 00 74 */	stw r0, 0x74(r1)
/* 8031C47C 00310E9C  39 61 00 70 */	addi r11, r1, 0x70
/* 8031C480 00310EA0  48 71 CE 8D */	bl _savegpr_18
/* 8031C484 00310EA4  7C 77 1B 78 */	mr r23, r3
/* 8031C488 00310EA8  4B FF D6 49 */	bl ResolvePartWaitStates__14OvershellPanelFv
/* 8031C48C 00310EAC  7E E3 BB 78 */	mr r3, r23
/* 8031C490 00310EB0  4B FF D3 71 */	bl ResolveReadyToPlayStates__14OvershellPanelFv
/* 8031C494 00310EB4  7E E3 BB 78 */	mr r3, r23
/* 8031C498 00310EB8  48 00 07 69 */	bl ResolveSignInWaitStates__14OvershellPanelFv
/* 8031C49C 00310EBC  7E E3 BB 78 */	mr r3, r23
/* 8031C4A0 00310EC0  48 00 09 11 */	bl ResolveAutoSignInStates__14OvershellPanelFv
/* 8031C4A4 00310EC4  3C 60 80 BA */	lis r3, "@stringBase0"@ha
/* 8031C4A8 00310EC8  3B 40 00 00 */	li r26, 0x0
/* 8031C4AC 00310ECC  3B 20 00 00 */	li r25, 0x0
/* 8031C4B0 00310ED0  3A C0 00 00 */	li r22, 0x0
/* 8031C4B4 00310ED4  3B C3 C2 C0 */	addi r30, r3, "@stringBase0"@l
/* 8031C4B8 00310ED8  3F A0 80 BB */	lis r29, kAssertStr@ha
/* 8031C4BC 00310EDC  3F E0 80 C9 */	lis r31, TheDebug@ha
/* 8031C4C0 00310EE0  3E 60 80 D1 */	lis r19, TheWiiProfileMgr@ha
/* 8031C4C4 00310EE4  3E 80 80 B8 */	lis r20, __RTTI__8BandUser@ha
/* 8031C4C8 00310EE8  3E A0 80 B5 */	lis r21, __RTTI__Q23Hmx6Object@ha
/* 8031C4CC 00310EEC  48 00 05 24 */	b .L_8031C9F0
.L_8031C4D0:
/* 8031C4D0 00310EF0  80 77 00 70 */	lwz r3, 0x70(r23)
/* 8031C4D4 00310EF4  7F 63 B0 2E */	lwzx r27, r3, r22
/* 8031C4D8 00310EF8  7F 63 DB 78 */	mr r3, r27
/* 8031C4DC 00310EFC  48 00 82 15 */	bl GetUser__13OvershellSlotCFv
/* 8031C4E0 00310F00  2C 03 00 00 */	cmpwi r3, 0x0
/* 8031C4E4 00310F04  41 82 00 48 */	beq .L_8031C52C
/* 8031C4E8 00310F08  7F 63 DB 78 */	mr r3, r27
/* 8031C4EC 00310F0C  48 00 82 05 */	bl GetUser__13OvershellSlotCFv
/* 8031C4F0 00310F10  80 63 00 00 */	lwz r3, 0x0(r3)
/* 8031C4F4 00310F14  81 83 00 00 */	lwz r12, 0x0(r3)
/* 8031C4F8 00310F18  81 8C 00 64 */	lwz r12, 0x64(r12)
/* 8031C4FC 00310F1C  7D 89 03 A6 */	mtctr r12
/* 8031C500 00310F20  4E 80 04 21 */	bctrl
/* 8031C504 00310F24  2C 03 00 00 */	cmpwi r3, 0x0
/* 8031C508 00310F28  41 82 00 24 */	beq .L_8031C52C
/* 8031C50C 00310F2C  7F 63 DB 78 */	mr r3, r27
/* 8031C510 00310F30  48 00 81 E1 */	bl GetUser__13OvershellSlotCFv
/* 8031C514 00310F34  81 83 00 04 */	lwz r12, 0x4(r3)
/* 8031C518 00310F38  81 8C 00 28 */	lwz r12, 0x28(r12)
/* 8031C51C 00310F3C  7D 89 03 A6 */	mtctr r12
/* 8031C520 00310F40  4E 80 04 21 */	bctrl
/* 8031C524 00310F44  7C 78 1B 78 */	mr r24, r3
/* 8031C528 00310F48  48 00 00 20 */	b .L_8031C548
.L_8031C52C:
/* 8031C52C 00310F4C  7F 63 DB 78 */	mr r3, r27
/* 8031C530 00310F50  48 00 81 D1 */	bl GetState__13OvershellSlotFv
/* 8031C534 00310F54  48 01 55 FD */	bl GetStateID__18OvershellSlotStateCFv
/* 8031C538 00310F58  2C 03 00 00 */	cmpwi r3, 0x0
/* 8031C53C 00310F5C  40 82 04 AC */	bne .L_8031C9E8
/* 8031C540 00310F60  3B 40 00 01 */	li r26, 0x1
/* 8031C544 00310F64  48 00 04 A4 */	b .L_8031C9E8
.L_8031C548:
/* 8031C548 00310F68  7F 63 DB 78 */	mr r3, r27
/* 8031C54C 00310F6C  48 00 81 B5 */	bl GetState__13OvershellSlotFv
/* 8031C550 00310F70  48 01 55 E1 */	bl GetStateID__18OvershellSlotStateCFv
/* 8031C554 00310F74  2C 03 00 32 */	cmpwi r3, 0x32
/* 8031C558 00310F78  40 82 00 A8 */	bne .L_8031C600
/* 8031C55C 00310F7C  7F 63 DB 78 */	mr r3, r27
/* 8031C560 00310F80  48 00 81 91 */	bl GetUser__13OvershellSlotCFv
/* 8031C564 00310F84  7C 7C 1B 78 */	mr r28, r3
/* 8031C568 00310F88  80 63 00 00 */	lwz r3, 0x0(r3)
/* 8031C56C 00310F8C  81 83 00 00 */	lwz r12, 0x0(r3)
/* 8031C570 00310F90  81 8C 00 64 */	lwz r12, 0x64(r12)
/* 8031C574 00310F94  7D 89 03 A6 */	mtctr r12
/* 8031C578 00310F98  4E 80 04 21 */	bctrl
/* 8031C57C 00310F9C  2C 03 00 00 */	cmpwi r3, 0x0
/* 8031C580 00310FA0  40 82 00 24 */	bne .L_8031C5A4
/* 8031C584 00310FA4  80 7D 28 58 */	lwz r3, kAssertStr@l(r29)
/* 8031C588 00310FA8  38 9E 00 0A */	addi r4, r30, 0xa
/* 8031C58C 00310FAC  38 DE 02 0A */	addi r6, r30, 0x20a
/* 8031C590 00310FB0  38 A0 05 8B */	li r5, 0x58b
/* 8031C594 00310FB4  4B CF 4A AD */	bl "MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc"
/* 8031C598 00310FB8  7C 64 1B 78 */	mr r4, r3
/* 8031C59C 00310FBC  38 7F 73 D8 */	addi r3, r31, TheDebug@l
/* 8031C5A0 00310FC0  48 10 24 21 */	bl Fail__5DebugFPCc
.L_8031C5A4:
/* 8031C5A4 00310FC4  7F 83 E3 78 */	mr r3, r28
/* 8031C5A8 00310FC8  4B E4 61 B9 */	bl GetControllerType__8BandUserCFv
/* 8031C5AC 00310FCC  81 9C 00 04 */	lwz r12, 0x4(r28)
/* 8031C5B0 00310FD0  7C 72 1B 78 */	mr r18, r3
/* 8031C5B4 00310FD4  7F 83 E3 78 */	mr r3, r28
/* 8031C5B8 00310FD8  81 8C 00 28 */	lwz r12, 0x28(r12)
/* 8031C5BC 00310FDC  7D 89 03 A6 */	mtctr r12
/* 8031C5C0 00310FE0  4E 80 04 21 */	bctrl
/* 8031C5C4 00310FE4  81 83 00 08 */	lwz r12, 0x8(r3)
/* 8031C5C8 00310FE8  81 8C 00 28 */	lwz r12, 0x28(r12)
/* 8031C5CC 00310FEC  7D 89 03 A6 */	mtctr r12
/* 8031C5D0 00310FF0  4E 80 04 21 */	bctrl
/* 8031C5D4 00310FF4  7C 03 90 00 */	cmpw r3, r18
/* 8031C5D8 00310FF8  40 82 00 10 */	bne .L_8031C5E8
/* 8031C5DC 00310FFC  7F 63 DB 78 */	mr r3, r27
/* 8031C5E0 00311000  48 00 C7 11 */	bl RevertToOverrideFlowReturnState__13OvershellSlotFv
/* 8031C5E4 00311004  48 00 00 1C */	b .L_8031C600
.L_8031C5E8:
/* 8031C5E8 00311008  80 77 00 84 */	lwz r3, 0x84(r23)
/* 8031C5EC 0031100C  38 03 FF FF */	subi r0, r3, 0x1
/* 8031C5F0 00311010  28 00 00 01 */	cmplwi r0, 0x1
/* 8031C5F4 00311014  41 81 00 0C */	bgt .L_8031C600
/* 8031C5F8 00311018  7F 63 DB 78 */	mr r3, r27
/* 8031C5FC 0031101C  48 00 A2 35 */	bl AttemptRemoveUser__13OvershellSlotFv
.L_8031C600:
/* 8031C600 00311020  80 98 00 00 */	lwz r4, 0x0(r24)
/* 8031C604 00311024  7F 63 DB 78 */	mr r3, r27
/* 8031C608 00311028  83 84 00 20 */	lwz r28, 0x20(r4)
/* 8031C60C 0031102C  48 00 80 F5 */	bl GetState__13OvershellSlotFv
/* 8031C610 00311030  48 01 58 01 */	bl PreventsOverride__18OvershellSlotStateFv
/* 8031C614 00311034  2C 03 00 00 */	cmpwi r3, 0x0
/* 8031C618 00311038  40 82 03 D0 */	bne .L_8031C9E8
/* 8031C61C 0031103C  7E E3 BB 78 */	mr r3, r23
/* 8031C620 00311040  7F 04 C3 78 */	mr r4, r24
/* 8031C624 00311044  38 A1 00 0C */	addi r5, r1, 0xc
/* 8031C628 00311048  48 00 0A E9 */	bl ShouldSeeRealGuitarPrompt__14OvershellPanelFP13LocalBandUserR20OvershellSlotStateID
/* 8031C62C 0031104C  2C 03 00 00 */	cmpwi r3, 0x0
/* 8031C630 00311050  41 82 00 1C */	beq .L_8031C64C
/* 8031C634 00311054  7F 03 C3 78 */	mr r3, r24
/* 8031C638 00311058  4B E4 80 D9 */	bl SetHasSeenRealGuitarPrompt__13LocalBandUserFv
/* 8031C63C 0031105C  80 81 00 0C */	lwz r4, 0xc(r1)
/* 8031C640 00311060  7F 63 DB 78 */	mr r3, r27
/* 8031C644 00311064  48 00 84 ED */	bl ShowState__13OvershellSlotF20OvershellSlotStateID
/* 8031C648 00311068  48 00 03 A0 */	b .L_8031C9E8
.L_8031C64C:
/* 8031C64C 0031106C  4B F8 EA 25 */	bl GetClosetMgr__9ClosetMgrFv
/* 8031C650 00311070  7C 72 1B 78 */	mr r18, r3
/* 8031C654 00311074  7F 63 DB 78 */	mr r3, r27
/* 8031C658 00311078  48 00 80 A9 */	bl GetState__13OvershellSlotFv
/* 8031C65C 0031107C  48 01 5C E5 */	bl InChooseCharFlow__18OvershellSlotStateFv
/* 8031C660 00311080  2C 03 00 00 */	cmpwi r3, 0x0
/* 8031C664 00311084  41 82 00 28 */	beq .L_8031C68C
/* 8031C668 00311088  2C 12 00 00 */	cmpwi r18, 0x0
/* 8031C66C 0031108C  41 82 00 20 */	beq .L_8031C68C
/* 8031C670 00311090  80 12 00 1C */	lwz r0, 0x1c(r18)
/* 8031C674 00311094  7C 00 C0 40 */	cmplw r0, r24
/* 8031C678 00311098  40 82 00 14 */	bne .L_8031C68C
/* 8031C67C 0031109C  7F 63 DB 78 */	mr r3, r27
/* 8031C680 003110A0  38 80 00 49 */	li r4, 0x49
/* 8031C684 003110A4  48 00 84 AD */	bl ShowState__13OvershellSlotF20OvershellSlotStateID
/* 8031C688 003110A8  48 00 03 60 */	b .L_8031C9E8
.L_8031C68C:
/* 8031C68C 003110AC  80 17 00 80 */	lwz r0, 0x80(r23)
/* 8031C690 003110B0  2C 00 00 01 */	cmpwi r0, 0x1
/* 8031C694 003110B4  41 82 00 40 */	beq .L_8031C6D4
/* 8031C698 003110B8  88 17 00 88 */	lbz r0, 0x88(r23)
/* 8031C69C 003110BC  2C 00 00 00 */	cmpwi r0, 0x0
... [truncated 367 of 517 asm lines]
```
