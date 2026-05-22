#include "meta/StorePackedMetadata.h"
#include "meta/StoreOffer.h"
#include "os/CommerceMgr_Wii.h"
#include "os/File.h"
#include "obj/ObjMacros.h"
#include "utl/Compress.h"
#include "utl/MemMgr.h"
#include "utl/Symbols2.h"
#include "sdk/RVL_SDK/revolution/cnt/cnt.h"
#include "sdk/RVL_SDK/revolution/nand/nand.h"

extern "C" int NANDGetStatus(const char *, NANDStatus *);

void CM_CNTSDCacheClearRSO();

StoreMetadataManager TheStoreMetadata;
std::vector<int> StoreMetadataManager::mSetlistOffers;
bool gDebugMakeAllSongsAvailable;
bool gDebugDontRelyOnCommerceServer;
int gStoreUseCompressedFiles;
const char *gStoreMetadataManagerLoadStepName[12];

class StoreIndexFileReader : public File {
public:
    StoreIndexFileReader() : mSize(-1) {}
    virtual ~StoreIndexFileReader() {
        if (mSize >= 0)
            CNTClose(&mFileInfo);
    }
    virtual int Read(void *, int);
    virtual bool ReadAsync(void *, int);
    virtual int Write(const void *, int);
    virtual int Seek(int, int);
    virtual int Tell();
    virtual void Flush();
    virtual bool Eof();
    virtual bool Fail();
    virtual int Size();
    virtual int UncompressedSize();
    virtual bool ReadDone(int &);
    virtual int GetFileHandle(DVDFileInfo *&);

    static void Init(unsigned long long, unsigned short);

    CNTFileInfo mFileInfo; // 0x4
    int mSize; // 0x48

    static CNTHandle mMetaDataCntHandle;
    static bool mMetaDataCntHandleInited;
};

CNTHandle StoreIndexFileReader::mMetaDataCntHandle;
bool StoreIndexFileReader::mMetaDataCntHandleInited;

int StoreIndexFileReader::Read(void *buf, int len) { return CNTRead(&mFileInfo, buf, len); }
bool StoreIndexFileReader::ReadAsync(void *, int) { return false; }
int StoreIndexFileReader::Write(const void *, int) { return 0; }
int StoreIndexFileReader::Seek(int, int) { return 0; }
int StoreIndexFileReader::Tell() { return 0; }
void StoreIndexFileReader::Flush() {}
bool StoreIndexFileReader::Eof() { return false; }
bool StoreIndexFileReader::Fail() { return false; }
int StoreIndexFileReader::Size() { return mSize; }
int StoreIndexFileReader::UncompressedSize() { return 0; }
bool StoreIndexFileReader::ReadDone(int &) { return false; }
int StoreIndexFileReader::GetFileHandle(DVDFileInfo *&) { return 0; }

