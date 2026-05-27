// rvl_shims.cpp — POSIX/malloc implementations of the Wii SDK (revolution/*)
// calls the RB3-Wii matched fork makes on the native (clang LP64) DTA-parse
// path. Replaces the excluded platform TUs: Mem_Wii.cpp (the malloc-backed
// allocator surface) and the OS/time/thread/report calls. Shape mirrors
// dc3-decomp/native/src/xdk_shims.cpp (which does the same for the Xbox SDK).
//
// Surface implemented (discovered from the compiled obj/utl/os/math set):
//   OSReport / OSVReport / OSPanic / OSError / OSFatal  -> stderr / abort
//   OSGetCurrentThread / gMainThreadID                  -> stable fake thread id
//   OSGetTime / OSGetTick / OSTicksToCalendarTime       -> clock_gettime
//   OSYieldThread / OSSleepTicks                         -> sched_yield / nanosleep
//   OSEnableCodeExecOnMEM2Lo16MB                         -> no-op
//   WiiMalloc / WiiFree / WiiAllocHeapAlign              -> malloc/free (from Mem_Wii.cpp)
//   __sys_alloc / __sys_free                             -> malloc/free
#ifdef HX_NATIVE

#include "revolution/OS.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <sched.h>

// ============================================================================
// Reporting / panics
// ============================================================================
extern "C" void OSReport(const char *msg, ...) {
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
}

extern "C" void OSVReport(const char *msg, va_list arg) { vfprintf(stderr, msg, arg); }

extern "C" void OSPanic(const char *file, int line, const char *msg, ...) {
    fprintf(stderr, "OSPanic at %s:%d: ", file ? file : "?", line);
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fputc('\n', stderr);
    abort();
}

extern "C" void OSFatal(GXColor, GXColor, const char *msg) {
    fprintf(stderr, "OSFatal: %s\n", msg ? msg : "");
    abort();
}

extern "C" void OSEnableCodeExecOnMEM2Lo16MB(void) {} // no MEM2 on native

// ============================================================================
// Threads — identity only (DTA path is single-threaded; the engine compares
// the current thread pointer for its per-thread string buffers).
// ============================================================================
extern "C" OSThread *OSGetCurrentThread(void) {
    // One opaque "thread" object per pthread. Stable identity is all the
    // engine needs (it pointer-compares against cached ids).
    static __thread OSThread tls;
    return &tls;
}

// Defined in ThreadCall_Wii.cpp on Wii (excluded here). MemMgr asserts that the
// main-thread id matches; leaving it null makes that assert always pass.
extern "C" {
OSThread *gMainThreadID = nullptr;
}

extern "C" void OSYieldThread(void) { sched_yield(); }

// --- Mutex (OSMutex backed by a recursive pthread_mutex_t) ---
static_assert(sizeof(pthread_mutex_t) <= sizeof(OSMutex), "OSMutex too small");

extern "C" void OSInitMutex(OSMutex *mutex) {
    pthread_mutex_t *m = (pthread_mutex_t *)mutex;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &attr);
    pthread_mutexattr_destroy(&attr);
}

extern "C" void OSLockMutex(OSMutex *mutex) {
    pthread_mutex_lock((pthread_mutex_t *)mutex);
}

