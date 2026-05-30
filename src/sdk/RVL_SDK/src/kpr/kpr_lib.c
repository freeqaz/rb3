#include "types.h"

void *kprProcDeadKeysFP;
void *kprProcRomajiFP;

void KPRInitRegionUS(void *queue) {
    (void)queue;
}

void KPRInitRegionJP(void *queue) {
    (void)queue;
}

void KPRInitRegionEU(void *queue) {
    (void)queue;
}

void KPRInitQueue(void *queue) {
    (void)queue;
}

void KPRClearQueue(void *queue) {
    (void)queue;
}

void KPRSetMode(void *queue, s32 mode) {
    (void)queue; (void)mode;
}

void KPRPutChar(void *queue, u16 c) {
    (void)queue; (void)c;
}

u16 KPRGetChar(void *queue) {
    (void)queue;
    return 0;
}

u16 KPRLookAhead(void *queue) {
    (void)queue;
    return 0;
}

void KPRProcessAltKeypad(void *queue, u16 c) {
    (void)queue; (void)c;
}

void KPRProcessDeadKeys(void *queue, u16 c) {
    (void)queue; (void)c;
}

void KPRProcessRomaji(void *queue, u16 c) {
    (void)queue; (void)c;
}
