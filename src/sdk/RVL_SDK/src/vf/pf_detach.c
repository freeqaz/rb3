#include "types.h"

extern s32 VFiPFVOL_detach(s8 drv);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_detach(s8 drv) {
    return VFiPFAPI_convertReturnValue(VFiPFVOL_detach(drv));
}
