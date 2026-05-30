#include "types.h"

extern s32 VFiPFVOL_unmount(s8 drv, s32 force);
extern s32 VFiPFAPI_convertReturnValue4unmount(s32 ret);

s32 VFipf2_unmount(s8 drv, s32 force) {
    return VFiPFAPI_convertReturnValue4unmount(VFiPFVOL_unmount(drv, force));
}
