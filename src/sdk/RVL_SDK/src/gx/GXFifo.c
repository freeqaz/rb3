#include <revolution/GX.h>
#include <revolution/base/PPCArch.h>
#include <revolution/os/OSContext.h>
#include <revolution/os/OSError.h>
#include <revolution/os/OSInterrupt.h>
#include <revolution/os/OSThread.h>
#include <string.h>

/* CP register pointer (set up by GXInit) — u16 mapped */
extern volatile u16 *__cpReg;
/* PI register pointer (set up by GXInit) — u32 mapped */
extern volatile u32 *__piReg;

/* CP status/control register indices (u16 array, byte offset = index*2) */
#define CP_STAT      0   /* __cpReg[0]  = byte 0x0  */
#define CP_CTRL      1   /* __cpReg[1]  = byte 0x2  */
#define CP_CLR       2   /* __cpReg[2]  = byte 0x4  */
/* CP FIFO configuration registers */
#define CP_FIFO_BASE_L  0x10  /* byte 0x20 */
#define CP_FIFO_BASE_H  0x11  /* byte 0x22 */
#define CP_FIFO_END_L   0x12  /* byte 0x24 */
#define CP_FIFO_END_H   0x13  /* byte 0x26 */
#define CP_FIFO_HI_L    0x14  /* byte 0x28 (hi watermark) */
#define CP_FIFO_HI_H    0x15  /* byte 0x2A */
#define CP_FIFO_LO_L    0x16  /* byte 0x2C (lo watermark) */
#define CP_FIFO_LO_H    0x17  /* byte 0x2E */
#define CP_FIFO_CNT_L   0x18  /* byte 0x30 (fifo count) */
#define CP_FIFO_CNT_H   0x19  /* byte 0x32 */
#define CP_FIFO_WP_L    0x1A  /* byte 0x34 (write ptr) */
#define CP_FIFO_WP_H    0x1B  /* byte 0x36 */
#define CP_FIFO_RP_L    0x1C  /* byte 0x38 (read ptr) */
#define CP_FIFO_RP_H    0x1D  /* byte 0x3A */
#define CP_FIFO_BP_L    0x1E  /* byte 0x3C (break pt) */
#define CP_FIFO_BP_H    0x1F  /* byte 0x3E */

/* PI FIFO register offsets (u32 array) */
#define PI_FIFO_BASE 3   /* __piReg[3]  = 0xC  */
#define PI_FIFO_END  4   /* __piReg[4]  = 0x10 */
#define PI_FIFO_WP   5   /* __piReg[5]  = 0x14 */

/* CP ctrl bits */
#define CP_CTRL_GPRDEN    (1 << 0)
#define CP_CTRL_CPULINKED (1 << 2)
#define CP_CTRL_GPLINKED  (1 << 3)
#define CP_CTRL_BP        (1 << 4)  /* breakpoint enable */
#define CP_CTRL_OVFEN     (1 << 1)  /* overflow interrupt enable */
#define CP_CTRL_BPINT     (1 << 5)  /* bp interrupt enable */

/* fifo objects */
GXFifoObjImpl CPUFifo;
GXFifoObjImpl GPFifo;

u8 CPUFifoReady;
u8 GPFifoReady;
u32 __GXOverflowCount;
s32 __GXCurrentBP;
void (*BreakPointCB)(void);
s32 GXOverflowSuspendInProgress;
OSThread *__GXCurrentThread;
u8 CPGPLinked;

void GXCPInterruptHandler(s16 arg0, OSContext *ctx) {
    OSContext sp8;
    u32 temp;

    gxdt->cpStatReg = (u32)__cpReg[CP_STAT];
    /* overflow interrupt handling: underflow bit in cpCtrlReg */
    if ((gxdt->cpCtrlReg >> 3) & 1) {
        if ((gxdt->cpStatReg >> 1) & 1) {
            OSResumeThread(__GXCurrentThread);
            GXOverflowSuspendInProgress = 0;
            temp = gxdt->cpClrReg | 3;
            gxdt->cpClrReg = temp;
            __cpReg[CP_CLR] = (u16)temp;
            temp = (gxdt->cpCtrlReg | 4) & ~8;
            gxdt->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
        }
    }
    if ((gxdt->cpCtrlReg >> 2) & 1) {
        if (gxdt->cpStatReg & 1) {
            __GXOverflowCount++;
            temp = (gxdt->cpCtrlReg & ~4) | 8;
            gxdt->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
            temp = (gxdt->cpClrReg | 1) & ~2;
            gxdt->cpClrReg = temp;
            __cpReg[CP_CLR] = (u16)temp;
            GXOverflowSuspendInProgress = 1;
            OSSuspendThread(__GXCurrentThread);
        }
    }
    temp = gxdt->cpCtrlReg;
    if ((temp >> 5) & 1) {
        if ((gxdt->cpStatReg >> 4) & 1) {
            temp = temp & ~0x20;
            gxdt->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
            if (BreakPointCB != NULL) {
                OSClearContext(&sp8);
                OSSetCurrentContext(&sp8);
                BreakPointCB();
                OSClearContext(&sp8);
                OSSetCurrentContext(ctx);
            }
        }
    }
}

