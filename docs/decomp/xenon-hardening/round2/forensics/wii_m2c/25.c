typedef struct Singer {
    /* 0x00 */ void *unk0;                          /* inferred */
    /* 0x04 */ char pad4[0x6C];                     /* maybe part of unk0[0x1C]? */
    /* 0x70 */ s32 unk70;                           /* inferred */
} Singer;                                           /* size >= 0x74 */

/* Singer::GetFrameMatchType (void) */
s32 GetFrameMatchType__6SingerFv(Singer *this) {
    s32 temp_r0;

    temp_r0 = this->unk70;
    if (temp_r0 != -1) {
        return (*(this->unk0->unk358 + (temp_r0 * 4)))->unk98;
    }
    return 4;
}