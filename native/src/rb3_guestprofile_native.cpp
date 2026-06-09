#ifdef HX_NATIVE
// rb3_guestprofile_native.cpp — OPT-IN native HACK (RB3_GUEST_PROFILE=1): fake a
// valid pad-0 "guest" profile so the customize / manage-band flows are reachable
// WITHOUT a real sign-in.
//
// HACK/TODO(guest-profile): real native sign-in (PlatformMgr signin +
// WiiProfileMgr NAND load, via decomp of PlatformMgr_Wii/WiiProfileMgr or a
// from-scratch reimplementation) is tracked as roadmap item C11
// (docs/native/NATIVE_PORT_ROADMAP.md). This hack populates the REAL
// TheWiiProfileMgr + TheProfileMgr state for pad 0 only, so it exercises the
// SAME real engine code paths the disc uses (LocalUser::CanSaveData /
// ProfileMgr::GetProfileForUser / Profile::HasValidSaveData) rather than
// short-circuiting them. The whole TU is HX_NATIVE-only and lives only on the
// native/web link line, so no shared/Wii decomp code changes.
//
// WHY OPT-IN (not default-on as first planned): enabling the guest profile makes
// a PRIMARY profile EXIST, which wakes a cascade of profile-driven paths that
// crash on hollow profile data / zeroed offline stubs. Confirmed by negative
// control (boot-to-gameplay is clean with this OFF, crashes with it ON):
//   1. MainHubPanel::ReloadMessages -> TheServer.GetPlayerID(): TheServer was a
//      zeroed stub -> FIXED by rb3_server_native.cpp (faithful offline Server).
//   2. RndMat::SyncProperty -> PropSync<RndTex> (material patch-texture sync):
//      still open; more dominoes likely.
// Until that cascade is fully resolved (roadmap C11 deeper work), this stays
// default-OFF so the native+web build boots clean. Set RB3_GUEST_PROFILE=1 to
// exercise/iterate it.
//
// WHY THIS ALONE DOES NOT make the closet show a character: the customize preview
// characters from world/shared/chars.milo are bodyless shells (no FileMerger /
// outfit children), so even with a valid profile the closet renders nothing until
// the body-source work lands (roadmap C13).
#include "os/PlatformMgr.h"
#include "meta/WiiProfileMgr.h"
#include "meta/Profile.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/BandProfile.h"
#include "os/Debug.h"
#include <cstdlib>

// Idempotent: safe to call every frame; only acts once.
void RB3InstallGuestProfile() {
    static bool sDone = false;
    if (sDone)
        return;
    // OPT-IN (default OFF): only install when RB3_GUEST_PROFILE is set. See the
    // header — default-on cascades into profile-driven crash paths.
    if (const char *e = ::getenv("RB3_GUEST_PROFILE"); !(e && e[0] && e[0] != '0')) {
        sDone = true;
        return;
    }
    const int kPad = 0;
    // (1) WiiProfileMgr.cpp IS compiled natively (src/system/meta is globbed;
    //     strong defs win over the stale weak stubs in dta_link_stubs.s), but its
    //     Init is gated out of the native boot. Init it now so mWiiProfiles[] /
    //     mPadProfileIndex[] reach a defined state. Init(rev, revWii) -> Clear ->
    //     all indices -1. (Revs mirror what a Wii boot would have used.)
    TheWiiProfileMgr.Init(151, 45);
    // (2) Bind pad 0 -> index 0, then CreateProfile(0) -> SetIndexValid(0, true)
    //     -> WiiProfileMgr::IsIndexValid(GetIndexForPad(0)) now returns true. This
    //     is the real API DoSignin uses. SetPadToIndex's PlatformMgr.SetUserSignedIn
    //     tail is a native no-op (excluded PlatformMgr_Wii.cpp), so we set the
    //     signin mask ourselves in step (3).
    TheWiiProfileMgr.SetPadToIndex(kPad, /*idx=*/kPad);
    TheWiiProfileMgr.CreateProfile(/*idx=*/kPad);
    // (3) Offline signin mask, pad 0 ONLY (scoped). The live recompute lives in
    //     PlatformMgr_Wii.cpp (no-op natively), so this stays set. IsUserAGuest
    //     already returns false, so CanSaveData()'s !IsGuest() is satisfied.
    ThePlatformMgr.mSigninMask |= (1 << kPad);
    // (4) Flip the pad-0 BandProfile to "loaded" so Profile::HasValidSaveData()'s
    //     state check passes (it also re-checks WiiProfileMgr.IsIndexValid, now
    //     true). The 4 BandProfile objects already exist (ProfileMgr::Init). A
    //     freshly-flipped profile has empty score/award/char data, so downstream
    //     readers (autosave gated off via unactivated SaveLoadMgr; accomplishments
    //     read empty -> no awards) stay quiet.
    if (BandProfile *p = TheProfileMgr.GetProfileFromPad(kPad)) {
        if (p->GetSaveState() != kMetaProfileLoaded)
            p->SetSaveState(kMetaProfileLoaded);
    }
    MILO_LOG("RB3 native HACK: installed pad-0 guest profile (signinMask|=1, "
             "WiiProfileMgr idx0 valid, BandProfile[0]=Loaded)\n");
    sDone = true;
}
#endif // HX_NATIVE
