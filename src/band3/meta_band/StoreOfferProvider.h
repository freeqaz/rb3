#pragma once
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include "utl/Symbol.h"
#include <vector>

class DataArray;
class StoreOffer;
class RndMat;

class StoreOfferProvider : public Hmx::Object, public UIListProvider {
public:
    struct Element {
        StoreOffer *mOffer; // 0x0
        Symbol mGroupHeading; // 0x4
        Symbol mShortcut; // 0x8
        bool mLocalize; // 0xc
    };

    Element *GetElementAtIndex(int) const;

protected:
    DataArray *mShortcuts; // 0x2c
    std::vector<StoreOffer *> *mOffers; // 0x30
    std::vector<Element *> mElements; // 0x34
    RndMat *mAlbumBgMat; // 0x40
    RndMat *mGroupBgMat; // 0x44
    RndMat *mSongBgMat; // 0x48
};
