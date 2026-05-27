// Native shim for <revolution/os/OSThread.h>.
// The RB3-Wii compiled set (MakeString.cpp, MemMgr.cpp) only ever uses
// OSThread* as an OPAQUE thread identity (pointer compares), never derefs its
// fields on the native DTA path. We provide a minimal opaque struct + the one
// accessor the engine calls.
#pragma once
#ifdef HX_NATIVE

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque on native — identity only. Keep a tiny field so sizeof()>0.
typedef struct OSThread {
    u8 *stackBase;
    u32 *stackEnd;
} OSThread;

OSThread *OSGetCurrentThread(void);

// Mutex — os/CritSec.h and os/SynchronizationEvent.h embed an OSMutex by value
// and call OSInitMutex/OSLockMutex/OSUnlockMutex. Back it with a pthread mutex
// (recursive). The struct must be large enough to hold a pthread_mutex_t; on
// glibc/x86_64 that's 40 bytes — 64 is a safe over-allocation.
typedef struct OSMutex {
    unsigned char _opaque[64];
} OSMutex;

void OSInitMutex(OSMutex *mutex);
void OSLockMutex(OSMutex *mutex);
void OSUnlockMutex(OSMutex *mutex);
int OSTryLockMutex(OSMutex *mutex);

// Condition variable — os/SynchronizationEvent.h embeds an OSCond by value.
// Opaque on native (the DTA path doesn't wait on it); sized for a pthread_cond_t.
typedef struct OSCond {
    unsigned char _opaque[64];
} OSCond;

void OSInitCond(OSCond *cond);
void OSWaitCond(OSCond *cond, OSMutex *mutex);
void OSSignalCond(OSCond *cond);

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
