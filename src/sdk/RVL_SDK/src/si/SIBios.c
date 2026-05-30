#include "types.h"

u8 Si[0x14];
u8 Type[0x10];
u32 __PADFixBits;
u8 Packet[0x80];
u8 XferTime[0x20];
u8 TypeTime[0x20];
u8 Alarm[0xC0];
u8 InputBuffer[0x20];
u8 InputBufferValid[0x10];
u8 InputBufferVcount[0x10];
u8 RDSTHandler[0x10];
u8 TypeCallback[0x40];

static void CompleteTransfer(s32 chan) {
    (void)chan;
}

void SIInterruptHandler(s16 type, void *context) {
    (void)type; (void)context;
}

void SIInit(void) {
}

s32 __SITransfer(s32 chan, const void *output, s32 outputLen, void *input, s32 inputLen, void *callback) {
    (void)chan; (void)output; (void)outputLen; (void)input; (void)inputLen; (void)callback;
    return 0;
}

void SISetXY(u32 x, u32 y) {
    (void)x; (void)y;
}

static void AlarmHandler(void *alarm, void *context) {
    (void)alarm; (void)context;
}

s32 SITransfer(s32 chan, const void *output, s32 outputLen, void *input, s32 inputLen, void *callback, s64 delay) {
    (void)chan; (void)output; (void)outputLen; (void)input; (void)inputLen; (void)callback; (void)delay;
    return 0;
}

static void GetTypeCallback(s32 chan, u32 type, void *context) {
    (void)chan; (void)type; (void)context;
}

u32 SIGetType(s32 chan) {
    (void)chan;
    return 0;
}
