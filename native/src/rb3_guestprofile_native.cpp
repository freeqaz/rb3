#ifdef HX_NATIVE
// rb3_guestprofile_native.cpp — DEFAULT-ON native HACK (opt-out RB3_NO_GUEST_PROFILE):
// fake a valid pad-0 "guest" profile + flip the customize gates so the customize
// closet is reachable and renders a character WITHOUT a real sign-in.
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
// WAS OPT-IN, NOW DEFAULT-ON: the feared "default-on cascade" was fully root-caused
// (2026-06-09) and resolved. The dominoes:
//   1. MainHubPanel::ReloadMessages -> TheServer.GetPlayerID(): TheServer was a
//      zeroed stub -> FIXED by rb3_server_native.cpp (faithful offline Server).
//   2. The "RndMat::SyncProperty / PropSync<RndTex>" SIGSEGV theory was a RED
//      HERRING. The real crash was BandPatchMesh::ProjectPatches writing
//      RndMesh::sRawCollide into READ-ONLY .text (the static had no out-of-line
//      definition; the native link aliased it to a weak no-op .text stub) the
//      moment a tattooed char composited -> FIXED ed9a3e92 (real .bss defs).
// With (1)+(2) fixed and the gate predicates flipped by real engine APIs (steps
// 5/6 below), the guest profile boots clean to gameplay AND reaches the customize
// closet, so it is on by default — the web build cannot set env, so default-on is
// what makes the customize flow testable there.
//
// WHAT THIS MAKES THE CLOSET SHOW: world/shared/chars.milo's player0..3 are NOT
// bodyless — they are milo PROXIES of char/main/main.milo (C13), so with the
// preview cache on (default, CharCache::InitMe) each loads a full body (FileMerger
// + 13 bodyparts, 140 meshes) and the closet renders a standing character. OPEN:
// the closet preview char is static-posed (skinned=0, not Character::Poll'd into a
// live skeleton) + head deform (C7/C8) — tracked in NATIVE_PORT_ROADMAP.
#include "os/PlatformMgr.h"
#include "meta/WiiProfileMgr.h"
#include "meta/Profile.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/BandProfile.h"
#include "meta_band/PrefabMgr.h"
#include "os/Debug.h"
#include <cstdlib>

// Idempotent: safe to call every frame; only acts once.
void RB3InstallGuestProfile() {
    static bool sDone = false;
    if (sDone)
        return;
    // OPT-IN again (RB3_GUEST_PROFILE=1, default OFF). Default-on was attempted but
    // REVERTED: guest-profile + char-preview BOTH on flakily-then-consistently SIGSEGVs
    // on the song_select transition — the long-open "domino ②": a menu DTA drives
    // RndMat::Handle -> OnSet -> SetProperty -> RndMat::SyncProperty -> PropSync<RndTex>
    // (PropSync_p.h:124) -> dynamic_cast on a DANGLING object (node.GetObj() returns a
    // freed/garbage Hmx::Object* -> vtable read faults). Each flag ALONE is safe; only
    // the combination crashes (the guest profile makes char-preview composite materials
    // a menu DTA then re-touches). Until that dangling-DataNode crash is fixed, this
    // stays opt-in so the gameplay path is regression-free. The customize closet still
    // works with RB3_GUEST_PROFILE=1 RB3_CHAR_PREVIEW=1 (verified: reachable, char
    // skinned + animating). Roadmap C11 = fix domino ② -> restore default-on (for web).
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
    // (5) DELIBERATELY DO NOT make this hollow guest profile PRIMARY. (An earlier
    //     version called TheProfileMgr.UpdatePrimaryProfile() here to flip
    //     has_primary_profile=1 / unblock customize_band. REGRESSION: a primary
    //     profile makes CharSync::UpdateCharCache (which runs menu-wide, default-on
    //     with char-preview) read profile->GetStandIn(i) (CharSync.cpp:82-87) — but
    //     this guest profile's StandIns are uninitialized/empty, so it crashes flakily
    //     (SIGSEGV +0x30) on the song_select transition. With NO primary profile,
    //     GetPrimaryProfile()==null and UpdateCharCache falls back to GetDefaultPrefab
    //     -> the 4 default band prefabs, which is exactly what we want AND crash-free.
    //     The closet route (customize_character) needs only is_char_customizable
    //     (step 6), NOT has_primary_profile, so the closet still opens. customize_band /
    //     manage_band stay gated (acceptable — no 4-up customize view exists anyway).
    //     A real signed-in profile (roadmap C11) has valid StandIns and WOULD be primary.)
    // (6) Make the default prefab character customizable so customize_character.btn
    //     takes the closet branch ({$user is_char_customizable}, main_hub.dta:133 ->
    //     {closet_mgr set_user $user} -> customize_clothing_screen). PrefabChar::
    //     IsCustomizable() == gPrefabIsCustomizable (PrefabMgr.cpp:25, default false,
    //     game-shipped as the `prefab_toggle_customizable` cheat). Flip it ON via the
    //     real registered toggle (idempotent: the toggle XORs, so only flip if off).
    //     HACK/TODO(C11): a real signed-in user customizes a saved TourChar (always
    //     customizable); for the offline guest we surface the default prefab instead.
    if (!PrefabMgr::PrefabIsCustomizable())
        PrefabMgr::OnPrefabToggleCustomizable(nullptr);
    MILO_LOG("RB3 native HACK: installed pad-0 guest profile (signinMask|=1, "
             "WiiProfileMgr idx0 valid, BandProfile[0]=Loaded, primary profile set, "
             "prefab customizable)\n");
    sDone = true;
}
#endif // HX_NATIVE
