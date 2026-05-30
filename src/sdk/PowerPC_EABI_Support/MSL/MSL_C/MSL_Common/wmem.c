#include "wmem.h"
#include <string.h>

wchar_t *wmemcpy(wchar_t *RESTRICT dest, const wchar_t *RESTRICT src, size_t count) {
    return (wchar_t *)memcpy(dest, src, count * sizeof(wchar_t));
}

wchar_t *wmemset(wchar_t *dest, wchar_t ch, size_t count) {
    wchar_t *p = dest - 1;
    count++;
    while (--count) *(++p) = ch;
    return dest;
}

wchar_t *wmemchr(const wchar_t *ptr, wchar_t ch, size_t count) {
    const wchar_t *m2 = ptr - 1;
    count++;
    while (--count) if (*(++m2) == ch) return (wchar_t *)m2;
    return NULL;
}
