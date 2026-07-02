// rb3-viewer — standalone .milo asset renderer (native, headless).
//
// A CLI wrapper around the proto-viewer render spine in rb3_render_mesh.cpp:
// boots the real curated config, stands up BandRnd (WGPU, headless), registers
// the rndobj + game + CHAR object factories, loads one .milo (plus optional
// dependency milos as subdirs), optionally settles CharHair physics with a
// manually driven TheTaskMgr clock, frames a camera over the scene bounds, and
// writes a PNG.
//
// Purpose: debug rendering bugs on individual assets (first customer: the
// "white wig / long hair" CharHair artifact) WITHOUT booting the whole game.
//
// Triggered by `--viewer` in argv or RB3_VIEWER=1 (see main_native.cpp). This
// mode reuses the LoadMiloAndWalk / RenderFrame / RenderToPng exports from
// rb3_render_mesh.h and adds: char-class factories, --subdir dep loading,
// --sim hair settling, --hide filtering, richer camera CLI, and --list census.
//
// CLI:
//   rb3-native --viewer <milo-rel-path>
//       [--out out.png] [--frames N] [--sim N]
//       [--subdir <milo>]... [--hide substr]... [--only-showing]
//       [--azimuth d --elevation d --distance u | --cam-dir x,y,z]
//       [--width W --height H] [--list] [--verbose]
//
// The milo path is relative to RB3_DATA (default
// /home/free/code/milohax/rb3/orig-assets/extracted) or absolute.

#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/DataFile.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "utl/Loader.h"
#include "utl/FilePath.h"
#include "utl/Symbol.h"
#include "os/System.h"

#include "rndobj/Dir.h"
#include "rndobj/Cam.h"
#include "rndobj/Mesh.h"
#include "math/Vec.h"

// --- char / bandobj factories the char & hair milos instantiate ---
#include "rndobj/Mesh.h"
#include "rndobj/MeshAnim.h"
#include "rndobj/MeshDeform.h"
#include "rndobj/Trans.h"
#include "rndobj/Tex.h"
#include "rndobj/TexBlender.h"
#include "rndobj/TexBlendController.h"
#include "rndobj/Mat.h"
#include "rndobj/Group.h"
#include "rndobj/MatAnim.h"
#include "rndobj/TransAnim.h"
#include "rndobj/PropAnim.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/AmbientOcclusion.h"
#include "char/CharClip.h"
#include "char/CharClipSet.h"
#include "char/CharCollide.h"
#include "char/CharLipSync.h"
#include "char/CharInterest.h"
#include "char/CharFaceServo.h"
#include "char/CharWeightSetter.h"
#include "char/CharHair.h"
#include "char/CharServoBone.h"
#include "bandobj/BandFaceDeform.h"
#include "bandobj/BandCharDesc.h"
#include "bandobj/OutfitConfig.h"

#include "math/Color.h"
#include "gfx/GpuDevice.h"
#include "gfx/Screenshot.h"
#include "platform/Rnd_Wgpu_RB3.h"

#include "rb3_render_mesh.h"

#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern DataArray *gSystemConfig;

// Defined in rb3_render_mesh.cpp / rb3_game_object_factories.cpp.
extern void RB3RegisterGameObjectFactories();

