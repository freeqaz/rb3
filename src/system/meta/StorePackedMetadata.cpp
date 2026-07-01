#include "meta/StorePackedMetadata.h"
#include "meta/StoreOffer.h"
#include "os/CommerceMgr_Wii.h"
#include "os/ContentMgr_Wii.h"
#include "os/File.h"
#include <set>
#include "obj/ObjMacros.h"
#include "utl/Compress.h"
#include "utl/MemMgr.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "sdk/RVL_SDK/revolution/cnt/cnt.h"
#include "sdk/RVL_SDK/revolution/nand/nand.h"
#include "os/Timer.h"
#include "sdk/ec/csup.h"

extern "C" int NANDGetStatus(const char *, NANDStatus *);

// LoadMgr / TheLoadMgr defined transitively via the includes above.
const char *Localize(Symbol, bool *);
class StorePanel : public Hmx::Object {
public:
    static StorePanel *Instance();
    DataNode HandleType(DataArray *);
};

void CM_CNTSDCacheClearRSO();
int CM_CNTSDCachePushDeleteContentRSO(unsigned long, unsigned long long, unsigned short, ...);
int DebugSdBackup(unsigned long long, unsigned short);

extern "C" {
    int EC_DownloadTitle(unsigned long long titleId, int param);
    int EC_DownloadContents(unsigned long long titleId, unsigned short *contents, int numContents);
}

extern "C" int EC_GetProgress(long opId, ECProgress *progress);
int DebugWaitAsyncOp(long opId);

void UpdateOfferStateFromEc(
    StoreOfferState *, const char *, int,
    const ECContentCatalogInfo *, const StorePackedOfferBase *
);

StoreMetadataManager TheStoreMetadata;
std::vector<int> StoreMetadataManager::mSetlistOffers;
bool gDebugMakeAllSongsAvailable;
bool gDebugDontRelyOnCommerceServer;
int gStoreUseCompressedFiles;
const char *gStoreMetadataManagerLoadStepName[12] = {
    "None",
    "Init",
    "WaitForConnect",
    "FastEnumerate",
    "FindIndex",
    "ComputeRequiredSpace",
    "UpdateTitle",
    "DeleteOldIndex",
    "PurchaseIndex",
    "DownloadIndex",
    "LoadIndex",
    "Error",
};

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

