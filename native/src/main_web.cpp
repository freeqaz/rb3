// RB3 Web Port — W3a entry point: App-driven boot in the browser.
//
// W2a stood up a static mesh-walk harness (LoadMiloAndWalk + RenderFrame) that
// rendered a single ?milo= named scene. W3a retires that as the DEFAULT path and
// drives the real RB3 `App` instead: construct `sApp = new App(0, nullptr)` once
// the async WebGPU device is ready, then call `sApp->RunOneFrame(frame)` every
// frame — mirroring DC3's native/src/main_web.cpp. That gives the full game
// boot spine (SystemPreInit/SystemInit, factory registration, TheUI poll/draw),
// so the menu renders with RndText glyph quads and (once W3b lands) responds to
// keyboard input.
//
// The W2 mesh-walk harness is KEPT, reachable behind `?milo=<path>` (so the W2
// pixelmatch regression test stays green and there's a fast geometry-only debug
// path). The boot machine forks on `?milo=` at BOOT_ENGINE_INIT:
//
//   ── shared ──
//   BOOT_INIT          → WebAssetsInit + WebAssetsFetchBundle
//   BOOT_FETCHING      → poll WebAssetsAllDone
//   BOOT_ENGINE_INIT   → chdir(/data) + platform=XBox + settings;
//                        HARNESS mode also runs SystemPreInit/SystemInit +
//                        RegisterCommonFactories + PreInitRender here (the App
//                        ctor owns those in APP mode); both arm StartGpuInit
//   BOOT_GPU_WAIT      → gBandRnd.Gpu().PollEvents() + IsReady() (async device)
//   BOOT_GPU_READY     → gBandRnd.InitGpuResources()
//   ── App mode (no ?milo=) ──
//   BOOT_APP_CTOR      → RB3RegisterLegacyRndAliases() + sApp = new App(0,nullptr);
//                        emit window.rb3AppBooted
//   BOOT_RUNNING       → sApp->RunOneFrame(frame); window.rb3FrameCount,
//                        window.rb3CurrentScreen
//   ── Harness mode (?milo=<path>) ──
//   BOOT_LOADING_MILO  → LoadMiloAndWalk(path); window.rb3MilosLoaded
//   BOOT_RUNNING_RENDER→ RenderFrame(walk); window.rb3FrameCount
//
// W3b wires real keyboard input; W3c recovers audio + a full song.

#ifdef __EMSCRIPTEN__

#include <emscripten/emscripten.h>
#include <emscripten/em_asm.h>
#include <emscripten/html5.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "App.h"

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/System.h"
#include "utl/Symbol.h"
#include "utl/Loader.h"

#include "platform/WebAssets.h"
#include "platform/Rnd_Wgpu_RB3.h"  // gBandRnd
#include "rndobj/Cam.h"

#include "ui/UI.h"          // TheUI — current-screen poll for window.rb3CurrentScreen
#include "ui/UIScreen.h"
#include "ui/UIPanel.h"
#include "ui/UIComponent.h"
#include "meta_band/BandSongMgr.h"  // TheSongMgr — song-list population probe (W3c-nav)
#include "meta_band/MusicLibrary.h"   // TheMusicLibrary — highlighted-song probe (W3c)
#include "meta_band/SongSortNode.h"   // SortNode::GetType/GetToken (W3c)
#include <vector>

#include "rb3_render_mesh.h"  // LoadMiloAndWalk / RenderFrame / WalkResult

#include <unistd.h>  // chdir
#include <errno.h>

// rndobj + synth object factories (HARNESS mode only — see RegisterCommonFactories).
#include "obj/Object.h"
#include "obj/ObjMacros.h"
#include "obj/Dir.h"
#include "rndobj/Dir.h"
#include "rndobj/Tex.h"
#include "rndobj/Group.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/PropAnim.h"

// Same global gSystemConfig pointer the engine and matched-fork share.
extern DataArray *gSystemConfig;

