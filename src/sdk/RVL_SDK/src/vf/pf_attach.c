#include "types.h"

extern s32 VFiPFVOL_attach(s8 drv, void *devInit, void *param, s32 numBuffers);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_attach(s8 drv, void *devInit, void *param, s32 numBuffers) {
    return VFiPFAPI_convertReturnValue(VFiPFVOL_attach(drv, devInit, param, numBuffers));
}
