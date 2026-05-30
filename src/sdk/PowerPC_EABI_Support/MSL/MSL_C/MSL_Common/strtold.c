#include "MSL_Common/strtold.h"

long double __strtold(int max_width, int (*ReadProc)(void *, int, int),
                      void *ReadProcArg, int *chars_scanned, int *overflow) {
    long double result = 0.0L;
    long double frac = 0.1L;
    int c, count = 0;
    int neg = 0;
    int in_frac = 0;
    int has_digits = 0;

    c = (*ReadProc)(ReadProcArg, 0, 1);
    while (c == ' ' || c == '\t') { c = (*ReadProc)(ReadProcArg, 0, 1); count++; }

    if (c == '+') { c = (*ReadProc)(ReadProcArg, 0, 1); count++; }
    else if (c == '-') { neg = 1; c = (*ReadProc)(ReadProcArg, 0, 1); count++; }

    while ((max_width < 0 || count < max_width) && c >= 0) {
        if (c >= '0' && c <= '9') {
            has_digits = 1;
            if (in_frac) {
                result += (long double)(c - '0') * frac;
                frac *= 0.1L;
            } else {
                result = result * 10.0L + (long double)(c - '0');
            }
        } else if (c == '.' && !in_frac) {
            in_frac = 1;
        } else if (c == 'e' || c == 'E') {
            int exp_neg = 0;
            int exp_val = 0;
            count++;
            c = (*ReadProc)(ReadProcArg, 0, 1);
            if (c == '+') { c = (*ReadProc)(ReadProcArg, 0, 1); count++; }
            else if (c == '-') { exp_neg = 1; c = (*ReadProc)(ReadProcArg, 0, 1); count++; }
            while (c >= '0' && c <= '9') {
                exp_val = exp_val * 10 + (c - '0');
                c = (*ReadProc)(ReadProcArg, 0, 1); count++;
            }
            (*ReadProc)(ReadProcArg, c, -1);
            while (exp_val-- > 0) {
                if (exp_neg) result /= 10.0L;
                else result *= 10.0L;
            }
            *chars_scanned = count;
            *overflow = 0;
            return neg ? -result : result;
        } else {
            break;
        }
        c = (*ReadProc)(ReadProcArg, 0, 1); count++;
    }
    (*ReadProc)(ReadProcArg, c, -1);

    *chars_scanned = count;
    *overflow = 0;
    return neg ? -result : result;
}

double atof(const char *str) {
    double result = 0.0;
    double frac = 0.1;
    int neg = 0;
    int in_frac = 0;
    const char *p = str;

    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') p++;

    while (*p >= '0' && *p <= '9') {
        result = result * 10.0 + (double)(*p - '0');
        p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            result += (double)(*p - '0') * frac;
            frac *= 0.1;
            p++;
        }
    }
    if (*p == 'e' || *p == 'E') {
        int exp_neg = 0, exp_val = 0;
        p++;
        if (*p == '-') { exp_neg = 1; p++; }
        else if (*p == '+') p++;
        while (*p >= '0' && *p <= '9') { exp_val = exp_val * 10 + (*p - '0'); p++; }
        while (exp_val-- > 0) { if (exp_neg) result /= 10.0; else result *= 10.0; }
    }
    return neg ? -result : result;
}
