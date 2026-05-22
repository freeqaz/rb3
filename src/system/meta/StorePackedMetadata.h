#pragma once
#include "obj/Object.h"
#include "obj/Msg.h"
#include "meta/StoreEnumeration.h"
#include "meta/StoreOffer.h"

class StoreTitleContentState;

DECLARE_MESSAGE(CommerceMgrOpCompleteMsg, "commerce_mgr_op_complete")
END_MESSAGE

class StoreMarqueeTable {
public:
    ~StoreMarqueeTable();
    bool Load(const char *);

    char *mBuffer; // 0x0
};

class StorePackedRedemptionOffer {
public:
    void EndianFix();

    unsigned short mOfferIndex; // 0x0
    char mTitleId[4]; // 0x2
};

class StoreRedemptionsTable : public std::vector<StorePackedOffer *> {
public:
    StoreRedemptionsTable() {}
    bool Load(const char *);
};

#pragma push
#pragma pack(1)
class StoreVersionHeader {
public:
    bool LoadFile(const char *);

    unsigned char mVersion; // 0x0
    unsigned short mBuildNumber; // 0x1
    unsigned char mCompressed; // 0x3
};
#pragma pop

class StoreOfferTable {
public:
    ~StoreOfferTable();
    bool Load(const char *);
    int OfferIndex(const StorePackedOfferBase *) const;

    int mNumOffers; // 0x0
    char *mBuffer; // 0x4
    StorePackedOffer **mOffers; // 0x8
    StoreOfferState *mBufferNewRelease; // 0xc
    unsigned short mNumNewReleases; // 0x10
    unsigned short *mNewReleases; // 0x14
};

class StoreRbnOfferTable {
public:
    ~StoreRbnOfferTable();
    bool Load(const char *);
    int OfferIndex(const StorePackedOfferBase *) const;

    int mNumOffers; // 0x0
    char *mBuffer; // 0x4
    StorePackedRBNOffer **mOffers; // 0x8
    StoreOfferState *mBufferNewRelease; // 0xc
    unsigned short mNumNewReleases; // 0x10
    unsigned short *mNewReleases; // 0x14
};

class StoreSingleStringTable {
public:
    ~StoreSingleStringTable() {
        if (mBuffer)
            _MemFree(mBuffer);
    }
    bool LoadFile(const char *);
    const char *GetString(int idx) const {
        if (idx < 0 || idx >= mNumStrings)
            return "STRING INDEX OUT OF BOUNDS";
        else
            return mStrings[idx];
    }

    int mNumStrings; // 0x0
    char *mBuffer; // 0x4
    char **mStrings; // 0x8
};

class StoreStringTable {
public:
    ~StoreStringTable() {}
    bool Load(const char *);
    bool IsValid(int);

    StoreSingleStringTable mNonLocalized; // 0x0
    StoreSingleStringTable mLocalized; // 0xc
};

class StorePackedPage {
public:
    void EndianFix();
    Symbol DefaultSort() const;

    int unk0;
    unsigned char unk4;
    unsigned char unk5;
    unsigned short unk6p0 : 4;
    unsigned short unk6p1 : 4;
    unsigned short mDefaultSort : 4;
    unsigned short mHasOffers : 1;
    unsigned short unk6p3 : 3;
    unsigned char mNumOffers; // 0x8
};

class StorePackedSubMenu {
public:
    unsigned short unk0;
    unsigned short unk2;
    unsigned short unk4;
    unsigned short unk6;
};

class StorePage {
public:
    StorePage() : mPageNumber(0), mPage(0), mOffers(0) {}
    char *LoadFromBuffer(char *, unsigned short);
    StorePackedOffer *Offer(int) const;
    const StorePackedOfferBase *BaseOffer(int) const;
    StorePackedRBNOffer *RbnOffer(int) const;
    class StorePackedSubMenu *Submenu(int) const;

