#include "Rso_Utl.h"
#include "decomp.h"
#include "os/Debug.h"
#include "revolution/os/OSError.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include <cstddef>
#include <cstring>

extern "C" {
int RSOGetFarCodeSize(void *, void *);
int RSOLinkFar(void *, void *, unsigned long *);
int RSOLinkJump(void *, void *, void *);
int RSOIsImportSymbolResolvedAll(void *);
int RSOIsImportSymbolResolved(void *, int);
int RSOGetNumImportSymbols(void *);
const char *RSOGetImportSymbolName(void *, int);
void *WiiAllocHeapAlign(int *size, int membank, unsigned int align);
void WiiFree(void *);
}

void *MemHeapStartAddr(int heap);

#define MAX_RSO_INITERS 8
#define kRSOBufferSize 0x10EC00
#define kDefaultRSOBufferSize 0x89460
#define kPostProcBufferSize 0x10EC00

static RsoInitFunc gRsoIniters[MAX_RSO_INITERS];
static RsoDeinitFunc gRsoDeiniters[MAX_RSO_INITERS];
static int gRsoIniterCount;
static void *staticRso;
static void *g_jumpCodeBuffer;

extern void *g_pRSOReserveBuf;
uint g_RSOBufOffset;

extern void *g_pDefaultRSOBuf;
uint g_DefaultRSOBufOffset;

static bool gbCleanBuffersOnPreInit = true;

DECOMP_FORCEACTIVE(Rso_Utl, "_unresolved func.\n")

void *RsoMemAlloc2Fake(int size) {
    size = size + 31 & ~31;
    MILO_ASSERT(size >= 0, 44);
    if (u32(g_pRSOReserveBuf) + g_RSOBufOffset + size >= 0x91000000) {
        OSReport(
            "ERROR: RSOs in MEM2 need to stay below the 16MB boundary.  The game is about to crash.\n"
        );
        return 0;
    }
    g_RSOBufOffset += size;
    MILO_ASSERT(g_RSOBufOffset <= kRSOBufferSize, 53);
    if (g_pRSOReserveBuf != NULL) {
        return (void *)(u32(g_pRSOReserveBuf) + g_RSOBufOffset - size);
    } else
        return NULL;
}

void *DefaultRsoMemAlloc2(int size) {
    size = size + 31 & ~31;
    MILO_ASSERT(size >= 0, 66);
    if (u32(g_pDefaultRSOBuf) + g_DefaultRSOBufOffset + size >= 0x91000000) {
        OSReport(
            "ERROR: RSOs in MEM2 need to stay below the 16MB boundary.  The game is about to crash.\n"
        );
        return 0;
    }
    g_DefaultRSOBufOffset += size;
    MILO_ASSERT(g_DefaultRSOBufOffset <= kDefaultRSOBufferSize, 75);
    if (g_pDefaultRSOBuf != NULL) {
        return (void *)(u32(g_pDefaultRSOBuf) + g_DefaultRSOBufOffset - size);
    } else
        return NULL;
}

void RsoAddIniter(RsoInitFunc init, RsoDeinitFunc deinit) {
    MILO_ASSERT(gRsoIniterCount < MAX_RSO_INITERS, 84);
    if (gRsoIniterCount < 8) {
        gRsoIniters[gRsoIniterCount] = init;
        gRsoDeiniters[gRsoIniterCount] = deinit;
        gRsoIniterCount++;
    }
}

DECOMP_FORCEACTIVE(Rso_Utl, "fast", "main")

void *LoadRsoFile(const char *filename, unsigned int &size, void *(*alloc)(int)) {
    File *rso = NewFile(filename, 0x10002);
    if (rso == NULL) {
        char buf[0x100];
        strncpy(buf, "../../system/run/", 0x100);
        strncat(buf, filename, 0x100);
        rso = NewFile(buf, 0x10002);
        if (rso == NULL) {
            MILO_FAIL("RSO: Couldn\'t load %s", filename);
            return NULL;
        }
    }
    int fsiz = rso->Size();
    void *ret = alloc(fsiz + 31 & ~31);
    if (ret == NULL)
        return NULL;
    size = rso->Read(ret, fsiz);
    delete rso;
    return ret;
}

