#include "utl/MemMgr.h"
#include "os/Debug.h"
#include <cstring>

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

    void FreeBlockStats(int &, int &, int &, int &);
    void MoreFreeBlockStats(int &, int &, int &, int &);
    void ResetMinFreeBlockStats();
    void InsertFreeBlock(FreeBlock *, int, FreeBlock *, FreeBlock *, int);
    int AllocSize(int *);
    void FindFreeNeighbors(AllocBlock *, FreeBlock *&, FreeBlock *&);
    const char *Name() const { return mName; }

    FreeBlock *mFreeBlockChain; // 0x0
    int *mStart;        // 0x4
    const char *mName;  // 0x8
    int mSizeWords;     // 0xC
    char mPad10[0xC];   // 0x10
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
