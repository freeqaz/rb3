#include "types.h"

extern s32 VFiPFVOL_errnum(void);
extern s32 VFiPFAPI_convertError(s32 err);

s32 VFipf2_errnum(void) {
    return VFiPFAPI_convertError(VFiPFVOL_errnum());
}
