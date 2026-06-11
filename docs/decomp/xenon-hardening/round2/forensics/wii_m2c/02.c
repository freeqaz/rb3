typedef struct Stats {
    /* 0x000 */ s32 unk0;                           /* inferred */
    /* 0x004 */ char pad4[0xC];                     /* maybe part of unk0[4]? */
    /* 0x010 */ s32 unk10;                          /* inferred */
    /* 0x014 */ s32 unk14;                          /* inferred */
    /* 0x018 */ f32 unk18;                          /* inferred */
    /* 0x01C */ u8 unk1C;                           /* inferred */
    /* 0x01D */ char pad1D[3];                      /* maybe part of unk1C[4]? */
    /* 0x020 */ s32 unk20;                          /* inferred */
    /* 0x024 */ s32 unk24;                          /* inferred */
    /* 0x028 */ s32 unk28;                          /* inferred */
    /* 0x02C */ s32 unk2C;                          /* inferred */
    /* 0x030 */ char pad30[8];                      /* maybe part of unk2C[3]? */
    /* 0x038 */ s32 unk38;                          /* inferred */
    /* 0x03C */ char pad3C[8];                      /* maybe part of unk38[3]? */
    /* 0x044 */ s32 unk44;                          /* inferred */
    /* 0x048 */ s32 unk48;                          /* inferred */
    /* 0x04C */ s32 unk4C;                          /* inferred */
    /* 0x050 */ s32 unk50;                          /* inferred */
    /* 0x054 */ s32 unk54;                          /* inferred */
    /* 0x058 */ s32 unk58;                          /* inferred */
    /* 0x05C */ char pad5C[0x2C];                   /* maybe part of unk58[0xC]? */
    /* 0x088 */ s32 unk88;                          /* inferred */
    /* 0x08C */ s32 unk8C;                          /* inferred */
    /* 0x090 */ s32 unk90;                          /* inferred */
    /* 0x094 */ f32 unk94;                          /* inferred */
    /* 0x098 */ f32 unk98;                          /* inferred */
    /* 0x09C */ f32 unk9C;                          /* inferred */
    /* 0x0A0 */ f32 unkA0;                          /* inferred */
    /* 0x0A4 */ s32 unkA4;                          /* inferred */
    /* 0x0A8 */ u8 unkA8;                           /* inferred */
    /* 0x0A9 */ u8 unkA9;                           /* inferred */
    /* 0x0AA */ char padAA[2];                      /* maybe part of unkA9[3]? */
    /* 0x0AC */ f32 unkAC;                          /* inferred */
    /* 0x0B0 */ s32 unkB0;                          /* inferred */
    /* 0x0B4 */ u8 unkB4;                           /* inferred */
    /* 0x0B5 */ char padB5[3];                      /* maybe part of unkB4[4]? */
    /* 0x0B8 */ f32 unkB8;                          /* inferred */
    /* 0x0BC */ char padBC[8];                      /* maybe part of unkB8[3]? */
    /* 0x0C4 */ stlpmtx_std::vector<Stats::StreakInfo, short unsigned, stlpmtx_std::StlNodeAlloc<Stats::StreakInfo>> unkC4; /* inferred */
    /* 0x0C4 */ char padC4[0x28];
    /* 0x0EC */ s32 unkEC;                          /* inferred */
    /* 0x0F0 */ char padF0[0x70];                   /* maybe part of unkEC[0x1D]? */
    /* 0x160 */ s32 unk160;                         /* inferred */
    /* 0x164 */ s32 unk164;                         /* inferred */
    /* 0x168 */ s32 unk168;                         /* inferred */
    /* 0x16C */ s32 unk16C;                         /* inferred */
    /* 0x170 */ char pad170[0x24];                  /* maybe part of unk16C[0xA]? */
    /* 0x194 */ f32 unk194;                         /* inferred */
    /* 0x198 */ char pad198[0x20];                  /* maybe part of unk194[9]? */
    /* 0x1B8 */ stlpmtx_std::vector<Stats::SectionInfo, short unsigned, stlpmtx_std::StlNodeAlloc<Stats::SectionInfo>> unk1B8; /* inferred */
    /* 0x1B8 */ char pad1B8[8];
    /* 0x1C0 */ f32 unk1C0;                         /* inferred */
    /* 0x1C4 */ f32 unk1C4;                         /* inferred */
    /* 0x1C8 */ f32 unk1C8;                         /* inferred */
} Stats;                                            /* size >= 0x1CC */