void GXInitFifoBase(GXFifoObj *obj, void *base, u32 size) {
    GXFifoObjImpl *fifo = (GXFifoObjImpl *)obj;
    u32 hiWatermark = size - 0x4000;
    void *end = (void *)((u8 *)base + size - 4);
    u32 loWatermark = (size >> 1) & ~0x1F;
    s32 count = 0;
    BOOL intrStatus;

    fifo->base = base;
    fifo->end = end;
    fifo->size = size;
    fifo->count = (u32)count;
    fifo->hiWatermark = (void *)hiWatermark;
    fifo->loWatermark = (void *)loWatermark;
    intrStatus = OSDisableInterrupts();
    fifo->readPtr = base;
    fifo->writePtr = base;
    fifo->count = (u32)count;
    if (count < 0) {
        fifo->count = fifo->size;
    }
    OSRestoreInterrupts(intrStatus);
}

u8 CPGPLinkCheck(void) {
    u32 match = 0;
    s32 diff1;
    s32 diff2;
    s32 overlap;

    if (!CPUFifoReady || !GPFifoReady) {
        return 0;
    }
    if (CPUFifo.base == GPFifo.base) {
        match = 1;
    }
    if (CPUFifo.end == GPFifo.end) {
        match += 1;
    }
    if (match == 2) {
        return 1;
    }
    diff1 = (s32)CPUFifo.end - (s32)GPFifo.base;
    diff2 = (s32)GPFifo.end - (s32)CPUFifo.base;
    overlap = 0;
    if ((diff1 > 0 && diff2 > 0) || (diff1 < 0 && diff2 < 0)) {
        overlap = 1;
    }
    if (overlap != 0) {
        OSReport("CPUFifo: %08X - %08X\n");
        OSReport("GP Fifo: %08X - %08X\n", GPFifo.base, GPFifo.end);
    }
    return 0;
}

void GXSetCPUFifo(GXFifoObj *obj) {
    BOOL intrStatus;
    u32 temp;

    intrStatus = OSDisableInterrupts();
    if (obj == NULL) {
        CPUFifoReady = 0;
        CPGPLinked = 0;
        CPUFifo.linked = 0;
        CPUFifo.active = 0;
        OSRestoreInterrupts(intrStatus);
        return;
    }
    {
        GXFifoObjImpl *src = (GXFifoObjImpl *)obj;
        *(u32 *)&CPUFifo.wrap = *(u32 *)&src->wrap;
        void *base  = src->base;
        void *end   = src->end;
        u32   size  = src->size;
        void *hi    = src->hiWatermark;
        void *lo    = src->loWatermark;
        void *rp    = src->readPtr;
        void *wrp   = src->writePtr;
        u32   cnt   = src->count;
        CPUFifo.base        = base;
        CPUFifo.end         = end;
        CPUFifo.size        = size;
        CPUFifo.hiWatermark = hi;
        CPUFifo.loWatermark = lo;
        CPUFifo.readPtr     = rp;
        CPUFifo.writePtr    = wrp;
        CPUFifo.count       = cnt;
        CPUFifoReady = 1;
        CPUFifo.active = 1;
        if (CPGPLinkCheck()) {
            u32 wp = 0;
            CPGPLinked = 1;
            __piReg[PI_FIFO_BASE] = (u32)CPUFifo.base & 0x3FFFFFFF;
            CPUFifo.linked = 1;
            __piReg[PI_FIFO_END] = (u32)CPUFifo.end & 0x3FFFFFFF;
            wp = (u32)CPUFifo.writePtr & 0x1FFFFFE0;
            __piReg[PI_FIFO_WP] = wp;
            temp = gxdt->cpClrReg | 3;
            gxdt->cpClrReg = temp;
            __cpReg[CP_CLR] = (u16)temp;
            temp = (gxdt->cpCtrlReg | 4) & ~8;
            gxdt->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
            temp = gxdt->cpCtrlReg | 0x10;
            gxdt->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
        } else {
            u8 wasLinked = CPGPLinked;
            u32 wp = 0;
            CPUFifo.linked = 0;
            if (wasLinked) {
                temp = gxdt->cpCtrlReg & ~0x10;
                gxdt->cpCtrlReg = temp;
                __cpReg[CP_CTRL] = (u16)temp;
                CPGPLinked = 0;
            }
            temp = gxdt->cpCtrlReg & ~0xC;
            gxdt->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
            __piReg[PI_FIFO_BASE] = (u32)CPUFifo.base & 0x3FFFFFFF;
            __piReg[PI_FIFO_END]  = (u32)CPUFifo.end  & 0x3FFFFFFF;
            wp = (u32)CPUFifo.writePtr & 0x1FFFFFE0;
            __piReg[PI_FIFO_WP]   = wp;
        }
    }
    PPCSync();
    OSRestoreInterrupts(intrStatus);
}

