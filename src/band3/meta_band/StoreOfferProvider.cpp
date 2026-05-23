#include "meta_band/StoreOfferProvider.h"
#include "meta/StoreOffer.h"
#include "meta_band/AppLabel.h"
#include "meta_band/BandStoreOffer.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Mat.h"
#include "ui/UIList.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIListSlot.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"

extern bool operator==(const StoreOffer *o, Symbol s);

StoreOfferProvider::StoreOfferProvider(
    std::vector<StoreOffer *> *offers, std::vector<StoreOffer *> *packs
)
    : mShortcuts(NULL),
      mOffers(offers),
      mPacks(packs),
      mElements(),
      mAlbumBgMat(NULL),
      mGroupBgMat(NULL),
      mSongBgMat(NULL) {}

StoreOfferProvider::~StoreOfferProvider() { ClearList(); }

int StoreOfferProvider::NumData() const { return mElements.size(); }

StoreOfferProvider::Element *StoreOfferProvider::GetElementAtIndex(int i) const {
    return mElements[i];
}

bool StoreOfferProvider::IsActive(int i) const {
    if (mElements.size() < 1)
        return false;
    Element *e = mElements[i];
    bool result = false;
    if (e->mOffer != NULL || e->mActive) {
        result = true;
    }
    return result;
}

Symbol StoreOfferProvider::DataSymbol(int i) const {
    if (mElements.size() != 0) {
        Element *e = mElements[i];
        if (e->mOffer) {
            return e->mOffer->ShortName();
        } else if (e->mActive) {
            return e->mGroupHeading;
        }
    }
    return gNullStr;
}

RndMat *StoreOfferProvider::Mat(int i, int j, UIListMesh *mesh) const {
    if (mElements.size() != 0) {
        StoreOffer *offer = mElements[j]->mOffer;
        if (mesh->Matches("bg")) {
            if (!offer) {
                return mGroupBgMat;
            } else if (offer->OfferType() == song) {
                return mSongBgMat;
            } else {
                return mAlbumBgMat;
            }
        }
    }
    return mesh->DefaultMat();
}

void StoreOfferProvider::InitData(RndDir *dir) {
    mAlbumBgMat = dir->Find<RndMat>("album.mat", true);
    mGroupBgMat = dir->Find<RndMat>("group.mat", true);
    mSongBgMat = dir->Find<RndMat>("song.mat", true);
}

Symbol StoreOfferProvider::PosToShortcut(int pos) {
    Element **start = &mElements[0];
    Element **it = &mElements[pos];
    while (it >= start) {
        Element *e = *it;
        if (e->mShortcut.Str() != gNullStr) {
            return e->mShortcut;
        }
        --it;
    }
    MILO_FAIL("StoreOfferProvider is missing a shortcut before index %i!", pos);
    return gNullStr;
}

int StoreOfferProvider::ShortcutToPos(Symbol s) {
    unsigned int n = mElements.size();
    for (unsigned int i = 0; i < n; i++) {
        if (mElements[i]->mShortcut == s) {
            return i;
        }
    }
    MILO_FAIL("StoreOfferProvider can't find shortcut \"%s\"!", s);
    return 0;
}

int StoreOfferProvider::PosToNextGroupPos(int pos) {
    unsigned int n = mElements.size();
    for (unsigned int i = pos + 1; i < n; i++) {
        if (mElements[i]->mGroupHeading.Str() != gNullStr) {
            return i;
        }
    }
    return 0;
}

int StoreOfferProvider::PosToPrevGroupPos(int pos) {
    for (int i = pos - 2; i >= 0; i--) {
        if (mElements[i]->mGroupHeading.Str() != gNullStr) {
            return i;
        }
    }
    int n = mElements.size();
    for (int i = n - 1; i > pos; i--) {
        if (mElements[i]->mGroupHeading.Str() != gNullStr) {
            return i;
        }
    }
    return 0;
}

void StoreOfferProvider::ClearList() {
    Element **start = &mElements[0];
    Element **end = start + mElements.size();
    for (Element **it = start; it != end; ++it) {
        delete *it;
    }
    mElements.clear();
    if (mShortcuts) {
        mShortcuts->Release();
        mShortcuts = NULL;
    }
}

const StoreOffer *StoreOfferProvider::FindPack(const StoreOffer *o) const {
    MILO_ASSERT(o->OfferType() == "song", 0x153);
    for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
         it != mOffers->end();
         ++it) {
        StoreOffer *cand = *it;
        if (cand->OfferType() == pack && cand->HasSong(o)) {
            return cand;
        }
    }
    if (mPacks) {
        for (std::vector<StoreOffer *>::iterator it = mPacks->begin();
             it != mPacks->end();
             ++it) {
            StoreOffer *cand = *it;
            if (cand->OfferType() == pack && cand->HasSong(o)) {
                return cand;
            }
        }
    }
    return NULL;
}