    int mPageNumber; // 0x0
    StorePackedPage *mPage; // 0x4
    union {
        unsigned short *mOffers;
        class StorePackedSubMenu *mSubmenus;
    }; // 0x8
};

class StorePageTable {
public:
    ~StorePageTable();
    StorePage *GetPage(unsigned short);

    int mNumOffsets; // 0x0
    int mNumPages; // 0x4
    char *mBuffer; // 0x8
    StorePage *mPages; // 0xc
    std::map<unsigned short, StorePage *> mPageLookup; // 0x10
};

class StoreMetadataManager : public Hmx::Object {
public:
    StoreMetadataManager()
        : mFlags(0), mLoadingState(0), mBasePath(), mVersion(0), mStringTable(0),
          mSongTable(0), mOfferTable(0), mRbnOfferTable(0), mPageTable(0),
          mCurrentPage(0), mMarqueeTable(0), mRedemptionsTable(0) {}
    ~StoreMetadataManager();
    virtual DataNode Handle(DataArray *, bool);

    StoreOfferState *GetOfferStatus(const class StorePackedOfferBase *);
    void Init();
    void UpdateOfferOwnership();
    void Load(const char *);
    void SetLoadingState(int);
    void Unload();
    void MarkDownloaded(unsigned long long, unsigned short);
    bool LoadingFailed() const;
    StorePage *LoadPage(unsigned short);
    StorePage *LoadDynamicPage(DataArray *);
    const StorePackedOfferBase *FindOfferFromSongId(int) const;
    const char *GetString(int idx) const {
        StoreStringTable *table = mStringTable;
        if (idx & 0x8000)
            return table->mLocalized.GetString((idx & 0x7FFF) - 1);
        else
            return table->mNonLocalized.GetString(idx - 1);
    }
    StoreError LoadError() const;
    void SetMetadataIndex(unsigned long long, unsigned short, long);
    const StorePackedOfferBase *GetOffer(unsigned short) const;
    bool Poll();
    void PollLoading();
    void UpdateOfferPrices();
    void UpdateAvailability();
    int SongStateFlags(const StorePackedSong *);
    int GetContentStateFlags(unsigned long long, unsigned short);
    int GetContentFileSize(unsigned long long, unsigned short);
    void MarkDeleted(unsigned long long, unsigned short);
    void AddOldMetadataIndex(unsigned long long, unsigned short);
    const StorePackedOfferBase *FindOffer(const char *, int *, bool *, bool *) const;
    void AddSetlistOffer(int);
    void ClearSetlistOffers();

    DataNode OnMsg(const CommerceMgrOpCompleteMsg &);
    void DebugPurchase();
    void DebugDownload();

    static std::vector<int> mSetlistOffers;

    unsigned int mFlags; // 0x1c
    int mLoadingState; // 0x20 - loading state
    int mContentSize; // 0x24
    String mBasePath; // 0x28
    StoreVersionHeader *mVersion; // 0x34
    StoreStringTable *mStringTable; // 0x38
    StoreSongTable *mSongTable; // 0x3c
    StoreOfferTable *mOfferTable; // 0x40
    StoreRbnOfferTable *mRbnOfferTable; // 0x44
    StorePageTable *mPageTable; // 0x48
    StorePage *mCurrentPage; // 0x4c
    StoreMarqueeTable *mMarqueeTable; // 0x50
    StoreRedemptionsTable *mRedemptionsTable; // 0x54
    std::map<unsigned long long, StoreTitleContentState *> unk58;
    int unk70;
    int unk74;
    // this here is a struct - StorePage
    int unk78;
    int unk7c; // 0x7c - some other buffer
    int unk80; // 0x80 - yet another buffer
    // end struct
    int mErrorMsg; // 0x84
    int unk88;
    int unk8c;
    unsigned short unk90;
    int unk94;
    std::list<std::pair<unsigned long long, unsigned short> > unk98;
    int unka0;
};

extern StoreMetadataManager TheStoreMetadata;
extern const char *gStoreMetadataManagerLoadStepName[12];