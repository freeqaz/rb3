#include "types.h"

extern s32 VFiPFDIR_fsnext(void *finddata);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_fsnext(void *finddata) {
    return VFiPFAPI_convertReturnValue(VFiPFDIR_fsnext(finddata));
}
