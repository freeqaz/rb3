#include "types.h"

extern s32 TRK_InitializeNub(void);
extern void TRK_NubWelcome(void);
extern void TRK_NubMainLoop(void);
extern void TRK_TerminateNub(void);

s32 TRK_mainError;

s32 TRK_main(void) {
    TRK_mainError = TRK_InitializeNub();
    if (!TRK_mainError) {
        TRK_NubWelcome();
        TRK_NubMainLoop();
    }
    TRK_TerminateNub();
    return TRK_mainError;
}
