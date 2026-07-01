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
//   BOOT_INIT          → WebAssetsInit + WebAssetsFetchBundle (config .dta/.dtb)
//                        + WebAssetsFetchBundle("/api/bundle/boot") (R3 boot milos)
//   BOOT_FETCHING      → poll WebAssetsAllDone (waits for BOTH bundles)
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
#include <set>
#include <map>

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

// Session-telemetry recorder — BootMark() taps RB3RecordBootMark for the boot
// timeline. (HX_NATIVE-guarded internally.)
#include "rb3_session_trace.h"

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
// Boot I/O attribution dump (handoff 02-boot-sync-read, Step 0). Prints open
// outcomes + per-path loader yield counts at appctor_done when RB3_BOOT_IO_STATS
// is set. Defined in native_file.cpp; no-op otherwise.
extern "C" void RB3BootIoStatsDump(const char *tag);
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

// Plumb a small set of loader/perf knobs from URL query params into the process
// environment so the web build is tunable without a rebuild (the wasm reads them
// via getenv). Must run before the first LoadMgr::Poll (BOOT_APP_CTOR), so it is
// called at BOOT_INIT. Lets loadperf-profile.mjs A/B e.g. ?loaderYieldMs=16 vs
// =100 to measure boot-smoothness trade-offs directly. Recognised params:
//   loaderYieldMs   -> RB3_LOADER_YIELD_MS   (drain yield interval, default 50)
//   loaderBudgetMs  -> RB3_LOADER_BUDGET_MS  (per-frame loader budget, default 8)
//   frameInstrument -> RB3_FRAME_INSTRUMENT  (per-frame timing log)
static void ApplyUrlLoaderEnv() {
    static const char *kPairs[][2] = {
        {"loaderYieldMs", "RB3_LOADER_YIELD_MS"},
        {"loaderBudgetMs", "RB3_LOADER_BUDGET_MS"},
        {"frameInstrument", "RB3_FRAME_INSTRUMENT"},
        // R3 — opt out of the eager boot-milo bundle so the netperf A/B can
        // measure the boot with/without it (control = the old sync-XHR path).
        // ?bootBundle=0 -> RB3_BOOT_BUNDLE_OFF=0 (any value present disables).
        {"bootBundle", "RB3_BOOT_BUNDLE_OFF"},
        // Handoff 02 (boot sync-read) Step-0 instrumentation + A/B knobs.
        //   ?bootIoStats=1            -> RB3_BOOT_IO_STATS (dump open/yield counts)
        //   ?bootNoResidencySkip=1    -> RB3_BOOT_NO_RESIDENCY_SKIP (disable Fix A)
        //   ?loaderMinYieldMs=16      -> RB3_LOADER_MIN_YIELD_MS (PollUntilLoaded
        //                                per-slice yield throttle; 0 = original)
        {"bootIoStats", "RB3_BOOT_IO_STATS"},
        {"bootNoResidencySkip", "RB3_BOOT_NO_RESIDENCY_SKIP"},
        {"loaderMinYieldMs", "RB3_LOADER_MIN_YIELD_MS"},
    };
    for (auto &p : kPairs) {
        char *v = (char *)EM_ASM_PTR({
            const params = new URLSearchParams(window.location.search);
            const val = params.get(UTF8ToString($0));
            if (val === null) return 0;
            const len = lengthBytesUTF8(val) + 1;
            const buf = _malloc(len);
            stringToUTF8(val, buf, len);
            return buf;
        }, p[0]);
        if (v) {
            if (v[0]) {
                ::setenv(p[1], v, 1);
                printf("RB3 Web: env %s=%s (from ?%s)\n", p[1], v, p[0]);
            }
            free(v);
        }
    }

    // Generic ?env=NAME=VALUE;... bridge drain (incremental-load-perf PLAN T1).
    // rb3_pre.js parses the single `?env` param into window.__rb3ExtraEnv (an
    // allowlisted RB3_* map) and ALSO seeds Module.ENV — but Emscripten's
    // getEnvStrings reads its OWN internal `var ENV={}`, never Module.ENV, so the
    // JS seed never reaches getenv(). The reliable path is a C++ ::setenv() (which
    // writes the live musl `environ` that getenv reads), done HERE at BOOT_INIT
    // before any flag is first read. This makes ANY RB3_* flag (e.g.
    // RB3_ASYNC_OPEN_OFF, RB3_MOGG_RANGE_OFF, RB3_BC_TEX_OFF) toggleable from the
    // browser with no rebuild and no per-flag allowlist entry above.
    char *blob = (char *)EM_ASM_PTR({
        try {
            var m = window.__rb3ExtraEnv;
            if (!m) return 0;
            var parts = [];
            for (var k in m) { if (m.hasOwnProperty(k)) parts.push(k + "=" + m[k]); }
            if (!parts.length) return 0;
            var s = parts.join("\n");
            var len = lengthBytesUTF8(s) + 1;
            var buf = _malloc(len);
            stringToUTF8(s, buf, len);
            return buf;
        } catch (e) { return 0; }
    });
    if (blob) {
        char *p = blob;
        while (*p) {
            char *eq = strchr(p, '=');
            char *nl = strchr(p, '\n');
            if (!eq || (nl && eq > nl)) {  // malformed line: skip to next
                if (!nl) break;
                p = nl + 1;
                continue;
            }
            *eq = '\0';
            if (nl)
                *nl = '\0';
            ::setenv(p, eq + 1, 1);
            printf("RB3 Web: env %s=%s (from ?env)\n", p, eq + 1);
            if (!nl)
                break;
            p = nl + 1;
        }
        free(blob);
    }
}

