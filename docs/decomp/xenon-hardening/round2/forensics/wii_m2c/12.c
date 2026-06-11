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