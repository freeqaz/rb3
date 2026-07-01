typedef struct VocalTrack {
    /* 0x00 */ char pad0[0x1C];
    /* 0x1C */ TrackConfig unk1C;                   /* inferred */
    /* 0x1C */ char pad1C[0x50];
    /* 0x6C */ s32 unk6C;                           /* inferred */
    /* 0x70 */ char pad70[4];
    /* 0x74 */ f32 unk74;                           /* inferred */
    /* 0x78 */ f32 unk78;                           /* inferred */
    /* 0x7C */ char pad7C[0xC];                     /* maybe part of unk78[4]? */
    /* 0x88 */ VocalTrackDir *unk88;                /* inferred */
    /* 0x8C */ char pad8C[8];                       /* maybe part of unk88[3]? */
    /* 0x94 */ void *unk94;                         /* inferred */
} VocalTrack;                                       /* size >= 0x98 */

typedef struct VocalTrackDir {
    /* 0x000 */ Hmx::Object *unk0;                  /* inferred */
    /* 0x004 */ char pad4[0x40C];                   /* maybe part of unk0[0x104]? */
    /* 0x410 */ f32 unk410;                         /* inferred */
    /* 0x414 */ f32 unk414;                         /* inferred */
} VocalTrackDir;                                    /* size >= 0x418 */

RndAnimatable *@STRING@Find<13RndAnimatable>__9ObjectDirFPCcb_P13RndAnimatable(ObjectDir *this, s8 *arg0, s32 arg1); /* extern */
MetaPerformer *Current__13MetaPerformerFv(MetaPerformer *this); /* extern */
? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
? FindObject__9ObjectDirFPCcb(ObjectDir *this, s8 *arg0, s32 arg1); /* extern */
void *GetActivePlayers__4GameFv(Game *this);        /* extern */
s8 *MakeString<PCc,PCc>__FPCcPCcPCc_PCc(s8 *arg0, s8 *arg1, s8 *arg2); /* extern */
s8 *PathName__FPCQ23Hmx6Object(Hmx::Object *arg0);  /* extern */
f64 ScrollSpeed__16BandSongMetadataCFv(BandSongMetadata *this); /* extern */
s32 Song__13MetaPerformerCFv(MetaPerformer *this);  /* extern */
s32 TrackNum__11TrackConfigCFv(TrackConfig *this);  /* extern */
? UpdateConfiguration__13VocalTrackDirFv(VocalTrackDir *this); /* extern */
void **__dynamic_cast(s32, struct RTTI *, struct RTTI *, struct RTTI *, ?); /* extern */
? RebuildHUD__10VocalTrackFv(VocalTrack *this);     /* static */
extern f32 @F_00000000;
extern f32 @F_0000803f;
extern Debug TheDebug;
extern Game *TheGame;
extern void *TheSongMgr;
extern struct RTTI __RTTI__13RndAnimatable;
extern struct RTTI __RTTI__5Track;
extern struct RTTI __RTTI__Q23Hmx6Object;
extern s8 *kNotObjectMsg;
static struct RTTI __RTTI__10VocalTrack;            /* unable to generate initializer: cannot parse @60983 as integer */
static ? @stringBase0;                              /* unable to generate initializer: unknown type */

