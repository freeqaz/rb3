#include "utl/PoolAlloc.h"
#include "utl/MemMgr.h"
#include "utl/DataPointMgr.h"
#include "os/Debug.h"
#include "os/CritSec.h"
#include "os/System.h"
#include <revolution/os/OSError.h>
#include <cstdio>
#ifdef HX_NATIVE
// FixedSizeAlloc::Alloc uses intptr_t for the free-list pop (fa463d01d, the
// shape that takes it to 100%). MWCC's MSL pulls intptr_t in transitively, so
// the Wii build never needed a declaration; clang's libc++ does not, so the
// native and web builds fail to compile without this. Guarded so MWCC's
// preprocessor never sees it and the match is untouched. <cstdint> is
// available under both the native clang toolchain and emscripten/musl.
#include <cstdint>
#endif

extern CriticalSection *gMemLock;
extern ChunkAllocator *gChunkAlloc[2];

bool MemTempAllocationsEnabled();
void InitDefaultHeap();

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

FixedSizeAlloc::FixedSizeAlloc(int mAllocSizeWords, ChunkAllocator *alloc, int j)
    : mAllocSizeWords(mAllocSizeWords), mFreeList(0), mMaxAllocs(0), mNumChunks(0),
      mNumAllocs(0), mNodesPerChunk(j), mAlloc(alloc) {
    MILO_ASSERT(mAllocSizeWords != 0, 0xDD);
}

void *FixedSizeAlloc::Alloc() {
    if (!mFreeList)
        Refill();
    intptr_t ret = (intptr_t)mFreeList;
    intptr_t next = (intptr_t)*mFreeList;
    mFreeList = (int *)next;
    mNumAllocs++;
    if (mNumAllocs > mMaxAllocs) {
        mMaxAllocs = mNumAllocs;
    }
    return (void *)ret;
}

void FixedSizeAlloc::Free(void *v) {
    *(int **)v = mFreeList;
    mFreeList = (int *)v;
    MILO_ASSERT(mNumAllocs > 0, 0x102);
    mNumAllocs--;
}

FixedSizeAlloc::~FixedSizeAlloc() {}

void *FixedSizeAlloc::RawAlloc(int size) {
    return mAlloc->RawPoolAlloc(size);
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

ChunkAllocator::ChunkAllocator(int heap, int bigHunk, int smallHunk)
    : mBigHunk(bigHunk), mSmallHunk(smallHunk), mTotalCapacity(0), mPeakCapacity(0),
      mNumHunks(0), mPoolEnd(0), mPoolStart(0), mHeap(heap) {
    InitDefaultHeap();
    static int fastHeap = MemFindHeap("fast");
    MemPushHeap(fastHeap);
    for (int i = 0; i < MAX_FIXED_ALLOCS; i++) {
        mAllocs[i] = new FixedSizeAlloc(i + 1, this, 20);
    }
    MemPopHeap();
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

void ChunkAllocator::UploadDebugStats() {
    if (UsingCD()) {
        for (int i = 0; i < 2; i++) {
            ChunkAllocator *alloc = gChunkAlloc[i];
            if (alloc) {
                int total = alloc->mTotalCapacity;
                if (total > alloc->mPeakCapacity) {
                    alloc->mPeakCapacity = total;
                    SendDataPoint("debug/pool", "TotalChunkSize", gChunkAlloc[i]->mPeakCapacity);
                }
            }
        }
    }
}

bool AddrIsInPool(void *addr, PoolType pool) {
    ChunkAllocator::Hunk *hunks;
    int n;
    unsigned int start;
    unsigned int end;
    for (int i = 0; i < 2; i++) {
        if (gChunkAlloc[i]) {
            hunks = gChunkAlloc[i]->mHunks;
            n = gChunkAlloc[i]->mNumHunks;
            for (int j = 0; j < n; j++) {
                start = (unsigned int)hunks->mStart;
                end = start + hunks->mSize;
                if ((unsigned int)addr >= start && (unsigned int)addr <= end) {
                    return true;
                }
                hunks++;
            }
        }
    }
    return false;
}

void *_PoolAlloc(int classSize, int reqSize, PoolType pool) {
#ifdef HX_NATIVE
    // Native build has no Wii fixed-size pools / ChunkAllocator (gChunkAlloc and
    // the heap descriptors are Wii-boot data that don't exist here). Route pool
    // allocations straight through the (malloc-backed) general allocator.
    //
    // CRITICAL: NEW_POOL_OVERLOAD's `operator new(size_t s)` hard-codes the
    // declaring class's `sizeof(obj)` as `classSize`, but when a DERIVED class
    // inherits that operator new (e.g. VorbisReader : ... : CriticalSection,
    // which inherits CriticalSection's pool overload), the C++ ABI calls it
    // with s == sizeof(DerivedClass), which is bigger. The matched-fork path
    // below MILO_ASSERTs `reqSize == classSize`. Use the LARGER of the two so
    // derived-class allocations are sized to their actual layout (the C++
    // compiler's reqSize already accounts for the derived sub-objects + any
    // base padding); otherwise the ctor's field stores past the base-class
    // allocation smash the next heap chunk's malloc header — the classic
    // glibc `_int_malloc` consistency abort that fires on the NEXT alloc.
    int allocSize = classSize > reqSize ? classSize : reqSize;
    return _MemAlloc(allocSize, 0x20);
#else
    if (MemTempAllocationsEnabled()) {
        bool notPoolSize = true;
        switch (reqSize) {
        case 0:
        case 4:
        case 8:
        case 0xC:
        case 0x10:
        case 0x14:
        case 0x18:
        case 0x20:
        case 0x24:
        case 0x28:
        case 0x29:
        case 0x30:
        case 0x38:
        case 0x3C:
        case 0x40:
        case 0x48:
        case 0x60:
        case 0x80:
        case 0xC0:
        case 0xD0:
            notPoolSize = false;
            break;
        }
        if (!notPoolSize) {
            return _MemAlloc(classSize, 0x20);
        }
    }
    MILO_ASSERT_FMT(classSize >= 0, "PoolAlloc class size is < 0: %d", classSize);
    CritSecTracker cst(gMemLock);
    if (!gChunkAlloc[pool]) {
        int fastHeap = MemFindHeap("fast");
        switch (pool) {
        case FastPool:
            gChunkAlloc[FastPool] = new ChunkAllocator(fastHeap, 0x947000, 0x19000);
            break;
        case MainPool:
            gChunkAlloc[MainPool] =
                new ChunkAllocator(MemFindHeap("main"), 0x400, 0x19000);
            break;
        default:
            MILO_ASSERT(0, 0x1F6);
        }
    }
    MILO_ASSERT(reqSize == classSize, 0x1FA);
    return gChunkAlloc[pool]->Alloc(classSize);
#endif // HX_NATIVE
}

void _PoolFree(int size, PoolType pool, void *addr) {
#ifdef HX_NATIVE
    // Mirror _PoolAlloc: everything came from the general allocator.
    _MemFree(addr);
#else
    if (!AddrIsInPool(addr, pool)) {
        _MemFree(addr);
    } else if (addr) {
        CritSecTracker cst(gMemLock);
        MILO_ASSERT(gChunkAlloc[pool], 0x22F);
        gChunkAlloc[pool]->Free(addr, size);
    }
#endif
}

#ifdef HX_NATIVE
// Defined here (ChunkAllocator visible) because the Wii pool data lives in an
// excluded TU. The native _PoolAlloc/_PoolFree never use these, but other
// compiled-but-unused functions reference the symbol.
ChunkAllocator *gChunkAlloc[2] = {nullptr, nullptr};
#endif
