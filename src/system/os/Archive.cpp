#include "Archive.h"
#include "os/BlockMgr.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/BinStream.h"
#include "utl/FileStream.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Option.h"
#include "math/Sort.h"
#include "zlib/zlib.h"
#include <algorithm>

Archive *TheArchive;
static bool gDebugArkOrder = false;
int kArkBlockSize = 0x10000;

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
        auto _tmp0 = strcmp(mTable[hashIdx], c);
        if (_tmp0 == 0)
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
) {
    if (!file || !*file)
        return false;
    String name(FileGetName(file));
    char path[256];
    FileGetPath(file, path);
    int nameValue = mHashTable.GetHashValue(name.c_str());
    int pathValue = mHashTable.GetHashValue(path);
    if (nameValue == -1 || pathValue == -1)
        return false;
    FileEntry entry;
    entry.mHashedName = nameValue;
    entry.mHashedPath = pathValue;
    std::vector<FileEntry>::iterator it =
        std::lower_bound(mFileEntries.begin(), mFileEntries.end(), entry);
    if (it == mFileEntries.end() || it->mHashedName != nameValue
        || it->mHashedPath != pathValue) {
        arkfileNum = 0;
        byteOffset = 0;
        fileSize = 0;
        fileUCSize = 0;
        return false;
    }
    unsigned long long u7 = 0;
    arkfileNum = 0;
    for (; arkfileNum < mNumArkfiles; arkfileNum++) {
        unsigned long long u6 = mArkfileSizes[arkfileNum] + u7;
        if (it->mOffset < u6)
            break;
        u7 = u6;
    }
    MILO_ASSERT(arkfileNum < mNumArkfiles, 0x1D4);
    byteOffset = it->mOffset - u7;
    fileSize = it->mSize;
    fileUCSize = 0;
    return true;
}

BinStream &operator>>(BinStream &bs, FileEntry &f) {
    bs >> f.mOffset >> f.mHashedName >> f.mHashedPath >> f.mSize >> f.mUCSize;
    long long highDword = ((int *)&f.mOffset)[0];
    if (highDword != 0) {
        MILO_FAIL(
            "operator>>(BinStream&,FileEntry&): file offset > 32 bits. will overflow FileEntryWiiShip::mOffset. high dword:0x%08x)\n",
            (unsigned int)highDword
        );
    }
    return bs;
}

void Archive::Read(int heap_headroom) {
    MILO_LOG("Reading the archive\n");
    FileStream arkhdr(MakeString("%s.hdr", mBasename), FileStream::kReadNoArk, true);
    MILO_ASSERT(!arkhdr.Fail(), 0x2D3);
    arkhdr.EnableReadEncryption();
    int version;
    arkhdr >> version;
    if (version != 6) {
        MILO_FAIL(" ERROR: %s  unsupported archive version %d", mBasename, version);
        return;
    } else {
        arkhdr >> mGuid;
        arkhdr >> mNumArkfiles;
        arkhdr >> mArkfileSizes;
        if (version == 3) {
            for (int i = 0; i < mArkfileSizes.size(); i++) {
                mArkfileNames.push_back(MakeString("%s_%d.ark", mBasename, i));
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
        std::vector<HashCollTestElem> collTest;
        arkhdr >> mFileEntries;

        {
            MemDoTempAllocations m(true, false);
            collTest.resize(mFileEntries.size());
            int overflows = 0;
            int idx;
            int size = mFileEntries.size();
            for (idx = 0; idx < size; idx++) {
                FileEntry &entry = mFileEntries[idx];
                const char *path = mHashTable[entry.mHashedPath];
                const char *name = mHashTable[entry.mHashedName];
                char buf[256];
                FileMakePath(path, name, buf);
                unsigned int hash = HashCRC(buf);
                collTest[idx].mPathName = buf;
                collTest[idx].mHashCRC = hash;
                unsigned int hi = ((unsigned int *)&entry.mOffset)[0];
                unsigned int lo = ((unsigned int *)&entry.mOffset)[1];
                if (hi != 0) {
                    overflows++;
                    TheDebug << MakeString(
                        "Archive::Read() file offset for %s (0x%08x%08x) is past the 32 bit boundary!\n",
                        buf,
                        hi,
                        lo
                    );
                }
            }
            if (overflows != 0) {
                MILO_FAIL(
                    "Archive::Read() there were %d 32 bit offset overflows in the file entry table!\n",
                    overflows
                );
            }
        }
        arkhdr.DisableEncryption();

        std::sort(collTest.begin(), collTest.end());
        int collisions = 0;
        for (int i = 1, collSize = collTest.size(); i < collSize; i++) {
            if (collTest[i - 1].mHashCRC == collTest[i].mHashCRC) {
                if (collTest[i - 1].mPathName == collTest[i].mPathName) {
                    if (collTest[i - 1].mPathName != "__split_ark__") {
                        TheDebug << MakeString(
                            "Archive::Read() DUPLICATE ENTRIES FOR PATH %s\n",
                            collTest[i].mPathName.c_str()
                        );
                    }
                } else {
                    collisions++;
                    TheDebug << MakeString(
                        "Archive::Read() HASH COLLISION - files %s and %s both map to crc hash value 0x%08x!\n",
                        collTest[i - 1].mPathName.c_str(),
                        collTest[i].mPathName.c_str(),
                        collTest[i].mHashCRC
                    );
                }
            }
        }
        if (collisions != 0) {
            MILO_FAIL(
                "Archive::Read() there were %d crc hash collisions in the file entry table!\n",
                collisions
            );
        }
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