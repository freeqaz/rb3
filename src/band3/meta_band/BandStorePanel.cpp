#include "meta_band/BandStorePanel.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/BandStoreOffer.h"
#include "meta_band/InputMgr.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/StoreOfferContentsProvider.h"
#include "meta_band/StoreOfferProvider.h"
#include "meta_band/UIEventMgr.h"
#include "meta_band/AppLabel.h"
#include "game/BandUser.h"
#include "meta/StorePackedMetadata.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/CommerceMgr_Wii.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "ui/UI.h"
#include "ui/UIList.h"
#include "ui/UIListLabel.h"
#include "ui/UIListProvider.h"
#include "utl/MakeString.h"
#include "utl/Messages.h"
#include "utl/Messages4.h"
#include "utl/NetCacheMgr.h"
#include "utl/NetLoader.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

BandStorePanel::BandStorePanel()
    : mMetadataLoader(0), mLastRequestExtra(0), mSort(gNullStr),
      mStartBrowserAtBottom(0), mUserCanDoInput(0), mShortcutProvider(0) {
    mOfferProvider = new StoreOfferProvider(&unk38, &unk48);
    mOfferContentsProvider = new StoreOfferContentsProvider();
}

BandStorePanel::~BandStorePanel() {
    delete mOfferProvider;
    delete mOfferContentsProvider;
}

BandStorePanel *BandStorePanel::Instance() {
    return ObjectDir::Main()->Find<BandStorePanel>("store_panel", true);
}

bool BandStorePanel::IsSongInLibrary(const int &id) const {
    return TheSongMgr.HasSong(id);
}

const char *BandStorePanel::GetIndexFile() const {
    return MakeString("%d", TheStoreMetadata.mVersion->mBuildNumber);
}

const char *BandStorePanel::GetRequestPrefix() const {
    return "dlc_store";
}

int BandStorePanel::StoreUser() const {
    LocalBandUser *l = TheInputMgr->GetUser()
        ? TheInputMgr->GetUser()->GetLocalBandUser()
        : 0;
    return (int)(LocalUser *)l;
}

StoreOffer *BandStorePanel::MakeNewOffer(const StorePackedOfferBase *base, bool isRbn) {
    return new BandStoreOffer(base, &TheSongMgr, isRbn);
}

StoreOffer *BandStorePanel::FindOffer(Symbol s) const {
    for (std::vector<StoreOffer *>::const_iterator it = unk38.begin();
         it != unk38.end(); ++it) {
        StoreOffer *o = *it;
        if (o->ShortName() == s)
            return o;
    }
    for (std::vector<StoreOffer *>::const_iterator it = unk48.begin();
         it != unk48.end(); ++it) {
        StoreOffer *o = *it;
        if (o->ShortName() == s)
            return o;
    }
    return 0;
}

StoreOffer *BandStorePanel::GetLoneOffer(bool extras) const {
    if (!extras) {
        MILO_ASSERT(unk38.size() == 1, 0xAA);
        return unk38[0];
    }
    MILO_ASSERT(!unk40.empty(), 0xAF);
    return unk40[0];
}

bool BandStorePanel::IsLoaded() const {
    return StorePanel::IsLoaded() || !mLoadOK;
}

void BandStorePanel::Unload() {
    mLastRequest.erase();
    delete mMetadataLoader;
    mMetadataLoader = 0;
    delete mShortcutProvider;
    mShortcutProvider = 0;
    StorePanel::Unload();
}

void BandStorePanel::Enter() {
    StorePanel::Enter();
    // Cross-cast via (BandUser*) defeats MWCC's inlined virtual-base
    // resolution and forces emission of an actual __dynamic_cast call,
    // matching the target's codegen. Runtime behavior is identical.
    LocalUser *u = dynamic_cast<LocalUser *>((BandUser *)(LocalBandUser *)StoreUser());
    if (u && !u->IsJoypadConnected()) {
        ExitError(kStoreErrorStoreServer);
    }
    TheSessionMgr->AddSink(this, LocalUserLeftMsg::Type());
}

