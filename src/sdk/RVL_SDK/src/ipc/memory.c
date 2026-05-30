#include "types.h"

u8 __heaps[0x80];

s32 iosCreateHeap(void *buf, s32 size) {
    (void)buf; (void)size;
    return 0;
}

void *__iosAlloc(s32 heap, s32 size) {
    (void)heap; (void)size;
    return 0;
}

void *iosAlloc(s32 heap, s32 size) {
    (void)heap; (void)size;
    return 0;
}

void *iosAllocAligned(s32 heap, s32 size, s32 align) {
    (void)heap; (void)size; (void)align;
    return 0;
}

void iosFree(s32 heap, void *ptr) {
    (void)heap; (void)ptr;
}
