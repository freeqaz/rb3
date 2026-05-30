#include "MSL_Common/wchar_def.h"
#include "MSL_Common/size_def.h"

int mbtowc(wchar_t *pwc, const char *s, size_t n) {
    if (s == NULL) return 0;
    if (n == 0) return -1;
    if (pwc != NULL) *pwc = (wchar_t)(unsigned char)*s;
    return (*s == '\0') ? 0 : 1;
}

int __mbtowc_noconv(wchar_t *pwc, const char *s, size_t n) {
    if (s == NULL) return 0;
    if (n == 0) return -1;
    if (pwc != NULL) *pwc = (wchar_t)(unsigned char)*s;
    if (*s == '\0') return 0;
    return 1;
}

int __wctomb_noconv(char *s, wchar_t wc) {
    if (s == NULL) return 0;
    *s = (char)(wc & 0xFF);
    return 1;
}

size_t mbstowcs(wchar_t *dst, const char *src, size_t n) {
    size_t i = 0;
    while (i < n) {
        wchar_t wc = (wchar_t)(unsigned char)*src;
        if (dst != NULL) dst[i] = wc;
        if (wc == L'\0') return i;
        i++;
        src++;
    }
    return i;
}

size_t wcstombs(char *dst, const wchar_t *src, size_t n) {
    size_t i = 0;
    while (i < n) {
        char c = (char)(*src & 0xFF);
        if (dst != NULL) dst[i] = c;
        if (c == '\0') return i;
        i++;
        src++;
    }
    return i;
}
