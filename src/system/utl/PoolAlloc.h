#ifndef UTL_POOLALLOC_H
#define UTL_POOLALLOC_H

#include <stddef.h>

#define MAX_FIXED_ALLOCS 0x40
#define MAX_HUNKS 0x400

// forward declaration
class FixedSizeAlloc;

int MemFindHeap(const char *);
void MemPushHeap(int);
void MemPopHeap();
void *_MemAlloc(int, int);

class ChunkAllocator {
public:
    struct Hunk {
        int *mStart; // 0x0
        int mSize; // 0x4
    };

    int mBigHunk; // 0x0
    int mSmallHunk; // 0x4
    int mTotalCapacity; // 0x8
    int mPeakCapacity; // 0xc
    Hunk mHunks[MAX_HUNKS]; // 0x10 - 0x200F (embedded hunk-record table)
    int mNumHunks; // 0x2010
    int *mPoolEnd; // 0x2014
    int *mPoolStart; // 0x2018
    int mHeap; // 0x201c
    FixedSizeAlloc *mAllocs[MAX_FIXED_ALLOCS]; // 0x2020

    ChunkAllocator(int heap, int bigHunk, int smallHunk);
    void *Alloc(int);
    void Free(void *, int);
    int *RawPoolAlloc(int);
    static void UploadDebugStats();

    void *operator new(size_t size) {
        static int _x = MemFindHeap("fast");
        MemPushHeap(_x);
        void *mem = _MemAlloc(size, 0);
        MemPopHeap();
        return mem;
    }
};

class FixedSizeAlloc {
public:
    FixedSizeAlloc(int, ChunkAllocator *, int);
    virtual ~FixedSizeAlloc();
    virtual void *RawAlloc(int);

    int mAllocSizeWords;
    int mNumAllocs;
    int mMaxAllocs;
    int mNumChunks;
    int *mFreeList;
    int mNodesPerChunk;
    ChunkAllocator *mAlloc;

    void *Alloc();
    void Free(void *);
    void Refill();
};

enum PoolType {
    MainPool,
    FastPool
};

bool AddrIsInPool(void *, PoolType);
void *_PoolAlloc(int, int, PoolType);
void _PoolFree(int, PoolType, void *);

#define NEW_POOL_OVERLOAD(obj)                                                           \
    void *operator new(size_t s) { return _PoolAlloc(sizeof(obj), s, FastPool); }        \
    void *operator new(size_t, void *place) { return place; }

#define DELETE_POOL_OVERLOAD(obj)                                                        \
    void operator delete(void *v) { _PoolFree(sizeof(obj), FastPool, v); }

#endif
