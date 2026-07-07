// rb3_platform_native.cpp — native (clang LP64) definitions for the Wii/online
// manager GLOBALS whose real ctors live in platform-EXCLUDED Wii TUs.
//
// THE PROBLEM (the systematic T4 crux, per BOOT_TO_SONG.md / DTA_MANAGER_STUBS.md):
//   RB3 derefs Wii/net manager globals (ThePlatformMgr, TheContentMgr, …) directly.
//   Their ctors are defined in EXCLUDED Wii TUs (e.g. PlatformMgr::PlatformMgr() is
//   in os/PlatformMgr_Wii.cpp:125, which #includes revolution/* and so cannot
//   compile under clang). The static initializer for `PlatformMgr ThePlatformMgr;`
//   (os/PlatformMgr.cpp:11) therefore resolves to a WEAK no-op stub ctor
//   (dta_link_stubs.s) → the global is ZEROED, never constructed → its MsgSource
//   base (the mSinks std::list) is garbage → BandUserMgr ctor's
//   `ThePlatformMgr.AddSink(this, signin_changed)` (game/BandUserMgr.cpp:58) faults
//   in MsgSource::RemoveSink walking the garbage list.
//
// THE FAITHFUL FIX (Wii-SDK-bound ctor → minimal native glue ctor):
//   We can't compile PlatformMgr_Wii.cpp (Wii SDK), but we CAN provide a minimal,
//   offline-safe PlatformMgr::PlatformMgr() / ~PlatformMgr() body here. The
//   compiler emits the base-class (MsgSource / ContentMgr::Callback) ctor calls
//   for us, so the MsgSource base — including mSinks — is properly constructed.
//   AddSink/RemoveSink then walk a valid (empty) list. The bodies mirror the Wii
//   ctor's offline-meaningful field inits (all signed-out, no net, no disc error).
//   These are STRONG defs; they win over the weak stubs in dta_link_stubs.s. (The
//   rb3-dta target keeps the weak stubs since this TU is only on the rb3-native
//   link line.) Mirrors how CreateNativeSynth replaced its weak stub.
//
// Wii-only side fields (mHomeMenuWii / mDiscErrorMgr) point at HomeMenu /
// DiscErrorMgrWii whose ctors are likewise Wii-SDK-bound. Several COMPILED TUs
// deref `ThePlatformMgr.mHomeMenuWii->mHomeMenuActive` (synth/MetaMusic.cpp,
// meta_band/OvershellPanel.cpp, …) and `HomeMenuActive()` (PlatformMgr.h:133).
// To keep those reads offline-safe (and the occasional write, e.g.
// SaveLoadStatusPanel `mHomeMenuWii->unk_0x9 = false`) we hand mHomeMenuWii a
// ZEROED block sized to the real HomeMenu — no Wii ctor runs, every bool reads
// false, the embedded std::list is never iterated natively (RegisterCallback is
// Wii-only). mDiscErrorMgr stays null (App.cpp:161's mActive write is already
// #ifndef HX_NATIVE-gated; GetDiscErrorMgrWii() callers are off the boot path).
#ifdef HX_NATIVE

#include "os/PlatformMgr.h"
#include "os/HomeMenu_Wii.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "platform/NativeCompatFlags.h" // NativeCompat read-once flag registry (W0.6)
#include "meta_band/BandSongMgr.h"
#include "obj/DataFile.h"
#include "obj/Data.h"
#include "utl/Locale.h"    // Localize() free function (W4.5-GAMERTAG)
#include "utl/Symbols4.h"  // extern Symbol player;
#include "utl/MakeString.h"
#include <cstring>
#include <cstdlib>
#include <string>

