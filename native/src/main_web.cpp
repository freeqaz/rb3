// RB3 Web Port — W2a entry point: drive gBandRnd and render a real .milo.
//
// Mirrors rb3-native's RB3_RENDER_MESH path (rb3_render_mesh.cpp) but driven by
// the browser main loop instead of a single CLI invocation. W1 stood up a
// file-static GpuDevice + a clear pass to prove the wasm/WebGPU pipeline; W2a
// retires that and drives the real BandRnd backend (gBandRnd), loads the milo
// named by the ?milo= URL param, and walks/draws its meshes every frame.
//
// Boot state machine:
//   BOOT_INIT          → WebAssetsInit + WebAssetsFetchBundle
//   BOOT_FETCHING      → poll WebAssetsAllDone
//   BOOT_ENGINE_INIT   → SystemPreInit/SystemInit + register factories +
//                        gBandRnd.PreInitRender + SetClearColor +
//                        gBandRnd.StartGpuInit(W, H, /*headless=*/false)
//   BOOT_GPU_WAIT      → gBandRnd.Gpu().PollEvents() + IsReady() (async device)
//   BOOT_GPU_READY     → gBandRnd.InitGpuResources()
//   BOOT_LOADING_MILO  → parse ?milo=, LoadMiloAndWalk(path); emit rb3MilosLoaded
//   BOOT_RUNNING_RENDER→ per-frame RenderFrame(walk); emit rb3FrameCount
//
// W2b adds full-coverage libc++ shims + pixelmatch; W3 wires input + audio +
// the full RB3_GAME App flow.

#ifdef __EMSCRIPTEN__

#include <emscripten/emscripten.h>
#include <emscripten/em_asm.h>
#include <emscripten/html5.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/System.h"
#include "utl/Symbol.h"
#include "utl/Loader.h"

#include "platform/WebAssets.h"
#include "platform/Rnd_Wgpu_RB3.h"  // gBandRnd
#include "rndobj/Cam.h"

#include "rb3_render_mesh.h"  // LoadMiloAndWalk / RenderFrame / WalkResult

#include <unistd.h>  // chdir

// rndobj + synth object factories so DirLoader can construct the live object
// graph (SystemInit only registers obj-level factories; mirror RunBoot's
// RegisterCommonFactories from main_native.cpp). gBandRnd.PreInitRender()
// registers the rndobj ones; these add the obj/synth ones the milo graph needs.
#include "obj/Object.h"
#include "obj/ObjMacros.h"
#include "obj/Dir.h"
#include "rndobj/Dir.h"
#include "rndobj/Tex.h"
#include "rndobj/Group.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/PropAnim.h"
#include "synth/Sfx.h"
#include "synth/SynthSample.h"
#include "synth/Sequence.h"
#include "synth/MidiInstrument.h"

// Same global gSystemConfig pointer the engine and matched-fork share.
extern DataArray *gSystemConfig;

// rb3_native_settings provides the live-tunable camera/gem settings — seed once
// from the (empty) browser env so any subsystem that reads from
// TheNativeSettings() sees a populated struct.
#include "rb3_native_settings.h"

