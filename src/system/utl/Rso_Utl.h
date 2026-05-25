#ifndef UTL_RSOUTL_H
#define UTL_RSOUTL_H

#include "os/File.h"

struct RSOObjectHeader;

typedef bool (*RsoInitFunc)(struct RSOObjectHeader *);
typedef void (*RsoDeinitFunc)(void);
typedef void (*RsoResolvedFunc)(const struct RSOObjectHeader *);

void *RsoMemAlloc2Fake(int size);
void *DefaultRsoMemAlloc2(int size);
void RsoAddIniter(RsoInitFunc, RsoDeinitFunc);
void *LoadRsoFile(const char *, unsigned int &, void *(*)(int));
bool RsoInit2Helper(
    struct RSOObjectHeader **module, const char *moduleName, unsigned char **bss,
    unsigned long **code, RsoResolvedFunc resolvedModule
);
bool RsoInit2HelperNoAlloc(
    struct RSOObjectHeader **module, const char *moduleName, unsigned char **bss,
    unsigned long **code, RsoResolvedFunc resolvedModule
);

#endif // UTL_RSOUTL_H