void GXSetGPFifo(GXFifoObj *obj) {
    BOOL intrStatus;
    GXData *gx;
    u32 temp;

    intrStatus = OSDisableInterrupts();
    gx = gxdt;
    temp = gx->cpCtrlReg & ~1;
    gx->cpCtrlReg = temp;
    __cpReg[CP_CTRL] = (u16)temp;
    temp = gx->cpCtrlReg & ~0xC;
    gx->cpCtrlReg = temp;
    __cpReg[CP_CTRL] = (u16)temp;
    if (obj == NULL) {
        GXFifoObjImpl *gp = &GPFifo;
        GPFifoReady = 0;
        CPGPLinked = 0;
        temp = gx->cpCtrlReg & ~0x10;
        gx->cpCtrlReg = temp;
        __cpReg[CP_CTRL] = (u16)temp;
        gp->active = 0;
        gp->linked = 0;
        OSRestoreInterrupts(intrStatus);
        return;
    }
    {
        GXFifoObjImpl *src = (GXFifoObjImpl *)obj;
        GXFifoObjImpl *gp = &GPFifo;
        *(u32 *)&gp->wrap = *(u32 *)&src->wrap;
        void *end    = src->end;
        u32   size   = src->size;
        void *hi     = src->hiWatermark;
        void *lo     = src->loWatermark;
        void *rp     = src->readPtr;
        void *wp     = src->writePtr;
        u32   cnt    = src->count;
        void *base   = src->base;
        gp->end         = end;
        gp->size        = size;
        gp->hiWatermark = hi;
        gp->loWatermark = lo;
        gp->readPtr     = rp;
        gp->writePtr    = wp;
        gp->count       = cnt;
        GPFifoReady = 1;
        gp->linked = 1;
        gp->base = base;
        __cpReg[CP_FIFO_BASE_L] = (u16)(u32)gp->base;
        __cpReg[CP_FIFO_END_L]  = (u16)(u32)gp->end;
        __cpReg[CP_FIFO_CNT_L]  = (u16)gp->count;
        __cpReg[CP_FIFO_WP_L]   = (u16)(u32)gp->writePtr;
        __cpReg[CP_FIFO_RP_L]   = (u16)(u32)gp->readPtr;
        __cpReg[CP_FIFO_HI_L]   = (u16)(u32)gp->hiWatermark;
        __cpReg[CP_FIFO_LO_L]   = (u16)(u32)gp->loWatermark;
        __cpReg[CP_FIFO_BASE_H] = (u16)(((u32)gp->base >> 16) & 0x3FFF);
        __cpReg[CP_FIFO_END_H]  = (u16)(((u32)gp->end >> 16) & 0x3FFF);
        __cpReg[CP_FIFO_CNT_H]  = (u16)((s32)gp->count >> 16);
        __cpReg[CP_FIFO_WP_H]   = (u16)(((u32)gp->writePtr >> 16) & 0x3FFF);
        __cpReg[CP_FIFO_RP_H]   = (u16)(((u32)gp->readPtr >> 16) & 0x3FFF);
        __cpReg[CP_FIFO_HI_H]   = (u16)((u32)gp->hiWatermark >> 16);
        __cpReg[CP_FIFO_LO_H]   = (u16)((u32)gp->loWatermark >> 16);
        PPCSync();
        if (CPGPLinkCheck()) {
            CPGPLinked = 1;
            gp->active = 1;
            temp = (gx->cpCtrlReg | 4) & ~8;
            gx->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
            temp = gx->cpCtrlReg | 0x10;
            gx->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
        } else {
            CPGPLinked = 0;
            gp->active = 0;
            temp = gx->cpCtrlReg & ~0xC;
            gx->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
            temp = gx->cpCtrlReg & ~0x10;
            gx->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
        }
        {
            GXData *gxd2 = gxdt;
            temp = gx->cpCtrlReg & ~2 & ~0x20;
            __cpReg[CP_CTRL] = (u16)temp;
            __cpReg[CP_CTRL] = (u16)gxd2->cpCtrlReg;
            temp = gxd2->cpClrReg | 3;
            gxd2->cpClrReg = temp;
            __cpReg[CP_CLR] = (u16)temp;
            temp = gxd2->cpCtrlReg | 1;
            gxd2->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
        }
    }
    OSRestoreInterrupts(intrStatus);
}

