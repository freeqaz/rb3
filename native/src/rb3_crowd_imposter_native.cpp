// ===========================================================================
// rb3_crowd_imposter_native.cpp — 2D bowl-imposter crowd (render-polish Fix B)
//
// Arena/festival/big_club venues render their bowl crowd as 2D camera-facing
// imposters: each crowd archetype Character is rendered ONCE per frame into a
// shared render-target texture (RTT), then thousands of billboard quads sample
// that texture (WorldCrowd::DrawShowing's 2D path, src/system/world/Crowd.cpp).
//
// On the Wii that RTT is `WiiRnd::GetSharedTex(...)`. On the native WebGPU
// backend `WiiRnd::GetSharedTex` was a weak no-op stub returning null
// (band3_link_stubs.s) → `gImpostorCamera` got no TargetTex → the engine's lazy
// mid-frame render-to-texture (BandRnd::BeginDrawTarget, fired from DrawMesh off
// `RndCam::sCurrent->TargetTex()`) never triggered → the bowl crowd was dead
// (empty bowl). This TU provides a STRONG `WiiRnd::GetSharedTex` that returns one
// persistent square RndTex render target, wiring the imposter path to the
// already-complete engine RTT machinery (BeginDrawTarget/EndDrawTarget +
// RndTex::FinishDrawTarget, closed per-archetype by `curCam->Select()`).
//
// `TheWiiRnd` itself is a weak no-op stub natively (rndobj_synth_link_stubs.s),
// so `this` is NOT a real WiiRnd — GetSharedTex deliberately ignores `this` and
// returns the file-static tex. The Wii `mSharedTexture[]` static array is size 3
// (kNumSharedTexTypes); the crowd asks for type 5 (out of range) — another reason
// to own a private persistent tex rather than touch that array.
//
// Companion native gap (Gap 2, in shared src behind HX_NATIVE): the billboard
// quads need camera-facing orientation. The portable RndMultiMesh::DrawShowing
// has an HX_NATIVE kFastBillboardXYZ branch (src/system/rndobj/MultiMesh.cpp) so
// the quads face the camera (the Wii path does this in rndwii/MultiMesh.cpp).
//
// Opt-out: RB3_CROWD_IMPOSTER_OFF=1 → GetSharedTex returns null (the prior dead
// behaviour — empty bowl, no RTT). Default ON.
//
// LANDING: rb3-only, this file + its two CMake list entries + the GetSharedTex
// weak-stub removal in band3_link_stubs.s. No engine change for Gap 1/Gap 3
// (Gap 3 — square RT aspect — is already handled by RndCam::ScreenAspect
// factoring mTargetTex->Height()/Width(), Cam.cpp:157-158).
// ===========================================================================
#ifdef HX_NATIVE

#include "rndwii/Rnd.h"   // WiiRnd, WiiRnd::SharedTexType, TheWiiRnd
#include "rndobj/Tex.h"   // RndTex, RndTex::kRendered, SetBitmap
#include "obj/Object.h"   // Hmx::Object::New

#include <cstdlib>
#include <cstdio>

namespace {

// One persistent square render target shared by every crowd archetype's imposter
// pass this frame. 256x256 RGBA8 — square so RndCam::ScreenAspect yields the
// authored kAspect=1.0 imposter projection (Crowd.cpp), and big enough that a
// camera-tight single character reads cleanly when billboarded across the bowl.
RndTex *gCrowdImposterTex = nullptr;

bool CrowdImposterDisabled() {
    static int s = -1;
    if (s < 0) {
        const char *e = getenv("RB3_CROWD_IMPOSTER_OFF");
        s = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s != 0;
}

} // namespace

// Strong def — wins over the weak no-op stub in band3_link_stubs.s. Non-virtual
// member; `this` (== &TheWiiRnd, itself a stub) is intentionally unused.
RndTex *WiiRnd::GetSharedTex(WiiRnd::SharedTexType, bool) {
    if (CrowdImposterDisabled()) return nullptr;
    if (!gCrowdImposterTex) {
        gCrowdImposterTex = Hmx::Object::New<RndTex>();
        // kRendered (0x2): Width()/Height()>0 so BandRnd::BeginDrawTarget accepts
        // it, and IsRenderTarget() (mType & kRendered) is true so the diffuse-bind
        // path treats the lazily-painted RT view as the billboard material's tex.
        // No bitmap data → PresyncBitmap is a safe no-op; the RT view is created
        // on first BeginDrawTarget and stored in the backend's sTexGpu side-table.
        gCrowdImposterTex->SetBitmap(256, 256, 32, RndTex::kRendered, false, "");
        // (No SetName — Hmx::Object::SetName asserts a non-null ObjectDir, and the
        // imposter tex is a free-standing global with no owning dir. The engine's
        // RTT-create debug log tolerates a null Name() → prints "?".)
        if (getenv("RB3_RENDER_DBG"))
            fprintf(stderr, "[crowd2d] created shared imposter RT 256x256\n");
    }
    return gCrowdImposterTex;
}

#endif // HX_NATIVE
