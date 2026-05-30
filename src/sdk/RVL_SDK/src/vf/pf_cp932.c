#include "types.h"

const u16 cp932_to_unicode[0x4272 / 2] = {0};

s32 VFiPFCODE_CP932_OEM2Unicode(u16 oem, u16 *uni) {
    (void)oem; (void)uni;
    return 0;
}

s32 VFiPFCODE_CP932_Unicode2OEM(u16 uni, u16 *oem) {
    (void)uni; (void)oem;
    return 0;
}

s32 VFiPFCODE_CP932_OEMCharWidth(u16 c) {
    (void)c;
    return 1;
}

s32 VFiPFCODE_CP932_isOEMMBchar(u16 c) {
    (void)c;
    return 0;
}

s32 VFiPFCODE_CP932_UnicodeCharWidth(u16 c) {
    (void)c;
    return 1;
}

s32 VFiPFCODE_CP932_isUnicodeMBchar(u16 c) {
    (void)c;
    return 0;
}
