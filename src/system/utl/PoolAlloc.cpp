#include "utl/PoolAlloc.h"
#include "utl/MemMgr.h"
#include "os/Debug.h"
#include "os/CritSec.h"
#include <revolution/os/OSError.h>
#include <cstdio>

extern CriticalSection *gMemLock;
extern ChunkAllocator *gChunkAlloc[2];

bool MemTempAllocationsEnabled();

int *ChunkAllocator::RawPoolAlloc(int size) {
    int alignedSize = size & ~3;
    mTotalCapacity += size;
    if ((unsigned int)((char *)mPoolStart + alignedSize) > (unsigned int)mPoolEnd) {
        if (MemNumHeaps() > 0) {
            if (mBigHunk == mSmallHunk) {
                printf(
                    "PoolAlloc warning: allocating small pool chunk (total: %d)\n",
                    mTotalCapacity
                );
            }
            MemPushHeap(mHeap);
        }
        MemDoTempAllocations tmp(false, false);
        mPoolStart = (int *)_MemAlloc(mBigHunk, 0);
        OSReport("PoolAlloc: allocating %d\n", mBigHunk);
        OSReport("Adding pool hunk %d (%d total size)\n", mNumHunks, mTotalCapacity);
        int fastHeap = MemFindHeap("fast");
        if (mNumHunks == 0 && mHeap == fastHeap) {
            OSReport("fast pool info:\n");
            int used, blocks, largest, total;
            MemFreeBlockStats(fastHeap, used, blocks, largest, total);
            OSReport("mPoolStart = %x largest = %d\n", mPoolStart, total);
        }
        if (MemNumHeaps() > 0) {
            MemPopHeap();
        }
        mHunks[mNumHunks].mStart = mPoolStart;
        mHunks[mNumHunks].mSize = mBigHunk;
        mNumHunks++;
        MILO_ASSERT(mNumHunks < MAX_HUNKS, 0xB6);
        mPoolEnd = (int *)((char *)mPoolStart + (mBigHunk & ~3));
        mPoolStart = (int *)((char *)mPoolStart + 0x40);
        mBigHunk = mSmallHunk;
    }
    int *ret = mPoolStart;
    mPoolStart = (int *)((char *)mPoolStart + alignedSize);
    return ret;
}

void *ChunkAllocator::operator new(size_t size) {
    static int _x = MemFindHeap("fast");
    MemPushHeap(_x);
    void *mem = _MemAlloc(size, 0);
    MemPopHeap();
    return mem;
}

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
