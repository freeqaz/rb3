typedef struct BandUser {
    /* 0x00 */ char pad0[0x7C];
    /* 0x7C */ s32 unk7C;                           /* inferred */
} BandUser;                                         /* size >= 0x80 */

typedef struct DataArray {
    /* 0x0 */ char pad0[0xA];
    /* 0xA */ s16 unkA;                             /* inferred */
} DataArray;                                        /* size >= 0xC */

typedef struct GemTrack {
    /* 0x00 */ char pad0[0x74];
    /* 0x74 */ TrackDir *unk74;                     /* inferred */
} GemTrack;                                         /* size >= 0x78 */

typedef struct GemTrainerPanel {
    /* 0x00 */ char pad0[4];
    /* 0x04 */ void *unk4;                          /* inferred */
    /* 0x08 */ char pad8[0x54];                     /* maybe part of unk4[0x16]? */
    /* 0x5C */ void *unk5C;                         /* inferred */
    /* 0x60 */ s32 unk60;                           /* inferred */
    /* 0x64 */ char pad64[0xC];                     /* maybe part of unk60[4]? */
    /* 0x70 */ GemTrack *unk70;                     /* inferred */
    /* 0x74 */ GemManager *unk74;                   /* inferred */
    /* 0x78 */ s32 unk78;                           /* inferred */
    /* 0x7C */ BandUser **unk7C;                    /* inferred */
    /* 0x80 */ char pad80[4];
    /* 0x84 */ s32 unk84;                           /* inferred */
    /* 0x88 */ stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>> unk88; /* inferred */
    /* 0x88 */ char pad88[0x21];
    /* 0xA9 */ u8 unkA9;                            /* inferred */
    /* 0xAA */ char padAA[2];                       /* maybe part of unkA9[3]? */
    /* 0xAC */ s32 unkAC;                           /* inferred */
    /* 0xB0 */ char padB0[4];
    /* 0xB4 */ TrainerGemTab *unkB4;                /* inferred */
    /* 0xB8 */ Metronome *unkB8;                    /* inferred */
    /* 0xBC */ char padBC[0x14];                    /* maybe part of unkB8[6]? */
    /* 0xD0 */ u8 unkD0;                            /* inferred */
} GemTrainerPanel;                                  /* size >= 0xD1 */

typedef struct ObjectDir {
    /* 0x0 */ Hmx::Object *unk0;                    /* inferred */
} ObjectDir;                                        /* size >= 0x4 */

typedef struct stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>> {
    /* 0x0 */ char pad0[8];
    /* 0x8 */ ? unk8;                               /* inferred */
    /* 0x8 */ char pad8[1];
} stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>>; /* size >= 0x9 */

