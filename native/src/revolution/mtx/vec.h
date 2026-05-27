// Native shim for <revolution/mtx/vec.h>.
// Only the Vec type is needed transitively (rndwii/Rnd.h); no PSVEC* funcs are
// called on the DTA-parse path.
#pragma once
#ifdef HX_NATIVE

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Vec {
    float x;
    float y;
    float z;
} Vec;

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
