#include "types.h"

static void __InitParams(void *ctx) {
    (void)ctx;
}

static void __CalcLFO(void *ctx) {
    (void)ctx;
}

s32 AXFXChorusExpInit(void *ctx, void *mem, s32 size) {
    (void)ctx; (void)mem; (void)size;
    return 0;
}

s32 AXFXChorusExpSettings(void *ctx) {
    (void)ctx;
    return 0;
}

void AXFXChorusExpShutdown(void *ctx) {
    (void)ctx;
}

void AXFXChorusExpCallback(void *ctx, void *buf, s32 size) {
    (void)ctx; (void)buf; (void)size;
}
