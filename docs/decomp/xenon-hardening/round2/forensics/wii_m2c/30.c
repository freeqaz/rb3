typedef struct SingerStats {
    /* 0x0 */ s32 unk0;                             /* inferred */
} SingerStats;                                      /* size >= 0x4 */

/* SingerStats::GetRankData (int) const */
s32 GetRankData__11SingerStatsCFi(SingerStats *this, s32 arg0) {
    return this->unk0 + (arg0 * 8);
}