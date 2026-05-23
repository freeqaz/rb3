#include "meta_band/StoreOfferProvider.h"
#include "meta/StoreOffer.h"
#include "meta_band/AppLabel.h"
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

StoreOfferProvider::~StoreOfferProvider() {
    ClearList();
    if (mShortcuts) {
        mShortcuts->Release();
        mShortcuts = NULL;
    }
}

int StoreOfferProvider::NumData() const { return mElements.size(); }

StoreOfferProvider::Element *StoreOfferProvider::GetElementAtIndex(int i) const {
    return mElements[i];
}

bool StoreOfferProvider::IsActive(int i) const {
    if (mElements.size() < 1)
        return false;
    Element *e = mElements[i];
    bool result = false;
    if (e->mOffer != NULL || e->mLocalize) {
        result = true;
    }
    return result;
}

Symbol StoreOfferProvider::DataSymbol(int i) const {
    if (mElements.size() != 0) {
        Element *e = mElements[i];
        if (e->mOffer) {
            return e->mOffer->ShortName();
        } else if (e->mLocalize) {
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
        if ((*it)->mShortcut.Str() != gNullStr) {
            return (*it)->mShortcut;
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
    for (std::vector<Element *>::iterator it = mElements.begin();
         it != mElements.end();
         ++it) {
        delete *it;
    }
    mElements.clear();
    if (mShortcuts) {
        mShortcuts->Release();
        mShortcuts = NULL;
    }
}

const StoreOffer *StoreOfferProvider::FindPack(const StoreOffer *o) const {
    return NULL;
}

const StoreOffer *StoreOfferProvider::FindAlbum(const StoreOffer *o) const {
    return NULL;
}

StoreOffer *StoreOfferProvider::FindOffer(Symbol s) const { return NULL; }

void StoreOfferProvider::BuildList(DataArray *) {}

void StoreOfferProvider::Text(int, int, UIListLabel *, UILabel *) const {}

DataNode StoreOfferProvider::Handle(DataArray *msg, bool warn) {
    return Hmx::Object::Handle(msg, warn);
}
