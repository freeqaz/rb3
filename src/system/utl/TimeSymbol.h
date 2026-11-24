#pragma once
#include "utl/Symbol.h"

// this header only exists because the Symbol time doesn't play nicely with MSL_C's
// function also called time. The libc++ <chrono> chain transitively
// pulls <time.h> into Symbols.cpp before the global declaration, which also
// trips the function-vs-variable redefinition. On Emscripten and macOS we rename the
// C++ identifier to _hmx_time_sym (the string-interned Symbol value "time" is
// unchanged at runtime; equality is by pointer). Both this header and
// Symbols.cpp apply the same rename so the extern matches the definition.
#if defined(__EMSCRIPTEN__) || (defined(HX_NATIVE) && defined(__APPLE__))
#define time _hmx_time_sym
#endif
extern Symbol time;
