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
#include <cstring>
#include <cstdlib>

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
      mPartyMicAllowed(false), mEnableSFX(false), unkce5a(false),
      mEnumerateFriendsCallback(0), mSendMsgCallback(0), mSignInUserCallback(0),
      mIsOnlineRestricted(true), unkce69(false), mIgnorePowerOperations(false) {
    // HomeMenu / DiscErrorMgrWii ctors are Wii-SDK-bound. Give mHomeMenuWii a
    // zeroed block so HomeMenuActive() and the compiled mHomeMenuWii-> reads are
    // safe (all-false); leave mDiscErrorMgr null (its sole boot deref is gated).
    mHomeMenuWii = static_cast<HomeMenu *>(::calloc(1, sizeof(HomeMenu)));
    mDiscErrorMgr = nullptr;
    mProfaneWord = 0;
    ClearNetError();
}

PlatformMgr::~PlatformMgr() {
    ::free(mHomeMenuWii);
    mHomeMenuWii = nullptr;
}

// RegionInit() runs on the native SystemPreInit spine (os/System.cpp:359). The
// Wii body reads the console's SC region; native has none. Default to NA so the
// "region has not been initialized" notify clears and region-keyed DTA resolves.
void PlatformMgr::RegionInit() { SetRegion(kRegionNA); }

// --- TheContentMgr: construct the real base ContentMgr global ---
// `ContentMgr *TheContentMgr;` and its `= &TheWiiContentMgr` static initializer
// both live in the EXCLUDED os/ContentMgr_Wii.cpp:70-74, so natively TheContentMgr
// is null → BandSongMgr::Init's `TheContentMgr->RegisterCallback(this,false)`
// (meta_band/BandSongMgr.cpp:63) and System.cpp's PreInit/Init faulted on null.
// The base ContentMgr (os/ContentMgr.cpp, COMPILED) is fully offline-safe: every
// virtual has a no-op/true default — StartRefresh() no-ops, RefreshDone() →
// mState==kDiscoveryEnumerating, NeverRefreshed() → mState==kDone, IsMounted/
// MountContent → true. With no Wii NAND content this is exactly the
// "nothing to refresh, refresh trivially done" behavior DTA_MANAGER_STUBS.md
// specifies. Point TheContentMgr at a real base ContentMgr (mirrors the Wii TU's
// _theContentMgrInit). Static-init order is safe: the ctor is trivial and no
// other static initializer touches TheContentMgr before main(); SetName/Init is
// driven later from the native SystemInit (os/System.cpp), where Main() exists.
ContentMgr *TheContentMgr = new ContentMgr();

#endif // HX_NATIVE
