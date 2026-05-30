#include "types.h"

extern void TRKRestoreExtended1Block(void);
extern u8 gTRKRestoreFlags[];

void TRKTargetContinue(void) {
    gTRKRestoreFlags[0] = 0;
    gTRKRestoreFlags[1] = 0;
    gTRKRestoreFlags[2] = 0;
    TRKRestoreExtended1Block();
}