// --- PlatformMgr: minimal native ctor/dtor so the MsgSource base constructs ---
// Field inits mirror PlatformMgr_Wii.cpp:125 (offline defaults: signed out, no
// net, no disk error, online-restricted like a fresh boot).
PlatformMgr::PlatformMgr()
    : mSigninMask(0), mSigninChangeMask(0), mGuideShowing(false), mConnected(false),
      unk2a(false), mScreenSaver(false), mRegion(kRegionNone),
      mDiskError(kNoDiskError), mTimer(), unk6a(false), unk6d(false), unk70(0),
      unk68(false), unk69(false), unk6b(false), unk6c(false),
      mEthernetCableConnected(false), unk43a0(false), unk43a1(false),
      unk43a2(false), unk43a3(false), mCheckingProfanity(false), unkca11(false),
      mProfanityAllowed(true), unkca14(0), mHomeMenuDisabled(0),
      mNetworkPlay(false), unkce56(false), mIsRestarting(false),
      mPartyMicAllowed(false), mEnableSFX(true), unkce5a(false),
      mEnumerateFriendsCallback(0), mSendMsgCallback(0), mSignInUserCallback(0),
      mIsOnlineRestricted(true), unkce69(false), mIgnorePowerOperations(false) {
    // HomeMenu / DiscErrorMgrWii ctors are Wii-SDK-bound. Give mHomeMenuWii a
    // zeroed block so HomeMenuActive() and the compiled mHomeMenuWii-> reads are
    // safe (all-false); leave mDiscErrorMgr null (its sole boot deref is gated).
    mHomeMenuWii = static_cast<HomeMenu *>(::calloc(1, sizeof(HomeMenu)));
    mDiscErrorMgr = nullptr;
    mProfaneWord = 0;
    ClearNetError();

    // SFX one-shot samples (menu blips/confirms, gameplay hits) are now decodable
    // natively via SynthSample::NewInst (rb3_sampleinst_native.cpp), so default to
    // loading their payloads — AreSFXEnabled() gates SampleData::Load. RB3_NO_SFX=1
    // restores the retail-disabled behavior (payloads seeked past → silent, lower
    // memory/load) for A/B or if a bank ships an unsupported (XMA/etc) format.
    // Truthy opt-in: RB3_NO_SFX set to a non-empty, non-"0" value restores the
    // retail-disabled-SFX behaviour. OptOutActive resolves the same truthy idiom
    // and returns the SFX-ENABLED state (!truthy) — identical to the prior
    // `e && e[0] && e[0] != '0' ? false : (init-list) true`.
    mEnableSFX = NativeCompat::Get().OptOutActive("RB3_NO_SFX");
}

PlatformMgr::~PlatformMgr() {
    ::free(mHomeMenuWii);
    mHomeMenuWii = nullptr;
}

// RegionInit() runs on the native SystemPreInit spine (os/System.cpp:359). The
// Wii body reads the console's SC region; native has none. Default to NA so the
// "region has not been initialized" notify clears and region-keyed DTA resolves.
void PlatformMgr::RegionInit() { SetRegion(kRegionNA); }

// --- GetName: Wii "Player N" fallback for the missing native profile subsystem ---
// (W4.5-GAMERTAG / Wave-15 acceptance A5.) Native has no profile/gamertag
// backend: TheWiiProfileMgr is Wii-SDK-bound and not compiled here, so
// IsSignedIn(pad) (PlatformMgr.cpp) is always false natively -> on Wii this
// unconditionally takes the ELSE branch of PlatformMgr_Wii.cpp:489-496:
//   return MakeString("%s %d", Localize(player, 0), pad + 1);
// Without this override, GetName resolves to the WEAK no-op stub in
// dta_link_stubs.s (__hmx_tramp_dta_172, `xorl %eax,%eax; ret` — returns NULL).
// Every consumer that formats that result with "%s" therefore prints the
// literal string "(null)": AppLabel::SetUserName(int)
// (meta_band/AppLabel.cpp:161, the song_select header) and the overshell
// `user_name.lbl` player plate (OvershellSlot.cpp:96) both call
// ThePlatformMgr.GetName(pad) — so this one strong definition (it wins over
// the weak stub) fixes every consumer at once, matching the acceptance's
// "one provider fixes header + overshell + all consumers".
// Flag-gated (RB3_PLAYER_NAME_FALLBACK, default-OFF, presence-mode) so
// flag-OFF stays byte-identical to today's stub (returns nullptr, same as the
// trampoline). Localize() already degrades gracefully when the token/Locale
// table isn't available (Locale.cpp Localize() falls back to the literal
// token string "player", not null/empty) — satisfies "localized token if
// available, literal fallback otherwise" without any extra fallback logic.
const char *PlatformMgr::GetName(int pad) const {
    static int enabled = -1;
    if (enabled < 0)
        enabled = ::getenv("RB3_PLAYER_NAME_FALLBACK") ? 1 : 0;
    if (!enabled)
        return nullptr;
    return MakeString("%s %d", Localize(player, 0), pad + 1);
}

