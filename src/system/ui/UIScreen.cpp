#include "ui/UIScreen.h"

#include "os/Debug.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"

#ifdef HX_NATIVE
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "utl/Loader.h"
#include <set>
#include <map>
#include <string>
#include <cstdlib>
#ifdef HX_NATIVE
#include "obj/Task.h" // W28-CROWD A1: beat stamp on the UnloadPanels marker
#endif
#endif

typedef std::vector<PanelRef>::iterator iterator;
typedef std::vector<PanelRef>::const_iterator const_iterator;
typedef std::vector<PanelRef>::reverse_iterator reverse_iterator;
typedef std::vector<PanelRef>::const_reverse_iterator const_reverse_iterator;

UIScreen *UIScreen::sUnloadingScreen;
int UIScreen::sMaxScreenId;

UIScreen::UIScreen()
    : mFocusPanel(NULL), mBack(NULL), mClearVram(false), mShowing(true),
      mScreenId(sMaxScreenId++) {
    MILO_ASSERT(sMaxScreenId < 0x8000, 0x1C);
}

void UIScreen::SetTypeDef(DataArray *data) {
    if (TypeDef() == data) {
        return;
    }

    Hmx::Object::SetTypeDef(data);
    mFocusPanel = NULL;
    mPanelList.clear();

    DataArray *panelsArr = data->FindArray(panels, false);
    if (panelsArr != NULL) {
        for (int i = 1; i < panelsArr->Size(); i++) {
            PanelRef pr;
            pr.mActive = true;
            pr.mAlwaysLoad = true;

            if (panelsArr->Node(i).Type() == kDataArray) {
                DataArray *panelArray = panelsArr->Array(i);
                pr.mPanel = panelArray->Obj<class UIPanel>(0);
                MILO_ASSERT(pr.mPanel, 0x3E);
                panelArray->FindData(active, pr.mActive, false);
                panelArray->FindData(always_load, pr.mAlwaysLoad, false);
            } else {
                pr.mPanel = panelsArr->Obj<class UIPanel>(i);
                MILO_ASSERT(pr.mPanel, 0x45);
            }

            mPanelList.push_back(pr);
        }
    }

    DataArray *focusArr = data->FindArray(focus, false);
    if (focusArr != NULL) {
        SetFocusPanel(focusArr->Obj<class UIPanel>(1));
    }

    if (mFocusPanel == NULL && !mPanelList.empty()) {
        SetFocusPanel(mPanelList[0].mPanel);
    }

    mBack = data->FindArray("back", false);
    mClearVram = false;
    data->FindData(clear_vram, mClearVram, false);
}

BEGIN_HANDLERS(UIScreen)
    HANDLE_EXPR(focus_panel, mFocusPanel)
    HANDLE_ACTION(set_focus_panel, SetFocusPanel(_msg->Obj<class UIPanel>(2)))
    HANDLE_ACTION(print, Print(TheDebug))
    HANDLE_ACTION(reenter_screen, ReenterScreen())
    HANDLE_ACTION(
        set_panel_active, SetPanelActive(_msg->Obj<class UIPanel>(2), _msg->Int(3))
    )
    HANDLE_ACTION(set_showing, SetShowing(_msg->Int(2)))
    HANDLE_EXPR(has_panel, HasPanel(_msg->Obj<class UIPanel>(2)))
    HANDLE_EXPR(add_panel, AddPanel(_msg->Obj<class UIPanel>(2), _msg->Int(3)))
    HANDLE_ACTION(foreach_panel, ForeachPanel(_msg))
    HANDLE_EXPR(exiting, Exiting())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_MEMBER_PTR(FocusPanel())
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_CHECK(0x75)
END_HANDLERS

bool UIScreen::Entering() const {
    for (const_iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->Active() && it->mPanel->Entering()) {
            return true;
        }
    }

    if (sUnloadingScreen != NULL && sUnloadingScreen->Unloading()) {
        return true;
    }

    sUnloadingScreen = NULL;
    return false;
}