bool StoreLoadPackedFile(
    const char *filename, bool compressed, int maxSize, bool relocate, bool extendedHeader,
    char **outBuf, char **outDataStart, char **outDataEnd, int *outCount
) {
    File *file;
    if (!(TheStoreMetadata.mFlags & 1)) {
        StoreIndexFileReader *reader = new StoreIndexFileReader();
        if (CNTOpen(&StoreIndexFileReader::mMetaDataCntHandle, filename, &reader->mFileInfo) != 0) {
            delete reader;
            reader = NULL;
        } else {
            reader->mSize = CNTGetLength(&reader->mFileInfo);
        }
        file = reader;
    } else {
        file = NewFile(filename, 2);
    }
    if (file == NULL) {
        MILO_LOG("Store: file %s is missing\n", filename);
        return false;
    }
    int fileSize = file->Size();
    int alignedSize = (fileSize + 0x1F) & ~0x1F;
    if (fileSize > maxSize) {
        MILO_LOG("Store: file %s is over budget (%d > %d)\n", filename, fileSize, maxSize);
        delete file;
        return false;
    }
    if (compressed && gStoreUseCompressedFiles) {
        char *rawBuf = (char *)_MemAllocTemp(alignedSize, 0x20);
        if (rawBuf == NULL) {
            MILO_LOG("Store: Failed to allocated %d byte buffer for store file %s.\n", fileSize, filename);
            delete file;
            return false;
        }
        char *decompBuf = (char *)_MemAllocTemp(maxSize, 0x20);
        if (decompBuf == NULL) {
            MILO_LOG("Store: Failed to allocated %d byte buffer for decompressing store file %s.\n", maxSize, filename);
            _MemFree(rawBuf);
            delete file;
            return false;
        }
        file->Read(rawBuf, alignedSize);
        delete file;
        int decompSize = maxSize;
        DecompressMem(rawBuf, fileSize, decompBuf, decompSize, true, NULL);
        _MemFree(rawBuf);
        *outBuf = (char *)_MemAlloc(decompSize, 4);
        memcpy(*outBuf, decompBuf, decompSize);
        _MemFree(decompBuf);
        *outDataStart = *outBuf;
        *outDataEnd = *outBuf + decompSize;
    } else {
        *outBuf = (char *)_MemAlloc(alignedSize, 0x20);
        if (*outBuf == NULL) {
            MILO_LOG("Store: Failed to allocated %d byte buffer for store file %s.\n", alignedSize, filename);
            delete file;
            return false;
        }
        *outDataEnd = *outBuf + fileSize;
        file->Read(*outBuf, alignedSize);
        *outDataStart = *outBuf;
        delete file;
    }
    if (outCount != NULL) {
        *outCount = *(unsigned short *)*outDataStart;
        if (extendedHeader) {
            *outDataStart += 4;
        } else {
            *outDataStart += 2;
        }
        if (relocate) {
            char **ptr = (char **)*outDataStart;
            for (int i = 0; i < *outCount; i++) {
                if (*ptr != NULL) {
                    *ptr += (int)*outBuf;
                }
                ptr++;
            }
        }
    }
    return true;
}

bool StoreStringTable::Load(const char *cc) {
    char buf[256];
    sprintf(buf, "%sstrings", cc);
    bool nonloc = mNonLocalized.LoadFile(buf);
    sprintf(buf, "%sstrings_%s", cc, SystemLanguage().mStr);
    return nonloc && mLocalized.LoadFile(buf);
}

bool StoreStringTable::IsValid(int i) {
    if (i & 0x8000U) {
        if (mLocalized.mNumStrings <= (i & 0x7FFFU))
            return false;
        else
            return true;
    } else
        return i >= 0 && i < mNonLocalized.mNumStrings;
}

const char *StorePackedSong::GetShortName() const {
    return TheStoreMetadata.GetString(unk4);
}

const char *StorePackedSong::GetName() const {
    return TheStoreMetadata.GetString(mNameIndex);
}

const char *StorePackedSong::GetArtist() const {
    return TheStoreMetadata.GetString(mArtistIndex);
}

const char *StorePackedSong::GetDataTitle() const {
    return MakeString("%c%c%c%c", unk6, unk7, unk8, unk9);
}

const char *StorePackedSong::GetUpgradeDataTitle() const {
    return MakeString("%c%c%c%c", unkc, unkd, unke, unkf);
}

StoreSongTable::~StoreSongTable() {
    if (mBuffer)
        _MemFree(mBuffer);
}

bool StoreSongTable::Load(const char *cc) {
    char buf[256];
    sprintf(buf, "%ssongs", cc);

    int *loc12c;
    char *loc130;
    bool ret = StoreLoadPackedFile(
        buf, true, 0x40000, false, true, &mBuffer, (char **)&loc12c, &loc130, &mNumSongs
    );
    if (!ret)
        return ret;
    else {
        loc12c += mNumSongs;
        int diff = loc130 - (char *)loc12c;
        int u1 = diff / 0x1cul;
        if (u1 != mNumSongs) {
            MILO_LOG(
                "There are %d bytes left in song file, at %d bytes per song is %d songs, but the file says there are %d songs.\n",
                diff,
                0x1cul,
                u1,
                mNumSongs
            );
        }
        mSongs = (StorePackedSong *)(char *)loc12c;
        StorePackedSong *song;
        for (int i = 0; i < mNumSongs; i++) {
            song = &mSongs[i];
            song->EndianFix();
            if (!TheStoreMetadata.mStringTable->IsValid(song->mNameIndex)) {
                MILO_LOG("Song %d: name %d is invalid\n", i, (int)song->mNameIndex);
            }
            if (!TheStoreMetadata.mStringTable->IsValid(song->mArtistIndex)) {
                MILO_LOG("Song %d: artist %d is invalid\n", i, (int)song->mArtistIndex);
            }
        }
        return true;
    }
}

