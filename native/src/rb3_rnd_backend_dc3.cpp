// rb3_rnd_backend_dc3.cpp — RB3RndBackend over the `dc3` flavor (WgpuRnd).
//
// Linked only when native/CMakeLists.txt has RB3_GPU_BACKEND=dc3. The engine's
// WgpuRnd is the renderer rb3-xenon and dc3-decomp use; it reads RB3-Wii's
// rndobj through src/platform/rndshape/RndShape_RB3Wii.h.
//
// Besides the facade, this TU defines the renderer hooks RB3's fork and game
// glue call by name that WgpuRnd has no use for (the menu UI post-grade flush,
// the outfit-compose latch), the legacy class aliases and the exit-time GPU
// teardown. The draw log, provenance scopes and progressive texture sharpen
// come from the engine (rndshape/RB3WiiDrawLog.cpp, platform/RB3TexSharpen.cpp
// with rndshape/RB3WiiTexSharpen.cpp), as they do for the rb3 flavor.
#include "rb3_rnd_backend.h"

#include "platform/Rnd_Wgpu.h"
#include "gfx/GpuDevice.h"
#include "rndobj/Cam.h"
#include "rndobj/DOFProc.h"
#include "rndobj/PostProc.h"
#include "math/Utl.h"
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
#include <cstdlib>

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
// Depth of field. RB3's base DOFProc ignores Set and reports Enabled() false,
// so camera shots never reached the engine's DofPass. This keeps what retail
// NgDOFProc::Set (0x82B8BF78) keeps: the focal plane, the blur depth and blur
// range after RndPostProc::DOFOverrides, and Enabled = maxBlur > 0. The
// projected-depth scale and bias retail also computes here are derived by
// gfx/DofPass.cpp from the same values and the current camera.
// ---------------------------------------------------------------------------
namespace {
class NativeDOFProc : public DOFProc {
public:
    NativeDOFProc()
        : mEnabled(false), mFocalPlane(1), mBlurDepth(1), mMinBlur(0), mMaxBlur(1) {}
    NEW_OBJ(NativeDOFProc)

    void Set(RndCam *, float focalPlane, float blurDepth, float maxBlur,
             float minBlur) override {
        DOFOverrideParams &o = RndPostProc::DOFOverrides();
        mFocalPlane = focalPlane;
        mBlurDepth = Max(o.mDepthScale * blurDepth + o.mDepthOffset, 0.0f);
        mMaxBlur = Clamp(0.0f, 1.0f, o.mMaxBlurScale * maxBlur + o.mMaxBlurOffset);
        mMinBlur = Clamp(0.0f, 1.0f, o.mMinBlurScale * minBlur + o.mMinBlurOffset);
        mEnabled = mMaxBlur > 0.0f;
        if (mBlurDepth <= 0.001f)
            mBlurDepth = 0.001f;
    }
    void UnSet() override { mEnabled = false; }
    bool Enabled() const override { return mEnabled; }
    float FocalPlane() override { return mFocalPlane; }
    float BlurDepth() override { return mBlurDepth; }
    float MaxBlur() override { return mMaxBlur; }
    float MinBlur() override { return mMinBlur; }

private:
    bool mEnabled;
    float mFocalPlane;
    float mBlurDepth;
    float mMinBlur;
    float mMaxBlur;
};
} // namespace

// Called by DOFProc::Init (rndobj/DOFProc.cpp) through a weak reference.
void RB3RegisterNativeDOFProc() {
    Hmx::Object::RegisterFactory(DOFProc::StaticClassName(), NativeDOFProc::NewObject);
}

// ---------------------------------------------------------------------------
// Fork-facing hooks the rb3 flavor implements and WgpuRnd has no use for.
// ---------------------------------------------------------------------------
class Rnd;
void RB3FlushMenuUIPostGrade(Rnd *) {}
bool gRB3OutfitComposeActive = false;

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
    // A process that never reaches Debug::Exit (the gtest binary returns from
    // main) would otherwise release WgpuRnd's handles from its static
    // destructor, after Dawn's Vulkan backend is gone, and segfault at exit.
    // atexit handlers registered now run before that destructor; the callback
    // is a no-op once the teardown has run.
    static bool sAtExit = false;
    if (sGpuReady && !sAtExit) {
        sAtExit = true;
        std::atexit(WgpuRndShutdownExitCallback);
    }
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
