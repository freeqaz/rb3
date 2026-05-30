#include "types.h"

extern s32 VFiPFVOL_getdev(s8 drv, void *info);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_devinf(s8 drv, void *info) {
    return VFiPFAPI_convertReturnValue(VFiPFVOL_getdev(drv, info));
}
