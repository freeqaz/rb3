// mwcc_compat.h — force-included into RB3's matched-fork sources on the native
// (clang LP64) build. The RB3 decomp is written for MetroWerks CodeWarrior 4.3
// targeting Wii PowerPC (Gekko/Broadway). This shim absorbs the MWCC-specific
// intrinsics and keywords clang doesn't provide, so the matched fork compiles
// under clang without editing it for asm-match.
//
// Analog of dc3-decomp/native/src/msvc_compat.h. Expected to be much THINNER
// than DC3's: clang and MWCC agree on far more than clang and MSVC do.
//
// Start minimal; grow as Phase 1 bring-up surfaces missing pieces. PowerPC
// paired-singles inline-asm blocks (math/Vec.h, math/Mtx.h, math/Geo.cpp,
// math/Rot.cpp, bandobj/BandIKEffector.cpp, bandobj/InlineHelp.cpp,
// char/CharForeTwist.cpp, char/CharHair.cpp, rndobj/Part.cpp) are NOT handled
// here — each gets an `#ifdef __MWERKS__ / #else <C++ fallback> / #endif` wrap
// in its own file (see roadmap §"MWCC paired-singles").
#pragma once

#ifdef HX_NATIVE

// Foundational decomp headers. In the MWCC/ninja build these reach every TU via
// the project's include setup + PCH; many leaf headers (e.g. utl/Symbol.h)
// assume the macro vocabulary (DONT_INLINE_CLASS, NEW_POOL_OVERLOAD, ALIGN, ...)
// and the s32/u32/uint typedefs + stricmp shim are already in scope. Force-
// including these here guarantees that for every native TU (C and C++).
#include "compiler_macros.h"
#include "types.h"

#ifdef __cplusplus
// C++-only leaf headers assume the pool-allocation macros (NEW_POOL_OVERLOAD /
// DELETE_POOL_OVERLOAD) and the global _MemAlloc/_MemFree/_MemAllocTemp
// declarations are already visible (e.g. utl/StringTable.h, obj/Data.h,
// obj/ObjPtr_p.h, os/ArkFile.h). In the MWCC build a global PCH supplies them;
// here we force-include PoolAlloc.h + MemMgr.h. Guarded for C++ since they
// declare classes (the C DataFlex.c TU also gets this force-include).
#include "utl/MemMgr.h"
#include "utl/PoolAlloc.h"

// Common CRT headers the decomp leaf TUs assume in scope (UINT_MAX, memset,
// snprintf, etc.) but reach via the global PCH on MWCC.
#include <climits>
#include <cstring>
#endif

#include <alloca.h>

// PowerPC paired-singles type. On Gekko/Broadway, __vec2x32float__ is the
// compiler's builtin "two packed 32-bit floats in one FPR" type used by the
// psq_l/psq_st/ps_* intrinsics. clang has no such type. The matched-fork math
// headers (math/Vec.h, math/Mtx.h, math/Color.h) declare `register
// __vec2x32float__ x;` even when the asm bodies are #ifdef-guarded out, and the
// PSQ_MOVE macro reinterpret-casts through it (a 2-float memcpy). Defining it as
// a 2-float POD makes all of that compile and behave correctly on native; the
// actual paired-singles asm blocks are individually #ifdef __MWERKS__-gated.
#ifdef __cplusplus
typedef struct __vec2x32float__ {
    float v[2];
} __vec2x32float__;
#endif

// MWCC exposes stack allocation as the __alloca intrinsic; clang uses alloca().
#ifndef __alloca
#define __alloca(n) alloca(n)
#endif

// MWCC pragmas clang doesn't recognize would warn under -Wunknown-pragmas; the
// native build already compiles the matched fork with -w, so these are silently
// ignored (no runtime semantics on the native target):
//   #pragma pool_data / dont_inline / fp_contract / force_active / aux
// Nothing to define here for them — listed for documentation.

#endif // HX_NATIVE
