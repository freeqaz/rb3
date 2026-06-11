typedef struct ProfileMgr {
    /* 0x000 */ char pad0[0x558];
    /* 0x558 */ s8 unk558;                          /* inferred */
    /* 0x559 */ char pad559[3];                     /* maybe part of unk558[4]? */
    /* 0x55C */ s32 unk55C;                         /* inferred */
    /* 0x560 */ s32 unk560;                         /* inferred */
    /* 0x564 */ s32 unk564;                         /* inferred */
    /* 0x568 */ s32 unk568;                         /* inferred */
    /* 0x56C */ s32 unk56C;                         /* inferred */
    /* 0x570 */ s32 unk570;                         /* inferred */
    /* 0x574 */ u8 unk574;                          /* inferred */
    /* 0x575 */ u8 unk575;                          /* inferred */
    /* 0x576 */ char pad576[2];                     /* maybe part of unk575[3]? */
    /* 0x578 */ f32 unk578;                         /* inferred */
    /* 0x57C */ f32 unk57C;                         /* inferred */
    /* 0x580 */ u8 unk580;                          /* inferred */
    /* 0x581 */ u8 unk581;                          /* inferred */
    /* 0x582 */ char pad582[2];                     /* maybe part of unk581[3]? */
    /* 0x584 */ s32 unk584;                         /* inferred */
    /* 0x588 */ u8 unk588;                          /* inferred */
    /* 0x589 */ u8 unk589;                          /* inferred */
    /* 0x58A */ u8 unk58A;                          /* inferred */
    /* 0x58B */ u8 unk58B;                          /* inferred */
    /* 0x58C */ char pad58C[0x18];                  /* maybe part of unk58B[0x19]? */
    /* 0x5A4 */ u8 unk5A4;                          /* inferred */
    /* 0x5A5 */ char pad5A5[3];                     /* maybe part of unk5A4[4]? */
    /* 0x5A8 */ s32 unk5A8;                         /* inferred */
    /* 0x5AC */ s32 unk5AC;                         /* inferred */
    /* 0x5B0 */ u8 unk5B0;                          /* inferred */
    /* 0x5B1 */ u8 unk5B1;                          /* inferred */
    /* 0x5B2 */ char pad5B2[1];
    /* 0x5B3 */ u8 unk5B3;                          /* inferred */
    /* 0x5B4 */ u8 unk5B4;                          /* inferred */
    /* 0x5B5 */ char pad5B5[3];                     /* maybe part of unk5B4[4]? */
    /* 0x5B8 */ s32 unk5B8;                         /* inferred */
    /* 0x5BC */ char pad5BC[0x10];                  /* maybe part of unk5B8[5]? */
    /* 0x5CC */ s32 unk5CC;                         /* inferred */
} ProfileMgr;                                       /* size >= 0x5D0 */

? Save__11ModifierMgrFR23FixedSizeSaveableStream(ModifierMgr *this, FixedSizeSaveableStream *arg0); /* extern */
? WriteEndian__9BinStreamFPCvi(BinStream *this, void *arg0, s32 arg1); /* extern */
? Write__9BinStreamFPCvi(BinStream *this, void *arg0, s32 arg1); /* extern */
void *__ct__6StringFPCc(String *this, s8 *arg0);    /* extern */
void *__dt__6StringFv(String *this, s16 destroyFlag); /* extern */
? __ls__9BinStreamFRC6String(BinStream *this, String *arg0); /* extern */
extern ModifierMgr *TheModifierMgr;
static ? @stringBase0;                              /* unable to generate initializer: unknown type */

