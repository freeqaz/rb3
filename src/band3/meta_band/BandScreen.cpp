#include "meta_band/BandScreen.h"
#include "meta_band/BandUI.h"
#include "meta_band/Utl.h"
#include "obj/Data.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "utl/Messages.h"

void BandScreen::Enter(UIScreen *s) {
    UIScreen::Enter(s);
    const DataNode &handled = HandleType(block_wipe_in_msg);
    if (handled.Type() == kDataUnhandled || !handled.Int()) {
        TheBandUI.WipeInIfNecessary();
    }
}

bool BandScreen::Entering() const { return UIScreen::Entering() || TheBandUI.WipingIn(); }

void BandScreen::Exit(UIScreen *s) {
    UIScreen::Exit(s);
    TheBandUI.WipeOutIfNecessary();
    UnloadInterstitials();
}

bool BandScreen::Exiting() const {
#ifdef HX_NATIVE
    if (getenv("UISCREEN_DBG")) {
        bool uiEx = UIScreen::Exiting();
        bool wipe = TheBandUI.WipingOut();
        static int sLastFlags = -1;
        int flags = (uiEx ? 1 : 0) | (wipe ? 2 : 0);
        static const char *sLastName = nullptr;
        if (flags != sLastFlags || Name() != sLastName) {
            MILO_LOG("UISCREEN_DBG: BandScreen::Exiting %s uiEx=%d wipingOut=%d\n",
                     Name(), uiEx ? 1 : 0, wipe ? 1 : 0);
            sLastFlags = flags;
            sLastName = Name();
        }
        return uiEx || wipe;
    }
#endif
    return UIScreen::Exiting() || TheBandUI.WipingOut();
}

void BandScreen::LoadPanels() {
    if (!TheBandUI.mShowVignettes) {
        FOREACH (it, mPanelList) {
            if (IsVignette(it->mPanel)) {
                it->mAlwaysLoad = false;
            }
        }
    }
    UIScreen::LoadPanels();
    if (TheUI.GetTransitionState() == kTransitionTo && TheUI.PushDepth() == 0) {
        MILO_ASSERT(TheUI.TransitionScreen() == this, 0x45);
        LoadInterstitials();
    }
}

bool BandScreen::CheckIsLoaded() { return UIScreen::CheckIsLoaded(); }
bool BandScreen::IsLoaded() const { return UIScreen::IsLoaded(); }

void BandScreen::LoadInterstitials() {
#ifdef HX_NATIVE
    if (getenv("RB3_CROWD_PANEL_DBG"))
        MILO_LOG("[PANELDBG] >> BandScreen::LoadInterstitials screen=%s showVignettes=%d\n",
                 Name(), (int)TheBandUI.mShowVignettes);
#endif
    if (TheBandUI.mShowVignettes) {
        TheBandUI.mInterstitialMgr->GetInterstitialsFromScreen(this, mExtraPanels);
#ifdef HX_NATIVE
        if (getenv("RB3_CROWD_PANEL_DBG"))
            MILO_LOG("[PANELDBG]    interstitials resolved: %d panel(s)\n",
                     (int)mExtraPanels.size());
#endif
        FOREACH (it, mExtraPanels) {
            UIPanel *cur = *it;
            cur->CheckLoad();
            cur->CheckIsLoaded();
        }
    }
}

void BandScreen::UnloadInterstitials() {
#ifdef HX_NATIVE
    if (getenv("RB3_CROWD_PANEL_DBG"))
        MILO_LOG("[PANELDBG] >> BandScreen::UnloadInterstitials screen=%s count=%d\n",
                 Name(), (int)mExtraPanels.size());
#endif
    FOREACH_REVERSE(it, mExtraPanels) { (*it)->CheckUnload(); }
}

BEGIN_HANDLERS(BandScreen)
    HANDLE_SUPERCLASS(UIScreen)
    HANDLE_CHECK(0x88)
END_HANDLERS