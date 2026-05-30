#include "types.h"

extern void TRKPostEvent(void *event);
extern s32 TRKGetNextEvent(void *event);
extern void *gTRKInputPendingPtr;
extern void TRKTestForPacket(void);
extern void TRKGetInput(void);

typedef struct {
    u32 pad[10];
} DBCommTable;

u8 TRK_Use_BBA;
s32 _MetroTRK_Has_Framing;
DBCommTable gDBCommTable;

void TRKLoadContext(void) {
}

void TRKEXICallBack(void *chan, void *dev) {
    (void)chan;
    (void)dev;
    gTRKInputPendingPtr = (void *)1;
}

void InitMetroTRKCommTable(void) {
}

void TRKUARTInterruptHandler(void) {
}

void TRK_InitializeIntDrivenUART(void) {
}

void EnableEXI2Interrupts(void) {
}

s32 TRKPollUART(void) {
    return 0;
}

s32 TRKReadUARTN(void *buf, s32 n) {
    (void)buf;
    (void)n;
    return 0;
}

s32 TRK_WriteUARTN(const void *buf, s32 n) {
    (void)buf;
    (void)n;
    return 0;
}

void ReserveEXI2Port(void) {
}

void UnreserveEXI2Port(void) {
}

void TRK_board_display(u32 val) {
    (void)val;
}

void InitializeProgramEndTrap(void) {
}
