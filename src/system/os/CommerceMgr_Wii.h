#pragma once
#include "obj/Msg.h"
#include "revolution/ec/ec.h"
#include "utl/Str.h"
#include "utl/VectorSizeDefs.h"
#include <map>
#include <vector>

struct ECLicensePricing {
    unsigned long long titleId; // 0x0
    unsigned long price;        // 0x8
    unsigned long nIndexes;     // 0xc
    unsigned short *indexes;    // 0x10
};

struct ECContentCatalogInfo {
    unsigned long long titleId;         // 0x0
    void *ratings;                      // 0x8
    unsigned long nRatings;             // 0xc
    ECLicensePricing *licensePricings;  // 0x10
    unsigned long nLicensePricings;     // 0x14
    void *attributes;                   // 0x18
    unsigned long nAttributes;          // 0x1c
    long isTitleIncluded;               // 0x20
    unsigned short *indexes;            // 0x24
    unsigned long nIndexes;             // 0x28
};

const char *GetAttributeStr(const ECContentCatalogInfo *, const char *);
void DebugPrint(ECContentCatalogInfo *);

class WiiCommerceMgr : public MsgSource {
public:
    enum LastCommerceOperation {
        kConnect,
        kListTitleContents,
        kListContentSetsPrice,
        kDownloadContents,
        kDownloadTitle,
        kListContentSetsPurchase,
        kPurchaseDataTitle,
        kCancel,
        kListContentSetsOffers,
        kGetTitleInfo,
        kDownloadTitleAndContents
    };
    WiiCommerceMgr();
    virtual DataNode Handle(DataArray *, bool);
    virtual ~WiiCommerceMgr();
    void Init();

    bool InitCommerce(Hmx::Object *);
    void DestroyCommerce();
    bool IsBusy() const;
    bool CheckPurchaseSync();
    bool NeedSync();
    void GetTitleInfo();
    void MarkChanged(bool);
    bool SetParentalControlPin(String pin);
    void InitPreDownload();
    void SpecifyOffer(class StorePurchaseable *);
    void SpecifyContentUnits(
        const std::vector<unsigned short VECTOR_SIZE_SMALL> &
    );
    void QueryOffers(
        unsigned long long titleId,
        std::map<unsigned long, bool> *outMap,
        Hmx::Object *callback
    );
    bool RequestOffers(Hmx::Object *callback);

    static unsigned long long MakeDataTitleId(const char *);

    static const char *mOpName[15];

    long mCommerceAsyncOpId; // 1c
    LastCommerceOperation mCommerceAsyncName; // 20
    int unk24; // 24
    Timer mCommerceTimeout; // 28
    float mOpTimeoutMs; // 58
    char unk5c[4]; // 5c
    unsigned long long mTitleId; // 60
    long mPrice; // 68
    char *mAttributes[7]; // 6c
    unsigned long mAttributesNum; // 88
    unsigned long long *mTitleIds; // 8c
    unsigned long mTitleIdsNum; // 90
    char unk94[0xe8 - 0x94]; // 94 - padding to mProgressPercent
    int mProgressPercent; // e8
    int mLastErrorCode; // ec
    char unkF0; // f0
    char unkF1; // f1
    char unkF2; // f2
    char unkF3; // f3
    unsigned char unkF4; // f4
    char unkF5; // f5
    char unkF6[0x2110 - 0xf6]; // padding
    int unk2110; // 2110
    char unk2114[0x2120 - 0x2114]; // padding
    ECContentCatalogInfo *mCatalogInfos; // 0x2120
    unsigned long mNumCatalogInfos; // 0x2124
    ECTitleInfo mTitleInfo; // 0x2128, sizeof = 0x28
    char unk2150; // 2150
    char unk2151; // 2151
    char unk2152[0x2154 - 0x2152]; // padding
    int unk2154; // 2154
    std::vector<unsigned short VECTOR_SIZE_SMALL> mContentUnits; // 0x2158, sizeof = 8
    int unk2160; // 2160
    char unk2164[0x41a8 - 0x2164]; // padding to virtual base
};

extern WiiCommerceMgr TheWiiCommerceMgr;
extern int gLastErrorReturnValue;
extern char gLastErrorDesc[0x80];

char *MakeTitleIdString(unsigned long long titleId);
