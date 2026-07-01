typedef struct ProfileMgr {
    /* 0x000 */ char pad0[0x554];
    /* 0x554 */ s32 unk554;                         /* inferred */
    /* 0x558 */ u8 unk558;                          /* inferred */
} ProfileMgr;                                       /* size >= 0x559 */

/* ProfileMgr::GlobalOptionsNeedsSave (void) */
u8 GlobalOptionsNeedsSave__10ProfileMgrFv(ProfileMgr *this) {
    if ((s32) this->unk554 != 1) {
        return 0U;
    }
    return this->unk558;
}