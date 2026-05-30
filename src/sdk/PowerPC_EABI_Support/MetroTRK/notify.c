#include "types.h"

extern s32 TRK_MessageSend(void);
extern s32 TRKGetNextEvent(void *event);
extern void *gTRKMsgBufs;
extern void *gTRKCPUState;

void TRKDoNotifyStopped(s32 stopStatus) {
    TRK_MessageSend();
}