extern "C" void OSUnlockMutex(OSMutex *mutex) {
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

extern "C" int OSTryLockMutex(OSMutex *mutex) {
    return pthread_mutex_trylock((pthread_mutex_t *)mutex) == 0 ? 1 : 0;
}

// --- Condition variable (OSCond backed by pthread_cond_t) ---
static_assert(sizeof(pthread_cond_t) <= sizeof(OSCond), "OSCond too small");

extern "C" void OSInitCond(OSCond *cond) {
    pthread_cond_init((pthread_cond_t *)cond, nullptr);
}
extern "C" void OSWaitCond(OSCond *cond, OSMutex *mutex) {
    pthread_cond_wait((pthread_cond_t *)cond, (pthread_mutex_t *)mutex);
}
extern "C" void OSSignalCond(OSCond *cond) {
    pthread_cond_signal((pthread_cond_t *)cond);
}

extern "C" void OSSleepTicks(s64 ticks) {
    if (ticks <= 0)
        return;
    // ticks at OS_TIME_SPEED (bus/4) -> nanoseconds.
    long long ns = (long long)((double)ticks * 1e9 / (double)OS_TIME_SPEED);
    struct timespec ts;
    ts.tv_sec = ns / 1000000000LL;
    ts.tv_nsec = ns % 1000000000LL;
    nanosleep(&ts, nullptr);
}

// ============================================================================
// Time — back the Wii tick clock with CLOCK_MONOTONIC / wall clock.
// ============================================================================
extern "C" s64 OSGetTime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    // Express wall-clock as Wii ticks (OS_TIME_SPEED ticks/sec) so the
    // tick<->calendar conversion below round-trips.
    return (s64)ts.tv_sec * (s64)OS_TIME_SPEED +
        (s64)((double)ts.tv_nsec * (double)OS_TIME_SPEED / 1e9);
}

extern "C" s32 OSGetTick(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (s32)((s64)ts.tv_sec * (s64)OS_TIME_SPEED +
                 (s64)((double)ts.tv_nsec * (double)OS_TIME_SPEED / 1e9));
}

extern "C" void OSTicksToCalendarTime(s64 ticks, OSCalendarTime *cal) {
    if (!cal)
        return;
    time_t secs = (time_t)(ticks / (s64)OS_TIME_SPEED);
    s64 rem = ticks % (s64)OS_TIME_SPEED;
    struct tm tmv;
    localtime_r(&secs, &tmv);
    cal->sec = tmv.tm_sec;
    cal->min = tmv.tm_min;
    cal->hour = tmv.tm_hour;
    cal->month_day = tmv.tm_mday;
    cal->month = tmv.tm_mon;       // 0-based, matches Wii
    cal->year = tmv.tm_year + 1900; // DateTime.cpp subtracts 1900 back off
    cal->week_day = tmv.tm_wday;
    cal->year_day = tmv.tm_yday;
    cal->msec = (s32)(rem / (s64)(OS_TIME_SPEED / 1000));
    cal->usec = (s32)((rem / (s64)(OS_TIME_SPEED / 1000000)) % 1000);
}

extern "C" s64 OSCalendarTimeToTicks(const OSCalendarTime *cal) {
    if (!cal)
        return 0;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_sec = cal->sec;
    tmv.tm_min = cal->min;
    tmv.tm_hour = cal->hour;
    tmv.tm_mday = cal->month_day;
    tmv.tm_mon = cal->month;
    tmv.tm_year = cal->year - 1900;
    time_t secs = mktime(&tmv);
    return (s64)secs * (s64)OS_TIME_SPEED;
}

// ============================================================================
// Memory — Mem_Wii.cpp is excluded; back the engine's Wii allocator surface
// (which MemMgr.cpp calls) with the host malloc.
// ============================================================================
extern "C" void *WiiMalloc(int size) { return malloc(size > 0 ? (size_t)size : 1); }

extern "C" void WiiFree(void *buf) { free(buf); }

extern "C" void *WiiAllocHeapAlign(int *size, int /*membank*/, u32 align) {
    if (!size || *size <= 0)
        return nullptr;
    void *p = nullptr;
    size_t a = align ? (size_t)align : 32;
    // posix_memalign requires power-of-two >= sizeof(void*).
    if (a < sizeof(void *) || (a & (a - 1)) != 0)
        a = 32;
    if (posix_memalign(&p, a, (size_t)*size) != 0)
        return nullptr;
    return p;
}

extern "C" void *__sys_alloc(int siz) { return malloc(siz > 0 ? (size_t)siz : 1); }
extern "C" void __sys_free(void *buf) { free(buf); }

#endif // HX_NATIVE
