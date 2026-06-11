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
                                        LeaveOptions__13OvershellSlotFv(temp_r27);
                                    }
                                }
                                if (RequiresRemoteUsers__18OvershellSlotStateFv(GetState__13OvershellSlotFv(temp_r27)) != 0) {
                                    temp_r3_10 = this->unk94;
                                    if (temp_r3_10->unk4->unk50(temp_r3_10) != 0) {
                                        ShowOnlineOptions__13OvershellSlotFv(temp_r27);
                                    }
                                }
                                if (GetStateID__18OvershellSlotStateCFv(GetState__13OvershellSlotFv(temp_r27)) == 0x29) {
                                    temp_r24_2 = __ct__6SymbolFPCc(&sp8, temp_r3 + 0x21B);
                                    GetObj__8DataNodeCFPC9DataArray(Property__Q23Hmx6ObjectCF6Symbolb(GetState__13OvershellSlotFv(temp_r27), (Symbol) temp_r24_2, 1), NULL);
                                    temp_r3_11 = __dynamic_cast(0, &__RTTI__8BandUser, &__RTTI__Q23Hmx6Object, 0);
                                    var_r18 = temp_r3_11;
                                    if (temp_r3_11 == NULL) {
                                        Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r3 + 0xA, 0x632, temp_r3 + 0x225));
                                    }
                                    if (var_r18 != NULL) {
                                        var_r18 = *var_r18;
                                    }
                                    temp_r3_12 = this->unk94;
                                    if (temp_r3_12->unk4->unk5C(temp_r3_12, var_r18) == 0) {
                                        LeaveKickConfirmation__13OvershellSlotFv(temp_r27);
                                    }
                                }
                                if (GetStateID__18OvershellSlotStateCFv(GetState__13OvershellSlotFv(temp_r27)) == 0x1E) {
                                    temp_r3_13 = this->unk94;
                                    if (temp_r3_13->unk4->unk68(temp_r3_13) == 0) {
                                        ShowOnlineOptions__13OvershellSlotFv(temp_r27);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else if (GetStateID__18OvershellSlotStateCFv(GetState__13OvershellSlotFv(temp_r27)) == 0) {
            var_r26 = 1;
        }
        var_r25 += 1;
        var_r22 += 4;
        goto loop_65;
    }
    if ((s8) @GUARD@ResolveSlotStates__14OvershellPanelFv@msgHideConnectControllerMesh == 0) {
        sp28 = (DataArray *)1;
        temp_r22 = hide_connect_controller_mesh;
        sp2C = 6;
        @LOCAL@ResolveSlotStates__14OvershellPanelFv@msgHideConnectControllerMesh.unk0 = &__vt__7Message;
        var_r3 = _PoolAlloc__Fii8PoolType(0x10, 0x10, (PoolType) 1);
        if (var_r3 != NULL) {
            var_r3 = __ct__9DataArrayFi(var_r3, 3);
        }
        sp14 = 5;
        @LOCAL@ResolveSlotStates__14OvershellPanelFv@msgHideConnectControllerMesh.unk4 = var_r3;
        sp10 = temp_r22;
        __as__8DataNodeFRC8DataNode(Node__9DataArrayFi(var_r3, 1), (DataNode *) &sp10);
        if (sp14 & 0x10) {
            temp_r0 = sp10->unkA - 1;
            sp10->unkA = temp_r0;
            if (temp_r0 == 0) {
                __dt__9DataArrayFv(sp10, 1);
            }
        }
        __as__8DataNodeFRC8DataNode(Node__9DataArrayFi(@LOCAL@ResolveSlotStates__14OvershellPanelFv@msgHideConnectControllerMesh.unk4, 2), (DataNode *) &sp28);
        if (sp2C & 0x10) {
            temp_r0_2 = sp28->unkA - 1;
            sp28->unkA = temp_r0_2;
            if (temp_r0_2 == 0) {
                __dt__9DataArrayFv(sp28, 1);
            }
        }
        __register_global_object(&@LOCAL@ResolveSlotStates__14OvershellPanelFv@msgHideConnectControllerMesh, __dt__7MessageFv, &@57005);
        @GUARD@ResolveSlotStates__14OvershellPanelFv@msgHideConnectControllerMesh = 1;
    }
    var_r4_2 = (DataArray *)1;
    if (var_r26 != 0) {
        temp_r3_14 = this->unk84;
        var_r0 = 1;
        if ((temp_r3_14 != 2) && (temp_r3_14 != 3)) {
            var_r0 = 0;
        }
        if (var_r0 == 0) {
            var_r4_2 = NULL;
        }
    }
    sp20 = var_r4_2;
    sp24 = 6;
    __as__8DataNodeFRC8DataNode(Node__9DataArrayFi(@LOCAL@ResolveSlotStates__14OvershellPanelFv@msgHideConnectControllerMesh.unk4, 2), (DataNode *) &sp20);
    if (sp24 & 0x10) {
        temp_r0_3 = sp20->unkA - 1;
        sp20->unkA = temp_r0_3;
        if (temp_r0_3 == 0) {
            __dt__9DataArrayFv(sp20, 1);
        }
    }
    HandleType__Q23Hmx6ObjectFP9DataArray(&sp18, this->unk0);
    if (sp1C & 0x10) {
        temp_r0_4 = ((DataArray *) sp18)->unkA - 1;
        ((DataArray *) sp18)->unkA = temp_r0_4;
        if (temp_r0_4 == 0) {
            __dt__9DataArrayFv((DataArray *) sp18, 1);
        }
    }
}