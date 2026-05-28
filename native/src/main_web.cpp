// RB3 Web Port — W1 (clear-frame) entry point.
//
// Mirrors rb3-native's RB3_BOOT=1 path (main_native.cpp:447-497) but driven by
// the browser main loop instead of a single CLI invocation. W1's goal is the
// smallest end-to-end proof the wasm pipeline runs: download the boot DTA
// bundle, run SystemPreInit + SystemInit, stand up BandRnd's GpuDevice via the
// WebGPU canvas surface, and emit a per-frame clear color so a Playwright
// screenshot can verify it differs from the page background.
//
// Boot state machine (mirrors dc3 main_web.cpp, simplified):
//   BOOT_INIT        → WebAssetsInit + WebAssetsFetchBundle
//   BOOT_FETCHING    → poll WebAssetsAllDone
//   BOOT_ENGINE_INIT → SystemPreInit/SystemInit + register factories +
//                      gBandRnd.InitGpu(W, H, /*headless=*/false)
//   BOOT_GPU_WAIT    → gBandRnd.Gpu().IsReady() (async WebGPU adapter/device)
//   BOOT_GPU_READY   → one BeginFrame/EndFrame to commit the initial clear
//   BOOT_RUNNING     → per-frame BeginFrame/EndFrame;
//                      window.rb3FrameCount = N for Playwright readiness
//
// W2 swaps the trivial clear loop for a real .milo render; W3 wires input +
// audio + the full RB3_GAME App flow. W1 deliberately does NOT touch the App.

#ifdef __EMSCRIPTEN__

#include <emscripten/emscripten.h>
#include <emscripten/em_asm.h>
#include <emscripten/html5.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/System.h"
#include "utl/Symbol.h"
#include "utl/Loader.h"

#include "platform/WebAssets.h"
#include "gfx/GpuDevice.h"

#include <webgpu/webgpu_cpp.h>
#include <unistd.h>  // chdir

// rndobj + synth object factories so DirLoader can construct the live object
// graph (SystemInit only registers obj-level factories; mirror RunBoot's
// RegisterCommonFactories from main_native.cpp).
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

// Mirrors main_native.cpp's RegisterCommonFactories — without these the milo
// loader silently WARNs on unknown classes. W1 doesn't load a milo, but boot
// DTA wiring sometimes constructs objects up front; registering early is cheap.
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

// ============================================================================
// Boot state machine
// ============================================================================

enum BootState {
    BOOT_INIT,
    BOOT_FETCHING,
    BOOT_ENGINE_INIT,
    BOOT_GPU_WAIT,
    BOOT_GPU_READY,
    BOOT_RUNNING,
    BOOT_ERROR,
};

static BootState sBootState = BOOT_INIT;
static int sFrameCount = 0;
static int sGpuWaitFrames = 0;
static const int kGpuWaitTimeout = 600;  // ~10s @ 60fps — Dawn adapter can be slow

// W1 dedicates a private GpuDevice + clear color instead of going through
// BandRnd::InitGpu — the latter does device + pipeline-manager + ring-buffer +
// default-texture setup in a SINGLE synchronous call that aborts on web
// because mGpu.IsReady() is false until the async WebGPU adapter callback
// fires. Splitting BandRnd's monolithic init across BOOT_GPU_WAIT is W2 work
// (when we need the full backend for milo rendering); for W1 the device + a
// clear-only render pass is enough to prove the toolchain.
static GpuDevice sWebGpu;
static const wgpu::Color kClearColor = {0.2, 0.4, 0.7, 1.0};  // matches smoke

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

    // Stand up the GpuDevice directly. On web, Init() returns true immediately
    // after starting the async adapter/device request; we poll IsReady() from
    // BOOT_GPU_WAIT until the JS callbacks fire. The canvas selector is baked
    // at compile time via MILO_WEB_CANVAS_SELECTOR (= "#rb3-canvas", set in
    // CMakeLists via milo_engine_set_web_canvas_selector). Width / height
    // passed here are fallbacks only — GpuDevice_Web reads the canvas element's
    // width/height attributes for the real dims.
    GpuDeviceDesc desc{};
    desc.headless = false;
    desc.width = 1280;
    desc.height = 720;
    desc.title = "rb3-web W1";
    printf("RB3 Web: GpuDevice.Init (async)\n");
    if (!sWebGpu.Init(desc)) {
        printf("RB3 Web: GpuDevice.Init FAILED (sync error before async dispatch)\n");
        sBootState = BOOT_ERROR;
        return;
    }
    printf("RB3 Web: GpuDevice.Init returned — waiting for async GPU adapter/device\n");
}

// Single-pass clear: acquire surface frame, BeginRenderPass with kClearColor,
// End + Submit. The browser auto-composites on requestAnimationFrame so no
// explicit Present is needed.
static void DrawClearFrame() {
    wgpu::TextureView view = sWebGpu.AcquireNextFrame();
    if (!view) {
        // Surface not yet configured / temporarily unavailable — skip this frame.
        return;
    }
    wgpu::CommandEncoder enc = sWebGpu.Device().CreateCommandEncoder();
    wgpu::RenderPassColorAttachment colorAtt{};
    colorAtt.view = view;
    colorAtt.loadOp = wgpu::LoadOp::Clear;
    colorAtt.storeOp = wgpu::StoreOp::Store;
    colorAtt.clearValue = kClearColor;
    wgpu::RenderPassDescriptor rp{};
    rp.label = "rb3-web W1 clear";
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &colorAtt;
    wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
    pass.End();
    wgpu::CommandBuffer cmd = enc.Finish();
    sWebGpu.Queue().Submit(1, &cmd);
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
        // few RAF ticks of returning from Init. Poll until IsReady().
        sGpuWaitFrames++;
        if (sWebGpu.IsReady()) {
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
        // GpuDevice's surface is configured inside the async device callback
        // (GpuDevice_Web.cpp:132 ConfigureSurface), so by the time IsReady()
        // returns true the surface is ready to AcquireNextFrame on.
        printf("RB3 Web: BandRnd ready — entering frame loop\n");
        sBootState = BOOT_RUNNING;
        // Fall through to draw frame 0 right away.
        [[fallthrough]];
    }

    case BOOT_RUNNING: {
        // Smallest possible draw: clear pass against the canvas surface.
        DrawClearFrame();
        sFrameCount++;
        // Surface to Playwright via window.rb3FrameCount so the smoke test can
        // verify the frame loop is actually running (≥5 frames is the threshold).
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
    if (sWebGpu.IsReady() && w > 0 && h > 0) {
        sWebGpu.ResizeSurface(w, h);
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

    printf("RB3 Web Port — W1 clear-frame harness initializing\n");

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
    // Cooperative main loop fallback (file loads block — fine for W1's bundle).
    emscripten_set_main_loop(mainLoop, 0, /*simulate_infinite_loop=*/0);
#endif
    return EXIT_SUCCESS;
}

#endif  // __EMSCRIPTEN__
