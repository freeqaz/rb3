#include "types.h"

typedef struct TRKEvent {
    s32 type;
    s32 data[5];
} TRKEvent;

typedef struct TRKEventQueue {
    s32 count;
    s32 head;
    s32 tail;
    TRKEvent events[3];
} TRKEventQueue;

TRKEventQueue gTRKEventQueue;

void TRKInitializeEventQueue(void) {
    gTRKEventQueue.count = 0;
    gTRKEventQueue.head = 0;
    gTRKEventQueue.tail = 0;
}

s32 TRKGetNextEvent(TRKEvent *event) {
    if (gTRKEventQueue.count == 0) {
        return -1;
    }
    *event = gTRKEventQueue.events[gTRKEventQueue.head];
    gTRKEventQueue.head = (gTRKEventQueue.head + 1) % 3;
    gTRKEventQueue.count--;
    return 0;
}

s32 TRKPostEvent(TRKEvent *event) {
    if (gTRKEventQueue.count >= 3) {
        return -1;
    }
    gTRKEventQueue.events[gTRKEventQueue.tail] = *event;
    gTRKEventQueue.tail = (gTRKEventQueue.tail + 1) % 3;
    gTRKEventQueue.count++;
    return 0;
}

s32 TRKConstructEvent(TRKEvent *event, s32 type) {
    event->type = type;
    return 0;
}

void TRKDestructEvent(TRKEvent *event) {
    (void)event;
}