bool UIScreen::Exiting() const {
    for (const_iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->Active() && it->mPanel->Exiting()) {
#ifdef HX_NATIVE
            if (getenv("UISCREEN_DBG")) {
                static const char *sLastPanel = nullptr;
                const char *pn = it->mPanel->Name();
                if (pn != sLastPanel) {
                    MILO_LOG("UISCREEN_DBG: UIScreen::Exiting %s -> panel %s exiting=1\n",
                             Name(), pn);
                    sLastPanel = pn;
                }
            }
#endif
            return true;
        }
    }

    return false;
}

bool UIScreen::Unloading() const {
    for (const_iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->mLoaded && it->mPanel->Unloading()) {
            return true;
        }
    }

    return false;
}

#ifdef HX_NATIVE
// --- song_select-prewarm (handoff 07 / wave-04 §A2) -------------------------
//
// While the user dwells on main_hub, issue the next screen's panel milos as
// kLoadBack (budgeted background, rides TheLoadMgr.Poll's RB3_LOADER_BUDGET_MS
// 8ms/frame). When the user ENTERs the next screen, UIPanel::Load adopts the
// prewarmed-and-finished DirLoader (HX_NATIVE branch) instead of re-`new`ing a
// loader that re-parses the ~2.8MB milo on the transition frame.
//
// Enablement (Q10, incremental-load-perf PLAN.md T9): default ON for WEB
// (__EMSCRIPTEN__), opt out with RB3_PREWARM_SCREENS=0; default OFF (opt-in) for
// native, where the A/B was neutral (the byte fetch is local, so warming the
// parse during dwell wins little). The web A/B showed a win — the next screen's
// milo fetch+parse is hidden in the dwell instead of freezing the transition
// frame — so it ships default-on there. PrewarmEnabled() is the single gate;
// RB3_PREWARM_DBG still keys the verbose logging off the env var's presence so
// default-on doesn't spam. No struct members are added — the "already prewarmed
// this screen" bit lives in a file-static set keyed by the screen pointer
// (erased in Exit so re-entry re-prewarms the *next* screen).
namespace {
// The single prewarm enablement gate. getenv once into a static (house style).
//   web:    default ON; RB3_PREWARM_SCREENS=0 opts out.
//   native: default OFF; any RB3_PREWARM_SCREENS value (incl. "0") opts in,
//           preserving the prior opt-in semantics for native A/B work.
bool PrewarmEnabled() {
    static int s = -1;
    if (s < 0) {
        const char *e = ::getenv("RB3_PREWARM_SCREENS");
#ifdef __EMSCRIPTEN__
        // Default ON; only an explicit "0" (or "off"/"false"/"no") disables.
        s = 1;
        if (e && (e[0] == '0' || e[0] == 'f' || e[0] == 'F' || e[0] == 'n' ||
                  e[0] == 'N' || e[0] == 'o' || e[0] == 'O'))
            s = 0;
#else
        // Native: unchanged opt-in — present (non-null) means on.
        s = (e != nullptr) ? 1 : 0;
#endif
    }
    return s != 0;
}
std::set<UIScreen *> &PrewarmedScreens() {
    static std::set<UIScreen *> s;
    return s;
}

// The exact set of Loader* the prewarm hook itself issued (via AddLoader). This
// is the ownership boundary: UIPanel::Load only adopts+deletes a loader whose
// pointer is in this set, and EvictPriorPrewarm only frees loaders in this set.
// Identity (the pointer), not the FilePath, is the key — a foreign loader for
// the same milo (another component's still-live DirLoader, or a concurrently-
// loading panel's own completed mLoader) has a DIFFERENT pointer and is never
// touched. With RB3_PREWARM_SCREENS off this set is never populated, so the
// adoption branch is inert and the stock new-DirLoader path always runs.
std::set<Loader *> &IssuedPrewarmLoaders() {
    static std::set<Loader *> s;
    return s;
}

// Per-screen record of the panel milos we issued a prewarm DirLoader for, so a
// LATER prewarm pass can evict an OLD generation's loaders that are no longer
// wanted. Without this, a finished kLoadBack DirLoader + its parsed PanelDir
// would sit in TheLoadMgr.mLoaders forever (nothing GCs an unowned loaded
// loader). Keyed by the SOURCE screen pointer (the one that prewarmed).
//
// Eviction is GENERATIONAL (at re-prewarm time), NOT eager-on-Exit: the
// main_hub -> song_select path routes through an intermediate
// song_select_enter_screen, so a prewarmed loader is legitimately still
// unadopted across the first Exit and an Exit-time sweep would wrongly kill it.
// And it only evicts loaders NOT in the new wanted-set — anything still wanted
// is kept for adoption (killing + reissuing would re-parse the milo for
// nothing). In the steady case (fixed source->target mapping ⇒ same panel set
// every dwell) nothing is ever evicted. The only residual is one stale set if
// the user prewarms once and never returns — bounded, default-OFF, freed at
// exit.
std::map<UIScreen *, std::vector<FilePath> > &PrewarmedFiles() {
    static std::map<UIScreen *, std::vector<FilePath> > m;
    return m;
}

// Free any loader `screen` prewarmed on a prior dwell that is (a) NOT in `keep`
// (the new target set), (b) still resident + finished, and (c) unadopted
// (mAccessed == false ⇒ no panel ever took its dir). An ADOPTED loader was
// already `delete`d in UIPanel::Load, so DirLoader::Find won't find it. An
// in-flight loader (!IsLoaded) is left alone — deleting mid-load would
// Cleanup/abort it.
void EvictPriorPrewarm(UIScreen *screen, const std::vector<FilePath> &keep) {
    std::map<UIScreen *, std::vector<FilePath> > &files = PrewarmedFiles();
    std::map<UIScreen *, std::vector<FilePath> >::iterator e = files.find(screen);
    if (e == files.end())
        return;
    bool dbg = ::getenv("RB3_PREWARM_SCREENS") != NULL;
    for (std::vector<FilePath>::iterator it = e->second.begin(); it != e->second.end();
         ++it) {
        bool stillWanted = false;
        for (std::vector<FilePath>::const_iterator k = keep.begin(); k != keep.end();
             ++k) {
            if (*k == *it) {
                stillWanted = true;
                break;
            }
        }
        if (stillWanted)
            continue;
        DirLoader *dl = DirLoader::Find(*it);
        // ONLY free a loader the prewarm itself issued (pointer identity). A
        // foreign loader for the same milo — e.g. a concurrently-loading panel's
        // own completed mLoader — has a different pointer and must not be deleted
        // (doing so would dangle that panel's mLoader). mAccessed==false means no
        // panel ever adopted OUR loader (an adopted one was already deleted in
        // UIPanel::Load and erased from the issued set).
        if (dl && IssuedPrewarmLoaders().count(dl) && dl->IsLoaded() && !dl->mAccessed) {
            if (dbg)
                MILO_LOG("RB3_PREWARM: evicting stale prewarm dir %s\n", it->c_str());
            IssuedPrewarmLoaders().erase(dl);
            delete dl; // ~DirLoader RELEASEs mDir (mAccessed false) ⇒ no leak
        }
    }
    // record is rewritten by the caller (issued = wanted)
}

// Parse RB3_PREWARM_NEXT into a current-screen -> next-screen map once. Multiple
// pairs may be comma-separated, e.g.
// "main_hub_screen:song_select_screen,song_select_screen:song_options_screen".
const std::map<std::string, std::string> &PrewarmNextMap() {
    static std::map<std::string, std::string> m;
    static bool init = false;
    if (!init) {
        init = true;
        // Default maps the real UIScreen object NAMES (band_ui.dta:
        // main_hub_screen / song_select_screen), not the milo basenames. Override
        // with RB3_PREWARM_NEXT="from_screen:to_screen[,from2:to2,...]".
        const char *spec = ::getenv("RB3_PREWARM_NEXT");
        std::string s = (spec && spec[0]) ? spec : "main_hub_screen:song_select_screen";
        size_t pos = 0;
        while (pos < s.size()) {
            size_t comma = s.find(',', pos);
            std::string pair =
                s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            size_t colon = pair.find(':');
            if (colon != std::string::npos) {
                std::string from = pair.substr(0, colon);
                std::string to = pair.substr(colon + 1);
                if (!from.empty() && !to.empty())
                    m[from] = to;
            }
            if (comma == std::string::npos)
                break;
            pos = comma + 1;
        }
    }
    return m;
}

void DoPrewarmNextScreen(UIScreen *screen) {
    bool ddbg = ::getenv("RB3_PREWARM_DBG") != NULL;
    const char *myName = screen->Name();
    if (!myName)
        return;
    const std::map<std::string, std::string> &nextMap = PrewarmNextMap();
    std::map<std::string, std::string>::const_iterator nit = nextMap.find(myName);
    if (nit == nextMap.end()) {
        if (ddbg)
            MILO_LOG("RB3_PREWARM_DBG: no next-screen mapping for '%s'\n", myName);
        return;
    }

    // Non-fatal lookup (parentDirs=false ⇒ returns null if the screen object
    // isn't resident yet, rather than MILO_FAIL).
    UIScreen *next = ObjectDir::Main()->Find<UIScreen>(nit->second.c_str(), false);
    if (!next) {
        if (ddbg)
            MILO_LOG("RB3_PREWARM_DBG: next screen '%s' not resident yet\n",
                     nit->second.c_str());
        return;
    }
    if (ddbg)
        MILO_LOG("RB3_PREWARM_DBG: '%s' -> prewarming '%s' (%d panels)\n", myName,
                 nit->second.c_str(), (int)next->mPanelList.size());

    bool dbg = ::getenv("RB3_PREWARM_SCREENS") != NULL;

    // Resolve the panel-milo set this prewarm targets. Mirror LoadPanels' load
    // predicate: only panels the next screen will actually load (mLoaded is
    // meaningless until LoadPanels runs, so gate on mAlwaysLoad / IsReferenced).
    std::vector<FilePath> wanted;
    for (std::vector<PanelRef>::iterator it = next->mPanelList.begin();
         it != next->mPanelList.end();
         ++it) {
        if (!(it->mAlwaysLoad || it->mPanel->IsReferenced()))
            continue;
        FilePath fp = it->mPanel->GetPanelFilePath();
        if (!fp.empty())
            wanted.push_back(fp);
    }

    // Reap loaders this screen prewarmed on a PRIOR dwell that are no longer in
    // the wanted set (the next-screen target changed) — anything still wanted is
    // KEPT so it can be adopted (the DirLoader::Find dedup below preserves it; we
    // must not kill+reissue it, which would re-parse the milo for nothing).
    EvictPriorPrewarm(screen, wanted);

    std::vector<FilePath> &issued = PrewarmedFiles()[screen];
    issued = wanted; // record this generation's target set
    for (std::vector<FilePath>::iterator it = wanted.begin(); it != wanted.end(); ++it) {
        if (DirLoader::Find(*it))
            continue; // already loaded or in-flight (incl. a still-live prewarm)
        if (dbg)
            MILO_LOG("RB3_PREWARM: %s -> prewarming %s panel milo %s\n", myName,
                     nit->second.c_str(), it->c_str());
        // Record the EXACT loader we issued so only we adopt/evict it. A null
        // return (shouldn't happen for a non-resident fp) is simply not tracked.
        if (Loader *ldr = TheLoadMgr.AddLoader(*it, kLoadBack))
            IssuedPrewarmLoaders().insert(ldr);
    }
}
} // namespace