// True once rb3_pre.js's IndexedDB pre-warm has finished loading (or decided to
// skip). Until then the warm-boot gate can't tell warm from cold, so BOOT waits
// a bounded number of ticks for this before deciding the boot bundle.
static bool IdbReady() {
    return EM_ASM_INT({ return window.__rb3IdbReady ? 1 : 0; }) != 0;
}

// R3 / W4b warm-boot gate. Returns true iff the boot-critical .milo_xbox set is
// already resident in the IndexedDB warm cache (window.__rb3IdbCache, pre-warmed
// by rb3_pre.js and written through on a prior boot by onBundleSuccess's
// bundleCacheWriteThrough). When true, BOOT_DECIDE_BOOT_BUNDLE skips the
// /api/bundle/boot re-download and the App ctor's per-file opens serve from IDB
// via native_file.cpp's cacheTryHit() — zero network for the boot set on warm
// boots.
//
// We probe a few of the LARGEST boot milos (the dominant cold-boot offenders); if
// they're all present the cache is populated for this asset version (rb3_pre.js
// version-pins + clears the store on an asset/wasm change, so a stale partial set
// can't be present). On a cold boot (empty/not-yet-ready IDB) this returns false
// and the bundle fetch fires as normal. The keys are the server-relative paths —
// the exact cache keys both the bundle write-back and the sync path derive.
static bool BootSetAlreadyCached() {
    // NOTE: the JS body is a SINGLE EM_ASM_INT argument, so it must contain no
    // top-level commas (the C preprocessor splits macro args on commas even
    // inside JS [..] brackets). The sentinel list is therefore a '|'-delimited
    // string split at runtime, not a comma array literal.
    return EM_ASM_INT({
        try {
            if (!window.__rb3IdbReady) return 0;       // pre-warm not done → cold
            var c = window.__rb3IdbCache;
            if (!c || c.size === 0) return 0;
            // Sentinels: the heaviest boot milos. If these are resident, the boot
            // bundle was written through on a prior boot for this asset version.
            var keys = ('ui/track/gen/track_shared.milo_xbox'
                + '|ui/track/gen/trackpanel.milo_xbox'
                + '|ui/overshell/gen/overshell_player_common.milo_xbox'
                + '|sfx/gen/common_bank.milo_xbox').split('|');
            for (var i = 0; i < keys.length; i++) {
                if (!c.has(keys[i])) return 0;
            }
            return 1;
        } catch (e) { return 0; }
    });
}

// ============================================================================
// Boot state machine
// ============================================================================

