#include "MSL_Common/strtoul.h"
#include "MSL_Common/restrict_def.h"

unsigned long __strtoul(int base, int max_width, int (*ReadProc)(void *, int, int),
                        void *ReadProcArg, int *chars_scanned, int *negative, int *overflow) {
    unsigned long result = 0;
    int c, count = 0;
    int neg = 0;
    int ovf = 0;

    c = (*ReadProc)(ReadProcArg, 0, 1);
    while (c == ' ' || c == '\t') { c = (*ReadProc)(ReadProcArg, 0, 1); count++; }

    if (c == '+') { c = (*ReadProc)(ReadProcArg, 0, 1); count++; }
    else if (c == '-') { neg = 1; c = (*ReadProc)(ReadProcArg, 0, 1); count++; }

    if (base == 0 || base == 16) {
        if (c == '0') {
            c = (*ReadProc)(ReadProcArg, 0, 1); count++;
            if (c == 'x' || c == 'X') { base = 16; c = (*ReadProc)(ReadProcArg, 0, 1); count++; }
            else if (base == 0) base = 8;
        } else if (base == 0) base = 10;
    }
    if (base == 0) base = 10;

    while ((max_width < 0 || count < max_width) && c >= 0) {
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else break;
        if (digit >= base) break;
        if (!ovf) {
            unsigned long prev = result;
            result = result * (unsigned long)base + (unsigned long)digit;
            if (result < prev) ovf = 1;
        }
        c = (*ReadProc)(ReadProcArg, 0, 1); count++;
    }
    (*ReadProc)(ReadProcArg, c, -1);

    *chars_scanned = count;
    *negative = neg;
    *overflow = ovf;
    return result;
}

unsigned long long __strtoull(int base, int max_width, int (*ReadProc)(void *, int, int),
                              void *ReadProcArg, int *chars_scanned, int *negative, int *overflow) {
    unsigned long long result = 0;
    int c, count = 0;
    int neg = 0;
    int ovf = 0;

    c = (*ReadProc)(ReadProcArg, 0, 1);
    while (c == ' ' || c == '\t') { c = (*ReadProc)(ReadProcArg, 0, 1); count++; }

    if (c == '+') { c = (*ReadProc)(ReadProcArg, 0, 1); count++; }
    else if (c == '-') { neg = 1; c = (*ReadProc)(ReadProcArg, 0, 1); count++; }

    if (base == 0 || base == 16) {
        if (c == '0') {
            c = (*ReadProc)(ReadProcArg, 0, 1); count++;
            if (c == 'x' || c == 'X') { base = 16; c = (*ReadProc)(ReadProcArg, 0, 1); count++; }
            else if (base == 0) base = 8;
        } else if (base == 0) base = 10;
    }
    if (base == 0) base = 10;

    while ((max_width < 0 || count < max_width) && c >= 0) {
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else break;
        if (digit >= base) break;
        if (!ovf) {
            unsigned long long prev = result;
            result = result * (unsigned long long)base + (unsigned long long)digit;
            if (result < prev) ovf = 1;
        }
        c = (*ReadProc)(ReadProcArg, 0, 1); count++;
    }
    (*ReadProc)(ReadProcArg, c, -1);

    *chars_scanned = count;
    *negative = neg;
    *overflow = ovf;
    return result;
}

unsigned long strtoul(const char *RESTRICT str, char **RESTRICT str_end, int base) {
    unsigned long result = 0;
    int neg = 0, ovf = 0, count = 0;
    const char *p = str;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '+') p++;
    else if (*p == '-') { neg = 1; p++; }
    if ((base == 0 || base == 16) && *p == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    else if (base == 0 && *p == '0') { base = 8; p++; }
    else if (base == 0) base = 10;
    while (*p) {
        int d;
        if (*p >= '0' && *p <= '9') d = *p - '0';
        else if (*p >= 'a' && *p <= 'z') d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'Z') d = *p - 'A' + 10;
        else break;
        if (d >= base) break;
        result = result * (unsigned long)base + (unsigned long)d;
        p++;
    }
    if (str_end != NULL) *str_end = (char *)p;
    return neg ? (unsigned long)(-(long)result) : result;
}

unsigned long long strtoull(const char *RESTRICT str, char **RESTRICT str_end, int base) {
    unsigned long long result = 0;
    int neg = 0;
    const char *p = str;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '+') p++;
    else if (*p == '-') { neg = 1; p++; }
    if ((base == 0 || base == 16) && *p == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    else if (base == 0 && *p == '0') { base = 8; p++; }
    else if (base == 0) base = 10;
    while (*p) {
        int d;
        if (*p >= '0' && *p <= '9') d = *p - '0';
        else if (*p >= 'a' && *p <= 'z') d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'Z') d = *p - 'A' + 10;
        else break;
        if (d >= base) break;
        result = result * (unsigned long long)base + (unsigned long long)d;
        p++;
    }
    if (str_end != NULL) *str_end = (char *)p;
    return neg ? (unsigned long long)(-(long long)result) : result;
}

long strtol(const char *RESTRICT str, char **RESTRICT str_end, int base) {
    int neg = 0;
    unsigned long uval;
    const char *p = str;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') p++;
    uval = strtoul(p, str_end, base);
    return neg ? -(long)uval : (long)uval;
}

long long strtoll(const char *RESTRICT str, char **RESTRICT str_end, int base) {
    int neg = 0;
    unsigned long long uval;
    const char *p = str;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') p++;
    uval = strtoull(p, str_end, base);
    return neg ? -(long long)uval : (long long)uval;
}

int atoi(const char *str) {
    return (int)strtol(str, NULL, 10);
}
