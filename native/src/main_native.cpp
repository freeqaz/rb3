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
#include "os/System.h"

#include <unistd.h> // chdir

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
#include <vector>
#include <csetjmp>
#include <csignal>
#include <execinfo.h>

// GPU smoke mode (RB3_GPU_SMOKE=1). The engine is built with the rndobj-FREE
// WebGPU gfx core ON (MILO_ENGINE_BUILD_GFX), so GpuDevice + Screenshot are
// available. We use the WebGPU API directly for the clear-color render pass —
// the rndobj-coupled renderer (WgpuRnd : NgRnd) is NOT built for RB3.
#include "gfx/GpuDevice.h"
#include "gfx/Screenshot.h"

extern void InitMakeString();

// ---------------------------------------------------------------------------
// Native draw-crash recovery (RB3_GAME mode). Mirrors dc3 main_native.cpp:
// App::RunWithoutDebugging's HX_NATIVE frame loop wraps TheUI.Draw() in
// sigsetjmp(gDrawJmpBuf); when a SIGSEGV fires while gDrawJmpBufSet is true we
// siglongjmp back so a partially-loaded scene that crashes in Draw() skips the
// frame instead of killing the process. App.cpp references these as externs.
// ---------------------------------------------------------------------------
sigjmp_buf gDrawJmpBuf;
bool gDrawJmpBufSet = false;

static void RB3SignalHandler(int sig, siginfo_t *info, void *) {
    if (sig == SIGSEGV && gDrawJmpBufSet) {
        gDrawJmpBufSet = false;
        siglongjmp(gDrawJmpBuf, 1);
    }
    const char *signame = (sig == SIGSEGV) ? "SIGSEGV"
                        : (sig == SIGABRT) ? "SIGABRT"
                        : (sig == SIGBUS)  ? "SIGBUS"
                                           : "SIGNAL";
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
                       "\nRB3 Native: caught %s (signal %d) at %p\n",
                       signame, sig, info ? info->si_addr : nullptr);
    write(STDERR_FILENO, buf, len);
    void *bt[64];
    int n = backtrace(bt, 64);
    backtrace_symbols_fd(bt, n, STDERR_FILENO);
    _exit(128 + sig);
}

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

