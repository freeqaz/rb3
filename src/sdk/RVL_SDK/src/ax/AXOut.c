#include <revolution/AX.h>
#include <revolution/AI.h>
#include <revolution/DSP.h>
#include <revolution/os/OSThread.h>
#include <revolution/os/OSTime.h>
#include <revolution/os/OSInterrupt.h>
#include <revolution/os/OSCache.h>

// BSS variables - declared in contiguous order so MWCC uses a single base register
ALIGN(8)
u8 __AXLocalProfile[0x38];

// __AXLocalProfile is 0x38 bytes; next ALIGN(32) variable starts at 0x40
ALIGN(32)
u8 __AXRmtOutBuffer[0x5A0];

ALIGN(32)
u8 __AXOutSBuffer[0x300];

ALIGN(32)
u8 __AXOutBuffer[0x480];

ALIGN(8)
DSPTask __AXDSPTask;

ALIGN(32)
u8 __AXDramImage[0x40];

// SBSS variables
AXExceedCallback __AXExceedCallback;
u32 __AXRmtCpuPtr;
u32 __AXRmtDspPtr;
u32 __AXRmtBuffLen;
u32 __AXOutputBufferMode;
// gap at 0x80C7B104
OSThreadQueue __AXOutThreadQueue;
u32 __AXDebugSteppingMode;
u32 __AXDSPDoneFlag;
u32 __AXDSPInitFlag;
AXFrameCallback __AXUserFrameCallback;
u32 __AXOutDspReady;
u32 __AXAiDmaFrame;
u32 __AXOutFrame;

int __AXOutNewFrame(void) {
    int retval;
    void* rmtptrs[4];
    void* rmt_base;
    u32 newDspPtr;
    u8* profile;

    {
        s64 t0 = OSGetTime();
        *(u32*)__AXLocalProfile = (u32)((u64)t0 >> 32);
        *(u32*)(__AXLocalProfile + 4) = (u32)t0;
    }

    retval = (0x60 - (AIGetDMABytesLeft() >> 2)) * 0xed5;

    if (__AXOutputBufferMode == 1) {
        __AXSyncPBs((void *)0);
    } else {
        __AXSyncPBs((void *)0x5f50);
    }

    __AXPrintStudio();

    {
        void* cmdListAddr = (void*)(u32)__AXGetCommandListAddress();
        DSPSendMailToDSP((void*)0xbabe0080);
        while (DSPCheckMailToDSP() != 0) {}

        DSPSendMailToDSP(cmdListAddr);
        while (DSPCheckMailToDSP() != 0) {}
    }

    __AXServiceCallbackStack();

    {
        s64 t = OSGetTime();
        *(u32*)(__AXLocalProfile + 0xC) = (u32)t;
        *(u32*)(__AXLocalProfile + 0x8) = (u32)((u64)t >> 32);
    }

    __AXProcessAux();

    {
        s64 t = OSGetTime();
        *(u32*)(__AXLocalProfile + 0x14) = (u32)t;
        *(u32*)(__AXLocalProfile + 0x10) = (u32)((u64)t >> 32);
    }

    {
        s64 t = OSGetTime();
        *(u32*)(__AXLocalProfile + 0x1C) = (u32)((u64)t >> 32);
        *(u32*)(__AXLocalProfile + 0x18) = (u32)t;
    }

    if (__AXUserFrameCallback != NULL) {
        __AXUserFrameCallback();
    }

    {
        s64 t = OSGetTime();
        *(u32*)(__AXLocalProfile + 0x24) = (u32)((u64)t >> 32);
        *(u32*)(__AXLocalProfile + 0x20) = (u32)t;
    }

    rmt_base = (u8*)__AXRmtOutBuffer + __AXRmtDspPtr * 2;
    rmtptrs[0] = rmt_base;
    rmtptrs[1] = (u8*)rmt_base + 0x168;
    rmtptrs[2] = (u8*)rmt_base + 0x2D0;
    rmtptrs[3] = (u8*)rmt_base + 0x438;

    newDspPtr = __AXRmtDspPtr + 0x12;
    if ((s32)newDspPtr >= (s32)__AXRmtBuffLen) {
        newDspPtr = 0;
    }
    if ((s32)__AXRmtCpuPtr >= (s32)__AXRmtDspPtr && (s32)__AXRmtCpuPtr < (s32)(__AXRmtDspPtr + 0x12)) {
        __AXRmtCpuPtr = newDspPtr;
    }
    __AXRmtDspPtr = newDspPtr;

    __AXNextFrame((u32)__AXOutSBuffer,
                  (u32)(__AXOutBuffer + __AXOutFrame * 0x180),
                  (u32 *)rmtptrs);

    __AXOutFrame = __AXOutFrame + 1;
    if (__AXOutputBufferMode == 1) {
        __AXOutFrame = __AXOutFrame % 3;
    } else {
        __AXOutFrame = __AXOutFrame & 1;
        AIInitDMA(__AXOutBuffer + __AXOutFrame * 0x180, 0x180);
    }

    {
        s64 t = OSGetTime();
        *(u32*)(__AXLocalProfile + 0x2C) = (u32)t;
        *(u32*)(__AXLocalProfile + 0x28) = (u32)((u64)t >> 32);
    }

    *(u32*)(__AXLocalProfile + 0x30) = __AXGetNumVoices();

    profile = (u8*)__AXGetCurrentProfile();
    if (profile != NULL) {
        int i;
        u8* src = __AXLocalProfile;
        for (i = 0; i < 7; i++) {
            profile[0] = src[0]; profile[1] = src[1];
            profile[2] = src[2]; profile[3] = src[3];
            profile[4] = src[4]; profile[5] = src[5];
            profile[6] = src[6]; profile[7] = src[7];
            src += 8;
            profile += 8;
        }
    }

    return retval;
}

