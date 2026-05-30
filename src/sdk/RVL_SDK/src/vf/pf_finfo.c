#include "types.h"

extern s32 VFiPFFILE_finfo(void *fp, s32 *size);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_finfo(void *fp, s32 *size) {
    return VFiPFAPI_convertReturnValue(VFiPFFILE_finfo(fp, size));
}