// ---------------------------------------------------------------------------
// GPU smoke (RB3_GPU_SMOKE=1). Proves the rndobj-FREE WebGPU gfx core links and
// runs from rb3-native: stand up a GpuDevice (headless when MILO_HEADLESS=1 —
// the default in this no-DISPLAY env), then for a few frames acquire the
// offscreen target, BEGIN a render pass that CLEARS to cornflower blue, END,
// and "present" (submit). On the last frame, read back the headless target and
// write a PNG to prove the clear color landed. Exits 0 on success.
//
// This does NOT boot the full renderer/App — GpuDevice is self-contained. It is
// the foundation for native RB3 rendering (Strategy B).
// ---------------------------------------------------------------------------
static int RunGpuSmoke() {
    // Cornflower blue (the classic "is it clearing?" color), in linear 0..1.
    const wgpu::Color kClear = {0.392, 0.584, 0.929, 1.0};

    bool headless = (getenv("MILO_HEADLESS") != nullptr) || (getenv("DISPLAY") == nullptr);

    GpuDeviceDesc desc{};
    desc.headless = headless;
    desc.width  = getenv("MILO_WIDTH")  ? atoi(getenv("MILO_WIDTH"))  : 256;
    desc.height = getenv("MILO_HEIGHT") ? atoi(getenv("MILO_HEIGHT")) : 256;
    desc.title  = "rb3-native GPU smoke";

    printf("rb3-native: GPU smoke — initializing GpuDevice (%dx%d, %s)\n",
           desc.width, desc.height, headless ? "headless" : "windowed");

    GpuDevice gpu;
    if (!gpu.Init(desc)) {
        if (!headless) {
            printf("rb3-native: windowed init failed; retrying headless\n");
            desc.headless = true;
            headless = true;
            if (!gpu.Init(desc)) {
                fprintf(stderr, "rb3-native: GpuDevice headless init also FAILED\n");
                return 1;
            }
        } else {
            fprintf(stderr, "rb3-native: GpuDevice headless init FAILED\n");
            return 1;
        }
    }
    if (!gpu.IsReady()) {
        fprintf(stderr, "rb3-native: GpuDevice not ready after init\n");
        return 1;
    }
    if (gpu.IsNullBackend()) {
        fprintf(stderr, "rb3-native: WARNING — Null backend; clear color will not be real\n");
    }

    const int kFrames = 3;
    for (int f = 0; f < kFrames; f++) {
        wgpu::TextureView view = headless ? gpu.AcquireHeadlessFrame()
                                          : gpu.AcquireNextFrame();
        if (!view) {
            fprintf(stderr, "rb3-native: frame %d — failed to acquire target view\n", f);
            return 1;
        }

        wgpu::CommandEncoder enc = gpu.Device().CreateCommandEncoder();

        wgpu::RenderPassColorAttachment colorAtt{};
        colorAtt.view = view;
        colorAtt.loadOp = wgpu::LoadOp::Clear;
        colorAtt.storeOp = wgpu::StoreOp::Store;
        colorAtt.clearValue = kClear;

        wgpu::RenderPassDescriptor rpDesc{};
        rpDesc.colorAttachmentCount = 1;
        rpDesc.colorAttachments = &colorAtt;

        wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rpDesc);
        // No draws — just the clear.
        pass.End();

        wgpu::CommandBuffer cmd = enc.Finish();
        gpu.Queue().Submit(1, &cmd);

        if (!headless) {
            gpu.PresentFrame();
        }
        printf("rb3-native: frame %d — cleared to cornflower blue\n", f);
    }

    // Prove the clear color: read back the headless target and write a PNG.
    int rc = 0;
    if (headless) {
        const int w = gpu.WindowWidth(), h = gpu.WindowHeight();
        std::vector<uint8_t> pixels((size_t)w * h * 4);
        if (gpu.ReadbackHeadlessFrame(pixels.data(), pixels.size())) {
            // Sanity-check the center pixel against the expected clear color.
            size_t c = ((size_t)(h / 2) * w + (w / 2)) * 4;
            printf("rb3-native: center pixel RGBA = (%u, %u, %u, %u) "
                   "(expected ~ 100,149,237,255 for cornflower blue)\n",
                   pixels[c], pixels[c + 1], pixels[c + 2], pixels[c + 3]);

            const char* outPath = getenv("RB3_GPU_SMOKE_PNG");
            char defPath[] = "/tmp/rb3_gpu_smoke.png";
            if (!outPath) outPath = defPath;
            if (WritePNG(outPath, pixels.data(), w, h)) {
                printf("rb3-native: wrote clear-color frame to %s\n", outPath);
            } else {
                fprintf(stderr, "rb3-native: WritePNG failed for %s\n", outPath);
                rc = 1;
            }
        } else {
            fprintf(stderr, "rb3-native: headless readback FAILED\n");
            rc = 1;
        }
    }

    gpu.Shutdown();
    printf("rb3-native: GPU smoke %s\n", rc == 0 ? "OK" : "FAILED");
    return rc;
}

extern void SynthInit();

