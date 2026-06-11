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