#include "utl/MemMgr.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "obj/Data.h"
#include "decomp.h"
#include <cstring>

#define MAX_BUF_THREADS 0x10
#define MAX_HEAPS 0x10
#define DIM(a) (sizeof(a) / sizeof(a[0]))

extern "C" void *WiiMalloc(int);
extern "C" void WiiFree(void *);
int GetFreeSystemMemory();

struct OSThread;

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
    void CheckConsistency(const char *, int);
    void Init(const char *, int, int *, int, bool, Strategy, int, bool);
    int *Alloc(int, int, int &);
    const char *Name() const { return mName; }

    FreeBlock *mFreeBlockChain; // 0x0
    int *mStart;        // 0x4
    const char *mName;  // 0x8
    int mSizeWords;     // 0xC
    int mHeapNum;       // 0x10
    bool mUseHeapAlign; // 0x14
    char mPad15[3];     // 0x15
    int mDebugLevel;    // 0x18
    int mStrategy;      // 0x1C
    bool mAllowTemp;    // 0x20
    char mPad21[3];     // 0x21
    int mNumFreeBytes;  // 0x24
    int mBiggestFree;   // 0x28
    int mLargestFree;   // 0x2C
    int mMinLargest;    // 0x30
};

// -----------------------------------------------------------------------------
// Globals (declared in source-order to reproduce the binary's BSS layout)
// -----------------------------------------------------------------------------
// gDefaultHeap lives in .data at 0x80BBCEF0 in the target binary. We keep it
// as an extern here so MWCC doesn't allocate BSS space for it; the actual
// definition will be supplied when MemInit's static-init code is decompiled.
extern int gDefaultHeap;

namespace MemMgr {
    OSThread *gThreadIds[16];  // .bss:0x80D0D828
    int gCurThread;            // .bss:0x80D0D868
    int gNumThreads;           // .bss:0x80D0D86C
}

namespace {
    int kTinyHeap;             // .bss:0x80D0D870
    int kFastHeap;             // .bss:0x80D0D874
    unsigned int gTimeStamp;   // .bss:0x80D0D878
    bool gMemInited;           // .sbss:0x80C7A368
}

// Force MWCC to keep unreferenced anon-namespace ints. These are read by
// MemInit (currently unimplemented) so they would otherwise be stripped.
DECOMP_FORCEACTIVE(MemMgr, kTinyHeap, kFastHeap);

Heap gHeaps[16];           // .bss:0x80D0D880
int gNumHeaps;             // .bss:0x80D0DBC0
MemHeapStack gThreadBuf[16]; // .bss:0x80D0DBC8
MemHeapStack gNullMemStack;  // .bss:0x80D0E008
int gSingleHeap;           // .bss:0x80D0E04C
CriticalSection *gMemLock;    // .bss:0x80D0E050
CriticalSection *gMemStackLock; // .bss:0x80D0E054
static CriticalSection sMemLock;       // .bss:0x80D0E064 (preceded by guard @0x80D0E058)
static CriticalSection sMemStackLock;  // .bss:0x80D0E08C (preceded by guard @0x80D0E080)
Timer gMemAllocTimer;      // .bss:0x80D0E0B0

bool gInsideMemFunc;       // .sbss:0x80C7A369

const char *kMemAssertStr = "Heap: %s File: %s Line: %d Error: %s\n";
volatile int gCheckConsistencyish;

// Forward decls used in this TU.
namespace MemMgr {
    // Helper: always-true validator for thread ids. Inlines to a no-op,
    // so the search loop body collapses to just incrementing the counter.
    inline bool ValidateThreadId(OSThread *) { return true; }
}

