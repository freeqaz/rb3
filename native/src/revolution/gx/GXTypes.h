// Native shim for <revolution/gx/GXTypes.h>.
// rndobj/Mesh.h, rndwii/Tex.h and os/Debug.cpp reference a few GX value types
// (GXColor for debug-text constants, texture enums). None of the GX pipeline
// runs on the DTA-parse path; only the type layouts/enums are needed.
#pragma once
#ifdef HX_NATIVE

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GXColor {
    u8 r, g, b, a;
} GXColor;

typedef struct _GXColorS10 {
    s16 r, g, b, a;
} GXColorS10;

typedef u8 GXBool;
typedef int GXTexWrapMode;
typedef int GXTexFilter;
typedef int GXTlutFmt;
typedef int GXTexFmt;
typedef int GXTexMapID;
typedef int GXColorSrc;
typedef int GXAttr;
typedef int GXAttrType;
typedef int GXCompCnt;
typedef int GXCompType;
typedef int GXPrimitive;
typedef int GXVtxFmt;

// Underscore-tag spellings the rndwii headers use directly
// (e.g. RndGXBegin(_GXPrimitive, _GXVtxFmt), Tex.h _GXTexMapID).
typedef int _GXPrimitive;
typedef int _GXVtxFmt;
typedef int _GXTexMapID;
typedef int _GXTexFmt;
typedef int _GXTexWrapMode;
typedef int _GXTexFilter;

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