void BandStorePanel::Exit() {
    TheSessionMgr->RemoveSink(this, LocalUserLeftMsg::Type());
    StorePanel::Exit();
}

DataNode BandStorePanel::OnMsg(const LocalUserLeftMsg &) {
    return DataNode(1);
}

DataNode BandStorePanel::OnMsg(const MetadataLoadedMsg &msg) {
    DataArray *data = msg->Array(2);
    String path(msg->Str(4));
    DataArray *found = data->FindArray(Symbol("metadata"), false);
    if (found) {
        // PopulateOffers + EnumerateOffers (msg fields 5 and 6 control flow)
        PopulateOffers(found, msg->Int(6) != 0);
        EnumerateOffers(msg->Int(6) != 0);
    }
    return DataNode(1);
}

Symbol BandStorePanel::SortName() {
    if (mSort == gNullStr) {
        return Symbol("by_song_first_letter");
    }
    return mSort;
}

inline const char *BandStoreShortcutProvider::RawTextAtData(int i) const {
    DataNode &n = mData->Node(i + mOffset);
    MILO_ASSERT(n.Type() == kDataString, 0x3A);
    return n.Str(0);
}

const char *BandStorePanel::ShortcutTextAtData(int i) {
    MILO_ASSERT(mShortcutProvider, 0x24A);
    return mShortcutProvider->RawTextAtData(i);
}

void BandStorePanel::SetShortcutData(DataArray *arr) {
    if (mShortcutProvider) {
        mShortcutProvider->SetData(arr);
    } else {
        mShortcutProvider = new BandStoreShortcutProvider(arr);
    }
}

void BandStorePanel::ApplyShortcutProvider(UIList *list) {
    MILO_ASSERT(mShortcutProvider, 0x258);
    list->SetProvider(mShortcutProvider);
}

void BandStoreShortcutProvider::Text(int i, int j, UIListLabel *listlabel, UILabel *label) const {
    if (mData->Node(j + mOffset).Evaluate().Type() == kDataString) {
        AppLabel *al = dynamic_cast<AppLabel *>(label);
        MILO_ASSERT(al, 0x30);
        al->SetRawStoreShortcut(j);
    } else {
        DataProvider::Text(i, j, listlabel, label);
    }
}

void BandStorePanel::LoadArt(const char *path, UIPanel *callback) {
    ObjectDir::Main()->Find<BandStorePanel>("store_panel", true);
    String full("dlc_store");
    full += path;
    StorePanel::LoadArt(full.c_str(), callback);
}

void BandStorePanel::Request(const String &path, bool extra) {
    int id = atoi(path.c_str());
    if (id == 0) {
        // Path-based request (album art download / config files)
        if (mLoadOK) {
            if (TheNetCacheMgr->GetHasFailed()) {
                HandleNetCacheMgrFailure();
                return;
            }
            MILO_ASSERT(mLastRequest.empty(), 0x1B4);
            MILO_ASSERT(TheNetCacheMgr->IsReady(), 0x1B5);
            MILO_ASSERT(!mMetadataLoader, 0x1B6);
            mLastRequest = path;
            mLastRequestExtra = extra;
            String url("dlc_store");
            url += MakeString(
                "/%s%s", PlatformRegionToSymbol(ThePlatformMgr.GetRegion()), path
            );
            // (skip platform-specific PID suffix in port-friendly build)
            mMetadataLoader = new DataNetLoader(url);
            mStartBrowserAtBottom = false;
            TheUI.Handle(update_loading_status_msg, false);
        }
    } else {
        // ID-based request (page lookup from metadata table). The full
        // implementation also dispatches a static MetadataLoadedMsg with
        // the page's offer index; the simplified form here covers the
        // tail steps shared with the message handler.
        StorePage *page = TheStoreMetadata.LoadPage((unsigned short)id);
        if (page) {
            mSort = page->mPage->DefaultSort();
        } else {
            mSort = Symbol("by_artist");
        }
        mPrevChunkPath = gNullStr;
        mNextChunkPath = gNullStr;
        mStartBrowserAtBottom = false;
        TheUI.Handle(update_loading_status_msg, false);
    }
}