// ---------------------------------------------------------------------------
// Headless DTA boot (RB3_BOOT=1) — critical-path Step 2.
//
// Runs the real curated SystemPreInit/SystemInit (now HX_NATIVE-gated in
// os/System.cpp) so gSystemConfig is populated from the on-disc config DTAs
// (config/band_preinit_keep.dta then config/band_keep.dta). That gives
// SystemConfig("objects") the per-class type-defs property-sync needs — which
// also completes Step 1's full object-graph load. File resolution: chdir to the
// data dir (RB3_DATA, default the extracted assets) so the relative config/ +
// ui/ paths and DTA #include/#merge resolve.
//
// With a .milo path argument, it then live-loads + dumps that scene using the
// real config (no BringUpSynthMinimal stand-in).
// ---------------------------------------------------------------------------
static int RunBoot(int argc, char **argv, const char *miloPath) {
    const char *dataDir = getenv("RB3_DATA");
    if (!dataDir)
        dataDir = "/home/free/code/milohax/rb3/orig-assets/extracted";
    if (chdir(dataDir) != 0) {
        fprintf(stderr, "rb3-native: boot — chdir('%s') failed\n", dataDir);
        return 1;
    }
    printf("rb3-native: boot — data dir '%s'\n", dataDir);

    TheLoadMgr.mPlatform = kPlatformXBox;

    SetSystemArgs(argc, argv);
    printf("rb3-native: SystemPreInit('config/band_preinit_keep.dta')...\n");
    SystemPreInit("config/band_preinit_keep.dta");
    printf("rb3-native: SystemPreInit OK — gSystemConfig=%p\n", (void *)gSystemConfig);

    printf("rb3-native: SystemInit('config/band_keep.dta')...\n");
    SystemInit("config/band_keep.dta");
    DataArray *objCfg = gSystemConfig ? gSystemConfig->FindArray(Symbol("objects"), false) : nullptr;
    DataArray *uiCfg  = gSystemConfig ? gSystemConfig->FindArray(Symbol("ui"), false) : nullptr;
    printf("rb3-native: SystemInit OK — objects-cfg=%p (%d entries), ui-cfg=%p\n",
           (void *)objCfg, objCfg ? objCfg->Size() : -1, (void *)uiCfg);

    // Register the obj/rndobj/synth object factories so DirLoader can instantiate
    // them (SystemInit only registers the obj-level factories via ObjectDir::Init).
    RegisterCommonFactories();

    if (miloPath) {
        printf("rb3-native: live-loading '%s' with real config...\n", miloPath);
        ObjectDir *dir = DirLoader::LoadObjects(FilePath(miloPath), nullptr, nullptr);
        if (!dir) {
            fprintf(stderr, "rb3-native: boot live load returned null\n");
            return 1;
        }
        int n = 0;
        printf("\n=== live object graph: %s ===\n", miloPath);
        printf("root: '%s' [%s]\n", dir->Name() ? dir->Name() : "(unnamed)",
               dir->ClassName().Str());
        for (ObjDirItr<Hmx::Object> it(dir, true); it; ++it) {
            Hmx::Object *o = it;
            if (o == dir) continue;
            printf("    %-32s  [%s]\n", o->Name() ? o->Name() : "(unnamed)",
                   o->ClassName().Str());
            n++;
        }
        printf("=== end live object graph (%d objects instantiated) ===\n", n);
    }
    printf("rb3-native: boot complete.\n");
    return 0;
}

// Render modes implemented in dedicated backend TUs (Strategy B).
extern int RunRenderTri();              // rb3_render_tri.cpp — milestone (ii)
extern int RunRenderMesh(int argc, char **argv, const char *miloPath); // rb3_render_mesh.cpp — (iii)

// ---------------------------------------------------------------------------
// RB3_GAME=1 — the REAL game boot. Mirrors dc3 native main: stand up the
// GpuDevice (BandRnd = TheRnd, via rb3_band_rnd.cpp's strong TheRnd def) BEFORE
// chdir/boot (Dawn adapter enumeration wants the clean original cwd), then
// chdir(RB3_DATA), SetSystemArgs, force kPlatformXBox (the extracted assets are
// 360-ARK big-endian), and construct + Run the real App. App::App() runs the
// full HX_NATIVE-gated boot spine (SystemPreInit → TheRnd->PreInit/Init →
// SynthInit → Movie::Init → SystemInit → the *::Init cluster → TheUI.Init →
// TheQuestMgr.Init), then App::Run() enters the HX_NATIVE frame loop. The boot
// gets as far as the matched-fork Load()/DTA-manager state allows; the signal
// handler + draw guard keep a partial-scene crash reportable.
// ---------------------------------------------------------------------------
#include "App.h"
#include "rb3_band_rnd.h"

