#include "App.h"
#ifndef HX_NATIVE
// Wii-only (pulls STLPORT map allocators). Only BandOffline::Init() needs it,
// and that call below is itself HX_NATIVE-gated.
#include "BandOffline.h"
#endif
#include "BudgetScreen.h"
#include "ChecksumData_wii.h"
#ifndef HX_NATIVE
#include "MSL_Common/null_def.h" // MWCC MSL header (NULL/nullptr); host STL supplies these
#endif
#include "beatmatch/BeatMatch.h"
#include "bandobj/Band.h"
#include "bandobj/BandDirector.h"
#include "bandobj/PatchDir.h"
#include "char/Char.h"
#include "decomp.h"
#include "game/BandUserMgr.h"
#include "game/Game.h"
#include "game/GameMicManager.h"
#include "meta/Achievements.h"
#include "meta/FixedSizeSaveable.h"
#include "meta/WiiProfileMgr.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/AssetMgr.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/CharCache.h"
#include "meta_band/CharSync.h"
#include "meta_band/ClosetMgr.h"
#include "meta_band/ContextChecker.h"
#include "meta_band/LessonMgr.h"
#include "meta_band/MetaPanel.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/PassiveMessenger.h"
#include "meta_band/PrefabMgr.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SaveLoadManager.h"
#include "meta_band/StoreRootPanel.h"
#include "meta_band/TrainingMgr.h"
#include "meta_band/UIStats.h"
#ifndef HX_NATIVE
#include "movie/CustomSplash_Wii.h" // pulls Wii TPL/GX SDK headers; gated out on native
#endif
#include "movie/Splash.h"
#include "net/Net.h"
#include "net_band/EntityUploader.h"
#include "net_band/RockCentral.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Archive.h"
#include "os/ContentMgr_Wii.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "os/Timer.h"
#include "os/UsbMidiGuitar.h"
#include "os/UsbMidiKeyboard.h"
#include "revolution/os/OSError.h"
#include "revolution/os/OSThread.h"
#include "revolution/os/OSTime.h"
#include "bandobj/BandDirector.h"
#include "rndobj/HiResScreen.h"
#include "rndobj/Rnd.h"
#ifndef HX_NATIVE
#include "rndwii/Env.h" // Wii GX renderer (GXLightObj etc.) — TheWiiRnd/WiiEnviron
#include "rndwii/Rnd.h"
#endif
#include "synth/BinkReader.h"
#include "synth/MicManagerInterface.h" // MicClientID (sNullMicClientID sentinel)
#include "synth/Synth.h"
#ifdef HX_WEB
#include "synth/Faders.h"    // Fader::Init() — web registers the inert synth
#include "synth/BinkClip.h"  // BinkClip::Init() — UI-object factories directly
#endif
#include "band3/tour/QuestManager.h"
#include "track/Track.h"
#include "ui/UI.h"
#include "ui/UIList.h"
#include "utl/Cheats.h"
#include "utl/Loader.h"
#ifdef HX_NATIVE
// Session-telemetry recorder API (RB3TraceInit / RB3RecordFrame / RB3RecordClock
// / gRB3TraceActive / RB3TraceSetFrame / RB3TraceSetSongMs / RB3TraceSetSimDt).
// The frame tap lives in RunOneFrame. native/src is on the native include path
// (CMAKE_SOURCE_DIR}/src).
#include "rb3_session_trace.h"
#include "rb3_replay.h" // RB3ReplayFixedClock/Active (fixed-clock replay status log)
#include "obj/Task.h"   // TheTaskMgr.mTime.mCycles (session-telemetry sim-dt capture)
#endif
#include "utl/Magnu.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Rso_Utl.h"
#include "utl/Option.h"
#include "world/World.h"
#ifndef HX_NATIVE
#include <list>            // gPlatformErrorMsg (std::list<DataArrayPtr>)
#include <revolution/VI.h> // Wii VI* (VISetBlack/VIFlush); gated out on native
#endif
#ifdef HX_NATIVE
#include <csetjmp> // native frame-loop draw guard (sigsetjmp)
#include <cstdlib> // getenv/atoi (MILO_MAX_FRAMES)
#include "audio/AudioDevice.h" // N9: AudioDevice::Suspend() at frame-loop exit
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "audio/AudioDevice.h" // RunOneFrame web audio pump (no-op until W3c)
#endif

#ifdef VERSION_SZBE69_B8
DECOMP_FORCEACTIVE(App, "_unresolved func.\n")
#endif
MicClientID sNullMicClientID(-1, -1);
ModalCallbackFunc *gRealCallback;

#ifdef HX_NATIVE
// Declared in rndwii/Rnd.h (Wii GX renderer), which is gated out on native.
// The asm-match #else frame loop still references it identically; provide the
// decl so it compiles + links (resolved by a weak no-op link stub).
void SetGPHangDetectEnabled(bool, const char *);
#endif

const int initArk = 2;
const int charArk = 5;
const int regularArks = 3;

extern bool gInitComplete;
extern int gCheckConsistencyish;
extern int gCooldown;
extern Timer gTriFrameTimer;
extern void MemCheckConsistency(const char *, int);
static void CheckForPassivePlatformErrors();
#ifndef HX_NATIVE
// Queue of platform-error DataArrays, drained into the passive-message UI.
// Defined in os/ContentMgr_Wii.cpp (Wii-only — no header), excluded from the
// native build, so both the extern and the drain loop are gated out on native.
extern std::list<DataArrayPtr> gPlatformErrorMsg;
#endif