namespace {

struct ViewerArgs {
    const char* miloPath = nullptr;
    const char* outPath = nullptr;              // default /tmp/rb3_viewer.png
    int frames = 1;                             // GPU warmup frames before readback
    int sim = 0;                                // CharHair settle steps (30fps)
    std::vector<std::string> subdirs;           // dependency milos (loaded first)
    std::vector<std::string> hides;             // hide meshes whose name contains substr
    bool onlyShowing = false;
    bool list = false;
    bool verbose = false;
    // camera
    bool hasAzEl = false;
    float azimuth = 20.0f, elevation = 15.0f;   // degrees
    float distance = -1.0f;                     // <=0 = auto
    bool hasCamDir = false;
    float camDir[3] = {0, 0, 0};
    int width = 640, height = 480;
};

void Usage() {
    fprintf(stderr,
        "usage: rb3-native --viewer <milo-rel-path> [options]\n"
        "  --out <path.png>       output PNG (default /tmp/rb3_viewer.png)\n"
        "  --frames N             GPU warmup frames before readback (default 1)\n"
        "  --sim N                CharHair settle steps at 30fps before render\n"
        "  --subdir <milo>        pre-load a dependency milo (repeatable)\n"
        "  --hide <substr>        skip meshes whose name contains substr (repeatable)\n"
        "  --only-showing         draw only meshes with Showing()==true\n"
        "  --azimuth d            camera azimuth in degrees (with --elevation)\n"
        "  --elevation d          camera elevation in degrees\n"
        "  --distance u           camera stand-off distance (default auto-frame)\n"
        "  --cam-dir x,y,z        explicit Milo-axis view direction (overrides az/el)\n"
        "  --width W --height H   render size (default 640x480)\n"
        "  --list                 print class/name census and exit (no GPU)\n"
        "  --verbose              extra per-mesh logging\n");
}

// Parse argv. Returns false on a parse error (usage already printed).
bool ParseArgs(int argc, char** argv, ViewerArgs& a) {
    for (int i = 1; i < argc; i++) {
        const char* s = argv[i];
        if (!strcmp(s, "--viewer")) continue;   // the mode selector itself
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { fprintf(stderr, "rb3-viewer: %s needs a value\n", name); return nullptr; }
            return argv[++i];
        };
        if (!strcmp(s, "--out")) { const char* v = next(s); if (!v) return false; a.outPath = v; }
        else if (!strcmp(s, "--frames")) { const char* v = next(s); if (!v) return false; a.frames = atoi(v); }
        else if (!strcmp(s, "--sim")) { const char* v = next(s); if (!v) return false; a.sim = atoi(v); }
        else if (!strcmp(s, "--subdir")) { const char* v = next(s); if (!v) return false; a.subdirs.push_back(v); }
        else if (!strcmp(s, "--hide")) { const char* v = next(s); if (!v) return false; a.hides.push_back(v); }
        else if (!strcmp(s, "--only-showing")) { a.onlyShowing = true; }
        else if (!strcmp(s, "--azimuth")) { const char* v = next(s); if (!v) return false; a.azimuth = (float)atof(v); a.hasAzEl = true; }
        else if (!strcmp(s, "--elevation")) { const char* v = next(s); if (!v) return false; a.elevation = (float)atof(v); a.hasAzEl = true; }
        else if (!strcmp(s, "--distance")) { const char* v = next(s); if (!v) return false; a.distance = (float)atof(v); }
        else if (!strcmp(s, "--cam-dir")) { const char* v = next(s); if (!v) return false;
            if (sscanf(v, "%f,%f,%f", &a.camDir[0], &a.camDir[1], &a.camDir[2]) == 3) a.hasCamDir = true; }
        else if (!strcmp(s, "--width")) { const char* v = next(s); if (!v) return false; a.width = atoi(v); }
        else if (!strcmp(s, "--height")) { const char* v = next(s); if (!v) return false; a.height = atoi(v); }
        else if (!strcmp(s, "--list")) { a.list = true; }
        else if (!strcmp(s, "--verbose")) { a.verbose = true; }
        else if (!strcmp(s, "--help") || !strcmp(s, "-h")) { Usage(); return false; }
        else if (s[0] == '-') { fprintf(stderr, "rb3-viewer: unknown option '%s'\n", s); Usage(); return false; }
        else if (!a.miloPath) { a.miloPath = s; }
        else { fprintf(stderr, "rb3-viewer: unexpected extra arg '%s'\n", s); return false; }
    }
    if (!a.miloPath) { fprintf(stderr, "rb3-viewer: missing <milo-rel-path>\n"); Usage(); return false; }
    return true;
}

