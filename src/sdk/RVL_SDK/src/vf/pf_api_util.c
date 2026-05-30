#include "types.h"

/* Error code conversion table: PrFILE2 error → API error */
s32 VFipf_error_to_api_error[40];

s32 VFiPFAPI_ParseOpenModeString(const char *mode) {
    s32 flags = 0;
    if (!mode) return -1;
    while (*mode) {
        switch (*mode) {
        case 'r': flags |= 0x01; break;
        case 'w': flags |= 0x02; break;
        case 'a': flags |= 0x04; break;
        case 'b': flags |= 0x08; break;
        case '+': flags |= 0x10; break;
        default: break;
        }
        mode++;
    }
    return flags;
}

s32 VFiPFAPI_convertError(s32 err) {
    if (err >= 0 && err < 40) {
        return VFipf_error_to_api_error[err];
    }
    return -1;
}

s32 VFiPFAPI_convertReturnValue(s32 ret) {
    if (ret >= 0) return ret;
    return VFiPFAPI_convertError(-ret);
}

s32 VFiPFAPI_convertReturnValue2NULL(s32 ret) {
    if (ret >= 0) return 0;
    return VFiPFAPI_convertError(-ret);
}

s32 VFiPFAPI_convertReturnValue4unmount(s32 ret) {
    (void)ret;
    return 0;
}