bool StorePackedOfferBase::IsVariousArtist() const {
    if (mNumSongs < 2)
        return false;
    unsigned int isRbn = mIsRBN;
    StorePackedSong *first;
    if (isRbn)
        first = &TheStoreMetadata.mSongTable->mSongs[((StorePackedRBNOffer *)this)->mSongs[0]];
    else
        first = &TheStoreMetadata.mSongTable->mSongs[((StorePackedOffer *)this)->mSongs[0]];
    unsigned short artist = first->mArtistIndex;
    for (int i = 1; i < mNumSongs; i++) {
        StorePackedSong *song;
        if (isRbn)
            song = &TheStoreMetadata.mSongTable->mSongs[((StorePackedRBNOffer *)this)->mSongs[i]];
        else
            song = &TheStoreMetadata.mSongTable->mSongs[((StorePackedOffer *)this)->mSongs[i]];
        if (song->mArtistIndex != artist)
            return true;
    }
    return false;
}

String StorePackedOfferBase::GetOfferId() const {
    String ret;
    ret.reserve(0x11);
    memcpy((void *)ret.c_str(), mId, 16);
    char *ptr = (char *)ret.c_str();
    ptr[16] = 0;
    return ret;
}

String StorePackedOfferBase::GetUpgradeId() const {
    String ret;
    ret.reserve(0x11);
    memcpy((void *)ret.c_str(), mUpgradeId, 16);
    char *ptr = (char *)ret.c_str();
    ptr[16] = 0;
    return ret;
}

const char *StorePackedOfferBase::GetName() const {
    return TheStoreMetadata.GetString(mNameIndex);
}

StoreOfferTable::~StoreOfferTable() {
    if (mBuffer) {
        _MemFree(mBuffer);
        mBuffer = 0;
    }
    delete[] mBufferNewRelease;
    mBufferNewRelease = 0;
}

#define BYTES_PER_OFFER 69UL

struct test {
    StorePackedOffer **n;
    StorePackedOffer **b[4];
};

bool StoreOfferTable::Load(const char *cc) {
    char buf[256];
    sprintf(buf, "%soffers", cc);
    test t;
    StorePackedOffer **loc130;
    bool ret = StoreLoadPackedFile(
        buf,
        true,
        0x40000,
        true,
        true,
        &mBuffer,
        (char **)&t.n,
        (char **)&loc130,
        &mNumOffers
    );
    if (!ret)
        return ret;
    else {
        int diff = (int)loc130 - (int)t.n;
        int actualNumOffers = diff / BYTES_PER_OFFER;
        if (actualNumOffers < mNumOffers) {
            MILO_LOG(
                "There are %d bytes left in offers file, at %d bytes per offer is %d offers, but the file says there are %d offers.\n",
                diff,
                BYTES_PER_OFFER,
                actualNumOffers,
                mNumOffers
            );
        }
        mOffers = t.n;
        void *buf = new StoreOfferState[mNumOffers];
        mBufferNewRelease = (StoreOfferState *)buf;
        memset(buf, 0, mNumOffers * sizeof(StoreOfferState));

        t.n += mNumOffers;
        t.b[0] = 0;
        t.b[1] = 0;
        t.b[2] = 0;
        for (int i = 0; i < mNumOffers; i++) {
            StorePackedOffer *curOffer = mOffers[i];
            curOffer->EndianFix();
            (((char **)&t.b[0])[curOffer->OfferType()])++;
        }
        return true;
    }
}

int StoreOfferTable::OfferIndex(const StorePackedOfferBase *base) const {
    for (int i = 0; i < mNumOffers; i++) {
        if (base == mOffers[i])
            return i;
    }
    return -1;
}

StoreRbnOfferTable::~StoreRbnOfferTable() {
    if (mBuffer) {
        _MemFree(mBuffer);
        mBuffer = 0;
    }
    delete[] mBufferNewRelease;
    mBufferNewRelease = 0;
}

