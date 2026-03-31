#include "utl/PoolAlloc.h"
#include "os/Debug.h"
#include "os/CritSec.h"

extern CriticalSection *gMemLock;
extern ChunkAllocator *gChunkAlloc[2];

FixedSizeAlloc::FixedSizeAlloc(int mAllocSizeWords, ChunkAllocator *alloc, int j)
    : mAllocSizeWords(mAllocSizeWords), mFreeList(0), mMaxAllocs(0), mNumChunks(0),
      mNumAllocs(0), mNodesPerChunk(j), mAlloc(alloc) {
    MILO_ASSERT(mAllocSizeWords != 0, 0xDD);
}

void *FixedSizeAlloc::Alloc() {
    if (!mFreeList)
        Refill();
    int *ret = mFreeList;
    int numAllocs = mNumAllocs + 1;
    int *next = (int *)*ret;
    mNumAllocs = numAllocs;
    mFreeList = next;
    if (numAllocs > mMaxAllocs) {
        mMaxAllocs = numAllocs;
    }
    return ret;
}

void FixedSizeAlloc::Free(void *v) {
    *(int **)v = mFreeList;
    mFreeList = (int *)v;
    MILO_ASSERT(mNumAllocs > 0, 0x102);
    mNumAllocs--;
}

void FixedSizeAlloc::Refill() {
    MILO_ASSERT(mFreeList == 0, 0x10a);
    int allocSize = mAllocSizeWords * mNodesPerChunk;
    mFreeList = (int *)RawAlloc(allocSize * 4);
    mNumChunks++;
    int *cur = mFreeList;
    int *end = mFreeList + (allocSize - mAllocSizeWords);
    while (cur < end) {
        int *next = cur + mAllocSizeWords;
        *cur = (int)next;
        cur = next;
    }
    *cur = 0;
}

void *ChunkAllocator::Alloc(int i) {
    int fixedSizeIndex = (i - 1) >> 2;
    MILO_ASSERT_FMT(fixedSizeIndex < MAX_FIXED_ALLOCS, "fixedSizeIndex (%d) < MAX_FIXED_ALLOCS (%d)\n", fixedSizeIndex, MAX_FIXED_ALLOCS);
    return mAllocs[fixedSizeIndex]->Alloc();
}

void ChunkAllocator::Free(void *v, int i) {
    int fixedSizeIndex = (i - 1) >> 2;
    MILO_ASSERT(fixedSizeIndex < MAX_FIXED_ALLOCS, 0x16D);
    MILO_ASSERT(mAllocs[fixedSizeIndex], 0x16E);
    mAllocs[fixedSizeIndex]->Free(v);
}

void _PoolFree(int size, PoolType pool, void *addr) {
    if (!AddrIsInPool(addr, pool)) {
        _MemFree(addr);
    } else if (addr) {
        CritSecTracker cst(gMemLock);
        MILO_ASSERT(gChunkAlloc[pool], 0x22F);
        gChunkAlloc[pool]->Free(addr, size);
    }
}
