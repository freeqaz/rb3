// rb3_rnd_backend_dc3.cpp — RB3RndBackend over the `dc3` flavor (WgpuRnd).
//
// Linked only when native/CMakeLists.txt has RB3_GPU_BACKEND=dc3. The engine's
// WgpuRnd is the renderer rb3-xenon and dc3-decomp use; it reads RB3-Wii's
// rndobj through src/platform/rndshape/RndShape_RB3Wii.h.
//
// Besides the facade, this TU defines the renderer hooks RB3's fork and game
// glue call by name (draw-provenance scopes, the menu UI post-grade flush, the
// outfit-compose latch, the legacy class aliases, the exit-time GPU teardown) and
// the BandRnd instrumentation entry points the harnesses read (draw log,
// progressive texture sharpen). Those last two are BandRnd features WgpuRnd does
// not have; here they report "off" (empty log, sharpen disabled), which is the
// state every caller already handles because both are env-gated off by default
// under the rb3 flavor too.
#include "rb3_rnd_backend.h"

#include "platform/Rnd_Wgpu.h"
#include "platform/RB3DrawLogDebug.h"
#include "platform/RB3TexSharpen.h"
#include "gfx/GpuDevice.h"
#include "rndobj/Cam.h"
#include "rndobj/Dir.h"
#include "rndobj/Env.h"
#include "rndobj/Group.h"
#include "rndobj/Lit.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Stats_NG.h"
#include "rndobj/MultiMesh.h"
#include "rndobj/Tex.h"
#include "rndobj/Text.h"
#include "rndobj/Trans.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/Symbol.h"

#include <cstdio>

// Mesh_Wgpu.cpp: draw one mesh now, whatever its showing flag says.
void DrawMeshImmediate(RndMesh *mesh);

// ---------------------------------------------------------------------------
// Render statistics. RB3's platform renderer (rndwii/Rnd.cpp) owns the
// NgStats block; the native link only has band3_link_stubs.s's weak 256-byte
// zero blob under the same name, which read as a pointer is null. Under this
// backend SpotlightDrawer::EndWorld -> DrawWorld reads and writes
// TheNgStats->mMotionBlurs on every venue frame (world/SpotlightDrawer.cpp), so
// game_screen segfaulted in Draw() each frame from its first frame and the
// track, HUD and every post-world draw were skipped. A real block fixes it.
// ---------------------------------------------------------------------------
static NgStats sNgStats;
NgStats *TheNgStats = &sNgStats;

// ---------------------------------------------------------------------------
// Legacy class aliases. RB3's 2010-era milos name these classes "Tex" / "Text" /
// "Dir"; the fork registers them as RndTex / RndText / RndDir. Same body as the
// rb3 flavor's (Rnd_Wgpu_RB3.cpp) — this is fork bookkeeping, not rendering.
// ---------------------------------------------------------------------------
void RB3RegisterLegacyRndAliases() {
    Hmx::Object::RegisterFactory(Symbol("Tex"), RndTex::NewObject);
    Hmx::Object::RegisterFactory(Symbol("Text"), RndText::NewObject);
    Hmx::Object::RegisterFactory(Symbol("Dir"), RndDir::NewObject);
}

namespace {
bool sPreInited = false;
bool sGpuReady = false;

// Exit-time GPU teardown, ahead of libc's static-destructor phase (the Vulkan
// ICD is unmapped by then). WgpuRnd::Terminate releases every wgpu handle it
// owns and then the device.
void WgpuRndShutdownExitCallback() {
    if (sGpuReady && gWgpuRnd) {
        gWgpuRnd->Terminate();
        sGpuReady = false;
    }
}
} // namespace

void RB3RegisterBandRndShutdown() { TheDebug.AddExitCallback(WgpuRndShutdownExitCallback); }

// ---------------------------------------------------------------------------
// Fork-facing hooks the rb3 flavor implements and WgpuRnd has no use for.
// ---------------------------------------------------------------------------
void RB3DrawScopePush(int, const char *) {}
void RB3DrawScopePop(int) {}
class Rnd;
void RB3FlushMenuUIPostGrade(Rnd *) {}
bool gRB3OutfitComposeActive = false;

// BandRnd draw log / provenance: not recorded by WgpuRnd.
const std::vector<RB3DrawRecord> &RB3DebugGetDrawLog() {
    static const std::vector<RB3DrawRecord> sEmpty;
    return sEmpty;
}
const std::vector<RB3DrawProv> &RB3DebugGetDrawProv() {
    static const std::vector<RB3DrawProv> sEmpty;
    return sEmpty;
}
void RB3DebugSetDrawLogEnabled(bool) {}
bool RB3DebugDrawLogEnabled() { return false; }