#ifdef VERSION_SZBE69
#pragma push
#pragma dont_inline on
#endif
void AppDebugModal(bool &b, char *msg, bool b2) {
    if (!b) {
        static DataNode &notify_level = DataVariable("notify_level");
        int notif_lvl = notify_level.Int();
        if (notif_lvl == 2) {
            gRealCallback(b, msg, b2);
            return;
        } else if (notif_lvl == 1) {
            Hmx::Object *disp = ObjectDir::sMainDir->FindObject("cheat_display", false);
            if (disp) {
                static Message show("show_prio", 0, 0);
                show[0] = DataNode(msg);
                show[1] = DataNode(200);
                disp->Handle(show, 0);
            } else
                goto log_notify;
        } else {
        log_notify:
            MILO_LOG("%s\n", msg);
        }
    } else
        gRealCallback(b, msg, b2);
}

App::App(int argc, char **argv) {
    static const int kESRBMs = 4000;
    static const int kRegularSplashMs = 4000;
    Timer init_time;
    init_time.Start();
    InitMakeString();
    class String s;
#ifdef HX_NATIVE
    // `c` must outlive the SystemPreInit/SetSystemArgs calls below, which read
    // through argv (= &c). A block-scoped `c` (as in the Wii path) leaves argv
    // dangling to an out-of-scope stack slot once the if exits — benign on Wii
    // (argc != 0, so the block never runs), but real UB and ASan-fatal on the
    // native/web argc==0 boot path. Function-scoped here; the Wii branch keeps
    // the original block-scoped form to preserve the asm match.
    const char *c = NULL;
    if (argc == 0) {
#ifdef MILO_DEBUG
        s = "band_r_wii.elf";
#else
        s = "band_s_wii.elf";
#endif
        c = s.c_str();
        argv = const_cast<char **>(&c);
        argc = 1;
    }
#else
    if (argc == 0) {
#ifdef MILO_DEBUG
        s = "band_r_wii.elf";
#else
        s = "band_s_wii.elf";
#endif
        const char *c = s.c_str();
        argv = const_cast<char **>(&c);
        argc = 1;
    }
#endif
    RsoAddIniter(CntSdRsoInit, CntSdRsoTerminate);
    EnableKeyCheats(false);
    SetFileChecksumData();
    SystemPreInit(argc, argv, "config/band_preinit_keep.dta");
    TheRnd->PreInit();
#ifndef HX_NATIVE
    // Wii disc-error manager: tracks DVD read failures. No optical disc on the
    // native host, so mDiscErrorMgr is never constructed — skip activating it.
    ThePlatformMgr.mDiscErrorMgr->mActive = true;
#endif
#if defined(MILO_DEBUG) && !defined(HX_NATIVE)
    TheRnd->SetClearColor(Hmx::Color(1, 0, 0));
#else
    // Native always clears to black. The (1,0,0) MILO_DEBUG sentinel was
    // useful when the rasterizer drew nothing — with V4's per-material
    // alpha-blend mapping, transparent UI/text quads composite over the
    // clear, so red bleeds through wherever the venue/skybox isn't drawing
    // (cosmetic venue deferral). Black matches retail RB3's release-build
    // path and renders gameplay frames as "highway over black void" rather
    // than "highway over red void".
#ifdef HX_NATIVE
    // RB3_CLEAR_COLOR=r,g,b lets a diagnostic distinguish "void is empty
    // (clear-color shows through)" from "void is opaque geometry rendering
    // black". This is the AUTHORITATIVE clear-color setter for the game (it
    // runs after main_native's). Default black.
    {
        float cr = 0, cg = 0, cb = 0;
        const char *cc = getenv("RB3_CLEAR_COLOR");
        if (cc) sscanf(cc, "%f,%f,%f", &cr, &cg, &cb);
        TheRnd->SetClearColor(Hmx::Color(cr, cg, cb));
    }
#else
    TheRnd->SetClearColor(Hmx::Color(0, 0, 0));
#endif
#endif
    TheRnd->Init();
#ifndef HX_NATIVE
    VISetBlack(true);
    VIFlush();
#endif
    bool fast = OptionBool("fast", false);
#ifndef HX_NATIVE
    Splash spl;
#ifdef MILO_DEBUG
    if (fast || !UsingCD()) {
#else
    if (fast) {
#endif
        spl.SetWaitForSplash(false);
    }
    if (ThePlatformMgr.GetRegion() == 1) {
        spl.AddScreen("ui/startup/eng/startup_autosave_esrb_keep.milo", kRegularSplashMs);
    } else {
        spl.AddScreen("ui/startup/eng/startup_autosave_keep.milo", kRegularSplashMs);
    }
    spl.AddScreen("ui/startup/startup_mtv_keep.milo", 2000);
#ifdef VERSION_SZBE69_B8
    if (spl.Unk64() && UsingCD()) {
#else
    if (spl.Unk64()) {
#endif
        if (TheRnd->GetAspect() == Rnd::kWidescreen) {
            spl.AddScreen("ui/startup/startup_movie_keep_wide.milo", 2000);
        } else {
            spl.AddScreen("ui/startup/startup_movie_keep.milo", 2000);
        }
    } else {
        spl.AddScreen("ui/startup/startup_harmonix_keep.milo", 2000);
    }
    spl.AddScreen("ui/startup/startup_ea_keep.milo", 2000);
    spl.PrepareNext();
#endif // HX_NATIVE (splash setup — Wii VI / CustomSplash / cosmetic logos)

    gInitComplete = false;

#ifndef HX_NATIVE
    CustomSplash csplash;
#ifdef VERSION_SZBE69_B8
    if (fast || !UsingCD()) {
#else
    if (fast) {
#endif
        csplash.SetUnk490(0);
    }
    csplash.Init();
    csplash.Show();
#endif // HX_NATIVE (CustomSplash — Wii-only movie/CustomSplash_Wii.h)
    SynthInit();
#ifdef HX_WEB
    // Web audio is LIVE since W3c: Synth.cpp is re-included in the web build (the
    // codec.h alloca clash is fixed by the #ifndef HX_NATIVE guard), so the
    // SynthInit() above runs the real NativeSynth (AudioDevice_Web AudioWorklet
    // ring-buffer path; PumpAudio is driven from RunOneFrame). These two Init()
    // calls remain ONLY as inert object-factory registration: the boot-path UI
    // milos (overshell / main_hub / splash) embed two non-audio synth object
    // classes — SynthFader (Fader) and BinkClip — and a milo that references an
    // unregistered class instantiates a null object that downstream code derefs →
    // wasm trap (the W3a frame-1 menu-load crash). Both are pure Hmx::Objects with
    // no audio-device dependency, so registering their factories here is safe.
    Fader::Init();
    BinkClip::Init();
#endif
    Movie::Init();
#ifndef HX_NATIVE
    csplash.EndShow();
#endif
    gInitComplete = true;
    TheRnd->BeginDrawing();
    TheRnd->EndDrawing();
#ifndef HX_NATIVE
    spl.BeginSplasher();
    float splasher_time = init_time.SplitMs();
#endif
    if (TheArchive != nullptr) {
        TheArchive->SetArchivePermission(1, &initArk);
    }
#ifndef HX_NATIVE
    spl.PrepareRemaining();
#endif
    SystemInit("config/band_keep.dta");
#ifdef MILO_DEBUG
    MagnuInit();
#endif
    PollTheSplasher();
    PollTheSplasher();
    static DataNode &notify_level = DataVariable("notify_level");
    notify_level = DataNode(1);
    gRealCallback = TheDebug.SetModalCallback(AppDebugModal);
    BinkReaderHeapInit();
    FixedSizeSaveable::Init(151, 5688);
    BandUserMgrInit();
    PollTheSplasher();
#ifndef HX_NATIVE
    TheNet.Init();
    PollTheSplasher();
    TheRockCentral.Init(false);
    PollTheSplasher();
    TheEntityUploader.Init();
    PollTheSplasher();
    GameMicManager::Init();
    UsbMidiKeyboard::Init();
    UsbMidiGuitar::Init();
    PollTheSplasher();
#endif // HX_NATIVE (online services + Wii USB peripheral mgrs — no native backing)
#ifdef HX_NATIVE
    // GameMicManager was mis-grouped in the #ifndef block above with online
    // services + Wii USB peripheral mgrs that genuinely lack native backing.
    // It HAS full native backing (Synth::Init registers every FxSend factory;
    // sfx/mic_fx.milo loads), and the no-device Init chain is a guarded no-op
    // (GetMicCount()==0). Leaving it gated keeps TheGameMicManager NULL while the
    // now-compiled VocalPlayer/Singer code unguarded-derefs it — a latent SIGSEGV
    // the instant a vocal player is added. Init it natively too (UsbMidi* + the
    // online services stay gated). Audit bring-online; opt-out RB3_NO_MIC_MGR.
    if (!getenv("RB3_NO_MIC_MGR"))
        GameMicManager::Init();
#endif
#ifdef __EMSCRIPTEN__
#define WEB_BOOT_MARK(s) printf("RB3 Web boot: %s\n", s)
#else
#define WEB_BOOT_MARK(s) ((void)0)
#endif
    WEB_BOOT_MARK("loading sound bank (common)");
    // The common SFX bank loads on ALL targets, web included. The old web skip
    // here (W3a "audio-free") was made stale by the W3c audio bring-up: Synth.cpp
    // + VorbisReader.cpp are now compiled into rb3-web, the SynthInit() above runs
    // the real NativeSynth (AudioDevice_Web), and every leaf factory the bank
    // embeds (Sfx / SynthSample / *GroupSeq / Fader / FxSend*) is registered before
    // this load. The synchronous LoadFile drains through the cooperative HX_WEB
    // PollUntilLoaded slice (Loader.cpp), so there is no longer an un-interruptible
    // spin. Xbox-360 SFX samples (kXMA) play via the offline PCM sidecars
    // (rb3_xma_sidecar / rb3_sampleinst_native); drum-kit banks (kBigEndPCM) decode
    // directly. native/web/server.py serves the sidecars on demand.
    {
        ObjDirPtr<ObjectDir> oPtr;
        oPtr.LoadFile(
            SystemConfig("sound", "banks", "common")->Str(1), 0, 1, kLoadFront, 0
        );
#ifndef __EMSCRIPTEN__
        TheSynth->SetDir(oPtr.Ptr());
#else
        // Web: TheSynth is the live NativeSynth (W3c), but null-guard defensively
        // to mirror the SetDolby pattern further below.
        if (TheSynth) TheSynth->SetDir(oPtr.Ptr());
#endif
        PollTheSplasher();
    }
    WEB_BOOT_MARK("sound bank done");

    SaveLoadManager::Init();
    WEB_BOOT_MARK("SaveLoadManager::Init done");
    CharInit();
    WEB_BOOT_MARK("CharInit done");
    PollTheSplasher();
    BeatMatchInit();
    WEB_BOOT_MARK("BeatMatchInit done");
    PollTheSplasher();
    TrackInit();
    WEB_BOOT_MARK("TrackInit done");
    PollTheSplasher();
    WorldInit();
    WEB_BOOT_MARK("WorldInit done");
    PollTheSplasher();
    BandInit();
    WEB_BOOT_MARK("BandInit done");
    PollTheSplasher();
    TheSongMgr.Init();
    WEB_BOOT_MARK("TheSongMgr.Init done");
    MetaPanel::Init();
    WEB_BOOT_MARK("MetaPanel::Init done");
    PollTheSplasher();
    GameInit();
    WEB_BOOT_MARK("GameInit done");
    PollTheSplasher();
#ifdef MILO_DEBUG
#ifndef HX_NATIVE
    // BandOffline.cpp is Wii-only (not in the native build); it just registers
    // the make_charbudget data func, which the native debug tools don't use.
    BandOffline::Init();
#endif
    PollTheSplasher();
    BudgetScreen::Register();
    PollTheSplasher();
#endif
    ContextCheckerInit();
    WEB_BOOT_MARK("ContextCheckerInit done");
    PollTheSplasher();
    WEB_BOOT_MARK("post-ContextChecker PollTheSplasher done");
#ifndef __EMSCRIPTEN__
    TheSynth->SetDolby(0, 1);
#else
    if (TheSynth) TheSynth->SetDolby(0, 1);
#endif
    PollTheSplasher();
    WEB_BOOT_MARK("before CharCache::Init");
    CharCache::Init();
    WEB_BOOT_MARK("CharCache::Init done");
    PrefabMgr::Init(nullptr);
    WEB_BOOT_MARK("PrefabMgr::Init done");
    CharSync::Init(nullptr);
    WEB_BOOT_MARK("CharSync::Init done");
    AssetMgr::Init();
    WEB_BOOT_MARK("AssetMgr::Init done");
    LessonMgr::Init();
    WEB_BOOT_MARK("LessonMgr::Init done");
    ClosetMgr::Init();
    WEB_BOOT_MARK("ClosetMgr::Init done");
    TrainingMgr::Init();
    WEB_BOOT_MARK("TrainingMgr::Init done");
    PatchDir::Init();
    WEB_BOOT_MARK("mgr Init cluster done");
    PollTheSplasher();
#ifndef HX_NATIVE
    TheWiiProfileMgr.Init(151, 45);
#endif // HX_NATIVE (Wii NAND profile mgr — no native backing)
    TheUI.Init();
    WEB_BOOT_MARK("TheUI.Init done");
#ifdef HX_NATIVE
    // Register the offline DTA-manager stubs whose real subsystems are excluded
    // from the native link (saveload_mgr / net_cache_mgr) so the splash boot
    // state machine advances. Placed after TheUI.Init() per DTA_MANAGER_STUBS §4.
    extern void RB3RegisterNativeManagerStubs();
    RB3RegisterNativeManagerStubs();
#endif
    TheCharSync->UpdateCharCache();
    WEB_BOOT_MARK("UpdateCharCache done");
    PollTheSplasher();
    TheQuestMgr.Init(SystemConfig("tour"));
    WEB_BOOT_MARK("TheQuestMgr.Init done");
    InitStoreOverlay();
    PollTheSplasher();
#ifndef HX_NATIVE
#ifdef VERSION_SZBE69_B8
    if (UsingCD())
#endif
        if (NewFile("charnames.zbm", 0x10002) == nullptr) {
            ThePlatformMgr.SetDiskError(kDiskError);
        }
#endif // HX_NATIVE (disc-error probe — no Wii disc)
    if (TheArchive != nullptr) {
        TheArchive->SetArchivePermission(1, &charArk);
    }
    WEB_BOOT_MARK("before PollUntilEmpty");
    TheLoadMgr.PollUntilEmpty();
    WEB_BOOT_MARK("PollUntilEmpty done");
    float total_time = init_time.SplitMs();
#ifdef HX_NATIVE
    (void)total_time; // splasher_time/MILO_LOG that consume it are gated out
#endif
    if (TheArchive != nullptr) {
        TheArchive->SetArchivePermission(7, &regularArks);
#ifdef MILO_DEBUG
        if (Archive::DebugArkOrder()) {
#ifndef HX_NATIVE
            MILO_LOG("Startup Time: %f %f\n", splasher_time, splasher_time - total_time);
#endif
        }
#else
        Archive::DebugArkOrder();
#endif
    }
#ifndef HX_NATIVE
    spl.EndSplasher();
#endif
    EnableKeyCheats(true);
    AutoGlitchReport::EnableCallback();
    MemSetAllowTemp("main", 0);
#ifndef HX_NATIVE
    MemPushHeap(MemFindHeap("fast")); // Wii fixed-heap regions; native uses the host allocator
    MemPushHeap(MemFindHeap("main"));
#endif // HX_NATIVE
    gGCNewLists = false;
    // gFrameMissThreshold = 166;
}
#ifdef VERSION_SZBE69
#pragma pop
#endif

App::~App() { TheDebug.Exit(0, true); }

#ifdef HX_NATIVE
// Per-frame core poll + draw. Extracted verbatim from the native frame-loop
// body (the old RunWithoutDebugging HX_NATIVE branch) so the SAME code drives
// both the native desktop loop and the web boot machine (main_web.cpp
// BOOT_RUNNING). The native-desktop-only concerns — HTTP debug server polling,
// the bounded frame counter, the sigsetjmp draw guard's process-level signal
// machinery — stay in RunWithoutDebugging / are gated below. This whole method
// is invisible to the Wii MWCC asm build (HX_NATIVE undefined there).
void App::RunOneFrame(int frame) {
    extern void RB3GameInputPoll(int frame);

    // ── SESSION-TELEMETRY frame tap ──────────────────────────────────────────
    // The single per-frame metrics tap, placed in RunOneFrame (NOT the native
    // RunWithoutDebugging loop) so ONE tap covers BOTH the native desktop loop
    // AND the web boot driver (main_web.cpp:653 calls RunOneFrame directly,
    // bypassing RunWithoutDebugging). RB3TraceInit() is lazy + idempotent (reads
    // RB3_SESSION_TRACE / RB3_FRAME_TRACE once); gRB3TraceActive is the single
    // predicted branch when tracing is off, so the timer + load-poll resets are
    // zero-cost in a normal run. The matching RB3RecordFrame call is at the
    // bottom of this method. See docs/native/SESSION_TELEMETRY_DESIGN.md
    // "Locked v1 contract" (Frame metrics).
    extern float gLoadPollMsThisFrame;
    extern float gLoadPollUntilMsThisFrame;
    RB3TraceInit();
    Timer rb3FrameTimer;
    if (gRB3TraceActive) {
        RB3TraceSetFrame(frame);
        // Reset the per-frame load-attribution counters before the polls below
        // accumulate into them (Loader.cpp adds during SystemPoll/TheLoadMgr.Poll).
        gLoadPollMsThisFrame = 0.0f;
        gLoadPollUntilMsThisFrame = 0.0f;
        rb3FrameTimer.Restart();
    } else if (RB3FixedClockActive()) {
        // W0.3b frozen-clock determinism harness (no trace): advance the frame
        // index every RunOneFrame so Task.cpp SEAM 1's once-per-frame accumulator
        // (gRB3TraceFrame != sReplayLastFrame) steps deterministically without a
        // loaded trace. No telemetry side-effects — the gRB3TraceActive fast-path
        // above is byte-identical when this harness flag is unset.
        RB3TraceSetFrame(frame);
    }

    SystemPoll(false);
    TheUI.Poll();
#ifdef HX_NATIVE
    // VENUE POLL FIX: drive the gameplay venue band-director tick every frame.
    //
    // The gameplay venue WorldDir lives on TheBandDirector->mCurWorld (e.g.
    // small_club_01), brought up separate from the world_panel PanelDir (V19
    // deferred-proxy bring-up), with world_panel->mLoaded = true. UIPanel::Poll()'s
    // `if (mDir && !mLoaded) mDir->Poll()` guard therefore SKIPS the venue
    // WorldDir::Poll, and the retail poll chain that would otherwise reach it
    // (GamePanel::Poll -> Game::Poll -> world poll) does not run in the native flow
    // (GamePanel never goes Active; the HUD/gem track is ticked elsewhere). So
    // nothing polls the venue. WorldDir::Poll is what runs the entire venue tick:
    //   (a) HandleType(select_camera_msg) -> BandDirector::OnSelectCamera, which
    //       selects/plays the camera shots that FRAME the band (without it the venue
    //       draws through a fixed default cam aimed at scenery, never the band),
    //   (b) mCameraManager PrePoll/Poll, which animates the active shot's camera,
    //   (c) RndDir::Poll, which polls the BandCharacters + their CharDriverMidi so
    //       the song's char clips animate the skeletons (without it the band draws
    //       frozen at its static load / bind pose — a T-pose).
    // The DRAW side is already compensated (BandDirector::DrawShowing draws
    // mCurWorld every frame); this is the missing POLL twin, placed right after
    // TheUI.Poll() so the game's song clock (TheTaskMgr) is current for the
    // director's mPropAnim->SetFrame(songTime*30) + char-clip advance.
    // BandDirector::Poll is internally gated on unke5 (EnableWorldPolling), so it is
    // a no-op outside active gameplay. Opt-out: RB3_VENUE_POLL_OFF=1.
    {
        static int sVenuePollOff = -1;
        if (sVenuePollOff < 0)
            sVenuePollOff = getenv("RB3_VENUE_POLL_OFF") ? 1 : 0;
        if (!sVenuePollOff && TheBandDirector)
            TheBandDirector->Poll();
    }
    // MUSIC-LIBRARY POLL FIX: the native frame loop (App::RunWithoutDebugging's
    // HX_NATIVE branch) delegates per-frame work to RunOneFrame and never reaches
    // the retail `inclusive_ui_poll` block (RunWithoutDebugging retail path,
    // below) that runs TheMusicLibrary->Poll() every frame. MusicLibrary::Poll
    // drives CheckSongPreview() — the song_select hover->preview state machine
    // (start timer on highlight change, then SongPreview::Start/Poll ->
    // TheSynth->NewStream) — plus the net-setlist art loader. Without this poll
    // the preview timer starts but is never checked, so the song-select AUDIO
    // PREVIEW never fires (silent hover). Runs before TheSynth->Poll() below so a
    // preview stream created this frame is decoded/refilled the same frame.
    // Null-guarded: TheMusicLibrary is only created once song_select is entered
    // (MusicLibrary::Init). Opt-out: RB3_NO_LIBRARY_POLL=1.
    {
        static int sNoLibPoll = -1;
        if (sNoLibPoll < 0)
            sNoLibPoll = getenv("RB3_NO_LIBRARY_POLL") ? 1 : 0;
        if (!sNoLibPoll && TheMusicLibrary)
            TheMusicLibrary->Poll();
    }
#endif
    RB3GameInputPoll(frame);
    TheTaskMgr.Poll();
    if (TheSynth)
        TheSynth->Poll();

#ifdef __EMSCRIPTEN__
    // Web audio pump (W3c wires the real AudioDevice_Web ring-buffer push; this
    // is a no-op until then). Native desktop pumps audio via miniaudio's own
    // callback thread, so this is web-only. Matches DC3's RunOneFrame.
    AudioDevice::GetInstance().PumpAudio();
#endif

    if (TheRnd)
        TheRnd->BeginDrawing();
#if defined(HX_NATIVE) && !defined(__EMSCRIPTEN__)
    // Native desktop: guard Draw() with a SIGSEGV longjmp so a partially-loaded
    // scene that segfaults skips the frame instead of killing the process. POSIX
    // signals don't exist under emcc, so on web we call Draw() directly — the
    // per-frame try/catch in main_web.cpp is the analogous safety net.
    extern sigjmp_buf gDrawJmpBuf;
    extern bool gDrawJmpBufSet;
    if (sigsetjmp(gDrawJmpBuf, 1) == 0) {
        gDrawJmpBufSet = true;
        TheUI.Draw();
        gDrawJmpBufSet = false;
    } else {
        gDrawJmpBufSet = false;
        MILO_LOG("RB3 Native: caught crash in Draw(), skipping frame %d\n", frame);
    }
#else
    TheUI.Draw();
#endif
    if (TheRnd)
        TheRnd->EndDrawing();

    // ── SESSION-TELEMETRY frame tap (record side) ────────────────────────────
    // Matches the entry tap above. Reads the frame ms (this RunOneFrame body
    // under the timer), the live screen name, and the pending-loader count, sets
    // the envelope song-ms (null-guarded chain via RB3TraceCurrentSongMs — -1 in
    // menus), then records the fr row (RB3RecordFrame applies the §4.7 decimation
    // + reads/zeroes the engine ld/st counters). Also lazily registers the nav
    // sink now that TheBandUI is alive (idempotent backstop; native main also
    // registers it right after the App ctor). All gated behind gRB3TraceActive.
    if (gRB3TraceActive) {
        extern float RB3TraceCurrentSongMs();
        extern void RB3TraceEnsureNavSink();
        rb3FrameTimer.Split();
        float rb3FrameMs = Timer::CyclesToMs(rb3FrameTimer.mCycles);
        UIScreen *rb3Scr = TheUI.CurrentScreen();
        const char *rb3ScrName = (rb3Scr && rb3Scr->Name()) ? rb3Scr->Name() : "?";
        // SIM-DT capture (replay seam 1 source): the menu/UI clock advance this
        // frame in SECONDS, from the TheTaskMgr.mTime accumulated-cycles delta
        // (TaskMgr::Poll did mTime.Split() at line ~590). This is the exact value
        // RB3_REPLAY_FIXED_CLOCK replays in Task.cpp seam 1. First frame / wrap
        // produces a non-positive delta, which we clamp to 0 (omitted on the wire).
        static double sRb3LastSimMs = 0.0;
        static bool   sRb3HaveSimMs = false;
        double rb3SimMsNow = (double)Timer::CyclesToMs(TheTaskMgr.mTime.mCycles);
        float rb3SimDt = 0.0f;
        if (sRb3HaveSimMs) {
            double d = rb3SimMsNow - sRb3LastSimMs;
            if (d > 0.0) rb3SimDt = (float)(d / 1000.0);
        }
        sRb3LastSimMs = rb3SimMsNow;
        sRb3HaveSimMs = true;
        float rb3SongMs = RB3TraceCurrentSongMs();
        RB3TraceSetSimDt(rb3SimDt);
        RB3TraceSetSongMs(rb3SongMs);
        // PER-FRAME CLOCK SAMPLE (M4): emit a tiny, UN-decimated clk{f,sdt,sm}
        // EVERY frame so RB3_REPLAY_FIXED_CLOCK feeds the EXACT recorded song-ms /
        // sim-dt at each frame N. The fr row below stays decimated (§4.7), so its
        // {sdt,sm} only survive at ~1 Hz and carry forward stale; the clk stream is
        // what frame-locks the fixed-clock replay's song clock to the recording.
        RB3RecordClock(rb3SimDt, rb3SongMs);
        RB3RecordFrame(rb3FrameMs, gLoadPollMsThisFrame, gLoadPollUntilMsThisFrame,
                       rb3ScrName, (int)TheLoadMgr.mLoading.size());
        RB3TraceEnsureNavSink();
    }
}
#endif

void App::DrawRegular() {
    if (ThePlatformMgr.mHomeMenuWii->mHomeMenuActive)
        ThePlatformMgr.Draw();
    else {
        TIMER_ACTION("begin_draw", TheRnd->BeginDrawing())
        TIMER_ACTION("ui_draw", TheUI.Draw())
        TIMER_ACTION("platform_draw", ThePlatformMgr.Draw())
        TIMER_ACTION("end_draw", TheRnd->EndDrawing())
    }
}

void App::CaptureHiRes() {
    bool notPaused = false;
    if (TheGame && !TheGame->mIsPaused)
        notPaused = true;
    if (notPaused)
        TheGame->SetPaused(true, true, true);
    DrawRegular();
    int max = TheHiResScreen.mTiling * TheHiResScreen.mTiling;
    for (int i = 0; i <= max; i++) {
        DrawRegular();
        TheHiResScreen.Accumulate();
    }
    TheHiResScreen.Finish();

    if (notPaused)
        TheGame->SetPaused(false, true, true);
}

void App::Draw() {
    if (TheHiResScreen.mActive)
        CaptureHiRes();
    else
        DrawRegular();
}

float gAvg;
float gSyncAvg;
float gTempThreshHigh = 16.8f;
float gTempThresh = gTempThreshHigh - 2.84f;
float gSleepAmt;
float gTempTimes[10240];
int gTempTimesIdx;
bool gPreventTriFrameSwitchage;

#pragma push
#pragma pool_data off
void PollTriFrame(float frameMs, float syncMs) {
    static const DataNode &venue_test = DataVariable("venue_test");
    if (venue_test == DataNode(1)) {
    } else {
        static float times[6];
        static int count;
        static float syncTimes[6];
        static int syncCount;
        static int trycount;

        syncTimes[(count + 1) % 6] = syncMs;
        gTempTimes[gTempTimesIdx] = frameMs;
        times[count % 6] = frameMs;
        gTempTimesIdx = (gTempTimesIdx + 1) % 10240;
        count++;
        syncCount++;
        gAvg = (times[0] + times[1] + times[2] + times[3] + times[4] + times[5]) / 6.0f;
        gSyncAvg = (syncTimes[0] + syncTimes[1] + syncTimes[2] + syncTimes[3]
                    + syncTimes[4] + syncTimes[5])
            / 6.0f;
        if (gSleepAmt > 0.0f) {
            OSSleepTicks(OSMicrosecondsToTicks((s64)(int)(1000.0f * gSleepAmt)));
        }
#ifndef HX_NATIVE
        if (!gPreventTriFrameSwitchage) {
            if (TheBandDirector->IsMusicVideo()) {
                TheWiiRnd.SetTriFrameRendering(false);
                WiiEnviron::mbEnableShadows = false;
            } else {
                // WiiRnd's tri-frame-rendering-enabled flag. This member (+0x149)
                // isn't mapped in rndwii/Rnd.h yet, so read it through the object
                // base to match the target.
                u8 triFrameOn = reinterpret_cast<u8 *>(&TheWiiRnd)[0x149];
                if (gCooldown++ < 8) {
                    trycount = 0;
                } else if (triFrameOn) {
                    if (gAvg < gTempThresh) {
                        if (++trycount > 6) {
                            TheWiiRnd.SetTriFrameRendering(false);
                            gCooldown = 0;
                            trycount = 0;
                        }
                    } else if (gSyncAvg > 250.0f) {
                        trycount--;
                    }
                } else if (gSyncAvg > 250.0f) {
                    if (++trycount > 6) {
                        TheWiiRnd.SetTriFrameRendering(true);
                        gCooldown = 0;
                        trycount = 0;
                    }
                }
            }
        }
#endif // HX_NATIVE (Wii GX tri-frame rendering throttle — no native GX)
    }
}
#pragma pop

void App::Run() { RunWithoutDebugging(); }

#pragma push
#pragma pool_data off
void App::RunWithoutDebugging() {
#ifdef HX_NATIVE
    // Native headless frame loop — mirrors dc3 App.cpp:1058-1156 HX_NATIVE branch
    // (RB3 has NO TheFlowMgr, so no flow poll). Bounded by MILO_MAX_FRAMES so the
    // headless run terminates; the draw is wrapped in a sigsetjmp guard so a
    // partially-loaded scene that segfaults in Draw() skips the frame instead of
    // crashing the process. Renders through TheRnd (= BandRnd, the Strategy-B
    // backend wired in native/src/rb3_band_rnd.cpp).
    // Embedded HTTP debug server (rb3_http_server.cpp). The Poll hooks are
    // no-ops unless RB3_HTTP=1 started the server (TheRB3HttpServer != null), so
    // a normal run is unaffected. ProcessCommands drains DTA-eval / input verbs
    // on this (main) thread; ProcessScreenshots reads back AFTER EndDrawing.
    extern void RB3HttpServerPoll(int frame);
    extern void RB3HttpServerPollScreenshots();
    // Start the HTTP debug server HERE (after the App ctor completed), not in
    // RunGame() — mirrors dc3 App.cpp:1070. Starting its background thread during
    // the heavy boot/ctor caused a boot-time SIGSEGV. No-op unless RB3_HTTP=1.
    extern void RB3HttpServerInit();
    extern void RB3HttpServerShutdown();
    RB3HttpServerInit();

    int maxFrames = 5;
    bool unbounded = false;
    if (const char *e = getenv("MILO_MAX_FRAMES")) {
        maxFrames = atoi(e);
        if (maxFrames <= 0) maxFrames = 5;
    } else if (getenv("RB3_HTTP")) {
        // HTTP keep-alive workflow: with the debug server on and no frame cap,
        // run indefinitely so an external client can drive the live instance.
        unbounded = true;
    }
    MILO_LOG("RB3 Native: entering frame loop — %s\n",
             unbounded ? "unbounded (RB3_HTTP keep-alive)" : "bounded");

    // C3: a `quit` input verb (rb3_game_input.cpp) sets this; breaking the loop
    // here returns from RunWithoutDebugging() so App::~App()'s TheDebug.Exit(0)
    // fires the exit-callback chain (incl. RB3SaveSaveGlobalOptions), giving an
    // HTTP-driven clean exit that persists state — unlike SIGTERM/SIGKILL.
    extern bool RB3CleanExitRequested();

    // TRACK-B load-perf instrumentation (env-gated, native-only). With
    // RB3_FRAME_INSTRUMENT=1 every RunOneFrame is wall-timed; frames over a
    // threshold (default 20ms) are logged with their wall ms so the long-frame
    // load tail is visible, and a running max + over-threshold count are kept.
    // This mirrors the web freeze (one RunOneFrame on web blocks the browser
    // exactly as long as it blocks this native frame loop — same CPU work), so a
    // collapse of the native tail predicts a smooth web tab. Zero cost when off.
    static int sFrameInstrument = -1;
    static float sLongFrameThreshMs = 20.0f;
    if (sFrameInstrument < 0) {
        sFrameInstrument = getenv("RB3_FRAME_INSTRUMENT") ? 1 : 0;
        if (const char *t = getenv("RB3_FRAME_INSTRUMENT_THRESH_MS"))
            if (t[0]) sLongFrameThreshMs = (float)atof(t);
    }
    float sMaxFrameMs = 0.0f;
    int sLongFrameCount = 0;

    // SESSION-TELEMETRY frame-trace (RB3_SESSION_TRACE / RB3_FRAME_TRACE) is now
    // emitted by the tap INSIDE RunOneFrame (so one tap serves both the native
    // loop and the web RunOneFrame driver). This loop no longer records `fr`
    // rows — that would double-emit. RB3FrameTraceRecord stays a back-compat
    // symbol in rb3_session_trace.cpp; it is simply not called here anymore.
    // Only RB3_FRAME_INSTRUMENT (the human-readable LONG-frame MILO_LOG) still
    // needs the loop's own wall-clock timing below.
    const bool wallTime = (sFrameInstrument != 0);

    for (int frame = 0; (unbounded || frame < maxFrames) && !RB3CleanExitRequested();
         frame++) {
        // The core poll + draw (SystemPoll → UI.Poll → RB3GameInputPoll →
        // TaskMgr.Poll → Synth.Poll → BeginDrawing → sigsetjmp-guarded UI.Draw →
        // EndDrawing) lives in RunOneFrame, shared verbatim with the web boot.
        // The HTTP debug server hooks are native-desktop-only and bracket it
        // exactly as before: ProcessCommands ran right after RB3GameInputPoll
        // (so HTTP-injected verbs land on the NEXT frame's RB3GameInputPoll
        // drain), and the screenshot readback runs after EndDrawing.
        if (wallTime) {
            extern float gLoadPollMsThisFrame;
            extern float gLoadPollUntilMsThisFrame;
            gLoadPollMsThisFrame = 0.0f;
            gLoadPollUntilMsThisFrame = 0.0f;
            Timer frameTimer;
            frameTimer.Restart();
            RunOneFrame(frame);
            frameTimer.Split();
            float ms = Timer::CyclesToMs(frameTimer.mCycles);
            if (ms > sMaxFrameMs) sMaxFrameMs = ms;
            UIScreen *scr = TheUI.CurrentScreen();
            const char *scrName = (scr && scr->Name()) ? scr->Name() : "?";
            if (sFrameInstrument && ms > sLongFrameThreshMs) {
                sLongFrameCount++;
                MILO_LOG("RB3 FRAME-INSTRUMENT: frame %d LONG %.1f ms "
                         "(poll=%.1f pollUntil=%.1f screen=%s) "
                         "[max=%.1f longCount=%d]\n",
                         frame, ms, gLoadPollMsThisFrame, gLoadPollUntilMsThisFrame,
                         scrName, sMaxFrameMs, sLongFrameCount);
            }
        } else {
            RunOneFrame(frame);
        }
        RB3HttpServerPoll(frame);
        RB3HttpServerPollScreenshots();
        // Frame-degradation fix: periodically trim the glibc heap so in-song RSS
        // does not ratchet up from arena fragmentation (the per-frame SFX/object
        // churn). Native-only TU (rb3_heap_maint_native.cpp); cadenced + opt-out
        // via RB3_HEAP_TRIM_OFF. No-op on web (wasm heap can't return to host).
        extern void RB3NativeHeapMaintenance(int frame);
        RB3NativeHeapMaintenance(frame);
        MILO_LOG("RB3 Native: frame %d complete\n", frame);
    }
    MILO_LOG("RB3 Native: %d frames done — exiting frame loop\n", maxFrames);
    // N9 (teardown SIGSEGV): quiesce the audio RT thread BEFORE Debug::Exit
    // fires the exit-callback chain. AudioDevice::Suspend() sets mSuspended and
    // takes mSourceMutex, guaranteeing MixSources is not mid-flight when
    // SynthTerminate (push_front head, runs first) calls TheSynth->Poll() and
    // then AudioDevice::Terminate() -> ma_device_uninit. Without this, the
    // PipeWire/ALSA RT callback thread can be executing PipeWire SPA code
    // that the process is about to unmap — the exact fault seen in N9 coredumps
    // (thread in ma_device_audio_thread__default_read_write in PipeWire mmap pages).
    // Suspend() is a no-op when audio was skipped (mInitialized=false path is
    // fine — mSuspended is still set, Terminate/SynthTerminate see it harmlessly).
    AudioDevice::GetInstance().Suspend();
    RB3HttpServerShutdown();
    return;
#endif
    Timer loop_timer;
    loop_timer.Restart();
    int frameticker = 0;
    while (true) {
        if (gCheckConsistencyish != 0 && (frameticker % (gCheckConsistencyish * 4 - 3)) == 0) {
                OSReport("checking consistency %d...\n", gCheckConsistencyish);
                MemCheckConsistency(__FILE__, 864);
            }
        frameticker++;
        SetGPHangDetectEnabled(false, __FUNCTION__);
        TIMER_ACTION("poll", {
            TIMER_ACTION("system_poll", SystemPoll(false))
            TIMER_ACTION("inclusive_ui_poll", {
                CheckForPassivePlatformErrors();
                TheUIStats->Poll();
                TheAchievements->Poll();
                TheAccomplishmentMgr->Poll();
                PrefabMgr::GetPrefabMgr()->Poll();
                TheSaveLoadMgr->Poll();
                TheProfileMgr.Poll();
                TheMusicLibrary->Poll();
                UpdateStoreOverlay();
            })
            TIMER_ACTION("synth_poll", TheSynth->Poll())
            TIMER_ACTION("net_poll", {
                TheNet.Poll();
                TheRockCentral.Poll();
                TheEntityUploader.Poll();
            })
            TIMER_ACTION("inclusive_ui_poll", TheUI.Poll())
            TheTaskMgr.Poll();
        })
        SetGPHangDetectEnabled(1, __FUNCTION__);
        Draw();

        float f = loop_timer.SplitMs();
        loop_timer.Restart();
        PollTriFrame(gTriFrameTimer.mLastMs, f);
    }
}
#pragma pop

// Drain any pending platform-error DataArrays (queued by the Wii content
// manager on a disc/NAND failure) into the passive-message UI, then clear the
// queue. Wii-only: gPlatformErrorMsg lives in the Wii content manager, which is
// excluded from the native build (the loop never runs there anyway — native
// returns early from RunWithoutDebugging before the poll path that calls this).
static void CheckForPassivePlatformErrors() {
#ifndef HX_NATIVE
    std::list<DataArrayPtr>::iterator it = gPlatformErrorMsg.begin();
    while (it != gPlatformErrorMsg.end()) {
        ThePassiveMessenger->TriggerMessage(
            *it, (PassiveMessageType)0, nullptr, false, gNullStr,
            0x800, 0, 0, 0, 0, gNullStr, gNullStr, gNullStr, 0
        );
        it = gPlatformErrorMsg.erase(it);
    }
#endif
}
