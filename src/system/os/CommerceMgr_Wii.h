#pragma once
#include "obj/Msg.h"

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
    virtual void Init();

    bool InitCommerce(Hmx::Object *);
    void DestroyCommerce();
    bool IsBusy() const;
    bool CheckPurchaseSync();
    bool NeedSync();
    void GetTitleInfo();
    bool SetParentalControlPin(String pin);

    static unsigned long long MakeDataTitleId(const char *);

    static const char *mOpName[15];

    long mCommerceAsyncOpId; // 1c
    LastCommerceOperation mCommerceAsyncName; // 20
    int unk24; // 24
    Timer mCommerceTimeout; // 28
    long mOpId; // 58
    unsigned long long mTitleId; // 60
    long mPrice; // 68
    char *mAttributes[7]; // 6c
    unsigned long mAttributesNum; // 88
    unsigned long mTitleIdsNum; // 8c
    unsigned long long *mTitleIds; // 90
    int unk94[21]; // 94 - padding to unkE0
    int mProgressPercent; // e8
    int mLastErrorCode; // ec
};

extern WiiCommerceMgr TheWiiCommerceMgr;
extern int gLastErrorReturnValue;
extern char gLastErrorDesc[0x80];

char *MakeTitleIdString(unsigned long long titleId);
