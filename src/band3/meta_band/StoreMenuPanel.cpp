#include "meta_band/StoreMenuPanel.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandStorePanel.h"
#include "meta_band/StoreMenuProvider.h"
#include "bandobj/BandList.h"
#include "meta/StorePackedMetadata.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "ui/UI.h"
#include "ui/UIList.h"
#include "utl/MakeString.h"
#include "utl/Messages2.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

StoreMenuPanel *StoreMenuPanel::inst;

StoreMenuPanel::StoreMenuPanel()
    : mMenuStack(), mCurrentMenuIx(-1), mPendingMenuIx(-1), mList(0),
      mStartingHighlightIx(0) {
    inst = this;
}

StoreMenuPanel::~StoreMenuPanel() { inst = nullptr; }

void StoreMenuPanel::SetPendingMenuIx(int ix) {
    mPendingMenuIx = ix;
    if (mList)
        mList->Conceal();
}

void StoreMenuPanel::FinishLoad() {
    UIPanel::FinishLoad();
    const DataArray *typeDef = TypeDef();
    MILO_ASSERT(typeDef, 0x2B);
    static Symbol menu_list("menu_list");
    const char *name = typeDef->FindArray(menu_list, true)->Str(1);
    mList = mDir->Find<BandList>(name, true);
}

void StoreMenuPanel::Unload() {
    mCurrentMenuIx = -1;
    for (int i = 0; i < (int)mMenuStack.size(); i++) {
        delete mMenuStack[i];
    }
    mMenuStack.clear();
    mList = nullptr;
    UIPanel::Unload();
}

void StoreMenuPanel::Enter() {
    BandStorePanel *storePanel = BandStorePanel::Instance();
    UIPanel::Enter();
    storePanel->AddSink(this);
    if (mCurrentMenuIx == -1) {
        mStartingHighlightIx = 0;
        mPendingMenuIx = -1;
        storePanel->Request(String(storePanel->GetIndexFile()), true);
    }
}

void StoreMenuPanel::Exit() {
    BandStorePanel::Instance()->RemoveSink(this);
    UIPanel::Exit();
}

void StoreMenuPanel::Poll() {
    UIPanel::Poll();
    if (mPendingMenuIx >= 0) {
        if (mList && !mList->IsAnimating()) {
            MILO_ASSERT(mPendingMenuIx < (int)mMenuStack.size(), 0x58);
            mList->SetShowing(true);
            mList->SetProvider(mMenuStack[mPendingMenuIx]);
            mList->SetSelected(mMenuStack[mPendingMenuIx]->mIxHighlight, -1);
            mList->ForceConcealedStateOnAllEntries();
            mList->Reveal();
            mCurrentMenuIx = mPendingMenuIx;
            mPendingMenuIx = -1;
            TheUI.Handle(new_provider_msg, false);
        }
    }
}

const char *StoreMenuPanel::GetCrumbText() const {
    const char *result = "";
    for (int i = 1; i <= mCurrentMenuIx; i++) {
        const char *title = mMenuStack[i]->GetTitle();
        if (title != gNullStr && strlen(title) != 0) {
            if (i == 1)
                result = MakeString("::%s", title);
            else
                result = MakeString("%s::%s", result, title);
        }
    }
    return result;
}

void StoreMenuPanel::AddMenu(DataArray *data, const char *path) {
    int next = mCurrentMenuIx + 1;
    StoreMenuProvider *provider;
    if (next >= (int)mMenuStack.size()) {
        provider = new StoreMenuProvider(data, path);
        mMenuStack.push_back(provider);
    } else {
        provider = mMenuStack[next];
        provider->SetData(data);
    }
    int numData = provider->NumData();
    int ix = 0;
    if (mMenuStack.size() == 1) {
        if (mStartingHighlightIx < numData)
            ix = mStartingHighlightIx;
    }
    while (!provider->IsActive(ix)) {
        ix = (ix + 1) % numData;
    }
    provider->mIxHighlight = ix;
    SetPendingMenuIx(next);
}

DataNode StoreMenuPanel::OnBack(const DataArray *) {
    if (mCurrentMenuIx > 0) {
        SetPendingMenuIx(mCurrentMenuIx - 1);
        return DataNode(1);
    }
    mStartingHighlightIx = 0;
    SetPendingMenuIx(-1);
    return DataNode(kDataUnhandled, 0);
}

DataNode StoreMenuPanel::OnMsg(const MetadataLoadedMsg &msg) {
    BandStorePanel *panel = BandStorePanel::Instance();
    if (msg->Int(3)) {
        if (msg->Array(2)->Node(0).Type() == kDataInt) {
            StorePage *page =
                TheStoreMetadata.LoadPage(msg->Array(2)->Int(0));
            if (page && !page->mPage->mHasOffers) {
                AddMenu(msg->Array(2), msg->Str(4));
            }
        }
    } else {
        panel->ExitError(kStoreErrorCacheRemoved);
    }
    return DataNode(1);
}

BEGIN_HANDLERS(StoreMenuPanel)
    HANDLE_EXPR(get_menu_provider, mMenuStack[mCurrentMenuIx])
    HANDLE(back, OnBack)
    HANDLE_ACTION(reset_last_menu, SetPendingMenuIx(mCurrentMenuIx - 1))
    HANDLE_ACTION(set_menu_waiting, SetPendingMenuIx(-1))
    HANDLE_EXPR(get_menu_waiting, mPendingMenuIx != -1)
    HANDLE_MESSAGE(MetadataLoadedMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x104)
END_HANDLERS
