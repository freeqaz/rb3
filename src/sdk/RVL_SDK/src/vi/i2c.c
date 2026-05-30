#include "types.h"

s32 __i2c_ident_first;

static void WaitMicroTime(u32 us) {
    (void)us;
}

static s32 sendSlaveAddr(u8 addr, s32 write) {
    (void)addr; (void)write;
    return 0;
}

s32 __VISendI2CData(u8 addr, const u8 *data, s32 len) {
    (void)addr; (void)data; (void)len;
    return 0;
}
