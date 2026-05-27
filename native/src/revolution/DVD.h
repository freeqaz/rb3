// Native shim for <revolution/DVD.h>.
// The DTA-parse compiled set only uses DVDFileInfo as a (sized) opaque type
// (os/File.h declares GetFileHandle(DVDFileInfo*&); CDReader.cpp stores a
// std::vector<DVDFileInfo>). No DVD* functions are called on this path, so we
// only reproduce the struct layout (no I/O entry points).
#pragma once
#ifdef HX_NATIVE

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct DVDCommandBlock;
typedef void (*DVDCBCallback)(s32 result, struct DVDCommandBlock *block);

typedef struct DVDCommandBlock {
    struct DVDCommandBlock *next;
    struct DVDCommandBlock *prev;
    u32 command;
    s32 state;
    u32 offset;
    u32 length;
    void *addr;
    u32 currTransferSize;
    u32 transferredSize;
    void *id;
    DVDCBCallback callback;
    void *userData;
} DVDCommandBlock;

struct DVDFileInfo;
typedef void (*DVDCallback)(s32 result, struct DVDFileInfo *fileInfo);

typedef struct DVDFileInfo {
    DVDCommandBlock cb;
    u32 startAddr;
    u32 length;
    DVDCallback callback;
} DVDFileInfo;

#ifdef __cplusplus
}
#endif

#endif // HX_NATIVE
