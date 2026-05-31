// RB3-Wii native GPU rendering — milestone (iii/iv): render a real loaded .milo.
//
// Boots the real config (SystemPreInit/SystemInit), stands up BandRnd
// (registers the rndobj factories + the GpuDevice), loads a milo via
// DirLoader::LoadObjects, finds (or synthesizes) a RndCam, then walks the dir
// drawing every RndMesh via RndMesh::DrawShowing -> BandRnd::DrawMesh, and
// writes a PNG of the result.
//
// Gated by RB3_RENDER_MESH=1 in main_native.cpp.

#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/DataFile.h"
#include "obj/Object.h"
#include "obj/ObjMacros.h"
#include "utl/Loader.h"
#include "utl/FilePath.h"
#include "utl/Symbol.h"
#include "os/System.h"

#include "rndobj/Dir.h"
#include "rndobj/Cam.h"
#include "rndobj/Mesh.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "rndobj/Trans.h"
#include "math/Mtx.h"
#include "math/Vec.h"

#include "gfx/GpuDevice.h"
#include "gfx/Screenshot.h"
#include "platform/Rnd_Wgpu_RB3.h"

#include "rb3_render_mesh.h"

#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern DataArray *gSystemConfig;

#include <algorithm>

// ---------------------------------------------------------------------------
// InvalidateGpuMesh — rb3 (BandRnd) GPU-backend variant.
//
// RndMesh::SetGeomOwner() calls ::InvalidateGpuMesh(this) under HX_NATIVE (added
// in a9f9973a for the W7-HUD BandScoreboard digit-slot fix). That symbol is
// defined by the engine's DC3-flavor renderer in platform/MeshGpuCache.cpp,
// which keys GPU uploads by mesh pointer and therefore must be told when a
// mesh's GeomOwner is swapped. rb3-native and rb3-web use the rb3 BandRnd
// backend (platform/Rnd_Wgpu_RB3.cpp), which compiles MeshGpuCache.cpp out and
// has NO pointer-keyed cache — rb3_render_mesh.cpp re-reads mesh->GeomOwner()
// fresh on every draw (see lines ~106/~395), so a GeomOwner swap is already
// picked up next frame and there is nothing to invalidate. This is therefore a
// correct no-op, not just a stub. Providing it resolves the undefined reference
// that otherwise breaks the rb3-native link; on web the missing function was
// silently auto-stubbed, so this also replaces that abort-stub with a real def.
void InvalidateGpuMesh(RndMesh *) {}

// 36-byte Xbox compressed vertex (only pos[0..2] needed for bounds here).
struct XboxCVertHdr { int pos[3]; int color; int uv; int norm; int tan; int b0; int b1; };

// Robust scene bounds: collects every drawn world-space vertex, then derives a
// per-axis MEDIAN center and a 90th-percentile radius. Raw min/max is fooled by
// a handful of outlier verts (degenerate geometry, occasional decompression
// artifacts) that blow the AABB up to e.g. z=121458 and leave the real model
// sub-pixel; the percentile framing ignores those.
struct Bounds {
    std::vector<Vector3> pts;
    Vector3 lo, hi;
    bool valid = false;
    void Add(const Vector3& p) {
        pts.push_back(p);
        if (!valid) { lo = hi = p; valid = true; return; }
        lo.x = p.x < lo.x ? p.x : lo.x; lo.y = p.y < lo.y ? p.y : lo.y; lo.z = p.z < lo.z ? p.z : lo.z;
        hi.x = p.x > hi.x ? p.x : hi.x; hi.y = p.y > hi.y ? p.y : hi.y; hi.z = p.z > hi.z ? p.z : hi.z;
    }
    Vector3 Center() const {
        if (pts.empty()) { Vector3 z; z.x = z.y = z.z = 0; return z; }
        std::vector<float> xs, ys, zs;
        xs.reserve(pts.size()); ys.reserve(pts.size()); zs.reserve(pts.size());
        for (auto& p : pts) { xs.push_back(p.x); ys.push_back(p.y); zs.push_back(p.z); }
        size_t m = pts.size() / 2;
        std::nth_element(xs.begin(), xs.begin()+m, xs.end());
        std::nth_element(ys.begin(), ys.begin()+m, ys.end());
        std::nth_element(zs.begin(), zs.begin()+m, zs.end());
        Vector3 c; c.x = xs[m]; c.y = ys[m]; c.z = zs[m]; return c;
    }
    float Radius(const Vector3& c) const {
        if (pts.empty()) return 1.0f;
        std::vector<float> d; d.reserve(pts.size());
        for (auto& p : pts) {
            float dx = p.x-c.x, dy = p.y-c.y, dz = p.z-c.z;
            d.push_back(sqrtf(dx*dx + dy*dy + dz*dz));
        }
        size_t k = (size_t)(d.size() * 0.90f);
        if (k >= d.size()) k = d.size()-1;
        std::nth_element(d.begin(), d.begin()+k, d.end());
        return d[k] > 0.01f ? d[k] : 1.0f;
    }
};

