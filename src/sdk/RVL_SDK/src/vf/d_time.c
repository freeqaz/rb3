#include "types.h"

static const u8 s_daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static u32 dTM_getDaysInMonth(u32 year, u32 month) {
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        return 29;
    }
    if (month < 1 || month > 12) return 0;
    return s_daysInMonth[month - 1];
}

u32 dTM_CalendarToSec(u32 year, u32 month, u32 day, u32 hour, u32 min, u32 sec) {
    u32 total_days = 0;
    u32 y, m;

    /* Count days from 1980 to current date */
    for (y = 1980; y < year; y++) {
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))
            total_days += 366;
        else
            total_days += 365;
    }
    for (m = 1; m < month; m++) {
        total_days += dTM_getDaysInMonth(year, m);
    }
    total_days += day - 1;

    return total_days * 86400 + hour * 3600 + min * 60 + sec;
}

s32 dTM_GetNowTime(u32 *year, u32 *month, u32 *day, u32 *hour, u32 *min, u32 *sec) {
    if (year) *year = 2010;
    if (month) *month = 9;
    if (day) *day = 1;
    if (hour) *hour = 0;
    if (min) *min = 0;
    if (sec) *sec = 0;
    return 0;
}
