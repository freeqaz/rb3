#include "os/CommerceMgr_Wii.h"
#include "os/ContentMgr_Wii.h"
#include "system/meta/StorePackedMetadata.h"
#include "revolution/ec/ec.h"

static char gMakeTitleIdString[5];

const char *WiiCommerceMgr::mOpName[15] = {
    "connect", "list_title_contents", "list_content_sets_price", "download_contents",
    "download_title", "list_content_sets_purchase", "purchase_data_title", "cancel",
    "list_content_sets_offers", "get_title_info", "download_title_and_contents",
    nullptr, nullptr, nullptr, nullptr
};

int gLastErrorReturnValue;
char gLastErrorDesc[0x80];

static bool gAllowNeedSyncReturn = true;

static char *gCommerceFilterName_OfferType = "offer_type";
static char *gCommerceFilterValue_Album = "album";
static char *gCommerceFilterValue_Everything = "*";
static char *gCommerceFilterValue_Pack = "pack";
static char *gCommerceFilterValue_Song = "song";
static char *gCommerceFilterValuePurchasable = "PURCHASABLE";

WiiCommerceMgr::WiiCommerceMgr()
    : mCommerceAsyncOpId(-1), mOpTimeoutMs(59000.0f), mProgressPercent(0),
      mLastErrorCode(0), unkF1(0), unkF3(0), unkF4(0), unkF5(1), unk2110(0x1FE),
      unk2150(0), unk2151(0), unk2154(0), unk2160(0) {
    mAttributes[0] = "Prices";
    mAttributes[1] = "MaxUserFileSize";
    mAttributes[2] = "MaxUserInodes";
    mAttributes[3] = gCommerceFilterName_OfferType;
    mAttributes[4] = "offer_id";
    mAttributes[5] = "index_version";
    mAttributesNum = 6;
}

WiiCommerceMgr::~WiiCommerceMgr() {}

void WiiCommerceMgr::MarkChanged(bool propagate) {
    *((char *)this + 0x4194) = 1;
    if (propagate) {
        TheWiiContentMgr.mDirty = true;
    }
}

void WiiCommerceMgr::Init() {
    SetName("commerce_mgr", ObjectDir::sMainDir);
    TheStoreMetadata.Init();
}

bool WiiCommerceMgr::IsBusy() const { return mCommerceAsyncOpId != -1; }

bool WiiCommerceMgr::NeedSync() {
    int r = EC_GetIsSyncNeeded();
    return (~((r + 0xFE2) | (-0xFE2 - r)) >> 31) & gAllowNeedSyncReturn;
}

bool WiiCommerceMgr::CheckPurchaseSync() { return true; }

void WiiCommerceMgr::GetTitleInfo() {
    for (int i = 0; i < mTitleIdsNum; i++) {
        unsigned long long titleId = mTitleIds[i];
        long r = EC_GetTitleInfo(titleId, &mTitleInfo);
        if (r != -4050) {
            const char *titleIdString = MakeTitleIdString(titleId);
            MILO_LOG(
                "Store: titleinfo: %d - titleId = %s | isTmdPresent = %d | isOnDevice = %d | type = %d | version = %d\n",
                r,
                titleIdString,
                mTitleInfo.isTmdPresent,
                mTitleInfo.isOnDevice,
                mTitleInfo.type,
                mTitleInfo.version
            );
        } else {
            const char *titleIdString = MakeTitleIdString(titleId);
            MILO_LOG("Store: titleId = %s not owned.\n", titleIdString);
        }
    }
}

bool WiiCommerceMgr::SetParentalControlPin(String pin) {
    const char *pinStr = pin.c_str();
    int ret = EC_SetParameter("PCPW", pinStr);
    if (ret == 0 || ret == -4075) return true;
    return false;
}

unsigned long long WiiCommerceMgr::MakeDataTitleId(const char *cc) {
    struct {
        union {
            unsigned long long u64parts;
            unsigned int u32parts[2];
            char charparts[0x8];
        };
    } tidParts;
    tidParts.u32parts[0] = 0x00010005;
    tidParts.u32parts[1] = 0;
    tidParts.charparts[4] = cc[0];
    tidParts.charparts[5] = cc[1];
    tidParts.charparts[6] = cc[2];
    tidParts.charparts[7] = cc[3];
    return tidParts.u64parts;
}

unsigned int FileSizeToBlocks(unsigned int fileSize, bool unk) {
    if (unk) {
        fileSize += 0x3FFF;
    }
    return fileSize >> 14;
}

char *MakeTitleIdString(unsigned long long titleId) {
    struct {
        union {
            unsigned long long u64parts;
            unsigned int u32parts[2];
            char charparts[0x8];
        };
    } tidParts;
    tidParts.u64parts = titleId;
    gMakeTitleIdString[0] = tidParts.charparts[4];
    gMakeTitleIdString[1] = tidParts.charparts[5];
    gMakeTitleIdString[2] = tidParts.charparts[6];
    gMakeTitleIdString[3] = tidParts.charparts[7];
    return gMakeTitleIdString;
}