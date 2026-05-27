#include <revolution/AX.h>
#include <revolution/OS.h>

/* Command list write pointer and related globals */
static u16 *__AXClWrite;
static int  __AXClMode;
static int  __AXCommandListPosition;
static u32  __AXCommandListCycles;

/* Volume globals */
static u16  __AXMasterVolume;
static u16  __AXAuxAVolume;
static u16  __AXAuxBVolume;
static u16  __AXAuxCVolume;
static u16  __AXCompressorReleaseFrames;

/* Compressor globals */
static u32  __AXCompressor;
static u16 *__AXCompressorTable; /* points to active compressor table */

/* Command list buffer (2 banks of 64 u16s = 128 bytes each) */
static u16  __AXCommandList[0x80]; /* 128 u16s total */

/* Forward declarations for AX internals not in public headers */
extern u32  __AXGetStudio(void);
extern u32  __AXGetPBs(void);
extern void __AXGetAuxAInput(u32 *out);
extern void __AXGetAuxAInputDpl2(u32 *out);
extern void __AXGetAuxAOutput(u32 *out);
extern void __AXGetAuxAOutputDpl2R(u32 *out);
extern void __AXGetAuxAOutputDpl2Ls(u32 *out);
extern void __AXGetAuxAOutputDpl2Rs(u32 *out);
extern void __AXGetAuxBInput(u32 *out);
extern void __AXGetAuxBInputDpl2(u32 *out);
extern void __AXGetAuxBOutput(u32 *out);
extern void __AXGetAuxBOutputDpl2R(u32 *out);
extern void __AXGetAuxBOutputDpl2Ls(u32 *out);
extern void __AXGetAuxBOutputDpl2Rs(u32 *out);
extern void __AXGetAuxCInput(u32 *out);
extern void __AXGetAuxCOutput(u32 *out);

u32 __AXGetCommandListCycles(void) {
    return __AXCommandListCycles;
}

u16 *__AXGetCommandListAddress(void) {
    int pos;
    u16 *base;

    pos = __AXCommandListPosition;
    base = __AXCommandList;
    __AXCommandListPosition = (pos + 1) & 1;
    __AXClWrite = base + ((pos + 1) & 1) * 0x40;
    return base + pos * 0x40;
}

/* Helper macro: write a u16 to the command list and advance pointer */
#define CL_WRITE(val) do { *__AXClWrite = (u16)(val); __AXClWrite++; } while(0)
#define CL_WRITE32HI(val) CL_WRITE((u32)(val) >> 16)
#define CL_WRITE32LO(val) CL_WRITE((u32)(val))

