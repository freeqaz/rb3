#include "types.h"

extern s32 VFiPFVOL_format(s8 drv, s32 arg1);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_format(s8 drv, s32 arg1) {
    return VFiPFAPI_convertReturnValue(VFiPFVOL_format(drv, arg1));
}
