#include "MSL_Common/mem_funcs.h"

void __copy_longs_aligned(void *dst, const void *src, size_t len) {
    unsigned long *d = (unsigned long *)dst;
    const unsigned long *s = (const unsigned long *)src;
    size_t longs = len >> 2;
    size_t i;
    for (i = 0; i < longs; i++) {
        *d++ = *s++;
    }
    if (len & 2) {
        unsigned short *ds = (unsigned short *)d;
        const unsigned short *ss = (const unsigned short *)s;
        *ds++ = *ss++;
        d = (unsigned long *)ds;
        s = (const unsigned long *)ss;
    }
    if (len & 1) {
        *(unsigned char *)d = *(const unsigned char *)s;
    }
}

void __copy_longs_rev_aligned(void *dst, const void *src, size_t len) {
    const unsigned char *s = (const unsigned char *)src + len;
    unsigned char *d = (unsigned char *)dst + len;
    size_t longs = len >> 2;
    size_t i;
    s -= 4; d -= 4;
    for (i = 0; i < longs; i++) {
        *(unsigned long *)d = *(const unsigned long *)s;
        d -= 4; s -= 4;
    }
    d += 4; s += 4;
    if (len & 2) {
        d -= 2; s -= 2;
        *(unsigned short *)d = *(const unsigned short *)s;
    }
    if (len & 1) {
        d--; s--;
        *d = *s;
    }
}

void __copy_longs_unaligned(void *dst, const void *src, size_t len) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    size_t i;
    for (i = 0; i < len; i++) {
        *d++ = *s++;
    }
}

void __copy_longs_rev_unaligned(void *dst, const void *src, size_t len) {
    unsigned char *d = (unsigned char *)dst + len;
    const unsigned char *s = (const unsigned char *)src + len;
    size_t i;
    for (i = 0; i < len; i++) {
        *--d = *--s;
    }
}
