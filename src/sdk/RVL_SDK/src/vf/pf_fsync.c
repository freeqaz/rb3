#include "types.h"

extern s32 VFiPFFILE_fsync(void *fp);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_fsync(void *fp) {
    return VFiPFAPI_convertReturnValue(VFiPFFILE_fsync(fp));
}