// Decode a big-endian Xbox-compressed position float (matches the backend).
static float BeF(int bits) {
    unsigned v = __builtin_bswap32((unsigned)bits); float f; memcpy(&f, &v, 4); return f;
}

static int CountAndBound(ObjectDir* dir, Bounds& b, int& meshCount, int& camCount, RndCam*& firstCam) {
    int total = 0;
    bool verbose = getenv("RB3_MESH_VERBOSE") != nullptr;
    for (ObjDirItr<Hmx::Object> it(dir, true); it; ++it) {
        Hmx::Object* o = it;
        total++;
        if (RndCam* cam = dynamic_cast<RndCam*>(o)) {
            camCount++;
            if (!firstCam) firstCam = cam;
        }
        if (RndMesh* mesh = dynamic_cast<RndMesh*>(o)) {
            RndMesh* owner = mesh->GeomOwner();
            if (!owner) owner = mesh;
            int nv = owner->mVerts.size();
            int ncomp = owner->mCompressedVerts ? (int)owner->mNumCompressedVerts : 0;
            int nf = (int)owner->mFaces.size();
            if (verbose) {
                printf("  mesh '%s' showing=%d verts=%d comp=%d faces=%d mat=%p geomOwner=%s\n",
                       mesh->Name() ? mesh->Name() : "(unnamed)", mesh->Showing() ? 1 : 0,
                       nv, ncomp, nf, (void*)mesh->Mat(),
                       owner == mesh ? "self" : (owner->Name() ? owner->Name() : "?"));
            }
            // Many milos store geometry on hidden template meshes (often
            // "_"-prefixed) that the game instances at runtime. For a static
            // render we draw any mesh that HAS geometry, regardless of Showing()
            // (unless RB3_ONLY_SHOWING=1).
            if (getenv("RB3_ONLY_SHOWING") && !mesh->Showing()) continue;
            if (nf <= 0) continue;
            int srcVerts = nv > 0 ? nv : ncomp;
            if (srcVerts <= 0) continue;
            meshCount++;
            const Transform& w = mesh->WorldXfm();
            const XboxCVertHdr* cv = (const XboxCVertHdr*)owner->mCompressedVerts;
            for (int i = 0; i < srcVerts; i++) {
                Vector3 lp;
                if (nv > 0) lp = owner->mVerts[i].pos;
                else { lp.x = BeF(cv[i].pos[0]); lp.y = BeF(cv[i].pos[1]); lp.z = BeF(cv[i].pos[2]); }
                Vector3 wp;
                wp.x = lp.x*w.m.x.x + lp.y*w.m.y.x + lp.z*w.m.z.x + w.v.x;
                wp.y = lp.x*w.m.x.y + lp.y*w.m.y.y + lp.z*w.m.z.y + w.v.y;
                wp.z = lp.x*w.m.x.z + lp.y*w.m.y.z + lp.z*w.m.z.z + w.v.z;
                b.Add(wp);
            }
        }
    }
    return total;
}

