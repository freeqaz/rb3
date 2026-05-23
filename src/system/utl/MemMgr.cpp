#include "utl/MemMgr.h"
#include "os/Debug.h"
#include "obj/Data.h"
#include <cstring>

extern "C" void *WiiMalloc(int);
extern "C" void WiiFree(void *);
int GetFreeSystemMemory();

struct MemHeapStack {
    int mStack[16]; // 0x0
    int mSize;      // 0x40
};

struct FreeBlock {
    int mSizeWords;          // 0x0
    unsigned int mTimeStamp; // 0x4
    FreeBlock *mNext;        // 0x8

    bool AttemptMerge(FreeBlock *, int);
};

struct AllocBlock {
    unsigned int mHeader; // 0x0
};

class Heap {
public:
    enum Strategy {
        kFirstFit = 0,
        kBestFit = 1,
        kBestLastFit = 2,
        kLRUFit = 3,
        kLastFit = 4,
    };

    struct FreeBlockInfo {
        FreeBlock *mBlock; // 0x0
        FreeBlock *mPrev;  // 0x4
        int mBlockSize;    // 0x8
        int mPadWords;     // 0xC
    };

    void FreeBlockStats(int &, int &, int &, int &);
    void MoreFreeBlockStats(int &, int &, int &, int &);
    void ResetMinFreeBlockStats();
    void InsertFreeBlock(FreeBlock *, int, FreeBlock *, FreeBlock *, int);
    int AllocSize(int *);
    int *SplitFromBack(int);
    bool Free(int *);
    int *Truncate(int *, int, int &);
    void FindFreeNeighbors(AllocBlock *, FreeBlock *&, FreeBlock *&);
    void FirstFit(int, int, FreeBlockInfo &);
    void BestFit(int, int, FreeBlockInfo &);
    void BestLastFit(int, int, FreeBlockInfo &);
    void LRUFit(int, int, FreeBlockInfo &);
    void LastFit(int, int, FreeBlockInfo &);
    const char *Name() const { return mName; }

    FreeBlock *mFreeBlockChain; // 0x0
    int *mStart;        // 0x4
    const char *mName;  // 0x8
    int mSizeWords;     // 0xC
    char mPad10[8];     // 0x10
    int mDebugLevel;    // 0x18
    int mStrategy;      // 0x1C
    bool mAllowTemp;    // 0x20
    char mPad21[3];     // 0x21
    int mNumFreeBytes;  // 0x24
    int mBiggestFree;   // 0x28
    int mLargestFree;   // 0x2C
    int mMinLargest;    // 0x30
};

extern int gDefaultHeap;
extern Heap gHeaps[16];
int gNumHeaps;
static bool gMemInited;
extern int gSingleHeap;

namespace {
    unsigned int gTimeStamp;
}

int MemNumHeaps() { return gNumHeaps; }

const char *MemHeapName(int heap) {
    if (heap < 0) return "system";
    return gHeaps[heap].Name();
}

static MemHeapStack &ThreadMemStack(bool);

int GetCurrentHeapNum() {
    MemHeapStack &stack = ThreadMemStack(false);
    int heap;
    if (stack.mSize != 0) {
        heap = stack.mStack[stack.mSize - 1];
    } else {
        heap = gDefaultHeap;
    }
    if (heap < 0 && gMemInited) {
        heap = 0;
    }
    return heap;
}

void MemFreeBlockStats(int heapNum, int &a, int &b, int &c, int &d) {
    CritSecTracker tracker(gMemLock);
    MILO_ASSERT(heapNum < 0x10, 0x293);
    gHeaps[heapNum].FreeBlockStats(a, b, c, d);
}

void Heap::InsertFreeBlock(
    FreeBlock *block, int sizeWords, FreeBlock *prev, FreeBlock *next, int timeStamp
) {
    MILO_ASSERT(block == prev || block == next, 0x300);
    block->mSizeWords = sizeWords;
    block->mNext = next;
    block->mTimeStamp = timeStamp;
    if (prev != nullptr) {
        prev->mNext = block;
    } else {
        mFreeBlockChain = block;
    }
}

void MemTerminate() {}

int MemFindAddrHeap(void *addr) {
    for (int i = 0; i < gNumHeaps; i++) {
        int *start = gHeaps[i].mStart;
        if (addr >= start && addr < start + gHeaps[i].mSizeWords) {
            return i;
        }
    }
    return -1;
}

void *MemHeapStartAddr(int heap) { return gHeaps[heap].mStart; }