extern "C" int RSOLinkList(void *, unsigned char *);
extern "C" int RSOListInit();
extern "C" int RSOUnLinkList(void *);
extern "C" int RSOGetJumpCodeSize();
extern "C" void RSOMakeJumpCode(void *, void *);
extern "C" void OSEnableCodeExecOnMEM2Lo16MB();
extern "C" void DVDInit();
extern void RndGxDrawDone();

void *RsoLoad(const char *filename, unsigned char **bss, void *(*alloc)(int)) {
    uint size;
    void *rso = LoadRsoFile(filename, size, alloc);
    if (rso == NULL) {
        return NULL;
    }
    int bssSize = ((int *)rso)[7];
    if (bssSize != 0) {
        *bss = (unsigned char *)alloc(bssSize);
    }
    if (RSOLinkList(rso, *bss) == 0) {
        TheDebug.Notify(MakeString("RSO: %s: LinkList failed!\n", filename));
    }
    return rso;
}

void *StaticRsoLoad(const char *filename) {
    uint size;
    void *rso = LoadRsoFile(filename, size, DefaultRsoMemAlloc2);
    if (rso == NULL) {
        return NULL;
    }
    if (RSOListInit() == 0) {
        TheDebug.Notify(MakeString("RSO: failed to load static function list\n"));
    }
    return rso;
}

bool RsoInitDefaults() {
    bool ok = true;
    for (int i = 0; i < gRsoIniterCount; i++) {
        ok = (bool)(ok & gRsoIniters[i]((struct RSOObjectHeader *)staticRso));
    }
    return ok;
}

void RsoPostTerminate() {
    MILO_ASSERT(g_pRSOReserveBuf && kRSOBufferSize && (kRSOBufferSize >= kPostProcBufferSize), 249);
    memset(g_pRSOReserveBuf, 0, kRSOBufferSize);
}

bool RsoInit(const char *staticRsoName) {
    OSEnableCodeExecOnMEM2Lo16MB();
    DVDInit();
    staticRso = StaticRsoLoad(staticRsoName);
    if (staticRso == NULL) {
        return false;
    }
    int jcsize = RSOGetJumpCodeSize();
    if (jcsize != 0) {
        g_jumpCodeBuffer = DefaultRsoMemAlloc2(jcsize);
        RSOMakeJumpCode(staticRso, g_jumpCodeBuffer);
    }
    return RsoInitDefaults();
}

void RsoPreInit() {
    if (gbCleanBuffersOnPreInit) {
        RndGxDrawDone();
    }
    g_RSOBufOffset = 0;
}

void RsoTerminate2HelperNoFree(
    struct RSOObjectHeader *module, unsigned char *bss, unsigned long *code,
    void (*unresolvedModule)()
) {
    (*(void (**)())((char *)module + 0x28))();
    RSOUnLinkList(module);
    unresolvedModule();
}

void RsoTerminate2Helper(
    struct RSOObjectHeader *module, unsigned char *bss, unsigned long *code,
    void (*unresolvedModule)()
) {
    (*(void (**)())((char *)module + 0x28))();
    RSOUnLinkList(module);
    unresolvedModule();
    if (bss >= (unsigned char *)MemHeapStartAddr(MemFindHeap("main"))) {
        _MemFree(bss);
    } else {
        WiiFree(bss);
    }
    if ((unsigned char *)module >= (unsigned char *)MemHeapStartAddr(MemFindHeap("main"))) {
        _MemFree(module);
    } else {
        WiiFree(module);
    }
    if (code != NULL) {
        if ((unsigned char *)code >= (unsigned char *)MemHeapStartAddr(MemFindHeap("main"))) {
            _MemFree(code);
        } else {
            WiiFree(code);
        }
    }
}

