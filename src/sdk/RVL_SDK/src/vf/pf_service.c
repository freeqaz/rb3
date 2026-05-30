#include "types.h"

s32 VFiPF_LE16_TO_U16_STR(const u8 *src, u16 *dst, s32 len) {
    s32 i;
    for (i = 0; i < len; i++) {
        dst[i] = (u16)(src[i * 2] | ((u16)src[i * 2 + 1] << 8));
        if (dst[i] == 0) break;
    }
    return i;
}