bool StoreRbnOfferTable::Load(const char *cc) {
    char buf[256];
    sprintf(buf, "%srbn_offers", cc);
    char *dataStart;
    char *dataEnd;
    bool ret = StoreLoadPackedFile(
        buf,
        true,
        0x40000,
        true,
        true,
        &mBuffer,
        &dataStart,
        &dataEnd,
        &mNumOffers
    );
    if (!ret)
        return ret;
    else {
        int diff = (int)dataEnd - (int)dataStart;
        int actualNumOffers = diff / 75UL;
        if (actualNumOffers < mNumOffers) {
            MILO_LOG(
                "There are %d bytes left in rbn_offers file, at %d bytes per offer is %d offers, but the file says there are %d offers.\n",
                diff,
                75UL,
                actualNumOffers,
                mNumOffers
            );
        }
        mOffers = (StorePackedRBNOffer **)dataStart;
        void *buf = new StoreOfferState[mNumOffers];
        mBufferNewRelease = (StoreOfferState *)buf;
        memset(buf, 0, mNumOffers * sizeof(StoreOfferState));
        for (int i = 0; i < mNumOffers; i++) {
            mOffers[i]->EndianFix();
        }
        return true;
    }
}

int StoreRbnOfferTable::OfferIndex(const StorePackedOfferBase *base) const {
    for (int i = 0; i < mNumOffers; i++) {
        if (base == mOffers[i])
            return i;
    }
    return -1;
}

void StorePackedRedemptionOffer::EndianFix() {
    mOfferIndex -= 1;
}

static bool CheckOtherTitlesExistence(const char *titleId) {
    char path[128];
    sprintf(
        path,
        "/title/00010000/%02x%02x%02x%02x/data/",
        titleId[0],
        titleId[1],
        titleId[2],
        titleId[3]
    );
    bool exists = true;
    if (!titleId[0] && !titleId[1] && !titleId[2] && !titleId[3]) {
        exists = false;
    } else {
        NANDStatus status;
        if (NANDGetStatus(path, &status) < 0)
            exists = false;
    }
    return exists;
}

bool StoreRedemptionsTable::Load(const char *cc) {
    char buf[256];
    sprintf(buf, "%sredemption_offers", cc);
    char *buffer;
    int numEntries;
    char *dataStart;
    char *dataEnd;
    bool ret = StoreLoadPackedFile(
        buf, true, 0x20000, false, false, &buffer, &dataStart, &dataEnd, &numEntries
    );
    if (!ret)
        return ret;
    unsigned long byteCount = numEntries * 6UL;
    unsigned int diff = (unsigned int)(dataEnd - dataStart);
    if (diff != byteCount) {
        MILO_LOG(
            "Redemption file says is has %d entries, but at %d bytes each that would take %d bytes, and there are %d bytes left in the file.\n",
            numEntries,
            6UL,
            byteCount,
            (int)diff
        );
        numEntries = diff / 6;
    }
    int numValid = 0;
    unsigned short *idxPtr = (unsigned short *)dataStart;
    StorePackedRedemptionOffer *offer = (StorePackedRedemptionOffer *)dataStart;
    unsigned short *validIndices = (unsigned short *)dataStart;
    for (int i = 0; i < numEntries; i++) {
        offer->EndianFix();
        if (CheckOtherTitlesExistence(offer->mTitleId)) {
            numValid++;
            *validIndices = offer->mOfferIndex;
            validIndices++;
        }
        offer = (StorePackedRedemptionOffer *)((char *)offer + 6);
    }
    reserve(numValid);
    for (int i = 0; i < numValid; i++) {
        int idx = *idxPtr;
        if (idx < TheStoreMetadata.mOfferTable->mNumOffers) {
            push_back(TheStoreMetadata.mOfferTable->mOffers[idx]);
        }
        idxPtr++;
    }
    _MemFree(buffer);
    return true;
}

void StoreRedemptionsTable::AddRedeemedOffer(const char *cc) {
    int index;
    bool isRbn;
    bool isUpgrade;
    bool found = TheStoreMetadata.FindOffer(cc, &index, &isRbn, &isUpgrade);
    if (!found || isUpgrade) {
        MILO_LOG("Redemption offer %s not found in store metadata.\n", cc);
        return;
    }
    StorePackedOffer *offer;
    if (isRbn)
        offer = (StorePackedOffer *)TheStoreMetadata.mRbnOfferTable->mOffers[index];
    else
        offer = TheStoreMetadata.mOfferTable->mOffers[index];
    for (iterator it = begin(); it != end(); ++it) {
        if (*it == offer)
            return;
    }
    push_back(offer);
}

