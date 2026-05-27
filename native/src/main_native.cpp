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
#include "utl/Loader.h"
#include "utl/FilePath.h"
#include "utl/Symbol.h"
#include "utl/ChunkStream.h"
#include "utl/BinStream.h"
#include "os/Endian.h"

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

    printf("rb3-native: dumping milo '%s' (platform=xbox)\n", miloPath);

    if (!DumpMiloHeader(miloPath, kPlatformXBox)) {
        fprintf(stderr, "rb3-native: FAILED to dump '%s'\n", miloPath);
        return 1;
    }
    return 0;
}