#pragma push
#pragma pool_data off
bool RsoInit2Helper(
    struct RSOObjectHeader **module, const char *moduleName, unsigned char **bss,
    unsigned long **code, RsoResolvedFunc resolvedModule
) {
    *module = (struct RSOObjectHeader *)RsoLoad(moduleName, bss, DefaultRsoMemAlloc2);
    if (*module == NULL) {
        return false;
    }
    int codeSize = RSOGetFarCodeSize(*module, staticRso);
    if (codeSize <= 0) {
        MILO_FAIL("RSO: no code loaded for %s\n", moduleName);
        return false;
    }
    if (MemNumHeaps() > 0) {
        static int _x = MemFindHeap("main");
        MemPushHeap(_x);
        *code = (unsigned long *)DefaultRsoMemAlloc2(codeSize);
        MemPopHeap();
    } else {
        *code = (unsigned long *)WiiAllocHeapAlign(&codeSize, 1, 4);
    }
    int res = RSOLinkFar(*module, staticRso, *code);
    if (res < 0) {
        MILO_FAIL("RSO: %s: RSOLinkFar returned %d\n", moduleName, res);
    }
    if (RSOLinkJump(*module, staticRso, g_jumpCodeBuffer) == -1) {
        MILO_WARN("RSO: %s: RSOLinkJump failed\n", moduleName);
    }
    if (RSOIsImportSymbolResolvedAll(*module) == 0) {
        FormatString fs("Missing symbols:\n");
        TheDebug << fs.Str();
        void *importTable = (char *)*module + 0x4c;
        int numImports = RSOGetNumImportSymbols(importTable);
        for (int i = 0; i < numImports; i++) {
            if (RSOIsImportSymbolResolved(*module, i) == 0) {
                TheDebug << MakeString("  %s\n", RSOGetImportSymbolName(importTable, i));
            }
        }
        MILO_WARN("RSO: %s: Not resolved.\n", moduleName);
        return false;
    }
    (*(void (**)())((char *)*module + 0x24))();
    resolvedModule(*module);
    return true;
}
#pragma pop

bool RsoInit2HelperNoAlloc(
    struct RSOObjectHeader **module, const char *moduleName, unsigned char **bss,
    unsigned long **code, RsoResolvedFunc resolvedModule
) {
    *module = (struct RSOObjectHeader *)RsoLoad(moduleName, bss, RsoMemAlloc2Fake);
    if (*module == NULL) {
        return false;
    }
    int codeSize = RSOGetFarCodeSize(*module, staticRso);
    if (codeSize <= 0) {
        MILO_FAIL("RSO: no code loaded for %s\n", moduleName);
        return false;
    }
    *code = (unsigned long *)RsoMemAlloc2Fake(codeSize);
    int res = RSOLinkFar(*module, staticRso, *code);
    if (res < 0) {
        MILO_FAIL("RSO: %s: RSOLinkFar returned %d\n", moduleName, res);
    }
    if (RSOLinkJump(*module, staticRso, g_jumpCodeBuffer) == -1) {
        MILO_WARN("RSO: %s: RSOLinkJump failed\n", moduleName);
    }
    if (RSOIsImportSymbolResolvedAll(*module) == 0) {
        FormatString fs("Missing symbols:\n");
        TheDebug << fs.Str();
        void *importTable = (char *)*module + 0x4c;
        int numImports = RSOGetNumImportSymbols(importTable);
        for (int i = 0; i < numImports; i++) {
            if (RSOIsImportSymbolResolved(*module, i) == 0) {
                TheDebug << MakeString("  %s\n", RSOGetImportSymbolName(importTable, i));
            }
        }
        MILO_WARN("RSO: %s: Not resolved.\n", moduleName);
        return false;
    }
    (*(void (**)())((char *)*module + 0x24))();
    resolvedModule(*module);
    return true;
}
