typedef struct Lyric {
    /* 0x00 */ char pad0[0x30];
    /* 0x30 */ void **unk30;                        /* inferred */
} Lyric;                                            /* size >= 0x34 */

/* Lyric::PitchNote (void) const */
s32 PitchNote__5LyricCFv(Lyric *this) {
    return (*this->unk30)->unk2A == 0;
}