Symbol StorePackedPage::DefaultSort() const {
    switch (mDefaultSort) {
    case 1:
        return "by_artist";
    case 2:
        return "by_song_first_letter";
    case 3:
        return "by_subgenre";
    case 4:
        return "by_year_released";
    case 5:
        return "by_author";
    case 6:
        return "by_label";
    case 7:
        return "by_difficulty";
    case 8:
        return "by_review";
    case 9:
        return "by_release_date";
    case 10:
        return "by_pack_first_letter";
    default:
        return "by_artist";
    }
}

char *StorePage::LoadFromBuffer(char *buffer, unsigned short num) {
    char *p = buffer + 9;
    int mult;
    mPageNumber = num;
    mPage = (StorePackedPage *)buffer;
    mPage->EndianFix();
    if (mPage->mHasOffers) {
        mOffers = (unsigned short *)p;
        mult = 2;
        for (int i = 0; i < mPage->mNumOffers; i++) {
            mOffers[i] -= 1;
        }
    } else {
        mOffers = (unsigned short *)p;
        mult = 8;
    }
    return p + mPage->mNumOffers * mult;
}

StorePackedOffer *StorePage::Offer(int idx) const {
    if (mPage->mHasOffers) {
        int key = mOffers[idx];
        if (!(key & 0x8000))
            return TheStoreMetadata.mOfferTable->mOffers[key];
    }
    return nullptr;
}

bool StorePageTable::Load(const char *cc) {
    char buf[256];
    sprintf(buf, "%spages", cc);
    char *dataStart;
    char *dataEnd;
    bool ret = StoreLoadPackedFile(
        buf, true, 0x80000, true, true, &mBuffer, &dataStart, &dataEnd, &mNumOffsets
    );
    if (!ret)
        return ret;
    mNumPages = 0;
    char **offsets = (char **)dataStart;
    for (int i = 0; i < mNumOffsets; i++) {
        if (offsets[i] != NULL)
            mNumPages++;
    }
    mPageLookup.clear();
    mPages = new StorePage[mNumPages];
    int pageIdx = 0;
    for (int i = 0; i < mNumOffsets; i++) {
        if (offsets[i] != NULL) {
            StorePage *page = &mPages[pageIdx];
            pageIdx++;
            page->LoadFromBuffer(offsets[i], i + 1);
            mPageLookup[i + 1] = page;
        }
    }
    return true;
}

StorePage *StorePageTable::GetPage(unsigned short idx) {
    std::map<unsigned short, StorePage *>::const_iterator it = mPageLookup.find(idx);
    if (it != mPageLookup.end())
        return it->second;
    else
        return nullptr;
}

StoreMetadataManager::~StoreMetadataManager() {
    unk98.clear();
    for (std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.begin();
         it != unk58.end(); ++it) {
        delete it->second;
    }
    unk58.clear();
}

void StoreMetadataManager::Init() {
    SetName("store", ObjectDir::sMainDir);
    mLoadingState = 0;
    mContentSize = 0;
    mErrorMsg = 0;
    if (SystemConfig("store", "local_metadata")->Int(1)) {
        gDebugMakeAllSongsAvailable = true;
        gDebugDontRelyOnCommerceServer = true;
        mFlags |= 1;
    }
}

void StoreOfferState::UpdateFlags(
    StorePackedOfferBase *base, unsigned char c1, unsigned char c2
) {
    mFlags &= 0xC0;
    if (c1 & 2)
        mFlags |= 1;
    if (c1 & 1)
        mFlags |= 2;
    if (c1 & 8)
        mFlags |= 0x20;
    if (c1 & 0x10)
        mFlags |= 0x10;
    if (c2 & 2)
        mFlags |= 8;
}

void StoreMetadataManager::UpdateOfferOwnership() {
    for (int i = 0; i < mOfferTable->mNumOffers; i++) {
    }
}

void StoreMetadataManager::MarkDownloaded(unsigned long long key, unsigned short idx) {
    std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(key);
    StoreTitleContentState *state;
    if (it == unk58.end()) {
        state = (StoreTitleContentState *)operator new(0x1000);
        if (state) memset(state, 0, 0x1000);
        unk58[key] = state;
    } else {
        state = (*it).second;
    }
    unsigned char *p = (unsigned char *)state + (idx << 3);
    if (p) {
        *p |= 1;
        mFlags |= 0x40;
    }
}

