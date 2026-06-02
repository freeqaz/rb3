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
#include "meta_band/BandUI.h"         // TheBandUI — overshell-slot view probe (pure-kbd nav)
#include "meta_band/OvershellPanel.h"
#include "meta_band/OvershellSlot.h"
#include "game/BandUser.h"
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

// NOTE: `Synth *TheSynth;` is defined by src/system/synth/Synth.cpp:58, which IS
// compiled into rb3-web since W3c re-included Synth.cpp (see RB3_WEB_NATIVE_FORK_EXCLUDE
// in native/CMakeLists.txt). The W3a-era duplicate definition that used to live here
// was removed — it only survived link via -Wl,--allow-multiple-definition and was a
// first-definition-wins footgun (one link-order change from binding TheSynth reads to
// a stale nullptr). The real global's static-init runs before the App ctor's guarded use.

// Live-tunable camera/gem settings — seed once from the (empty) browser env.
#include "rb3_native_settings.h"

// Engine: register the legacy short milo class names (Tex/Text/Dir). The native
// RunGame calls this before any UI milo loads; the App-boot path must too (the
// App ctor's Rnd::PreInit registers the prefixed RndXxx names but not the
// aliases). Safe to call any time before a milo loads.
extern void RB3RegisterLegacyRndAliases();

// B3 (web persistence): ProfileMgr global-options + profile-0 GameplayOptions
// persistence. The bodies are in native/src/rb3_save_native.cpp (now compiled
// into rb3-web alongside the IDB backend in rb3_save_web.cpp); these are just
// the externs we call from the boot machine. RB3InstallWebPersistBackend slots
// the IndexedDB-backed IPersistBackend behind the C2 interface BEFORE the App
// ctor, so MetaPanel's in-ctor RB3SaveLoadGlobalOptions re-apply reads from IDB.
extern void RB3InstallWebPersistBackend();
extern void RB3SaveLoadGlobalOptions();
extern void RB3SaveSaveGlobalOptions();
// Live A/V-calibration probe/poke for the headless round-trip test (B3 VERIFY).
// Read-only published value (window.rb3ExcessVideoLag) + a polled set command
// (window.rb3SetExcessVideoLag) — same JS-bridge style as window.rb3CurrentScreen,
// so the test never needs an EXPORTED_FUNCTIONS change.
#include "meta_band/ProfileMgr.h"
// W3b-splash: the thread-safe verb-inject bridge defined by
// native/src/rb3_game_input.cpp. Enqueues a raw verb string ("start" /
// "confirm" / ...) drained next frame by RB3GameInputPoll -> ExecVerb ->
// ExecButton -> TheUI.Handle(ButtonDownMsg(...)) — the SAME path /api/input
// uses (proven on native: one `start` advances splash_state, one `confirm`
// crosses to main_hub_screen). We only declare it here (no include of the
// concurrently-owned rb3_game_input.cpp) and only CALL it from the web-only
// splash-advance hook below.
extern void RB3GameInputInjectVerb(const std::string &verb);

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
    // Pure-keyboard nav diagnosis: publish the first local overshell slot's
    // current view symbol + track + difficulty (web mirror of native's
    // {rb3_overshell} probe), so the headless web harness can watch the
    // part/difficulty sub-flow (choose_part_guitar -> choose_diff ->
    // ready_to_play -> hidden) advance under real keypresses.
    const char* ovView = "none";
    const char* ovTrack = "?";
    const char* ovDiff = "?";
    OvershellPanel* ovp = TheBandUI.GetOvershell();
    if (ovp) {
        for (int i = 0; i < 4; i++) {
            OvershellSlot* s = ovp->GetSlot(i);
            if (s && s->GetUser() && s->GetUser()->IsLocal()) {
                Symbol v = s->GetCurrentView();
                ovView = v.Str() ? v.Str() : "?";
                ovTrack = s->GetUser()->GetTrackSym().Str();
                ovDiff = s->GetUser()->GetDifficultySym().Str();
                break;
            }
        }
    } else {
        ovView = "no_overshell";
    }
    EM_ASM({
        window.rb3CurrentScreen = UTF8ToString($0);
        window.rb3FocusButton  = UTF8ToString($1);
        window.rb3OvershellView = UTF8ToString($2);
        window.rb3OvershellTrack = UTF8ToString($3);
        window.rb3OvershellDiff = UTF8ToString($4);
    }, name, focus, ovView, ovTrack ? ovTrack : "?", ovDiff ? ovDiff : "?");
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

