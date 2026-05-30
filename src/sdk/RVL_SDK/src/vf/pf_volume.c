#include "types.h"

u8 VFipf_vol_set[0x27D48];

s32 VFiPFVOL_DoMountVolume(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFVOL_p_unmount(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFVOL_p_format(void *vol, void *param) {
    (void)vol; (void)param;
    return 0;
}

s32 VFiPFVOL_InitModule(void) {
    return 0;
}

s32 VFiPFVOL_CheckForRead(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFVOL_CheckForWrite(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFVOL_SetCurrentDir(void *vol, const void *path) {
    (void)vol; (void)path;
    return 0;
}

s32 VFiPFVOL_GetCurrentDir(void *vol, void *path) {
    (void)vol; (void)path;
    return 0;
}

s32 VFiPFVOL_SetCurrentVolume(s32 drive) {
    (void)drive;
    return 0;
}

s32 VFiPFVOL_GetCurrentVolume(s32 *drive) {
    (void)drive;
    return 0;
}

void *VFiPFVOL_GetVolumeFromDrvChar(s32 drive) {
    (void)drive;
    return 0;
}

s32 VFiPFVOL_LoadVolumeLabelFromBuf(void *vol, const void *buf) {
    (void)vol; (void)buf;
    return 0;
}

s32 VFiPFVOL_errnum(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFVOL_getdev(void *vol, void *info) {
    (void)vol; (void)info;
    return 0;
}

s32 VFiPFVOL_attach(void *vol, void *drv) {
    (void)vol; (void)drv;
    return 0;
}

s32 VFiPFVOL_detach(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFVOL_format(void *vol, void *param) {
    (void)vol; (void)param;
    return 0;
}

s32 VFiPFVOL_unmount(void *vol) {
    (void)vol;
    return 0;
}