void __GXSaveFifo(void) {
    BOOL intrStatus;

    intrStatus = OSDisableInterrupts();
    if (CPUFifoReady) {
        u32 wp = __piReg[PI_FIFO_WP];
        u32 *cpuf = (u32 *)&CPUFifo;
        cpuf[6] = (wp & 0x1FFFFFE0) + 0x80000000U;
        CPUFifo.wrap = (u8)((wp >> 29) & 1);
    }
    if (GPFifoReady) {
        u32 rp_hi, rp_lo, rp;
        u32 cnt_hi, cnt_lo;
        u32 *gpf = (u32 *)&GPFifo;
        rp_hi = (u32)__cpReg[CP_FIFO_RP_H];
        rp_lo = (u32)__cpReg[CP_FIFO_RP_L];
        rp = (rp_hi << 16) | rp_lo;
        gpf[5] = rp + 0x80000000U;
        cnt_hi = (u32)__cpReg[CP_FIFO_CNT_H];
        cnt_lo = (u32)__cpReg[CP_FIFO_CNT_L];
        gpf[7] = (cnt_hi << 16) | cnt_lo;
    }
    if (CPGPLinked) {
        CPUFifo.readPtr = GPFifo.readPtr;
        CPUFifo.count = GPFifo.count;
        GPFifo.writePtr = CPUFifo.writePtr;
        GPFifo.wrap = CPUFifo.wrap;
    } else if (CPUFifoReady) {
        s32 diff = (s32)CPUFifo.writePtr - (s32)CPUFifo.readPtr;
        CPUFifo.count = diff;
        if (diff < 0) {
            CPUFifo.count = (u32)(diff + (s32)CPUFifo.size);
        }
    }
    OSRestoreInterrupts(intrStatus);
}

u8 __GXIsGPFifoReady(void) {
    return GPFifoReady;
}

u8 GXIsCPUGPFifoLinked(void) {
    return CPGPLinked;
}

void GXGetGPStatus(u8 *overflowed, u8 *underflowed, u8 *readIdle, u8 *cmdIdle, u8 *brkpt) {
    u16 stat;

    stat = *__cpReg;
    gxdt->cpStatReg = (u32)stat;
    *overflowed = stat & 1;
    *underflowed = ((u32)gxdt->cpStatReg >> 1) & 1;
    *readIdle = ((u32)gxdt->cpStatReg >> 2) & 1;
    *cmdIdle = ((u32)gxdt->cpStatReg >> 3) & 1;
    *brkpt = ((u32)gxdt->cpStatReg >> 4) & 1;
}

BOOL GXGetCPUFifo(GXFifoObj *obj) {
    GXFifoObjImpl *dst;
    u32 *src;
    u32 a, b;

    if (!CPUFifoReady) {
        return FALSE;
    }
    __GXSaveFifo();
    dst = (GXFifoObjImpl *)obj;
    src = (u32 *)&CPUFifo;
    a = src[0]; /* base */
    b = src[1]; /* end */
    dst->end = (void *)b;
    dst->base = (void *)a;
    a = src[2]; /* size */
    b = src[3]; /* hi */
    dst->hiWatermark = (void *)b;
    dst->size = a;
    a = src[4]; /* lo */
    b = src[5]; /* readPtr */
    dst->readPtr = (void *)b;
    dst->loWatermark = (void *)a;
    a = src[6]; /* writePtr */
    b = src[7]; /* count */
    dst->count = b;
    dst->writePtr = (void *)a;
    *(u32 *)&dst->wrap = src[8]; /* wrap+active+linked+pad */
    return TRUE;
}