enum BootState {
    BOOT_INIT,
    BOOT_DECIDE_BOOT_BUNDLE,  // R3: wait (bounded) for IDB readiness, then warm-gate the boot bundle
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
static int sIdbWaitFrames = 0;  // R3: bounded wait for the IDB pre-warm before deciding the boot bundle
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

// ============================================================================
// A2 (incremental-load-perf PLAN.md T9) — per-screen dependency bundles.
//
// When the user ENTERs a screen, fire an ASYNC fetch of the NEXT screen's
// dependency bundle (/api/bundle/screen/<name>) so its extra .milo_xbox/.dta
// land in warm MEMFS during the dwell, BEFORE that screen's panel loaders ask
// for them. This reuses the exact boot-bundle async fetch+unpack path
// (WebAssetsFetchBundle): the bundle is downloaded off-thread and unpacked into
// /data/<rel>, and the engine's File ctor serves the now-resident bytes from
// MEMFS instead of freezing the wasm thread on a per-file sync XHR.
//
// The current->next mapping mirrors UIScreen.cpp's prewarm default
// (splash->main_hub, main_hub->song_select). A screen whose bundle manifest is
// absent emits an empty bundle (server.py), so an unmapped/unknown screen is a
// harmless no-op. We fire each screen's bundle at most once per session (a
// file-static seen-set), keyed by the predicted-next screen name.
//
// Default ON for web; opt out with RB3_SCREEN_BUNDLES_OFF (any value disables).
// Tunable mapping via RB3_SCREEN_BUNDLE_NEXT="from:to,from2:to2" (same syntax as
// RB3_PREWARM_NEXT). This is the prefetch counterpart to the UIScreen kLoadBack
// prewarm (Q10): bundles warm MEMFS at the byte layer; prewarm warms the parsed
// PanelDir at the loader layer. They compose.
static const char *kDefaultScreenBundleMap =
    "splash_screen:main_hub,main_hub_screen:song_select";

static bool ScreenBundlesEnabled() {
    // Default ON; opt out with a TRUTHY RB3_SCREEN_BUNDLES_OFF. A literal "0"
    // (or empty) means "not disabled" so the ?env= A/B bridge can pass
    // RB3_SCREEN_BUNDLES_OFF=0 to force ON without ambiguity (mirrors the engine
    // RB3_PP_OFF / RB3_PIPELINE_PREWARM_OFF idiom). getenv once into a static.
    static int s = -1;
    if (s < 0) {
        const char *e = ::getenv("RB3_SCREEN_BUNDLES_OFF");
        s = (e && e[0] && e[0] != '0') ? 0 : 1;  // truthy OFF flag => disabled
    }
    return s != 0;
}

// Resolve the bundle name to fetch when entering `fromScreen`, or "" if none.
// Parsed once from RB3_SCREEN_BUNDLE_NEXT (or the default). Keys are UIScreen
// object names; values are the server bundle <name> (screen-<name>.manifest).
static std::string NextScreenBundleName(const char *fromScreen) {
    static std::map<std::string, std::string> sMap;
    static bool sInit = false;
    if (!sInit) {
        sInit = true;
        const char *spec = ::getenv("RB3_SCREEN_BUNDLE_NEXT");
        std::string s = (spec && spec[0]) ? spec : kDefaultScreenBundleMap;
        size_t pos = 0;
        while (pos < s.size()) {
            size_t comma = s.find(',', pos);
            std::string pair = s.substr(
                pos, comma == std::string::npos ? std::string::npos : comma - pos);
            size_t colon = pair.find(':');
            if (colon != std::string::npos) {
                std::string from = pair.substr(0, colon);
                std::string to = pair.substr(colon + 1);
                if (!from.empty() && !to.empty())
                    sMap[from] = to;
            }
            if (comma == std::string::npos)
                break;
            pos = comma + 1;
        }
    }
    if (!fromScreen)
        return "";
    std::map<std::string, std::string>::const_iterator it = sMap.find(fromScreen);
    return it == sMap.end() ? "" : it->second;
}

// Called each frame from BOOT_RUNNING. On a change of current screen, fire the
// matching screen bundle once. Cheap (a strcmp + a static-set lookup) when the
// screen hasn't changed or has no mapping.
static void WebScreenBundleHook() {
    if (!ScreenBundlesEnabled())
        return;
    UIScreen *scr = TheUI.CurrentScreen();
    const char *name = (scr && scr->Name()) ? scr->Name() : "";
    if (!name[0])
        return;

    static std::string sLastScreen;
    if (sLastScreen == name)
        return;  // no transition this frame
    sLastScreen = name;

    std::string bundle = NextScreenBundleName(name);
    if (bundle.empty())
        return;

    static std::set<std::string> sFired;
    if (sFired.count(bundle))
        return;  // one fetch per bundle per session
    sFired.insert(bundle);

    std::string url = "/api/bundle/screen/" + bundle;
    printf("RB3 Web: screen '%s' entered -> prefetch bundle %s\n", name, url.c_str());
    WebAssetsFetchBundle(url.c_str());
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

    // Lock the note-highway clock to the audio clock (kills the Wii's fixed
    // -20ms A/V calibration, which the latency-free WebGPU renderer must not
    // apply — it reads as "audio leads the visuals"). Same call as native.
    // See RB3ApplyNativeAVCalibration() in rb3_synth_native.cpp.
    extern void RB3ApplyNativeAVCalibration();
    RB3ApplyNativeAVCalibration();

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

// Emit a boot-phase timestamp to the JS perf timeline (performance.mark) and
// window.rb3BootPhase, so loadperf-profile.mjs can attribute boot time to the
// exact phase (fetch / engine-init / GPU / App-ctor). Cheap; always on.
static void BootMark(const char *phase) {
    EM_ASM_({
        var p = UTF8ToString($0);
        try { performance.mark('rb3:boot:' + p); } catch (e) {}
        window.rb3BootPhase = p;
        if (!window.rb3BootPhaseLog) window.rb3BootPhaseLog = [];
        window.rb3BootPhaseLog.push([p, +performance.now().toFixed(1)]);
    }, phase);
    // SESSION-TELEMETRY boot tap: emit a `boot` row for this phase. RB3TraceInit
    // is idempotent; the recorder gates on gRB3TraceActive. On web the sink is
    // armed lazily once the JS toggle is read, so an early phase that fires
    // before init simply no-ops (acceptable — the bulk of boot phases land after).
    RB3TraceInit();
    RB3RecordBootMark(phase);
}

static void mainLoop() {
    switch (sBootState) {
    case BOOT_INIT: {
        ApplyUrlLoaderEnv();  // URL-param loader/perf knobs (before first Poll)
        BootMark("fetch_start");
        printf("RB3 Web: downloading assets (bundle)...\n");
        WebAssetsInit();
        WebAssetsFetchBundle();  // config bundle (.dta/.dtb) — /api/bundle
        // Signal to the loading screen that asset fetch has started.
        EM_ASM({ window.rb3AssetsLoaded = 0; window.rb3AssetsTotal = 0; });
        // R3 — defer the boot-milo bundle decision one phase so the IDB pre-warm
        // (rb3_pre.js, racing the wasm download) can finish: the warm-boot gate
        // needs __rb3IdbReady to tell warm from cold. The config bundle above is
        // already in flight, so this short bounded wait overlaps it (no time lost
        // on a cold boot, and it's what lets a warm boot skip the 60 MB bundle).
        sIdbWaitFrames = 0;
        sBootState = BOOT_DECIDE_BOOT_BUNDLE;
        break;
    }

    case BOOT_DECIDE_BOOT_BUNDLE: {
        // Fire (or skip) the boot-critical .milo_xbox bundle (/api/bundle/boot).
        // Both bundles are async and bump the engine's shared sPending counter,
        // so BOOT_FETCHING's WebAssetsAllDone() gate waits for whichever fire.
        // With the boot milos resident in MEMFS, the App ctor's sync milo reads
        // hit warm MEMFS instead of freezing the wasm thread on a per-file
        // synchronous XHR (the dominant cold-boot stall).
        //
        // Wait (bounded, ~0.5s @60fps) for the IDB pre-warm before deciding, so
        // a warm boot is correctly detected; if it never readies, fall through
        // and fetch the bundle (cold path — correct, just no warm skip).
        static const int kIdbWaitTimeout = 30;
        bool off = getenv("RB3_BOOT_BUNDLE_OFF") != nullptr;
        if (!off && !IdbReady() && ++sIdbWaitFrames < kIdbWaitTimeout)
            break;  // keep waiting; config bundle keeps downloading meanwhile

        // Opt out with ?bootBundle=0 (RB3_BOOT_BUNDLE_OFF) for the A/B control.
        // The boot bundle is ON by default; passing ?bootBundle=0 sets the env
        // var (ApplyUrlLoaderEnv ran in BOOT_INIT), so its presence == "disable".
        if (off) {
            const char *v = getenv("RB3_BOOT_BUNDLE_OFF");
            printf("RB3 Web: boot-milo bundle DISABLED (RB3_BOOT_BUNDLE_OFF=%s)\n", v ? v : "");
        } else if (BootSetAlreadyCached()) {
            // W4b warm-boot: the boot milos are already in the IndexedDB warm
            // cache (written through on a previous boot by onBundleSuccess's
            // bundleCacheWriteThrough). Skip the ~60 MB bundle re-download and let
            // the App ctor's per-file opens hit IDB via native_file.cpp's
            // cacheTryHit() — zero network for the boot set on repeat boots.
            // (Design alt (b): gate the bundle when the set is IDB-resident.
            // (a) write-back makes the set AVAILABLE in IDB; (b) the gate is what
            // actually prevents the re-download, so both are needed.)
            printf("RB3 Web: boot-milo set is IDB-resident — skipping bundle (warm boot)\n");
        } else {
            printf("RB3 Web: fetching boot-milo bundle (/api/bundle/boot)...\n");
            WebAssetsFetchBundle("/api/bundle/boot");
        }
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
        BootMark("fetch_done");
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
            BootMark("engine_init_done");
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
        BootMark("gpu_ready");
        printf("RB3 Web: GPU ready — initializing resources...\n");
        gBandRnd.InitGpuResources();
        BootMark("appctor_start");
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
        BootMark("appctor_done");
        RB3BootIoStatsDump("appctor_done"); // Step 0 attribution (gated)
        printf("RB3 Web: App constructed — entering RunOneFrame loop\n");
        EM_ASM({ window.rb3AppBooted = 1; });
        sBootState = BOOT_RUNNING;
        break;
    }

    case BOOT_RUNNING: {
        // Wave-3 / M1 measurement: the per-frame JSONL frame-trace
        // (RB3_FRAME_TRACE=<path>, native/src/rb3_frame_trace.cpp) is normally
        // driven from App::RunWithoutDebugging's frame loop — which the WEB build
        // never enters (Emscripten can't block in a C++ for-loop; this main-loop
        // tick calls RunOneFrame directly). So on web the recorder + the
        // gFrameTraceActive-gated attribution counters never fire. Mirror the
        // native frame-trace wrap here, web-only + env-gated, so the same
        // counter-attributed trace JSONL is produced in the browser. Costs one
        // static branch when RB3_FRAME_TRACE is unset.
        static int sFrameTrace = -1;
        if (sFrameTrace < 0)
            sFrameTrace = getenv("RB3_FRAME_TRACE") ? 1 : 0;
        extern void RB3FrameTraceRecord(int frame, float dtMs, float loadPollMs,
                                        float loadPollUntilMs, const char *screen,
                                        int pendingLoaders);
        extern float gLoadPollMsThisFrame;
        extern float gLoadPollUntilMsThisFrame;
        try {
            if (sFrameTrace) {
                gLoadPollMsThisFrame = 0.0f;
                gLoadPollUntilMsThisFrame = 0.0f;
                double t0 = emscripten_get_now();
                sApp->RunOneFrame(sFrameCount);
                float ms = (float)(emscripten_get_now() - t0);
                UIScreen *scr = TheUI.CurrentScreen();
                const char *scrName = (scr && scr->Name()) ? scr->Name() : "?";
                RB3FrameTraceRecord(sFrameCount, ms, gLoadPollMsThisFrame,
                                    gLoadPollUntilMsThisFrame, scrName,
                                    (int)TheLoadMgr.mLoading.size());
            } else {
                sApp->RunOneFrame(sFrameCount);
            }
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
        // A2 (T9): on a screen change, async-prefetch the next screen's
        // dependency bundle into MEMFS during the dwell. No-op when disabled
        // (RB3_SCREEN_BUNDLES_OFF) or the screen has no next-bundle mapping.
        WebScreenBundleHook();
        sFrameCount++;
        EM_ASM_({ window.rb3FrameCount = $0; }, sFrameCount);

#ifdef __EMSCRIPTEN__
        // SESSION-TELEMETRY web egress: drain the C++ recorder ring to the JS
        // window.__rb3Trace array on a cadence. Unlike native (which streams to a
        // FILE* on every push), the web sink only auto-drains at ring half-fill
        // (~8192 events) — so a session shorter than that would egress ONLY the
        // hdr line and never the per-frame fr/in/nav/clk rows. Flushing every ~30
        // frames drains the ring ~2x/sec; the pre-js ~5s timer + sendBeacon then
        // ship it. Guarded __EMSCRIPTEN__ so native (drains on push) never
        // double-flushes. RB3TraceFlush no-ops when the recorder isn't armed.
        if ((sFrameCount % 30) == 0)
            RB3TraceFlush();
#endif

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

// SESSION-TELEMETRY tail flush: drain the C++ recorder ring to window.__rb3Trace
// on demand. rb3_pre.js's pagehide/visibilitychange teardown calls this BEFORE
// its sendBeacon so the final <30 frames still in the ring (between the periodic
// BOOT_RUNNING flush and unload) egress instead of being lost. No-ops when the
// recorder isn't armed.
EMSCRIPTEN_KEEPALIVE
void rb3_trace_flush() {
    RB3TraceFlush();
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
