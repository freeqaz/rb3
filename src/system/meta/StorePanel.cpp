#include "meta/StorePanel.h"
#include "meta/StorePackedMetadata.h"
#include "meta/StoreOffer.h"
#include "os/ContentMgr.h"
#include "os/CommerceMgr_Wii.h"
#include "os/PlatformMgr.h"
#include "utl/NetCacheMgr.h"
#include "utl/Symbols.h"
#include "utl/Messages.h"

bool gStoreAllowBandShotForceShowing = true;

StorePanel::StorePanel()
    : mLoadOK(0), mShowTestOffers(1), mPendingArtLoader(0),
      mAlbumTex(Hmx::Object::New<RndTex>()), mPendingArtCallback(0), mStorePreviewMgr(0),
      mEnum(0), unk70(0), unk71(0), mPurchaser(0), mSource(gNullStr),
      mBackupSource(gNullStr), mSessionStatus(kSessionNone), mCurrentOffer(0),
      mCurrentOfferUpgrade(0), unk89(0), mStoreMode(0) {}

StorePanel::~StorePanel() {
    DeleteAll(unk38);
    DeleteAll(unk40);
    DeleteAll(unk48);
    delete mAlbumTex;
}

void StorePanel::Load() {
    UIPanel::Load();
    mLoadOK = true;
    TheContentMgr->StartRefresh();
    TheNetCacheMgr->Load((NetCacheMgr::CacheSize)1);
    MILO_ASSERT(!mStorePreviewMgr, 0x84);
    mStorePreviewMgr = new StorePreviewMgr();
    mStorePreviewMgr->AddSink(this);
    MILO_ASSERT(!mPurchaser, 0x88);
    RELEASE(mEnum);
    mSessionStatus = kSessionNone;
    unk89 = false;
}

void StorePanel::HandleNetCacheMgrFailure() {
    StoreError err = kStoreErrorSuccess;
    NetCacheMgrFailType failTy = TheNetCacheMgr->GetFailType();
    switch (failTy) {
    case kNCMFT_StoreServer:
    case kNCMFT_NoSpace:
        MILO_WARN("Failure %d in NetCacheMgr.\n", failTy);
        break;
    case kNCMFT_StorageDeviceMissing:
        err = kStoreErrorNoMetadata;
        break;
    default:
        MILO_WARN("Unknown failure %d in NetCacheMgr.\n", failTy);
        break;
    }
    if (err != kStoreErrorNoMetadata && !ThePlatformMgr.IsEthernetCableConnected()) {
        err = kStoreErrorNoMetadata;
    }
    if (err != kStoreErrorSuccess)
        ExitError(err);
}

DECOMP_FORCEACTIVE(
    StorePanel,
    "( 0) <= (failType) && (failType) < ( kNCMFT_Max)",
    "Unknown failure %d in a net cache loader!\n"
)

bool StorePanel::IsLoaded() const {
    return UIPanel::IsLoaded() && TheContentMgr->RefreshDone()
        && (mSessionStatus == kSessionCreated || !mLoadOK);
}

void StorePanel::PollForLoading() {
    UIPanel::PollForLoading();
    if (mLoadOK && mSessionStatus == kSessionNone && TheNetCacheMgr->IsReady()) {
        if (!TheWiiCommerceMgr.InitCommerce(this)) {
            ExitError((StoreError)100);
        } else
            mSessionStatus = kSessionCreating;
    } else if (TheNetCacheMgr->GetHasFailed())
        HandleNetCacheMgrFailure();
}

void StorePanel::LoadMetadata() {
    if (mLoadOK && mSessionStatus == kSessionCreated) {
        TheStoreMetadata.Load("store_test/");
    }
}

bool StorePanel::IsMetadataLoaded() {
    if (TheStoreMetadata.LoadingFailed()) {
        ExitError(TheStoreMetadata.LoadError());
        return false;
    } else
        return (TheStoreMetadata.mFlags >> 3) & 1;
}

void StorePanel::Enter() {
    UIPanel::Enter();
    gStoreAllowBandShotForceShowing = false;
    SetShowing(mLoadOK);
    unk70 = false;
}

void StorePanel::Poll() {}

void StorePanel::Exit() {
    gStoreAllowBandShotForceShowing = true;
    UIPanel::Exit();
}

bool StorePanel::Exiting() const {
    if (mPurchaser && mPurchaser->IsEnumerating()) {
        return true;
    }
    if (mEnum && mEnum->IsEnumerating()) {
        return true;
    }
    return UIPanel::Exiting();
}

bool StorePanel::Unloading() const {
    if (GetState() != kUp && !TheNetCacheMgr->IsUnloaded()) {
        return true;
    } else
        return UIPanel::Unloading();
}

StorePanel *StorePanel::Instance() {
    return ObjectDir::Main()->Find<StorePanel>("store_panel", true);
}

bool StorePanel::IsEnumerating() const { return mEnum && mEnum->IsEnumerating(); }

bool StorePanel::InCheckout() const { return mPurchaser; }

void StorePanel::LoadArt(const char *cc, UIPanel *panel) {
    String str(cc);
    for (std::list<StorePanel *>::iterator it = unk54.begin(); it != unk54.end(); ++it) {
    }
    mPendingArtCallback = panel;
}

void StorePanel::CancelArt() {
    mPendingArtLoader = 0;
    mPendingArtCallback = 0;
}

void StorePanel::SetSource(Symbol s, bool b) {
    mSource = s;
    if (b)
        mBackupSource = s;
    if (mSource == setlist_upsell) {
        SetStoreMode(setlist);
    }
}

void StorePanel::SetSourceToBackup() { mSource = mBackupSource; }

