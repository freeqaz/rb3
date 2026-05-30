#include "types.h"

s32 VFipf_w_strlen(const char *s) {
    const char *p = s;
    while (*p != '\0') {
        p++;
    }
    return (s32)(p - s);
}

char *VFipf_w_strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++) != '\0') {}
    return ret;
}

s32 VFipf_w_strncmp(const char *s1, const char *s2, s32 n) {
    while (n > 0) {
        u8 c1 = (u8)*s1;
        u8 c2 = (u8)*s2;
        if (c1 != c2) {
            return (s32)c1 - (s32)c2;
        }
        if (c1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
        n--;
    }
    return 0;
}