StorePage *StoreMetadataManager::LoadPage(unsigned short idx) {
    mCurrentPage = mPageTable->GetPage(idx);
    return mCurrentPage;
}

void StoreMetadataManager::Load(const char *cc) {
    if (!(mFlags & 0xC)) {
        mBasePath = cc;
        mErrorMsg = 0;
        SetLoadingState(1);
        mFlags |= 4;
    }
}

StoreMarqueeTable::~StoreMarqueeTable() {
    if (mBuffer)
        _MemFree(mBuffer);
}

bool StoreMarqueeTable::Load(const char *cc) {
    char buf[256];
    sprintf(buf, "%smarquees", cc);
    char *dataStart;
    char *dataEnd;
    bool ret = StoreLoadPackedFile(
        buf, true, 0x20000, false, false, &mBuffer, &dataStart, &dataEnd, &mNumMarquees
    );
    if (!ret)
        return ret;
    unsigned long byteCount = mNumMarquees * 10;
    int diff = dataEnd - dataStart;
    if ((unsigned long)diff != byteCount) {
        MILO_LOG(
            "Marquee file says is has %d entries, but at %d bytes each that would take %d bytes, and there are %d bytes left in the file.\n",
            mNumMarquees,
            10UL,
            byteCount,
            diff
        );
        mNumMarquees = (unsigned int)diff / 10;
    }
    mMarquees = dataStart;
    return true;
}

StorePageTable::~StorePageTable() {
    mPageLookup.clear();
    delete[] mPages;
    if (mBuffer)
        _MemFree(mBuffer);
}

void StoreMetadataManager::Unload() {
    if (StoreIndexFileReader::mMetaDataCntHandleInited) {
        CNTReleaseHandle(&StoreIndexFileReader::mMetaDataCntHandle);
        StoreIndexFileReader::mMetaDataCntHandleInited = false;
    }
    CM_CNTSDCacheClearRSO();
    if (mFlags & 2) {
        TheWiiCommerceMgr.DestroyCommerce();
        mFlags &= ~2;
    }
    mCurrentPage = NULL;
    delete mVersion;
    mVersion = NULL;
    delete mStringTable;
    mStringTable = NULL;
    delete mSongTable;
    mSongTable = NULL;
    delete mOfferTable;
    mOfferTable = NULL;
    delete mRbnOfferTable;
    mRbnOfferTable = NULL;
    delete mPageTable;
    mPageTable = NULL;
    delete mMarqueeTable;
    mMarqueeTable = NULL;
    delete mRedemptionsTable;
    mRedemptionsTable = NULL;
    delete (StoreVersionHeader *)unk7c;
    unk7c = 0;
    if (unk80 != 0) {
        _MemFree((void *)unk80);
        unk80 = 0;
    }
    for (std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.begin();
         it != unk58.end(); ++it) {
        delete it->second;
    }
    unk58.clear();
    unk70 = 0;
    unk74 = 0;
    SetLoadingState(0);
    mFlags &= ~0x1C;
    unk8c = 0;
    unk88 = 0;
    unk90 = 0;
    unk94 = 0;
    unk98.clear();
    unka0 = 0;
}

StoreOfferState *StoreMetadataManager::GetOfferStatus(const StorePackedOfferBase *base) {
    int idx = mOfferTable->OfferIndex(base);
    if (idx >= 0) {
        return &mOfferTable->mBufferNewRelease[idx];
    } else {
        idx = mRbnOfferTable->OfferIndex(base);
        if (idx >= 0)
            return &mRbnOfferTable->mBufferNewRelease[idx];
    }
    return 0;
}

BEGIN_HANDLERS(StoreMetadataManager)
    HANDLE_MESSAGE(CommerceMgrOpCompleteMsg)
    HANDLE_EXPR(exit_error, mErrorMsg)
    HANDLE_EXPR(check_content_size, mContentSize)
    HANDLE_EXPR(debug_purchase, (DebugPurchase(), 0))
    HANDLE_EXPR(debug_download, (DebugDownload(), 0))
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0xC10)
END_HANDLERS