// Bridge for UIPanel::Load (separate TU): is `ldr` a loader the prewarm hook
// issued? Only such a loader may be adopted+deleted by the panel-load path. When
// RB3_PREWARM_SCREENS is off the issued set is empty, so this is always false and
// the adoption branch never fires (stock new-DirLoader path runs). NOT in the
// anon namespace so UIPanel.cpp can extern-declare and call it.
bool RB3PrewarmIssuedLoader(Loader *ldr) {
    return ldr != NULL && IssuedPrewarmLoaders().count(ldr) != 0;
}

// The panel adopted `ldr` and is about to delete it; drop it from the issued set
// so EvictPriorPrewarm never re-finds a freed pointer.
void RB3PrewarmForgetLoader(Loader *ldr) { IssuedPrewarmLoaders().erase(ldr); }
#endif

void UIScreen::Poll() {
    HandleType(poll_msg);

    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->Active() && !it->mPanel->Paused()) {
            it->mPanel->Poll();
        }
    }

#ifdef HX_NATIVE
    // song_select-prewarm gate (handoff 07). Only after THIS screen is fully
    // loaded (so we ride its idle dwell, not its own load) and only once per
    // screen instance (re-armed in Exit). CheckIsLoaded() is side-effect-free
    // here: a fully-loaded screen's panels are all past kUnloaded, so the call
    // is a pure read. Q10: default ON for web, opt-in for native (PrewarmEnabled).
    if (PrewarmEnabled()) {
        bool loaded = CheckIsLoaded();
        if (::getenv("RB3_PREWARM_DBG")) {
            static std::set<UIScreen *> sSeen;
            if (sSeen.find(this) == sSeen.end()) {
                sSeen.insert(this);
                MILO_LOG("RB3_PREWARM_DBG: Poll screen '%s' checkLoaded=%d\n",
                         Name() ? Name() : "(null)", (int)loaded);
            }
        }
        if (loaded && PrewarmedScreens().find(this) == PrewarmedScreens().end()) {
            PrewarmedScreens().insert(this);
            DoPrewarmNextScreen(this);
        }
    }
