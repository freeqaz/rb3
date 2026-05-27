// Native shim for <revolution/GX.h> (Wii GX graphics).
// On the DTA-parse path only the texture-object layout is needed: utl/BINK.h
// embeds several _GXTexObj by value. The real public GXTexObj is 32 bytes
// (GX_DECL_PUBLIC_STRUCT(GXTexObj, 32)). No GX* functions are called.
#pragma once
#ifdef HX_NATIVE

#include "revolution/gx/GXTypes.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GXTexObj {
    u8 dummy[32];
} GXTexObj;

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