// --- Web-only global definitions for excluded-TU symbols ---
// Synth.cpp is excluded from the web build (audio-free W3a; the codec.h alloca
// clash, see W3c). Its `Synth *TheSynth;` global is therefore undefined on web.
// On native a weak `.s` stub (dta_link_stubs.s) supplies it, but those x86 GAS
// stubs are NOT assembled under emcc — so without an explicit definition here
// `-sERROR_ON_UNDEFINED_SYMBOLS=0` resolves TheSynth to a GARBAGE address. The
// App ctor's `if (TheSynth) TheSynth->SetDolby(...)` then sees garbage != 0,
// calls through a junk vtable, and the wasm runtime traps ("table index out of
// bounds"). Defining it null here makes the guard work (same class of fix as
// the TheNetSession global in rb3_netsession_native.cpp). W3c links the real
// Synth and removes this.
class Synth;
Synth *TheSynth = nullptr;

// Live-tunable camera/gem settings — seed once from the (empty) browser env.
#include "rb3_native_settings.h"

// Engine: register the legacy short milo class names (Tex/Text/Dir). The native
// RunGame calls this before any UI milo loads; the App-boot path must too (the
// App ctor's Rnd::PreInit registers the prefixed RndXxx names but not the
// aliases). Safe to call any time before a milo loads.
extern void RB3RegisterLegacyRndAliases();

// ============================================================================
// HARNESS-mode factory registration (W2). Only used when ?milo= is present —
// the App-boot path registers factories via the App ctor (Rnd::PreInit /
// BandInit / UIManager::Init etc). See the W2 writeup for why the synth leaves
// are deliberately omitted from the harness path.
// ============================================================================
static void RegisterCommonFactories() {
    Hmx::Object::Init();
    ObjectDir::Register();
    REGISTER_OBJ_FACTORY(RndDir)
    REGISTER_OBJ_FACTORY(RndTex)
    REGISTER_OBJ_FACTORY(RndGroup)
    REGISTER_OBJ_FACTORY(EventTrigger)
    REGISTER_OBJ_FACTORY(RndPropAnim)
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
    // App-driven boot (default; no ?milo=)
    BOOT_APP_CTOR,
    BOOT_RUNNING,
    // W2 mesh-walk harness (?milo=<path>)
    BOOT_LOADING_MILO,
    BOOT_RUNNING_RENDER,
    BOOT_ERROR,
};

static BootState sBootState = BOOT_INIT;
static int sFrameCount = 0;
static int sGpuWaitFrames = 0;
static const int kGpuWaitTimeout = 600;  // ~10s @ 60fps — Dawn adapter can be slow
static App *sApp = nullptr;
static bool sHarnessMode = false;       // true iff ?milo= present
static std::string sMiloPath;           // harness-mode milo path
static WalkResult sWalk;                 // harness-mode loaded scene
static int sMilosLoaded = 0;

// Render resolution fallbacks; GpuDevice_Web reads the canvas element dims.
static const int kW = 1280;
static const int kH = 720;

// Publish the current UI screen name to the page so the smoke can poll
// screen-flow progress (splash/startup → menu). No-op if TheUI has no screen.
static void PublishCurrentScreen() {
    const char* name = "";
    const char* focus = "";
    UIScreen* scr = TheUI.CurrentScreen();
    if (scr && scr->Name())
        name = scr->Name();
    // The currently-focused UI component name (the button a Confirm acts on) —
    // lets the W3c-nav smoke verify the focus chain (mb_playnow → pn_quickplay
    // → qp_quickplay) as it drives the menu.
    if (scr && scr->FocusPanel() && scr->FocusPanel()->FocusComponent() &&
        scr->FocusPanel()->FocusComponent()->Name())
        focus = scr->FocusPanel()->FocusComponent()->Name();
    EM_ASM({
        window.rb3CurrentScreen = UTF8ToString($0);
        window.rb3FocusButton  = UTF8ToString($1);
    }, name, focus);
}

// Publish the discovered song count so the smoke can confirm the song DB is
// populated on web (W3c-nav: NativeContentMgr::StartRefresh reads
// /data/songs/songs.dta). GetRankedSongs returns the menu-visible set.
static void PublishSongCount() {
    int n = 0;
    std::vector<int> ranked;
    TheSongMgr.GetRankedSongs(ranked, false, false);
    n = (int)ranked.size();
    EM_ASM_({ window.rb3SongCount = $0; }, n);
}

