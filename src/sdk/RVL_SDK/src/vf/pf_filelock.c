#include "types.h"

void VFiPF_InitLockFile(void *fp) {
    *(s32 *)fp = 0;
}

s32 VFiPF_UnLockFile(void *fp) {
    return *(s32 *)fp;
}
