#include "meta_band/StoreOfferContentsProvider.h"
#include "meta_band/AppLabel.h"
#include "meta/StoreOffer.h"
#include "meta/StorePackedMetadata.h"
#include "bandobj/CheckboxDisplay.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/CommerceMgr_Wii.h"
#include "os/Debug.h"
#include "ui/UILabel.h"
#include "ui/UIListLabel.h"
#include "ui/UIListCustom.h"
#include "ui/UIListMesh.h"
#include "ui/UIListSlot.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/VectorSizeDefs.h"
#include <vector>

StoreOfferContentsProvider::StoreOfferContentsProvider()
    : mElements(), mListType(kListPurchase), unk38(0), unk3c(false) {}

StoreOfferContentsProvider::~StoreOfferContentsProvider() { ClearList(); }

void StoreOfferContentsProvider::InitData(RndDir *) {}

void StoreOfferContentsProvider::Text(
    int, int col, UIListLabel *slot, UILabel *label
) const {
    AppLabel *appLabel = dynamic_cast<AppLabel *>(label);
    MILO_ASSERT(appLabel, 0x36);
    if (slot->Matches("name")) {
        appLabel->SetTextToken(mElements[col]->mSong->GetName());
    } else if (slot->Matches("downloaded")) {
        bool isDownloaded = IsActive(col);
        if (mListType == kListPurchase) isDownloaded = !isDownloaded;
        if (isDownloaded) {
            appLabel->SetIcon('0');
        } else {
            appLabel->SetTextToken(gNullStr);
        }
    } else {
        label->SetTextToken(gNullStr);
    }
}

RndMat *
StoreOfferContentsProvider::Mat(int, int, UIListMesh *mesh) const {
    return mesh->DefaultMat();
}

void StoreOfferContentsProvider::Custom(
    int, int col, UIListCustom *slot, Hmx::Object *obj
) const {
    MILO_ASSERT(slot, 0x5B);
    Element *element = mElements[col];
    if (slot->Matches("check")) {
        CheckboxDisplay *cd = dynamic_cast<CheckboxDisplay *>(obj);
        MILO_ASSERT(cd, 0x62);
        cd->SetChecked(element->mChecked);
    }
}

Symbol StoreOfferContentsProvider::DataSymbol(int) const {
    return Symbol(gNullStr);
}

bool StoreOfferContentsProvider::IsActive(int idx) const {
    bool ret = true;
    Element *element = mElements[idx];
    if (mListType == kListDownload) {
        unsigned int contentIdx = element->mSong->unka;
        int flagsA = TheStoreMetadata.GetContentStateFlags(
            element->mSong->DataTitle(), (unsigned short)(contentIdx + 1)
        );
        int flagsB = TheStoreMetadata.GetContentStateFlags(
            element->mSong->DataTitle(), contentIdx
        );
        ret = (flagsA | flagsB) & 1;
    } else if (mListType == kListPurchase) {
        ret = !(TheStoreMetadata.SongStateFlags(element->mSong) & 1);
    }
    return ret;
}

int StoreOfferContentsProvider::NumData() const { return mElements.size(); }

void StoreOfferContentsProvider::BuildList(StoreOffer *offer, ListType type) {
    ClearList();
    MILO_ASSERT(offer, 0x95);
    mOffer = offer;
    mListType = type;
    for (int i = 0; i < mOffer->NumSongs(); i++) {
        StorePackedSong *song;
        if (mOffer->mPackedData->mIsRBN) {
            song = &TheStoreMetadata.mSongTable
                        ->mSongs[mOffer->mPackedRbnOffer->mSongs[i]];
        } else {
            song = &TheStoreMetadata.mSongTable
                        ->mSongs[mOffer->mPackedOffer->mSongs[i]];
        }
        Element *element = new Element;
        if (element) {
            element->mSong = song;
            element->mChecked = false;
        }
        mElements.push_back(element);
    }
    mCurrentSongIndex = 0;
    unk3c = false;
    mSpecifiedCount = 0;
    unk38 = 0;
}

void StoreOfferContentsProvider::ClearList() {
    Element **start = mElements.begin();
    Element **end = mElements.end();
    while (start != end) {
        delete *start;
        ++start;
    }
    mElements.clear();
}

