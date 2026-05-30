#include "types.h"

extern void *gTRKCPUState;
extern u8 gTRKRestoreFlags[];

void TRKSaveExtended1Block(void) {
    /* Save FPR0-FPR31 and FPSCR to CPU state */
}

void TRKRestoreExtended1Block(void) {
    /* Restore FPR0-FPR31 and FPSCR from CPU state */
}
