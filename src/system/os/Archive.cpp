#include "Archive.h"
#include "os/BlockMgr.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/BinStream.h"
#include "utl/FileStream.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Option.h"
#include "math/Sort.h"
#include "zlib/zlib.h"

Archive *TheArchive;
bool gDebugArkOrder = false;
int kArkBlockSize = 0x10000;

int HashCRC(const char *str) {
    if (!str || !*str)
        return 0;
    static signed char sInit;
    static const uLongf *pTbl;
    unsigned int crc = 0xFFFFFFFF;
    if (!sInit) {
        pTbl = get_crc_table();
        sInit = true;
    }
    const char *p = str;
    while (*p) {
        crc = (crc >> 8) ^ pTbl[(crc ^ *p) & 0xff];
        p++;
    }
    return crc ^ 0xFFFFFFFF;
}

ArkHash::ArkHash() : mHeap(0), mHeapEnd(0), mFree(0), mTable(0), mTableSize(0) {}

int ArkHash::GetHashValue(const char *c) const {
    int hashIdx = HashString(c, mTableSize);
    MILO_ASSERT(hashIdx < mTableSize, 0x107);
    while (mTable[hashIdx]) {
        if (strcmp(mTable[hashIdx], c) == 0)
            return hashIdx;
        if (++hashIdx == mTableSize)
            hashIdx = 0;
    }
    return -1;
}

int ArkHash::Read(BinStream &bs, int len) {
    _MemFree(mHeap);
    _MemFree(mTable);

    int heapSize;
    bs >> heapSize;
    int allocSize = heapSize + len;
    char *heap = (char *)_MemAlloc(allocSize, 0);
    bs.Read(heap, heapSize);
    bs >> mTableSize;
    mHeap = heap;
    mHeapEnd = heap + allocSize;
    mFree = heap + heapSize;
    memset(mFree, 0, mHeapEnd - mFree);

    mTable = (char **)_MemAlloc(mTableSize * sizeof(char *), 0);
    char **p = mTable;
    char **pEnd = mTable + mTableSize;
    while (p != pEnd) {
        int offset;
        bs >> offset;
        if (offset != 0) {
            *p = heap + offset;
        } else {
            *p = nullptr;
        }
        p++;
    }
}

Archive::Archive(const char *c, int i) : mBasename(c), mMode(kRead), mIsPatched(false) {
    Read(i);
}

bool Archive::GetFileInfo(
    const char *file,
    int &arkfileNum,
    unsigned long long &byteOffset,
    int &fileSize,
    int &fileUCSize
) {}

BinStream &operator>>(BinStream &bs, FileEntry &f) {
    bs >> f.mOffset >> f.mHashedName >> f.mHashedPath >> f.mSize >> f.mUCSize;
    if (f.mOffset != 0) {
        MILO_FAIL(
            "operator>>(BinStream&,FileEntry&): file offset > 32 bits. will overflow FileEntryWiiShip::mOffset. high dword:0x%08x)\n",
            f.mOffset
        );
    }
    return bs;
}

void Archive::Read(int heap_headroom) {
    TheDebug << MakeString("Reading the archive\n");
    FileStream arkhdr(MakeString("%s.hdr", mBasename), FileStream::kReadNoArk, true);
    MILO_ASSERT(!arkhdr.Fail(), 723);
    arkhdr.EnableReadEncryption();
    int version;
    arkhdr >> version;
    if (version != 6) {
        MILO_FAIL(" ERROR: %s  unsupported archive version %d", mBasename, version);
        return;
    } else {
        arkhdr >> mGuid;
        arkhdr >> mNumArkfiles >> mArkfileSizes;

        if (version == 3) {
            for (int i = 0; i < mArkfileSizes.size(); i++) {
                mArkfileNames.push_back(String(MakeString("%s_%d.ark", mBasename, i)));
            }
        } else
            arkhdr >> mArkfileNames;

        if (version > 5)
            arkhdr >> mArkfileCachePriority;
        else {
            for (int i = 0; i < mArkfileSizes.size(); i++) {
                mArkfileCachePriority.push_back(-1);
            }
        }

        mHashTable.Read(arkhdr, heap_headroom);

        arkhdr >> mFileEntries;
    }
}

bool Archive::DebugArkOrder() { return gDebugArkOrder; }

bool Archive::HasArchivePermission(int i) const {
    for (int x = unk64, idx = 0; x > 0; x--, idx++) {
        if (i == unk60[idx])
            return true;
    }
    return false;
}

void Archive::SetArchivePermission(int i, const int *ci) {
    unk64 = i;
    unk60 = ci;
}

int Archive::GetArkfileCachePriority(int arkfileNum) const {
    MILO_ASSERT(arkfileNum < mArkfileCachePriority.size(), 0x4BB);
    return mArkfileCachePriority[arkfileNum];
}

int Archive::GetArkfileNumBlocks(int file) const {
    return (mArkfileSizes[file] - 1) / kArkBlockSize + 1;
}

void Archive::GetGuid(HxGuid &g) const { g = mGuid; }

const char *Archive::GetArkfileName(int filenum) const {
    MILO_ASSERT(filenum < mArkfileNames.size(), 1227);
    return mArkfileNames[filenum].c_str();
}

void ArchiveInit() {
    if (UsingCD() || OptionBool("force_ark", false)) {
        Symbol plat = PlatformSymbol(TheLoadMgr.GetPlatform());
        const char *hdrName;
        if (UsingCD()) {
            String titlePath(TheContentMgr->TitleContentPath());
            if (!titlePath.empty()) {
                hdrName = MakeString("%s/gen/patch_%s", titlePath.c_str(), plat);
            }
        } else {
            hdrName = MakeString("gen/patch_%s", plat);
        }
        TheArchive = new Archive(MakeString("gen/main_%s", plat), 0);
        TheArchive->SetArchivePermission(1, &preinitArk);
    }
    gDebugArkOrder = OptionBool("debug_arkorder", false);
    TheBlockMgr.Init();
}