void StorePackedRanks::EndianFix() {
    unsigned char buf[12]; memcpy(buf, this, 12); int rank;
    if (buf[0] >> 6) MILO_WARN("%s(%d) : Warning: %s", __FILE__, __LINE__, "!padding");
    rank = ((buf[0] & 0x3F) << 4) | (buf[1] >> 4);
    mGuitar = rank;
    rank = ((buf[1] & 0x0F) << 6) | (buf[2] >> 2);
    mVocals = rank;
    rank = ((buf[2] & 0x03) << 8) | buf[3];
    mBand = rank;
    if (buf[4] >> 6) MILO_WARN("%s(%d) : Warning: %s", __FILE__, __LINE__, "!padding");
    rank = ((buf[4] & 0x3F) << 4) | (buf[5] >> 4);
    mKeys = rank;
    rank = ((buf[5] & 0x0F) << 6) | (buf[6] >> 2);
    mDrums = rank;
    rank = ((buf[6] & 0x03) << 8) | buf[7];
    mBass = rank;
    if (buf[8] >> 6) MILO_WARN("%s(%d) : Warning: %s", __FILE__, __LINE__, "!padding");
    rank = ((buf[8] & 0x3F) << 4) | (buf[9] >> 4);
    mRealKeys = rank;
    rank = ((buf[9] & 0x0F) << 6) | (buf[10] >> 2);
    mRealBass = rank;
    rank = ((buf[10] & 0x03) << 8) | buf[11];
    mRealGuitar = rank;
}

void StorePackedPage::EndianFix() {
    unsigned char *b = (unsigned char *)this;
    unsigned char b6 = b[6];
    mHasOffers = b6 >> 4;
    mDefaultSort = b6;
    unsigned char b7 = b[7];
    unk6p0 = b7 >> 4;
    unk6p1 = b7;
}

unsigned long long StorePackedSong::DataTitle() const {
    return WiiCommerceMgr::MakeDataTitleId(&unk6);
}

bool StoreVersionHeader::LoadFile(const char *cc) {
    char *buffer;
    char *dataStart;
    char *dataEnd;
    bool ret = StoreLoadPackedFile(
        cc, false, 0x20, false, true, &buffer, &dataStart, &dataEnd, NULL
    );
    if (!ret)
        return false;
    unsigned char *data = (unsigned char *)buffer;
    mVersion = data[0];
    unsigned int hi = data[1] << 8;
    mBuildNumber = hi | data[2];
    mCompressed = data[3];
    gStoreUseCompressedFiles = data[3];
    return true;
}

StoreError StoreMetadataManager::LoadError() const {
    return (StoreError)mErrorMsg;
}

bool StoreMetadataManager::LoadingFailed() const {
    return mLoadingState == 11;
}

int StoreMetadataManager::GetContentStateFlags(unsigned long long key, unsigned short idx) {
    std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(key);
    StoreTitleContentState *state;
    if (it == unk58.end()) {
        state = (StoreTitleContentState *)operator new(0x1000);
        if (state) memset(state, 0, 0x1000);
        unk58[key] = state;
    } else {
        state = (*it).second;
    }
    return ((unsigned char *)state)[idx << 3];
}

int StoreMetadataManager::GetContentFileSize(unsigned long long key, unsigned short idx) {
    std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(key);
    StoreTitleContentState *state;
    if (it == unk58.end()) {
        state = (StoreTitleContentState *)operator new(0x1000);
        if (state) memset(state, 0, 0x1000);
        unk58[key] = state;
    } else {
        state = (*it).second;
    }
    return *(int *)((char *)state + (idx << 3) + 4);
}

void StoreMetadataManager::MarkDeleted(unsigned long long key, unsigned short idx) {
    std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(key);
    StoreTitleContentState *state;
    if (it == unk58.end()) {
        state = (StoreTitleContentState *)operator new(0x1000);
        if (state) memset(state, 0, 0x1000);
        unk58[key] = state;
    } else {
        state = (*it).second;
    }
    unsigned char *p = (unsigned char *)state + (idx << 3);
    if (p) {
        *p &= ~1;
        mFlags |= 0x40;
    }
}