void GXGetFifoPtrs(GXFifoObj *obj, void **readPtr, void **writePtr) {
    GXFifoObjImpl *fifo = (GXFifoObjImpl *)obj;
    *readPtr = fifo->readPtr;
    *writePtr = fifo->writePtr;
}

void *GXGetFifoBase(GXFifoObj *obj) {
    GXFifoObjImpl *fifo = (GXFifoObjImpl *)obj;
    return fifo->base;
}

u32 GXGetFifoSize(GXFifoObj *obj) {
    GXFifoObjImpl *fifo = (GXFifoObjImpl *)obj;
    return fifo->size;
}

void GXGetFifoLimits(GXFifoObj *obj, u32 *hiWatermark, u32 *loWatermark) {
    GXFifoObjImpl *fifo = (GXFifoObjImpl *)obj;
    *hiWatermark = (u32)fifo->hiWatermark;
    *loWatermark = (u32)fifo->loWatermark;
}

u32 GXGetFifoCount(GXFifoObj *obj) {
    GXFifoObjImpl *fifo = (GXFifoObjImpl *)obj;
    return fifo->count;
}

u8 GXGetFifoWrap(GXFifoObj *obj) {
    GXFifoObjImpl *fifo = (GXFifoObjImpl *)obj;
    return fifo->wrap;
}

GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback cb) {
    GXBreakPtCallback old;
    BOOL intrStatus;

    old = BreakPointCB;
    intrStatus = OSDisableInterrupts();
    BreakPointCB = cb;
    OSRestoreInterrupts(intrStatus);
    return old;
}

void GXEnableBreakPt(void *breakPt) {
    BOOL intrStatus;
    u32 temp;

    intrStatus = OSDisableInterrupts();
    temp = gxdt->cpCtrlReg & ~1;
    gxdt->cpCtrlReg = temp;
    __cpReg[CP_CTRL] = (u16)temp;
    __cpReg[CP_FIFO_BP_L] = (u16)(u32)breakPt;
    __cpReg[CP_FIFO_BP_H] = (u16)(((u32)breakPt >> 16) & 0x3FFF);
    temp = gxdt->cpCtrlReg & ~2 & ~0x20;
    gxdt->cpCtrlReg = temp;
    __cpReg[CP_CTRL] = (u16)temp;
    temp = gxdt->cpCtrlReg | 0x22;
    gxdt->cpCtrlReg = temp;
    __cpReg[CP_CTRL] = (u16)temp;
    __GXCurrentBP = (s32)breakPt;
    temp = gxdt->cpCtrlReg | 1;
    gxdt->cpCtrlReg = temp;
    __cpReg[CP_CTRL] = (u16)temp;
    OSRestoreInterrupts(intrStatus);
}

void GXDisableBreakPt(void) {
    BOOL intrStatus;
    u32 temp;

    intrStatus = OSDisableInterrupts();
    temp = gxdt->cpCtrlReg & ~2 & ~0x20;
    gxdt->cpCtrlReg = temp;
    __cpReg[CP_CTRL] = (u16)temp;
    __GXCurrentBP = 0;
    OSRestoreInterrupts(intrStatus);
}

void __GXFifoInit(void) {
    __OSSetInterruptHandler(0x11, (OSInterruptHandler)GXCPInterruptHandler);
    __OSUnmaskInterrupts(0x4000);
    __GXCurrentThread = OSGetCurrentThread();
    GXOverflowSuspendInProgress = 0;
    memset(&CPUFifo, 0, 0x24);
    memset(&GPFifo, 0, 0x24);
    CPUFifoReady = 0;
    GPFifoReady = 0;
}

