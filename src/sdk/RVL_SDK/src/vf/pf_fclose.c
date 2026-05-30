#include "types.h"

extern s32 VFiPFFILE_fclose(void *fp);
extern s32 VFiPFAPI_convertReturnValue(s32 ret);

s32 VFipf2_fclose(void *fp) {
    return VFiPFAPI_convertReturnValue(VFiPFFILE_fclose(fp));
}