const StoreOffer *StoreOfferProvider::FindAlbum(const StoreOffer *o) const {
    MILO_ASSERT(o->OfferType() == "song", 0x16e);
    for (std::vector<StoreOffer *>::iterator it = mOffers->begin();
         it != mOffers->end();
         ++it) {
        StoreOffer *cand = *it;
        if (cand->OfferType() == album && cand->HasSong(o)) {
            return cand;
        }
    }
    if (mPacks) {
        for (std::vector<StoreOffer *>::iterator it = mPacks->begin();
             it != mPacks->end();
             ++it) {
            StoreOffer *cand = *it;
            if (cand->OfferType() == album && cand->HasSong(o)) {
                return cand;
            }
        }
    }
    return NULL;
}

StoreOffer *StoreOfferProvider::FindOffer(Symbol s) const {
    std::vector<StoreOffer *>::iterator it =
        std::find(mOffers->begin(), mOffers->end(), s);
    if (it == mOffers->end()) {
        if (!mPacks)
            return NULL;
        it = std::find(mPacks->begin(), mPacks->end(), s);
        if (it == mPacks->end())
            return NULL;
        return *it;
    }
    return *it;
}

void StoreOfferProvider::BuildList(DataArray *) {}

void StoreOfferProvider::Text(int i, int pos, UIListLabel *listLabel, UILabel *label)
    const {
    AppLabel *appLabel = dynamic_cast<AppLabel *>(label);
    MILO_ASSERT(appLabel, 0x36);

    if (mElements.size() < 1) {
        appLabel->SetTextToken(gNullStr);
        return;
    }

    Element *e = mElements[pos];
    StoreOffer *offer = e->mOffer;
    UIListSlot *slot = (UIListSlot *)listLabel;
    if (offer) {
        if (slot->Matches("album")) {
            if (offer->OfferType() == album || offer->OfferType() == pack) {
                appLabel->SetOfferName(offer);
                return;
            }
        } else if (slot->Matches("song")) {
            if (offer->OfferType() == song) {
                appLabel->SetOfferName(offer);
                return;
            }
        } else if (slot->Matches("rbn_icon")) {
            if (offer->mPackedData->mIsRBN) {
                label->SetIcon(0x55);
                return;
            }
        } else if (slot->Matches("cost")) {
            if (!(offer->mOfferState && (offer->mOfferState->mFlags & 1)) && !offer->InLibrary() &&
                !offer->IsCompletelyUnavailable()) {
                appLabel->SetOfferCost(offer);
                return;
            }
        } else if (slot->Matches("new")) {
            if (!(offer->mOfferState && (offer->mOfferState->mFlags & 1)) && !offer->InLibrary() &&
                offer->IsNewRelease() && !offer->IsCompletelyUnavailable()) {
                appLabel->SetTextToken(store_new);
                return;
            }
        } else if (slot->Matches("purchased")) {
            BandStoreOffer *bso = dynamic_cast<BandStoreOffer *>(offer);
            MILO_ASSERT(bso, 0x7c);
            bool isPurchased = bso->IsPurchased();
            bool inLibrary = bso->InLibrary();
            bool isDownloaded =
                offer->mOfferState && (offer->mOfferState->mFlags & 2);
            bool upgradeAvail = bso->mUpgradeAvailable;
            if (!inLibrary) {
                if (isDownloaded) {
                    if (offer->mOfferState->mFlags & 2) {
                        appLabel->SetTextToken(store_upgrade_in_library);
                    }
                    return;
                }
                if (isPurchased) {
                    appLabel->SetTextToken(store_upgrade_purchased);
                    return;
                }
                appLabel->SetTextToken(store_upgrade_available);
                return;
            }
            if (!isPurchased && !upgradeAvail) {
                return;
            }
            if (upgradeAvail) {
                appLabel->SetTextToken(store_upgrade_available);
                return;
            }
            if (isPurchased) {
                if (offer->mOfferState->mFlags & 2) {
                    appLabel->SetTextToken(store_downloaded);
                } else {
                    appLabel->SetTextToken(store_purchased);
                }
                return;
            }
            appLabel->SetTextToken(store_in_library);
            return;
        }
    } else {
        if (e->mActive == 0) {
            if (slot->Matches("group") && e->mIsCover == 0) {
                appLabel->SetStoreGroupName(this, pos);
                return;
            } else if (slot->Matches("famousby") && e->mIsCover != 0) {
                appLabel->SetTextToken(store_famous_by);
                return;
            } else if (slot->Matches("famousby_group") && e->mIsCover != 0) {
                appLabel->SetStoreGroupName(this, pos);
                return;
            }
        } else {
            if (slot->Matches("group_center") && e->mActive != 0) {
                appLabel->SetStoreGroupName(this, pos);
                return;
            }
        }
    }
    appLabel->SetTextToken(gNullStr);
}

DataNode StoreOfferProvider::Handle(DataArray *msg, bool warn) {
    return Hmx::Object::Handle(msg, warn);
}
