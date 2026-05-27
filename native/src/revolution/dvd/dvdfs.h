// Native shim for <revolution/dvd/dvdfs.h>.
// os/AsyncFileCNT.h / os/AsyncFileHolmes.h only use DVDFileInfo* in virtual
// signatures; reuse the type from the DVD.h shim. No DVD filesystem functions
// run on the DTA-parse path.
#pragma once
#ifdef HX_NATIVE
#include "revolution/DVD.h"
#endif
