#include "meta_band/InterstitialPanel.h"
#include "InterstitialPanel.h"
#include "decomp.h"
#include "meta/DeJitterPanel.h"
#include "obj/ObjMacros.h"
#include "ui/PanelDir.h"
#include "ui/UIPanel.h"
#include "utl/Messages4.h"
#include "utl/Symbols.h"

InterstitialPanel::InterstitialPanel() : mCamshotDone(0), unk88(0), mShowing(1) {}

void InterstitialPanel::Load() { UIPanel::Load(); }

void InterstitialPanel::Enter() {
    DeJitterPanel::Enter();
    mCamshotDone = 0;
    unk88 = 0;
}

bool InterstitialPanel::Exiting() const {
#ifdef HX_NATIVE
    // The transition vignette (e.g. tv3_*, the difficulty->game cab-ride video +
    // venue camera move) gates Exiting() on mCamshotDone (set by the venue camera
    // animation's transition_camshot_done .trg) and unk88>=3 (3 post-camshot
    // Draws). Headless/native has no working venue render+anim for these cosmetic
    // shell vignettes (deferred — see WorldInstance::SyncDir / BackdropPanel),
    // so transition_camshot_done never fires and Exiting() stays true forever →
    // the part_difficulty -> tv3 -> game_screen transition stalls in kTransitionTo.
    // Same seam + rationale as BackdropPanel::Exiting()'s native outro fix below:
    // treat the cosmetic camshot as immediately done so the screen-flow advances
    // to game_screen (where Game::LoadSong runs).
    bool baseEx = UIPanel::Exiting();
    // RB3_TV3_PLAY: opt-in HOLD experiment — restore the retail gate so the tv3
    // transition screen is NOT torn down until the vignette sequencer fires
    // transition_camshot_done (-> SetCamshotDone) and 3 post-camshot Draws pass.
    // Default-off: byte-identical to the prior native short-circuit (return baseEx).
    static int sPlay = -1;
    if (sPlay < 0)
        sPlay = getenv("RB3_TV3_PLAY") ? 1 : 0;
    bool ret = sPlay ? (baseEx || !mCamshotDone || unk88 < 3) : baseEx;
    if (getenv("INTERSTITIAL_DBG") || getenv("RB3_TV3SEQ_DBG")) {
        static int sLastFlags = -1;
        int flags = (baseEx ? 1 : 0) | (mCamshotDone ? 2 : 0) | ((unk88 & 0xF) << 4)
            | (ret ? 0x100 : 0);
        if (flags != sLastFlags) {
            MILO_LOG("RB3_TV3SEQ_DBG: InterstitialPanel::Exiting %s baseEx=%d camshotDone=%d unk88=%d play=%d -> %d\n",
                     Name(), baseEx ? 1 : 0, mCamshotDone ? 1 : 0, unk88, sPlay, ret ? 1 : 0);
            sLastFlags = flags;
        }
    }
    return ret;
#else
    return UIPanel::Exiting() || !mCamshotDone || unk88 < 3;
#endif
}

void InterstitialPanel::Unload() {
    if (mLoader && mLoader->IsLoaded()) {
        mDir = dynamic_cast<PanelDir *>(mLoader->GetDir());
        MILO_ASSERT_FMT(mDir, "%s not PanelDir", mLoader->mFile);
        RELEASE(mLoader);
    }
    UIPanel::Unload();
}

void InterstitialPanel::Draw() {
    if (mCamshotDone)
        unk88++;
    else if (mShowing)
        UIPanel::Draw();
}

void InterstitialPanel::SetCamshotDone() { mCamshotDone = true; }

BEGIN_HANDLERS(InterstitialPanel)
    HANDLE_ACTION(transition_camshot_done, SetCamshotDone())
    HANDLE_ACTION(set_showing, mShowing = _msg->Int(2))
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x62)
END_HANDLERS

BackdropPanel::BackdropPanel() : mOutroDone(0) {}

void BackdropPanel::Enter() {
    DeJitterPanel::Enter();
    mOutroDone = true;
}

void BackdropPanel::Exit() {
    mOutroDone = false;
    mDir->Handle(vignette_outro_msg, true);
    UIPanel::Exit();
#ifdef HX_NATIVE
    // The venue backdrop's outro animation (vignette_outro_msg -> the venue
    // .anim/.trg that fires vignette_outro_done -> SetOutroDone) is a cosmetic
    // 3D-venue effect. Headless/native has no working venue render+anim for the
    // shell venues (the 3D backdrops are deferred — see WorldInstance::SyncDir),
    // so vignette_outro_done never fires and mOutroDone stays false → Exiting()
    // stays true forever → the screen transition (e.g. splash_screen ->
    // main_hub_screen) stalls in kTransitionTo waiting on the old screen to
    // finish Exiting(). Treat the cosmetic outro as immediately done on native so
    // the screen-flow transition completes. (The §5.2 transition force-complete
    // the menu-flow spec anticipated, scoped to the venue backdrop outro.)
    mOutroDone = true;
#endif
}

bool BackdropPanel::Exiting() const {
#ifdef HX_NATIVE
    bool baseEx = UIPanel::Exiting();
    bool result = baseEx || !mOutroDone;
    if (getenv("INTERSTITIAL_DBG")) {
        static int sLastFlags = -1;
        int flags = (baseEx ? 1 : 0) | (mOutroDone ? 2 : 0) | (result ? 4 : 0);
        if (flags != sLastFlags) {
            MILO_LOG("INTERSTITIAL_DBG: BackdropPanel::Exiting %s baseEx=%d outroDone=%d -> %d\n",
                     Name(), baseEx ? 1 : 0, mOutroDone ? 1 : 0, result ? 1 : 0);
            sLastFlags = flags;
        }
    }
    return result;
#else
    return UIPanel::Exiting() || !mOutroDone;
#endif
}

void BackdropPanel::SetOutroDone() { mOutroDone = true; }

BEGIN_HANDLERS(BackdropPanel)
    HANDLE_ACTION(vignette_outro_done, SetOutroDone())
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x8A)
END_HANDLERS

DECOMP_FORCEFUNC(InterstitialPanel, BackdropPanel, ClassName())
DECOMP_FORCEFUNC(InterstitialPanel, BackdropPanel, SetType(0))
DECOMP_FORCEFUNC(InterstitialPanel, InterstitialPanel, ClassName())
DECOMP_FORCEFUNC(InterstitialPanel, InterstitialPanel, SetType(0))
DECOMP_FORCEDTOR(InterstitialPanel, InterstitialPanel)
DECOMP_FORCEDTOR(InterstitialPanel, BackdropPanel)