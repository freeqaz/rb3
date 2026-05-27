// Native shim for the <revolution/OS.h> umbrella.
// The real Wii umbrella pulls ~40 PPC SDK headers; on native we only need the
// handful the DTA-parse compiled set actually touches.
#pragma once
#ifdef HX_NATIVE

#include "revolution/gx/GXTypes.h" // GXColor (os/Debug.cpp debug-text constants)
#include "revolution/os/OSError.h"
#include "revolution/os/OSThread.h"
#include "revolution/os/OSTime.h"

#ifdef __cplusplus
extern "C" {
#endif

// Misc OS surface used by the compiled set outside the headers above.
void OSYieldThread(void);
void OSSleepTicks(s64 ticks);

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
