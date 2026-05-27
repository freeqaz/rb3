// rb3_render_hook.cpp — RB3-specific implementation of the engine's
// GameRenderHook interface (the seam graduated in engine commit 9a58e86).
//
// Mirrors dc3-decomp/native/src/dc3_render_hook.cpp. The engine's WebGPU
// renderer (Rnd_Wgpu.cpp) dispatches two game-driven stages through this hook
// (a HUD/overlay draw and a per-character render-to-texture loop) so the
// renderer never names game-specific types. DC3 fills the slot with HamDirector
// / HamCharacter logic; RB3's analog (band gameplay overlay + per-player RTT)
// will live here.
//
// State today
// -----------
// RB3 is built with MILO_ENGINE_BUILD_GFX OFF (its 2010-era rndobj/ cannot
// compile against the DC3-wired WebGPU layer — see native/CMakeLists.txt). So
// the engine's Rnd_Wgpu.cpp is NOT linked into rb3-native and these methods are
// never actually called yet. We keep the implementation anyway so the seam
// exists for the day RB3 gets its own renderer: the file self-registers
// gBandHook at C++ static-init time (harmless — SetGameRenderHook just stores a
// pointer), and the symbol is benign on the GFX-off link line.
//
// Linkage
// -------
// A file-scope static struct BandRenderHookAutoRegister registers gBandHook
// with the engine at static-init time. Every RB3 target that links this TU gets
// the hook registered automatically; there is no explicit init call.

#include "platform/GameRenderHook.h"

namespace {

class BandRenderHook : public GameRenderHook {
public:
    // Band gameplay HUD/overlay pass. On a future RB3 renderer this would issue
    // the track/score/HUD overlay draws after the venue is resolved into the
    // framebuffer. No-op today (GFX off; no renderer linked).
    void DrawGameOverlay(void* /*renderCtx*/) override {
        // No-op today. See comment above.
    }

    // Per-player impostor / render-to-texture pre-pass. RB3's band gameplay has
    // no impostor RTT loop wired on native yet; no-op today.
    void RenderCharacterImpostors(void* /*renderCtx*/) override {
        // No-op today. See comment above.
    }
};

BandRenderHook gBandHook;

struct BandRenderHookAutoRegister {
    BandRenderHookAutoRegister() { SetGameRenderHook(&gBandHook); }
};

BandRenderHookAutoRegister gBandRenderHookAutoRegister;

}  // namespace

// Public init hook, callable from startup code that wants explicit ordering
// over static-init. Idempotent.
void RegisterBandRenderHook() {
    SetGameRenderHook(&gBandHook);
}
