#include "types.h"

s32 VFiPFENT_ITER_RecalcEntryIterator(void *vol, void *iter) {
    (void)vol; (void)iter;
    return 0;
}

s32 VFiPFENT_ITER_GetEntry(void *vol, void *iter, void *ent) {
    (void)vol; (void)iter; (void)ent;
    return 0;
}

s32 VFiPFENT_ITER_DoFindEntry(void *vol, void *iter, const void *name) {
    (void)vol; (void)iter; (void)name;
    return 0;
}

s32 VFiPFENT_ITER_DoAllocateEntry(void *vol, void *iter, u32 count) {
    (void)vol; (void)iter; (void)count;
    return 0;
}

s32 VFiPFENT_ITER_DoGetEntryOfPath(void *vol, void *iter, const void *path) {
    (void)vol; (void)iter; (void)path;
    return 0;
}

s32 VFiPFENT_ITER_IteratorInitialize(void *vol, void *iter, u32 cluster) {
    (void)vol; (void)iter; (void)cluster;
    return 0;
}

s32 VFiPFENT_ITER_IsAtLogicalEnd(void *vol, void *iter) {
    (void)vol; (void)iter;
    return 0;
}

s32 VFiPFENT_ITER_Advance(void *vol, void *iter) {
    (void)vol; (void)iter;
    return 0;
}

s32 VFiPFENT_ITER_Retreat(void *vol, void *iter) {
    (void)vol; (void)iter;
    return 0;
}

s32 VFiPFENT_ITER_FindEntry(void *vol, void *iter, const void *name) {
    (void)vol; (void)iter; (void)name;
    return 0;
}

s32 VFiPFENT_ITER_AllocateEntry(void *vol, void *iter, u32 count) {
    (void)vol; (void)iter; (void)count;
    return 0;
}

s32 VFiPFENT_ITER_GetEntryOfPath(void *vol, const void *path, void *ent) {
    (void)vol; (void)path; (void)ent;
    return 0;
}

s32 VFiPFENT_ITER_GetEntryOfPattern(void *vol, const void *pattern, void *ent) {
    (void)vol; (void)pattern; (void)ent;
    return 0;
}

s32 VFiPFENT_ITER_FindCluster(void *vol, void *iter, u32 cluster) {
    (void)vol; (void)iter; (void)cluster;
    return 0;
}
