// Native shim for <revolution/nand/nand.h>.
// utl/BufStreamNAND.h embeds a NANDFileInfo by value; only the layout is
// needed (no NAND* functions are called on the DTA-parse path).
#pragma once
#ifdef HX_NATIVE

#include "types.h"

typedef struct NANDFileInfo {
    s32 fileDescriptor;
    s32 origFd;
    char origPath[64];
    char tmpPath[64];
    u8 accType;
    u8 stage;
    u8 mark;
} NANDFileInfo;

typedef s32 NANDResult;

#endif // HX_NATIVE
