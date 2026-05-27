#include <revolution/GX.h>
#include <revolution/gx/GXTypes.h>
#include <revolution/os/OSContext.h>
#include <revolution/os/OSInterrupt.h>
#include <revolution/os/OSThread.h>
#include <revolution/os/OSTime.h>
#include <revolution/base/PPCArch.h>

extern volatile u16 *__peReg;
extern volatile u16 *__memReg;
extern u8 __GXIsGPFifoReady(void);
extern void __GXCleanGPFifo(void);
extern void __GXInitRevisionBits(void);
extern void __GXSetDirtyState(void);

static OSThreadQueue FinishQueue;
static u8 DrawDone;
static GXDrawDoneCallback DrawDoneCB;
static void (*TokenCB)(u16);

void GXSetMisc(u32 token, u32 val) {
    switch ((s32)token) {
    case 1: {
        GXData *gx = gxdt;
        u16 vlim = (u16)val;
        u32 nlz = __cntlzw((u32)vlim);
        gx->SHORT_0x4 = vlim;
        gx->SHORT_0x0 = (u16)((nlz >> 5) & 0xFFFF);
        gx->lastWriteWasXF = 1;
        if (vlim == 0) {
            return;
        }
        gx->gxDirtyFlags |= 8;
        break;
    }
    case 2: {
        u32 bval = ((u32)(-(s32)val)) | val;
        GXData *gx = gxdt;
        gx->dlistSave = (GXBool)(bval >> 31);
        break;
    }
    case 3: {
        u32 bval = ((u32)(-(s32)val)) | val;
        GXData *gx = gxdt;
        gx->BYTE_0x5FA = (u8)(bval >> 31);
        break;
    }
    }
}

void GXFlush(void) {
    volatile u32 *fifo = (volatile u32 *)0xCC018000;
    if (gxdt->gxDirtyFlags != 0) {
        __GXSetDirtyState();
    }
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    PPCSync();
}

void __GXAbort(void) {
    volatile u32 *cp = (volatile u32 *)0xCC003000;

    if (gxdt->BYTE_0x5FA) {
        if ((u8)__GXIsGPFifoReady()) {
            volatile u16 *memReg = __memReg;
            u32 lo = memReg[0x4E/2];
            u32 prev, hi, rdPtr;
            do {
                prev = lo;
                hi = memReg[0x50/2];
                lo = memReg[0x4E/2];
            } while (lo != prev);
            rdPtr = (lo << 16) | hi;
            s32 timeout = 8;
            u32 mark = (u32)0 ^ 0x80000000;
            do {
                OSTime startTime = OSGetTime();
                OSTime curTime;
                do { curTime = OSGetTime(); } while (curTime - startTime <= (OSTime)timeout);
                memReg = __memReg;
                lo = memReg[0x4E/2];
                do {
                    prev = lo;
                    hi = memReg[0x50/2];
                    lo = memReg[0x4E/2];
                } while (lo != prev);
                u32 newPtr = (lo << 16) | hi;
                prev = rdPtr;
                rdPtr = newPtr;
            } while (rdPtr != prev);
        }
    }

    cp[0x18/4] = 1;
    {
        OSTime startTime = OSGetTime();
        s32 timeout = 0x32;
        u32 mark = (u32)0 ^ 0x80000000;
        OSTime curTime;
        do { curTime = OSGetTime(); } while (curTime - startTime <= (OSTime)timeout);
    }
    cp[0x18/4] = 0;
    {
        OSTime startTime = OSGetTime();
        s32 timeout = 5;
        u32 mark = (u32)0 ^ 0x80000000;
        OSTime curTime;
        do { curTime = OSGetTime(); } while (curTime - startTime <= (OSTime)timeout);
    }
}

