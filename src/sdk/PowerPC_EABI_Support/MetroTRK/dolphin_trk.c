#include "types.h"

extern s32 TRK_main(void);
extern void TRKInitializeEventQueue(void);
extern void TRKInitializeSerialHandler(void);
extern s32 TRKInitializeMessageBuffers(void);
extern void TRK_InitializeEndian(void);
extern void TRKPostEvent(void *event);

u32 TRK_ISR_OFFSETS[15] = {
    0x100, 0x200, 0x300, 0x400, 0x500,
    0x600, 0x700, 0x800, 0x900, 0xC00,
    0xD00, 0xE00, 0xF00, 0x1000, 0x1300
};

u32 lc_base;

void InitMetroTRK(void) {
    TRK_main();
}

void InitMetroTRK_BBA(void) {
    TRK_main();
}

void EnableMetroTRKInterrupts(void) {
}

s32 TRKTargetTranslate(u32 addr) {
    return (s32)addr;
}

void __TRK_copy_vectors(void) {
}

void TRKInitializeTarget(void) {
}

void __TRKreset(void) {
}
