#include "UIPanel.h"
#include "utl/MemMgr.h"
#include "ui/UIComponent.h"
#include "ui/PanelDir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "obj/DataUtl.h"
#include "os/Debug.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"
#ifdef HX_NATIVE
#include "obj/Task.h" // W28-CROWD A1: beat stamp on the CheckUnload UNLOAD line
#endif

#ifdef HX_NATIVE
// Defined in UIScreen.cpp (the prewarm hook owns the issued-loader set). Gate the
// adoption branch below on these so the panel only ever adopts+deletes a loader
// the RB3_PREWARM_SCREENS hook itself issued — never a foreign loader owned by
// another component. With the flag off the issued set is empty ⇒ always false.
// (Loader is a complete type here via obj/DirLoader.h -> utl/Loader.h.)
bool RB3PrewarmIssuedLoader(Loader *);
void RB3PrewarmForgetLoader(Loader *);
#endif

int UIPanel::sMaxPanelId = 0;

UIPanel::UIPanel()
    : mDir(0), mLoader(0), mFocusName(), mState(kUnloaded), mLoaded(0), mPaused(0),
      mShowing(1), mForceExit(0), mLoadRefs(0), mFilePath(), mPanelId(sMaxPanelId++) {
    MILO_ASSERT(sMaxPanelId < 0x8000, 0x24);
}

class ObjectDir *UIPanel::DataDir() {
    if (mDir) {
        return mDir->DataDir();
    }
    return Hmx::Object::DataDir();
}

void UIPanel::SetTypeDef(DataArray *data) {
    if (TypeDef() != data) {
        Hmx::Object::SetTypeDef(data);
        if (data) {
            data->FindData(focus, mFocusName, false);
            data->FindData(force_exit, mForceExit, false);
        }
    }
}

void UIPanel::CheckLoad() {
#ifdef HX_NATIVE
    // W27-CROWD STEP-0 probe (RB3_CROWD_PANEL_DBG): per-panel mLoadRefs transition,
    // to trace the interstitial->regular-panel refcount handshake across the
    // splash->main_hub transition and find where the streetslomo panel hits 0.
    // Byte-inert to the decomp build (HX_NATIVE only) + env-gated; the shared
    // statement below is left byte-identical (logs the post-increment value).
    if (getenv("RB3_CROWD_PANEL_DBG"))
        MILO_LOG("[PANELDBG] CheckLoad %s refs->%d\n", Name(), mLoadRefs + 1);
#endif
    if (++mLoadRefs == 1)
        Load();
}

void UIPanel::CheckUnload() {
    if (mLoadRefs >= 1) {
        if (mState == kDown) {
            Handle(exit_complete_msg, false);
        }
#ifdef HX_NATIVE
        if (getenv("RB3_CROWD_PANEL_DBG"))
            MILO_LOG("[PANELDBG] CheckUnload %s refs->%d%s beat=%.3f\n", Name(),
                     mLoadRefs - 1, (mLoadRefs - 1) == 0 ? " UNLOAD" : "",
                     TheTaskMgr.Beat());
#endif
        if (--mLoadRefs == 0)
            Unload();
    }
}

void UIPanel::SetLoadedDir(class PanelDir *dir, bool loaded) {
    MILO_ASSERT(!mLoader, 106);
    MILO_ASSERT(dir, 107);
    if (mDir) {
        mDir->SetOwnerPanel(nullptr);
    }
    mDir = dir;
    mLoaded = loaded;
    mDir->SetOwnerPanel(this);
}

void UIPanel::UnsetLoadedDir() {
    MILO_ASSERT(!mLoader, 120);
    if (mDir) {
        mDir->SetOwnerPanel(nullptr);
    }
    mDir = nullptr;
    mLoaded = false;
}

#ifdef HX_NATIVE
FilePath UIPanel::GetPanelFilePath() {
    FilePath fp;
    if (TypeDef()) {
        static Symbol fileSym("file");
        DataArray *found = TypeDef()->FindArray(fileSym, false);
        if (found) {
            Hmx::Object *thisObj = DataSetThis(this);
            fp.Set(FileGetPath(found->File(), 0), found->Str(1));
            DataSetThis(thisObj);
        }
    }
    return fp;
}
#endif

