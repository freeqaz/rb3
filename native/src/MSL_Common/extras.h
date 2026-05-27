// Native shim for MWCC MSL_Common/extras.h.
// Provides the non-standard case-insensitive string helpers that the MWCC MSL
// supplies but POSIX/clang does not have under the same names.
//
// Used by: rndobj/MeshDeform.cpp, rndobj/Utl.cpp.
#pragma once
#ifdef HX_NATIVE

#include <strings.h>   // strcasecmp, strncasecmp (POSIX.1-2001)
#include <string.h>    // strnlen (POSIX.1-2008)

#ifndef stricmp
#define stricmp  strcasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif

#endif // HX_NATIVE
