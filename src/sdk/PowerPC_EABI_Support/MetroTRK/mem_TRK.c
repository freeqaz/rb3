#include "types.h"

void *TRK_memcpy(void *dst, const void *src, s32 n) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    s32 i;

    if (n <= 0)
        return dst;

    if ((((u32)d | (u32)s) & 3) == 0) {
        s32 words = n >> 2;
        s32 rem = n & 3;
        u32 *dw = (u32 *)d;
        const u32 *sw = (const u32 *)s;
        for (i = 0; i < words; i++) {
            *dw++ = *sw++;
        }
        d = (u8 *)dw;
        s = (const u8 *)sw;
        for (i = 0; i < rem; i++) {
            *d++ = *s++;
        }
    } else {
        for (i = 0; i < n; i++) {
            *d++ = *s++;
        }
    }

    return dst;
}

void *TRK_memset(void *dst, s32 val, s32 n) {
    u8 *d = (u8 *)dst;
    u8 v = (u8)val;
    s32 i;

    if (n <= 0)
        return dst;

    if (((u32)d & 3) == 0) {
        u32 vw = ((u32)v << 24) | ((u32)v << 16) | ((u32)v << 8) | (u32)v;
        s32 words = n >> 2;
        s32 rem = n & 3;
        u32 *dw = (u32 *)d;
        for (i = 0; i < words; i++) {
            *dw++ = vw;
        }
        d = (u8 *)dw;
        for (i = 0; i < rem; i++) {
            *d++ = v;
        }
    } else {
        for (i = 0; i < n; i++) {
            *d++ = v;
        }
    }

    return dst;
}
