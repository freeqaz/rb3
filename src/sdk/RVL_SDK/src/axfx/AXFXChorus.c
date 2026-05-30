#include "types.h"

s32 AXFXChorusInit(void *chorus, void *mem, s32 size) {
    (void)chorus; (void)mem; (void)size;
    return 0;
}

void AXFXChorusShutdown(void *chorus) {
    (void)chorus;
}

s32 AXFXChorusSettings(void *chorus) {
    (void)chorus;
    return 0;
}

void AXFXChorusCallback(void *chorus, void *buf, s32 size) {
    (void)chorus; (void)buf; (void)size;
}
