#include "types.h"

extern s32 VFiPFDIR_fsfirst(const char *path, const char *pattern, void *finddata);
extern void *VFiPFVOL_GetVolumeFromDrvChar(s8 drv);
extern void VFiPFVOL_SetCurrentVolume(void);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_fsfirst(const char *path, const char *pattern, void *finddata) {
    void *vol = VFiPFVOL_GetVolumeFromDrvChar((s8)path[0]);
    if (vol != NULL) {
        VFiPFVOL_SetCurrentVolume();
    }
    return VFiPFAPI_convertReturnValue(VFiPFDIR_fsfirst(path, pattern, finddata));
}