int Heap::AllocSize(int *mem) {
    if (mem < mStart || mem >= mStart + mSizeWords) {
        return 0;
    }
    unsigned int header = ((unsigned int *)mem)[-1];
    return (((header >> 8) - 1) - (header & 0xFF)) * 4;
}

void Heap::FindFreeNeighbors(AllocBlock *block, FreeBlock *&prev, FreeBlock *&next) {
    FreeBlock *cur = mFreeBlockChain;
    FreeBlock *last = nullptr;
    while (cur != nullptr && cur < (FreeBlock *)block) {
        last = cur;
        cur = cur->mNext;
    }
    next = cur;
    prev = last;
}

void Heap::FirstFit(int sizeWords, int alignShift, FreeBlockInfo &info) {
    FreeBlock *block = mFreeBlockChain;
    FreeBlock *prev = nullptr;
    for (; block != nullptr; prev = block, block = block->mNext) {
        int blockSize = block->mSizeWords;
        int wordAddr = ((int)block >> 2) + 1;
        int padWords =
            ((((unsigned int)(wordAddr + (1 << alignShift) - 1)) >> alignShift) << alignShift) -
            wordAddr;
        if (blockSize >= sizeWords + padWords) {
            info.mBlockSize = blockSize;
            info.mPadWords = padWords;
            info.mBlock = block;
            info.mPrev = prev;
            return;
        }
    }
}

void Heap::LastFit(int sizeWords, int alignShift, FreeBlockInfo &info) {
    FreeBlock *block = mFreeBlockChain;
    FreeBlock *prev = nullptr;
    if (block != nullptr) {
        int shift = alignShift + 2;
        for (; block != nullptr; prev = block, block = block->mNext) {
            int blockSize = block->mSizeWords;
            int padWords =
                (((((int)block + blockSize * 4 - sizeWords * 4) >> shift) << shift) - 4 -
                 (int)block) /
                4;
            if (padWords >= 0) {
                info.mBlockSize = blockSize;
                info.mPadWords = padWords;
                info.mBlock = block;
                info.mPrev = prev;
            }
        }
    }
}

int *Heap::SplitFromBack(int n) {
    FreeBlock *startBlock = (FreeBlock *)mStart;
    MILO_ASSERT(mSizeWords == startBlock->mSizeWords, 0x2C9);
    int newSize = mSizeWords - n;
    if (n == 0 || newSize < 8) {
        return nullptr;
    }
    mSizeWords = newSize;
    startBlock->mSizeWords = newSize;
    startBlock->mNext = nullptr;
    return mStart + mSizeWords;
}

int *Heap::Truncate(int *mem, int truncWords, int &outSize) {
    if (mem < mStart || mem >= mStart + mSizeWords) {
        return nullptr;
    }
    unsigned int header = ((unsigned int *)mem)[-1];
    AllocBlock *allocBlock = (AllocBlock *)(mem - 1);
    int newFreeWords = ((header >> 8) - 1 - (header & 0xFF)) - truncWords;
    MILO_ASSERT(newFreeWords >= 0, 0x49E);
    if (newFreeWords > 8) {
        FreeBlock *prev = nullptr;
        FreeBlock *next = nullptr;
        FindFreeNeighbors(allocBlock, prev, next);
        unsigned int timeStamp = gTimeStamp;
        FreeBlock *block = (FreeBlock *)(mem + truncWords);
        gTimeStamp = timeStamp + 1;
        InsertFreeBlock(block, newFreeWords, prev, next, timeStamp);
        if (mDebugLevel >= 1) {
            int *fillEnd = (int *)block + block->mSizeWords;
            int *fillStart = (int *)block + 3;
            for (int *p = fillStart; p < fillEnd; p++) {
                *p = 0xDEADDEAD;
            }
        }
        if (next != nullptr) {
            block->AttemptMerge(next, mDebugLevel);
        }
        unsigned int oldHeader = allocBlock->mHeader;
        allocBlock->mHeader =
            (oldHeader & 0xFF) | (((oldHeader >> 8) - newFreeWords) << 8);
    }
    outSize = allocBlock->mHeader >> 8;
    return mem;
}