inline void StoreIndexFileReader::Init(unsigned long long titleId, unsigned short idx) {
    int rc = (int)contentInitHandleTitleNAND(
        titleId, idx,
        (void *)&StoreIndexFileReader::mMetaDataCntHandle, &gCNTAllocator
    );
    if (rc != 0) {
        TheDebug << MakeString(
            "Store: error in contentInitHandleTitleNAND: %d, redownloading\n",
            (long)rc
        );
        std::map<unsigned long long, StoreTitleContentState *>::iterator it =
            TheStoreMetadata.unk58.find(titleId);
        StoreTitleContentState *state;
        if (it == TheStoreMetadata.unk58.end()) {
            state = (StoreTitleContentState *)operator new(0x1000);
            if (state) memset(state, 0, 0x1000);
            TheStoreMetadata.unk58[titleId] = state;
        } else {
            state = (*it).second;
        }
        ((unsigned char *)state)[(idx << 3) & 0x7FFF8] &= ~1;
        TheStoreMetadata.SetLoadingState(9);
    } else {
        StoreIndexFileReader::mMetaDataCntHandleInited = true;
        CNTDir dirBuf;
        CNTOpenDir(
            &StoreIndexFileReader::mMetaDataCntHandle, "/", &dirBuf
        );
        CNTDirEntry nameBuf;
        while (CNTReadDir(&dirBuf, &nameBuf) != 0) {
            char *namep = (char *)&nameBuf;
            char *vp = strstr(namep, "version");
            if (vp != NULL) {
                *vp = 0;
                TheStoreMetadata.mBasePath = namep;
                *vp = 'v';
            }
        }
        CNTCloseDir(&dirBuf);
    }
}
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
        TheDebug.Notify(MakeString("Store: file %s is missing\n", filename));
        return false;
    }
    int fileSize = file->Size();
    int alignedSize = (fileSize + 0x1F) & ~0x1F;
    if (fileSize > maxSize) {
        TheDebug.Notify(MakeString("Store: file %s is over budget (%d > %d)\n", filename, fileSize, maxSize));
        delete file;
        return false;
    }
    if (compressed && gStoreUseCompressedFiles) {
        char *rawBuf = (char *)_MemAllocTemp(alignedSize, 0x20);
        if (rawBuf == NULL) {
            TheDebug.Notify(MakeString("Store: Failed to allocated %d byte buffer for store file %s.\n", fileSize, filename));
            delete file;
            return false;
        }
        char *decompBuf = (char *)_MemAllocTemp(maxSize, 0x20);
        if (decompBuf == NULL) {
            TheDebug.Notify(MakeString("Store: Failed to allocated %d byte buffer for decompressing store file %s.\n", maxSize, filename));
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
            TheDebug.Notify(MakeString("Store: Failed to allocated %d byte buffer for store file %s.\n", alignedSize, filename));
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
        bool valid = false;
        if ((i & 0x7FFF) < mLocalized.mNumStrings)
            valid = true;
        return valid;
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

struct OfferLoadCtx {
    StorePackedOffer **n;
    StorePackedOffer **b[4];
};

bool StoreOfferTable::Load(const char *cc) {
    char buf[256];
    sprintf(buf, "%soffers", cc);
    OfferLoadCtx ctx;
    StorePackedOffer **loc130;
    bool ret = StoreLoadPackedFile(
        buf,
        true,
        0x40000,
        true,
        true,
        &mBuffer,
        (char **)&ctx.n,
        (char **)&loc130,
        &mNumOffers
    );
    if (!ret)
        return ret;
    else {
        int diff = (int)loc130 - (int)ctx.n;
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
        mOffers = ctx.n;
        void *buf = new StoreOfferState[mNumOffers];
        mBufferNewRelease = (StoreOfferState *)buf;
        memset(buf, 0, mNumOffers * sizeof(StoreOfferState));

        ctx.n += mNumOffers;
        ctx.b[0] = 0;
        ctx.b[1] = 0;
        ctx.b[2] = 0;
        for (int i = 0; i < mNumOffers; i++) {
            StorePackedOffer *curOffer = mOffers[i];
            curOffer->EndianFix();
            (((char **)&ctx.b[0])[curOffer->OfferType()])++;
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
    char **offsets = (char **)dataStart;
    mNumPages = 0;
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
        StorePackedOffer *offer = mOfferTable->mOffers[i];
        StoreOfferState *state = &mOfferTable->mBufferNewRelease[i];
        int andFlags = 0xFF;
        int orFlags = 0;
        bool allDownloaded = false;
        for (int j = 0; j < offer->mNumSongs; j++) {
            StorePackedSong *song = &mSongTable->mSongs[offer->mSongs[j]];
            int sflags = SongStateFlags(song);
            andFlags &= sflags;
            orFlags |= sflags;
            if (!allDownloaded) {
                unsigned long long dataTitle = WiiCommerceMgr::MakeDataTitleId(&song->unk6);
                std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(dataTitle);
                StoreTitleContentState *titleState;
                if (it == unk58.end()) {
                    titleState = (StoreTitleContentState *)operator new(0x1000);
                    if (titleState) memset(titleState, 0, 0x1000);
                    unk58[dataTitle] = titleState;
                } else {
                    titleState = (*it).second;
                }
                unsigned short rawA = *(unsigned short *)((char *)song + 0xa);
                unsigned short unka = song->unka;
                allDownloaded = true;
                if (!(((unsigned char *)titleState)[(rawA >> 4) & 0xFF8] & 1)
                    && !(((unsigned char *)titleState)[(unka << 3) + 8] & 1)) {
                    allDownloaded = false;
                }
            }
        }
        state->UpdateFlags(offer, andFlags, orFlags);
        if (allDownloaded) state->mFlags |= 4;
        if (gDebugMakeAllSongsAvailable) state->mFlags |= 0x40;
        if (gDebugDontRelyOnCommerceServer) state->mPrice = 0x141;
    }
    for (int i = 0; i < mRbnOfferTable->mNumOffers; i++) {
        StorePackedRBNOffer *offer = mRbnOfferTable->mOffers[i];
        StoreOfferState *state = &mRbnOfferTable->mBufferNewRelease[i];
        int andFlags = 0xFF;
        int orFlags = 0;
        bool allDownloaded = false;
        for (int j = 0; j < offer->mNumSongs; j++) {
            StorePackedSong *song = &mSongTable->mSongs[offer->mSongs[j]];
            int sflags = SongStateFlags(song);
            andFlags &= sflags;
            orFlags |= sflags;
            if (!allDownloaded) {
                unsigned long long dataTitle = WiiCommerceMgr::MakeDataTitleId(&song->unk6);
                std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(dataTitle);
                StoreTitleContentState *titleState;
                if (it == unk58.end()) {
                    titleState = (StoreTitleContentState *)operator new(0x1000);
                    if (titleState) memset(titleState, 0, 0x1000);
                    unk58[dataTitle] = titleState;
                } else {
                    titleState = (*it).second;
                }
                unsigned short rawA = *(unsigned short *)((char *)song + 0xa);
                unsigned short unka = song->unka;
                allDownloaded = true;
                if (!(((unsigned char *)titleState)[(rawA >> 4) & 0xFF8] & 1)
                    && !(((unsigned char *)titleState)[(unka << 3) + 8] & 1)) {
                    allDownloaded = false;
                }
            }
        }
        state->UpdateFlags(offer, andFlags, orFlags);
        if (allDownloaded) state->mFlags |= 4;
        if (gDebugMakeAllSongsAvailable) state->mFlags |= 0x40;
        if (gDebugDontRelyOnCommerceServer) state->mPrice = 0x7B;
    }
    mFlags &= ~0x40;
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
    if (mPages) {
        delete[] mPages;
    }
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
    unsigned char b7 = b[7];
    mHasOffers = b6 >> 4;
    unk6p0 = b7 >> 4;
    mDefaultSort = b6;
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
    unsigned int build = data[2];
    build |= (unsigned int)data[1] << 8;
    mBuildNumber = build;
    mCompressed = data[3];
    gStoreUseCompressedFiles = mCompressed;
    return true;
}

StoreError StoreMetadataManager::LoadError() const {
    return (StoreError)mErrorMsg;
}

bool StoreMetadataManager::LoadingFailed() const {
    return mLoadingState == 11;
}

int StoreMetadataManager::GetContentStateFlags(unsigned long long key, unsigned short idx) {
    StoreTitleContentState *state;
    std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(key);
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
    StoreTitleContentState *state;
    std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(key);
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
    return true;
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

// Out-of-line WiiCommerceMgr helpers used below that are not yet declared in
// os/CommerceMgr_Wii.h. Forward-declared here as plain C symbols using the
// MetroWerks-mangled names so the call sites still link against the existing
// object code.
extern "C" {
    int RequestStoreIndex__14WiiCommerceMgrFPQ23Hmx6Object(WiiCommerceMgr *, Hmx::Object *);
    int RequestSpecifiedResourceRequirements__14WiiCommerceMgrFQ214WiiCommerceMgr19RequestResourceTypeUxPQ23Hmx6Object(
        WiiCommerceMgr *, int, unsigned long long, Hmx::Object *
    );
    int UpdateTitle__14WiiCommerceMgrFUx(WiiCommerceMgr *, unsigned long long);
    int DownloadIndexContentUnit__14WiiCommerceMgrFUxiPQ23Hmx6Object(
        WiiCommerceMgr *, unsigned long long, int, Hmx::Object *
    );
    void WaitAsyncOp__14WiiCommerceMgrFlQ214WiiCommerceMgr21LastCommerceOperation(
        WiiCommerceMgr *, long, int
    );
    int EC_PurchaseDataTitle(unsigned long long, long, int);
    int EC_SetParameter_pcpw(const char *, const char *);
    int EC_GetCachedBalance(int *);
    int CheckRequestedDownloadSize__14WiiCommerceMgrFv(WiiCommerceMgr *);
    extern char gUsersPIN[];
}

// C++ mangled - use namespace-less names so the compiler mangles them naturally.
void CM_CNTSDGetUserAvailableAreaRSO(unsigned long *, unsigned long *, unsigned long *, unsigned long *);
int CM_CNTSDCachePopRSO(long);
void CM_CNTSDDeleteBackupRSO(unsigned long long, unsigned short);

void StoreMetadataManager::SetLoadingState(int state) {
    if (mLoadingState == state) return;
    mLoadingState = state;
    if ((unsigned int)state > 11) return;
    switch (state) {
    case 0:
        mFlags &= ~4;
        return;
    case 11:
        mFlags &= ~0x1C;
        return;
    case 1: {
        if (TheWiiCommerceMgr.InitCommerce(NULL) == 0) {
            SetLoadingState(11);
        } else {
            mFlags |= 2;
            SetLoadingState(2);
        }
        return;
    }
    case 2:
    case 3:
        return;
    case 4: {
        if (RequestStoreIndex__14WiiCommerceMgrFPQ23Hmx6Object(&TheWiiCommerceMgr, NULL) == 0) {
            mErrorMsg = 2;
            SetLoadingState(11);
        } else {
            SetLoadingState(5);
        }
        return;
    }
    case 5: {
        std::vector<unsigned short VECTOR_SIZE_SMALL> units;
        units.push_back(unk90);
        TheWiiCommerceMgr.SpecifyContentUnits(units);
        RequestSpecifiedResourceRequirements__14WiiCommerceMgrFQ214WiiCommerceMgr19RequestResourceTypeUxPQ23Hmx6Object(
            &TheWiiCommerceMgr, 1, *(unsigned long long *)&unk88, NULL
        );
        return;
    }
    case 7:
        SetLoadingState(5);
        return;
    case 8: {
        unsigned short idx = unk90;
        unsigned long long titleId = *(unsigned long long *)&unk88;
        StoreTitleContentState *titleState;
        std::map<unsigned long long, StoreTitleContentState *>::iterator it =
            unk58.find(titleId);
        if (it == unk58.end()) {
            titleState = (StoreTitleContentState *)operator new(0x1000);
            if (titleState) memset(titleState, 0, 0x1000);
            unk58[titleId] = titleState;
        } else {
            titleState = it->second;
        }
        int st = ((unsigned char *)titleState)[idx << 3] & 2;
        if (st == 2) {
            SetLoadingState(6);
        } else {
            if (TheWiiCommerceMgr.unkF2) {
                EC_SetParameter("PCPW", gUsersPIN);
            }
            long opId = EC_PurchaseDataTitle(titleId, unk94, 0);
            if (opId < 0) {
                mErrorMsg = 2;
                SetLoadingState(11);
            } else {
                WaitAsyncOp__14WiiCommerceMgrFlQ214WiiCommerceMgr21LastCommerceOperation(
                    &TheWiiCommerceMgr, opId, 6
                );
            }
        }
        return;
    }
    case 6: {
        unsigned long long titleId = *(unsigned long long *)&unk88;
        unsigned short idx = unk90;
        if (UpdateTitle__14WiiCommerceMgrFUx(&TheWiiCommerceMgr, titleId) == 0) {
            mErrorMsg = 2;
            SetLoadingState(11);
        }
        return;
    }
    case 9: {
        unsigned short idx = unk90;
        unsigned long long titleId = *(unsigned long long *)&unk88;
        StoreTitleContentState *titleState;
        std::map<unsigned long long, StoreTitleContentState *>::iterator it =
            unk58.find(titleId);
        if (it == unk58.end()) {
            titleState = (StoreTitleContentState *)operator new(0x1000);
            if (titleState) memset(titleState, 0, 0x1000);
            unk58[titleId] = titleState;
        } else {
            titleState = it->second;
        }
        if ((((unsigned char *)titleState)[idx << 3] & 1) == 1) {
            SetLoadingState(10);
        } else {
            if (DownloadIndexContentUnit__14WiiCommerceMgrFUxiPQ23Hmx6Object(
                    &TheWiiCommerceMgr, titleId, idx, NULL
                ) == 0) {
                mErrorMsg = 2;
                SetLoadingState(11);
            }
        }
        return;
    }
    case 10:
        return;
    }
}

void StoreContentStateCache::PollUpdate() {
    if (mIndexInConfig == -1) return;
    if (mTitleIdx == 0) {
        mTitleIdx = 1;
        DataArray *cfg = SystemConfig("store", "title_starting_indices");
        mIndexInConfig = cfg->Node(1).Int(cfg);
    }
    if ((unsigned int)mIndexInConfig < TheWiiCommerceMgr.mTitleIdsNum) {
        TheWiiCommerceMgr.QueryOffers(
            TheWiiCommerceMgr.mTitleIds[mIndexInConfig], NULL, &TheStoreMetadata
        );
    } else {
        mIndexInConfig = -1;
    }
}

void StoreContentStateCache::UpdateContentStateFromFastEnum() {
    if (0 != TheWiiCommerceMgr.unk2110) {
        StoreTitleContentState *state;
        unsigned long long titleId = TheWiiCommerceMgr.mTitleIds[mIndexInConfig];
        std::map<unsigned long long, StoreTitleContentState *>::iterator it = find(titleId);
        if (it == end()) {
            state = (StoreTitleContentState *)operator new(0x1000);
            if (state) memset(state, 0, 0x1000);
            (*this)[titleId] = state;
        } else {
            state = (*it).second;
        }
        char *catalog = (char *)&TheWiiCommerceMgr + 0x110;
        for (unsigned long i = 0; i < TheWiiCommerceMgr.unk2110; i++) {
            unsigned short slot = *(unsigned short *)(catalog + 4);
            unsigned char *entry = (unsigned char *)state + (slot << 3);
            *entry = 0;
            if (*(unsigned int *)catalog & 1) {
                *entry |= 1;
            } else if (TheWiiContentMgr.Contains(titleId, slot)) {
                *entry |= 1;
            }
            if (*(unsigned int *)catalog & 2) *entry |= 2;
            *entry |= 4;
            *(int *)(entry + 4) = *(int *)(catalog + 0xC);
            catalog += 0x10;
        }
    }
    mIndexInConfig += 1;
}

void StoreMetadataManager::MarkPurchased(ECContentCatalogInfo *info) {
    unsigned long long key = info->titleId;
    std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(key);
    StoreTitleContentState *state;
    if (it == unk58.end()) {
        state = (StoreTitleContentState *)operator new(0x1000);
        if (state) memset(state, 0, 0x1000);
        unk58[key] = state;
    } else {
        state = (*it).second;
    }
    if (state) {
        for (unsigned long i = 0; i < info->nIndexes; i++) {
            unsigned short idx = info->indexes[i];
            unsigned char *p = (unsigned char *)state + (idx << 3);
            *p |= 2;
        }
        mFlags |= 0x40;
    }
}

int StoreMetadataManager::SongStateFlags(const StorePackedSong *song) {
    unsigned long long dataTitle =
        WiiCommerceMgr::MakeDataTitleId(&song->unk6);
    StoreTitleContentState *state2;
    StoreTitleContentState *state;
    std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(dataTitle);
    if (it == unk58.end()) {
        state = (StoreTitleContentState *)operator new(0x1000);
        if (state) memset(state, 0, 0x1000);
        unk58[dataTitle] = state;
    } else {
        state = (*it).second;
    }
    unsigned short rawA = *(unsigned short *)((char *)song + 0xa);
    unsigned short unka = (rawA >> 7) & 0x1FF;
    unsigned char flags =
        ((unsigned char *)state)[(rawA >> 4) & 0xFF8]
        & ((unsigned char *)state)[(unka + 1) * 8];
    if (song->unkc != 0) {
        unsigned short upgradeIdx = song->unk10;
        unsigned long long upgradeTitle =
            WiiCommerceMgr::MakeDataTitleId(&song->unkc);
        std::map<unsigned long long, StoreTitleContentState *>::iterator it2 = unk58.find(upgradeTitle);
        if (it2 == unk58.end()) {
            state2 = (StoreTitleContentState *)operator new(0x1000);
            if (state2) memset(state2, 0, 0x1000);
            unk58[upgradeTitle] = state2;
        } else {
            state2 = (*it2).second;
        }
        unsigned char ub = ((unsigned char *)state2)[(upgradeIdx << 3) & 0x7FFF8];
        if (ub & 1) flags |= 8;
        if (ub & 2) flags |= 0x10;
    }
    return flags;
}

void StoreMetadataManager::UpdateOfferPrices() {
    for (unsigned long i = 0; i < TheWiiCommerceMgr.mNumCatalogInfos; i++) {
        ECContentCatalogInfo *info = &TheWiiCommerceMgr.mCatalogInfos[i];
        if (info->licensePricings == NULL) {
            TheDebug.Notify(MakeString(
                "Store: ECContentCatalogInfo %d has null licensePricings",
                (int)i
            ));
            continue;
        }
        int newPrice = (int)atoi((const char *)info->licensePricings + 4);
        const char *offerId = GetAttributeStr(info, "offer_id");
        if (offerId == NULL) continue;
        int idx;
        bool isRbn;
        bool isUpgrade;
        if (!FindOffer(offerId, &idx, &isRbn, &isUpgrade)) continue;
        if (isRbn) {
            if (isUpgrade) {
                StorePackedRBNOffer *off = mRbnOfferTable->mOffers[idx];
                StorePackedSong *firstSong;
                if (off->mIsRBN) {
                    firstSong = &TheStoreMetadata.mSongTable->mSongs[off->mSongs[0]];
                } else {
                    firstSong = &TheStoreMetadata.mSongTable->mSongs[
                        ((StorePackedOffer *)off)->mSongs[0]
                    ];
                }
                unsigned long long expected =
                    WiiCommerceMgr::MakeDataTitleId(&firstSong->unkc);
                unsigned short expectedIdx = firstSong->unk10;
                if (info->titleId == expected && info->nIndexes == 1
                    && *info->indexes == expectedIdx) {
                    mRbnOfferTable->mBufferNewRelease[idx].unk2 =
                        (unsigned short)newPrice;
                } else {
                    TheDebug.Notify(MakeString(
                        "Store: upgrade offer %s says upgrade is at %llx %d, but store index data says %llx %d",
                        offerId, info->titleId, *info->indexes,
                        expected, expectedIdx
                    ));
                }
            } else {
                UpdateOfferStateFromEc(
                    &mRbnOfferTable->mBufferNewRelease[idx], offerId, newPrice,
                    info, mRbnOfferTable->mOffers[idx]
                );
            }
        } else if (isUpgrade) {
            StorePackedOffer *off = mOfferTable->mOffers[idx];
            StorePackedSong *firstSong;
            if (off->mIsRBN) {
                firstSong = &TheStoreMetadata.mSongTable->mSongs[
                    ((StorePackedRBNOffer *)off)->mSongs[0]
                ];
            } else {
                firstSong = &TheStoreMetadata.mSongTable->mSongs[off->mSongs[0]];
            }
            unsigned long long expected =
                WiiCommerceMgr::MakeDataTitleId(&firstSong->unkc);
            unsigned short expectedIdx = firstSong->unk10;
            if (info->titleId == expected && info->nIndexes == 1
                && *info->indexes == expectedIdx) {
                mOfferTable->mBufferNewRelease[idx].unk2 =
                    (unsigned short)newPrice;
            } else {
                TheDebug.Notify(MakeString(
                    "Store: upgrade offer %s says upgrade is at %llx %d, but store index data says %llx %d",
                    offerId, info->titleId, *info->indexes,
                    expected, expectedIdx
                ));
            }
        } else {
            UpdateOfferStateFromEc(
                &mOfferTable->mBufferNewRelease[idx], offerId, newPrice,
                info, mOfferTable->mOffers[idx]
            );
        }
    }
    if (*(unsigned int *)((char *)&TheWiiCommerceMgr + 0x211C)
        != *(unsigned int *)((char *)&TheWiiCommerceMgr + 0x2118)) {
        mFlags &= ~0x10;
    } else {
        mFlags &= ~0x10;
        if (TheWiiCommerceMgr.RequestOffers(this) != 0) {
            mFlags |= 0x10;
        }
    }
}

void StoreMetadataManager::UpdateAvailability() {
    if (!(mFlags & 8)) return;
    for (unsigned long i = 0; i < TheWiiCommerceMgr.mTitleIdsNum; i++) {
        unsigned long long titleId = TheWiiCommerceMgr.mTitleIds[i];
        StoreTitleContentState *state;
        std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(titleId);
        if (it == unk58.end()) {
            state = (StoreTitleContentState *)operator new(0x1000);
            if (state) memset(state, 0, 0x1000);
            unk58[titleId] = state;
        } else {
            state = (*it).second;
        }
        unsigned char *p = (unsigned char *)state;
        for (int j = 0; j < 0x200; j++) {
            if (TheWiiContentMgr.Contains(titleId, j)) {
                *p |= 1;
            } else {
                *p &= ~1;
            }
            p += 8;
        }
    }
    UpdateOfferOwnership();
}

int DebugWaitAsyncOp(long opId) {
    ECProgress progress;
    int result = -0xfa9;
    while (result == -0xfa9) {
        Timer::Sleep(10);
        result = EC_GetProgress(opId, &progress);
    }
    TheDebug << MakeString(
        "async op %d done: %d err: %d",
        progress.operation,
        (long)progress.status,
        progress.errCode
    );
    int ret = 0;
    if (result == 0 && progress.errCode == 0)
        ret = 1;
    return ret;
}

void StoreMetadataManager::DebugPurchase() {
    int balance = 0;
    EC_GetCachedBalance(&balance);
    for (int i = 0; i < mOfferTable->mNumOffers; i++) {
        StoreOfferState *state = &mOfferTable->mBufferNewRelease[i];
        if (state->mFlags & 1) continue;
        StorePackedOffer *offer = mOfferTable->mOffers[i];
        String offerId = offer->GetOfferId();
        for (unsigned long j = 0; j < TheWiiCommerceMgr.mNumCatalogInfos; j++) {
            ECContentCatalogInfo *info = &TheWiiCommerceMgr.mCatalogInfos[j];
            if (info->licensePricings == NULL) continue;
            const char *offerIdAttr = GetAttributeStr(info, "offer_id");
            int price = (int)strtol((const char *)info->licensePricings + 4, 0, 10);
            if (price > balance) {
                FormatString fmt("DebugPurchase: out of money\n");
                TheDebug << fmt.Str();
                return;
            }
            if (offerId == offerIdAttr) {
                unsigned short nameKey = offer->mNameIndex;
                StoreStringTable *strTable = TheStoreMetadata.mStringTable;
                const char *name;
                if (nameKey & 0x8000) {
                    name = strTable->mLocalized
                               .GetString((nameKey & 0x7FFF) - 1);
                } else {
                    name = strTable->mNonLocalized
                               .GetString(nameKey - 1);
                }
                TheDebug << MakeString("DebugPurchase: %s\n", name);
                auto _tmp1 = EC_PurchaseDataTitle(info->titleId, *info->indexes, price);
                DebugWaitAsyncOp(
                    _tmp1
                );
                balance -= price;
                state->mFlags |= 6;
            }
        }
    }
}

StorePage *StoreMetadataManager::LoadDynamicPage(DataArray *arr) {
    if (unk7c == 0) {
        unk7c = (int)operator new(9);
        ((unsigned short *)unk7c)[0] = 0xFFFF;
        ((unsigned short *)unk7c)[1] = 0;
        ((unsigned short *)unk7c)[2] = 0;
        ((unsigned short *)unk7c)[3] &= 0x00FF;
        ((StorePackedPage *)unk7c)->mDefaultSort = 2;
        ((StorePackedPage *)unk7c)->mHasOffers = 1;
        ((StorePackedPage *)unk7c)->mNumOffers = 0;
    }
    if (arr == NULL) {
        return (StorePage *)&unk78;
    }
    unk78 = -1;
    int count = 0;
    std::list<const char *> names;
    short numNodes = arr->Size();
    for (int i = 0; i < numNodes; i++) {
        const DataNode &node = arr->Node(i);
        auto _tmp0 = node.Type();
        if (_tmp0 == kDataArray) {
            DataArray *sub = node.Array(NULL);
            DataArray *idArr = sub->FindArray(id, false);
            if (idArr != NULL && idArr->Size() > 1) {
                count++;
                const char *idStr = idArr->Node(1).Str(idArr);
                names.push_back(idStr);
            }
        }
    }
    if (unk80 != 0) {
        _MemFree((void *)unk80);
        unk80 = 0;
    }
    unk80 = (int)_MemAlloc(count * 2, 4);
    char realCount = 0;
    int byteOff = 0;
    for (std::list<const char *>::iterator it = names.begin(); it != names.end(); ++it) {
        int idx;
        bool isRbn;
        bool isUpgrade;
        if (FindOffer(*it, &idx, &isRbn, &isUpgrade)) {
            if (isRbn) idx |= 0x8000;
            realCount++;
            *(unsigned short *)((char *)unk80 + byteOff) = (unsigned short)idx;
            byteOff += 2;
        }
    }
    ((StorePackedPage *)unk7c)->mNumOffers = realCount;
    return (StorePage *)&unk78;
}

void StoreMetadataManager::PollLoading() {
    bool loaded;
    if (mFlags & 1) {
        SetLoadingState(0xA);
    }
    switch (mLoadingState) {
    case 2:
        if (TheWiiCommerceMgr.unkF4 != 0) {
            SetLoadingState(3);
        }
        return;
    case 3: {
        ((StoreContentStateCache *)&unk58)->PollUpdate();
        if (unk74 == -1) {
            SetLoadingState(4);
        }
        return;
    }
    case 10: {
        if (mVersion == NULL) {
            int ok;
            if (!(mFlags & 1)) {
                StoreIndexFileReader::Init(
                    *(unsigned long long *)&unk88, unk90
                );
                ok = StoreIndexFileReader::mMetaDataCntHandleInited;
            } else {
                ok = 1;
            }
            if (ok != 0) {
                mVersion = new StoreVersionHeader();
                loaded = mVersion->LoadFile(MakeString("%sversion", String(mBasePath)));
                goto load_check;
            }
            return;
        }
        if (mStringTable == NULL) {
            mStringTable = new StoreStringTable();
            loaded = mStringTable->Load(mBasePath.c_str());
            goto load_check;
        }
        if (mSongTable == NULL) {
            mSongTable = new StoreSongTable();
            loaded = mSongTable->Load(mBasePath.c_str());
            goto load_check;
        }
        if (mOfferTable == NULL) {
            mOfferTable = new StoreOfferTable();
            loaded = mOfferTable->Load(mBasePath.c_str());
            goto load_check;
        }
        if (mRbnOfferTable == NULL) {
            mRbnOfferTable = new StoreRbnOfferTable();
            loaded = mRbnOfferTable->Load(mBasePath.c_str());
            goto load_check;
        }
        if (mPageTable == NULL) {
            mPageTable = new StorePageTable();
            loaded = mPageTable->Load(mBasePath.c_str());
            mCurrentPage = LoadPage(mVersion->mBuildNumber);
            goto load_check;
        }
        if (mMarqueeTable == NULL) {
            mMarqueeTable = new StoreMarqueeTable();
            loaded = mMarqueeTable->Load(mBasePath.c_str());
            goto load_check;
        }
        if (mRedemptionsTable == NULL) {
            mRedemptionsTable = new StoreRedemptionsTable();
            loaded = mRedemptionsTable->Load(mBasePath.c_str());
            goto load_check;
        }
        }
        if (StoreIndexFileReader::mMetaDataCntHandleInited) {
            CNTReleaseHandle(&StoreIndexFileReader::mMetaDataCntHandle);
            StoreIndexFileReader::mMetaDataCntHandleInited = false;
        }
        if (TheWiiContentMgr.mMode == 0) {
            int rcpop = CM_CNTSDCachePopRSO(-1);
            if (rcpop != 0) {
                TheDebug.Fail(MakeString("Store: delete index by popping tmpcache failed: %d\n", (long)rcpop));
            }
        }
        int rcdel = EC_DeleteContents(*(unsigned long long *)&unk88, &unk90, 1);
        if (rcdel != 0 && TheWiiContentMgr.mMode != 0) {
            TheDebug.Fail(MakeString("Store: delete index by EC_DeleteContents failed: %d\n", (long)rcdel));
        }
        CM_CNTSDDeleteBackupRSO(*(unsigned long long *)&unk88, unk90);
        mFlags |= 8;
        SetLoadingState(0);
        UpdateAvailability();
        if (!gDebugDontRelyOnCommerceServer
            && TheWiiCommerceMgr.RequestOffers(this) != 0) {
            mFlags |= 0x10;
        }
        return;
    load_check:
        if (!loaded) {
            mErrorMsg = 6;
            SetLoadingState(0xB);
        }
        return;
    }
}

DataNode StoreMetadataManager::OnMsg(const CommerceMgrOpCompleteMsg &msg) {
    const DataArray *data = msg.Data();
    if (data->Node(2).Int(data) == 0) {
        if (mLoadingState == 3) {
            const DataArray *cfg = SystemConfig("store", "title_starting_indices");
            unk70 += 1;
            if (unk70 < cfg->Size()) {
                unk74 = cfg->Node(unk70).Int(cfg);
            } else {
                unk74 = -1;
            }
        } else {
            int errCode = data->Node(3).Int(data);
            switch (errCode) {
            case -4076: mErrorMsg = 0x65; break;
            case -4084:
            case -4086: mErrorMsg = 0x67; break;
            case -4082: mErrorMsg = 0x66; break;
            case -4055: mErrorMsg = 0x68; break;
            case -4056: mErrorMsg = 0x69; break;
            case -4043: mErrorMsg = 3; break;
            default: mErrorMsg = 0x64; break;
            }
            SetLoadingState(0xB);
            if (mFlags & 0x10) {
                ((Hmx::Object *)StorePanel::Instance())->HandleType(msg.Data());
            }
        }
    } else if (mFlags & 0x10) {
        UpdateOfferPrices();
    } else if (mLoadingState == 3) {
        ((StoreContentStateCache *)&unk58)->UpdateContentStateFromFastEnum();
    } else if (mLoadingState == 4) {
        unsigned long a, b, c, d;
        CM_CNTSDGetUserAvailableAreaRSO(&a, &b, &c, &d);
        TheDebug << MakeString(
            "CNTSDGetUserAvailableArea(%d, %d, %d, %d);\n", a, b, c, d
        );
        if (TheWiiContentMgr.mMode == 1) {
            SetLoadingState(5);
        } else {
            mContentSize = 0;
            SetLoadingState(8);
        }
    } else if (mLoadingState == 8) {
        unsigned short idx = unk90;
        unsigned long long titleId = *(unsigned long long *)&unk88;
        StoreTitleContentState *state;
        std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(titleId);
        if (it == unk58.end()) {
            state = (StoreTitleContentState *)operator new(0x1000);
            if (state) memset(state, 0, 0x1000);
            unk58[titleId] = state;
        } else {
            state = (*it).second;
        }
        unsigned char *p = (unsigned char *)state + ((idx << 3) & 0x7FFF8);
        *p |= 2;
        SetLoadingState(6);
    } else if (mLoadingState == 6) {
        SetLoadingState(9);
    } else if (mLoadingState == 5) {
        mContentSize = 0;
        int rc = CheckRequestedDownloadSize__14WiiCommerceMgrFv(&TheWiiCommerceMgr);
        if (rc != 0) {
            if (rc == 4) {
                mContentSize = *(int *)((char *)&TheWiiCommerceMgr + 0x4190);
                mErrorMsg = 0x69;
            } else if (rc == 5) {
                mErrorMsg = 0x68;
            } else {
                mErrorMsg = 0x64;
            }
            SetLoadingState(0xB);
        } else {
            SetLoadingState(8);
        }
    } else if (mLoadingState == 9) {
        unsigned short idx = unk90;
        unsigned long long titleId = *(unsigned long long *)&unk88;
        StoreTitleContentState *state;
        std::map<unsigned long long, StoreTitleContentState *>::iterator it = unk58.find(titleId);
        if (it == unk58.end()) {
            state = (StoreTitleContentState *)operator new(0x1000);
            if (state) memset(state, 0, 0x1000);
            unk58[titleId] = state;
        } else {
            state = (*it).second;
        }
        unsigned char *p = (unsigned char *)state + ((idx << 3) & 0x7FFF8);
        *p |= 1;
        SetLoadingState(0xA);
    }
    return DataNode(1);
}

void StoreMetadataManager::DebugDownload() {
    std::set<unsigned long long> downloadedTitles;
    int useCntCache = (TheWiiContentMgr.mMode == 0) ? 1 : 0;
    int success = 1;

    unsigned short contentList[512];

    for (int i = 0; i < mOfferTable->mNumOffers && success; i++) {
        unsigned char flags = mOfferTable->mBufferNewRelease[i].mFlags;
        if (!(flags & 1) || (flags & 2))
            continue;

        StorePackedOffer *offer = mOfferTable->mOffers[i];
        StorePackedSong *firstSong;
        if (offer->mIsRBN) {
            firstSong = &mSongTable->mSongs
                [((StorePackedRBNOffer *)offer)->mSongs[0]];
        } else {
            firstSong = &mSongTable->mSongs[offer->mSongs[0]];
        }
        unsigned long long titleId = WiiCommerceMgr::MakeDataTitleId(&firstSong->unk6);

        if (downloadedTitles.find(titleId) == downloadedTitles.end()) {
            const char *offerName = TheStoreMetadata.GetString(offer->mNameIndex);
            TheDebug << MakeString("DebugDownload: found offer %s\n", offerName);
            TheDebug << MakeString("DebugDownload: EC_DownloadTitle %llx\n", titleId);
            EC_DownloadTitle(titleId, 0);
            DebugWaitAsyncOp(0);
            downloadedTitles.insert(titleId);
        }

        int numContents = 0;
        for (int j = 0; j < offer->mNumSongs; j++) {
            StorePackedSong *song;
            if (offer->mIsRBN) {
                song = &mSongTable->mSongs
                    [((StorePackedRBNOffer *)offer)->mSongs[j]];
            } else {
                song = &mSongTable->mSongs[offer->mSongs[j]];
            }
            unsigned short base = song->unka;
            contentList[numContents] = base;
            numContents++;
            contentList[numContents] = (unsigned short)(base + 1);
            numContents++;
        }

        if (useCntCache) {
            CM_CNTSDCacheClearRSO();
            for (int k = 0; (unsigned long)k < (unsigned long)numContents; k++) {
                CM_CNTSDCachePushDeleteContentRSO(1, titleId, contentList[k]);
            }
        }

        TheDebug << MakeString(
            "DebugDownload: downloading %d content units\n", (unsigned long)numContents
        );
        if (DebugWaitAsyncOp(EC_DownloadContents(titleId, contentList, numContents)) == 0) {
            FormatString fmt1("DebugDownload: failed, so quitting.\n");
            TheDebug << fmt1.Str();
            success = false;
        } else if (useCntCache) {
            for (int k = 0; (unsigned long)k < (unsigned long)numContents; k++) {
                success = DebugSdBackup(titleId, contentList[k]);
                if (!success) {
                    FormatString fmt2("DebugDownload: failed, so quitting.\n");
                    TheDebug << fmt2.Str();
                    break;
                }
            }
        }
    }
    FormatString fmt3("DebugDownload: starting a content refresh.\n");
    TheDebug << fmt3.Str();
    TheWiiContentMgr.mDirty = true;
    TheWiiContentMgr.StartRefresh();
}

void UpdateOfferStateFromEc(
    StoreOfferState *state, const char *offerName, int newPrice,
    const ECContentCatalogInfo *info, const StorePackedOfferBase *offerBase
) {
    StorePackedSong *firstSong;
    if (offerBase->mIsRBN) {
        firstSong = &TheStoreMetadata.mSongTable->mSongs
            [((const StorePackedRBNOffer *)offerBase)->mSongs[0]];
    } else {
        firstSong = &TheStoreMetadata.mSongTable->mSongs
            [((const StorePackedOffer *)offerBase)->mSongs[0]];
    }
    unsigned long long expected = WiiCommerceMgr::MakeDataTitleId(&firstSong->unk6);
    if (info->titleId != expected) {
        TheDebug << MakeString(
            "Store: offer %s from ecommerce says titleid %llx but store index data says %llx\n",
            offerName,
            info->titleId,
            expected
        );
        return;
    }
    unsigned long indexCount = (unsigned long)(offerBase->mNumSongs * 2);
    unsigned long ecCount = info->nIndexes;
    if (ecCount != indexCount) {
        TheDebug << MakeString(
            "Store: offer %s from ecommerce says there are %d contents but store index data says %d\n",
            offerName,
            ecCount,
            (int)indexCount
        );
        return;
    }
    std::vector<unsigned short VECTOR_SIZE_SMALL> ecContents;
    std::vector<unsigned short VECTOR_SIZE_SMALL> expectedContents;
    for (unsigned long i = 0; i < info->nIndexes; i++) {
        ecContents.push_back(info->indexes[i]);
    }
    for (int j = 0; j < offerBase->mNumSongs; j++) {
        StorePackedSong *song;
        if (offerBase->mIsRBN) {
            song = &TheStoreMetadata.mSongTable->mSongs
                [((const StorePackedRBNOffer *)offerBase)->mSongs[j]];
        } else {
            song = &TheStoreMetadata.mSongTable->mSongs
                [((const StorePackedOffer *)offerBase)->mSongs[j]];
        }
        unsigned short base = song->unka;
        expectedContents.push_back(base);
        expectedContents.push_back((unsigned short)(base + 1));
    }
    std::sort(ecContents.begin(), ecContents.end());
    std::sort(expectedContents.begin(), expectedContents.end());
    bool match = true;
    for (unsigned long k = 0; k < info->nIndexes; k++) {
        if (ecContents[k] != expectedContents[k])
            match = false;
    }
    if (match) {
        state->mPrice = (unsigned short)newPrice;
        const char *priceStr = GetAttributeStr(info, "MaxUserFileSize");
        int parsed;
        if (priceStr)
            parsed = strtoul(priceStr, NULL, 10);
        else
            parsed = 0;
        state->unk8 = parsed;
        state->mFlags |= 0x40;
    } else {
        TheDebug << MakeString(
            "Store: for offer %s, ecommerce lists content units [", offerName
        );
        int i;
        for (i = 0; (unsigned long)i < info->nIndexes - 1; i++) {
            TheDebug << MakeString("%d, ", ecContents[i]);
        }
        TheDebug << MakeString("%d] but store index lists [", ecContents[i]);
        for (i = 0; (unsigned long)i < info->nIndexes - 1; i++) {
            TheDebug << MakeString("%d, ", expectedContents[i]);
        }
        TheDebug << MakeString("%d]\n", expectedContents[i]);
    }
}

void DebugPrint(ECContentCatalogInfo *) {}

bool StoreSingleStringTable::LoadFile(const char *filename) {
    char *dataStart, *dataEnd;
    bool ret = StoreLoadPackedFile(
        filename, true, 0x20000, true, true, &mBuffer, &dataStart, &dataEnd, (int *)this
    );
    if (!ret)
        return ret;
    mStrings = (char **)dataStart;
    dataStart += mNumStrings * 4;
    int remaining = (int)(dataEnd - dataStart);
    int estCount = ((remaining >> 31 & 1) + remaining) >> 1;
    if (mNumStrings > estCount) {
        TheDebug << MakeString(
            "%d strings, but based on the data size, there can only be %d.\n",
            mNumStrings,
            estCount
        );
        mNumStrings = estCount;
    }
    int nullCount = 0;
    for (char *p = dataStart; p < dataEnd; p++) {
        if ((signed char)*p == 0)
            nullCount++;
    }
    if (nullCount != mNumStrings) {
        TheDebug << MakeString(
            "There are %d null terminators, should have %d.\n", nullCount, mNumStrings
        );
    }
    int i = 0;
    int iOff = 0;
    char *cur = dataStart;
    for (; i < mNumStrings && cur < dataEnd; ) {
        if (mStrings[i] != cur) {
            FormatString fmt("String %d does not match up\n");
            TheDebug << fmt.Str();
        }
        while (cur < dataEnd && (signed char)*cur != 0)
            cur++;
        if (cur < dataEnd) cur++;
        iOff += 4;
        i++;
    }
    return true;
}

void StorePackedSong::EndianFix() {
    typedef unsigned char u8;
    typedef unsigned short u16;
    u8 *p = (u8 *)this;
    u8 b10 = p[0x10];
    u8 b10b = p[0x10];
    u16 s10 = (u16)((*(u16 *)(p + 0x10) & ~0x40u) | ((b10 << 5) & 0x40u));
    *(u16 *)(p + 0x10) = s10;
    u8 bA = p[0xa];
    *(u16 *)(p + 0xa) = (u16)((*(u16 *)(p + 0xa) & ~0xFF80u) | (((((u16)bA & 1u) << 8u) | p[0xb]) << 7u & 0xFF80u));
    *(u16 *)(p + 0x10) = (u16)((s10 & ~0xFF80u) | (((((u16)b10b & 1u) << 8u) | p[0x11]) << 7u & 0xFF80u));
    mOfferIndex -= 1;
    unk18 -= 1;
    unk1a -= 1;
}
__declspec(noinline) void _outline_EndianFix(StorePackedRanks* _obj) {
    return _obj->EndianFix();
}


void StorePackedOfferBase::EndianFixBase() {
    unsigned char *p = (unsigned char *)this;
    unsigned char b1 = p[0x1];
    unsigned char b40 = p[0x40];
    unsigned char b0 = p[0x0];
    unsigned char b3f = p[0x3f];
    p[0x0] = (unsigned char)((((((b0 & ~0x60) | ((b0 << 5) & 0x60)) & ~0x1C) | (((int)(b0 & 0x1F) >> 2 << 2) & 0x1C)) & ~2) | (((int)(b0 & 0x3F) >> 5 << 1) & 2));
        unsigned long _bm = (unsigned char)(((((b1 & ~0xF8) | ((b1 << 3) & 0xF8)) & ~4) | (((int)(b1 & 0x3F) >> 5 << 2) & 4)) & ~3);
    p[0x1] = _bm | (((int)b1 >> 6) & 3);
    unsigned short s3f = *(unsigned short *)(p + 0x3f);
    s3f = (unsigned short)((s3f & ~0xFFF0) | (((((int)(b40 >> 4 & 0xF) | (b3f << 4 & 0xF0)) << 4 & 0xFF0) | (b40 & 0xF)) << 4 & 0xFFF0));
    *(unsigned short *)(p + 0x3f) = s3f;
    s3f = (unsigned short)((s3f & ~0xF) | ((int)b3f >> 4 & 0xF));
    _outline_EndianFix(&mRanks);
    p[0x41] = ((p[0x41]) & ~0xF8) | (((p[0x41]) << 3) & 0xF8);
}

void StorePackedOffer::EndianFix() {
    EndianFixBase();
    unsigned char b0 = ((unsigned char *)this)[0x0];
    ((unsigned char *)this)[0x0] = b0 & ~0x80;  // clear mIsRBN bit
    for (int i = 0; i < mNumSongs; i++) {
        mSongs[i] -= 1;
    }
}

void StorePackedRBNOffer::EndianFix() {
    EndianFixBase();
    unsigned char b0 = ((unsigned char *)this)[0x0];
    ((unsigned char *)this)[0x0] = b0 | 0x80;   // set mIsRBN bit
    for (int i = 0; i < mNumSongs; i++) {
        mSongs[i] -= 1;
    }
}

const char *StorePackedOfferBase::GetArtist() const {
    if (mArtistIndex != 0) {
        return TheStoreMetadata.GetString(mArtistIndex);
    }
    unsigned char isRbn = mIsRBN;
    StorePackedSong *firstSong;
    if (isRbn)
        firstSong =
            &TheStoreMetadata.mSongTable->mSongs[((const StorePackedRBNOffer *)this)->mSongs[0]];
    else
        firstSong =
            &TheStoreMetadata.mSongTable->mSongs[((const StorePackedOffer *)this)->mSongs[0]];
    unsigned short artistIdx = firstSong->mArtistIndex;
    const StorePackedOffer *asOffer = (const StorePackedOffer *)((char *)this + 2);
    for (int i = 1; i < mNumSongs; i++) {
        StorePackedSong *song;
        if (isRbn)
            song = &TheStoreMetadata.mSongTable->mSongs
                [((const StorePackedRBNOffer *)asOffer)->mSongs[0]];
        else
            song = &TheStoreMetadata.mSongTable->mSongs[asOffer->mSongs[0]];
        if (song->mArtistIndex != artistIdx) {
            return Localize(store_various_artists, NULL);
        }
        asOffer = (const StorePackedOffer *)((char *)asOffer + 2);
    }
    return TheStoreMetadata.GetString(artistIdx);
}

const char *StorePackedOfferBase::GetAlbumName() const {
    if (mAlbumIndex != 0) {
        return TheStoreMetadata.GetString(mAlbumIndex);
    }
    if (OfferType() == kStoreOfferAlbum) {
        return TheStoreMetadata.GetString(mNameIndex);
    }
    return "";
}

const char *StorePackedOffer::GetArtPath() const {
    if (mArtIndex != 0) {
        const char *artStr = TheStoreMetadata.GetString(mArtIndex);
        Symbol plat = PlatformSymbol(TheLoadMgr.GetPlatform());
        return MakeString("/preview_art/%s_nomip.png_%s", artStr, plat);
    }
    return gNullStr;
}

const char *StorePackedRBNOffer::GetArtPath() const {
    unsigned short songIdx = mSongs[0];
    StorePackedSong *song = &TheStoreMetadata.mSongTable->mSongs[songIdx];
    Symbol plat = PlatformSymbol(TheLoadMgr.GetPlatform());
    return MakeString("/album_art/UGC_%d_keep.png_%s", song->mSongID, plat);
}

const char *StorePackedOffer::GetPreviewPath() const {
    if (OfferType() == kStoreOfferSong) {
        unsigned short songIdx = mSongs[0];
        StorePackedSong *song = &TheStoreMetadata.mSongTable->mSongs[songIdx];
        const char *shortName = TheStoreMetadata.GetString(song->unk4);
        return MakeString("/preview_audio/%s_prev.bik", shortName);
    }
    return gNullStr;
}

const char *StorePackedRBNOffer::GetPreviewPath() const {
    unsigned short songIdx = mSongs[0];
    StorePackedSong *song = &TheStoreMetadata.mSongTable->mSongs[songIdx];
    const char *shortName = TheStoreMetadata.GetString(song->unk4);
    return MakeString("/audio_prev/UGC_%s_prev.bik", shortName);
}
