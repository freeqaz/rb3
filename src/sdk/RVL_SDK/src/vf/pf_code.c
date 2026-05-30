#include "types.h"

const u8 VFipf_valid_fn_char[0x60];

s32 VFiPFCODE_Combine_Width(s32 c1, s32 c2) {
    return ((u8)c1 << 8) | (u8)c2;
}

s32 VFiPFCODE_Divide_Width(s32 c) {
    return (c >> 8) & 0xFF;
}
