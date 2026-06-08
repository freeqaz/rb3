#include "test_helpers.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csetjmp>
#include <unistd.h>     // chdir, access

// Draw-crash recovery globals — defined in main_native.cpp for rb3-native, but
// that TU (which also defines main()) is excluded from rb3-tests since gtest_main
// provides main(). App.cpp + rb3_http_handlers.cpp reference them as externs, so
// define them here for the test binary.
sigjmp_buf gDrawJmpBuf;
bool gDrawJmpBufSet = false;

#include "utl/Symbol.h"
#include "os/System.h"
#include "obj/Object.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"

// rndobj + synth object classes whose factories the milo loader needs to
// instantiate the live object graph (mirrors main_native.cpp::RegisterCommonFactories).
#include "rndobj/Dir.h"
#include "rndobj/Tex.h"
#include "rndobj/Group.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/PropAnim.h"
#include "synth/Sfx.h"
#include "synth/SynthSample.h"
#include "synth/Sequence.h"
#include "synth/MidiInstrument.h"

extern void InitMakeString();
extern DataArray *gSystemConfig;

static bool sSymbolInitialized = false;
static bool sEngineInitialized = false;
static bool sEngineOk = false;

void EnsureSymbolInit() {
    if (sSymbolInitialized)
        return;
    sSymbolInitialized = true;
    InitMakeString();
    Symbol::Init();
}

// Mirrors main_native.cpp::RegisterCommonFactories — registers the obj/rndobj/
// synth factories the milo loader constructs by class name.
static void RegisterCommonFactories() {
    Hmx::Object::Init();
    ObjectDir::Register();
    REGISTER_OBJ_FACTORY(RndDir)
    REGISTER_OBJ_FACTORY(RndTex)
    REGISTER_OBJ_FACTORY(RndGroup)
    REGISTER_OBJ_FACTORY(EventTrigger)
    REGISTER_OBJ_FACTORY(RndPropAnim)
    REGISTER_OBJ_FACTORY(Sfx)
    REGISTER_OBJ_FACTORY(SynthSample)
    REGISTER_OBJ_FACTORY(MidiInstrument)
    REGISTER_OBJ_FACTORY(Sequence)
    REGISTER_OBJ_FACTORY(WaitSeq)
    REGISTER_OBJ_FACTORY(RandomGroupSeq)
    REGISTER_OBJ_FACTORY(SerialGroupSeq)
    REGISTER_OBJ_FACTORY(ParallelGroupSeq)
    REGISTER_OBJ_FACTORY(SfxSeq)
}

bool EnsureEngineInit() {
    if (sEngineInitialized)
        return sEngineOk;
    sEngineInitialized = true;
    sSymbolInitialized = true; // SystemInit includes Symbol init

    const char *dataDir = getenv("RB3_DATA");
    if (!dataDir)
        dataDir = "/home/free/code/milohax/rb3/orig-assets/extracted";
    if (access(dataDir, R_OK) != 0 || chdir(dataDir) != 0) {
        fprintf(stderr, "test: engine init — data dir '%s' unavailable\n", dataDir);
        return false;
    }

    setenv("MILO_HEADLESS", "1", 1);
    TheLoadMgr.mPlatform = kPlatformXBox;

    static char arg0[] = "rb3-tests";
    static char *fakeArgv[] = {arg0, nullptr};
    SetSystemArgs(1, fakeArgv);

    SystemPreInit("config/band_preinit_keep.dta");
    SystemInit("config/band_keep.dta");
    RegisterCommonFactories();

    sEngineOk = true;
    return true;
}