void GXAbortFrame(void) {
    GXData *gx = gxdt;
    volatile u32 *cp = (volatile u32 *)0xCC003000;

    if (gx->BYTE_0x5FA) {
        if ((u8)__GXIsGPFifoReady()) {
            volatile u16 *memReg = __memReg;
            u32 lo = memReg[0x4E/2];
            u32 prev, hi, rdPtr;
            do {
                prev = lo;
                hi = memReg[0x50/2];
                lo = memReg[0x4E/2];
            } while (lo != prev);
            rdPtr = (lo << 16) | hi;
            s32 timeout = 8;
            u32 mark = (u32)0 ^ 0x80000000;
            do {
                OSTime startTime = OSGetTime();
                OSTime curTime;
                do { curTime = OSGetTime(); } while (curTime - startTime <= (OSTime)timeout);
                memReg = __memReg;
                lo = memReg[0x4E/2];
                do {
                    prev = lo;
                    hi = memReg[0x50/2];
                    lo = memReg[0x4E/2];
                } while (lo != prev);
                u32 newPtr = (lo << 16) | hi;
                prev = rdPtr;
                rdPtr = newPtr;
            } while (rdPtr != prev);
        }
    }

    cp[0x18/4] = 1;
    {
        OSTime startTime = OSGetTime();
        s32 timeout = 0x32;
        u32 mark = (u32)0 ^ 0x80000000;
        OSTime curTime;
        do { curTime = OSGetTime(); } while (curTime - startTime <= (OSTime)timeout);
    }
    cp[0x18/4] = 0;
    {
        OSTime startTime = OSGetTime();
        s32 timeout = 5;
        u32 mark = (u32)0 ^ 0x80000000;
        OSTime curTime;
        do { curTime = OSGetTime(); } while (curTime - startTime <= (OSTime)timeout);
    }

    if ((u8)__GXIsGPFifoReady()) {
        __GXCleanGPFifo();
        __GXInitRevisionBits();
        gx->gxDirtyFlags = 0;
        goto flush;
        __GXSetDirtyState();  /* dead code — matches original binary layout */
flush: {
            volatile u32 *fifo = (volatile u32 *)0xCC018000;
            fifo[0] = 0;
            fifo[0] = 0;
            fifo[0] = 0;
            fifo[0] = 0;
            fifo[0] = 0;
            fifo[0] = 0;
            fifo[0] = 0;
            fifo[0] = 0;
        }
        PPCSync();
    }
}

void GXSetDrawSync(u16 token) {
    BOOL intrStatus;
    GXData *gx;
    volatile u32 *fifo;
    u32 reg1;
    u32 reg2;

    intrStatus = OSDisableInterrupts();
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    reg1 = ((u32)token) | 0x48000000;
    gx = gxdt;
    WGPIPE.i = reg1;

    /* Build second BP register: opcode 0x47 with token */
    reg2 = reg1;
    reg2 = __rlwimi(reg2, (u32)token, 0, 16, 31);
    reg2 = __rlwimi(reg2, (u32)0x47, 24, 0, 7);
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = reg2;

    if (gx->gxDirtyFlags != 0) {
        __GXSetDirtyState();
    }

    fifo = (volatile u32 *)0xCC018000;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    PPCSync();
    OSRestoreInterrupts(intrStatus);
    gx->lastWriteWasXF = 0;
}

void GXSetDrawDone(void) {
    BOOL intrStatus;
    GXData *gx;
    volatile u32 *fifo;

    intrStatus = OSDisableInterrupts();
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    WGPIPE.i = 0x45000002;
    gx = gxdt;

    if (gx->gxDirtyFlags != 0) {
        __GXSetDirtyState();
    }

    fifo = (volatile u32 *)0xCC018000;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    fifo[0] = 0;
    PPCSync();
    DrawDone = 0;
    OSRestoreInterrupts(intrStatus);
    gx->lastWriteWasXF = 0;
}

void GXPixModeSync(void) {
    WGPIPE.c = GX_FIFO_CMD_LOAD_BP_REG;
    GXData *gx = gxdt;
    WGPIPE.i = gx->zControl;
    gx->lastWriteWasXF = 0;
}

void GXPokeAlphaMode(GXCompare func, u8 threshold) {
    volatile u16 *peReg = __peReg;
    u32 val = (u32)threshold;
    val = __rlwimi(val, (u32)func, 8, 0, 23);
    peReg[0x6/2] = (u16)val;
}

void GXPokeAlphaRead(u32 type) {
    volatile u16 *peReg = __peReg;
    u32 val = 0;
    val = __rlwimi(val, type, 0, 30, 31);
    val |= 0x4;
    peReg[0x8/2] = (u16)val;
}

void GXPokeAlphaUpdate(GXBool enable) {
    volatile u16 *peReg = __peReg;
    u32 val = peReg[0x2/2];
    val = __rlwimi(val, (u32)enable, 4, 27, 27);
    peReg[0x2/2] = (u16)val;
}

void GXPokeBlendMode(GXBlendMode type, GXBlendFactor srcFactor, GXBlendFactor dstFactor, GXLogicOp op) {
    volatile u16 *peReg = __peReg;
    u32 blendEnable = 0;
    u32 reg = peReg[0x2/2];  /* lhz before blendEnable logic */

    if (type == GX_BM_BLEND || type == GX_BM_SUBTRACT) {
        blendEnable = 1;
    }
    u32 typeM3 = (u32)type - 3;
    reg = __rlwimi(reg, blendEnable, 0, 31, 31);
    u32 typeM2 = (u32)type - 2;
    peReg = __peReg;
    u32 subEnable = __cntlzw(typeM3);
    u32 logicEnable = __cntlzw(typeM2);
    u32 opcode = 0x41;
    reg = __rlwimi(reg, subEnable, 6, 20, 20);
    reg = __rlwimi(reg, logicEnable, 28, 30, 30);
    reg = __rlwimi(reg, (u32)op, 12, 16, 19);
    reg = __rlwimi(reg, (u32)srcFactor, 8, 21, 23);
    reg = __rlwimi(reg, (u32)dstFactor, 5, 24, 26);
    reg = __rlwimi(reg, opcode, 24, 0, 7);
    peReg[0x2/2] = (u16)reg;
}