void StoreOfferContentsProvider::SetChecked(int idx, bool checked) {
    if (IsActive(idx)) {
        Element *element = mElements[idx];
        MILO_ASSERT(element, 0xB2);
        element->mChecked = checked;
    }
}

void StoreOfferContentsProvider::ToggleChecked(int idx) {
    Element *element = mElements[idx];
    MILO_ASSERT(element, 0xBB);
    SetChecked(idx, !element->mChecked);
}

void StoreOfferContentsProvider::ToggleAllChecked() {
    bool newState = !AllChecked();
    for (unsigned int i = 0; i < mElements.size(); i++) {
        SetChecked(i, newState);
    }
}

void StoreOfferContentsProvider::AcceptCurChecked() {
    std::vector<unsigned short VECTOR_SIZE_SMALL> contentUnits;
    for (unsigned int i = 0; i < mElements.size(); i++) {
        Element *element = mElements[i];
        if (element->mChecked) {
            unsigned short base = (unsigned short)element->mSong->unka;
            contentUnits.push_back(base);
            contentUnits.push_back((unsigned short)(base + 1));
        }
    }
    TheWiiCommerceMgr.SpecifyContentUnits(contentUnits);
}

void StoreOfferContentsProvider::RefreshBlocks() {
    TheWiiCommerceMgr.InitPreDownload();
    AcceptCurChecked();
    TheWiiCommerceMgr.SpecifyOffer(mOffer);
}

bool StoreOfferContentsProvider::SpecifyFirstSongContents() {
    mCurrentSongIndex = 0;
    mSpecifiedCount = 0;
    return SpecifyNextSongContents();
}

bool StoreOfferContentsProvider::SpecifyNextSongContents() {
    std::vector<unsigned short VECTOR_SIZE_SMALL> contentUnits;
    bool found = false;
    while ((unsigned int)mCurrentSongIndex < mElements.size() && !found) {
        Element *element = mElements[mCurrentSongIndex];
        if (element->mChecked) {
            unsigned short base = (unsigned short)element->mSong->unka;
            contentUnits.push_back(base);
            contentUnits.push_back((unsigned short)(base + 1));
            found = true;
        }
        mCurrentSongIndex++;
    }
    if (found) {
        mSpecifiedCount++;
        TheWiiCommerceMgr.SpecifyContentUnits(contentUnits);
    }
    return found;
}

bool StoreOfferContentsProvider::AnyChecked() {
    for (unsigned int i = 0; i < mElements.size(); i++) {
        if (IsActive(i) && mElements[i]->mChecked)
            return true;
    }
    return false;
}

bool StoreOfferContentsProvider::AllChecked() {
    for (unsigned int i = 0; i < mElements.size(); i++) {
        if (IsActive(i) && !mElements[i]->mChecked)
            return false;
    }
    return true;
}

int StoreOfferContentsProvider::NumChecked() {
    int count = 0;
    for (unsigned int i = 0; i < mElements.size(); i++) {
        if (IsActive(i) && mElements[i]->mChecked)
            count++;
    }
    return count;
}

BEGIN_HANDLERS(StoreOfferContentsProvider)
    HANDLE_ACTION(build_list, BuildList(dynamic_cast<StoreOffer *>(_msg->GetObj(2)), (ListType)_msg->Int(3)))
    HANDLE_ACTION(clear_list, ClearList())
    HANDLE_ACTION(toggle_checked, ToggleChecked(_msg->Int(2)))
    HANDLE_ACTION(toggle_all_checked, ToggleAllChecked())
    HANDLE_EXPR(any_checked, AnyChecked())
    HANDLE_EXPR(num_checked, NumChecked())
    HANDLE_EXPR(get_current_song_index, mSpecifiedCount)
    HANDLE_ACTION(accept_cur_checked, AcceptCurChecked())
    HANDLE_ACTION(refresh_blocks, RefreshBlocks())
    HANDLE_EXPR(specify_next_song_contents, SpecifyNextSongContents())
    HANDLE_EXPR(specify_first_song_contents, SpecifyFirstSongContents())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x148)
END_HANDLERS
