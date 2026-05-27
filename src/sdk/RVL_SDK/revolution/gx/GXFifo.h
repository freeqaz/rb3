#ifndef RVL_SDK_GX_FIFO_H
#define RVL_SDK_GX_FIFO_H
#include "revolution/gx/GXInternal.h"
#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

GX_DECL_PUBLIC_STRUCT(GXFifoObj, 128);

typedef void (*GXBreakPtCallback)(void);

void GXInitFifoBase(GXFifoObj *obj, void *base, u32 size);
void GXSetCPUFifo(GXFifoObj *obj);
void GXSetGPFifo(GXFifoObj *obj);

void GXGetGPStatus(u8 *overflowed, u8 *underflowed, u8 *readIdle, u8 *cmdIdle, u8 *brkpt);
BOOL GXGetCPUFifo(GXFifoObj *obj);

u32 GXGetFifoCount(GXFifoObj *obj);
u8 GXGetFifoWrap(GXFifoObj *obj);

void GXGetFifoPtrs(GXFifoObj *obj, void **readPtr, void **writePtr);
void *GXGetFifoBase(GXFifoObj *obj);
u32 GXGetFifoSize(GXFifoObj *obj);
void GXGetFifoLimits(GXFifoObj *obj, u32 *hiWatermark, u32 *loWatermark);
u8 GXIsCPUGPFifoLinked(void);

u32 GXGetOverflowCount(void);
u32 GXResetOverflowCount(void);

GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback cb);
void GXEnableBreakPt(void *breakPt);
void GXDisableBreakPt(void);

void *GXRedirectWriteGatherPipe(void *ptr);
void GXRestoreWriteGatherPipe(void);

void __GXFifoInit(void);
void __GXCleanGPFifo(void);
void __GXSaveFifo(void);
u8 __GXIsGPFifoReady(void);
u8 CPGPLinkCheck(void);

extern GXFifoObjImpl CPUFifo;
extern GXFifoObjImpl GPFifo;
extern u8 CPUFifoReady;
extern u8 GPFifoReady;
extern u32 __GXOverflowCount;
extern s32 __GXCurrentBP;
extern void (*BreakPointCB)(void);
extern s32 GXOverflowSuspendInProgress;
extern u8 CPGPLinked;

#ifdef __cplusplus
}
#endif
#endif
