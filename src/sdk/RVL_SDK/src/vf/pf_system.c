#include "types.h"
#include "revolution/os/OSTime.h"

typedef struct {
    u16 date;
    u16 time;
} VFSysTime;

s32 VFipf_sys_set;

void VFiPFSYS_initializeSYS(void) {}

s32 VFiPFSYS_GetCurrentContextID(void) {
    return VFipf_sys_set++;
}

void VFiPFSYS_TimeStamp(VFSysTime *ts) {
    OSCalendarTime cal;
    OSTicksToCalendarTime(OSGetTime(), &cal);
    ts->date = (u16)(((cal.year - 1980) << 9) | ((cal.month + 1) << 5) | cal.month_day);
    ts->time = (u16)((cal.hour << 11) | (cal.min << 5) | (cal.sec >> 1));
}
