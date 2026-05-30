#include "types.h"

typedef s32 (*TRKHandlerFunc)(void *msg);

extern s32 TRKHandleOverflow(void *msg);
extern s32 TRKHandleReadMemory(void *msg);
extern s32 TRKHandleWriteMemory(void *msg);
extern s32 TRKHandleReadRegisters(void *msg);
extern s32 TRKHandleWriteRegisters(void *msg);
extern s32 TRKHandleContinue(void *msg);
extern s32 TRKHandleStep(void *msg);
extern s32 TRKHandleStop(void *msg);
extern s32 TRKHandleConnect(void *msg);
extern s32 TRKHandleVersions(void *msg);
extern s32 TRKHandleReset(void *msg);

s32 TRK_DispatchMessage(void *msg) {
    (void)msg;
    return 0;
}