/* ProfileMgr::SaveGlobalOptions (FixedSizeSaveableStream &) */
void SaveGlobalOptions__10ProfileMgrFR23FixedSizeSaveableStream(ProfileMgr *this, FixedSizeSaveableStream *arg0) {
    String sp58;
    s32 sp54;
    s32 sp50;
    s32 sp4C;
    f32 sp48;
    f32 sp44;
    s32 sp40;
    s32 sp3C;
    s32 sp38;
    s32 sp34;
    s32 sp30;
    s32 sp2C;
    s32 sp28;
    s32 sp24;
    s32 sp20;
    s32 sp1C;
    s32 sp18;
    u8 sp15;
    u8 sp14;
    u8 sp13;
    u8 sp12;
    u8 sp11;
    u8 sp10;
    u8 spF;
    u8 spE;
    u8 spD;
    u8 spC;
    u8 spB;
    s8 spA;
    u8 sp9;
    u8 sp8;

    sp4C = 0x20007;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp4C, 4);
    sp48 = this->unk578;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp48, 4);
    sp44 = this->unk57C;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp44, 4);
    sp40 = this->unk55C;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp40, 4);
    sp3C = this->unk560;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp3C, 4);
    sp38 = this->unk564;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp38, 4);
    sp34 = this->unk56C;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp34, 4);
    sp15 = this->unk580;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp15, 1);
    sp30 = this->unk568;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp30, 4);
    sp14 = this->unk581;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp14, 1);
    sp2C = this->unk584;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp2C, 4);
    sp13 = this->unk588;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp13, 1);
    sp12 = this->unk589;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp12, 1);
    sp11 = this->unk5A4;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp11, 1);
    sp28 = this->unk5A8;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp28, 4);
    sp24 = this->unk5AC;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp24, 4);
    sp10 = this->unk5B0;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp10, 1);
    spF = this->unk5B1;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spF, 1);
    sp20 = this->unk570;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp20, 4);
    spE = this->unk574;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spE, 1);
    spD = this->unk575;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spD, 1);
    sp1C = this->unk5CC;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp1C, 4);
    spC = this->unk58A;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spC, 1);
    spB = this->unk58B;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spB, 1);
    Save__11ModifierMgrFR23FixedSizeSaveableStream(TheModifierMgr, arg0);
    __ct__6StringFPCc(&sp58, "profile_mgr\0signin_changed\0ProfileMgr.cpp\0profile\0padNum != -1\0pUser\0netServer\0mGlobalOptionsSaveState != kMetaProfileUnchanged\0\0%s can't load new %s version %d > %d\0%s can't load new %s alt version %d > %d\0sound\0slider\0slider_voicechat\0mSliderConfig\00 <= ixVol && ixVol < mSliderConfig->Size() - 1\0Mic:%s\n\0ratio_sliders\0background_music_level.fade\0crowd_level.fade\0sfx_level.fade\0instrument_level.fade\0bass_boost.send\0wet_gain\0dry_gain\0mVoiceChatSliderConfig\00 <= mVoiceChatVolume && mVoiceChatVolume < mVoiceChatSliderConfig->Size() - 1\0crowd_audio.volume\0error_message\0init\0passive_message_use_headphones\0passive_message_dont_use_headphones\00\0PlatformAudioLatency:%f\n\0PlatformVideoLatency:%f\n\0( 0) <= (type) && (type) < ( kJoypadNumTypes)\0( 0) <= (ctx) && (ctx) < ( kNumLagContexts)\0print_mic_gains\0Mic gain is %f\n\0Mic gain is forced to %f (would be %f if not forcing)\n\0mic\0Mic gain forcing: %s\n\0INACTIVE\0ACTIVE\0Mic output gain forcing: %s - output gain for mic %d is %f dB\n\0pProfile\0campaign\0tour\0qp_career_songinfo\0qp_coop\0user\0pMachineMgr\0%s(%d): %s unhandled msg: %s" + 0x80);
    __ls__9BinStreamFRC6String((BinStream *) arg0, &sp58);
    __dt__6StringFv(&sp58, -1);
    spA = 0;
    Write__9BinStreamFPCvi((BinStream *) arg0, &spA, 1);
    sp54 = 0;
    sp50 = 0;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp50, 8);
    sp9 = this->unk5B3;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp9, 1);
    sp8 = this->unk5B4;
    Write__9BinStreamFPCvi((BinStream *) arg0, &sp8, 1);
    sp18 = this->unk5B8;
    WriteEndian__9BinStreamFPCvi((BinStream *) arg0, &sp18, 4);
    this->unk558 = 0;
}