// W3c Part B: publish the currently-highlighted song-list node so the gameplay
// smoke can navigate the music library deterministically to a *specific* song
// (e.g. 20th Century Boy) instead of guessing key-press offsets. On a song row
// GetType()==kNodeSong and GetToken() is the song shortname Symbol (the value
// the music_library select_highlighted_node handler resolves); on a header/
// function row we publish the token too (so the smoke can tell a non-song row
// apart — confirming a header node null-derefs OwnedSongSortNode and traps).
// window.rb3HighlightedSong = "<token>", window.rb3HighlightedType = <int type>.
static void PublishHighlightedSong() {
    const char *token = "";
    int type = -1;
    if (TheMusicLibrary) {
        SortNode *node = TheMusicLibrary->GetHighlightedNode();
        if (node) {
            type = (int)node->GetType();
            Symbol t = node->GetToken();
            if (t.Str())
                token = t.Str();
        }
    }
    EM_ASM_({
        window.rb3HighlightedSong = UTF8ToString($0);
        window.rb3HighlightedType = $1;
    }, token, type);
}

static void DoEngineInit() {
    // The MEMFS bundle unpacks files at /data/<rel>/..., mirroring the on-disc
    // layout. chdir so the matched-fork's relative paths resolve like rb3-native.
    if (chdir("/data") != 0) {
        printf("RB3 Web: chdir('/data') failed (errno=%d)\n", errno);
        sBootState = BOOT_ERROR;
        return;
    }
    printf("RB3 Web: cwd=/data\n");

    // 360-ARK assets are big-endian Xbox (same as rb3-native RB3_BOOT/RB3_GAME).
    TheLoadMgr.mPlatform = kPlatformXBox;

    // Seed live-tunable settings (camera/gem) from env (none in browser).
    TheNativeSettings().InitFromEnv();

    if (sHarnessMode) {
        // ── HARNESS mode (W2): the static mesh-walk path owns engine init.
        printf("RB3 Web: HARNESS mode (?milo=%s)\n", sMiloPath.c_str());
        printf("RB3 Web: SystemPreInit('config/band_preinit_keep.dta')...\n");
        SystemPreInit("config/band_preinit_keep.dta");
        printf("RB3 Web: SystemInit('config/band_keep.dta')...\n");
        SystemInit("config/band_keep.dta");
        RegisterCommonFactories();
        gBandRnd.PreInitRender();
        gBandRnd.SetClearColor(Hmx::Color(0.12f, 0.14f, 0.18f));
    } else {
        // ── App mode (default): the App ctor owns SystemPreInit/SystemInit and
        // factory registration. Don't run them here. Just arm the GPU; App is
        // constructed in BOOT_APP_CTOR after the device is ready.
        printf("RB3 Web: APP mode (no ?milo=) — App ctor drives engine init\n");
        // Black clear (the App ctor's HX_NATIVE arm also sets black via
        // TheRnd->SetClearColor; set it here so the canvas isn't a default color
        // before the first App draw).
        gBandRnd.SetClearColor(Hmx::Color(0, 0, 0));
    }

    // Phase 1 of the two-phase GPU bring-up (async on web). Poll IsReady() in
    // BOOT_GPU_WAIT. Canvas selector is baked via MILO_WEB_CANVAS_SELECTOR.
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
        // Signal to the loading screen that asset fetch has started.
        EM_ASM({ window.rb3AssetsLoaded = 0; window.rb3AssetsTotal = 0; });
        sBootState = BOOT_FETCHING;
        break;
    }

    case BOOT_FETCHING: {
        // Publish incremental progress for the loading bar on every poll tick.
        {
            int loaded = WebAssetsCompletedCount();
            int failed = WebAssetsFailedCount();
            EM_ASM_({ window.rb3AssetsLoaded = $0 + $1; }, loaded, failed);
        }
        if (!WebAssetsAllDone()) break;
        int ok = WebAssetsCompletedCount();
        int fail = WebAssetsFailedCount();
        printf("RB3 Web: assets ready (%d files, %d errors)\n", ok, fail);
        if (fail > 0)
            printf("RB3 Web: WARNING — %d asset fetch errors; continuing\n", fail);
        // Signal total so the loading bar can show 100% for a moment.
        EM_ASM_({ window.rb3AssetsTotal = $0; }, ok + fail);
        // Decide the boot mode now (before engine init): ?milo= → W2 harness,
        // else → App-driven boot.
        sMiloPath = GetMiloPathFromUrl();
        sHarnessMode = !sMiloPath.empty();
        sBootState = BOOT_ENGINE_INIT;
        break;
    }

    case BOOT_ENGINE_INIT: {
        // Wrap heavy init in try/catch so a thrown exception settles into
        // BOOT_ERROR (one console line) instead of aborting the wasm runtime.
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
        // GpuDevice's RequestAdapter/RequestDevice are async on web; poll until
        // the JS callbacks fire (usually within a few RAF ticks).
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
        // Phase 2: pipelines / depth / rings / default textures. After this the
        // GpuDevice is fully usable, so the App ctor's Rnd::PreInit (which
        // creates default rndobj objects) and any milo load can proceed.
        printf("RB3 Web: GPU ready — initializing resources...\n");
        gBandRnd.InitGpuResources();
        sBootState = sHarnessMode ? BOOT_LOADING_MILO : BOOT_APP_CTOR;
        break;
    }

    // ── App-driven boot ──────────────────────────────────────────────────────
    case BOOT_APP_CTOR: {
        // Register the legacy Tex/Text/Dir aliases BEFORE the App ctor loads any
        // UI/font milo (mirrors native RunGame). The App ctor then runs the full
        // boot spine (SystemPreInit/SystemInit, BandInit, TheUI.Init, ...). The
        // GPU is already up, so Rnd::Init/CreateDefaults see a ready device.
        printf("RB3 Web: registering legacy rnd aliases + constructing App...\n");
        try {
            RB3RegisterLegacyRndAliases();
            sApp = new App(0, nullptr);
        } catch (...) {
            printf("RB3 Web: boot error — exception during App construction\n");
            sBootState = BOOT_ERROR;
            break;
        }
        printf("RB3 Web: App constructed — entering RunOneFrame loop\n");
        EM_ASM({ window.rb3AppBooted = 1; });
        sBootState = BOOT_RUNNING;
        break;
    }

    case BOOT_RUNNING: {
        try {
            sApp->RunOneFrame(sFrameCount);
        } catch (...) {
            printf("RB3 Web: boot error — exception during RunOneFrame\n");
            sBootState = BOOT_ERROR;
            break;
        }
        sFrameCount++;
        EM_ASM_({ window.rb3FrameCount = $0; }, sFrameCount);
        // Publish the current screen name periodically (cheap; every frame is
        // fine but throttle the EM_ASM to keep the JS bridge light).
        if ((sFrameCount & 7) == 0) {
            PublishCurrentScreen();
            PublishSongCount();
            PublishHighlightedSong();
        }
        break;
    }

    // ── W2 mesh-walk harness (?milo=) ─────────────────────────────────────────
    case BOOT_LOADING_MILO: {
        std::string path = (sMiloPath[0] == '/') ? sMiloPath : ("/data/" + sMiloPath);
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
        try {
            if (sWalk.ok) {
                RenderFrame(sWalk);
            } else {
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
    setbuf(stdout, nullptr);
    setbuf(stderr, nullptr);

    printf("RB3 Web Port — W3a App-driven boot initializing\n");

#ifdef MILO_WEB_ASYNCIFY
    // JSPI mode: JS drives the loop via requestAnimationFrame + await so
    // synchronous file loads (and PollUntilLoaded's emscripten_sleep yields) can
    // suspend/resume without freezing the browser. dc3-web uses the same pattern.
    EM_ASM({
        async function tick() {
            await Module._rb3MainLoopTick();
            requestAnimationFrame(tick);
        }
        requestAnimationFrame(tick);
    });
    emscripten_exit_with_live_runtime();
#else
    emscripten_set_main_loop(mainLoop, 0, /*simulate_infinite_loop=*/0);
#endif
    return EXIT_SUCCESS;
}

#endif  // __EMSCRIPTEN__