// Register the char/bandobj factory set a char/hair milo needs. Mirrors
// native/tests/test_charload5b.cpp:RegisterCharLoadFactories plus the three
// viewer-only Inits (CharHair/OutfitConfig/RndAmbientOcclusion) called out in
// scout-rb3-infra.md §3.4. Deliberately does NOT register Character (its ctor
// news CharacterTest -> char_test overlay MILO_FAIL) and does NOT call
// CharInit()/BandInit() wholesale (they carry overlay/preload traps). Idempotent.
void RegisterViewerCharFactories() {
    // rndobj leaves/containers referenced by char & hair milos
    RndMeshAnim::Init();
    RndMeshDeform::Init();
    RndTexBlender::Init();
    RndTexBlendController::Init();
    RndMatAnim::Init();
    RndTransAnim::Init();
    RndPropAnim::Init();
    EventTrigger::Init();
    RndAmbientOcclusion::Init();   // in-game done by Rnd::PreInit; not by the synth harness

    // char objects (the head/skeleton/clip + hair set)
    CharClipSet::Init();
    CharClip::Init();
    CharCollide::Init();
    CharLipSync::Init();
    CharInterest::Init();
    CharFaceServo::Init();
    CharWeightSetter::Init();
    CharServoBone::Init();
    CharHair::Init();
    BandFaceDeform::Init();

    // bandobj outfit tint config — Init() also news the static sMat/sCam/
    // sBandCharDesc used by MatSwap::Compose (call Init, not a bare Register).
    // OutfitConfig::Init does Hmx::Object::New<BandCharDesc>(), so BandCharDesc's
    // factory must be registered first — but only the lightweight Register() (the
    // full BandCharDesc::Init() also ReloadPrefabs() + loads deform.milo, which a
    // static asset render doesn't need). Without this, New<BandCharDesc>() hits
    // "Unknown class BandCharDesc" -> MILO_FAIL -> null sBandCharDesc -> crash.
    BandCharDesc::Register();
    OutfitConfig::Init();
}

// Resolve a possibly-relative milo path to an absolute one anchored at the data
// dir (mirrors RunRenderMesh). Uses the current cwd first if it exists there.
void ResolveMiloPath(const char* in, const char* dataDir, char* out, size_t outSz) {
    if (in[0] == '/') { snprintf(out, outSz, "%s", in); return; }
    if (access(in, F_OK) == 0) {
        char cwd[2048]; getcwd(cwd, sizeof(cwd));
        snprintf(out, outSz, "%s/%s", cwd, in);
    } else {
        snprintf(out, outSz, "%s/%s", dataDir, in);
    }
}

// Print a class/name census of the loaded dir (recursive, includes subdirs).
void PrintCensus(ObjectDir* dir) {
    if (!dir) return;
    printf("=== census: dir '%s' [%s] ===\n",
           dir->Name() ? dir->Name() : "(unnamed)", dir->ClassName().Str());
    int n = 0;
    for (ObjDirItr<Hmx::Object> it(dir, true); it; ++it) {
        Hmx::Object* o = it;
        printf("  [%s] %s\n", o->ClassName().Str(), o->Name() ? o->Name() : "(unnamed)");
        n++;
    }
    printf("=== end census (%d objects) ===\n", n);
}

// Draw one frame walking every drawable RndMesh, applying --hide / --only-showing.
void ViewerDrawFrame(ObjectDir* dir, RndCam* cam, const ViewerArgs& a) {
    if (cam) { RndCam::sCurrent = cam; cam->Select(); }
    gBandRnd.BeginFrame(cam);
    for (ObjDirItr<Hmx::Object> it(dir, true); it; ++it) {
        RndMesh* mesh = dynamic_cast<RndMesh*>((Hmx::Object*)it);
        if (!mesh) continue;
        RndMesh* owner = mesh->GeomOwner(); if (!owner) owner = mesh;
        bool hasGeom = owner->mFaces.size() > 0 &&
                       (owner->mVerts.size() > 0 || owner->mNumCompressedVerts > 0);
        if (!hasGeom) continue;
        if (a.onlyShowing && !mesh->Showing()) continue;
        const char* nm = mesh->Name();
        bool hidden = false;
        if (nm) for (const std::string& h : a.hides) if (strstr(nm, h.c_str())) { hidden = true; break; }
        if (hidden) continue;
        if (mesh->Showing()) mesh->DrawShowing();
        else                 gBandRnd.DrawMesh(mesh);
    }
    gBandRnd.EndFrame();
}

// Drive N CharHair settle steps at 30fps with a manually advanced TaskMgr clock.
// This is what makes hair physics run without a Character (see scout §3.4 /
// scout-dc3-viewer §5): Poll() each CharHair after SetSecondsAndBeat.
void SimulateHair(ObjectDir* dir, int steps, bool verbose) {
    const float dt = 1.0f / 30.0f;
    const float bpm = 120.0f;               // arbitrary; hair sim uses seconds
    int hairCount = 0;
    for (ObjDirItr<CharHair> ci(dir, true); ci; ++ci) hairCount++;
    printf("rb3-viewer: --sim %d steps over %d CharHair object(s)\n", steps, hairCount);
    for (int i = 0; i < steps; i++) {
        float t = (float)(i + 1) * dt;
        float beat = t * (bpm / 60.0f);
        TheTaskMgr.SetSecondsAndBeat(t, beat, false);
        int polled = 0;
        for (ObjDirItr<CharHair> ci(dir, true); ci; ++ci) { ci->Poll(); polled++; }
        if (verbose && (i == 0 || i == steps - 1))
            printf("  sim step %d/%d t=%.3f polled %d hair\n", i + 1, steps, t, polled);
    }
}

}  // namespace