void __AXOutAiCallback(void) {
    u32 nextFrame;

    if ((s32)__AXDSPDoneFlag != 1) {
        if (__AXOutDspReady == 1) {
            __AXOutDspReady = 0;
            __AXOutNewFrame();
        } else {
            __AXOutDspReady = 2;
            DSPAssertTask(&__AXDSPTask);
        }

        if (__AXOutputBufferMode == 1) {
            AIInitDMA(__AXOutBuffer + __AXAiDmaFrame * 0x180, 0x180);
            nextFrame = (__AXAiDmaFrame + 1) % 3;
            if (nextFrame != __AXOutFrame) {
                __AXAiDmaFrame = nextFrame;
            }
        }
    }
}

void __AXDSPInitCallback(void) {
    __AXDSPInitFlag = 1;
}

void __AXDSPResumeCallback(void) {
    if (__AXOutDspReady == 2) {
        __AXOutDspReady = 0;
        __AXOutNewFrame();
        if (__AXExceedCallback != NULL) {
            __AXExceedCallback();
        }
    } else {
        __AXOutDspReady = 1;
    }
}

void __AXDSPDoneCallback(void) {
    __AXDSPDoneFlag = 1;
    OSWakeupThread(&__AXOutThreadQueue);
}

void __AXDSPRequestCallback(void) {
}

void __AXOutInitDSP(void) {
    __AXDSPTask.iramMmemLen = axDspSlaveLength;
    __AXDSPTask.prio = 0;
    __AXDSPInitFlag = 0;
    __AXDSPTask.iramMmemAddr = axDspSlave;
    __AXDSPTask.iramDspAddr = NULL;
    __AXDSPTask.dramMmemLen = 0x40;
    __AXDSPTask.dramDspAddr = 0xcd2;
    __AXDSPTask.startVector = axDspInitVector;
    __AXDSPTask.resumeVector = axDspResumeVector;
    __AXDSPTask.initCallback = (DSPTaskCallback)__AXDSPInitCallback;
    __AXDSPTask.resumeCallback = (DSPTaskCallback)__AXDSPResumeCallback;
    __AXDSPTask.doneCallback = (DSPTaskCallback)__AXDSPDoneCallback;
    __AXDSPTask.requestCallback = (DSPTaskCallback)__AXDSPRequestCallback;
    __AXDSPDoneFlag = 0;
    __AXDSPTask.dramMmemAddr = __AXDramImage;
    OSInitThreadQueue(&__AXOutThreadQueue);
    if (DSPCheckInit() == 0) {
        DSPInit();
    }
    DSPAddTask(&__AXDSPTask);
    while (__AXDSPInitFlag == 0) {}
}

