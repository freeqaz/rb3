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

#include <cstdlib>
#include <cstring>

#include "rndobj/Mesh.h"  // RndMesh::Name() for the relocated per-draw name matches
#include "rndobj/Mat.h"   // RndMat::Name() for the relocated material/halo name matches

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
    // Per-draw geometric / draw-guard policy (W1.7 B1–B5). Each branch here was
    // an inline RB3 asset-name match in the engine renderer's DrawMesh; relocated
    // so the shared engine no longer names RB3 content. The engine keeps the
    // matrix/palette math and applies these DECISIONS; each RB3_* opt-out flag is
    // read (and statically cached once) HERE, preserving the prior semantics.
    DrawGeomPolicy QueryDrawGeomPolicy(RndMesh* mesh,
                                       float* /*outWorld16*/) override {
        DrawGeomPolicy p;
        const char* nm = mesh ? mesh->Name() : nullptr;
        // B1: hub focused-menu highlight-bar placement fix (skinned UI bar whose
        // label translation must be injected). Opt-out RB3_NO_HUB_BAR_PLACEMENT_FIX.
        // The engine ANDs this with `skinned`, matching the prior inline guard.
        if (nm && (std::strncmp(nm, "highlight_main", 14) == 0 ||
                   std::strncmp(nm, "highlight_pattern", 17) == 0)) {
            static int hubBarOff = -1;
            if (hubBarOff < 0)
                hubBarOff = std::getenv("RB3_NO_HUB_BAR_PLACEMENT_FIX") ? 1 : 0;
            p.hubBarPlacement = !hubBarOff;
        }
        // B4: hub focused-menu highlight-bar shard-guard exemption (same named
        // meshes as B1, independent flag). The skinned highlight quad legitimately
        // stretches many times its bind AABB to shrink-wrap a wide label, which the
        // scene-crossing shard guard would false-drop. Opt-out
        // RB3_NO_HUB_BAR_SHARD_EXEMPT.
        if (nm && (std::strncmp(nm, "highlight_main", 14) == 0 ||
                   std::strncmp(nm, "highlight_pattern", 17) == 0)) {
            static int shardExemptOff = -1;
            if (shardExemptOff < 0)
                shardExemptOff = std::getenv("RB3_NO_HUB_BAR_SHARD_EXEMPT") ? 1 : 0;
            p.shardExemptHubBar = !shardExemptOff;
        }
        // B2: scrollbar-thumb placement fix — the skinned red thumb reuses the
        // scrollbar-bg track's world xfm (the engine caches the bg world and
        // applies it, guarded by `skinned && have`). Opt-out
        // RB3_SCROLLBAR_THUMB_FIX_OFF. `scrollbarBg`/`scrollbarThumb` are mutually
        // exclusive (distinct mesh names). Mirrors the prior `if (mesh->Name())`
        // guard.
        if (nm) {
            static int sbarThumbOff = -1;
            if (sbarThumbOff < 0)
                sbarThumbOff = std::getenv("RB3_SCROLLBAR_THUMB_FIX_OFF") ? 1 : 0;
            if (!sbarThumbOff) {
                if (std::strcmp(nm, "scrollbar_bg.mesh") == 0)
                    p.scrollbarBg = true;
                else if (std::strcmp(nm, "scrollbar.mesh") == 0)
                    p.scrollbarThumb = true;
            }
        }
        // B3: skel-rebake mesh-level gate. rebake enabled (default-on; opt-out
        // RB3_NO_SKEL_REBAKE) AND this is NOT a per-frame-driven dynamic (face /
        // hair / fingernail) outfit mesh. The engine ANDs this with the numBones /
        // rebound / worst-bone-static conditions and keeps all rebake math. Mirrors
        // the prior `dynamicMesh = mn0 && (...)` (a null name is NOT dynamic).
        {
            static int rebakeOff = -1;
            if (rebakeOff < 0)
                rebakeOff = std::getenv("RB3_NO_SKEL_REBAKE") ? 1 : 0;
            bool dynamicMesh = nm &&
                (std::strstr(nm, "facehair") || std::strstr(nm, "goatee") ||
                 std::strstr(nm, "hair") || std::strstr(nm, "bedhead") ||
                 std::strstr(nm, "blownback") || std::strstr(nm, "mohawk") ||
                 std::strstr(nm, "fingernails") || std::strstr(nm, "eyebrow") ||
                 std::strstr(nm, "tongue") || std::strstr(nm, "facial"));
            p.skelRebakeMesh = !rebakeOff && !dynamicMesh;
        }
        return p;
    }

    // B3/B5: STATIC shared band skeleton dir name (skeleton_unshared.milo).
    bool IsBandMemberSkeletonFile(const char* storedFile) override {
        return storedFile &&
               std::strstr(storedFile, "skeleton_unshared.milo") != nullptr;
    }

    // B3: per-frame-driven dynamic-chain bone (hair / facial / finger) excluded
    // from the one-time static rebake.
    bool IsRebakeDynamicBone(const char* bn) override {
        return bn && (std::strstr(bn, "hair") || std::strstr(bn, "-lid") ||
                      std::strstr(bn, "_lid") || std::strstr(bn, "jaw") ||
                      std::strstr(bn, "lip") || std::strstr(bn, "brow") ||
                      std::strstr(bn, "eye") || std::strstr(bn, "mouth") ||
                      std::strstr(bn, "cheek") || std::strstr(bn, "nose") ||
                      std::strstr(bn, "tongue") || std::strstr(bn, "goatee") ||
                      std::strstr(bn, "index") || std::strstr(bn, "middle") ||
                      std::strstr(bn, "pinky") || std::strstr(bn, "ring") ||
                      std::strstr(bn, "thumb") || std::strstr(bn, "finger"));
    }

    DrawMaterialPolicy QueryDrawMaterialPolicy(RndMesh* /*mesh*/,
                                               RndMat* /*mat*/,
                                               bool /*skinned*/,
                                               RndMesh* /*owner*/,
                                               const char* /*camName*/) override {
        return DrawMaterialPolicy();
    }

    // B6: halo-source NAME exclusions (RB3HaloPass::IsHaloSourceMat). The engine
    // keeps the emissive-map/multiplier DATA test; this answers ONLY the name-based
    // exclusion + the RB3_SMASHER_HALO opt-in flag (statically cached once, prior
    // semantics preserved):
    //   - "surface" (full-quad track plane): blooming it washes the whole track ->
    //     always excluded.
    //   - "gem_smasher_glow" (now-bar plate): excluded UNLESS RB3_SMASHER_HALO=1
    //     (opt back in for A/B); default-off avoids the giant-white-sphere blowout.
    HaloPolicy QueryHaloPolicy(RndMat* mat) override {
        HaloPolicy p;
        if (!mat) return p;
        const char* mn = mat->Name();
        if (mn && std::strstr(mn, "surface")) { p.forceExclude = true; return p; }
        if (mn && std::strstr(mn, "gem_smasher_glow")) {
            static int sSmasherHalo = -1;
            if (sSmasherHalo < 0) {
                const char* e = std::getenv("RB3_SMASHER_HALO");
                sSmasherHalo = (e && e[0] && e[0] != '0') ? 1 : 0;
            }
            if (!sSmasherHalo) p.forceExclude = true;
        }
        return p;
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
