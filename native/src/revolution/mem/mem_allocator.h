// Native shim for <revolution/mem/mem_allocator.h>.
// os/ContentMgr_Wii.h declares `extern MEMAllocator gCNTAllocator;` (by value),
// so the layout is needed. No allocator functions run on the DTA-parse path.
#pragma once
#ifdef HX_NATIVE

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MEMAllocator;
typedef void *(*MEMAllocatorAllocFunc)(struct MEMAllocator *allocator, u32 size);
typedef void (*MEMAllocatorFreeFunc)(struct MEMAllocator *allocator, void *block);

typedef struct MEMAllocatorFuncs {
    MEMAllocatorAllocFunc allocFunc;
    MEMAllocatorFreeFunc freeFunc;
} MEMAllocatorFuncs;

typedef struct MEMAllocator {
    const MEMAllocatorFuncs *funcs;
    struct MEMiHeapHead *heap;
    u32 heapParam1;
    u32 heapParam2;
} MEMAllocator;

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