// --- TheContentMgr: native ContentMgr that scans the extracted song tree ---
// `ContentMgr *TheContentMgr;` and its `= &TheWiiContentMgr` static initializer
// both live in the EXCLUDED os/ContentMgr_Wii.cpp:70-74, so natively TheContentMgr
// is null → BandSongMgr::Init's `TheContentMgr->RegisterCallback(this,false)`
// (meta_band/BandSongMgr.cpp:63) and System.cpp's PreInit/Init faulted on null.
//
// The DTA-driven refresh contract (the real flow):
//   1. A DTA handler sends `{content_mgr start_refresh}` (game.dta:348/376/446/483,
//      main_hub.dta:102, meta_loading.dta:319, song_select.dta:1879, …).
//   2. Console ContentMgr::PollRefresh walks Wii NAND/DVD content sources, mounts
//      them, enumerates `.dta`s, dispatches them to its registered Callbacks
//      (`SongMgr` is one — registered in BandSongMgr::Init), and settles at
//      mState=kDiscoveryEnumerating with `{content_mgr refresh_done}` answering 1.
//   3. A second DTA handler gates progress on `{content_mgr refresh_done}` (game.dta
//      :356/384/454/485, song_select.dta:1878, meta_loading.dta:328, …) — once true
//      the DTA state machine advances.
//
// On native there is no disc/SD scan, but the SAME contract applies: the songs
// live under `$RB3_DATA/songs/`, and BandSongMgr::AddSongs(DataReadFile(songs.dta))
// is the inlet (it calls AddSongData + ContentDone, the same path the Callback's
// ContentDone hook would take). NativeContentMgr therefore overrides StartRefresh()
// to do exactly that: load songs/songs.dta, push it through TheSongMgr.AddSongs,
// and fire ContentDone() on all registered Callbacks (the same dispatch
// ContentMgr_Wii::PollRefresh runs at the end of a real disc scan). The state ends
// at kDiscoveryEnumerating so RefreshDone()=true and DTA gates flip.
//
// This replaces the imperative `TheSongMgr.AddSongs` hack that previously lived
// in rb3_game_input.cpp's RB3RegisterNativeManagerStubs() — content loading now
// flows through the real DTA `{content_mgr start_refresh}` channel.
class NativeContentMgr : public ContentMgr {
public:
    NativeContentMgr() : ContentMgr(), mRefreshed(false) {}

