#include "math_api.h"
#include <math.h>

int __fpclassifyf(float x) {
    union { float f; unsigned int u; } v;
    v.f = x;
    unsigned int exp = (v.u >> 23) & 0xFF;
    unsigned int mant = v.u & 0x7FFFFF;
    if (exp == 0xFF) {
        return mant ? FP_NAN : FP_INFINITE;
    } else if (exp == 0) {
        return mant ? FP_SUBNORMAL : FP_ZERO;
    }
    return FP_NORMAL;
}

int __signbitd(double x) {
    union { double d; unsigned int i[2]; } u;
    u.d = x;
    return u.i[0] >> 31;
}

int __fpclassifyd(double x) {
    union { double d; unsigned int i[2]; } u;
    u.d = x;
    unsigned int exp = (u.i[0] >> 20) & 0x7FF;
    unsigned int mant_hi = u.i[0] & 0xFFFFF;
    unsigned int mant_lo = u.i[1];
    if (exp == 0x7FF) {
        return (mant_hi | mant_lo) ? FP_NAN : FP_INFINITE;
    } else if (exp == 0) {
        return (mant_hi | mant_lo) ? FP_SUBNORMAL : FP_ZERO;
    }
    return FP_NORMAL;
}
