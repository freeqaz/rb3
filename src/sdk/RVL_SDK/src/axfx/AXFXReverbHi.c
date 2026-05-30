#include "types.h"

s32 AXFXReverbHiInit(void *ctx, void *mem, s32 size) {
    (void)ctx; (void)mem; (void)size;
    return 0;
}

void AXFXReverbHiShutdown(void *ctx) {
    (void)ctx;
}

s32 AXFXReverbHiSettings(void *ctx) {
    (void)ctx;
    return 0;
}

void AXFXReverbHiCallback(void *ctx, void *buf, s32 size) {
    (void)ctx; (void)buf; (void)size;
}