void __AXNextFrame(u32 param_1, u32 param_2, u32 *param_3) {
    u16 *startAddr;
    u32 studioAddr;
    u32 pbAddr;
    u32 local[2];

    startAddr = __AXClWrite;
    __AXCommandListCycles = 0x1e83;

    studioAddr = __AXGetStudio();

    CL_WRITE(0);
    CL_WRITE32HI(studioAddr);
    CL_WRITE32LO(studioAddr);

    __AXCommandListCycles += 0x101e;

    if (__AXClMode == 0) {
        CL_WRITE(1);
        CL_WRITE32HI(param_1);
        CL_WRITE32LO(param_1);
        __AXCommandListCycles += 0x2dd;
    } else if (__AXClMode == 1) {
        CL_WRITE(2);
        CL_WRITE32HI(param_1);
        CL_WRITE32LO(param_1);
        __AXCommandListCycles += 0x33d;
    } else if (__AXClMode == 2) {
        CL_WRITE(3);
        CL_WRITE32HI(param_1);
        CL_WRITE32LO(param_1);
        __AXCommandListCycles += 0x39d;
    }

    pbAddr = __AXGetPBs();
    CL_WRITE(4);
    CL_WRITE32HI(pbAddr);
    CL_WRITE32LO(pbAddr);

    if (__AXClMode == 2) {
        __AXGetAuxAInput(local);
        if (local[0] != 0) {
            CL_WRITE(8);
            CL_WRITE(__AXAuxAVolume);
            CL_WRITE32HI(local[0]);
            CL_WRITE32LO(local[0]);
            __AXGetAuxAInputDpl2(local);
            CL_WRITE32HI(local[0]);
            CL_WRITE32LO(local[0]);
            __AXGetAuxAOutput(local);
            CL_WRITE32HI(local[0]);
            CL_WRITE32LO(local[0]);
            __AXGetAuxAOutputDpl2R(local);
            CL_WRITE32HI(local[0]);
            CL_WRITE32LO(local[0]);
            __AXGetAuxAOutputDpl2Ls(local);
            CL_WRITE32HI(local[0]);
            CL_WRITE32LO(local[0]);
            __AXGetAuxAOutputDpl2Rs(local);
            CL_WRITE32HI(local[0]);
            CL_WRITE32LO(local[0]);
            __AXCommandListCycles += 0x8bb;
        }

        __AXGetAuxBInput(local);
        if (local[0] != 0) {
            CL_WRITE(6);
            CL_WRITE(__AXAuxBVolume);
            CL_WRITE32HI(local[0]);
            CL_WRITE32LO(local[0]);
            __AXGetAuxBOutput(local);
            CL_WRITE32HI(local[0]);
            CL_WRITE32LO(local[0]);
            __AXCommandListCycles += 0x8bb;
        }

        __AXGetAuxCInput(local);
        if (local[0] != 0) {
            CL_WRITE(7);
            CL_WRITE(__AXAuxCVolume);
            CL_WRITE32HI(local[0]);
            CL_WRITE32LO(local[0]);
            __AXGetAuxCOutput(local);
            CL_WRITE32HI(local[0]);
            CL_WRITE32LO(local[0]);
            __AXCommandListCycles += 0x8bb;
        }
    }

    if (__AXCompressor != 0) {
        CL_WRITE(10);
        CL_WRITE(0x8000);
        CL_WRITE(__AXCompressorReleaseFrames);
        CL_WRITE32HI((u32)(void *)__AXCompressorTable);
        CL_WRITE32LO((u32)(void *)__AXCompressorTable);
        __AXCommandListCycles += 0x73a;
    }

    CL_WRITE(0xd);
    CL_WRITE32HI(param_3[0]);
    CL_WRITE32LO(param_3[0]);
    CL_WRITE32HI(param_3[1]);
    CL_WRITE32LO(param_3[1]);
    CL_WRITE32HI(param_3[2]);
    CL_WRITE32LO(param_3[2]);
    CL_WRITE32HI(param_3[3]);
    CL_WRITE32LO(param_3[3]);
    __AXCommandListCycles += 0x199;

    if (__AXClMode == 2) {
        CL_WRITE(0xc);
        CL_WRITE(__AXMasterVolume);
        CL_WRITE32HI(param_1);
        CL_WRITE32LO(param_1);
        CL_WRITE32HI(param_2);
        CL_WRITE32LO(param_2);
        __AXCommandListCycles += 0x4ab;
    } else {
        CL_WRITE(0xb);
        CL_WRITE(__AXMasterVolume);
        CL_WRITE32HI(param_1);
        CL_WRITE32LO(param_1);
        CL_WRITE32HI(param_2);
        CL_WRITE32LO(param_2);
        __AXCommandListCycles += 0x494;
    }

    CL_WRITE(0xe);
    __AXCommandListCycles += 0x1e;

    DCFlushRange(startAddr, 0x80);
}

void __AXClInit(void) {
    __AXClMode = 0;
    __AXCommandListPosition = 0;
    __AXClWrite = __AXCommandList;
    __AXCompressor = 1;
    __AXCompressorTable = (u16 *)__AXCompressorDefaultTable;
    __AXCompressorReleaseFrames = 10;
    __AXMasterVolume = 0x8000;
    __AXAuxAVolume = 0x8000;
    __AXAuxBVolume = 0x8000;
    __AXAuxCVolume = 0x8000;
}

void __AXClQuit(void) {
}

void AXSetCompressor(u32 on) {
    __AXCompressor = on;
}

u16 AXGetMasterVolume(void) {
    return __AXMasterVolume;
}

u16 AXGetAuxAReturnVolume(void) {
    return __AXAuxAVolume;
}

u16 AXGetAuxBReturnVolume(void) {
    return __AXAuxBVolume;
}

u16 AXGetAuxCReturnVolume(void) {
    return __AXAuxCVolume;
}

void AXSetMasterVolume(u16 vol) {
    if (vol > 0x8000) {
        vol = 0x8000;
    }
    __AXMasterVolume = vol;
}

void AXSetAuxAReturnVolume(u16 vol) {
    __AXAuxAVolume = vol;
}

void AXSetAuxBReturnVolume(u16 vol) {
    __AXAuxBVolume = vol;
}

void AXSetAuxCReturnVolume(u16 vol) {
    __AXAuxCVolume = vol;
}
