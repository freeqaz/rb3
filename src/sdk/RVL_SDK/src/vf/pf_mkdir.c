#include "types.h"

extern s32 VFiPFDIR_mkdir(const char *path);
extern void *VFiPFVOL_GetVolumeFromDrvChar(s8 drv);
extern void VFiPFVOL_SetCurrentVolume(void);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_mkdir(const char *path) {
    void *vol = VFiPFVOL_GetVolumeFromDrvChar((s8)path[0]);
    if (vol != NULL) {
        VFiPFVOL_SetCurrentVolume();
    }
    return VFiPFAPI_convertReturnValue(VFiPFDIR_mkdir(path));
}