/* VocalTrack::UpdateVocalStyle (void) */
void UpdateVocalStyle__10VocalTrackFv(VocalTrack *this, ? arg_sp0) {
    s32 sp8;
    VocalTrackDir *temp_r3_2;
    VocalTrackDir *temp_r3_3;
    VocalTrackDir *var_r26;
    VocalTrackDir *var_r3;
    s32 temp_r0_2;
    s32 temp_r26;
    s32 var_r30;
    s8 *temp_r27_2;
    s8 *var_r5;
    u32 var_r25;
    void **temp_r3;
    void **temp_r3_4;
    void *temp_r0;
    void *temp_r24;
    void *temp_r27;
    void *temp_r4;

    temp_r0 = this->unk94;
    temp_r27 = GetActivePlayers__4GameFv(TheGame);
    if ((temp_r0 != NULL) && (temp_r0->unk4->unk2C(temp_r0) == 0)) {
        var_r25 = 0U;
        var_r30 = 0;
loop_10:
        if (var_r25 < (u16) temp_r27->unk4) {
            temp_r24 = *(temp_r27->unk0 + var_r30);
            if ((temp_r24 != NULL) && ((s32) temp_r24->unk24C == 3)) {
                temp_r26 = temp_r24->unk248;
                if ((temp_r26 != TrackNum__11TrackConfigCFv(&this->unk1C)) && (temp_r24->unk4->unk2C(temp_r24) != 0)) {
                    temp_r3 = __dynamic_cast(temp_r24->unk230->unk7C, NULL, &__RTTI__10VocalTrack, &__RTTI__5Track, 0);
                    if (temp_r3 != NULL) {
                        (*temp_r3)->unk188(this->unk6C);
                    }
                }
            }
            var_r25 += 1;
            var_r30 += 4;
            goto loop_10;
        }
    }
    temp_r3_2 = this->unk88;
    if (temp_r3_2 != NULL) {
        temp_r4 = this->unk94;
        if (temp_r4 != NULL) {
            temp_r0_2 = temp_r4->unk250;
            if (temp_r0_2 != 1) {
                if (temp_r0_2 == 4) {
                    return;
                }
                goto block_16;
            }
        } else {
block_16:
            UpdateConfiguration__13VocalTrackDirFv(temp_r3_2);
            temp_r3_3 = this->unk88;
            this->unk78 = temp_r3_3->unk414 - temp_r3_3->unk410;
            sp8 = Song__13MetaPerformerCFv(Current__13MetaPerformerFv((MetaPerformer *) temp_r3_3));
            temp_r27_2 = "popping unbaked plate\0%s recycling plate at %.2f sec\n\0%s baking plate at %.2f sec\n\0Too many tube plates - please file a bug to Josh Stoddard and include the Watson output.\0max plates queued -> %d\n\0max verts in a plate -> %d\n\0max faces in a plate -> %d\n\0resetting all plates\n\0dumping plates in %s\n\0\t[%d] @ %x, xPos: %.2f, xStart: %.2f, XEnd: %.2f, verts: %d, faces: %d, baked: %d\n\0\t[%d] @ %x, <empty>, verts: %d, faces: %d, baked: %d\n\0part %d front\0part %d back\0part %d phoneme\0lead deploy\0harmony deploy\0deploy_mask_lead.mat\0deploy_mask_harmony.mat\0%s new plate added.  Please alert HUD/Track owner and include the Watson output.\0vocal_jitter_debug\0VocalTrack.cpp\0list.empty()\0pUser\0force_static_vocals\0track_graphics\0markers.grp\0beat_marker.mesh\0config/track_graphics.dta\0lyric_overlap_ms\0static_vocal_parameters\0static_deploy_x_size\0static_deploy_buffer_x\0static_phrase_margin_x\0lyric_shift_ms\0lyric_shift_anticipation_ms\0min_lyric_highlight_ms\0phrase_highlight_ms\0lyric timing data:\n\0\t overlap window ms %.0f\n\0\t static deploy size %.2f\n\0\t static deploy gap size %.2f\n\0\t now bar offset %.2f\n\0\t standard lyric shift ms %.0f\n\0\t fast lyric shift ms %.0f\n\0\t lyric shift anticipation ms %.0f\n\0\t min lyric highlight ms %.0f\n\0\t phrase highlight anticipation ms %.0f\n\0part < 3\0creating new %s lyric plate\n\0lead\0harmony\0Max Lyric Plates: %d\n\0GraphicsUtl.h\0result\0VocalTrack::CreateMarker() added new %s mesh at run-time (total %d); please alert HUD/Track owner\0mesh\0mesh->GeomOwner() != mesh\0tambourine_preview.anim\0lead_color\0harmony_1_color\0harmony_2_color\0Range Shift Data\n\0[%d]\tstart ms: %.2f, intro ms: %.2f, min: %.1f -> %.1f, max: %.1f -> %.1f\n\0( 0) <= (part) && (part) < ( 3)\0Dumping %s lyric plates\n\0[%d] %x (%.2f - %.2f) %s\n\0\t<empty>\n\0\t[%d] %x\0 %s x:%.2f (%.2f - %.2f)\n\0\n\0recycling lyric plate at %.2f sec %s\n\0current: %i\n\0debug_score_current.txt\0\0relative lyric placement changed in baked plate (lead)\0relative lyric placement changed in baked plate (harmony)\0\t%3.2f\t(%6.2fms)\t\0| \0\"%s\"\0 |\0downbeat_marker.mesh\0phrase_marker.mesh\0Finished shifting lyrics for part %d to %.2f at %.2f sec\n\0Sliding lyrics for part %d to %.2f at %.2f sec\n\0lyrics.grp\0lyrics_harmony.grp\0deploy zones for part %d by song seconds\n\0[%d] %.2f - %.2f\n\0Adding lyrics for part %d: %d - %d\n\0Skipping redundant lyric \"%s\" @ %d\n\0NEW EXTRA LYRIC: \"%s\" @ %d\n\0NEW LYRIC: \"%s\" @ %d\n\0lyric.mDeployIdx (%d) < mNextDeployZone (%d) for part %d\n\0tight shift between '%s' and '%s' at %.2f sec: %.0f ms preview\n\0lyric phrase too big for window: \"%s\"\0new final deploy section for part %d\n\0tambourine_gems.mm\0--------\n\0singer->FrameTargetPitch()\0: \0singer->FrameMicPitch()\0singer->FrameBestHit()\0mPlayer->Freestyling()\0phoneme phrase\n\0non-singing section\n\0singing\n\0pitchFrame\0frameScore\0harmonyScore\0pitchZ\0v.z\0vocal_feedback.anim\0last: %i\n\0clearing all lyric plates\n\0LINE %d NOTE %d TIME %.2f PITCHES \0UNPITCHED \0tube pitch to z: %d -> %1.2f\n\0mCharOptMicID != -1\0trying to increment unimplemented vocal param %d\0scrolling\0static\0unrecognized\0invalid vocal player\0%s(%d): %s unhandled msg: %s\0New LyricShift begin %.2f sec, end %.2f x, fast: %d\n" + 0x5CD;
            var_r26 = this->unk88;
            this->unk74 = (f32) ScrollSpeed__16BandSongMetadataCFv(TheSongMgr->unk4->unk70(TheSongMgr, TheSongMgr->unk4->unkA8(TheSongMgr, &sp8, 1))) * (this->unk78 / 16.8f);
            FindObject__9ObjectDirFPCcb((ObjectDir *) var_r26, temp_r27_2, 0);
            temp_r3_4 = __dynamic_cast(0, &__RTTI__13RndAnimatable, &__RTTI__Q23Hmx6Object, NULL);
            if (temp_r3_4 == NULL) {
                var_r3 = var_r26;
                if (var_r26 != NULL) {
                    var_r3 = (VocalTrackDir *) var_r26->unk0;
                }
                if (PathName__FPCQ23Hmx6Object((Hmx::Object *) var_r3) != NULL) {
                    if (var_r26 != NULL) {
                        var_r26 = (VocalTrackDir *) var_r26->unk0;
                    }
                    var_r5 = PathName__FPCQ23Hmx6Object((Hmx::Object *) var_r26);
                } else {
                    var_r5 = (s8 *) @STRING@Find<13RndAnimatable>__9ObjectDirFPCcb_P13RndAnimatable;
                }
                Fail__5DebugFPCc(&TheDebug, MakeString<PCc,PCc>__FPCcPCcPCc_PCc(kNotObjectMsg, temp_r27_2, var_r5));
            }
            temp_r3_4->unk4->unk34(temp_r3_4, &@F_0000803f, &@F_00000000, @F_00000000, @F_0000803f);
            RebuildHUD__10VocalTrackFv(this);
        }
    }
}