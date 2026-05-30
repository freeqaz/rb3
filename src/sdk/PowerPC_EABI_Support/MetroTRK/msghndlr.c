#include "types.h"

extern s32 TRK_AppendBuffer(void *buf, const void *data, s32 len);
extern s32 TRK_ReadBuffer(void *buf, void *data, s32 len);
extern s32 TRKAppendBuffer1_ui32(void *buf, u32 val);
extern s32 TRKReadBuffer_ui32(void *buf, u32 *val);
extern s32 TRKAppendBuffer_ui8(void *buf, u8 val);
extern s32 TRKReadBuffer_ui8(void *buf, u8 *val);
extern s32 TRK_RequestSend(void *msg);
extern void *TRKGetBuffer(s32 bufId);
extern s32 TRK_GetFreeBuffer(s32 *bufId, void **buf);
extern s32 TRK_ReleaseBuffer(s32 bufId);
extern s32 TRKResetBuffer(void *buf, s32 full);
extern s32 TRK_SetBufferPosition(void *buf, s32 pos);
extern void *gTRKMsgBufs;
extern void *gTRKCPUState;

s32 g_CurrentSequence;
s32 IsTRKConnected;

s32 GetTRKConnected(void) {
    return IsTRKConnected;
}

s32 TRK_DoConnect(void *msg) {
    (void)msg;
    IsTRKConnected = 1;
    return 0;
}

s32 TRKDoDisconnect(void *msg) {
    (void)msg;
    IsTRKConnected = 0;
    return 0;
}

s32 TRKDoReset(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKDoOverride(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKDoReadMemory(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKDoWriteMemory(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKDoReadRegisters(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKDoWriteRegisters(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKDoContinue(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKDoStep(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKDoStop(void *msg) {
    (void)msg;
    return 0;
}

s32 TRKDoSetOption(void *msg) {
    (void)msg;
    return 0;
}
