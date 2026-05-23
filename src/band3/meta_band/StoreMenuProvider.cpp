#include "meta_band/StoreMenuProvider.h"
#include "meta/StorePackedMetadata.h"
#include "meta/StoreOffer.h"
#include "net_band/RockCentral.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "ui/UIListLabel.h"
#include "ui/UIListSlot.h"
#include "ui/UILabel.h"
#include "utl/Locale.h"
#include "utl/MakeString.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"

const char *StoreMenuProviderGetTitleFromData(DataArray *data) {
    DataArray *info = data->FindArray("index_info", false);
    if (info) {
        DataArray *title = info->FindArray("title", false);
        if (title) {
            DataNode node(title->Node(1));
            if (node.Type() == kDataString) {
                return node.Str(NULL);
            }
            return Localize(node.Sym(NULL), NULL);
        }
    }
    return gNullStr;
}

StoreMenuProvider::StoreMenuProvider(DataArray *data, const char *path)
    : mPath(path), mTitle(), mUnk40(0) {
    SetData(data);
}

StoreMenuProvider::~StoreMenuProvider() {}

void StoreMenuProvider::SetData(DataArray *data) {
    if (data == NULL)
        return;
    if (data->Node(0).Type() == kDataInt) {
        unsigned short pageId = data->Node(0).Int(data);
        mIxHighlight = 0;
        StorePage *page = TheStoreMetadata.LoadPage(pageId);
        if (page == NULL) {
            mTitle = gNullStr;
            TheDebug.Notify(
                MakeString("StoreMenuProvider: Page %d does not exist!\n", pageId)
            );
            return;
        }
        mPage = page;
        mTitle = TheStoreMetadata.GetString(page->mPage->unk0);
    } else {
        mTitle = StoreMenuProviderGetTitleFromData(data);
        if (data->FindArray(offers, false)) {
            mPage = TheStoreMetadata.LoadDynamicPage(NULL);
            return;
        }
        MILO_WARN("StoreMenuProvider: Page does not have offers!\n");
    }
}

int StoreMenuProvider::NumData() const { return mPage->mPage->mNumOffers; }

bool StoreMenuProvider::IsActive(int i) const {
    if (mPage->mPage->mHasOffers) {
        return true;
    }
    StorePackedSubMenu *submenu = mPage->Submenu(i);
    if (submenu == NULL) {
        return false;
    }
    if (submenu->unk6 == 0 || TheRockCentral.mState == 2) {
        return (short)submenu->unk4 != 0;
    }
    return false;
}

void StoreMenuProvider::Text(int, int i, UIListLabel *slot, UILabel *label) const {
    if (slot->Matches("filter")) {
        if (mPage->mPage->mHasOffers) {
            label->SetTextToken(Symbol(mPage->BaseOffer(i)->GetName()));
            return;
        }
        StorePackedSubMenu *submenu = mPage->Submenu(i);
        if (submenu) {
            label->SetTextToken(Symbol(TheStoreMetadata.GetString(submenu->unk2)));
            return;
        }
    } else if (slot->Matches("count")) {
        if (!mPage->mPage->mHasOffers) {
            StorePackedSubMenu *submenu = mPage->Submenu(i);
            if (submenu && (short)submenu->unk4 >= 0) {
                label->SetInt(submenu->unk4, true);
                return;
            }
        }
    }
    label->SetTextToken(Symbol(gNullStr));
}

const char *StoreMenuProvider::GetTitle() { return mTitle.c_str(); }

const char *StoreMenuProvider::GetFileName(int i) {
    if (!mPage->mPage->mHasOffers) {
        StorePackedSubMenu *submenu = mPage->Submenu(i);
        if (submenu) {
            if (submenu->unk6 != 0) {
                return TheStoreMetadata.GetString(submenu->unk6);
            }
            return MakeString("%d", submenu->unk0);
        }
    }
    return "1";
}

BEGIN_HANDLERS(StoreMenuProvider)
    HANDLE_EXPR(get_highlight_ix, 0)
    HANDLE_ACTION(set_highlight_ix, mIxHighlight = _msg->Int(2))
    HANDLE_EXPR(get_string, GetFileName(_msg->Int(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x14E)
END_HANDLERS