// Build a camera looking at the scene bounds from an offset, framing the model.
static RndCam* SynthesizeCamera(const Bounds& b) {
    RndCam* cam = Hmx::Object::New<RndCam>();
    if (!cam) return nullptr;

    Vector3 center = b.Center();
    float radius = b.Radius(center);

    float yfov = 0.9f; // ~51.5 deg
    float dist = radius / tanf(yfov * 0.5f) * 1.3f;

    // Milo camera-local: X=right, Y=forward(depth), Z=up. Place the camera on a
    // 3/4 diagonal (down -Y, off to +X, elevated +Z) so elongated geometry isn't
    // viewed edge-on. RB3_CAM_DIR=x,y,z overrides the (normalized) view dir.
    Vector3 dir; dir.x = 0.55f; dir.y = -1.0f; dir.z = 0.45f;
    const char* dirEnv = getenv("RB3_CAM_DIR");
    if (dirEnv) sscanf(dirEnv, "%f,%f,%f", &dir.x, &dir.y, &dir.z);
    float dl = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if (dl > 0) { dir.x /= dl; dir.y /= dl; dir.z /= dl; }
    Vector3 eye;
    eye.x = center.x + dir.x * dist;
    eye.y = center.y + dir.y * dist;
    eye.z = center.z + dir.z * dist;

    // World transform rows: x=right(+X), y=forward(toward center), z=up.
    Vector3 fwd; fwd.x = center.x - eye.x; fwd.y = center.y - eye.y; fwd.z = center.z - eye.z;
    float fl = sqrtf(fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z);
    if (fl > 0) { fwd.x /= fl; fwd.y /= fl; fwd.z /= fl; }
    Vector3 worldUp; worldUp.x = 0; worldUp.y = 0; worldUp.z = 1;
    // right = fwd x up  (then up = right x fwd)
    Vector3 right;
    right.x = fwd.y*worldUp.z - fwd.z*worldUp.y;
    right.y = fwd.z*worldUp.x - fwd.x*worldUp.z;
    right.z = fwd.x*worldUp.y - fwd.y*worldUp.x;
    float rl = sqrtf(right.x*right.x + right.y*right.y + right.z*right.z);
    if (rl > 0) { right.x /= rl; right.y /= rl; right.z /= rl; }
    Vector3 up;
    up.x = right.y*fwd.z - right.z*fwd.y;
    up.y = right.z*fwd.x - right.x*fwd.z;
    up.z = right.x*fwd.y - right.y*fwd.x;

    Transform w;
    w.m.x = right;  // camera-local X (right)
    w.m.y = fwd;    // camera-local Y (forward/depth)
    w.m.z = up;     // camera-local Z (up)
    w.v = eye;
    cam->SetWorldXfm(w);
    cam->SetFrustum(Max(0.1f, dist - radius * 2.0f), dist + radius * 4.0f + 10.0f, yfov, 1.0f);
    cam->SetScreenRect(Hmx::Rect(0, 0, 1, 1));

    printf("rb3-native: synth camera eye=(%.2f,%.2f,%.2f) center=(%.2f,%.2f,%.2f) r=%.2f dist=%.2f\n",
           eye.x, eye.y, eye.z, center.x, center.y, center.z, radius, dist);
    return cam;
}

// ---------------------------------------------------------------------------
// Reusable pieces (shared with the web boot machine via rb3_render_mesh.h).
// ---------------------------------------------------------------------------