#endif
}

#pragma push
#pragma pool_data off
// Likely weak based on how bss gets used
void UIScreen::Enter(UIScreen *from) {
    if (from != NULL) {
        sUnloadingScreen = from;
        from->UnloadPanels();
    }

    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->Active() && it->mPanel->GetState() == UIPanel::kDown) {
            it->mPanel->Enter();
        }
    }

    static Message msg("enter", 0);
    msg[0] = from;
    HandleType(msg);
    Poll();
}
#pragma pop

void UIScreen::Draw() {
    if (!mShowing) {
        return;
    }

    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->Active() && it->mPanel->Showing()) {
            it->mPanel->Draw();
        }
    }
}

void UIScreen::SetFocusPanel(class UIPanel *panel) {
    if (panel == mFocusPanel) {
        return;
    }

    if (mFocusPanel != NULL)
        mFocusPanel->FocusIn();
    mFocusPanel = panel;
    if (mFocusPanel != NULL)
        mFocusPanel->FocusOut();
}

bool UIScreen::InComponentSelect() const {
    UIComponent *component = TheUI.FocusComponent();
    if (component != NULL) {
        return component->GetState() == UIComponent::kSelecting;
    }

    return false;
}

bool UIScreen::SharesPanels(UIScreen *screen) {
    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (screen->HasPanel(it->mPanel)) {
            return true;
        }
    }

    return false;
}

