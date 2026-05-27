// Native shim for <revolution/os/OSError.h>.
// Provides OSReport/OSPanic/OSError and the few extras the compiled set uses.
#pragma once
#ifdef HX_NATIVE

#include "revolution/gx/GXTypes.h" // GXColor (OSFatal signature)
#include "types.h"
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSError_FileLine(file_, line_, ...) OSPanic(file_, line_, __VA_ARGS__)
#define OSError_Line(line_, ...) OSError_FileLine(__FILE__, line_, __VA_ARGS__)
#define OSError(...) OSError_Line(__LINE__, __VA_ARGS__)

void OSReport(const char *msg, ...) __attribute__((format(printf, 1, 2)));
void OSPanic(const char *file, int line, const char *msg, ...)
    __attribute__((format(printf, 3, 4)));
void OSVReport(const char *msg, va_list arg);
void OSFatal(GXColor textColor, GXColor bgColor, const char *msg);

// Rso_Utl.cpp references this; no-op on native (no MEM2 exec region).
void OSEnableCodeExecOnMEM2Lo16MB(void);

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
