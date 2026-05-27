// Native shim for <revolution/TPL.h>.
// os/DiscErrorMgr_Wii.h only uses TPLPalette* (pointers); no TPL* functions are
// called on the DTA-parse path. Forward-declare the palette type.
#pragma once
#ifdef HX_NATIVE

#include "revolution/GX.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TPLDescriptor TPLDescriptor;
typedef struct TPLPalette {
    u32 version;
    u32 numImages;
    TPLDescriptor *descriptors;
} TPLPalette;

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