MemHeapStack &ThreadMemStack(bool createIfMissing) {
    CriticalSection *lock = gMemStackLock;
    if (lock != nullptr) {
        lock->Enter();
    }
    if (MemMgr::gNumThreads == 0) {
        MemMgr::gThreadIds[0] = OSGetCurrentThread();
        MemMgr::gNumThreads = 1;
    } else {
        OSThread *current = OSGetCurrentThread();
        if (MemMgr::gThreadIds[MemMgr::gCurThread] != current) {
            int idx = 0;
            OSThread **slot = MemMgr::gThreadIds;
            while (idx < MemMgr::gNumThreads) {
                if (*slot == OSGetCurrentThread()) break;
                slot++;
                idx++;
            }
            if (createIfMissing == 0) {
                MemHeapStack &nullStack = gNullMemStack;
                if (lock != nullptr) {
                    lock->Exit();
                }
                return nullStack;
            }
            if (idx == MemMgr::gNumThreads) {
                int cur;
                for (cur = 0; cur < MemMgr::gNumThreads; cur++) {
                    if (!MemMgr::ValidateThreadId(MemMgr::gThreadIds[cur])) break;
                }
                if (cur == MemMgr::gNumThreads) {
                    MILO_ASSERT(MemMgr::gNumThreads < MAX_BUF_THREADS, 0x264);
                    MemMgr::gThreadIds[cur] = OSGetCurrentThread();
                    MemMgr::gNumThreads++;
                }
                idx = cur;
            }
            MemMgr::gCurThread = idx;
        }
    }
    MemHeapStack &result = gThreadBuf[MemMgr::gCurThread];
    if (lock != nullptr) {
        lock->Exit();
    }
    return result;
}

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
    MILO_ASSERT(heapNum < MAX_HEAPS, 0x293);
    gHeaps[heapNum].FreeBlockStats(a, b, c, d);
}

void MemMoreFreeBlockStats(int heapNum, int &a, int &b, int &c, int &d) {
    CritSecTracker tracker(gMemLock);
    MILO_ASSERT(heapNum < MAX_HEAPS, 0x29b);
    gHeaps[heapNum].MoreFreeBlockStats(a, b, c, d);
}

void MemResetMinFreeBlockStats(int heapNum) {
    CritSecTracker tracker(gMemLock);
    MILO_ASSERT(heapNum < MAX_HEAPS, 0x2a3);
    gHeaps[heapNum].ResetMinFreeBlockStats();
}

static int gPrevFree[MAX_HEAPS] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

void MemDelta(const char *name, int heapNum) {
    int numLargeFrags = 0;
    int numRightFrags = 0;
    int numFreeBytes = 0;
    int biggestFreeBlock = 0;
    MemFreeBlockStats(heapNum, numLargeFrags, numRightFrags, numFreeBytes, biggestFreeBlock);
    if (gPrevFree[heapNum] == -1) {
        gPrevFree[heapNum] = numFreeBytes;
    }
    int delta = gPrevFree[heapNum] - numFreeBytes;
    TheDebug << name << " lfrag:" << numLargeFrags << " rfrag:" << numRightFrags
             << " largest:" << numFreeBytes << " free:" << numFreeBytes
             << " delta:" << delta << "\n";
    gPrevFree[heapNum] = numFreeBytes;
}