void StorePanel::SetStoreMode(Symbol s) {
    if (s == dlc)
        mStoreMode = 0;
    else if (s == manage)
        mStoreMode = 1;
    else if (s == token)
        mStoreMode = 2;
    else if (s == setlist)
        mStoreMode = 3;
}

void StorePanel::PopulateOffers(DataArray *arr, bool b) {
    DeleteAll(unk38);
    DeleteAll(unk40);
    if (mStoreMode == 0) {
        StorePage *page;
        if (arr->Node(0).Type() == kDataInt) {
            int idx = ((const DataArray *)arr)->Node(0).Int(arr);
            page = TheStoreMetadata.LoadPage(idx);
        } else {
            page = TheStoreMetadata.LoadDynamicPage(arr);
        }
        if (page != nullptr) {
            if (page->mPage->mHasOffers && page->mPage->mNumOffers != 0) {
                for (int i = 0; i < page->mPage->mNumOffers; i++) {
                    const StorePackedOfferBase *offer = page->BaseOffer(i);
                    TheStoreMetadata.GetOfferStatus(offer);
                    unk38.push_back(MakeNewOffer(offer, (page->mOffers[i] >> 15) & 1));
                }
            }
        } else {
            mStoreMode = 1;
        }
    }
    if (mStoreMode == 1) {
        int rowOff, idxOff;
        rowOff = 0;
        idxOff = 0;
        for (int i = 0; i < TheStoreMetadata.mOfferTable->mNumOffers; i++) {
            unsigned char flags =
                ((StoreOfferState *)((char *)TheStoreMetadata.mOfferTable->mBufferNewRelease + rowOff))->mFlags;
            if ((flags & 1) || (flags & 2)) {
                StorePackedOffer *po =
                    *(StorePackedOffer **)((char *)TheStoreMetadata.mOfferTable->mOffers + idxOff);
                unk38.push_back(MakeNewOffer(po, false));
            }
            rowOff += 0xC;
            idxOff += 4;
        }
        rowOff = 0;
        idxOff = 0;
        for (int i = 0; i < TheStoreMetadata.mRbnOfferTable->mNumOffers; i++) {
            unsigned char flags =
                ((StoreOfferState *)((char *)TheStoreMetadata.mRbnOfferTable->mBufferNewRelease + rowOff))->mFlags;
            if ((flags & 1) || (flags & 2)) {
                StorePackedRBNOffer *po =
                    *(StorePackedRBNOffer **)((char *)TheStoreMetadata.mRbnOfferTable->mOffers + idxOff);
                unk38.push_back(MakeNewOffer(po, true));
            }
            rowOff += 0xC;
            idxOff += 4;
        }
        std::sort(unk38.begin(), unk38.end(), StoreOfferSort);
        for (std::vector<StoreOffer *>::iterator it = unk38.begin(); it != unk38.end(); ++it) {
        }
        mStoreMode = 0;
    }
    if (mStoreMode == 2) {
        // TheStoreMetadata.unk54 is a std::vector<const StorePackedOfferBase *>*
#define VEC (**(std::vector<const StorePackedOfferBase *> **)((char *)&TheStoreMetadata + 0x54))
        for (std::vector<const StorePackedOfferBase *>::iterator it = VEC.begin();
             it != VEC.end(); ++it) {
            unk38.push_back(
                MakeNewOffer(*it, TheStoreMetadata.mRbnOfferTable->OfferIndex(*it) != -1)
            );
        }
#undef VEC
        mStoreMode = 0;
    }
    if (mStoreMode == 3) {
        for (std::vector<int>::iterator it = StoreMetadataManager::mSetlistOffers.begin();
             it != StoreMetadataManager::mSetlistOffers.end(); ++it) {
            const StorePackedOfferBase *offer = TheStoreMetadata.FindOfferFromSongId(*it);
            if (offer != nullptr) {
                unk38.push_back(
                    MakeNewOffer(offer, TheStoreMetadata.mRbnOfferTable->OfferIndex(offer) != -1)
                );
            }
        }
        std::sort(unk38.begin(), unk38.end(), StoreOfferSort);
        for (std::vector<StoreOffer *>::iterator it = unk38.begin(); it != unk38.end(); ++it) {
        }
        mStoreMode = 0;
    }
    std::map<const StorePackedOfferBase *, bool> seen;
    DeleteAll(unk48);
    for (std::vector<StoreOffer *>::iterator it = unk38.begin(); it != unk38.end(); ++it) {
        StoreOffer *off = *it;
        if (off->mAlbum.mPackedData != nullptr) {
            if (seen.find(off->mAlbum.mPackedData) == seen.end()) {
                unk48.push_back(MakeNewOffer(off->mAlbum.mPackedData, off->mPackedData->mIsRBN));
                seen[off->mAlbum.mPackedData] = true;
            }
        }
        if (off->mPack.mPackedData != nullptr) {
            if (seen.find(off->mAlbum.mPackedData) == seen.end()) {
                unk48.push_back(MakeNewOffer(off->mPack.mPackedData, off->mPackedData->mIsRBN));
                seen[off->mPack.mPackedData] = true;
            }
        }
    }
}

void StorePanel::EnumerateOffers(bool b) {
    RELEASE(mEnum);
    int size = unk38.size();
    mEnum = new WiiEnumeration(size);
    mEnum->Start();
    Hmx::Object::HandleType(enum_start_msg);
}

void StorePanel::UpdateFromEnumProduct(StorePurchaseable *sp, const EnumProduct *ep) {
    MILO_ASSERT(sp, 0x48A);
    MILO_ASSERT(ep, 0x48B);
}

bool StorePanel::ToggleTestOffers() {
    mShowTestOffers = !mShowTestOffers;
    return mShowTestOffers;
}