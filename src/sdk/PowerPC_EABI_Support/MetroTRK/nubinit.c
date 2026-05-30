#include "types.h"

extern void TRKInitializeEventQueue(void);
extern void TRKInitializeSerialHandler(void);
extern void TRKInitializeTarget(void);
extern s32 TRKInitializeMessageBuffers(void);
extern void TRK_InitializeEndian(void);
extern void TRK_MessageSend(void);

s32 gTRKBigEndian;

s32 TRK_InitializeNub(void) {
    TRK_InitializeEndian();
    TRKInitializeEventQueue();
    TRKInitializeSerialHandler();
    TRKInitializeTarget();
    return TRKInitializeMessageBuffers();
}

void TRK_TerminateNub(void) {
    TRKInitializeSerialHandler();
}

void TRK_NubWelcome(void) {
    TRK_MessageSend();
}

void TRK_InitializeEndian(void) {
    union {
        s32 i;
        u8 c[4];
    } endian;
    endian.i = 1;
    gTRKBigEndian = (endian.c[0] == 0);
}