bool UIScreen::HasPanel(class UIPanel *panel) {
    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->mPanel == panel && it->mActive) {
            return true;
        }
    }

    return false;
}

bool UIScreen::AddPanel(class UIPanel *panel, bool alwaysLoad) {
    if (HasPanel(panel)) {
        return false;
    }

    PanelRef ref;
    ref.mActive = true;
    ref.mAlwaysLoad = alwaysLoad;
    ref.mLoaded = panel->IsLoaded();
    ref.mPanel = panel;

    mPanelList.push_back(ref);
    return true;
}

void UIScreen::Exit(UIScreen *to) {
#ifdef HX_NATIVE
    // Re-arm prewarm for the next dwell on this screen (handoff 07).
    PrewarmedScreens().erase(this);
#endif
    static Message msg("exit", 0);
    msg[0] = to;
    HandleType(msg);

    if (to != NULL) {
        to->LoadPanels();
    }

    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (!it->mLoaded) {
            continue;
        }

        if ((it->mPanel->ForceExit() || to == NULL || !to->HasPanel(it->mPanel))
            && it->mPanel->GetState() == UIPanel::kUp) {
            it->mPanel->Exit();
        }
    }
}

void UIScreen::ReenterScreen() {
    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->Active()) {
            it->mPanel->Exit();
        }
    }

    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->Active()) {
            it->mPanel->Enter();
        }
    }
}

void UIScreen::SetPanelActive(class UIPanel *panel, bool active) {
    bool found = false;
    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->mPanel == panel) {
            it->mActive = active;
            found = true;
        }
    }

    if (!found) {
        MILO_WARN(
            "UIScreen::SetPanelActive: not found\nscreen %s\npanel %s\n",
            PathName(this),
            PathName(panel)
        );
    }
}

void UIScreen::SetShowing(bool show) { mShowing = show; }

