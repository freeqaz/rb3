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
    return UIPanel::Exiting() || !mCamshotDone || unk88 < 3;
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