void __GXCleanGPFifo(void) {
    BOOL intrStatus;
    u32 temp;

    if (!GPFifoReady) {
        return;
    }
    intrStatus = OSDisableInterrupts();
    {
        GXData *gx = gxdt;
        temp = gx->cpCtrlReg & ~1;
        gx->cpCtrlReg = temp;
        __cpReg[CP_CTRL] = (u16)temp;
        temp = gx->cpCtrlReg & ~0xC;
        gx->cpCtrlReg = temp;
        __cpReg[CP_CTRL] = (u16)temp;
        GPFifo.readPtr = GPFifo.writePtr;
        __cpReg[CP_FIFO_CNT_L] = 0;
        GPFifo.count = 0;
        __cpReg[CP_FIFO_WP_L] = (u16)(u32)GPFifo.writePtr;
        __cpReg[CP_FIFO_RP_L] = (u16)(u32)GPFifo.readPtr;
        __cpReg[CP_FIFO_CNT_H] = (u16)((s32)GPFifo.count >> 16);
        __cpReg[CP_FIFO_WP_H] = (u16)(((u32)GPFifo.writePtr >> 16) & 0x3FFF);
        __cpReg[CP_FIFO_RP_H] = (u16)(((u32)GPFifo.readPtr >> 16) & 0x3FFF);
        PPCSync();
        if (CPGPLinked) {
            u32 wp = 0;
            wp = (u32)GPFifo.writePtr & 0x1FFFFFE0;
            CPUFifo.readPtr = GPFifo.readPtr;
            CPUFifo.writePtr = GPFifo.writePtr;
            CPUFifo.count = GPFifo.count;
            __piReg[PI_FIFO_WP] = wp;
            temp = (gx->cpCtrlReg | 4) & ~8;
            gx->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
            temp = gx->cpCtrlReg | 0x10;
            gx->cpCtrlReg = temp;
            __cpReg[CP_CTRL] = (u16)temp;
        }
        {
            u32 t2;
            temp = gx->cpCtrlReg & ~2;
            t2 = temp & ~0x20;
            gx->cpCtrlReg = t2;
            __cpReg[CP_CTRL] = (u16)t2;
        }
        __GXCurrentBP = 0;
        temp = gx->cpClrReg | 3;
        gx->cpClrReg = temp;
        __cpReg[CP_CLR] = (u16)temp;
        temp = gx->cpCtrlReg | 1;
        gx->cpCtrlReg = temp;
        __cpReg[CP_CTRL] = (u16)temp;
        OSRestoreInterrupts(intrStatus);
    }
}

u32 GXGetOverflowCount(void) {
    return __GXOverflowCount;
}

u32 GXResetOverflowCount(void) {
    u32 old = __GXOverflowCount;
    __GXOverflowCount = 0;
    return old;
}

void *GXRedirectWriteGatherPipe(void *ptr) {
    BOOL intrStatus;

    intrStatus = OSDisableInterrupts();
    GXFlush();
    do { } while (PPCMfwpar() & 1);
    PPCMtwpar(0x0C008000);
    if (CPGPLinked) {
        u32 temp;
        temp = gxdt->cpCtrlReg & ~0x10;
        gxdt->cpCtrlReg = temp;
        __cpReg[CP_CTRL] = (u16)temp;
        temp = gxdt->cpCtrlReg & ~0xC;
        gxdt->cpCtrlReg = temp;
        __cpReg[CP_CTRL] = (u16)temp;
    }
    __GXSaveFifo();
    {
        u32 wp = 0;
        __piReg[PI_FIFO_BASE] = wp;
        wp |= (u32)ptr & 0x1FFFFFE0;
        __piReg[PI_FIFO_END] = 0x04000000;
        __piReg[PI_FIFO_WP] = wp;
    }
    PPCSync();
    OSRestoreInterrupts(intrStatus);
    return (void *)0xCC008000;
}

void GXRestoreWriteGatherPipe(void) {
    BOOL intrStatus;
    volatile u8 *pipe;
    int i;

    intrStatus = OSDisableInterrupts();
    pipe = (volatile u8 *)0xCC008000;
    for (i = 0; i < 31; i++) {
        *pipe = 0;
    }
    PPCSync();
    do { } while (PPCMfwpar() & 1);
    PPCMtwpar(0x0C008000);
    {
        u32 wp = 0;
        __piReg[PI_FIFO_BASE] = (u32)CPUFifo.base & 0x3FFFFFFF;
        __piReg[PI_FIFO_END] = (u32)CPUFifo.end & 0x3FFFFFFF;
        wp = wp | ((u32)CPUFifo.writePtr & 0x1FFFFFE0);
        __piReg[PI_FIFO_WP] = wp;
    }
    if (CPGPLinked) {
        u32 temp;
        temp = gxdt->cpClrReg | 3;
        gxdt->cpClrReg = temp;
        __cpReg[CP_CLR] = (u16)temp;
        temp = (gxdt->cpCtrlReg | 4) & ~8;
        gxdt->cpCtrlReg = temp;
        __cpReg[CP_CTRL] = (u16)temp;
        temp = gxdt->cpCtrlReg | 0x10;
        gxdt->cpCtrlReg = temp;
        __cpReg[CP_CTRL] = (u16)temp;
    }
    PPCSync();
    OSRestoreInterrupts(intrStatus);
}