// Inject the empty type-def stubs the legacy short-name milo classes need.
// Idempotent: only inserts if not already present. Requires gSystemConfig.
static void InjectTypeDefStubs() {
    DataArray* objCfg = gSystemConfig ? gSystemConfig->FindArray(Symbol("objects"), false) : nullptr;
    if (!objCfg) return;
    // The decomp names the texture/dir classes RndTex/RndDir (OBJ_CLASSNAME),
    // but the on-disc config's `objects` type-def array is keyed by the legacy
    // short names. When a loaded "Tex"/"Dir" object runs OBJ_SET_TYPE it does
    // SystemConfig("objects", StaticClassName()="RndTex", "types"), which
    // MILO_FAILs ("Couldn't find 'RndTex' in array"). Inject empty
    // (RndTex (types)) / (RndDir (types)) stubs so that lookup finds an (empty)
    // types array instead of faulting. Mesh geometry needs no real type-defs.
    if (objCfg->FindArray(Symbol("RndTex"), false) &&
        objCfg->FindArray(Symbol("RndDir"), false) &&
        objCfg->FindArray(Symbol("RndText"), false))
        return;
    // W2b: + RndText — the bandobj Dir factories (ChordShapeGenerator/ArpeggioShape)
    // construct RndText children, whose OBJ_SET_TYPE looks up "RndText" type-defs.
    DataArray* stubs = DataReadString("(RndTex (types)) (RndDir (types)) (RndText (types))");
    if (stubs) {
        objCfg->InsertNodes(objCfg->Size(), stubs);
        printf("rb3-render: injected RndTex/RndDir/RndText type-def stubs\n");
        stubs->Release();
    }
}

// Registers the band3/bandobj/track/world milo object-class factories (defined
// in rb3_game_object_factories.cpp). Without them a multi-chunk milo embedding a
// bandobj Dir subclass (OverdriveMeterDir, GemTrackDir, ...) can't construct the
// class -> nested-dir byte extent mis-skipped -> stream desync -> runaway
// std::vector<Viewport>::resize -> SIGSEGV / wasm OOM. Idempotent.
extern void RB3RegisterGameObjectFactories();

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>  // emscripten_sleep (JSPI yield)
#include <emscripten/heap.h>        // emscripten_get_heap_size
#include "utl/Loader.h"             // TheLoadMgr / PollFrontLoader
#include "utl/Std.h"                // ListFind

// Web milo load that YIELDS to the browser mid-load.
//
// DirLoader::LoadObjects() calls LoadMgr::PollUntilLoaded(), which pumps the
// loader to completion in ONE synchronous call with the per-state time-slice
// split DISABLED (it sets unk1c = 1e30, so CheckSplit() never trips and
// CreateObjects/LoadObjs process every object in a single PollFrontLoader). On
// native that's fine (tracksystem loads in ~9s). In the browser that whole
// load runs inside one rb3MainLoopTick() with NO return to the event loop — and
// because the split is disabled, even an inter-poll yield can't help: the entire
// 1100-object multi-chunk milo (tracksystem + its 10MB track_shared dep) is
// processed in ONE un-interruptible poll of ~38s wasm CPU. The browser then
// kills the unresponsive renderer tab (verified: the crash carries NO abort /
// "Cannot enlarge memory" / RangeError, and `window.rb3LoadPolls` stays 0 the
// whole time → a single in-progress poll, not OOM; bumping MAXIMUM_MEMORY to
// 2GB does NOT prevent it). This is the W2a/W2b web-asset-load wall.
//
// The fix uses the loader's OWN cooperative time-slice — the mechanism the Wii
// uses to spread a load across frames. We mirror LoadMgr::Poll()'s time-sliced
// (`#else`) body: set a small split budget (unk1c) and Restart() the split
// timer before each poll, so CheckSplit() trips after ~8ms and CreateObjects/
// LoadObjs return mid-load. Between polls we emscripten_sleep(0) — under JSPI
// (MILO_WEB_ASYNCIFY, default on) this resolves via setTimeout(…,0), yielding a
// full event-loop turn (paint + input) before the wasm stack resumes. The tab
// stays responsive and the load completes incrementally. This touches NO
// matched-fork code (the loop is re-derived from Loader.cpp's PollUntilLoaded /
// Poll; DirLoader's ctor / GetDir / IsLoaded and LoadMgr's mLoading / mTimer /
// unk1c are all public API).
static ObjectDir* LoadMiloDirYielding(const char* miloPath) {
    // Construct the loader exactly as DirLoader::LoadObjects does. The ctor
    // self-registers into TheLoadMgr.mLoading (Loader::Loader -> push_front).
    DirLoader loader(FilePath(miloPath), kLoadFront, NULL, NULL, NULL, false);

    // Per-poll wall-clock budget (ms) before CheckSplit() forces the front
    // loader's state to bail. Small enough that one poll never hangs the tab,
    // large enough to make real progress per slice.
    const float kSliceMs = 8.0f;

    Loader* theLdr = &loader;
    int polls = 0;
    while (!theLdr->IsLoaded()) {
        // Mirror LoadMgr::Poll()'s time-sliced body: arm the split budget and
        // restart the split timer so CheckSplit() trips after kSliceMs of work.
        TheLoadMgr.unk1c = kSliceMs;
        TheLoadMgr.mTimer.Restart();

        TheLoadMgr.PollFrontLoader();
        polls++;

        // Faithful PollUntilLoaded queue maintenance: a dep (track_shared,
        // ingame_bank) spawns a new front loader; once it finishes it MUST be
        // popped or PollFrontLoader keeps re-polling the done loader forever and
        // our DirLoader never advances (an infinite-poll livelock).
        if (!ListFind(TheLoadMgr.mLoading, theLdr))
            break;
        if (TheLoadMgr.mLoading.front()->IsLoaded())
            TheLoadMgr.mLoading.pop_front();

        // Expose load progress to the page (and the test harness) every few
        // polls; throttle the console trace to keep it readable on big loads.
        if ((polls & 3) == 0)
            EM_ASM({ window.rb3LoadPolls = $0; }, polls);
        if ((polls % 50) == 0) {
            Loader* f = TheLoadMgr.mLoading.empty() ? nullptr : TheLoadMgr.mLoading.front();
            printf("rb3-render: load poll %d heap=%zuMB front=%s\n",
                   polls, (size_t)(emscripten_get_heap_size() / (1024 * 1024)),
                   f ? f->mFile.c_str() : "(empty)");
        }
        emscripten_sleep(0);  // JSPI: suspend -> event loop -> resume
    }
    printf("rb3-render: yielding load done (%d polls)\n", polls);
    return loader.GetDir();
}
#endif  // __EMSCRIPTEN__