void UIPanel::Load() {
    if (mState != kUnloaded)
        MILO_FAIL("Can't load a panel already in state %i", mState);
    HandleType(load_msg);
    if (TypeDef()) {
        static Symbol fileSym("file");
        FilePath fp;
        LoaderPos pos = kLoadBack;
        DataArray *found = TypeDef()->FindArray(fileSym, false);
        if (found) {
            Hmx::Object *thisObj = DataSetThis(this);
            fp.Set(FileGetPath(found->File(), 0), found->Str(1));
            if (found->Size() == 3) {
                pos = (LoaderPos)found->Int(2);
            }
            DataSetThis(thisObj);
        }
        int heapInt = GetCurrentHeapNum();
        DataArray *heapArr = TypeDef()->FindArray(heap, false);
        if (heapArr) {
            heapInt = MemFindHeap(heapArr->Str(1));
        }
        if (!fp.empty()) {
            MemPushHeap(heapInt);
            MILO_ASSERT(!mLoader, 0xAD);
#ifdef HX_NATIVE
            // song_select-prewarm adoption (handoff 07): if RB3_PREWARM_SCREENS
            // pre-issued a background DirLoader for this milo while the user
            // dwelled on the previous screen, that loader sits in
            // TheLoadMgr.mLoaders (a finished kLoadBack loader is removed from
            // mLoading on completion but kept in mLoaders until ~DirLoader). The
            // unmodified path below would `new DirLoader(fp, ...)` a SECOND
            // loader that re-opens the ChunkStream and re-parses the (~2.8 MB)
            // milo on this frame — the whole point of prewarming is to avoid
            // exactly that. So adopt the prewarmed loader instead: hand its
            // already-built PanelDir to this panel via the normal
            // SetLoadedDir()/PollForLoading() machinery and delete the prewarm
            // loader. SetLoadedDir asserts !mLoader, so this branch must run
            // before we ever assign mLoader.
            //
            // Lifetime: GetDir() sets the loader's mAccessed flag; deleting the
            // loader then does NOT RELEASE mDir (see DirLoader::~DirLoader), so
            // the PanelDir survives and is owned by the panel exactly as if the
            // panel had loaded it itself. If we never adopt (user never enters
            // the prewarmed screen), the prewarm loader's own ~DirLoader frees
            // its dir (mAccessed stays false) — no leak, no double-delete.
            //
            // CRITICAL ownership gate: only adopt a loader the prewarm hook
            // ISSUED (RB3PrewarmIssuedLoader, pointer identity). DirLoader::Find
            // can legitimately return a loader owned by ANOTHER component (e.g.
            // an ObjDirPtr<T>::LoadFile loader still in flight, or a second
            // panel's own completed-but-not-yet-polled mLoader for the same
            // milo) — stealing+`delete`ing that is a use-after-free for its
            // owner. The issued set is only ever populated by the
            // RB3_PREWARM_SCREENS hook, so with the flag OFF this is always
            // false and the stock new-DirLoader path below runs unchanged
            // (flag-off = byte/behavior identical, the acceptance criterion).
            if (mLoadRefs == 1) {
                if (DirLoader *prewarmed = DirLoader::Find(fp)) {
                    if (RB3PrewarmIssuedLoader(prewarmed) && prewarmed->IsLoaded()) {
                        class PanelDir *pDir =
                            dynamic_cast<class PanelDir *>(prewarmed->GetDir());
                        if (pDir) {
                            if (getenv("RB3_PREWARM_SCREENS"))
                                MILO_LOG(
                                    "RB3_PREWARM: UIPanel %s adopted prewarmed dir for %s\n",
                                    Name(), fp.c_str());
                            SetLoadedDir(pDir, false);
                            RB3PrewarmForgetLoader(prewarmed);
                            delete prewarmed;
                            MemPopHeap();
                            return;
                        }
                    }
                }
            }
#endif
            mLoader = new DirLoader(fp, pos, 0, 0, 0, false);
            MILO_ASSERT(mLoader, 0xAF);
            mLoaded = false;
            MemPopHeap();
        }
    }
}

void UIPanel::Unload() {
    HandleType(unload_msg);
    if (UIPanel::IsLoaded()) {
        bool async = false;
        if (TypeDef()) {
            DataArray *unloadArr = TypeDef()->FindArray(unload_async, false);
            if (unloadArr) {
                if (unloadArr->Int(1) != 0)
                    async = true;
            }
        }
        if (async) {
            TheLoadMgr.StartAsyncUnload();
            mFilePath.SetRoot(mDir->GetPathName());
        } else
            mFilePath.SetRoot(gNullStr);
        RELEASE(mDir);
        if (async)
            TheLoadMgr.FinishAsyncUnload();
    }
    RELEASE(mLoader);
    MILO_ASSERT(mLoadRefs == 0, 0xD9);
    mLoaded = false;
    mState = kUnloaded;
}

void UIPanel::PollForLoading() {
    MILO_ASSERT(mState == kUnloaded, 0xE0);
    if (mLoader && mLoader->IsLoaded()) {
        class PanelDir *pDir = dynamic_cast<class PanelDir *>(mLoader->GetDir());
        MILO_ASSERT_FMT(pDir, "%s not PanelDir", mLoader->mFile);
        RELEASE(mLoader);
        SetLoadedDir(pDir, mLoaded);
    }
}

bool UIPanel::IsLoaded() const {
    if (mState != kUnloaded)
        return true;
    if (mLoader && !mLoader->IsLoaded())
        return false;
    DataNode node = const_cast<UIPanel *>(this)->HandleType(is_loaded_msg);
    if (node.Type() != kDataUnhandled)
        return node.Int();
    else
        return true;
}