? WriteEndian__9BinStreamFPCvi(BinStream *this, void *arg0, s32 arg1); /* extern */
? Write__9BinStreamFPCvi(BinStream *this, void *arg0, s32 arg1); /* extern */
? SaveSingerStats__5StatsCFR9BinStream(Stats *this, BinStream *arg0); /* static */
BinStream *__ls<Q25Stats10StreakInfo,Us>__FR9BinStreamRCQ211stlpmtx_std83vector<Q25Stats10StreakInfo,Us,Q211stlpmtx_std34StlNodeAlloc<Q25Stats10StreakInfo>>_R9BinStream(BinStream *arg0, stlpmtx_std::vector<Stats::StreakInfo, short unsigned, stlpmtx_std::StlNodeAlloc<Stats::StreakInfo>> *arg1); /* static */
BinStream *__ls<Q25Stats11SectionInfo,Us>__FR9BinStreamRCQ211stlpmtx_std85vector<Q25Stats11SectionInfo,Us,Q211stlpmtx_std35StlNodeAlloc<Q25Stats11SectionInfo>>_R9BinStream(BinStream *arg0, stlpmtx_std::vector<Stats::SectionInfo, short unsigned, stlpmtx_std::StlNodeAlloc<Stats::SectionInfo>> *arg1); /* static */

