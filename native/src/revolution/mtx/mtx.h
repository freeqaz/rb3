// Native shim for <revolution/mtx/mtx.h>.
// rndwii/Rnd.h embeds Mtx/Mtx44 fields by value; only the type layouts are
// needed on the DTA-parse path (no PSMTX*/C_MTX* funcs are called).
#pragma once
#ifdef HX_NATIVE

#include "revolution/mtx/vec.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef float Mtx[3][4];   // 48 bytes
typedef float Mtx44[4][4]; // 64 bytes
typedef float PSQuaternion[4];

#define MTXDegToRad(a) ((a) * 0.01745329252f)

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