bool UIPanel::CheckIsLoaded() {
    if (mState != kUnloaded)
        return true;
    else {
        PollForLoading();
        if (IsLoaded()) {
            FinishLoad();
            return true;
        } else
            return false;
    }
}

void UIPanel::FinishLoad() {
    HandleType(finish_load_msg);
    MILO_ASSERT(mLoadRefs > 0, 0x118);
    mState = kDown;
}

UIComponent *UIPanel::FocusComponent() {
    if (mDir)
        return mDir->FocusComponent();
    else
        return nullptr;
}

UIPanel::~UIPanel() { Unload(); }

bool UIPanel::Entering() const {
    if (mDir && !mLoaded)
        return mDir->Entering();
    else
        return false;
}

bool UIPanel::Exiting() const {
    bool ret;
    if (mDir && !mLoaded)
        ret = mDir->Exiting();
    else
        ret = false;
#ifdef HX_NATIVE
    if (ret && getenv("UISCREEN_DBG")) {
        static const char *sLastPanel = nullptr;
        if (Name() != sLastPanel) {
            MILO_LOG("UISCREEN_DBG: UIPanel::Exiting %s mDir=%p !mLoaded=%d -> 1 (dir->Exiting)\n",
                     Name(), (void *)mDir, !mLoaded);
            sLastPanel = Name();
        }
    }
#endif
    return ret;
}

bool UIPanel::Unloading() const {
    if (!mFilePath.empty()) {
        if (TheLoadMgr.GetLoader(mFilePath)) {
            return true;
        }
        const_cast<UIPanel *>(this)->mFilePath.SetRoot(gNullStr);
    }
    return false;
}

void UIPanel::Enter() {
    MILO_ASSERT(mState == kDown, 0x14E);
    if (!mFocusName.empty() && mDir) {
        SetFocusComponent(mDir->FindComponent(mFocusName.c_str()));
    }
    MILO_ASSERT(mLoadRefs > 0, 0x154);
    mState = kUp;
    if (mDir && !mLoaded) {
        mDir->Enter();
    }
    HandleType(enter_msg);
}

void UIPanel::Exit() {
    MILO_ASSERT(mState == kUp, 0x165);
    bool resetFocus = false;
    const DataArray *td = TypeDef();
    if (td) {
        td->FindData("reset_focus", resetFocus, false);
    }
    if (!resetFocus && FocusComponent()) {
        mFocusName = FocusComponent()->Name();
    }
    MILO_ASSERT(mLoadRefs > 0, 0x16E);
    mState = kDown;
    HandleType(exit_msg);
    if (mDir && !mLoaded) {
        mDir->Exit();
    }
}

void UIPanel::Poll() {
    HandleType(poll_msg);
    if (mDir && !mLoaded) {
        mDir->Poll();
    }
}

void UIPanel::Draw() {
    if (mDir && !mLoaded) {
        mDir->DrawShowing();
    }
}

void UIPanel::SetFocusComponent(UIComponent *comp) {
    if (mDir) {
        mDir->SetFocusComponent(comp, gNullStr);
    }
}

BEGIN_HANDLERS(UIPanel)
    HANDLE_EXPR(is_loaded, IsLoaded())
    HANDLE_EXPR(check_is_loaded, CheckIsLoaded())
    HANDLE_EXPR(is_unloaded, GetState() == kUnloaded)
    HANDLE_EXPR(is_referenced, IsReferenced())
    HANDLE_EXPR(is_up, GetState() == kUp)
    HANDLE_ACTION(set_paused, SetPaused(_msg->Int(2)))
    HANDLE_EXPR(paused, Paused())
    HANDLE(load, OnLoad)
    HANDLE_ACTION(unload, CheckUnload())
    HANDLE_ACTION(set_focus, SetFocusComponent(_msg->Obj<UIComponent>(2)))
    HANDLE_ACTION(enter, Enter())
    HANDLE_ACTION_STATIC(exit, Exit())
    HANDLE_EXPR(loaded_dir, mDir)
    HANDLE_ACTION(set_showing, SetShowing(_msg->Int(2)))
    HANDLE_EXPR(showing, Showing())
    HANDLE_ACTION(set_loaded_dir, SetLoadedDir(_msg->Obj<class PanelDir>(2), false))
    HANDLE_ACTION(set_loaded_dir_shared, SetLoadedDir(_msg->Obj<class PanelDir>(2), true))
    HANDLE_ACTION(unset_loaded_dir, UnsetLoadedDir())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_MEMBER_PTR(mDir)
    HANDLE_CHECK(450)
END_HANDLERS

DataNode UIPanel::OnLoad(DataArray *da) {
    CheckLoad();
    if (da->Size() > 2) {
        if (da->Int(2) && mLoader) {
            TheLoadMgr.PollUntilLoaded(mLoader, 0);
            bool bLoaded = CheckIsLoaded();
            MILO_ASSERT(bLoaded, 0x1D1);
        }
    }
    return 0;
}
