#include "types.h"

s32 VFiPFSEC_ReadFAT(void *vol, u32 sector, void **page) {
    (void)vol; (void)sector; (void)page;
    return 0;
}

s32 VFiPFSEC_WriteFAT(void *vol, u32 sector, const void *buf) {
    (void)vol; (void)sector; (void)buf;
    return 0;
}

s32 VFiPFSEC_ReadData(void *vol, u32 sector, void **page) {
    (void)vol; (void)sector; (void)page;
    return 0;
}

s32 VFiPFSEC_ReadDataSector(void *vol, u32 sector, u32 count, void *buf) {
    (void)vol; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 VFiPFSEC_WriteData(void *vol, u32 sector, const void *buf) {
    (void)vol; (void)sector; (void)buf;
    return 0;
}

s32 VFiPFSEC_WriteDataSector(void *vol, u32 sector, u32 count, const void *buf) {
    (void)vol; (void)sector; (void)count; (void)buf;
    return 0;
}