/* Stats::SaveForEndGame (BinStream &) const */
void SaveForEndGame__5StatsCFR9BinStream(Stats *this, BinStream *arg0) {
    s32 sp94;
    s32 sp90;
    s32 sp8C;
    f32 sp88;
    f32 sp84;
    s32 sp80;
    s32 sp7C;
    s32 sp78;
    s32 sp74;
    s32 sp70;
    s32 sp6C;
    s32 sp68;
    s32 sp64;
    s32 sp60;
    s32 sp5C;
    s32 sp58;
    s32 sp54;
    s32 sp50;
    s32 sp4C;
    f32 sp48;
    s32 sp44;
    f32 sp40;
    f32 sp3C;
    f32 sp38;
    s32 sp34;
    s32 sp30;
    s32 sp2C;
    s32 sp28;
    s32 sp24;
    f32 sp20;
    s32 sp1C;
    f32 sp18;
    f32 sp14;
    f32 sp10;
    f32 spC;
    u8 spB;
    u8 spA;
    u8 sp9;
    u8 sp8;

    sp94 = this->unk0;
    WriteEndian__9BinStreamFPCvi(arg0, &sp94, 4);
    __ls<Q25Stats10StreakInfo,Us>__FR9BinStreamRCQ211stlpmtx_std83vector<Q25Stats10StreakInfo,Us,Q211stlpmtx_std34StlNodeAlloc<Q25Stats10StreakInfo>>_R9BinStream(arg0, &this->unkC4);
    sp90 = this->unk10;
    WriteEndian__9BinStreamFPCvi(arg0, &sp90, 4);
    sp8C = this->unk14;
    WriteEndian__9BinStreamFPCvi(arg0, &sp8C, 4);
    sp88 = this->unk18;
    WriteEndian__9BinStreamFPCvi(arg0, &sp88, 4);
    sp84 = this->unk194;
    WriteEndian__9BinStreamFPCvi(arg0, &sp84, 4);
    spB = this->unk1C;
    Write__9BinStreamFPCvi(arg0, &spB, 1);
    sp80 = this->unkEC;
    WriteEndian__9BinStreamFPCvi(arg0, &sp80, 4);
    sp7C = this->unk24;
    WriteEndian__9BinStreamFPCvi(arg0, &sp7C, 4);
    sp78 = this->unk2C;
    WriteEndian__9BinStreamFPCvi(arg0, &sp78, 4);
    sp74 = this->unk28;
    WriteEndian__9BinStreamFPCvi(arg0, &sp74, 4);
    sp70 = this->unk20;
    WriteEndian__9BinStreamFPCvi(arg0, &sp70, 4);
    sp6C = this->unk38;
    WriteEndian__9BinStreamFPCvi(arg0, &sp6C, 4);
    sp68 = this->unk4C;
    WriteEndian__9BinStreamFPCvi(arg0, &sp68, 4);
    sp64 = this->unk50;
    WriteEndian__9BinStreamFPCvi(arg0, &sp64, 4);
    sp60 = this->unk54;
    WriteEndian__9BinStreamFPCvi(arg0, &sp60, 4);
    sp5C = this->unk58;
    WriteEndian__9BinStreamFPCvi(arg0, &sp5C, 4);
    sp58 = this->unk44;
    WriteEndian__9BinStreamFPCvi(arg0, &sp58, 4);
    sp54 = this->unk48;
    WriteEndian__9BinStreamFPCvi(arg0, &sp54, 4);
    sp50 = this->unk88;
    WriteEndian__9BinStreamFPCvi(arg0, &sp50, 4);
    sp4C = this->unk8C;
    WriteEndian__9BinStreamFPCvi(arg0, &sp4C, 4);
    sp48 = this->unk94;
    WriteEndian__9BinStreamFPCvi(arg0, &sp48, 4);
    sp44 = this->unk90;
    WriteEndian__9BinStreamFPCvi(arg0, &sp44, 4);
    sp40 = this->unk98;
    WriteEndian__9BinStreamFPCvi(arg0, &sp40, 4);
    sp3C = this->unk9C;
    WriteEndian__9BinStreamFPCvi(arg0, &sp3C, 4);
    sp38 = this->unkA0;
    WriteEndian__9BinStreamFPCvi(arg0, &sp38, 4);
    sp34 = this->unkA4;
    WriteEndian__9BinStreamFPCvi(arg0, &sp34, 4);
    spA = this->unkA8;
    Write__9BinStreamFPCvi(arg0, &spA, 1);
    sp9 = this->unkA9;
    Write__9BinStreamFPCvi(arg0, &sp9, 1);
    sp30 = this->unk160;
    WriteEndian__9BinStreamFPCvi(arg0, &sp30, 4);
    sp2C = this->unk164;
    WriteEndian__9BinStreamFPCvi(arg0, &sp2C, 4);
    sp28 = this->unk168;
    WriteEndian__9BinStreamFPCvi(arg0, &sp28, 4);
    sp24 = this->unk16C;
    WriteEndian__9BinStreamFPCvi(arg0, &sp24, 4);
    sp20 = this->unkAC;
    WriteEndian__9BinStreamFPCvi(arg0, &sp20, 4);
    sp1C = this->unkB0;
    WriteEndian__9BinStreamFPCvi(arg0, &sp1C, 4);
    sp8 = this->unkB4;
    Write__9BinStreamFPCvi(arg0, &sp8, 1);
    sp18 = this->unkB8;
    WriteEndian__9BinStreamFPCvi(arg0, &sp18, 4);
    __ls<Q25Stats11SectionInfo,Us>__FR9BinStreamRCQ211stlpmtx_std85vector<Q25Stats11SectionInfo,Us,Q211stlpmtx_std35StlNodeAlloc<Q25Stats11SectionInfo>>_R9BinStream(arg0, &this->unk1B8);
    sp14 = this->unk1C0;
    WriteEndian__9BinStreamFPCvi(arg0, &sp14, 4);
    sp10 = this->unk1C4;
    WriteEndian__9BinStreamFPCvi(arg0, &sp10, 4);
    spC = this->unk1C8;
    WriteEndian__9BinStreamFPCvi(arg0, &spC, 4);
    SaveSingerStats__5StatsCFR9BinStream(this, arg0);
}