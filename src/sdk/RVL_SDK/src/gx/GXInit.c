#include "types.h"

void *__piReg;
void *__cpReg;
void *__peReg;
void *__memReg;

void *__GXDefaultTexRegionCallback(void *a, s32 b) {
    (void)a; (void)b;
    return 0;
}

void *__GXDefaultTlutRegionCallback(s32 a) {
    (void)a;
    return 0;
}

void __GXShutdown(void) {
}

void __GXInitRevisionBits(void) {
}

void *GXInit(void *buf, u32 size) {
    (void)buf; (void)size;
    return 0;
}

void __GXInitGX(void) {
}
