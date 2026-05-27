// rb3 (Wii decomp) native — full-engine boot harness (milestone b / Phase 1).
//
// Boots the minimal Milo engine state under clang LP64 (the engine is linked in
// its real, GFX-off form — see native/CMakeLists.txt). Two modes:
//
//   FLOOR   (no argv): init the object/symbol/data subsystems, register the
//           common Milo object factories, and exit cleanly. Proves rb3-native
//           links the full (GFX-off) engine and runs to a controlled exit.
//
//   STRETCH (argv[1] = path to a .milo / .milo_xbox): load the scene via RB3's
//           object system (DirLoader::LoadObjects, the same path the engine's
//           DirLoader tests use) and recursively print the scene tree —
//           each object's name + class, indented by subdir depth.
//
// Mirrors main_dta.cpp's minimal-init style: we do NOT boot the full game
// App/UI flow (no SystemPreInit/SystemInit, no renderer, no audio device). We
// bring up exactly the subsystems the milo load path touches.

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "obj/ObjMacros.h"
#include "utl/Loader.h"
#include "utl/FilePath.h"
#include "utl/Symbol.h"
#include "utl/ChunkStream.h"
#include "utl/BinStream.h"
#include "os/Endian.h"

// rndobj + synth object classes whose factories we register so the loader can
// instantiate the live object graph (these forks are now clang-LP64-clean).
#include "rndobj/Dir.h"
#include "rndobj/Tex.h"
#include "rndobj/Group.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/PropAnim.h"
#include "synth/Sfx.h"
#include "synth/SynthSample.h"
#include "synth/Sequence.h"
#include "synth/MidiInstrument.h"
#include "synth/Synth.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern void InitMakeString();

// gSystemConfig (os/System.cpp) is the global DTA tree that SystemConfig()
// returns. The full SystemInit reads config/objects.dta etc. into it; we skip
// that heavy flow, but a few load-path code sites deref SystemConfig() (e.g.
// DirLoader's "force_milo_inline" check). Give it an empty array so those reads
// see a valid (empty) tree instead of faulting.
extern DataArray *gSystemConfig;

