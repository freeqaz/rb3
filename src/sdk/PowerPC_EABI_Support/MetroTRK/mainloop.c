#include "types.h"

extern void TRKTestForPacket(void);
extern void TRKGetInput(void);
extern void TRKProcessInput(void);
extern s32 TRKGetNextEvent(void *event);
extern void *gTRKInputPendingPtr;

void TRK_NubMainLoop(void) {
    while (1) {
        TRKTestForPacket();
        if (gTRKInputPendingPtr != NULL) {
            TRKGetInput();
            TRKProcessInput();
        }
    }
}