// Entry point for the --viewer / RB3_VIEWER mode. Called from main_native.cpp.
int RunViewer(int argc, char** argv) {
    ViewerArgs a;
    if (!ParseArgs(argc, argv, a)) return 2;

    const char* dataDir = getenv("RB3_DATA");
    if (!dataDir) dataDir = "/home/free/code/milohax/rb3/orig-assets/extracted";

    char absMilo[4096];
    ResolveMiloPath(a.miloPath, dataDir, absMilo, sizeof(absMilo));

    if (a.verbose) setenv("RB3_MESH_VERBOSE", "1", 1);

    // --- GPU FIRST (before chdir), unless --list (census needs no GPU). ---
    // Dawn/Vulkan adapter enumeration wants the clean original cwd.
    if (!a.list) {
        gBandRnd.SetClearColor(Hmx::Color(0.12f, 0.14f, 0.18f));
        bool headless = (getenv("MILO_HEADLESS") != nullptr) || (getenv("DISPLAY") == nullptr);
        if (!getenv("MILO_HEADLESS")) headless = true;   // viewer is always headless
        if (!gBandRnd.InitGpu(a.width, a.height, headless)) {
            fprintf(stderr, "rb3-viewer: GPU init FAILED (sandboxed? re-run with "
                            "dangerouslyDisableSandbox)\n");
            return 2;
        }
    }

    if (chdir(dataDir) != 0) {
        fprintf(stderr, "rb3-viewer: chdir('%s') failed\n", dataDir);
        return 1;
    }
    printf("rb3-viewer: data dir '%s'\n", dataDir);

    TheLoadMgr.mPlatform = kPlatformXBox;
    SetSystemArgs(argc, argv);

    printf("rb3-viewer: SystemPreInit/SystemInit...\n");
    SystemPreInit("config/band_preinit_keep.dta");
    SystemInit("config/band_keep.dta");

    // Factory bring-up: rndobj base + legacy aliases (PreInitRender), game/bandobj
    // Dir + leaf factories, then the char/hair set. PreInitRender only REGISTERS
    // rndobj factories (+ Tex/Text/Dir aliases) reading gSystemConfig — it does NOT
    // touch the GPU — so it is safe (and required) even on the --list no-GPU path:
    // skipping it leaves classes unregistered -> Unknown-class stream desync ->
    // heap corruption on load (trap #4). Only InitGpu is skipped for --list.
    gBandRnd.PreInitRender();
    RB3RegisterGameObjectFactories();
    RegisterViewerCharFactories();

    // --- Load dependency milos first (so cross-milo tex/mesh refs resolve). ---
    // Each is loaded into its own dir then AppendSubDir'd into the subject dir
    // after the subject loads (below).
    std::vector<ObjectDir*> depDirs;
    for (const std::string& sd : a.subdirs) {
        char absDep[4096];
        ResolveMiloPath(sd.c_str(), dataDir, absDep, sizeof(absDep));
        printf("rb3-viewer: loading subdir '%s'\n", absDep);
        ObjectDir* d = DirLoader::LoadObjects(FilePath(absDep), nullptr, nullptr);
        if (d) depDirs.push_back(d);
        else fprintf(stderr, "rb3-viewer: subdir load FAILED: %s\n", absDep);
    }

    // --- Load the subject milo. ---
    printf("rb3-viewer: loading '%s'\n", absMilo);
    ObjectDir* dir = DirLoader::LoadObjects(FilePath(absMilo), nullptr, nullptr);
    if (!dir) {
        fprintf(stderr, "rb3-viewer: DirLoader::LoadObjects returned null\n");
        return 1;
    }

    // Attach dependency dirs as subdirs so name lookups (hair_shared_spec.tex,
    // bone_hair.mesh, ...) resolve, then re-sync references.
    for (ObjectDir* d : depDirs) {
        ObjDirPtr<ObjectDir> ptr(d);
        dir->AppendSubDir(ptr);
    }
    if (RndDir* rd = dynamic_cast<RndDir*>(dir)) rd->SyncObjects();
    else dir->SyncObjects();

    if (a.list) {
        PrintCensus(dir);
        fflush(stdout);
        _exit(0);
    }

    // --- Bounds + camera. ---
    SceneBounds b = ViewerComputeBounds(dir);
    printf("rb3-viewer: dir '%s' [%s] — %d objects, %d drawable meshes, %d cams\n",
           dir->Name() ? dir->Name() : "(unnamed)", dir->ClassName().Str(),
           b.totalObjects, b.meshCount, b.camCount);
    if (b.meshCount == 0) {
        fprintf(stderr, "rb3-viewer: no drawable meshes; nothing to render\n");
        return 1;
    }
    if (!b.valid) {
        fprintf(stderr, "rb3-viewer: could not compute scene bounds\n");
        return 1;
    }

    // Compute the view direction. --cam-dir wins; else az/el spherical (Milo
    // axes: X=right, Y=forward/depth, Z=up). Default = the RENDER_MESH diagonal.
    float dir3[3];
    const float* dirPtr = nullptr;
    if (a.hasCamDir) { dir3[0] = a.camDir[0]; dir3[1] = a.camDir[1]; dir3[2] = a.camDir[2]; dirPtr = dir3; }
    else if (a.hasAzEl) {
        float az = a.azimuth * (float)M_PI / 180.0f;
        float el = a.elevation * (float)M_PI / 180.0f;
        // View direction FROM eye TO center. Eye sits at az/el on a sphere; the
        // look direction is the negation. Milo: y is depth (forward), z is up.
        dir3[0] = -sinf(az) * cosf(el);
        dir3[1] = -cosf(az) * cosf(el);
        dir3[2] = -sinf(el);
        dirPtr = dir3;
    }
    RndCam* cam = ViewerMakeCamera(b, dirPtr, a.distance);
    if (cam) { RndCam::sCurrent = cam; cam->Select(); }

    // --- Settle hair (optional), then warmup frames, then final readback. ---
    if (a.sim > 0) SimulateHair(dir, a.sim, a.verbose);

    int warmup = a.frames > 1 ? a.frames : 1;
    for (int f = 0; f < warmup - 1; f++) ViewerDrawFrame(dir, cam, a);

    // Final frame + PNG. Set the output path through the env var RenderToPng
    // reads, then reuse the shared readback+PNG+_exit path via a WalkResult.
    const char* outPath = a.outPath ? a.outPath : "/tmp/rb3_viewer.png";
    setenv("RB3_RENDER_MESH_PNG", outPath, 1);

    // Draw the final frame ourselves (applies --hide), then hand a WalkResult to
    // RenderToPng which re-draws once and does the readback/PNG/_exit. To keep
    // --hide applied on that final draw too, we render then read back directly.
    ViewerDrawFrame(dir, cam, a);

    GpuDevice& gpu = gBandRnd.Gpu();
    int gw = gpu.WindowWidth(), gh = gpu.WindowHeight();
    std::vector<uint8_t> pixels((size_t)gw * gh * 4);
    if (!gpu.ReadbackHeadlessFrame(pixels.data(), pixels.size())) {
        fprintf(stderr, "rb3-viewer: readback FAILED\n");
        return 1;
    }
    int clearR = (int)(0.12f * 255), clearG = (int)(0.14f * 255), clearB = (int)(0.18f * 255);
    int nonClear = 0;
    for (size_t i = 0; i < (size_t)gw * gh; i++) {
        const uint8_t* p = &pixels[i * 4];
        if (abs((int)p[0]-clearR) > 14 || abs((int)p[1]-clearG) > 14 || abs((int)p[2]-clearB) > 14)
            nonClear++;
    }
    printf("rb3-viewer: non-clear pixels=%d / %d (%.1f%%)\n",
           nonClear, gw * gh, 100.0 * nonClear / (gw * gh));

    int rc = 0;
    if (WritePNG(outPath, pixels.data(), gw, gh))
        printf("rb3-viewer: wrote %s\n", outPath);
    else { fprintf(stderr, "rb3-viewer: WritePNG FAILED\n"); rc = 1; }

    printf("rb3-viewer: %s\n", rc == 0 ? "OK" : "FAILED");
    fflush(stdout); fflush(stderr);
    _exit(rc);   // dodge the ObjectDir-vs-Dawn static-dtor teardown race
    return rc;
}