void __AXOutInit(int mode) {
    int i;
    u32* p;
    void* rmtptrs[4];

    __AXOutFrame = 0;
    __AXAiDmaFrame = 0;
    __AXOutputBufferMode = mode;
    __AXDebugSteppingMode = 0;

    // Clear __AXOutBuffer (0x480 bytes = 0x20 * 9 u32s, stride 0x24)
    p = (u32*)__AXOutBuffer;
    i = 0x20;
    do {
        p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
        p[4] = 0; p[5] = 0; p[6] = 0; p[7] = 0;
        p[8] = 0;
        p = (u32*)((u8*)p + 0x24);
    } while (--i != 0);
    DCFlushRange(__AXOutBuffer, 0x480);

    // Clear __AXOutSBuffer (0x300 bytes = 0x18 * 8 u32s, stride 0x20)
    i = 0x18;
    p = (u32*)__AXOutSBuffer;
    do {
        p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
        p[4] = 0; p[5] = 0; p[6] = 0; p[7] = 0;
        p = (u32*)((u8*)p + 0x20);
    } while (--i != 0);
    DCFlushRange(__AXOutSBuffer, 0x300);

    // Clear __AXRmtOutBuffer (0x5A0 bytes = 0x24 * 10 u32s, stride 0x28)
    i = 0x24;
    p = (u32*)__AXRmtOutBuffer;
    do {
        p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
        p[4] = 0; p[5] = 0; p[6] = 0; p[7] = 0;
        p[8] = 0; p[9] = 0;
        p = (u32*)((u8*)p + 0x28);
    } while (--i != 0);
    DCFlushRange(__AXRmtOutBuffer, 0x5a0);

    __AXOutInitDSP();
    AIRegisterDMACallback(__AXOutAiCallback);

    rmtptrs[0] = __AXRmtOutBuffer;
    rmtptrs[1] = (u8*)__AXRmtOutBuffer + 0x168;
    rmtptrs[2] = (u8*)__AXRmtOutBuffer + 0x2D0;
    rmtptrs[3] = (u8*)__AXRmtOutBuffer + 0x438;

    __AXRmtCpuPtr = 0x12;
    __AXRmtDspPtr = 0x12;
    __AXRmtBuffLen = 0xb4;

    if (__AXOutputBufferMode == 1) {
        __AXNextFrame((u32)__AXOutSBuffer,
                      (u32)(__AXOutBuffer + 0x180),
                      (u32 *)rmtptrs);
    } else {
        __AXNextFrame((u32)__AXOutSBuffer,
                      (u32)(__AXOutBuffer + 0x300),
                      (u32 *)rmtptrs);
    }

    __AXOutDspReady = 1;
    __AXUserFrameCallback = NULL;

    if (__AXOutputBufferMode == 1) {
        AIInitDMA(__AXOutBuffer + __AXAiDmaFrame * 0x180, 0x180);
        __AXAiDmaFrame++;
    } else {
        AIInitDMA(__AXOutBuffer + __AXOutFrame * 0x180, 0x180);
    }

    AIStartDMA();
    __AXExceedCallback = NULL;
}

void __AXOutQuit(void) {
    BOOL intrState;

    intrState = OSDisableInterrupts();
    __AXUserFrameCallback = NULL;
    DSPCancelTask(&__AXDSPTask);
    OSSleepThread(&__AXOutThreadQueue);
    AIRegisterDMACallback(NULL);
    AIStopDMA();
    __AXExceedCallback = NULL;
    OSRestoreInterrupts(intrState);
}

AXFrameCallback AXRegisterCallback(AXFrameCallback callback) {
    AXFrameCallback old;
    BOOL intrState;

    old = __AXUserFrameCallback;
    intrState = OSDisableInterrupts();
    __AXUserFrameCallback = callback;
    OSRestoreInterrupts(intrState);
    return old;
}
