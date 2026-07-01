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