// ---------------------------------------------------------------------------
// Object factory registration.
//
// The milo loader constructs each object by class name via
// Hmx::Object::NewObject(sym); an unregistered class only WARNs and yields a
// null object (which then desyncs the positional object-data read). So we
// register the object classes we can up front.
//
// SCOPE NOTE: rb3-native currently links only the obj/utl/os/math matched-fork
// subset (rndobj/ and synth/ are not yet native-clean — see native/CMakeLists.txt).
// So we register the obj-level factories here. When a loaded milo contains
// rndobj/synth object classes (Mesh, Tex, Sfx, ...), those WARN as
// "Can't make <Class>" — expected until those forks compile. ObjectDir itself
// (the milo root) and any obj/-level objects DO load, so the root header + dir
// metadata still dump.
// ---------------------------------------------------------------------------
static void RegisterCommonFactories() {
    Hmx::Object::Init();   // REGISTER_OBJ_FACTORY(Object)
    ObjectDir::Register(); // REGISTER_OBJ_FACTORY(ObjectDir)

    // rndobj/synth factories. We register via REGISTER_OBJ_FACTORY directly
    // (name -> `new Class`) rather than the game's RndXXX::Init()/Synth::Init(),
    // which are coupled to the Rnd/Synth singletons + GPU + SystemConfig. Direct
    // registration is enough for the loader to construct the live object graph.
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

// ---------------------------------------------------------------------------
// Scene-tree (names + types) dump.
//
// We read the milo's ObjectDir HEADER directly from the ChunkStream rather than
// running the full DirLoader object-data load. The header carries exactly the
// names + types the dump wants — dirClass/dirName plus each object's
// className/objName — and reading it needs ZERO object factories. This is the
// same byte-for-byte sequence DirLoader::LoadHeader + CreateObjects walk, and
// the engine's own test_dirloader.cpp StreamPositionTracking test reads it the
// same way. It works even though RB3's rndobj/synth object classes are not yet
// native-clean (so the full DirLoader::LoadObjects path would desync once it
// tries to construct an unregistered class).
//
// Milo ObjectDir header (rev > 0xD, the format for these RB3 assets):
//   int     rev
//   Symbol  dirClass
//   string  dirName
//   int     extSize1, extSize2     (reserve hints)
//   int     numEntries
//   numEntries * { Symbol className; string objName }
//   ... then per-object data (NOT read here).
// ---------------------------------------------------------------------------

// Mirror DirLoader::ResolveEndianness: if the byte-swapped rev compares smaller
// than the raw rev, the stream is the opposite endianness — flip it.
static int ReadRevResolveEndian(BinStream &bs) {
    int rev;
    bs >> rev;
    if ((int)EndianSwap((unsigned int)rev) < rev) {
        rev = EndianSwap((unsigned int)rev);
        bs.UseLittleEndian(true);
    }
    return rev;
}

static bool DumpMiloHeader(const char *path, Platform plat) {
    ChunkStream cs(path, ChunkStream::kRead, 0x8000, false, plat, false);
    if (cs.Fail()) {
        fprintf(stderr, "rb3-native: could not open/parse chunk stream for '%s'\n", path);
        return false;
    }
    // Drain any TempEof chunk-boundary markers before the first real read.
    for (EofType t = cs.Eof(); t != NotEof; t = cs.Eof()) {
        if (t == RealEof) {
            fprintf(stderr, "rb3-native: unexpected EOF before header\n");
            return false;
        }
    }

    int rev = ReadRevResolveEndian(cs);
    if (rev < 7) {
        fprintf(stderr, "rb3-native: rev %d too old (need >= 7)\n", rev);
        return false;
    }
    if (rev <= 0xD) {
        fprintf(stderr, "rb3-native: rev %d uses an older header layout; "
                        "only rev > 13 is dumped here.\n", rev);
        return false;
    }

    Symbol dirClass;
    cs >> dirClass;
    char dirName[0x80];
    cs.ReadString(dirName, sizeof(dirName));

    int extSize1 = 0, extSize2 = 0;
    cs >> extSize1 >> extSize2;

    int numEntries = 0;
    cs >> numEntries;

    printf("\n=== scene tree: %s ===\n", path);
    printf("milo rev %d\n", rev);
    printf("root: '%s' [%s]\n", dirName[0] ? dirName : "(unnamed)", dirClass.Str());
    printf("  (%d object%s)\n", numEntries, numEntries == 1 ? "" : "s");

    if (numEntries < 0 || numEntries > 100000) {
        fprintf(stderr, "rb3-native: implausible entry count %d (desync?)\n", numEntries);
        return false;
    }

    for (int i = 0; i < numEntries; i++) {
        Symbol className;
        cs >> className;
        char objName[0x80];
        cs.ReadString(objName, sizeof(objName));
        if (cs.Fail()) {
            fprintf(stderr, "rb3-native: stream failed reading entry %d\n", i);
            return false;
        }
        printf("    %-32s  [%s]\n", objName[0] ? objName : "(unnamed)", className.Str());
    }
    printf("=== end scene tree (%d objects) ===\n", numEntries);
    return true;
}

// ---------------------------------------------------------------------------
// Live object-graph dump. Walks the ObjectDir returned by DirLoader::LoadObjects
// (real instantiation via the registered factories), printing each LIVE object's
// Name() + ClassName(). This is the (b2) milestone: objects are constructed, not
// just read from the header.
// ---------------------------------------------------------------------------
extern void SynthPreInit();

// Minimal synth singleton bring-up for the live load. Synth/Sfx/SynthSample
// ctors deref TheSynth (e.g. Sfx::Sfx -> TheSynth->mMasterFader), and Synth
// ctor + Synth::Init read SystemConfig("synth"). Give gSystemConfig a minimal
// synth array (null synth, no mics) and stand up TheSynth + its master faders.
// This is a stand-in for the real SystemInit config load (critical-path Step 2).
static void BringUpSynthMinimal() {
    if (TheSynth)
        return;
    // RB3_SYSCFG=<abs path to a .dta> loads a real system config (so
    // SystemConfig("objects") has the per-class type-defs property-sync needs).
    // A suitable wrapper nests config/objects.dta under an `objects` key plus a
    // minimal `synth` block. Without it, fall back to a minimal in-memory config
    // (enough to construct objects, but property-sync of typed props will fail).
    const char *syscfg = getenv("RB3_SYSCFG");
    if (syscfg) {
        printf("rb3-native: loading system config '%s'\n", syscfg);
        gSystemConfig = DataReadFile(syscfg, true);
        if (!gSystemConfig)
            fprintf(stderr, "rb3-native: failed to read RB3_SYSCFG '%s'\n", syscfg);
    }
    if (!gSystemConfig)
        gSystemConfig = DataReadString(
            "(synth (mics 0) (use_null_synth 1) (mute 0)) (objects)"
        );
    SynthPreInit();   // TheSynth = new Synth() (null synth; reads synth cfg)
    if (TheSynth)
        TheSynth->Init(); // creates mMasterFader/mSfxFader + registers synth factories
}

static bool DumpLiveTree(const char *miloPath) {
    // TheLoadMgr.GetPlatform() drives the .milo_<plat> extension + endianness.
    TheLoadMgr.mPlatform = kPlatformXBox;

    BringUpSynthMinimal();

    ObjectDir *dir = DirLoader::LoadObjects(FilePath(miloPath), nullptr, nullptr);
    if (!dir) {
        fprintf(stderr, "rb3-native: DirLoader::LoadObjects returned null\n");
        return false;
    }

    int n = 0;
    printf("\n=== live object graph: %s ===\n", miloPath);
    printf("root: '%s' [%s]\n", dir->Name() ? dir->Name() : "(unnamed)",
           dir->ClassName().Str());
    for (ObjDirItr<Hmx::Object> it(dir, true); it; ++it) {
        Hmx::Object *o = it;
        if (o == dir)
            continue;
        printf("    %-32s  [%s]\n", o->Name() ? o->Name() : "(unnamed)",
               o->ClassName().Str());
        n++;
    }
    printf("=== end live object graph (%d objects instantiated) ===\n", n);
    return true;
}

int main(int argc, char **argv) {
    setbuf(stdout, nullptr);

    // ---- Minimal engine bring-up (shared by both modes) ----
    InitMakeString();
    Symbol::Init(); // creates the global StringTable used to intern symbols

    // Empty SystemConfig so SystemConfig()-deref sites on the load path are safe.
    if (!gSystemConfig)
        gSystemConfig = new DataArray(0);

    RegisterCommonFactories();

    // ---- FLOOR: no milo path -> controlled clean exit ----
    if (argc < 2) {
        printf("rb3-native: engine + RB3 matched fork linked and initialized.\n");
        printf("rb3-native: no .milo path given; nothing to load. Exiting cleanly.\n");
        printf("usage: %s <abs-path-to.milo[_xbox]>\n", argv[0]);
        return 0;
    }

    // ---- STRETCH: dump the milo scene tree (names + types) ----
    const char *miloPath = argv[1];

    // These assets are Xbox-format (big-endian PPC). Tell the loader/stream so
    // the ChunkStream byte-swaps correctly on the little-endian host.
    TheLoadMgr.mPlatform = kPlatformXBox;

    printf("rb3-native: loading milo '%s' (platform=xbox)\n", miloPath);

    // RB3_LIVE_LOAD=1 opts into the full DirLoader object-graph load (real object
    // instantiation via the registered factories — the b2 milestone). It needs
    // the boot singletons (TheSynth) + a populated gSystemConfig, which is the
    // headless-DTA-boot work (critical-path Step 2); see BringUpSynthMinimal().
    // The proven, regression-safe DEFAULT is the header-only names+types dump
    // (straight from the ChunkStream, zero factories — works for all 60 milos).
    if (getenv("RB3_LIVE_LOAD")) {
        if (DumpLiveTree(miloPath))
            return 0;
        fprintf(stderr, "rb3-native: live load failed; falling back to header dump\n");
    }

    if (!DumpMiloHeader(miloPath, kPlatformXBox)) {
        fprintf(stderr, "rb3-native: FAILED to dump '%s'\n", miloPath);
        return 1;
    }
    return 0;
}