WalkResult LoadMiloAndWalk(const char* miloPath) {
    WalkResult r;
    if (!miloPath || !*miloPath) {
        fprintf(stderr, "rb3-render: LoadMiloAndWalk needs a .milo path\n");
        return r;
    }

    InjectTypeDefStubs();
    RB3RegisterGameObjectFactories();

    printf("rb3-render: loading '%s'\n", miloPath);
#ifdef __EMSCRIPTEN__
    // Browser: pump the loader with periodic JSPI yields so the page stays
    // responsive during the long synchronous multi-chunk load (see above).
    ObjectDir* dir = LoadMiloDirYielding(miloPath);
#else
    ObjectDir* dir = DirLoader::LoadObjects(FilePath(miloPath), nullptr, nullptr);
#endif
    if (!dir) {
        fprintf(stderr, "rb3-render: DirLoader::LoadObjects returned null\n");
        return r;
    }
    r.dir = dir;

    Bounds bounds;
    int meshCount = 0, camCount = 0;
    RndCam* firstCam = nullptr;
    int total = CountAndBound(dir, bounds, meshCount, camCount, firstCam);
    r.totalObjects = total;
    r.meshCount = meshCount;
    r.camCount = camCount;
    printf("rb3-render: dir '%s' [%s] — %d objects, %d drawable meshes, %d cams\n",
           dir->Name() ? dir->Name() : "(unnamed)", dir->ClassName().Str(),
           total, meshCount, camCount);
    if (bounds.valid) {
        printf("rb3-render: scene bounds lo=(%.2f,%.2f,%.2f) hi=(%.2f,%.2f,%.2f)\n",
               bounds.lo.x, bounds.lo.y, bounds.lo.z, bounds.hi.x, bounds.hi.y, bounds.hi.z);
    }

    if (meshCount == 0) {
        fprintf(stderr, "rb3-render: no drawable meshes in this milo; nothing to render\n");
        return r;
    }

    // Pick a camera: prefer a synthesized framing camera (loaded scene cameras
    // often point elsewhere). Use RB3_USE_SCENE_CAM=1 to use the milo's camera.
    RndCam* cam = nullptr;
    if (getenv("RB3_USE_SCENE_CAM") && firstCam) {
        cam = firstCam;
        printf("rb3-render: using scene camera '%s'\n", cam->Name() ? cam->Name() : "(unnamed)");
    } else if (bounds.valid) {
        cam = SynthesizeCamera(bounds);
    }
    if (cam) {
        RndCam::sCurrent = cam;
        cam->Select();
    }
    r.cam = cam;
    r.ok = true;
    return r;
}