bool Heap::Free(int *mem) {
    if (mem < mStart || mem >= mStart + mSizeWords) {
        return false;
    }
    FreeBlock *prev = nullptr;
    FreeBlock *next = nullptr;
    AllocBlock *allocBlock = (AllocBlock *)(mem - 1);
    FindFreeNeighbors(allocBlock, prev, next);
    unsigned int header = allocBlock->mHeader;
    unsigned int timeStamp = gTimeStamp;
    FreeBlock *block = (FreeBlock *)((int *)allocBlock - (header & 0xFF));
    gTimeStamp = timeStamp + 1;
    InsertFreeBlock(block, header >> 8, prev, next, timeStamp);
    if (mDebugLevel >= 1) {
        int *fillEnd = (int *)block + block->mSizeWords;
        int *fillStart = (int *)block + 3;
        for (int *p = fillStart; p < fillEnd; p++) {
            *p = 0xDEADDEAD;
        }
    }
    if (next != nullptr) {
        block->AttemptMerge(next, mDebugLevel);
    }
    if (prev != nullptr) {
        prev->AttemptMerge(block, mDebugLevel);
    }
    return true;
}

void Heap::BestFit(int sizeWords, int alignShift, FreeBlockInfo &info) {
    FreeBlock *block = mFreeBlockChain;
    int bestSize = 0x7FFFFFFF;
    int bestPad = 0x7FFFFFFF;
    int align = 1 << alignShift;
    FreeBlock *prev = nullptr;
    FreeBlock *bestBlock = nullptr;
    FreeBlock *bestPrev = nullptr;
    int largestSeen = 0;
    for (; block != nullptr; prev = block, block = block->mNext) {
        int blockSize = block->mSizeWords;
        int wordAddr = ((int)block >> 2) + 1;
        int padWords =
            ((((unsigned int)(wordAddr + align - 1)) >> alignShift) << alignShift) - wordAddr;
        bool fits = blockSize >= sizeWords + padWords;
        if (fits && blockSize < bestSize) {
            bestSize = blockSize;
            bestPad = padWords;
            bestBlock = block;
            bestPrev = prev;
            if (!mAllowTemp) {
                info.mBlock = block;
                info.mPrev = prev;
                info.mBlockSize = blockSize;
                info.mPadWords = padWords;
                if (blockSize == sizeWords + padWords) {
                    return;
                }
            }
        }
        if (mAllowTemp && blockSize > largestSeen) {
            info.mBlock = bestBlock;
            info.mPrev = bestPrev;
            info.mBlockSize = bestSize;
            info.mPadWords = bestPad;
            if (bestSize == sizeWords + bestPad) {
                return;
            }
            largestSeen = blockSize;
        }
    }
}

void Heap::BestLastFit(int sizeWords, int alignShift, FreeBlockInfo &info) {
    FreeBlock *prev = nullptr;
    FreeBlock *block = mFreeBlockChain;
    int bestSize = 0;
    if (!mAllowTemp) {
        bestSize = 0x7FFFFFFF;
    }
    if (block != nullptr) {
        int shift = alignShift + 2;
        int sizeBytes = sizeWords << 2;
        for (; block != nullptr; prev = block, block = block->mNext) {
            int blockSize = block->mSizeWords;
            int padWords =
                ((((((int)block + (blockSize << 2)) - sizeBytes) >> shift) << shift) - 4 -
                 (int)block) /
                4;
            if (padWords >= 0) {
                if (blockSize >= bestSize || blockSize <= info.mBlockSize) {
                    info.mBlockSize = blockSize;
                    info.mPadWords = padWords;
                    info.mBlock = block;
                    info.mPrev = prev;
                }
            }
            if (blockSize > bestSize) {
                bestSize = blockSize;
            }
        }
    }
}

void Heap::LRUFit(int sizeWords, int alignShift, FreeBlockInfo &info) {
    FreeBlock *block = mFreeBlockChain;
    FreeBlock *prev = nullptr;
    int bestTime = 0x7FFFFFFF;
    for (; block != nullptr; prev = block, block = block->mNext) {
        int blockSize = block->mSizeWords;
        int wordAddr = ((int)block >> 2) + 1;
        unsigned int timeStamp = block->mTimeStamp;
        int padWords =
            ((((unsigned int)(wordAddr + (1 << alignShift) - 1)) >> alignShift) << alignShift) -
            wordAddr;
        if (blockSize >= sizeWords + padWords && (int)timeStamp < bestTime) {
            info.mBlockSize = blockSize;
            info.mPadWords = padWords;
            info.mBlock = block;
            info.mPrev = prev;
            bestTime = timeStamp;
        }
    }
}

int MemFindHeap(const char *name) {
    for (int i = 0; i < gNumHeaps; i++) {
        if (strcmp(gHeaps[i].mName, name) == 0) {
            return i;
        }
    }
    if (gSingleHeap) {
        return 0;
    }
    MILO_ASSERT_FMT(gNumHeaps <= 0, "could not find heap %s", name);
    return -1;
}