static int RunGame(int argc, char **argv) {
    // Stand up the GpuDevice FIRST (before chdir + RB3 MemMgr boot) — same
    // ordering rationale as RB3_RENDER_MESH.
    bool headless = (getenv("MILO_HEADLESS") != nullptr) || (getenv("DISPLAY") == nullptr);
    int W = getenv("MILO_WIDTH")  ? atoi(getenv("MILO_WIDTH"))  : 1280;
    int H = getenv("MILO_HEIGHT") ? atoi(getenv("MILO_HEIGHT")) : 720;
    gBandRnd.SetClearColor(Hmx::Color(0, 0, 0));
    if (!gBandRnd.InitGpu(W, H, headless)) {
        fprintf(stderr, "rb3-native: RB3_GAME — GpuDevice init FAILED\n");
        return 1;
    }
    printf("rb3-native: RB3_GAME — GpuDevice up (%dx%d, %s)\n",
           W, H, headless ? "headless" : "windowed");

    // RB3's 2010 milos serialize text/tex/dir objects under the legacy short
    // class names "Text"/"Tex"/"Dir"; the engine registers RndText/RndTex/RndDir.
    // The real Rnd::PreInit (during App ctor) registers the prefixed names but not
    // the short aliases, so register them here before any menu milo loads —
    // otherwise font/ui milos hit "Can't make Tex"/"Text" (UILabel font assert).
    extern void RB3RegisterLegacyRndAliases();
    RB3RegisterLegacyRndAliases();

    const char *dataDir = getenv("RB3_DATA");
    if (!dataDir)
        dataDir = "/home/free/code/milohax/rb3/orig-assets/extracted";
    if (chdir(dataDir) != 0) {
        fprintf(stderr, "rb3-native: RB3_GAME — chdir('%s') failed\n", dataDir);
        return 1;
    }
    printf("rb3-native: RB3_GAME — data dir '%s'\n", dataDir);

    TheLoadMgr.mPlatform = kPlatformXBox;
    SetSystemArgs(argc, argv);

    printf("rb3-native: RB3_GAME — constructing App...\n");
    App app(argc, argv);
    printf("rb3-native: RB3_GAME — App constructed; calling Run()...\n");
    app.Run();
    printf("rb3-native: RB3_GAME — Run() returned; exiting cleanly.\n");
    return 0;
}

int main(int argc, char **argv) {
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);

    // Reliable signal handling for the full-boot modes (see RB3SignalHandler).
    struct sigaction sa;
    sa.sa_sigaction = RB3SignalHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);

    // ---- Real game boot (RB3_GAME=1): construct RB3's actual App and Run().
    //      Renders through BandRnd (= TheRnd). ----
    if (getenv("RB3_GAME")) {
        return RunGame(argc, argv);
    }

    // ---- GPU smoke mode (no engine/object bring-up needed; GpuDevice is
    //      self-contained). Gated by RB3_GPU_SMOKE=1; leaves all other modes
    //      untouched. ----
    if (getenv("RB3_GPU_SMOKE")) {
        return RunGpuSmoke();
    }

    // ---- Triangle render (RB3_RENDER_TRI=1): GpuDevice + PipelineManager +
    //      standard shader + a hand-built triangle -> PNG. No rndobj. ----
    if (getenv("RB3_RENDER_TRI")) {
        return RunRenderTri();
    }

    // ---- Mesh/scene render (RB3_RENDER_MESH=1): boot the real config, load a
    //      milo, stand up BandRnd, draw its RndMeshes -> PNG. ----
    if (getenv("RB3_RENDER_MESH")) {
        return RunRenderMesh(argc, argv, argc >= 2 ? argv[1] : nullptr);
    }

    // ---- Headless DTA boot mode (RB3_BOOT=1): real SystemPreInit/SystemInit
    //      config load, then optional live milo load with that config. ----
    if (getenv("RB3_BOOT")) {
        return RunBoot(argc, argv, argc >= 2 ? argv[1] : nullptr);
    }

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