void Heap::InsertFreeBlock(
    FreeBlock *iBlock, int sizeWords, FreeBlock *iPrevBlock, FreeBlock *iNextBlock, int timeStamp
) {
    MILO_ASSERT((iBlock != iPrevBlock) && (iBlock != iNextBlock), 0x300);
    iBlock->mSizeWords = sizeWords;
    iBlock->mNext = iNextBlock;
    iBlock->mTimeStamp = timeStamp;
    if (iPrevBlock != nullptr) {
        iPrevBlock->mNext = iBlock;
    } else {
        mFreeBlockChain = iBlock;
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

extern OSThread *gMainThreadID;

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


void Heap::Init(const char *name, int heapNum, int *start, int sizeWords,
                bool useHeapAlign, Strategy strategy, int debugLevel, bool allowTemp) {
    MILO_ASSERT_FMT(start, "Could not allocate %d bytes for heap %s\n", sizeWords * 4, name);
    int *aligned = (int *)(((unsigned int)((char *)start - 4)) & ~0xF);
    int *newStart = aligned + 4; // +0x10 bytes
    int padWords = newStart - start;
    int newSizeWords = sizeWords - padWords;
    mName = name;
    mHeapNum = heapNum;
    mUseHeapAlign = useHeapAlign;
    mStrategy = strategy;
    mDebugLevel = debugLevel;
    mAllowTemp = allowTemp;
    mSizeWords = newSizeWords;
    mStart = newStart;
    unsigned int ts = gTimeStamp;
    gTimeStamp = ts + 1;
    InsertFreeBlock((FreeBlock *)newStart, newSizeWords, nullptr, nullptr, ts);
    mNumFreeBytes = newSizeWords << 2;
    mBiggestFree = mNumFreeBytes;
    mLargestFree = mNumFreeBytes;
    mMinLargest = mNumFreeBytes;
    if (mDebugLevel >= 1) {
        FreeBlock *block = mFreeBlockChain;
        int *fillStart = (int *)block + 3;
        int *fillEnd = (int *)block + block->mSizeWords;
        for (int *p = fillStart; p < fillEnd; p++) {
            *p = 0xDEADDEAD;
        }
    }
}

int *Heap::SplitFromBack(int n) {
    FreeBlock *startBlock = (FreeBlock *)mStart;
    if (mSizeWords != startBlock->mSizeWords) {
        FormatString fs("can't split heap if it is in use.");
        TheDebug.Fail(fs.Str());
    }
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
    MILO_ASSERT(truncWords >= 0, 0x49E);
    unsigned int header = ((unsigned int *)mem)[-1];
    AllocBlock *allocBlock = (AllocBlock *)(mem - 1);
    int newFreeWords = ((header >> 8) - 1 - (header & 0xFF)) - truncWords;
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

int *Heap::Alloc(int sizeWords, int alignShift, int &actualSize) {
    FreeBlockInfo info;
    info.mBlock = nullptr;
    info.mPrev = nullptr;
    info.mBlockSize = 0x7FFFFFFF;
    info.mPadWords = 0x7FFFFFFF;
    switch (mStrategy) {
    case kFirstFit:
        FirstFit(sizeWords, alignShift, info);
        break;
    case kBestFit:
        BestFit(sizeWords, alignShift, info);
        break;
    case kBestLastFit:
        BestLastFit(sizeWords, alignShift, info);
        break;
    case kLRUFit:
        LRUFit(sizeWords, alignShift, info);
        break;
    case kLastFit:
        LastFit(sizeWords, alignShift, info);
        break;
    default:
        MILO_ASSERT(false, 0x432);
        break;
    }
    FreeBlock *block = info.mBlock;
    if (block == nullptr) {
        int leftFrag, rightFrag, totalFree, biggest;
        FreeBlockStats(leftFrag, rightFrag, totalFree, biggest);
        bool isMain = true;
        if (gMainThreadID != nullptr && gMainThreadID != OSGetCurrentThread()) {
            isMain = false;
        }
        if (!isMain) {
            gInsideMemFunc = false;
            gMemLock->Abandon();
        }
        return nullptr;
    }
    // Split off left padding if any
    if (info.mPadWords > 8) {
        FreeBlock *newBlock = (FreeBlock *)((int *)block + info.mPadWords);
        info.mBlock = newBlock;
        int newSize = info.mBlockSize - info.mPadWords;
        info.mBlockSize = newSize;
        newBlock->mSizeWords = newSize;
        newBlock->mNext = block->mNext;
        newBlock->mTimeStamp = block->mTimeStamp;
        InsertFreeBlock(block, info.mPadWords, info.mPrev, newBlock, newBlock->mTimeStamp);
        info.mPadWords = 0;
        info.mPrev = block;
    }
    int allocWords = sizeWords + info.mPadWords;
    int leftoverWords = info.mBlockSize - allocWords;
    if (leftoverWords > 8) {
        InsertFreeBlock(
            (FreeBlock *)((int *)info.mBlock + allocWords), leftoverWords, info.mPrev,
            info.mBlock->mNext, info.mBlock->mTimeStamp
        );
    } else {
        allocWords = info.mBlockSize;
        if (info.mPrev == nullptr) {
            mFreeBlockChain = info.mBlock->mNext;
        } else {
            info.mPrev->mNext = info.mBlock->mNext;
        }
    }
    int padW = info.mPadWords;
    AllocBlock *allocBlock = (AllocBlock *)((int *)info.mBlock + padW);
    unsigned int newHeader = allocBlock->mHeader;
    newHeader = (newHeader & ~0xFFu) | ((unsigned int)padW & 0xFFu);
    newHeader = (newHeader & 0xFFu) | ((unsigned int)allocWords << 8);
    allocBlock->mHeader = newHeader;
    int *zeroEnd = (int *)allocBlock;
    int *zeroStart = zeroEnd - (newHeader & 0xFF);
    while (zeroStart != zeroEnd) {
        *zeroStart = 0;
        zeroStart++;
    }
    if (mDebugLevel >= 1) {
        unsigned int hdr = allocBlock->mHeader;
        int *fillStart = (int *)allocBlock + 1;
        int *fillEnd = (int *)allocBlock - (hdr & 0xFF) + (hdr >> 8);
        if (fillStart < fillEnd) {
            int *p = fillStart;
            int *bulkEnd = fillEnd - 8;
            int byteSize = (int)((char *)fillEnd - (char *)p);
            int bytePlus3 = byteSize + 3;
            if ((bytePlus3 / 4) > 8) {
                bool valid = false;
                if (p <= fillEnd) {
                    bool ok = true;
                    if (!(byteSize & 0x80000000) && (bytePlus3 & 0x80000000)) {
                        ok = false;
                    }
                    if (ok) valid = true;
                }
                if (valid && p < bulkEnd) {
                    do {
                        p[0] = 0xABCDABCD; p[1] = 0xABCDABCD;
                        p[2] = 0xABCDABCD; p[3] = 0xABCDABCD;
                        p[4] = 0xABCDABCD; p[5] = 0xABCDABCD;
                        p[6] = 0xABCDABCD; p[7] = 0xABCDABCD;
                        p += 8;
                    } while (p < bulkEnd);
                }
            }
            if (p < fillEnd) {
                do {
                    *p = 0xABCDABCD;
                    p++;
                } while (p < fillEnd);
            }
        }
    }
    actualSize = allocBlock->mHeader >> 8;
    return (int *)allocBlock + 1;
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
    MILO_ASSERT(s.mSize + 1 < DIM(s.mStack), 0x607);
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

void MemHandle::Unlock() {
    MILO_ASSERT(mAlloc->mBack == this, 0xb1c);
    MILO_ASSERT(mAlloc->mLockCount > 0, 0xb1d);
    --mAlloc->mLockCount;
}

void MemFreeH(MemHandle *h) {
    // Only valid from main thread
    extern OSThread *gMainThreadID;
    MILO_ASSERT(gMainThreadID == nullptr || gMainThreadID == OSGetCurrentThread(), 0xb42);
    if (h != nullptr) {
        MILO_ASSERT(h->mAlloc->mLockCount == 0, 0xb47);
        _MemFree(h->mAlloc);
        _PoolFree(sizeof(MemHandle), MainPool, h);
    }
}

void *MemTruncate(void *mem, int newSize) {
    CritSecTracker tracker(gMemLock);
    if (mem == nullptr) return nullptr;
    if (newSize == 0) {
        _MemFree(mem);
        return nullptr;
    }
    int outWords;
    int sizeWords = (newSize + 3) >> 2;
    int i;
    for (i = 0; i < gNumHeaps; i++) {
        if (gHeaps[i].Truncate((int *)mem, sizeWords, outWords) != nullptr) break;
    }
    void *result = mem;
    if (i == gNumHeaps) {
        result = realloc(mem, newSize);
        outWords = sizeWords;
    }
    return result;
}

void *_MemRealloc(void *mem, int newSize, int align) {
    CritSecTracker tracker(gMemLock);
    if (gNumHeaps != 0) {
        int oldSize = MemAllocSize(mem);
        void *dst = _MemAlloc(newSize, align);
        int copySize = newSize < oldSize ? newSize : oldSize;
        memcpy(dst, mem, copySize);
        _MemFree(mem);
        return dst;
    }
    return realloc(mem, newSize);
}

int MemNumHeaps() { return gNumHeaps; }

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

const char *MemHeapName(int heap) {
    if (heap < 0) return "system";
    return gHeaps[heap].Name();
}

extern unsigned char *g_pRSOReserveBuf;

// On first request of exactly kRSOBufferSize (0x10EC00 bytes), return the
// pre-reserved RSO buffer. Subsequent calls fall through to _MemAlloc.
void *_MemAllocOrRSOBuf(int size, int align) {
    static bool RSOBufUsed = false;
    if (!RSOBufUsed && size == 0x10EC00) {
        RSOBufUsed = true;
        MILO_ASSERT(g_pRSOReserveBuf, 0x846);
        return g_pRSOReserveBuf;
    }
    return _MemAlloc(size, align);
}

extern "C" void *WiiAllocHeapAlign(int *, int, int);

void *_MemAllocTemp(int size, int align) {
    CritSecTracker tracker(gMemLock);
    MemDoTempAllocations temp(true, false);
    return _MemAlloc(size, align);
}

struct HeapDesc {
    const char *mName;
    int mKind;       // 1 = AddHeap, 2 = SplitHeap
    int mSizeBytes;
    int mDebugLevel;
    bool mUseHeapAlign;
    bool mAllowTemp;
    char mPad12[2];
    int mStrategy;
};

extern HeapDesc gHeapData[4];
extern char gZeroAllocBuf[0x20];
extern int gSysAllocs;
extern int gSysAllocBytes;

void AddHeap(const char *name, int heapNum, int sizeBytes, bool useHeapAlign, int region,
             Heap::Strategy strategy, int debugLevel, bool allowTemp);
void SplitHeap(int srcHeap, const char *name, int newHeapNum, int sizeBytes,
               bool useHeapAlign, Heap::Strategy strategy, int debugLevel, bool allowTemp);

void MemInit() {
    if (gMemInited) return;
    CriticalSection *lock = &sMemLock;
    gMemInited = true;
    gInsideMemFunc = true;
    gMemLock = lock;
    gMemStackLock = &sMemStackLock;
    if (lock != nullptr) lock->Enter();
    int *tmpBuf = (int *)WiiMalloc(0x10000);
    int numHeaps;
    if (gSingleHeap != 0) {
        numHeaps = 1;
    } else {
        numHeaps = 4;
    }
    gNumHeaps = numHeaps;
    MILO_ASSERT(gNumHeaps < MAX_HEAPS, 0x704);
    int totalCombinedBytes = 0;
    int singleHeapSplitAccum = 0;
    int idx = 3;
    HeapDesc *desc = &gHeapData[3];
    Heap *heap = &gHeaps[3]; // unused; placeholder for asm pattern
    do {
        const char *name = desc->mName;
        int sizeBytes = desc->mSizeBytes;
        bool useHeapAlign = desc->mUseHeapAlign;
        int kind = desc->mKind;
        bool allowTemp = desc->mAllowTemp;
        int debugLevel = desc->mDebugLevel;
        int strategy = desc->mStrategy;
        if (gSingleHeap != 0) {
            totalCombinedBytes += sizeBytes;
            if (idx == 0) {
                AddHeap(name, idx, totalCombinedBytes, useHeapAlign, kind,
                        (Heap::Strategy)strategy, debugLevel, allowTemp);
            }
        } else if (kind != 2) {
            if (idx == 0) {
                sizeBytes += singleHeapSplitAccum;
            }
            AddHeap(name, idx, sizeBytes, useHeapAlign, kind,
                    (Heap::Strategy)strategy, debugLevel, allowTemp);
            if (idx > 0) {
                singleHeapSplitAccum += sizeBytes - (gHeaps[idx].mSizeWords * 4);
            }
        }
        idx--;
        desc--;
        (void)heap;
    } while (idx >= 0);
    kTinyHeap = MemFindHeap("tiny");
    kFastHeap = MemFindHeap("fast");
    WiiFree(tmpBuf);
    gDefaultHeap = 0;
    int splitSrc = -1;
    int i = 0;
    HeapDesc *desc2 = &gHeapData[0];
    do {
        const char *name = desc2->mName;
        int sizeBytes = desc2->mSizeBytes;
        bool useHeapAlign = desc2->mUseHeapAlign;
        int kind = desc2->mKind;
        bool allowTemp = desc2->mAllowTemp;
        int debugLevel = desc2->mDebugLevel;
        int strategy = desc2->mStrategy;
        if (splitSrc < 0 && kind == 1) {
            splitSrc = i;
        }
        if (kind == 2) {
            if (splitSrc < 0) {
                FormatString fs("How can we not have a region 1 heap yet?\n");
                TheDebugFailer << fs.Str();
            }
            SplitHeap(splitSrc, name, i, sizeBytes, useHeapAlign,
                      (Heap::Strategy)strategy, debugLevel, allowTemp);
        }
        i++;
        desc2++;
    } while (i < 4);
    gInsideMemFunc = false;
    if (lock != nullptr) lock->Exit();
}

void *_MemAlloc(int iSizeBytes, int align) {
    if (!gMemInited) {
        MemInit();
    }
    AutoTimer autoTimer(&gMemAllocTimer, 0.0f, nullptr, nullptr);
    MILO_ASSERT(iSizeBytes >= 0, 0x86d);
    if (iSizeBytes == 0) {
        return gZeroAllocBuf;
    }
    CritSecTracker tracker(gMemLock);
    int currentHeap = GetCurrentHeapNum();
    if (kFastHeap != currentHeap) {
        int tinyHeap = kTinyHeap;
        if (tinyHeap != GetCurrentHeapNum() && iSizeBytes < 0x400) {
            MemPushHeap(tinyHeap);
            void *result = _MemAlloc(iSizeBytes, align);
            MemPopHeap();
            return result;
        }
    }
    int heapNum = GetCurrentHeapNum();
    Heap *heap = (heapNum > -1) ? &gHeaps[heapNum] : nullptr;
    MILO_ASSERT(heap, 0x89d);
    if (heap != nullptr && heap->mStrategy == 2 && heap->mAllowTemp == 0) {
        int pushCount = 0;
        MemHeapStack &stack = ThreadMemStack(true);
        while (heap != nullptr && stack.mSize > 0) {
            pushCount++;
            stack.mSize--;
            int h = GetCurrentHeapNum();
            heap = (h > -1) ? &gHeaps[h] : nullptr;
            if (heap->mAllowTemp != 0) break;
        }
        bool oldAllowTemp = heap->mAllowTemp;
        heap->mAllowTemp = true;
        int oldStrategy = heap->mStrategy;
        heap->mStrategy = 2;
        void *result = _MemAlloc(iSizeBytes, align);
        heap->mStrategy = oldStrategy;
        heap->mAllowTemp = oldAllowTemp;
        stack.mSize += pushCount;
        return result;
    }
    if (heap == nullptr) {
        void *result = WiiMalloc(iSizeBytes);
        gSysAllocBytes += iSizeBytes;
        gSysAllocs += 1;
        return result;
    }
    MILO_ASSERT(!gInsideMemFunc, 0x90a);
    gInsideMemFunc = true;
    int sizeWords = ((iSizeBytes + 3) >> 2) + 1;
    if ((unsigned int)sizeWords < 3) {
        sizeWords = 3;
    }
    int alignShift;
    if (align == 0) {
        alignShift = 0;
    } else {
        int bitCount = 0;
        int hasLowBit = 0;
        while (align > 1) {
            if (align & 1) {
                hasLowBit = 1;
            }
            align >>= 1;
            bitCount++;
        }
        int sum = bitCount + hasLowBit - 2;
        alignShift = sum & (-sum & ~sum) >> 31;
    }
    int actualSize;
    void *mem = heap->Alloc(sizeWords, alignShift, actualSize);
    if (mem == nullptr) {
        Heap *fastHeap = &gHeaps[kFastHeap];
        if (heap != fastHeap && iSizeBytes < 0x400) {
            int saved = fastHeap->mStrategy;
            fastHeap->mStrategy = heap->mStrategy;
            mem = fastHeap->Alloc(sizeWords, alignShift, actualSize);
            fastHeap->mStrategy = saved;
        }
        Heap *p = &gHeaps[0];
        Heap *base = p;
        while (mem == nullptr && p < base + gNumHeaps) {
            if (p != heap) {
                int saved = p->mStrategy;
                p->mStrategy = heap->mStrategy;
                mem = p->Alloc(sizeWords, alignShift, actualSize);
                p->mStrategy = saved;
            }
            p += 1;
        }
        if (mem == nullptr) {
            int leftFrag1 = 0, rightFrag1 = 0, biggest1 = 0, total1 = 0;
            int leftFrag2 = 0, rightFrag2 = 0, biggest2 = 0, total2 = 0;
            MemFreeBlockStats(0, leftFrag1, rightFrag1, biggest1, total1);
            MemFreeBlockStats(1, leftFrag2, rightFrag2, biggest2, total2);
            MILO_FAIL(
                "I tried all the heaps to get %d bytes, it was not available.\n"
                "main: free(%d) biggest(%d) lfrag(%d) rfrag(%d)\n"
                "fast: free(%d) biggest(%d) lfrag(%d) rfrag(%d)\n",
                iSizeBytes, biggest1, total1, leftFrag1, rightFrag1,
                biggest2, total2, leftFrag2, rightFrag2
            );
            return nullptr;
        }
    }
    void *allocated_mem = mem;
    gInsideMemFunc = false;
    MILO_ASSERT(allocated_mem, 0x98a);
    MILO_ASSERT(allocated_mem != (void *)0x01000000, 0x98b);
    return allocated_mem;
}

extern char gZeroAllocBuf[0x20];

extern OSThread *gMainThreadID;

MemHandle *_MemAllocH(int size) {
    bool isMain = true;
    if (gMainThreadID != nullptr && gMainThreadID != OSGetCurrentThread()) {
        isMain = false;
    }
    MILO_ASSERT(isMain, 0xb23);
    int heapNum = GetCurrentHeapNum();
    Heap *heap = (heapNum > -1) ? &gHeaps[heapNum] : nullptr;
    bool ok = (heap != nullptr) && heap->mUseHeapAlign;
    MILO_ASSERT(ok, 0xb27);
    // Allocate (size aligned up to 16 + 0x20 header)
    void *data = _MemAlloc(((size - 1) & ~0xF) + 0x20, 0x10);
    MemHandle *h = (MemHandle *)_PoolAlloc(sizeof(MemHandle), sizeof(MemHandle), MainPool);
    if (h != nullptr) {
        new (h) MemHandle(data);
    }
    return h;
}

void _MemFree(void *mem) {
    if (mem == nullptr) return;
    if (mem == gZeroAllocBuf) return;
    if (mem == g_pRSOReserveBuf) return;
    CritSecTracker tracker(gMemLock);
    MILO_ASSERT(!gInsideMemFunc, 0x9b7);
    gInsideMemFunc = true;
    int i;
    for (i = 0; i < gNumHeaps; i++) {
        auto _tmp0 = gHeaps[i].Free((int *)mem);
        if (_tmp0) break;
    }
    if (i == gNumHeaps) {
        WiiFree(mem);
    }
    gInsideMemFunc = false;
}


void AddHeap(const char *name, int heapNum, int sizeBytes, bool useHeapAlign, int region,
             Heap::Strategy strategy, int debugLevel, bool allowTemp) {
    int actualSize = sizeBytes;
    int *raw_mem = (int *)WiiAllocHeapAlign(&actualSize, region, useHeapAlign ? 0x20 : -0x20);
    if (raw_mem == nullptr) {
        int available = GetFreeSystemMemory();
        raw_mem = (int *)WiiMalloc(available);
        MILO_ASSERT(raw_mem, 0x7f5);
        if (available < actualSize) {
            OSReport("not enough memory for heap:%s. Wanted:%d. Got:%d\n", name, sizeBytes, available);
        }
        actualSize = available;
    }
    gHeaps[heapNum].Init(name, heapNum, raw_mem, actualSize >> 2, useHeapAlign, strategy,
                         debugLevel, allowTemp);
}

void SplitHeap(int srcHeap, const char *name, int newHeapNum, int sizeBytes,
               bool useHeapAlign, Heap::Strategy strategy, int debugLevel, bool allowTemp) {
    int sizeWords = sizeBytes >> 2;
    int *mem = gHeaps[srcHeap].SplitFromBack(sizeWords);
    if (!mem) {
        FormatString fs("Can't split that much (or that little) off this heap.");
        TheDebug.Fail(fs.Str());
    }
    gHeaps[newHeapNum].Init(name, newHeapNum, mem, sizeWords, useHeapAlign, strategy,
                            debugLevel, allowTemp);
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

DataNode CycleMemConsistencyCheck(DataArray *) {
    gCheckConsistencyish = gCheckConsistencyish - 1;
    if (gCheckConsistencyish < 0) {
        gCheckConsistencyish = gCheckConsistencyish + 8;
    }
    OSReport("gCheckConsistencyish %d", gCheckConsistencyish);
    return DataNode(0);
}

void MemCheckConsistency(const char *file, int line) {
    CritSecTracker tracker(gMemLock);
    for (int i = 0; i < gNumHeaps; i++) {
        gHeaps[i].CheckConsistency(file, line);
    }
}

#define HEAP_ASSERT(cond, msg)                                                           \
    ((cond) ||                                                                           \
     (TheDebugFailer << MakeString(kMemAssertStr, mName, file, line, msg), 0))

void Heap::CheckConsistency(const char *file, int line) {
    int *p = mStart;
    FreeBlock *freeBlock = mFreeBlockChain;
    int *heapEnd = mStart + mSizeWords;
    while (p < heapEnd) {
        int blockWords;
        if (freeBlock != nullptr && p == (int *)freeBlock) {
            FreeBlock *nextFree = freeBlock->mNext;
            blockWords = freeBlock->mSizeWords;
            if (nextFree != nullptr) {
                HEAP_ASSERT((int *)nextFree < heapEnd, "nextBlock->StartAddr() < heapEnd");
                HEAP_ASSERT(
                    nextFree >= (FreeBlock *)((int *)freeBlock + freeBlock->mSizeWords),
                    "nextBlock->StartAddr() >= freeBlock->EndAddr()"
                );
            }
            if (mDebugLevel >= 2) {
                int checksum = 0;
                int *pp = (int *)freeBlock + 3;
                int *blockEnd = (int *)freeBlock + freeBlock->mSizeWords;
                if (pp < blockEnd) {
                    for (; pp < blockEnd; pp++) {
                        checksum ^= *pp ^ 0xDEADDEAD;
                    }
                }
                HEAP_ASSERT(checksum == 0, "checksum == 0");
            }
            freeBlock = nextFree;
        } else {
            int *q = p;
            unsigned int header;
            do {
                header = *q;
                q++;
            } while (header == 0);
            blockWords = header >> 8;
        }
        HEAP_ASSERT(blockWords >= 0, "sizeWords >= 0");
        HEAP_ASSERT(blockWords <= mSizeWords, "sizeWords <= mSizeWords");
        p += blockWords;
    }
}

// ChunkAllocator pool table used by MemPrintOverview.
class ChunkAllocator;
extern ChunkAllocator *gChunkAlloc[2];

// Forward declarations for MakeString templates used here. Including
// MakeString.h would drag in Symbol.h; only need the templates.
#include "utl/MakeString.h"
#include "utl/TextStream.h"

void MemPrintOverview(int heapIdx, TextStream &stream) {
    int totalFree = 0;
    int minTotalFree = 0;
    int i;
    for (i = 0; i < gNumHeaps; i++) {
        if (heapIdx == kNoHeap || heapIdx == i) {
            int numFreeBytes, leftFrag, rightFrag, biggestFree;
            int mNumFreeBytes, mLargestFree, mBiggestFree, mMinLargest;
            MemFreeBlockStats(i, leftFrag, rightFrag, numFreeBytes, biggestFree);
            MemMoreFreeBlockStats(i, mNumFreeBytes, mLargestFree, mBiggestFree, mMinLargest);
            stream << MakeString(
                " [%5s] free:%5d big:%5d lfrag:%4d rfrag:%3d minfree:%5d minbig:%5d size:%6d\n",
                MemHeapName(i), numFreeBytes >> 10, biggestFree >> 10, leftFrag, rightFrag,
                mBiggestFree >> 10, mMinLargest >> 10, gHeaps[i].mSizeWords >> 8
            );
            totalFree += numFreeBytes;
            minTotalFree += mBiggestFree;
        }
    }
    MILO_ASSERT(minTotalFree <= totalFree, 0xAC8);
    stream << MakeString(
        "totalFree: %7d minTotalFree: %7d, totalFree(64): %7d minTotalFree(64): %7d\n",
        totalFree >> 10, minTotalFree >> 10,
        (totalFree >> 10) - 0x10000, (minTotalFree >> 10) - 0x10000
    );
    for (i = 0; i < 2; i++) {
        ChunkAllocator *ca = gChunkAlloc[i];
        if (ca != nullptr) {
            stream << MakeString(
                " [%5s pool] free:%7d TotalChunksSize:%7d NumHunks:%d\n",
                MemHeapName(*(int *)((char *)ca + 0x201c)),
                (int)(*(int **)((char *)ca + 0x2014) - *(int **)((char *)ca + 0x2018)) >> 8,
                *(int *)((char *)ca + 0x8) >> 10,
                *(int *)((char *)ca + 0x2010)
            );
        }
    }
}
