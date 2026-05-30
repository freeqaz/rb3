#include "types.h"

extern s32 VFiPFFILE_fwrite(const void *buf, s32 size, s32 count, void *fp);
extern u32 VFiPFAPI_convertReturnValue2NULL(s32 ret);

u32 VFipf2_fwrite(const void *buf, s32 size, s32 count, void *fp) {
    return VFiPFAPI_convertReturnValue2NULL(VFiPFFILE_fwrite(buf, size, count, fp));
}
