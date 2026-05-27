#include "revolution/dvd/dvdqueue.h"
#include "revolution/dvd/dvd.h"
#include "revolution/OS.h"

// Each queue head is a sentinel with only next/prev pointers (8 bytes each entry)
typedef struct {
    DVDCommandBlock* next;
    DVDCommandBlock* prev;
} DVDQueueHead;

static DVDQueueHead WaitingQueue[4];

void __DVDClearWaitingQueue(void) {
    WaitingQueue[0].next = (DVDCommandBlock*)&WaitingQueue[0];
    WaitingQueue[0].prev = (DVDCommandBlock*)&WaitingQueue[0];
    WaitingQueue[1].next = (DVDCommandBlock*)&WaitingQueue[1];
    WaitingQueue[1].prev = (DVDCommandBlock*)&WaitingQueue[1];
    WaitingQueue[2].next = (DVDCommandBlock*)&WaitingQueue[2];
    WaitingQueue[2].prev = (DVDCommandBlock*)&WaitingQueue[2];
    WaitingQueue[3].next = (DVDCommandBlock*)&WaitingQueue[3];
    WaitingQueue[3].prev = (DVDCommandBlock*)&WaitingQueue[3];
}

BOOL __DVDPushWaitingQueue(s32 prio, DVDCommandBlock* block) {
    BOOL enabled = OSDisableInterrupts();
    WaitingQueue[prio].prev->next = block;
    block->prev = WaitingQueue[prio].prev;
    block->next = (DVDCommandBlock*)&WaitingQueue[prio];
    WaitingQueue[prio].prev = block;
    OSRestoreInterrupts(enabled);
    return TRUE;
}

DVDCommandBlock* __DVDPopWaitingQueue(void) {
    s32 i;
    DVDCommandBlock* block;
    BOOL enabled;
    enabled = OSDisableInterrupts();
    for (i = 0; i < 4; i++) {
        if (WaitingQueue[i].next != (DVDCommandBlock*)&WaitingQueue[i]) {
            OSRestoreInterrupts(enabled);
            enabled = OSDisableInterrupts();
            block = WaitingQueue[i].next;
            WaitingQueue[i].next = block->next;
            block->next->prev = (DVDCommandBlock*)&WaitingQueue[i];
            OSRestoreInterrupts(enabled);
            block->next = NULL;
            block->prev = NULL;
            return block;
        }
    }
    OSRestoreInterrupts(enabled);
    return NULL;
}

BOOL __DVDCheckWaitingQueue(void) {
    s32 i;
    BOOL enabled = OSDisableInterrupts();
    for (i = 0; i < 4; i++) {
        if (WaitingQueue[i].next != (DVDCommandBlock*)&WaitingQueue[i]) {
            OSRestoreInterrupts(enabled);
            return TRUE;
        }
    }
    OSRestoreInterrupts(enabled);
    return FALSE;
}

DVDCommandBlock* __DVDGetNextWaitingQueue(void) {
    s32 i;
    DVDCommandBlock* block;
    BOOL enabled = OSDisableInterrupts();
    for (i = 0; i < 4; i++) {
        block = WaitingQueue[i].next;
        if (block != (DVDCommandBlock*)&WaitingQueue[i]) {
            OSRestoreInterrupts(enabled);
            return block;
        }
    }
    OSRestoreInterrupts(enabled);
    return NULL;
}

BOOL __DVDDequeueWaitingQueue(DVDCommandBlock* block) {
    DVDCommandBlock* prev;
    DVDCommandBlock* next;
    BOOL enabled = OSDisableInterrupts();
    prev = (DVDCommandBlock*)block->prev;
    next = block->next;
    if (prev == NULL || next == NULL) {
        OSRestoreInterrupts(enabled);
        return FALSE;
    }
    prev->next = next;
    next->prev = prev;
    OSRestoreInterrupts(enabled);
    return TRUE;
}
