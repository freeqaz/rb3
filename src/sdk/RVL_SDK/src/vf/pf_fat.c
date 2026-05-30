#include "types.h"

const u32 fat_special_values[15] = {0};

s32 VFiPFFAT_ReadFATSector(void *vol, u32 sector, void **page) {
    (void)vol; (void)sector; (void)page;
    return 0;
}

s32 VFiPFFAT_SearchForNumFreeClusters(void *vol, u32 start, u32 count, u32 *result) {
    (void)vol; (void)start; (void)count; (void)result;
    return 0;
}

s32 VFiPFFAT_FindClusterLink(void *vol, u32 cluster, u32 *next) {
    (void)vol; (void)cluster; (void)next;
    return 0;
}

s32 VFiPFFAT_FindClusterLinkPage(void *vol, u32 cluster, void **page, u32 *next) {
    (void)vol; (void)cluster; (void)page; (void)next;
    return 0;
}

s32 VFiPFFAT_ReadClusterPage(void *vol, u32 cluster, void **page) {
    (void)vol; (void)cluster; (void)page;
    return 0;
}

s32 VFiPFFAT_WriteCluster(void *vol, u32 cluster, u32 value) {
    (void)vol; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT_WriteClusterPage(void *vol, void *page, u32 cluster, u32 value) {
    (void)vol; (void)page; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT_DoAllocateChain(void *vol, u32 cluster, u32 count, u32 *first) {
    (void)vol; (void)cluster; (void)count; (void)first;
    return 0;
}

s32 VFiPFFAT_GetClusterInChain(void *vol, u32 start, u32 offset, u32 *result) {
    (void)vol; (void)start; (void)offset; (void)result;
    return 0;
}

s32 VFiPFFAT_GetClusterContinuousSectorInChain(void *vol, u32 cluster, u32 *sector, u32 *count) {
    (void)vol; (void)cluster; (void)sector; (void)count;
    return 0;
}

s32 VFiPFFAT_GetClusterAllocatedInChain(void *vol, u32 start, u32 offset, u32 *cluster) {
    (void)vol; (void)start; (void)offset; (void)cluster;
    return 0;
}

s32 VFiPFFAT_GetClusterSpecified(void *vol, u32 cluster, u32 *result) {
    (void)vol; (void)cluster; (void)result;
    return 0;
}

s32 VFiPFFAT_GetClusterAllocated(void *vol, u32 hint, u32 *cluster) {
    (void)vol; (void)hint; (void)cluster;
    return 0;
}

s32 VFiPFFAT_GetSector(void *vol, u32 cluster, u32 offset, u32 *sector) {
    (void)vol; (void)cluster; (void)offset; (void)sector;
    return 0;
}

s32 VFiPFFAT_UpdateFATEntry(void *vol, u32 cluster, u32 value) {
    (void)vol; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT_UpdateAlternateFATEntry(void *vol, u32 cluster, u32 value) {
    (void)vol; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT_GetSectorSpecified(void *vol, u32 cluster, u32 *sector) {
    (void)vol; (void)cluster; (void)sector;
    return 0;
}

s32 VFiPFFAT_GetSectorAllocated(void *vol, u32 hint, u32 *sector) {
    (void)vol; (void)hint; (void)sector;
    return 0;
}

s32 VFiPFFAT_GetContinuousSector(void *vol, u32 cluster, u32 *sector, u32 *count) {
    (void)vol; (void)cluster; (void)sector; (void)count;
    return 0;
}

s32 VFiPFFAT_CountAllocatedClusters(void *vol, u32 start, u32 *count) {
    (void)vol; (void)start; (void)count;
    return 0;
}

s32 VFiPFFAT_CountFreeClusters(void *vol, u32 *count) {
    (void)vol; (void)count;
    return 0;
}

s32 VFiPFFAT_FreeChain(void *vol, u32 cluster, u32 keepCount) {
    (void)vol; (void)cluster; (void)keepCount;
    return 0;
}

s32 VFiPFFAT_GetBeforeChain(void *vol, u32 cluster, u32 offset, u32 *prev) {
    (void)vol; (void)cluster; (void)offset; (void)prev;
    return 0;
}

s32 VFiPFFAT_GetBeforeSector(void *vol, u32 sector, u32 *prev) {
    (void)vol; (void)sector; (void)prev;
    return 0;
}

s32 VFiPFFAT_InitFATRegion(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFFAT_MakeRootDir(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFFAT_RefreshFSINFO(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFFAT_TraceClustersChain(void *vol, u32 cluster, u32 count, u32 *result) {
    (void)vol; (void)cluster; (void)count; (void)result;
    return 0;
}

s32 VFiPFFAT_WriteValueToSpecifiedCluster(void *vol, u32 cluster, u32 value) {
    (void)vol; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT_ReadValueToSpecifiedCluster(void *vol, u32 cluster, u32 *value) {
    (void)vol; (void)cluster; (void)value;
    return 0;
}

void VFiPFFAT_InitHint(void *vol) {
    (void)vol;
}

void VFiPFFAT_SetHint(void *vol, u32 cluster) {
    (void)vol; (void)cluster;
}

s32 VFiPFFAT_ResetFFD(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFFAT_InitFFD(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFFAT_FinalizeFFD(void *vol) {
    (void)vol;
    return 0;
}

void VFiPFFAT_SetLastAccess(void *vol, u32 cluster) {
    (void)vol; (void)cluster;
}

u32 VFiPFFAT_GetValueOfEOC2(void *vol) {
    (void)vol;
    return 0;
}