    // The real DTA-driven content refresh entry point. Called by:
    //   - ContentMgr::Handle({content_mgr start_refresh}) via HANDLE_ACTION (any DTA
    //     `{content_mgr start_refresh}` directive routes here).
    //   - PollRefresh's nested re-refresh on dirty+CanRefreshOnDone (matched-fork
    //     ContentMgr.cpp:188).
    // On Wii this would kick the ContentMgr_Wii enumeration state machine; on
    // native we synchronously read songs.dta, push it to BandSongMgr::AddSongs
    // (which itself calls ContentDone()), then dispatch ContentDone() to every
    // OTHER registered Callback (mirroring the matched-fork PollRefresh tail at
    // ContentMgr.cpp:190-196). Idempotent — repeated start_refresh calls (the
    // game does refresh on every screen entry) re-fire ContentDone but don't
    // re-load the DTA file.
    virtual void StartRefresh() {
        const char *dataRoot = ::getenv("RB3_DATA");
#ifdef __EMSCRIPTEN__
        // On web there is no RB3_DATA env var: the boot machine (main_web.cpp's
        // DoEngineInit) chdir's to /data, where WebAssets unpacked the MEMFS
        // bundle — including songs/songs.dta. Default the data root to /data so
        // song discovery finds the same songs.dta native reads via $RB3_DATA.
        // Without this the web song_select list is empty (the no-op branch
        // below). W3c-nav.
        if (!dataRoot)
            dataRoot = "/data";
#endif
        if (!dataRoot) {
            MILO_LOG("NativeContentMgr: RB3_DATA unset — refresh is a no-op\n");
            mState = kDiscoveryEnumerating;
            return;
        }
        // Enter the "refresh in progress" state (RefreshInProgress() → true) so
        // any DTA poll mid-refresh sees the right answer. ContentMgr.cpp's
        // PollRefresh uses kMounting(5) as the "post-mount, will-settle-next-poll"
        // state; we use it transiently for the same semantic.
        mState = kMounting;
        mDirty = false;

        if (!mRefreshed) {
            mRefreshed = true;
            // The engine has already chdir'd INTO the data root (main_native.cpp
            // RunBoot chdir($RB3_DATA); main_web.cpp DoEngineInit chdir(/data)),
            // and the matched-fork resolves every config/ + ui/ path RELATIVE to
            // that cwd. The songs DTA must use the SAME relative-to-cwd path —
            // prepending $RB3_DATA again double-counts when RB3_DATA is relative
            // (e.g. "orig-assets/extracted"): the resulting
            // "orig-assets/extracted/songs/songs.dta" misses (cwd is already the
            // data root) and DataReadFile returns an EMPTY array, so 0 songs ever
            // load. Use the cwd-relative "songs/songs.dta" — correct for both
            // native and web since both chdir into the data root first.
            const char *songsPath = "songs/songs.dta";
            (void)dataRoot;
            // DataReadFile parses a .dta file -> DataArray. The matched-fork
            // BandSongMgr::AddSongs path then calls AddSongData() + ContentDone()
            // (BandSongMgr.cpp:796), so a single AddSongs is exactly what a
            // post-mount per-file dispatch would produce.
            DataArray *songs = DataReadFile(songsPath, true);
            if (songs) {
                TheSongMgr.AddSongs(songs);
                std::vector<int> ranked;
                TheSongMgr.GetRankedSongs(ranked, false, false);
                MILO_LOG("NativeContentMgr: StartRefresh loaded %s — TheSongMgr now "
                         "has %d ranked songs\n", songsPath, (int)ranked.size());
                songs->Release();
            } else {
                MILO_LOG("NativeContentMgr: StartRefresh could not load %s (song "
                         "list will be empty)\n", songsPath);
            }
        }

        // Fire ContentDone on every registered Callback. BandSongMgr::AddSongs
        // already called its own ContentDone() (the song-rankings rebuild that
        // also feeds TheRockCentral.SyncAvailableSongs / TheSaveLoadMgr->AutoSave);
        // calling it again here would double-rank. The matched-fork PollRefresh
        // dispatches to EVERY callback at the end of a refresh — we mirror that
        // by walking the list and skipping &TheSongMgr (the SongMgr callback that
        // AddSongs already fed).
        ContentMgr::Callback *songMgrCb = static_cast<ContentMgr::Callback *>(&TheSongMgr);
        for (std::list<Callback *>::iterator it = mCallbacks.begin();
             it != mCallbacks.end();
             ++it) {
            if (*it && *it != songMgrCb)
                (*it)->ContentDone();
        }

        // Settle at the post-refresh idle state: RefreshDone()=true,
        // RefreshInProgress()/InDiscoveryState()=false, NeverRefreshed()=false.
        // This is the state the matched-fork ContentMgr_Wii::PollRefresh leaves
        // mState in after a successful enumeration (ContentMgr.cpp:154).
        mState = kDiscoveryEnumerating;
    }

private:
    bool mRefreshed;
};

// Point TheContentMgr at a NativeContentMgr instance. Static-init order is safe:
// the ctor is trivial (no DTA / Symbol / Main() touches) and no other static
// initializer touches TheContentMgr before main(); SetName/Init is driven later
// from the native SystemInit (os/System.cpp), where Main() exists.
ContentMgr *TheContentMgr = new NativeContentMgr();

#endif // HX_NATIVE
