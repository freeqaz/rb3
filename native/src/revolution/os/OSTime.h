// Native shim for <revolution/os/OSTime.h>.
// Keeps the Wii tick<->time macros (the engine uses OSTicksToSeconds etc.) but
// drops the PPC OSHardware.h include. We hardcode the Wii bus clock so the
// tick math compiles; the actual time functions are implemented in
// rvl_shims.cpp on top of clock_gettime, returning ticks at this same rate.
#pragma once
#ifdef HX_NATIVE

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef s64 OSTime;

// Wii: bus clock 243 MHz; time base = bus/4.
#ifndef OS_BUS_CLOCK_SPEED
#define OS_BUS_CLOCK_SPEED 243000000u
#endif
#define OS_TIME_SPEED (OS_BUS_CLOCK_SPEED / 4)

#define OSTicksToSeconds(x) ((x) / (OS_TIME_SPEED))
#define OSTicksToMilliseconds(x) ((x) / (OS_TIME_SPEED / 1000))
#define OSTicksToMicroseconds(x) (((x) * 8) / (OS_TIME_SPEED / 125000))
#define OSTicksToNanoseconds(x) (((x) * 8000) / (OS_TIME_SPEED / 125000))

#define OSSecondsToTicks(x) ((x) * (OS_TIME_SPEED))
#define OSMillisecondsToTicks(x) ((x) * (OS_TIME_SPEED / 1000))
#define OSMicrosecondsToTicks(x) ((x) * (OS_TIME_SPEED / 125000) / 8)
#define OSNanosecondsToTicks(x) ((x) * (OS_TIME_SPEED / 125000) / 8000)

typedef struct OSCalendarTime {
    s32 sec;
    s32 min;
    s32 hour;
    s32 month_day;
    s32 month;
    s32 year;
    s32 week_day;
    s32 year_day;
    s32 msec;
    s32 usec;
} OSCalendarTime;

s64 OSGetTime(void);
s32 OSGetTick(void);
void OSTicksToCalendarTime(s64 time, OSCalendarTime *cal);
s64 OSCalendarTimeToTicks(const OSCalendarTime *cal);

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