void BandStorePanel::ExitStore(StoreError err) const {
    if (!TheUIEventMgr->HasActiveTransitionEvent()) {
        static Message msg("init", DataNode(-1));
        msg[0] = DataNode((int)err);
        TheUIEventMgr->TriggerEvent(store_load_failed, msg.mData);
    }
}

BEGIN_HANDLERS(BandStorePanel)
    HANDLE_EXPR(get_request_prefix, "dlc_store")
    HANDLE_ACTION(request, Request(String(_msg->Str(2)), _msg->Int(3)))
    HANDLE_ACTION(request_prev_chunk,
                  (Request(String(mPrevChunkPath.c_str()), true), mStartBrowserAtBottom = true))
    HANDLE_ACTION(request_next_chunk, Request(String(mNextChunkPath.c_str()), true))
    HANDLE_EXPR(should_start_browser_at_bottom, mStartBrowserAtBottom)
    HANDLE_EXPR(request_in_progress, mMetadataLoader != 0 || !(TheStoreMetadata.mFlags & 8))
    HANDLE_EXPR(num_offers, (int)unk38.size())
    HANDLE_EXPR(lone_offer, GetLoneOffer(false))
    HANDLE_EXPR(num_extra_offers, (int)unk40.size())
    HANDLE_EXPR(first_extra_offer, GetLoneOffer(true))
    HANDLE_EXPR(offer_provider, mOfferProvider)
    HANDLE_EXPR(offer_contents_provider, mOfferContentsProvider)
    HANDLE_EXPR(
        user_can_do_input,
        mUserCanDoInput == 0 && IsLoaded() && !IsEnumerating() && mLastRequest.empty()
            && (TheWiiCommerceMgr.mCommerceAsyncOpId == -1
                || TheWiiCommerceMgr.mCommerceAsyncName == 8)
    )
    HANDLE_ACTION(set_shortcut_data, SetShortcutData(_msg->Array(2)))
    HANDLE_ACTION(apply_shortcut_provider, ApplyShortcutProvider(_msg->Obj<UIList>(2)))
    HANDLE_MESSAGE(LocalUserLeftMsg)
    HANDLE_EXPR(sort_name, SortName())
    HANDLE_MESSAGE(MetadataLoadedMsg)
    HANDLE_SUPERCLASS(StorePanel)
    HANDLE_CHECK(0x2B0)
END_HANDLERS

BEGIN_PROPSYNCS(BandStorePanel)
    SYNC_PROP(waiting, mUserCanDoInput)
    SYNC_SUPERCLASS(StorePanel)
END_PROPSYNCS

void BandStorePanel::Poll() {
    StorePanel::Poll();
}

int BandStorePanel::UpdateOffers(const std::list<EnumProduct> &list, bool b) {
    return StorePanel::UpdateOffers(list, b);
}

Symbol DataProvider::DataSymbol(int i) const {
    DataNode &n = mData->Node(i + mOffset);
    if (n.Type() == kDataArray) {
        return n.Array()->Sym(0);
    } else {
        return n.ForceSym();
    }
}

bool DataProvider::IsActive(int i) const {
    return std::find(mDisabled.begin(), mDisabled.end(), DataSymbol(i)) == mDisabled.end();
}

UIListWidgetState
DataProvider::ElementStateOverride(int, int i, UIListWidgetState s) const {
    return std::find(mDimmed.begin(), mDimmed.end(), DataSymbol(i)) != mDimmed.end()
        ? kUIListWidgetInactive
        : s;
}