void RenderFrame(const WalkResult& walk) {
    if (!walk.ok || !walk.dir) return;
    // Re-select the camera each frame (a different milo/cam may have run
    // BeginDrawing in between on the web App path; keep our framing cam current).
    if (walk.cam) {
        RndCam::sCurrent = walk.cam;
        walk.cam->Select();
    }

    // For showing meshes, go through RndMesh::DrawShowing (the engine body,
    // proving the matched-fork virtual dispatch -> BandRnd::DrawMesh). For
    // hidden template-geometry meshes (the common case for these milos) call
    // BandRnd::DrawMesh directly so the static scene still renders.
    bool onlyShowing = getenv("RB3_ONLY_SHOWING") != nullptr;
    gBandRnd.BeginFrame(walk.cam);
    for (ObjDirItr<Hmx::Object> it(walk.dir, true); it; ++it) {
        Hmx::Object* o = it;
        if (RndMesh* mesh = dynamic_cast<RndMesh*>(o)) {
            RndMesh* owner = mesh->GeomOwner(); if (!owner) owner = mesh;
            bool hasGeom = owner->mFaces.size() > 0 &&
                           (owner->mVerts.size() > 0 || owner->mNumCompressedVerts > 0);
            if (!hasGeom) continue;
            if (mesh->Showing())
                mesh->DrawShowing();          // engine virtual body
            else if (!onlyShowing)
                gBandRnd.DrawMesh(mesh);      // hidden template geometry
        }
    }
    gBandRnd.EndFrame();
}

#ifndef __EMSCRIPTEN__
// NATIVE-ONLY: render one frame, read it back, write a PNG, and _exit(rc). The
// PNG readback path needs a headless RGBA8 target; _exit dodges the
// RB3-ObjectDir-vs-Dawn-device static-destructor race on process exit. This
// MUST stay out of the web build (the browser loops forever).
int RenderToPng(const WalkResult& walk) {
    if (!walk.ok) return 1;

    RenderFrame(walk);

    GpuDevice& gpu = gBandRnd.Gpu();
    int gw = gpu.WindowWidth(), gh = gpu.WindowHeight();
    std::vector<uint8_t> pixels((size_t)gw * gh * 4);
    if (!gpu.ReadbackHeadlessFrame(pixels.data(), pixels.size())) {
        fprintf(stderr, "rb3-native: readback FAILED\n");
        return 1;
    }

    // Clear color ~ (31,36,46). Count non-clear pixels = rendered geometry.
    int clearR = (int)(0.12f * 255), clearG = (int)(0.14f * 255), clearB = (int)(0.18f * 255);
    int nonClear = 0;
    for (size_t i = 0; i < (size_t)gw * gh; i++) {
        const uint8_t* p = &pixels[i * 4];
        if (abs((int)p[0] - clearR) > 14 || abs((int)p[1] - clearG) > 14 || abs((int)p[2] - clearB) > 14)
            nonClear++;
    }
    const uint8_t* center = &pixels[((size_t)(gh / 2) * gw + (gw / 2)) * 4];
    printf("rb3-native: center pixel RGBA=(%u,%u,%u,%u); non-clear pixels=%d / %d (%.1f%%)\n",
           center[0], center[1], center[2], center[3], nonClear, gw * gh,
           100.0 * nonClear / (gw * gh));

    const char* outPath = getenv("RB3_RENDER_MESH_PNG");
    char defPath[] = "/tmp/rb3_render_mesh.png";
    if (!outPath) outPath = defPath;
    int rc = 0;
    if (WritePNG(outPath, pixels.data(), gw, gh)) {
        printf("rb3-native: wrote mesh frame to %s\n", outPath);
    } else {
        fprintf(stderr, "rb3-native: WritePNG FAILED\n");
        rc = 1;
    }

    printf("rb3-native: RENDER_MESH %s\n", rc == 0 ? "OK" : "FAILED");
    // The render + PNG are complete and flushed. Skip the global/static
    // teardown (RB3 ObjectDir + Dawn device destructors race during exit, which
    // can fault after a clean render). _exit avoids running those destructors.
    fflush(stdout); fflush(stderr);
    _exit(rc);
    return rc;  // not reached
}

