#include "types.h"

extern void TRKTargetContinue(void);
extern s32 TRKGetNextEvent(void *event);

void *gTRKInputPendingPtr;

void TRKTestForPacket(void) {
    /* Poll serial interface for incoming data */
}

void TRKGetInput(void) {
    /* Transfer pending serial data to message buffer */
    gTRKInputPendingPtr = NULL;
}

void TRKProcessInput(void) {
    /* Process received TRK protocol packet */
}

void TRKInitializeSerialHandler(void) {}

void TRKTerminateSerialHandler(void) {}