RndDir *@STRING@Find<6RndDir>__9ObjectDirFPCcb_P6RndDir(ObjectDir *this, s8 *arg0, s32 arg1); /* extern */
? ClearAllGemWidgets__8TrackDirFv(TrackDir *this);  /* extern */
? ClearAllGems__10GemManagerFv(GemManager *this);   /* extern */
? ClearMissedPhrases__10GemManagerFv(GemManager *this); /* extern */
? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
? FindObject__9ObjectDirFPCcb(ObjectDir *this, s8 *arg0, s32 arg1); /* extern */
u32 FocusPanel__9UIManagerFv(UIManager *this);      /* extern */
s32 GetCurrSection__12TrainerPanelCFv(TrainerPanel *this); /* extern */
s32 GetDifficulty__8BandUserCFv(BandUser *this);    /* extern */
s32 GetGemListByDiff__6SongDBCFii(SongDB *this, s32 arg0, s32 arg1); /* extern */
GemManager *GetGemManager__8GemTrackFv(GemTrack *this); /* extern */
s32 GetLoopTicks__12TrainerPanelCFi(TrainerPanel *this, s32 arg0); /* extern */
f32 GetMusicSpeed__4GameCFv(Game *this);            /* extern */
s32 GetSectionTicks__12TrainerPanelCFi(TrainerPanel *this, s32 arg0); /* extern */
TrainerSection *GetSection__12TrainerPanelFi(TrainerPanel *this, s32 arg0); /* extern */
s32 GetStartTick__14TrainerSectionCFv(TrainerSection *this); /* extern */
s32 GetTick__12TrainerPanelCFv(TrainerPanel *this); /* extern */
s32 GetType__5TrackCFv(Track *this);                /* extern */
? Init__13TrainerGemTabFP6RndDir9TrackType(TrainerGemTab *this, RndDir *arg0, TrackType arg1); /* extern */
s32 IsWaiting__4GameFv(Game *this);                 /* extern */
s8 *MakeString<PCc,PCc>__FPCcPCcPCc_PCc(s8 *arg0, s8 *arg1, s8 *arg2); /* extern */
s8 *MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(s8 *arg0, s8 *arg1, s32 arg2, s8 *arg3); /* extern */
s8 *PathName__FPCQ23Hmx6Object(Hmx::Object *arg0);  /* extern */
? Poll__7UIPanelFv(UIPanel *this);                  /* extern */
? Poll__9MetronomeFiQ29Metronome15OverrideEnabled(Metronome *this, s32 arg0, Metronome::OverrideEnabled arg1); /* extern */
s32 SymToTrackType__F6Symbol(Symbol arg0);          /* extern */
void *__dt__9DataArrayFv(DataArray *this, s16 destroyFlag); /* extern */
GemTrack *__dynamic_cast(?, struct RTTI *, struct RTTI *, ?); /* extern */
? AddBeatMask__15GemTrainerPanelFi(GemTrainerPanel *this, s32 arg0); /* static */
? HandleTrackShifting__15GemTrainerPanelFv(GemTrainerPanel *this); /* static */
void Poll__15GemTrainerPanelFv(GemTrainerPanel *this); /* static */
? __as__Q211stlpmtx_std65_Vector_impl<7GameGem,Us,Q211stlpmtx_std22StlNodeAlloc<7GameGem>>FRCQ211stlpmtx_std65_Vector_impl<7GameGem,Us,Q211stlpmtx_std22StlNodeAlloc<7GameGem>>(stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>> *this, stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>> *arg0); /* static */
extern f32 @F_0000803f;
extern Debug TheDebug;
extern Game *TheGame;
extern SongDB *TheSongDB;
extern UIManager *TheUI;
extern struct RTTI __RTTI__5Track;
extern struct RTTI __RTTI__6RndDir;
extern struct RTTI __RTTI__8GemTrack;
extern struct RTTI __RTTI__Q23Hmx6Object;
extern s8 *kAssertStr;
extern s8 *kNotObjectMsg;
extern ? loop_msg;
static ? @stringBase0;                              /* unable to generate initializer: unknown type */

