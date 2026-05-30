#include "types.h"

extern s32 VFiPFFILE_fread(s32 buf, s32 size, s32 count, void *fp);
extern u32 VFiPFAPI_convertReturnValue2NULL(s32 ret);

u32 VFipf2_fread(s32 buf, s32 size, s32 count, void *fp) {
    return VFiPFAPI_convertReturnValue2NULL(VFiPFFILE_fread(buf, size, count, fp));
}