void MemSetAllowTemp(char *name, bool allow) {
    int heap = MemFindHeap(name);
    if (heap >= 0) {
        gHeaps[heap].mAllowTemp = false;
    }
}

bool MemTempAllocationsEnabled() { return false; }

void Heap::FreeBlockStats(int &i1, int &i2, int &totalFree, int &biggest) {
    FreeBlock *block = mFreeBlockChain;
    int idx = 0;
    int maxBytes = 0;
    int sumBytes = 0;
    int maxIdx = -1;
    for (; block != nullptr; block = block->mNext) {
        int bytes = block->mSizeWords << 2;
        if (maxBytes < bytes) {
            maxBytes = bytes;
            maxIdx = idx;
        }
        sumBytes += bytes;
        idx++;
    }
    totalFree = sumBytes;
    biggest = maxBytes;
    mLargestFree = maxBytes;
    if (maxBytes < mMinLargest) {
        mMinLargest = maxBytes;
    }
    mNumFreeBytes = totalFree;
    if (totalFree < mBiggestFree) {
        mBiggestFree = totalFree;
    }
    i1 = maxIdx;
    i2 = (idx - maxIdx) - 1;
}

void Heap::MoreFreeBlockStats(int &i1, int &i2, int &i3, int &i4) {
    i1 = mNumFreeBytes;
    i3 = mBiggestFree;
    i2 = mLargestFree;
    i4 = mMinLargest;
}

void Heap::ResetMinFreeBlockStats() {
    mBiggestFree = mNumFreeBytes;
    mMinLargest = mLargestFree;
}

bool FreeBlock::AttemptMerge(FreeBlock *next, int debugLevel) {
    int thisSize = mSizeWords;
    if ((int *)this + thisSize == (int *)next) {
        unsigned int ts = mTimeStamp;
        if (ts < next->mTimeStamp) {
            ts = next->mTimeStamp;
        }
        int nextSize = next->mSizeWords;
        FreeBlock *nextNext = next->mNext;
        mNext = nextNext;
        mSizeWords = thisSize + nextSize;
        mTimeStamp = ts;
        if (debugLevel >= 1) {
            int *ptr = (int *)next;
            int *end = ptr + 3;
            while (ptr < end) {
                *ptr = 0xDEADDEAD;
                ptr++;
            }
        }
        return true;
    }
    return false;
}

void MemPushHeap(int iHeap) {
    MemHeapStack &s = ThreadMemStack(true);
    MILO_ASSERT(iHeap > kNoHeap && iHeap < gNumHeaps, 0x606);
    MILO_ASSERT(s.mSize + 1 < sizeof(s.mStack) / sizeof(s.mStack[0]), 0x607);
    s.mStack[s.mSize] = iHeap;
    s.mSize++;
}

void MemPopHeap() {
    MemHeapStack &s = ThreadMemStack(true);
    MILO_ASSERT(s.mSize > 0, 0x610);
    s.mSize--;
}

bool MemDoTempAllocations::enabled;

MemDoTempAllocations::MemDoTempAllocations(bool noTemp, bool reset) {
    CritSecTracker tracker(gMemLock);
    int heapNum = GetCurrentHeapNum();
    Heap *heap = heapNum > -1 ? &gHeaps[heapNum] : nullptr;
    if (heap != nullptr) {
        mOld = heap->mStrategy;
        if (reset) {
            heap->mStrategy = 0;
        } else {
            if (noTemp) {
                heap->mStrategy = 2;
            } else {
                heap->mStrategy = 1;
            }
            enabled = noTemp;
        }
    } else {
        mOld = -1;
    }
}

MemDoTempAllocations::~MemDoTempAllocations() {
    CritSecTracker tracker(gMemLock);
    if (mOld != -1) {
        int heapNum = GetCurrentHeapNum();
        Heap *heap = heapNum > -1 ? &gHeaps[heapNum] : nullptr;
        MILO_ASSERT(heap, 0x675);
        heap->mStrategy = mOld;
    }
    enabled = mOld == 2;
}

void MemMoreFreeBlockStats(int heapNum, int &a, int &b, int &c, int &d) {
    CritSecTracker tracker(gMemLock);
    MILO_ASSERT(heapNum < 0x10, 0x29b);
    gHeaps[heapNum].MoreFreeBlockStats(a, b, c, d);
}

