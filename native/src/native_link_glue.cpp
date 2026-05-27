// native_link_glue.cpp — symbol glue for the RB3-Wii native (clang LP64) build.
//
// NOTE on divergence from rb3-xenon: the RB3-Wii decomp uses an OLDER ObjPtr
// design than the Xbox/DC3 decomps. There is no `ObjRefConcrete<T>` class and
// no out-of-line `CopyRef` machinery, and the `BinStream operator<<` for
// ObjPtr/ObjPtrList/ObjOwnerPtr/ObjDirPtr are fully-inline templates in
// obj/ObjPtr_p.h. As a result the large block of explicit template
// instantiations that rb3-xenon/native_link_glue.cpp carries does NOT apply
// here — RB3-Wii has no separate link_glue.cpp at all.
//
// This file therefore starts minimal and grows ONLY as the linker reports
// genuinely-missing symbols on the DTA-parse path.
#pragma once

#include <cstdlib>

// Data globals normally defined in excluded Wii platform TUs (Debug_Wii /
// ContentMgr_Wii / System_Wii). Referenced by compiled-but-off-path code; give
// them inert definitions so the link is clean (no null-deref PLT relocs).
bool gbDbgRequestForcedHang = false;
bool gbDbgRequestHangRecovery = false;
const char *gCurContentName = "";

// Ogg/Vorbis allocator stubs. The DTA path does not pull oggvorbis, but some
// engine TUs reference these; provide malloc-backed impls just in case the
// linker pulls them in.
extern "C" {
void *OggMalloc(int n) { return malloc((size_t)n); }
void *OggCalloc(int n, int sz) { return calloc((size_t)n, (size_t)sz); }
void *OggRealloc(void *p, int n) { return realloc(p, (size_t)n); }
void OggFree(void *p) { free(p); }
}
