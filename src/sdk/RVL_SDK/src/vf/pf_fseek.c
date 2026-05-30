#include "types.h"

extern s32 VFiPFFILE_fseek(void *fp, s32 offset, s32 whence);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_fseek(void *fp, s32 offset, s32 whence) {
    return VFiPFAPI_convertReturnValue(VFiPFFILE_fseek(fp, offset, whence));
}