int RunRenderMesh(int argc, char **argv, const char *miloPath) {
    if (!miloPath) {
        fprintf(stderr, "rb3-native: RENDER_MESH needs a .milo path argument\n");
        return 1;
    }

    const char *dataDir = getenv("RB3_DATA");
    if (!dataDir) dataDir = "/home/free/code/milohax/rb3/orig-assets/extracted";

    // Resolve the milo path BEFORE chdir. An absolute path is used as-is; a
    // relative path is interpreted relative to the data dir (the same way
    // RunBoot loads it after chdir'ing there), so e.g.
    // `world/shared/.../foo.milo_xbox` resolves under RB3_DATA.
    char absMilo[4096];
    if (miloPath[0] == '/') {
        snprintf(absMilo, sizeof(absMilo), "%s", miloPath);
    } else {
        // If it exists relative to the current cwd, keep that; else anchor to data dir.
        if (access(miloPath, F_OK) == 0) {
            char cwd[2048]; getcwd(cwd, sizeof(cwd));
            snprintf(absMilo, sizeof(absMilo), "%s/%s", cwd, miloPath);
        } else {
            snprintf(absMilo, sizeof(absMilo), "%s/%s", dataDir, miloPath);
        }
    }

    // Stand up the GpuDevice FIRST — before chdir + the RB3 boot. Dawn/Vulkan
    // adapter enumeration reads the Vulkan loader's layer-discovery (which can
    // use relative paths) and allocates through the system allocator; doing it
    // in the original cwd, before RB3's MemMgr / config boot perturbs global
    // state, keeps it on the same clean path the GPU-smoke + triangle modes use.
    gBandRnd.SetClearColor(Hmx::Color(0.12f, 0.14f, 0.18f));
    bool headless = (getenv("MILO_HEADLESS") != nullptr) || (getenv("DISPLAY") == nullptr);
    int W = getenv("MILO_WIDTH") ? atoi(getenv("MILO_WIDTH")) : 640;
    int H = getenv("MILO_HEIGHT") ? atoi(getenv("MILO_HEIGHT")) : 480;
    if (!gBandRnd.InitGpu(W, H, headless)) return 1;

    if (chdir(dataDir) != 0) {
        fprintf(stderr, "rb3-native: chdir('%s') failed\n", dataDir);
        return 1;
    }
    printf("rb3-native: RENDER_MESH — data dir '%s'\n", dataDir);

    TheLoadMgr.mPlatform = kPlatformXBox;
    SetSystemArgs(argc, argv);

    printf("rb3-native: SystemPreInit/SystemInit...\n");
    SystemPreInit("config/band_preinit_keep.dta");
    SystemInit("config/band_keep.dta");
    DataArray* objCfg = gSystemConfig ? gSystemConfig->FindArray(Symbol("objects"), false) : nullptr;
    printf("rb3-native: config ready — objects-cfg=%p (%d entries)\n",
           (void*)objCfg, objCfg ? objCfg->Size() : -1);

    // Inject the legacy short-name type-def stubs BEFORE PreInitRender (matches
    // the original ordering). LoadMiloAndWalk re-runs this idempotently.
    InjectTypeDefStubs();

    // Register the rndobj factories (after the config boot, since the Init fns
    // read gSystemConfig).
    gBandRnd.PreInitRender();

    // Load + walk (also injects the type-def stubs), then render to PNG (_exit).
    WalkResult walk = LoadMiloAndWalk(absMilo);
    if (!walk.ok) return 1;
    return RenderToPng(walk);
}
#endif  // !__EMSCRIPTEN__
