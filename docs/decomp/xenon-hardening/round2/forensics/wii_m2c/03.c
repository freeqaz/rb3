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
                    if (temp_r3_10->unk4->unk1C(temp_r3_10) == 0) {
                        ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 0x11);
                    }
                }
            }
            if ((GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x18) && ((temp_r3_11 = this->unk38, ((temp_r3_11->unk4->unk50(temp_r3_11) == 0) != 0)) || (HasTransitionEvent__10UIEventMgrCF6Symbol(TheUIEventMgr, (Symbol) __ct__6SymbolFPCc(&sp1C, "setup_providers\0user_name.lbl\0OvershellSlot.cpp\0mUserNameLabel\0mState\0Unknown user entry state %i\n\0pUser\0pLocUser\0pUser->IsLocal()\0reconnect_controller.lbl\0overshell_reconnect_controller\0wii_error_remote_extension_x\0skip_choose_part\0forced_part\0trackType != kTrackVocals\0trackType != kTrackDrum\0track == kTrackVocals || !harmony\0track == kTrackDrum || !proDrums\0cymBit != 0\0skip_choose_diff_prompt\0difficulty\0pUser->GetTrackType() != kTrackNone\0required_song_options_chosen\0kick_user\0pUserToKick != NULL\0mSwappableProfilesProvider\0pLocalUser\0swap_user\0mState->GetStateID() == kState_LinkingCode\0\0code\0pUser->GetControllerType() == kControllerVocals\0pUser->GetControllerType() != kControllerVocals\0in_track_mode\0slot_view\0GetUser()->IsLocal()\0pause_menu_quit_token\0mSlotOverrideFlow == kOverrideFlow_None\0type != kOverrideFlow_None\0mSlotOverrideFlow == type\0overshell_up.cue\0overshell_down.cue\0go_to_wiiprofilecreator\0update_controller_type\0update_user_name\0update_local_status\0update_restart_allowed\0update_sign_in_continue\0update_pad_num\0update_remote_feedback\0update_remote_status\0update_show_vocal_bg\0update_song_difficulty_ranking\0update_lefty_and_static_toggle\0set_difficulty_restriction\0update_mics\0update_online_enabled\0!InGame()\0mCharForEdit != NULL\0illegal attempt made to delete guest character\n\0pProfile\0illegal attempt made to rename guest character to %s\n\0pUser && pUser->IsLocal()\0button_error.cue\0overshell_back.cue\0on_start\0Local user %s cannot join\0play_instr_sfx_local\0slider.cue\0overshell_select.cue\0mCharProvider\0update_char_provider\0update_users_provider\0update_profiles_provider\0update_part_select_provider\0update_character_portrait\0update_char_more_options\0slot state %i is not an Enter Flow Prompt\n\0%s(%d): %s unhandled msg: %s" + 0x37D)) != 0))) {
                ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 0x30);
            }
            if ((GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x17) && ((temp_r3_12 = this->unk38, ((temp_r3_12->unk4->unk50(temp_r3_12) == 0) != 0)) || (HasTransitionEvent__10UIEventMgrCF6Symbol(TheUIEventMgr, (Symbol) __ct__6SymbolFPCc(&sp18, "setup_providers\0user_name.lbl\0OvershellSlot.cpp\0mUserNameLabel\0mState\0Unknown user entry state %i\n\0pUser\0pLocUser\0pUser->IsLocal()\0reconnect_controller.lbl\0overshell_reconnect_controller\0wii_error_remote_extension_x\0skip_choose_part\0forced_part\0trackType != kTrackVocals\0trackType != kTrackDrum\0track == kTrackVocals || !harmony\0track == kTrackDrum || !proDrums\0cymBit != 0\0skip_choose_diff_prompt\0difficulty\0pUser->GetTrackType() != kTrackNone\0required_song_options_chosen\0kick_user\0pUserToKick != NULL\0mSwappableProfilesProvider\0pLocalUser\0swap_user\0mState->GetStateID() == kState_LinkingCode\0\0code\0pUser->GetControllerType() == kControllerVocals\0pUser->GetControllerType() != kControllerVocals\0in_track_mode\0slot_view\0GetUser()->IsLocal()\0pause_menu_quit_token\0mSlotOverrideFlow == kOverrideFlow_None\0type != kOverrideFlow_None\0mSlotOverrideFlow == type\0overshell_up.cue\0overshell_down.cue\0go_to_wiiprofilecreator\0update_controller_type\0update_user_name\0update_local_status\0update_restart_allowed\0update_sign_in_continue\0update_pad_num\0update_remote_feedback\0update_remote_status\0update_show_vocal_bg\0update_song_difficulty_ranking\0update_lefty_and_static_toggle\0set_difficulty_restriction\0update_mics\0update_online_enabled\0!InGame()\0mCharForEdit != NULL\0illegal attempt made to delete guest character\n\0pProfile\0illegal attempt made to rename guest character to %s\n\0pUser && pUser->IsLocal()\0button_error.cue\0overshell_back.cue\0on_start\0Local user %s cannot join\0play_instr_sfx_local\0slider.cue\0overshell_select.cue\0mCharProvider\0update_char_provider\0update_users_provider\0update_profiles_provider\0update_part_select_provider\0update_character_portrait\0update_char_more_options\0slot state %i is not an Enter Flow Prompt\n\0%s(%d): %s unhandled msg: %s" + 0x37D)) != 0))) {
                ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 0x31);
            }
            if ((GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x3B) && ((temp_r3_13 = this->unk38, ((temp_r3_13->unk4->unk50(temp_r3_13) == 0) != 0)) || (HasTransitionEvent__10UIEventMgrCF6Symbol(TheUIEventMgr, (Symbol) __ct__6SymbolFPCc(&sp14, "setup_providers\0user_name.lbl\0OvershellSlot.cpp\0mUserNameLabel\0mState\0Unknown user entry state %i\n\0pUser\0pLocUser\0pUser->IsLocal()\0reconnect_controller.lbl\0overshell_reconnect_controller\0wii_error_remote_extension_x\0skip_choose_part\0forced_part\0trackType != kTrackVocals\0trackType != kTrackDrum\0track == kTrackVocals || !harmony\0track == kTrackDrum || !proDrums\0cymBit != 0\0skip_choose_diff_prompt\0difficulty\0pUser->GetTrackType() != kTrackNone\0required_song_options_chosen\0kick_user\0pUserToKick != NULL\0mSwappableProfilesProvider\0pLocalUser\0swap_user\0mState->GetStateID() == kState_LinkingCode\0\0code\0pUser->GetControllerType() == kControllerVocals\0pUser->GetControllerType() != kControllerVocals\0in_track_mode\0slot_view\0GetUser()->IsLocal()\0pause_menu_quit_token\0mSlotOverrideFlow == kOverrideFlow_None\0type != kOverrideFlow_None\0mSlotOverrideFlow == type\0overshell_up.cue\0overshell_down.cue\0go_to_wiiprofilecreator\0update_controller_type\0update_user_name\0update_local_status\0update_restart_allowed\0update_sign_in_continue\0update_pad_num\0update_remote_feedback\0update_remote_status\0update_show_vocal_bg\0update_song_difficulty_ranking\0update_lefty_and_static_toggle\0set_difficulty_restriction\0update_mics\0update_online_enabled\0!InGame()\0mCharForEdit != NULL\0illegal attempt made to delete guest character\n\0pProfile\0illegal attempt made to rename guest character to %s\n\0pUser && pUser->IsLocal()\0button_error.cue\0overshell_back.cue\0on_start\0Local user %s cannot join\0play_instr_sfx_local\0slider.cue\0overshell_select.cue\0mCharProvider\0update_char_provider\0update_users_provider\0update_profiles_provider\0update_part_select_provider\0update_character_portrait\0update_char_more_options\0slot state %i is not an Enter Flow Prompt\n\0%s(%d): %s unhandled msg: %s" + 0x37D)) != 0))) {
                ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 0x3C);
            }
            if ((GetStateID__18OvershellSlotStateCFv(this->unk20) == 0xC9) && ((temp_r3_14 = this->unk38, ((temp_r3_14->unk4->unk50(temp_r3_14) == 0) != 0)) || (HasTransitionEvent__10UIEventMgrCF6Symbol(TheUIEventMgr, (Symbol) __ct__6SymbolFPCc(&sp10, "setup_providers\0user_name.lbl\0OvershellSlot.cpp\0mUserNameLabel\0mState\0Unknown user entry state %i\n\0pUser\0pLocUser\0pUser->IsLocal()\0reconnect_controller.lbl\0overshell_reconnect_controller\0wii_error_remote_extension_x\0skip_choose_part\0forced_part\0trackType != kTrackVocals\0trackType != kTrackDrum\0track == kTrackVocals || !harmony\0track == kTrackDrum || !proDrums\0cymBit != 0\0skip_choose_diff_prompt\0difficulty\0pUser->GetTrackType() != kTrackNone\0required_song_options_chosen\0kick_user\0pUserToKick != NULL\0mSwappableProfilesProvider\0pLocalUser\0swap_user\0mState->GetStateID() == kState_LinkingCode\0\0code\0pUser->GetControllerType() == kControllerVocals\0pUser->GetControllerType() != kControllerVocals\0in_track_mode\0slot_view\0GetUser()->IsLocal()\0pause_menu_quit_token\0mSlotOverrideFlow == kOverrideFlow_None\0type != kOverrideFlow_None\0mSlotOverrideFlow == type\0overshell_up.cue\0overshell_down.cue\0go_to_wiiprofilecreator\0update_controller_type\0update_user_name\0update_local_status\0update_restart_allowed\0update_sign_in_continue\0update_pad_num\0update_remote_feedback\0update_remote_status\0update_show_vocal_bg\0update_song_difficulty_ranking\0update_lefty_and_static_toggle\0set_difficulty_restriction\0update_mics\0update_online_enabled\0!InGame()\0mCharForEdit != NULL\0illegal attempt made to delete guest character\n\0pProfile\0illegal attempt made to rename guest character to %s\n\0pUser && pUser->IsLocal()\0button_error.cue\0overshell_back.cue\0on_start\0Local user %s cannot join\0play_instr_sfx_local\0slider.cue\0overshell_select.cue\0mCharProvider\0update_char_provider\0update_users_provider\0update_profiles_provider\0update_part_select_provider\0update_character_portrait\0update_char_more_options\0slot state %i is not an Enter Flow Prompt\n\0%s(%d): %s unhandled msg: %s" + 0x37D)) != 0))) {
                ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 0xCD);
            }
            if ((GetStateID__18OvershellSlotStateCFv(this->unk20) == 0xCC) && ((temp_r3_15 = this->unk38, ((temp_r3_15->unk4->unk50(temp_r3_15) == 0) != 0)) || (HasTransitionEvent__10UIEventMgrCF6Symbol(TheUIEventMgr, (Symbol) __ct__6SymbolFPCc(&spC, "setup_providers\0user_name.lbl\0OvershellSlot.cpp\0mUserNameLabel\0mState\0Unknown user entry state %i\n\0pUser\0pLocUser\0pUser->IsLocal()\0reconnect_controller.lbl\0overshell_reconnect_controller\0wii_error_remote_extension_x\0skip_choose_part\0forced_part\0trackType != kTrackVocals\0trackType != kTrackDrum\0track == kTrackVocals || !harmony\0track == kTrackDrum || !proDrums\0cymBit != 0\0skip_choose_diff_prompt\0difficulty\0pUser->GetTrackType() != kTrackNone\0required_song_options_chosen\0kick_user\0pUserToKick != NULL\0mSwappableProfilesProvider\0pLocalUser\0swap_user\0mState->GetStateID() == kState_LinkingCode\0\0code\0pUser->GetControllerType() == kControllerVocals\0pUser->GetControllerType() != kControllerVocals\0in_track_mode\0slot_view\0GetUser()->IsLocal()\0pause_menu_quit_token\0mSlotOverrideFlow == kOverrideFlow_None\0type != kOverrideFlow_None\0mSlotOverrideFlow == type\0overshell_up.cue\0overshell_down.cue\0go_to_wiiprofilecreator\0update_controller_type\0update_user_name\0update_local_status\0update_restart_allowed\0update_sign_in_continue\0update_pad_num\0update_remote_feedback\0update_remote_status\0update_show_vocal_bg\0update_song_difficulty_ranking\0update_lefty_and_static_toggle\0set_difficulty_restriction\0update_mics\0update_online_enabled\0!InGame()\0mCharForEdit != NULL\0illegal attempt made to delete guest character\n\0pProfile\0illegal attempt made to rename guest character to %s\n\0pUser && pUser->IsLocal()\0button_error.cue\0overshell_back.cue\0on_start\0Local user %s cannot join\0play_instr_sfx_local\0slider.cue\0overshell_select.cue\0mCharProvider\0update_char_provider\0update_users_provider\0update_profiles_provider\0update_part_select_provider\0update_character_portrait\0update_char_more_options\0slot state %i is not an Enter Flow Prompt\n\0%s(%d): %s unhandled msg: %s" + 0x37D)) != 0))) {
                ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 0xCE);
            }
            if (GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x13) {
                temp_r3_16 = temp_r30_2->unk4;
                if ((temp_r3_16->unk4->unk1C(temp_r3_16) != 0) && (AllLocalUsersInSessionAreGuests__11BandUserMgrCFv(this->unk34) == 0)) {
                    ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 0x14);
                }
            }
            if ((GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x36) && (DoesAnySlotHaveChar__14OvershellPanelCFP8CharData(this->unk30, this->unk84) != 0)) {
                ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 0x42);
            }
            if (GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x1A) {
                temp_r3_17 = this->unk38;
                if (temp_r3_17->unk4->unk50(temp_r3_17) != 0) {
                    ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 7);
                }
            }
            var_r28 = this->unk38->unk54->unk1C;
            if (var_r28 != NULL) {
                var_r28 = *var_r28;
            }
            if (GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x41) {
                var_r0 = temp_r30_2;
                if (temp_r30_2 != NULL) {
                    var_r0 = temp_r30_2->unk0;
                }
                if ((u32) var_r28 != (u32) var_r0) {
                    LeaveOptions__13OvershellSlotFv(this);
                }
            }
            if (GetStateID__18OvershellSlotStateCFv(this->unk20) == 0xCB) {
                temp_r3_18 = temp_r31->unk0;
                if (IsPrimaryProfileCritical__10ProfileMgrFPC9LocalUser(&TheProfileMgr, temp_r3_18->unk0->unk68(temp_r3_18)) == 0) {
                    LeaveOptions__13OvershellSlotFv(this);
                }
            }
            if (GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x37) {
                temp_r3_19 = temp_r30_2->unk4;
                if (temp_r3_19->unk4->unk2C(temp_r3_19) == 0) {
                    LeaveOptions__13OvershellSlotFv(this);
                }
            }
            if ((GetStateID__18OvershellSlotStateCFv(this->unk20) == 0x20) && ((GetObj__8DataNodeCFPC9DataArray(Property__Q23Hmx6ObjectCF6Symbolb((Hmx::Object *) this->unk20, (Symbol) __ct__6SymbolFPCc(&sp8, "setup_providers\0user_name.lbl\0OvershellSlot.cpp\0mUserNameLabel\0mState\0Unknown user entry state %i\n\0pUser\0pLocUser\0pUser->IsLocal()\0reconnect_controller.lbl\0overshell_reconnect_controller\0wii_error_remote_extension_x\0skip_choose_part\0forced_part\0trackType != kTrackVocals\0trackType != kTrackDrum\0track == kTrackVocals || !harmony\0track == kTrackDrum || !proDrums\0cymBit != 0\0skip_choose_diff_prompt\0difficulty\0pUser->GetTrackType() != kTrackNone\0required_song_options_chosen\0kick_user\0pUserToKick != NULL\0mSwappableProfilesProvider\0pLocalUser\0swap_user\0mState->GetStateID() == kState_LinkingCode\0\0code\0pUser->GetControllerType() == kControllerVocals\0pUser->GetControllerType() != kControllerVocals\0in_track_mode\0slot_view\0GetUser()->IsLocal()\0pause_menu_quit_token\0mSlotOverrideFlow == kOverrideFlow_None\0type != kOverrideFlow_None\0mSlotOverrideFlow == type\0overshell_up.cue\0overshell_down.cue\0go_to_wiiprofilecreator\0update_controller_type\0update_user_name\0update_local_status\0update_restart_allowed\0update_sign_in_continue\0update_pad_num\0update_remote_feedback\0update_remote_status\0update_show_vocal_bg\0update_song_difficulty_ranking\0update_lefty_and_static_toggle\0set_difficulty_restriction\0update_mics\0update_online_enabled\0!InGame()\0mCharForEdit != NULL\0illegal attempt made to delete guest character\n\0pProfile\0illegal attempt made to rename guest character to %s\n\0pUser && pUser->IsLocal()\0button_error.cue\0overshell_back.cue\0on_start\0Local user %s cannot join\0play_instr_sfx_local\0slider.cue\0overshell_select.cue\0mCharProvider\0update_char_provider\0update_users_provider\0update_profiles_provider\0update_part_select_provider\0update_character_portrait\0update_char_more_options\0slot state %i is not an Enter Flow Prompt\n\0%s(%d): %s unhandled msg: %s" + 0x21E), 1), NULL), temp_r3_20 = __dynamic_cast(0, &__RTTI__13LocalBandUser, &__RTTI__Q23Hmx6Object, 0), temp_r3_21 = temp_r3_20->unk4, ((temp_r3_21->unk4->unk24(temp_r3_21) == 0) != 0)) || (temp_r3_22 = temp_r3_20->unk4, ((temp_r3_22->unk4->unk18(temp_r3_22) == 0) != 0)) || (temp_r3_23 = temp_r3_20->unk4, ((temp_r3_23->unk4->unk20(temp_r3_23) == 0) == 0)))) {
                ShowState__13OvershellSlotF20OvershellSlotStateID(this, (OvershellSlotStateID) 0x1F);
            }
        }
    }
}