#include "types.h"

extern s32 VFiPFFILE_fopen(const char *path, const char *mode);
extern void *VFiPFVOL_GetVolumeFromDrvChar(s8 drv);
extern void VFiPFVOL_SetCurrentVolume(void);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_create(const char *path, s32 flags) {
    void *vol = VFiPFVOL_GetVolumeFromDrvChar((s8)path[0]);
    if (vol != NULL) {
        VFiPFVOL_SetCurrentVolume();
    }
    return VFiPFAPI_convertReturnValue(VFiPFFILE_fopen(path, "w+b"));
}
