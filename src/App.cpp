#include "App.h"
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
#include "utl/Magnu.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Rso_Utl.h"
#include "utl/Option.h"
#include "world/World.h"
#ifndef HX_NATIVE
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
u64 sNullMicClientID;
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

#ifdef VERSION_SZBE69
#pragma push
#pragma dont_inline on
#endif
void AppDebugModal(bool &b, char *abc, bool b2) {
    if (!b) {
        static DataNode &notify_level = DataVariable("notify_level");
        int notif_lvl = notify_level.Int();
        if (notif_lvl == 2) {
            gRealCallback(b, abc, b2);
            return;
        } else if (notif_lvl == 1) {
            Hmx::Object *disp = ObjectDir::sMainDir->FindObject("cheat_display", false);
            if (disp) {
                static Message show("show_prio", 0, 0);
                show[0] = DataNode(abc);
                show[1] = DataNode(200);
                disp->Handle(show, 0);
            } else
                goto asdf;
        } else {
        asdf:
            MILO_LOG("%s\n", abc);
        }
    } else
        gRealCallback(b, abc, b2);
}

App::App(int argc, char **argv) {
    static const int kESRBMs = 4000;
    static const int kRegularSplashMs = 4000;
    Timer init_time;
    init_time.Start();
    InitMakeString();
    class String s;
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
#ifdef __EMSCRIPTEN__
#define WEB_BOOT_MARK(s) printf("RB3 Web boot: %s\n", s)
#else
#define WEB_BOOT_MARK(s) ((void)0)
#endif
    WEB_BOOT_MARK("loading sound bank (common)");
#ifndef __EMSCRIPTEN__
    {
        ObjDirPtr<ObjectDir> oPtr;
        Loader *ldr = nullptr;
        oPtr.LoadFile(
            SystemConfig("sound", "banks", "common")->Str(1), 0, 1, kLoadFront, 0
        );
        TheSynth->SetUnk40(oPtr.Ptr());
        PollTheSplasher();
    }
#else
    // Web (W3a, audio-free): SKIP the common sound bank load. Synth.cpp /
    // VorbisReader.cpp are excluded from the web build, so the synth leaf
    // factories (Sfx / SynthSample / SynthFader / FxSendEQ / *GroupSeq) are not
    // registered. DirLoader skips them ("Can't make ..."), but the bank's
    // remaining registered objects enter a PreLoad/PostLoad path that needs the
    // synth subsystem state that never boots — an un-interruptible spin (the W2
    // "synth sample-read path" wall, now reached via the App ctor). The menu
    // renders fine without SFX; W3c recovers the synth + re-enables this load.
    // TheSynth is null here anyway, so SetUnk40 is a no-op we also skip.
    WEB_BOOT_MARK("sound bank SKIPPED on web (audio-free W3a)");
#endif
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
    // BandOffline::Init()
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
    // InitStoreOverlay();
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
float gTempThresh;
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
                u8 triFrameOn = *(reinterpret_cast<u8 *>(&TheWiiRnd) + 0x149);
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

    for (int frame = 0; (unbounded || frame < maxFrames) && !RB3CleanExitRequested();
         frame++) {
        // The core poll + draw (SystemPoll → UI.Poll → RB3GameInputPoll →
        // TaskMgr.Poll → Synth.Poll → BeginDrawing → sigsetjmp-guarded UI.Draw →
        // EndDrawing) lives in RunOneFrame, shared verbatim with the web boot.
        // The HTTP debug server hooks are native-desktop-only and bracket it
        // exactly as before: ProcessCommands ran right after RB3GameInputPoll
        // (so HTTP-injected verbs land on the NEXT frame's RB3GameInputPoll
        // drain), and the screenshot readback runs after EndDrawing.
        RunOneFrame(frame);
        RB3HttpServerPoll(frame);
        RB3HttpServerPollScreenshots();
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

// Stub: original implementation (.text:0x80010420, size 0xF8) not yet decompiled.
// Empty body unblocks the link; behavior diff is a no-op poll path.
static void CheckForPassivePlatformErrors() {}
