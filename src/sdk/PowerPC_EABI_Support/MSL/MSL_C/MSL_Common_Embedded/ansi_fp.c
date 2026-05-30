#include "MSL_Common/ansi_fp.h"
#include <string.h>

void __ull2dec(decimal *d, unsigned long long val) {
    char buf[32];
    int i = 0;
    d->sign = 0;
    d->exp = 0;
    if (val == 0) {
        d->sig.length = 1;
        d->sig.text[0] = '0';
        return;
    }
    while (val > 0) {
        buf[i++] = (char)('0' + (int)(val % 10));
        val /= 10;
    }
    d->sig.length = (unsigned char)i;
    d->exp = (short)(i - 1);
    for (int j = 0; j < i; j++) {
        d->sig.text[j] = (unsigned char)buf[i - 1 - j];
    }
}

void __timesdec(decimal *result, const decimal *a, const decimal *b) {
    (void)a;
    (void)b;
    memset(result, 0, sizeof(decimal));
}

void __str2dec(decimal *d, const char *str, short max_sig) {
    (void)max_sig;
    memset(d, 0, sizeof(decimal));
    d->sig.text[0] = '0';
    d->sig.length = 1;
    if (str == NULL) return;
    if (*str == '-') { d->sign = 1; str++; }
    else if (*str == '+') str++;
    int i = 0;
    while (*str >= '0' && *str <= '9' && i < SIGDIGLEN) {
        d->sig.text[i++] = (unsigned char)*str++;
    }
    d->sig.length = (unsigned char)i;
}

void __two_exp(decimal *d, long n) {
    memset(d, 0, sizeof(decimal));
    (void)n;
}

int __equals_dec(const decimal *a, const decimal *b) {
    (void)a;
    (void)b;
    return 0;
}

int __less_dec(const decimal *a, const decimal *b) {
    (void)a;
    (void)b;
    return 0;
}

void __minus_dec(decimal *result, const decimal *a, const decimal *b) {
    (void)a;
    (void)b;
    memset(result, 0, sizeof(decimal));
}

void __num2dec_internal(decimal *d, double val) {
    (void)val;
    memset(d, 0, sizeof(decimal));
}

void __num2dec(const decform *form, double val, decimal *d) {
    (void)form;
    __num2dec_internal(d, val);
}

double __dec2num(const decimal *d) {
    (void)d;
    return 0.0;
}
