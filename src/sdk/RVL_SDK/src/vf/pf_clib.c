#include "types.h"

s32 VFipf_toupper(s32 c) {
    if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
    return c;
}

void *VFipf_memcpy(void *dst, const void *src, s32 n) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n-- > 0) *d++ = *s++;
    return dst;
}

void *VFipf_memset(void *dst, s32 val, s32 n) {
    u8 *d = (u8 *)dst;
    while (n-- > 0) *d++ = (u8)val;
    return dst;
}

s32 VFipf_strlen(const char *s) {
    const char *p = s;
    while (*p != '\0') p++;
    return (s32)(p - s);
}

char *VFipf_strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++) != '\0') {}
    return ret;
}

s32 VFipf_strcmp(const char *s1, const char *s2) {
    unsigned char c1, c2;
    do {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;
        if (c1 == '\0') return (s32)c1 - (s32)c2;
    } while (c1 == c2);
    return (s32)c1 - (s32)c2;
}

s32 VFipf_strncmp(const char *s1, const char *s2, s32 n) {
    unsigned char c1, c2;
    while (n-- > 0) {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;
        if (c1 != c2) return (s32)c1 - (s32)c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}
