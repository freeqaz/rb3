#include "types.h"
#include "MSL_Common/wchar_def.h"
#include "MSL_Common/size_def.h"

size_t wcslen(const wchar_t *str) {
    const wchar_t *p = str;
    while (*p != L'\0') {
        p++;
    }
    return (size_t)(p - str);
}

wchar_t *wcscpy(wchar_t *dest, const wchar_t *src) {
    wchar_t *ret = dest;
    while ((*dest++ = *src++) != L'\0') {}
    return ret;
}

wchar_t *wcsncpy(wchar_t *dest, const wchar_t *src, size_t count) {
    wchar_t *ret = dest;
    while (count > 0 && *src != L'\0') {
        *dest++ = *src++;
        count--;
    }
    while (count-- > 0) {
        *dest++ = L'\0';
    }
    return ret;
}

int wcscmp(const wchar_t *lhs, const wchar_t *rhs) {
    while (*lhs != L'\0' && *lhs == *rhs) {
        lhs++;
        rhs++;
    }
    return (int)(unsigned short)*lhs - (int)(unsigned short)*rhs;
}

wchar_t *wcschr(const wchar_t *str, wchar_t ch) {
    while (*str != ch) {
        if (*str == L'\0') {
            return NULL;
        }
        str++;
    }
    return (wchar_t *)str;
}