void UIScreen::LoadPanels() {
#ifdef HX_NATIVE
    if (getenv("RB3_CROWD_PANEL_DBG"))
        MILO_LOG("[PANELDBG] >> UIScreen::LoadPanels screen=%s\n", Name());
#endif
    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->mAlwaysLoad || it->mPanel->IsReferenced()) {
            it->mPanel->CheckLoad();
            it->mLoaded = true;
        } else {
            it->mLoaded = false;
        }
    }

    HandleType(load_panels_msg);
}

void UIScreen::UnloadPanels() {
#ifdef HX_NATIVE
    if (getenv("RB3_CROWD_PANEL_DBG"))
        MILO_LOG("[PANELDBG] >> UIScreen::UnloadPanels screen=%s beat=%.3f\n", Name(),
                 TheTaskMgr.Beat());
#endif
    for (reverse_iterator it = mPanelList.rbegin(); it != mPanelList.rend(); it++) {
        if (it->mLoaded) {
            it->mPanel->CheckUnload();
        }
    }
}

bool UIScreen::CheckIsLoaded() {
    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->Active() && !it->mPanel->CheckIsLoaded()) {
#ifdef HX_NATIVE
            if (getenv("UISCREEN_DBG")) {
                static int sLastReportFrame = -1;
                static const char *sLastBlockedPanel = nullptr;
                const char *pn = it->mPanel->Name();
                if (pn != sLastBlockedPanel) {
                    MILO_LOG("UISCREEN_DBG: UIScreen::CheckIsLoaded %s -> BLOCKED on panel %s (state=%d)\n",
                             Name(), pn, (int)it->mPanel->GetState());
                    sLastBlockedPanel = pn;
                }
            }
#endif
            return false;
        }
    }

    return true;
}

bool UIScreen::IsLoaded() const {
    for (const_iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->Active() && it->mPanel->GetState() == UIPanel::kUnloaded) {
            return false;
        }
    }

    // please don't tell me const_cast is what they did lol
    DataNode result = const_cast<UIScreen *>(this)->HandleType(is_loaded_msg);
    if (result.Type() != kDataUnhandled) {
        return result.Int();
    }

    return true;
}

bool UIScreen::AllPanelsDown() {
    for (const_iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (it->Active() && it->mPanel->GetState() != UIPanel::kDown) {
            return false;
        }
    }

    return true;
}

void UIScreen::Print(TextStream &s) {
    s << "{UIScreen " << Name() << "\n";

    if (mPanelList.size() != 0) {
        s << "   Panels:\n";
        for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
            s << "      " << it->mPanel->Name() << " ";
            if (!it->mActive) {
                s << "(active " << it->mActive << ") ";
            }
            if (!it->mAlwaysLoad) {
                s << "(always_load " << it->mAlwaysLoad << ") ";
            }

            const DataArray *typeDef = it->mPanel->TypeDef();
            if (typeDef != NULL) {
                DataArray *fileArray = typeDef->FindArray(file, false);
                if (fileArray != NULL) {
                    DataNode type = fileArray->Node(1);
                    if (type.Type() == kDataString || type.Type() == kDataSymbol) {
                        s << "(" << type.LiteralStr() << ") ";
                    } else {
                        s << "(dynamic) ";
                    }
                }
            } else {
                s << " ";
            }

            if (it->mPanel == mFocusPanel) {
                s << "(focus)";
            }

            s << "\n";
        }
    }

    s << "}\n";
}

DataNode UIScreen::OnMsg(const ButtonDownMsg &msg) {
    if (mBack != NULL && msg.GetAction() == kAction_Cancel) {
        DataNode n = mBack->Evaluate(1);
        if (n.Type() != kDataUnhandled) {
            Message m(go_back_screen, n.Str(), msg.GetUser());
            TheUI.Handle(m, true);
        }
    }

    return DataNode(kDataUnhandled, 0);
}

DataNode UIScreen::ForeachPanel(const DataArray *da) {
    // {$screen foreach_panel $panel ...}

    DataNode *var = da->Var(2);
    DataNode tmp = *var;

    for (iterator it = mPanelList.begin(); it != mPanelList.end(); it++) {
        if (!it->mActive) {
            continue;
        }

        *var = it->mPanel;
        for (int i = 3; i < da->Size(); i++) {
            da->Command(i)->Execute();
        }
    }

    *var = tmp;
    return 0;
}

// void UIScreen::SetType(Symbol) {}
// void UIScreen::ClassName() const {}
