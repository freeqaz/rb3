#include "types.h"

static void __InitParams(void *ctx) {
    (void)ctx;
}

s32 AXFXDelayInit(void *ctx, void *mem, s32 size) {
    (void)ctx; (void)mem; (void)size;
    return 0;
}

s32 AXFXDelaySettings(void *ctx) {
    (void)ctx;
    return 0;
}

void AXFXDelayShutdown(void *ctx) {
    (void)ctx;
}

void AXFXDelayCallback(void *ctx, void *buf, s32 size) {
    (void)ctx; (void)buf; (void)size;
}