// Mirrors main_native.cpp's RegisterCommonFactories — the synth/obj factories
// the milo dependency graph references. gBandRnd.PreInitRender() registers the
// rndobj ones (Trans/Cam/Mesh/Env/Mat/Tex/Light/MultiMesh/Group/Dir + aliases);
// these cover the obj/synth side. Duplicate registration is harmless (overwrite).
static void RegisterCommonFactories() {
    Hmx::Object::Init();
    ObjectDir::Register();
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

// Read the ?milo= URL query param into a std::string (empty if unset).
static std::string GetMiloPathFromUrl() {
    char* s = (char*)EM_ASM_PTR({
        const params = new URLSearchParams(window.location.search);
        const p = params.get('milo') || '';
        const len = lengthBytesUTF8(p) + 1;
        const buf = _malloc(len);
        stringToUTF8(p, buf, len);
        return buf;
    });
    std::string out(s ? s : "");
    free(s);
    return out;
}

// ============================================================================
// Boot state machine
// ============================================================================

enum BootState {
    BOOT_INIT,
    BOOT_FETCHING,
    BOOT_ENGINE_INIT,
    BOOT_GPU_WAIT,
    BOOT_GPU_READY,
    BOOT_LOADING_MILO,
    BOOT_RUNNING_RENDER,
    BOOT_ERROR,
};

static BootState sBootState = BOOT_INIT;
static int sFrameCount = 0;
static int sGpuWaitFrames = 0;
static const int kGpuWaitTimeout = 600;  // ~10s @ 60fps — Dawn adapter can be slow
static WalkResult sWalk;
static int sMilosLoaded = 0;

// Render resolution — fallbacks; GpuDevice_Web reads the canvas element's
// width/height for the real dims.
static const int kW = 1280;
static const int kH = 720;

static void DoEngineInit() {
    // The MEMFS bundle from the dev server unpacks files at /data/<rel>/...,
    // exactly mirroring the on-disc layout (config/, ui/, world/, ...). chdir
    // so the matched-fork's relative paths (`config/...`, ui DTA #include) all
    // resolve like they do in rb3-native's RB3_BOOT mode.
    if (chdir("/data") != 0) {
        printf("RB3 Web: chdir('/data') failed (errno=%d)\n", errno);
        sBootState = BOOT_ERROR;
        return;
    }
    printf("RB3 Web: cwd=/data\n");

    // The matched-fork load path needs platform=XBox: assets in orig-assets/
    // are 360-ARK big-endian (same as rb3-native RB3_BOOT).
    TheLoadMgr.mPlatform = kPlatformXBox;

    // Seed the live-tunable settings (camera/gem) from env (none in browser;
    // this just zeroes the struct + applies defaults).
    TheNativeSettings().InitFromEnv();

    printf("RB3 Web: SystemPreInit('config/band_preinit_keep.dta')...\n");
    SystemPreInit("config/band_preinit_keep.dta");
    printf("RB3 Web: SystemPreInit complete (gSystemConfig=%p)\n",
           (void *)gSystemConfig);

    printf("RB3 Web: SystemInit('config/band_keep.dta')...\n");
    SystemInit("config/band_keep.dta");
    printf("RB3 Web: SystemInit complete\n");

    // Register obj/rndobj/synth factories now that the engine subsystems are up
    // (SystemInit only registers the obj-level factories via ObjectDir::Init).
    RegisterCommonFactories();

    // Register the rndobj factories the milo loader needs (Trans/Cam/Mesh/Env/
    // Mat/Tex/Light/MultiMesh/Group/Dir + the legacy Tex/Text/Dir aliases).
    gBandRnd.PreInitRender();

    // Match the native RB3_RENDER_MESH clear color so the canvas isn't black.
    gBandRnd.SetClearColor(Hmx::Color(0.12f, 0.14f, 0.18f));

    // Phase 1 of the two-phase GPU bring-up. On web Init() returns true
    // immediately after dispatching the async adapter/device request; we poll
    // gBandRnd.Gpu().IsReady() from BOOT_GPU_WAIT until the JS callbacks fire.
    // The canvas selector is baked at compile time via MILO_WEB_CANVAS_SELECTOR
    // (= "#rb3-canvas"). Width/height here are fallbacks only.
    printf("RB3 Web: gBandRnd.StartGpuInit (async)\n");
    if (!gBandRnd.StartGpuInit(kW, kH, /*headless=*/false)) {
        printf("RB3 Web: StartGpuInit FAILED (sync error before async dispatch)\n");
        sBootState = BOOT_ERROR;
        return;
    }
    printf("RB3 Web: StartGpuInit returned — waiting for async GPU adapter/device\n");
}

static void mainLoop() {
    switch (sBootState) {
    case BOOT_INIT: {
        printf("RB3 Web: downloading assets (bundle)...\n");
        WebAssetsInit();
        WebAssetsFetchBundle();
        sBootState = BOOT_FETCHING;
        break;
    }

    case BOOT_FETCHING: {
        if (!WebAssetsAllDone()) break;
        int ok = WebAssetsCompletedCount();
        int fail = WebAssetsFailedCount();
        printf("RB3 Web: assets ready (%d files, %d errors)\n", ok, fail);
        if (fail > 0) {
            printf("RB3 Web: WARNING — %d asset fetch errors; continuing\n", fail);
        }
        sBootState = BOOT_ENGINE_INIT;
        break;
    }

    case BOOT_ENGINE_INIT: {
        // Wrap the heavy init in a try/catch so a thrown exception inside the
        // matched-fork settles into BOOT_ERROR (visible as a single console
        // line) rather than aborting the wasm runtime mid-frame.
        try {
            DoEngineInit();
        } catch (...) {
            printf("RB3 Web: boot error — exception during engine init\n");
            sBootState = BOOT_ERROR;
            break;
        }
        if (sBootState != BOOT_ERROR) {
            sBootState = BOOT_GPU_WAIT;
            printf("RB3 Web: waiting for GPU...\n");
        }
        break;
    }

    case BOOT_GPU_WAIT: {
        // GpuDevice::Init's RequestAdapter / RequestDevice are async on web; the
        // device only shows up once the JS callbacks fire — usually within a
        // few RAF ticks of returning from StartGpuInit. Poll until IsReady().
        sGpuWaitFrames++;
        gBandRnd.Gpu().PollEvents();
        if (gBandRnd.Gpu().IsReady()) {
            printf("RB3 Web: GPU ready (after %d frames)\n", sGpuWaitFrames);
            sBootState = BOOT_GPU_READY;
            break;
        }
        if (sGpuWaitFrames >= kGpuWaitTimeout) {
            printf("RB3 Web: GPU not ready after %d frames — giving up\n", sGpuWaitFrames);
            sBootState = BOOT_ERROR;
        }
        break;
    }

    case BOOT_GPU_READY: {
        // Device + surface ready (the surface format was queried in the async
        // device callback). Phase 2: create pipelines / depth / rings / default
        // textures. mTargetFmt is latched to the surface format here.
        printf("RB3 Web: GPU ready — initializing resources...\n");
        gBandRnd.InitGpuResources();
        sBootState = BOOT_LOADING_MILO;
        break;
    }

    case BOOT_LOADING_MILO: {
        std::string milo = GetMiloPathFromUrl();
        if (milo.empty()) {
            // No ?milo= specified: nothing to render. Fall through to the render
            // loop anyway so the canvas shows the clear color and the frame
            // counter advances (W1-parity behavior).
            printf("RB3 Web: no ?milo= param — clear-only render loop\n");
            sBootState = BOOT_RUNNING_RENDER;
            break;
        }
        // Anchor a relative path under /data (the bundle/MEMFS root). The
        // on-demand fetch hook in native_file.cpp pulls each milo dependency
        // (sibling _meshes.milo_xbox + textures) lazily as DirLoader walks it.
        std::string path = (milo[0] == '/') ? milo : ("/data/" + milo);
        printf("RB3 Web: loading milo '%s'\n", path.c_str());
        try {
            sWalk = LoadMiloAndWalk(path.c_str());
        } catch (...) {
            printf("RB3 Web: boot error — exception during milo load\n");
            sBootState = BOOT_ERROR;
            break;
        }
        if (sWalk.ok) {
            sMilosLoaded++;
            printf("RB3 Web: milo loaded (%d meshes, %d cams) — entering render loop\n",
                   sWalk.meshCount, sWalk.camCount);
        } else {
            printf("RB3 Web: milo load produced no drawable geometry — clear-only loop\n");
        }
        EM_ASM_({ window.rb3MilosLoaded = $0; }, sMilosLoaded);
        sBootState = BOOT_RUNNING_RENDER;
        break;
    }

    case BOOT_RUNNING_RENDER: {
        // Per-frame: draw the loaded scene (or just clear if nothing loaded).
        // No Present/Swap — under WebGPU the surface auto-composites when the
        // requestAnimationFrame callback returns.
        try {
            if (sWalk.ok) {
                RenderFrame(sWalk);
            } else {
                // Clear-only frame: BeginFrame/EndFrame with the loaded cam null.
                gBandRnd.BeginFrame(sWalk.cam);
                gBandRnd.EndFrame();
            }
        } catch (...) {
            printf("RB3 Web: boot error — exception during render frame\n");
            sBootState = BOOT_ERROR;
            break;
        }
        sFrameCount++;
        EM_ASM_({ window.rb3FrameCount = $0; }, sFrameCount);
        break;
    }

    case BOOT_ERROR:
        // One-shot diagnostic; further ticks no-op.
        break;
    }
}

// ============================================================================
// Exported C API — JS calls these directly via Module._rb3MainLoopTick.
// ============================================================================

extern "C" {

EMSCRIPTEN_KEEPALIVE
void rb3MainLoopTick() {
    mainLoop();
}

EMSCRIPTEN_KEEPALIVE
void rb3_resize_canvas(int w, int h) {
    if (gBandRnd.Gpu().IsReady() && w > 0 && h > 0) {
        gBandRnd.Gpu().ResizeSurface(w, h);
    }
}

}  // extern "C"

// ============================================================================
// Entry point
// ============================================================================

int main(int argc, char **argv) {
    // Unbuffer stdout/stderr so printf is immediate in the browser console.
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);

    printf("RB3 Web Port — W2a milo-render harness initializing\n");

#ifdef MILO_WEB_ASYNCIFY
    // JSPI mode: JS drives the loop via requestAnimationFrame + await so
    // synchronous file loads (WebAssetsFetchSync) can yield without freezing
    // the browser. dc3-web uses the same pattern.
    EM_ASM({
        async function tick() {
            await Module._rb3MainLoopTick();
            requestAnimationFrame(tick);
        }
        requestAnimationFrame(tick);
    });
    emscripten_exit_with_live_runtime();
#else
    // Cooperative main loop fallback (file loads block — fine for the bundle).
    emscripten_set_main_loop(mainLoop, 0, /*simulate_infinite_loop=*/0);
#endif
    return EXIT_SUCCESS;
}

#endif  // __EMSCRIPTEN__
