#pragma once
#include "obj/Msg.h"
#include "revolution/ec/ec.h"
#include "utl/VectorSizeDefs.h"
#include <vector>

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
    bool SetParentalControlPin(String pin);

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
    char unkF4; // f4
    char unkF5; // f5
    char unkF6[0x2110 - 0xf6]; // padding
    int unk2110; // 2110
    char unk2114[0x2128 - 0x2114]; // padding to ECTitleInfo
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