void StoreMetadataManager::AddOldMetadataIndex(unsigned long long key, unsigned short idx) {
    std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(key);
    StoreTitleContentState *state;
    if (it == unk58.end()) {
        state = (StoreTitleContentState *)operator new(0x1000);
        if (state) memset(state, 0, 0x1000);
        unk58[key] = state;
    } else {
        state = (*it).second;
    }
    if (((unsigned char *)state)[idx << 3] & 1) {
        unk98.push_back(std::pair<unsigned long long, unsigned short>(key, idx));
    }
}

void StoreMetadataManager::AddSetlistOffer(int offer) {
    mSetlistOffers.push_back(offer);
}

void StoreMetadataManager::ClearSetlistOffers() {
    mSetlistOffers.clear();
}

void StoreMetadataManager::SetMetadataIndex(unsigned long long key, unsigned short idx, long l) {
    *(unsigned long long *)&unk88 = key;
    unk90 = idx;
    unk94 = l;
}

const StorePackedOfferBase *StoreMetadataManager::GetOffer(unsigned short key) const {
    if (key & 0x8000) {
        int idx = key & 0x7FFF;
        StoreRbnOfferTable *table = TheStoreMetadata.mRbnOfferTable;
        if (idx < table->mNumOffers)
            return table->mOffers[(unsigned short)idx];
    } else {
        StoreOfferTable *table = TheStoreMetadata.mOfferTable;
        if ((int)key < table->mNumOffers)
            return table->mOffers[key];
    }
    return NULL;
}

bool StoreMetadataManager::FindOffer(
    const char *id, int *outIndex, bool *outIsRbn, bool *outIsUpgrade
) const {
    StoreOfferTable *offerTable = mOfferTable;
    for (int i = 0; i < offerTable->mNumOffers; i++) {
        StorePackedOffer *offer = offerTable->mOffers[i];
        bool idMatch = strncmp(offer->mId, id, 16) == 0;
        if (idMatch) {
            *outIndex = i;
            *outIsRbn = false;
            *outIsUpgrade = false;
            return true;
        }
        bool upgradeMatch = strncmp(offer->mUpgradeId, id, 16) == 0;
        if (upgradeMatch) {
            *outIndex = i;
            *outIsRbn = false;
            *outIsUpgrade = true;
            return true;
        }
    }
    StoreRbnOfferTable *rbnTable = mRbnOfferTable;
    for (int i = 0; i < rbnTable->mNumOffers; i++) {
        StorePackedRBNOffer *offer = rbnTable->mOffers[i];
        bool idMatch = strncmp(offer->mId, id, 16) == 0;
        if (idMatch) {
            *outIndex = i;
            *outIsRbn = true;
            *outIsUpgrade = false;
            return true;
        }
        bool upgradeMatch = strncmp(offer->mUpgradeId, id, 16) == 0;
        if (upgradeMatch) {
            *outIndex = i;
            *outIsRbn = true;
            *outIsUpgrade = true;
            return true;
        }
    }
    return false;
}

bool StoreMetadataManager::Poll() {
    if (TheWiiCommerceMgr.mCommerceAsyncOpId == -1) {
        if (mFlags & 4)
            PollLoading();
        return mFlags & 8;
    }
    return false;
}

const StorePackedOfferBase *StoreMetadataManager::FindOfferFromSongId(int songId) const {
    StorePackedSong *found = NULL;
    for (int i = 0; i < mSongTable->mNumSongs; i++) {
        if ((unsigned int)songId == mSongTable->mSongs[i].mSongID) {
            found = &mSongTable->mSongs[i];
            break;
        }
    }
    if (found)
        return GetOffer(found->mOfferIndex);
    return NULL;
}

const StorePackedOfferBase *StorePage::BaseOffer(int idx) const {
    if (mPage->mHasOffers) {
        int key = mOffers[idx];
        if (key & 0x8000) {
            return TheStoreMetadata.mRbnOfferTable->mOffers[key & 0x7FFF];
        } else {
            return TheStoreMetadata.mOfferTable->mOffers[key];
        }
    }
    return NULL;
}

StorePackedRBNOffer *StorePage::RbnOffer(int idx) const {
    if (mPage->mHasOffers) {
        unsigned short key = mOffers[idx];
        if (key & 0x8000)
            return TheStoreMetadata.mRbnOfferTable->mOffers[key & 0x7FFF];
    }
    return NULL;
}

StorePackedSubMenu *StorePage::Submenu(int idx) const {
    if (mPage->mHasOffers)
        return NULL;
    return &mSubmenus[idx];
}
