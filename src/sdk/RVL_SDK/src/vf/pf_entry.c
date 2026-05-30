#include "types.h"

s32 VFiPFENT_searchEmptyTailSFN(void *vol, void *iter, u32 count) {
    (void)vol; (void)iter; (void)count;
    return 0;
}

s32 VFiPFENT_findEmptyTailSFN(void *vol, void *sfn, void *iter) {
    (void)vol; (void)sfn; (void)iter;
    return 0;
}

u8 VFiPFENT_CalcCheckSum(const u8 *sfn) {
    (void)sfn;
    return 0;
}

void VFiPFENT_LoadShortNameFromBuf(void *sfn, const void *buf) {
    (void)sfn; (void)buf;
}

s32 VFiPFENT_loadEntryNumericFieldsFromBuf(void *ent, const void *buf) {
    (void)ent; (void)buf;
    return 0;
}

s32 VFiPFENT_LoadLFNEntryFieldsFromBuf(void *lfn, const void *buf, s32 idx) {
    (void)lfn; (void)buf; (void)idx;
    return 0;
}

s32 VFiPFENT_storeLFNEntryFieldsToBuf(void *buf, const void *lfn, s32 idx) {
    (void)buf; (void)lfn; (void)idx;
    return 0;
}

s32 VFiPFENT_GetEntryOfPath(void *vol, const void *path, void *ent) {
    (void)vol; (void)path; (void)ent;
    return 0;
}

s32 VFiPFENT_GetParentEntryOfPath(void *vol, const void *path, void *ent) {
    (void)vol; (void)path; (void)ent;
    return 0;
}

s32 VFiPFENT_findEntry(void *vol, const void *name, void *ent, void *iter) {
    (void)vol; (void)name; (void)ent; (void)iter;
    return 0;
}

s32 VFiPFENT_allocateEntry(void *vol, void *ent, u32 count) {
    (void)vol; (void)ent; (void)count;
    return 0;
}

s32 VFiPFENT_GetRootDir(void *vol, void *ent) {
    (void)vol; (void)ent;
    return 0;
}

s32 VFiPFENT_MakeRootDir(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFENT_CompareAttr(u8 attr, u8 mask) {
    (void)attr; (void)mask;
    return 0;
}

s32 VFiPFENT_compareEntryName(void *vol, const void *ent, const void *name) {
    (void)vol; (void)ent; (void)name;
    return 0;
}

s32 VFiPFENT_UpdateSFNEntry(void *vol, void *ent) {
    (void)vol; (void)ent;
    return 0;
}

s32 VFiPFENT_UpdateEntry(void *vol, void *ent) {
    (void)vol; (void)ent;
    return 0;
}

s32 VFiPFENT_AdjustSFN(void *vol, void *sfn) {
    (void)vol; (void)sfn;
    return 0;
}

s32 VFiPFENT_RemoveEntry(void *vol, void *ent) {
    (void)vol; (void)ent;
    return 0;
}

s32 VFiPFENT_getcurrentDateTimeForEnt(void *ent) {
    (void)ent;
    return 0;
}

s32 VFiPFENT_InitENT(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFENT_FillVoidEntryToSectors(void *vol, u32 sector, u32 count) {
    (void)vol; (void)sector; (void)count;
    return 0;
}
