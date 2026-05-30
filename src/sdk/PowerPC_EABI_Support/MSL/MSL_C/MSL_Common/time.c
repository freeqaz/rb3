#include "revolution/OS.h"
#include "time.h"

clock_t clock(void) {
    return OSGetTick();
}
