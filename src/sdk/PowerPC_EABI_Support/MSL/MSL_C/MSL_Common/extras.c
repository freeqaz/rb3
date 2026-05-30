#include "MSL_Common/extras.h"
#include "MSL_Common/wchar_def.h"
#include "MSL_Common/size_def.h"
#include <stdlib.h>

char *strdup(const char *s) {
    return NULL;
}

int stricmp(const char *s1, const char *s2) {
    unsigned char c1, c2;
    do {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';
    } while (c1 != '\0' && c1 == c2);
    return (int)c1 - (int)c2;
}

int strnicmp(const char *s1, const char *s2, size_t n) {
    unsigned char c1, c2;
    while (n-- > 0) {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';
        if (c1 != c2) return (int)c1 - (int)c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}

int strncasecmp(const char *lhs, const char *rhs, size_t maxlen) {
    return strnicmp(lhs, rhs, maxlen);
}

int strcasecmp(const char *lhs, const char *rhs) {
    return stricmp(lhs, rhs);
}

int wcsnicmp(const wchar_t *s1, const wchar_t *s2, size_t n) {
    unsigned short c1, c2;
    while (n-- > 0) {
        c1 = (unsigned short)*s1++;
        c2 = (unsigned short)*s2++;
        if (c1 >= L'A' && c1 <= L'Z') c1 += L'a' - L'A';
        if (c2 >= L'A' && c2 <= L'Z') c2 += L'a' - L'A';
        if (c1 != c2) return (int)c1 - (int)c2;
        if (c1 == L'\0') return 0;
    }
    return 0;
}
