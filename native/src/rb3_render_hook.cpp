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
// RB3 builds with MILO_ENGINE_BUILD_GFX=ON and the `rb3` GPU-backend flavor
// (native/CMakeLists.txt FORCEs it): the engine's DC3-shaped Rnd_Wgpu.cpp is
// NOT linked, but RB3's own backend Rnd_Wgpu_RB3.cpp (BandRnd : Rnd) IS
// compiled and linked into rb3-native. That renderer does not yet dispatch the
// two frame-pass stages (W1.7 wires them at the RB3 frame seams — both stay
// no-ops here because RB3 has no native overlay/impostor pass yet), and the
// per-draw policy methods below are inert until W1.7.S2–S4 relocate the engine
// renderer's inline asset-name branches into them. The file self-registers
// gBandHook at C++ static-init time (SetGameRenderHook just stores a pointer).
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

    // ------------------------------------------------------------------------
    // Per-draw policy (W1.7). No-op today: each method returns the default
    // "no override" POD so the engine renderer keeps running its inline asset-
    // name branches unchanged (byte-identical output). W1.7.S2–S4 move the
    // actual name matches + `RB3_*` env-flag reads INTO these methods, one
    // behavior per commit; until then the seam exists but is inert.
    // ------------------------------------------------------------------------
    DrawGeomPolicy QueryDrawGeomPolicy(RndMesh* /*mesh*/,
                                       float* /*outWorld16*/) override {
        return DrawGeomPolicy();
    }

    DrawMaterialPolicy QueryDrawMaterialPolicy(RndMesh* /*mesh*/,
                                               RndMat* /*mat*/,
                                               bool /*skinned*/,
                                               RndMesh* /*owner*/,
                                               const char* /*camName*/) override {
        return DrawMaterialPolicy();
    }

    HaloPolicy QueryHaloPolicy(RndMat* /*mat*/) override {
        return HaloPolicy();
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
