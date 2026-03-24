#include "utl/MemMgr.h"

struct MemHeapStack {
    int mStack[16]; // 0x0
    int mSize;      // 0x40
};

extern int gDefaultHeap;
static bool gMemInited;

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
