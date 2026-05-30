#include "types.h"

s32 VFiPFCACHE_InitPageList(void *cache, void *buf, u32 size) {
    (void)cache; (void)buf; (void)size;
    return 0;
}

s32 VFiPFCACHE_FlushPageIfNeeded(void *cache, void *page) {
    (void)cache; (void)page;
    return 0;
}

s32 VFiPFCACHE_DoAllocatePage(void *cache, u32 sector, void **page) {
    (void)cache; (void)sector; (void)page;
    return 0;
}

s32 VFiPFCACHE_DoReadPage(void *cache, u32 sector, void **page) {
    (void)cache; (void)sector; (void)page;
    return 0;
}

s32 VFiPFCACHE_DoReadPageAndFlushIfNeeded(void *cache, u32 sector, void **page) {
    (void)cache; (void)sector; (void)page;
    return 0;
}

s32 VFiPFCACHE_DoReadNumSector(void *cache, u32 sector, u32 count, void *buf) {
    (void)cache; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 VFiPFCACHE_DoWriteNumSectorAndFreeIfNeeded(void *cache, u32 sector, u32 count, const void *buf) {
    (void)cache; (void)sector; (void)count; (void)buf;
    return 0;
}

void VFiPFCACHE_SetCache(void *vol, void *cache) {
    (void)vol; (void)cache;
}

void VFiPFCACHE_SetFATBufferSize(void *cache, u32 size) {
    (void)cache; (void)size;
}

void VFiPFCACHE_SetDataBufferSize(void *cache, u32 size) {
    (void)cache; (void)size;
}

s32 VFiPFCACHE_InitCaches(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFCACHE_UpdateModifiedSector(void *cache, u32 sector) {
    (void)cache; (void)sector;
    return 0;
}

s32 VFiPFCACHE_AllocateDataPage(void *vol, u32 sector, void **page) {
    (void)vol; (void)sector; (void)page;
    return 0;
}

s32 VFiPFCACHE_FreeDataPage(void *vol, void *page) {
    (void)vol; (void)page;
    return 0;
}

s32 VFiPFCACHE_ReadFATPage(void *vol, u32 sector, void **page) {
    (void)vol; (void)sector; (void)page;
    return 0;
}

s32 VFiPFCACHE_ReadDataPage(void *vol, u32 sector, void **page) {
    (void)vol; (void)sector; (void)page;
    return 0;
}

s32 VFiPFCACHE_ReadDataPageAndFlushIfNeeded(void *vol, u32 sector, void **page) {
    (void)vol; (void)sector; (void)page;
    return 0;
}

s32 VFiPFCACHE_ReadDataNumSector(void *vol, u32 sector, u32 count, void *buf) {
    (void)vol; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 VFiPFCACHE_WriteFATPage(void *vol, u32 sector, const void *buf) {
    (void)vol; (void)sector; (void)buf;
    return 0;
}

s32 VFiPFCACHE_WriteDataPage(void *vol, u32 sector, const void *buf) {
    (void)vol; (void)sector; (void)buf;
    return 0;
}

s32 VFiPFCACHE_WriteDataNumSectorAndFreeIfNeeded(void *vol, u32 sector, u32 count, const void *buf) {
    (void)vol; (void)sector; (void)count; (void)buf;
    return 0;
}

void *VFiPFCACHE_SearchDataCache(void *vol, u32 sector) {
    (void)vol; (void)sector;
    return 0;
}

s32 VFiPFCACHE_FlushFATCache(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFCACHE_FlushDataCacheSpecific(void *vol, u32 sector) {
    (void)vol; (void)sector;
    return 0;
}

s32 VFiPFCACHE_FlushAllCaches(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFCACHE_FreeAllCaches(void *vol) {
    (void)vol;
    return 0;
}