void MemResetMinFreeBlockStats(int heapNum) {
    CritSecTracker tracker(gMemLock);
    MILO_ASSERT(heapNum < 0x10, 0x2a3);
    gHeaps[heapNum].ResetMinFreeBlockStats();
}

void *MemResizeElem(void *&mem, int &totalSize, void *cutPoint, int cutLength, int insertLength, const char *name) {
    void *old = mem;
    int suffixSize = 0;
    int prefixSize = (char *)cutPoint - (char *)mem;
    int newTotalSize = prefixSize;
    if (insertLength > -1) {
        suffixSize = (totalSize - prefixSize) - cutLength;
        newTotalSize = prefixSize + insertLength + suffixSize;
    }
    if (newTotalSize != totalSize) {
        mem = _MemAlloc(newTotalSize, 0);
        totalSize = newTotalSize;
        if (prefixSize != 0) {
            memcpy(mem, old, prefixSize);
        }
        if (suffixSize != 0) {
            memcpy((char *)mem + prefixSize + insertLength, (char *)cutPoint + cutLength, suffixSize);
        }
        _MemFree(old);
    }
    return (char *)mem + prefixSize;
}

// Global new/delete operators - direct branches to _MemAlloc/_MemFree.
void *operator new(size_t size) throw(std::bad_alloc) { return _MemAlloc(size, 0); }
void *operator new[](size_t size) throw(std::bad_alloc) { return _MemAlloc(size, 0); }
void operator delete(void *v) throw() { _MemFree(v); }
void operator delete[](void *v) throw() { _MemFree(v); }

void PublicMemFree(void *v) { _MemFree(v); }

// Pool/heap dispatcher allocators. Threshold: 0x80 for the regular variant,
// 0x100 for the STL variant.
void *_MemOrPoolAlloc(int size, PoolType type) {
    if (size == 0) return nullptr;
    if (size > 0x80) return _MemAlloc(size, 0);
    return _PoolAlloc(size, size, type);
}

void _MemOrPoolFree(int size, PoolType type, void *mem) {
    if (mem == nullptr) return;
    if (size > 0x80) {
        _MemFree(mem);
    } else {
        _PoolFree(size, type, mem);
    }
}

void *_MemOrPoolAllocSTL(int size, PoolType type) {
    if (size == 0) return nullptr;
    if (size > 0x100) return _MemAlloc(size, 0);
    return _PoolAlloc(size, size, type);
}

void _MemOrPoolFreeSTL(int size, PoolType type, void *mem) {
    if (mem == nullptr) return;
    if (size > 0x100) {
        _MemFree(mem);
    } else {
        _PoolFree(size, type, mem);
    }
}

// MemHandle - opaque handle for relocatable allocations. mAlloc points to a
// small header (MemHandleAlloc) followed by user data at +0x10.
MemHandle::MemHandle(void *alloc) {
    mAlloc = (MemHandleAlloc *)alloc;
    mAlloc->mBack = this;
    mAlloc->mLockCount = 0;
}

void *MemHandle::Lock() {
    ++mAlloc->mLockCount;
    return (char *)mAlloc + 0x10;
}

int GetFreeSystemMemory() {
    int low = 0;
    int high = 0x40000000;
    int mid;
    do {
        mid = (high + low) / 2;
        void *ptr = WiiMalloc(mid);
        if (ptr != nullptr) {
            low = mid;
            WiiFree(ptr);
        } else {
            high = mid;
        }
    } while (low + 1 < high);
    return low;
}

int MemAllocSize(void *mem) {
    CritSecTracker tracker(gMemLock);
    if (mem == nullptr) return 0;
    for (int i = 0; i < gNumHeaps; i++) {
        int size = gHeaps[i].AllocSize((int *)mem);
        if (size != 0) {
            return size;
        }
    }
    MILO_FAIL("Can't determine size of allocation.");
    return 0;
}

void MemResetMinFreeBlockStats(int);

DataNode ResetHWM(DataArray *) {
    for (int i = 0; i < gNumHeaps; i++) {
        MemResetMinFreeBlockStats(i);
    }
    return DataNode(0);
}

extern "C" void OSReport(const char *, ...);
extern volatile int gCheckConsistencyish;

DataNode CycleMemConsistencyCheck(DataArray *) {
    gCheckConsistencyish = gCheckConsistencyish - 1;
    if (gCheckConsistencyish < 0) {
        gCheckConsistencyish = gCheckConsistencyish + 8;
    }
    OSReport("gCheckConsistencyish %d", gCheckConsistencyish);
    return DataNode(0);
}