/* GemTrainerPanel::Poll (void) */
void Poll__15GemTrainerPanelFv(GemTrainerPanel *this, ? arg_sp0) {
    DataArray *sp10;
    s32 sp8;
    ? *temp_r4;
    ? *temp_r6;
    ? *temp_r6_2;
    ? var_r27_2;
    GemManager *temp_r3_2;
    GemTrack *temp_r3;
    GemTrack *temp_r3_5;
    GemTrainerPanel *var_r28;
    ObjectDir *temp_r3_4;
    ObjectDir *var_r28_2;
    ObjectDir *var_r3;
    TrainerSection *temp_r27;
    s16 temp_r0_2;
    s32 temp_r0;
    s32 temp_r31;
    s32 temp_r3_3;
    s32 temp_r3_7;
    s32 temp_r3_8;
    s32 temp_r3_9;
    s32 var_r27;
    s32 var_r4;
    s8 *temp_r29;
    s8 *var_r5;
    stlpmtx_std::_Vector_impl<GameGem, short unsigned, stlpmtx_std::StlNodeAlloc<GameGem>> *var_r31;
    void *temp_r3_6;

    if (((GemTrack *) this->unk70 == NULL) && ((void *) this->unk5C != NULL) && ((s32) (*this->unk7C)->unk7C != 0)) {
        temp_r3 = __dynamic_cast(0, &__RTTI__8GemTrack, &__RTTI__5Track, 0);
        this->unk70 = temp_r3;
        if (temp_r3 == NULL) {
            temp_r6 = "reset_score\0metronome_hi.cue\0metronome_lo.cue\0m\0\0set_key\0trainers/song_name\0GemTrainerPanel.cpp\0mTrack != NULL\0mGemManager != NULL\0gem_preview\0range == 10.0f\0update_thermometer\0i < mGemPlayer->GetGemStatus()->GetSize()\0!mPattern.empty()\0trainers/speed\0beat_mask.wid\0trainers/metronome\0song_lessons\0%s(%d): %s unhandled msg: %s\0vector";
            Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r6 + 0x4C, 0xC1, temp_r6 + 0x60));
        }
        temp_r3_2 = GetGemManager__8GemTrackFv(this->unk70);
        this->unk74 = temp_r3_2;
        if (temp_r3_2 == NULL) {
            temp_r6_2 = "reset_score\0metronome_hi.cue\0metronome_lo.cue\0m\0\0set_key\0trainers/song_name\0GemTrainerPanel.cpp\0mTrack != NULL\0mGemManager != NULL\0gem_preview\0range == 10.0f\0update_thermometer\0i < mGemPlayer->GetGemStatus()->GetSize()\0!mPattern.empty()\0trainers/speed\0beat_mask.wid\0trainers/metronome\0song_lessons\0%s(%d): %s unhandled msg: %s\0vector";
            Fail__5DebugFPCc(&TheDebug, MakeString<PCc,i,PCc>__FPCcPCciPCc_PCc(kAssertStr, temp_r6_2 + 0x4C, 0xC5, temp_r6_2 + 0x6F));
        }
        var_r28 = this;
        var_r31 = &this->unk88;
        var_r27 = 0;
        do {
            temp_r3_3 = GetGemListByDiff__6SongDBCFii(TheSongDB, this->unk5C->unk248, var_r27);
            var_r28->unk60 = temp_r3_3;
            __as__Q211stlpmtx_std65_Vector_impl<7GameGem,Us,Q211stlpmtx_std22StlNodeAlloc<7GameGem>>FRCQ211stlpmtx_std65_Vector_impl<7GameGem,Us,Q211stlpmtx_std22StlNodeAlloc<7GameGem>>(var_r31, temp_r3_3 + 4);
            var_r27 += 1;
            var_r31 += 8;
            var_r28 += 4;
        } while (var_r27 < 4);
        temp_r4 = "reset_score\0metronome_hi.cue\0metronome_lo.cue\0m\0\0set_key\0trainers/song_name\0GemTrainerPanel.cpp\0mTrack != NULL\0mGemManager != NULL\0gem_preview\0range == 10.0f\0update_thermometer\0i < mGemPlayer->GetGemStatus()->GetSize()\0!mPattern.empty()\0trainers/speed\0beat_mask.wid\0trainers/metronome\0song_lessons\0%s(%d): %s unhandled msg: %s\0vector";
        temp_r29 = temp_r4 + 0x83;
        temp_r3_4 = this->unk4->unk20(this, temp_r4);
        var_r28_2 = temp_r3_4;
        FindObject__9ObjectDirFPCcb(temp_r3_4, temp_r29, 0);
        temp_r3_5 = __dynamic_cast(0, &__RTTI__6RndDir, &__RTTI__Q23Hmx6Object, 0);
        if (temp_r3_5 == NULL) {
            var_r3 = var_r28_2;
            if (var_r28_2 != NULL) {
                var_r3 = (ObjectDir *) var_r28_2->unk0;
            }
            if (PathName__FPCQ23Hmx6Object((Hmx::Object *) var_r3) != NULL) {
                if (var_r28_2 != NULL) {
                    var_r28_2 = (ObjectDir *) var_r28_2->unk0;
                }
                var_r5 = PathName__FPCQ23Hmx6Object((Hmx::Object *) var_r28_2);
            } else {
                var_r5 = (s8 *) @STRING@Find<6RndDir>__9ObjectDirFPCcb_P6RndDir;
            }
            Fail__5DebugFPCc(&TheDebug, MakeString<PCc,PCc>__FPCcPCcPCc_PCc(kNotObjectMsg, temp_r29, var_r5));
        }
        sp8 = GetType__5TrackCFv((Track *) this->unk70);
        Init__13TrainerGemTabFP6RndDir9TrackType(this->unkB4, (RndDir *) temp_r3_5, (TrackType) SymToTrackType__F6Symbol((Symbol) &sp8));
    }
    Poll__7UIPanelFv((UIPanel *) this);
    if (((GemManager *) this->unk74 != NULL) && ((void *) this->unk5C != NULL)) {
        if (GetCurrSection__12TrainerPanelCFv((TrainerPanel *) this) < 0) {
            return;
        }
        if (IsWaiting__4GameFv(TheGame) != 0) {
            if (((s32) this->unk78 != GetDifficulty__8BandUserCFv(*this->unk7C)) && ((s32) this->unkD0 == 0)) {
                var_r4 = 0;
loop_31:
                temp_r3_6 = this->unk5C->unk2D0;
                if (var_r4 < (s32) temp_r3_6->unk4) {
                    if (var_r4 != -1) {
                        temp_r3_7 = temp_r3_6->unk0;
                        *(temp_r3_7 + var_r4) = *(temp_r3_7 + var_r4) | 0x40;
                    }
                    var_r4 += 1;
                    goto loop_31;
                }
                this->unkD0 = 1;
            }
        } else {
            this->unkD0 = 0;
            if ((s32) this->unkA9 != 0) {
                temp_r27 = GetSection__12TrainerPanelFi((TrainerPanel *) this, GetCurrSection__12TrainerPanelCFv((TrainerPanel *) this));
                temp_r31 = GetSectionTicks__12TrainerPanelCFi((TrainerPanel *) this, GetCurrSection__12TrainerPanelCFv((TrainerPanel *) this));
                AddBeatMask__15GemTrainerPanelFi(this, GetStartTick__14TrainerSectionCFv(temp_r27) + temp_r31);
                this->unkA9 = 0;
            }
            temp_r3_8 = GetTick__12TrainerPanelCFv((TrainerPanel *) this);
            temp_r0 = this->unk84;
            if ((temp_r3_8 >= temp_r0) && (temp_r0 != 0)) {
                if ((s32) this->unkAC > 0) {
                    this->unk4->unk10(&sp10, this, loop_msg.unk4, 1);
                    if (sp14 & 0x10) {
                        temp_r0_2 = sp10->unkA - 1;
                        sp10->unkA = temp_r0_2;
                        if (temp_r0_2 == 0) {
                            __dt__9DataArrayFv(sp10, 1);
                        }
                    }
                }
                this->unk84 += GetLoopTicks__12TrainerPanelCFi((TrainerPanel *) this, GetCurrSection__12TrainerPanelCFv((TrainerPanel *) this));
                this->unkAC += 1;
            }
            var_r27_2 = 0;
            if (FocusPanel__9UIManagerFv(TheUI) != this) {
                var_r27_2 = 2;
            } else if (@F_0000803f != GetMusicSpeed__4GameCFv(TheGame)) {
                var_r27_2 = 1;
            }
            Poll__9MetronomeFiQ29Metronome15OverrideEnabled(this->unkB8, temp_r3_8, (Metronome::OverrideEnabled) var_r27_2);
            HandleTrackShifting__15GemTrainerPanelFv(this);
            temp_r3_9 = GetDifficulty__8BandUserCFv(*this->unk7C);
            if ((s32) this->unk78 != temp_r3_9) {
                ClearAllGems__10GemManagerFv(this->unk74);
                ClearMissedPhrases__10GemManagerFv(this->unk74);
                ClearAllGemWidgets__8TrackDirFv(this->unk70->unk74);
                this->unk4->unk7C(this, temp_r3_8, temp_r3_9);
            }
        }
    }
}