#ifdef HX_WEB
// W3b-splash: route the splash menu's Start/Confirm keys through the proven
// direct-injection verb path, web-only.
//
// WHY: on web, live keyboard presses flow through the REAL joypad path
// (rb3_joypad_native.cpp JoypadPoll -> SendButtonMessages -> gJoypadMsgSource
// -> TheUI). rb3_game_input.cpp's per-frame edge-detect loop therefore
// deliberately does NOT raw-inject menu keys (its "DOUBLE-FIRE GUARD") — it
// lets JoypadPoll own them. But that JoypadPoll path does not advance the
// boot splash_screen (the splash's overshell add-user / WaitOvershell gate
// never fires from a bare SendButtonMessages there), so the page stalls on
// 'splash_screen'. The HTTP /api/input path — RB3GameInputInjectVerb("start"
// /"confirm") -> ExecVerb -> ExecButton -> TheUI.Handle(ButtonDownMsg) —
// DOES advance it (proven on native: `start` then `confirm` -> main_hub).
//
// So, only while on splash_screen, edge-detect Start(bit 11 = kPad_Start) and
// Confirm(bit 6 = kPad_X) on window._rb3Keys and inject the matching verb.
// Scoping to splash_screen keeps us off every other screen (where JoypadPoll
// works), so we never double-fire menu nav. The verb is enqueued thread-safe
// and drained by the NEXT frame's RB3GameInputPoll — the exact same queue the
// HTTP bridge uses.
static void WebSplashAdvanceHook() {
    // Only act on the boot splash; PublishCurrentScreen already mirrors
    // TheUI.CurrentScreen()->Name() into window.rb3CurrentScreen, but read the
    // engine directly here (authoritative, no JS round-trip lag).
    UIScreen *scr = TheUI.CurrentScreen();
    const char *name = (scr && scr->Name()) ? scr->Name() : "";
    bool onSplash = (std::strcmp(name, "splash_screen") == 0);

    // _rb3Keys is a bitmask whose bits ARE JoypadButton values (see
    // rb3_joypad_native.cpp / rb3_game_input.cpp InitWebInput). Read it once.
    unsigned int keys = (unsigned int)EM_ASM_INT({ return window._rb3Keys || 0; });

    // Per-bit rising-edge latch (own prev-mask, independent of the joypad/input
    // layers' own latches so we don't perturb them).
    static unsigned int sPrevKeys = 0;
    unsigned int pressed = keys & ~sPrevKeys;
    sPrevKeys = keys;

    if (!onSplash)
        return;

    // Diagnostic A/B: window.rb3NoSplashHook=1 disables this verb-inject aid so a
    // harness can test whether the RAW keyboard path (JoypadPoll ->
    // SendButtonMessages) crosses splash on its own.
    static int sNoHook = -1;
    if (sNoHook < 0) sNoHook = (int)EM_ASM_INT({ return (window.rb3NoSplashHook ? 1 : 0); });
    if (sNoHook)
        return;

    const unsigned int kStartBit   = 1u << 11;  // kPad_Start
    const unsigned int kConfirmBit  = 1u << 6;   // kPad_X (Confirm)
    if (pressed & kStartBit) {
        printf("RB3 Web splash: Start edge -> inject verb 'start'\n");
        RB3GameInputInjectVerb("start");
    }
    if (pressed & kConfirmBit) {
        printf("RB3 Web splash: Confirm edge -> inject verb 'confirm'\n");
        RB3GameInputInjectVerb("confirm");
    }
}
#endif  // HX_WEB

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
            // B3: slot the IndexedDB-backed persist backend BEFORE the App ctor,
            // so gPersist is the web backend before MetaPanel's in-ctor
            // RB3SaveLoadGlobalOptions re-apply (MetaPanel.cpp:332) fires its
            // first Read. The save-cache prewarm (rb3_pre.js) is tiny and is
            // already ready by now, so that Read returns the persisted blob.
            RB3InstallWebPersistBackend();
            sApp = new App(0, nullptr);
            // B3: idempotent belt-and-suspenders load AFTER the ctor returns —
            // mirrors main_native.cpp:714. The in-ctor MetaPanel re-apply is the
            // authoritative load; this also covers profile-0 GameplayOptions and
            // is exact-size-gated, so it's harmless if MetaPanel already loaded.
            RB3SaveLoadGlobalOptions();
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
#ifdef HX_WEB
        // W3b-splash: after the frame's RB3GameInputPoll has drained any verb we
        // injected last frame, edge-detect splash Start/Confirm and enqueue the
        // verb for the NEXT frame's poll. Web-only; no-op off splash_screen.
        WebSplashAdvanceHook();
#endif
        sFrameCount++;
        EM_ASM_({ window.rb3FrameCount = $0; }, sFrameCount);

        // B3: polled-flag exit save. The rb3_pre.js visibilitychange:hidden /
        // pagehide listeners set window.__rb3SaveRequested; we clear it and run
        // RB3SaveSaveGlobalOptions here on the main thread. Write() is sync into
        // the JS Map (the IDB put is a queued microtask), so the bytes + a queued
        // put exist before the tab unloads. This replaces native's
        // TheDebug.AddExitCallback(RB3SaveSaveGlobalOptions) (main_native.cpp:650),
        // which never fires on web (runtime kept alive, no exit()).
        if (EM_ASM_INT({ return window.__rb3SaveRequested ? 1 : 0; })) {
            EM_ASM({ window.__rb3SaveRequested = 0; });
            RB3SaveSaveGlobalOptions();
        }

        // B3 VERIFY: process a pending live A/V-calibration poke (test-only),
        // then publish the current value. window.rb3SetExcessVideoLag is a magic
        // sentinel (NaN = "no command"); the test sets it to a finite float, we
        // apply it via SetExcessVideoLag and clear the sentinel. The exact field
        // MetaPanel re-applies, so a reload round-trip exercises the real path.
        {
            double cmd = EM_ASM_DOUBLE({
                return (typeof window.rb3SetExcessVideoLag === 'number')
                    ? window.rb3SetExcessVideoLag : NaN;
            });
            if (cmd == cmd) {  // not-NaN
                TheProfileMgr.SetExcessVideoLag((float)cmd);
                EM_ASM({ window.rb3SetExcessVideoLag = undefined; });
            }
            EM_ASM_({ window.rb3ExcessVideoLag = $0; },
                    (double)TheProfileMgr.GetExcessVideoLag());
        }

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
