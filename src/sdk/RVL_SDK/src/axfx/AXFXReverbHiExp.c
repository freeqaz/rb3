#include "types.h"

static void __AllocDelayLine(void *ctx) {
    (void)ctx;
}

static void __BzeroDelayLines(void *ctx) {
    (void)ctx;
}

static void __FreeDelayLine(void *ctx) {
    (void)ctx;
}

static void __InitParams(void *ctx) {
    (void)ctx;
}

s32 AXFXReverbHiExpInit(void *ctx, void *mem, s32 size) {
    (void)ctx; (void)mem; (void)size;
    return 0;
}

s32 AXFXReverbHiExpSettings(void *ctx) {
    (void)ctx;
    return 0;
}

void AXFXReverbHiExpShutdown(void *ctx) {
    (void)ctx;
}

void AXFXReverbHiExpCallback(void *ctx, void *buf, s32 size) {
    (void)ctx; (void)buf; (void)size;
}