// BandRnd progressive texture sharpen: not implemented by WgpuRnd.
bool RB3ProgressiveSharpenEnabled() { return false; }
int RB3SharpenPerFrame() { return 0; }
int RB3SharpenLoadSidecar(ObjectDir *, const uint8_t *, uint32_t) { return 0; }
int RB3SharpenStep(int) { return 0; }
bool RB3SharpenComplete() { return true; }
void RB3SharpenReset() {}
RB3SharpenStatus RB3SharpenGetStatus() { return RB3SharpenStatus(); }

// ---------------------------------------------------------------------------
// RB3RndBackend
// ---------------------------------------------------------------------------
namespace RB3RndBackend {

const char *FlavorName() { return "dc3"; }

void PreInitRender() {
    if (sPreInited)
        return;
    sPreInited = true;
    // The rndobj factory block of Rnd::PreInit, minus the GPU / overlay /
    // console / default-object parts (identical list to the rb3 flavor's).
    RndTransformable::Init();
    RndCam::Init();
    RndMesh::Init();
    RndEnviron::Init();
    RndMat::Init();
    RndTex::Init();
    RndLight::Init();
    RndMultiMesh::Init();
    RndTransformable::Register();
    RndGroup::Init();
    RndDir::Init();
    RB3RegisterLegacyRndAliases();
    printf("RB3RndBackend[dc3]: rndobj factories registered\n");
}

bool StartGpuInit(int width, int height, bool headless) {
    GpuDeviceDesc desc{};
    desc.headless = headless;
    desc.width = width;
    desc.height = height;
    desc.title = "RB3 Native — WebGPU (dc3 backend)";
    if (!gWgpuRnd->mGpu.Init(desc)) {
        fprintf(stderr, "RB3RndBackend[dc3]: GpuDevice init failed\n");
        return false;
    }
    // The Rnd virtual resolution DrawRect coordinates are expressed in.
    gWgpuRnd->mWidth = width;
    gWgpuRnd->mHeight = height;
    return true;
}

void InitGpuResources() {
    gWgpuRnd->InitGpuResources();
    sGpuReady = gWgpuRnd->GpuResourcesReady();
    printf("RB3RndBackend[dc3]: WgpuRnd up (%dx%d, %s)\n", gWgpuRnd->mGpu.WindowWidth(),
           gWgpuRnd->mGpu.WindowHeight(), gWgpuRnd->mGpu.IsHeadless() ? "headless" : "windowed");
}

bool InitGpu(int width, int height, bool headless) {
    if (sGpuReady)
        return true;
    if (!StartGpuInit(width, height, headless))
        return false;
    if (!gWgpuRnd->mGpu.IsReady()) {
        fprintf(stderr, "RB3RndBackend[dc3]: GpuDevice not ready after sync init\n");
        return false;
    }
    InitGpuResources();
    return sGpuReady;
}

bool GpuReady() { return sGpuReady; }
GpuDevice &Gpu() { return gWgpuRnd->Gpu(); }
void SetClearColor(const Hmx::Color &c) { gWgpuRnd->SetClearColor(c); }
// WgpuRnd arms MILO_SCREENSHOT_DIR / _FRAMES itself, in InitGpuResources.
void InitScreenshots() {}

void BeginFrame(RndCam *cam) {
    if (cam)
        cam->Select();
    gWgpuRnd->BeginDrawing();
}
void DrawMesh(RndMesh *mesh) { DrawMeshImmediate(mesh); }
void EndFrame() { gWgpuRnd->EndDrawing(); }
bool InPass() { return gWgpuRnd->IsInPass(); }
int FrameCount() { return gWgpuRnd->FrameID(); }
// WgpuRnd uploads lazily at first draw; it has no ahead-of-time warm sweep.
int WarmGpuForDir(ObjectDir *, float) { return 0; }

} // namespace RB3RndBackend

// ---------------------------------------------------------------------------
// The engine's two consumer seams the dc3 backend calls (platform/MeshFilter.h,
// platform/DebugPanel.h). The answers are rb3-xenon's (its
// native/src/rb3_render_glue.cpp), and the reasons carry over unchanged to Wii
// RB3: DC3's mesh filter is a list of Kinect overlays, and RB3 on either console
// has no Kinect content to suppress; DebugPanel is an ImGui slider overlay DC3
// owns, with no RB3 counterpart.
// ---------------------------------------------------------------------------
#include "platform/DebugPanel.h"
#include "platform/MeshFilter.h"

bool ShouldSkipMesh(const char * /*meshName*/, RndMat * /*mat*/) { return false; }

namespace DebugPanel {
    void Init() {}
    void Draw() {}
    bool IsVisible() { return false; }
    void Toggle() {}
    void SetVisible(bool) {}
} // namespace DebugPanel
