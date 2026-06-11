typedef struct TambourineManager {
    /* 0x00 */ char pad0[0x1C];
    /* 0x1C */ void *unk1C;                         /* inferred */
} TambourineManager;                                /* size >= 0x20 */

/* TambourineManager::TambourineGems (void) const */
s32 TambourineGems__17TambourineManagerCFv(TambourineManager *this) {
    return (*this->unk1C->unk358)->unk8 + 0x18;
}