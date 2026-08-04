#include "meta_band/BandStoreOffer.h"
#include "meta_band/BandSongMgr.h"
#include "meta/StoreOffer.h"
#include "meta/StorePackedMetadata.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"

BandStoreOffer::BandStoreOffer(const StorePackedOfferBase *base, SongMgr *mgr, bool b)
    : StoreOffer(base, mgr, b) {
    mUpgradeAvailable = false;
    String upgradeId = base->GetUpgradeId();
    if (!upgradeId.empty()) {
        BandSongMgr *bandSongMgr = dynamic_cast<BandSongMgr*>(mSongMgr);
        mUpgradeAvailable = mPackedData->mNumSongs != 0;
        for (int i = 0; i < mPackedData->mNumSongs; i++) {
            StorePackedSong *song;
            if (mPackedData->mIsRBN) {
                unsigned short i2 = *(unsigned short*)((const char*)mPackedData + 0x49 + i * 2);
                song = TheStoreMetadata.mSongTable->mSongs + i2;
            } else {
                unsigned short i2 = *(unsigned short*)((const char*)mPackedData + 0x43 + i * 2);
                song = TheStoreMetadata.mSongTable->mSongs + i2;
            }
            int songID = song->mSongID;
            if (bandSongMgr == NULL || bandSongMgr->GetUpgradeData(songID) == NULL) {
                mUpgradeAvailable = false;
                break;
            }
        }
    }
}

bool BandStoreOffer::IsCompletelyUnavailable() const {
    return StoreOffer::IsCompletelyUnavailable();
}

BEGIN_HANDLERS(BandStoreOffer)
    HANDLE_EXPR(has_available_upgrade, (signed char)mPackedData->mUpgradeId[0])
    HANDLE_EXPR(upgrade_purchased, mOfferState && (mOfferState->mFlags & 0x10))
    HANDLE_EXPR(upgrade_downloaded, mOfferState && (mOfferState->mFlags & 0x20))
    HANDLE_EXPR(upgrade, this)
    HANDLE_EXPR(upgrade_in_library, mUpgradeAvailable)
    HANDLE_SUPERCLASS(StoreOffer)
    HANDLE_CHECK(0x79)
END_HANDLERS