void GXPokeColorUpdate(GXBool enable) {
    volatile u16 *peReg = __peReg;
    u32 val = peReg[0x2/2];
    val = __rlwimi(val, (u32)enable, 3, 28, 28);
    peReg[0x2/2] = (u16)val;
}

void GXPokeDstAlpha(GXBool enable, u8 alpha) {
    volatile u16 *peReg = __peReg;
    u32 val = 0;
    val = __rlwimi(val, (u32)alpha, 0, 24, 31);
    val = __rlwimi(val, (u32)enable, 8, 23, 23);
    peReg[0x4/2] = (u16)val;
}

void GXPokeDither(GXBool dither) {
    volatile u16 *peReg = __peReg;
    u32 val = peReg[0x2/2];
    val = __rlwimi(val, (u32)dither, 2, 29, 29);
    peReg[0x2/2] = (u16)val;
}

void GXPokeZMode(GXBool enable, GXCompare func, GXBool update) {
    volatile u16 *peReg = __peReg;
    u32 val = 0;
    val = __rlwimi(val, (u32)enable, 0, 31, 31);
    val = __rlwimi(val, (u32)func, 1, 28, 30);
    val = __rlwimi(val, (u32)update, 4, 27, 27);
    peReg[0x0/2] = (u16)val;
}

void GXPeekZ(u32 x, u32 y, u32 *z) {
    u32 addr = 0xC8000000;
    u32 one = 1;
    addr = __rlwimi(addr, x, 2, 20, 29);
    addr = __rlwimi(addr, y, 12, 10, 19);
    addr = __rlwimi(addr, one, 22, 8, 9);
    *z = *(volatile u32 *)addr;
}

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb) {
    void (*old)(u16) = TokenCB;
    BOOL intrStatus = OSDisableInterrupts();
    TokenCB = cb;
    OSRestoreInterrupts(intrStatus);
    return old;
}

void GXTokenInterruptHandler(s32 intr, OSContext *ctx) {
    void (*cb)(u16) = TokenCB;
    volatile u16 *peReg = __peReg;
    u16 token = peReg[0xE/2];
    if (cb != NULL) {
        OSContext tmpCtx;
        OSClearContext(&tmpCtx);
        OSSetCurrentContext(&tmpCtx);
        ((void (*)(u16))TokenCB)(token);
        OSClearContext(&tmpCtx);
        OSSetCurrentContext(ctx);
    }
    peReg = __peReg;
    {
        u32 val = peReg[0xA/2];
        val |= 0x4;
        peReg[0xA/2] = (u16)val;
    }
}

GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb) {
    GXDrawDoneCallback old = DrawDoneCB;
    BOOL intrStatus = OSDisableInterrupts();
    DrawDoneCB = cb;
    OSRestoreInterrupts(intrStatus);
    return old;
}

void GXFinishInterruptHandler(s32 intr, OSContext *ctx) {
    volatile u16 *peReg = __peReg;
    {
        u32 val = peReg[0xA/2];
        val |= 0x8;
        peReg[0xA/2] = (u16)val;
    }
    GXDrawDoneCallback cb = DrawDoneCB;
    DrawDone = 1;
    if (cb != NULL) {
        OSContext tmpCtx;
        OSClearContext(&tmpCtx);
        OSSetCurrentContext(&tmpCtx);
        ((GXDrawDoneCallback)DrawDoneCB)();
        OSClearContext(&tmpCtx);
        OSSetCurrentContext(ctx);
    }
    OSWakeupThread(&FinishQueue);
}

void __GXPEInit(void) {
    volatile u16 *peReg;
    u32 val;
    __OSSetInterruptHandler(OS_INTR_PI_PE_TOKEN, GXTokenInterruptHandler);
    __OSSetInterruptHandler(OS_INTR_PI_PE_FINISH, GXFinishInterruptHandler);
    OSInitThreadQueue(&FinishQueue);
    __OSUnmaskInterrupts(0x2000);
    __OSUnmaskInterrupts(0x1000);
    peReg = __peReg;
    val = peReg[0xA/2];
    val |= 0xF;
    peReg[0xA/2] = (u16)val;
}
