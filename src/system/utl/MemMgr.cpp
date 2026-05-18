#include "utl/MemMgr.h"
#include "os/Debug.h"
#include <cstring>

struct MemHeapStack {
    int mStack[16]; // 0x0
    int mSize;      // 0x40
};

class Heap {
public:
    void FreeBlockStats(int &, int &, int &, int &);
    const char *Name() const { return mName; }

    char mPad0[8];      // 0x0
    const char *mName;  // 0x8
    char mPadC[0x28];   // 0xC, total size 0x34
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
