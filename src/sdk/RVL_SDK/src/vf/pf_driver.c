#include "types.h"

s32 VFiPFDRV_GetBPBInformation(void *drv, void *bpb) {
    (void)drv; (void)bpb;
    return 0;
}

s32 VFiPFDRV_GetFSINFOInformation(void *drv, void *fsinfo) {
    (void)drv; (void)fsinfo;
    return 0;
}

s32 VFiPFDRV_StoreFreeCountToFSINFO(void *drv, u32 freeCount) {
    (void)drv; (void)freeCount;
    return 0;
}

s32 VFiPFDRV_IsInserted(void *drv) {
    (void)drv;
    return 0;
}

s32 VFiPFDRV_IsDetected(void *drv) {
    (void)drv;
    return 0;
}

s32 VFiPFDRV_IsWProtected(void *drv) {
    (void)drv;
    return 0;
}

s32 VFiPFDRV_init(void *drv) {
    (void)drv;
    return 0;
}

s32 VFiPFDRV_finalize(void *drv) {
    (void)drv;
    return 0;
}

s32 VFiPFDRV_mount(void *drv, void *vol) {
    (void)drv; (void)vol;
    return 0;
}

s32 VFiPFDRV_unmount(void *drv) {
    (void)drv;
    return 0;
}

s32 VFiPFDRV_format(void *drv, void *param) {
    (void)drv; (void)param;
    return 0;
}

s32 VFiPFDRV_lread(void *drv, u32 sector, u32 count, void *buf) {
    (void)drv; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 VFiPFDRV_lwrite(void *drv, u32 sector, u32 count, const void *buf) {
    (void)drv; (void)sector; (void)count; (void)buf;
    return 0;
}

s32 VFiPFDRV_lerase(void *drv, u32 sector, u32 count) {
    (void)drv; (void)sector; (void)count;
    return 0;
}
