#include "HDCache.h"
#include "math/SHA1.h"
#include "os/Archive.h"
#include "os/Debug.h"
#include "os/OSFuncs.h"
#include "os/System.h"
#include "utl/BinStream.h"
#include "utl/HxGuid.h"
#include "utl/MemStream.h"
#include "utl/Option.h"

HDCache TheHDCache;

HDCache::HDCache()
    : mBlockState(0), mWriteFileIdx(0), unk18(-1), unk20(0), unk24(0), unk28(-1),
      unk2c(-1), mLockId(0), unk34(0), mCritSec(0), mHdrIdx(0), mHdrBuf(0), unk64(0) {}

HDCache::~HDCache() {}

void HDCache::Init() {
    mCritSec = new CriticalSection();
    if (TheArchive) {
        OptionBool("no_hdcache", true);
        int numArkfiles = TheArchive->mNumArkfiles;
        mReadArkFiles.resize(numArkfiles);
        mWriteArkFiles.resize(numArkfiles);
        FileStream *header = OpenHeader();
        bool next = header && header->Size() == HdrSize();
        if (next) {
            header->EnableReadEncryption();
            int version;
            *header >> version;
            next = version == 2;
        }
        if (next) {
            HxGuid guid1, guid2;
            *header >> guid1;
            TheArchive->GetGuid(guid2);
            next = guid1 == guid2;
        }
        int numFilesToOpen = 0;
        if (next) {
            *header >> numFilesToOpen;
            if (numFilesToOpen < 0 || numFilesToOpen > numArkfiles) {
                numFilesToOpen = 0;
                next = false;
            }
        }
        OpenFiles(numFilesToOpen);
        mBlockState = new int *[numArkfiles];
        CSHA1 sha;
        unsigned char blockBuf[0x1000];
        for (int i = 0; i < numArkfiles; i++) {
            unsigned int blockSize = 0;
            if (i < numFilesToOpen) {
                *header >> blockSize;
                if (blockSize > 0x1000 || (blockSize & 3)) {
                    next = false;
                } else {
                    header->Read(blockBuf, blockSize);
                }
                if (header->Fail() || !next) {
                    blockSize = 0;
                    next = false;
                    numFilesToOpen = 0;
                }
                if (next) {
                    sha.Update(blockBuf, blockSize);
                }
            }
            if (mReadArkFiles[i] == NULL || mReadArkFiles[i]->Fail() ||
                mWriteArkFiles[i] == NULL || mWriteArkFiles[i]->Fail()) {
                if (mReadArkFiles[i] != NULL) {
                    delete mReadArkFiles[i];
                }
                mReadArkFiles[i] = NULL;
                if (mWriteArkFiles[i] != NULL) {
                    delete mWriteArkFiles[i];
                }
                mWriteArkFiles[i] = NULL;
            }
            if (mReadArkFiles[i] != NULL) {
                int numDwords = ((TheArchive->GetArkfileNumBlocks(i) + 0x1F) / 32);
                int *blockMem = new int[numDwords];
                memcpy(blockMem, blockBuf, blockSize);
                memset((char *)blockMem + blockSize, 0, numDwords * 4 - blockSize);
                mBlockState[i] = blockMem;
            } else {
                mBlockState[i] = NULL;
            }
        }
        char hash1[256];
        char hash2[256];
        if (next) {
            memset(hash1, 0, 256);
            memset(hash2, 0, 256);
            sha.Final()->ReportHash(hash1, 0);
            header->Read(hash2, 0x100);
            next = false;
            if (!header->Fail())
                next = memcmp(hash1, hash2, 256) == 0;
        }
        if (OptionBool("skip_hdcache", false)) {
            next = false;
        }
        if (next) {
            unk64 = true;
            TheDebug << MakeString("Using the archive cache\n");
        } else {
            for (int i = 0; i < numArkfiles; i++) {
                if (mBlockState[i] != NULL) {
                    int numDwords = (TheArchive->GetArkfileNumBlocks(i) + 0x1F) / 32;
                    memset(mBlockState[i], 0, numDwords * 4);
                }
            }
        }
        if (header != NULL) {
            delete header;
        }
        mHdrFmt = "";
        mFileFmt = "";
        mHdrBuf = new MemStream(true);
    }
}

void HDCache::OpenFiles(int numCachedArkfiles) {
    if (mFileFmt.empty())
        return;
    int numArkfiles = TheArchive->mNumArkfiles;
    MILO_ASSERT(numCachedArkfiles <= numArkfiles, 0x22E);
    FileMkDir(FileGetPath(mFileFmt.c_str(), 0));
    std::vector<int> pendingArkfiles;
    for (int i = 0; i < numArkfiles; i++) {
        const char *fileFmt = MakeString(mFileFmt.c_str(), i);
        bool exists = FileExists(fileFmt, 0x10000);
        int prio = TheArchive->GetArkfileCachePriority(i);
        if (exists && i > numCachedArkfiles) {
            FileDelete(fileFmt);
        }
        if (prio >= 0) {
            pendingArkfiles.push_back(i);
        }
    }
    const char *hdrFmt = MakeString(mHdrFmt.c_str(), 0);
    mHdr[0] = NewFile(hdrFmt, 0x50204);
    bool hdrValid = mHdr[0] && !mHdr[0]->Fail();
    if (hdrValid) {
        int hdrSize = HdrSize();
        mHdr[0]->Truncate(hdrSize);
        delete mHdr[0]; mHdr[0] = NULL;
        mHdr[0] = NewFile(hdrFmt, 0x50004);
        hdrValid = mHdr[0]->Size() == hdrSize;
    }
    if (!hdrValid) {
        delete mHdr[0]; mHdr[0] = NULL;
        return;
    }
    while (!pendingArkfiles.empty()) {
        int maxPrio = -1;
        std::vector<int>::iterator max = pendingArkfiles.end();
        for (std::vector<int>::iterator it = pendingArkfiles.begin();
             it != pendingArkfiles.end(); ++it) {
            int prio = TheArchive->GetArkfileCachePriority(*it);
            if (prio > maxPrio) {
                maxPrio = prio;
                max = it;
            }
        }
        MILO_ASSERT(max != pendingArkfiles.end(), 0x26E);
        int idx = *max;
        const char *fileFmt = MakeString(mFileFmt.c_str(), idx);
        File *file = NewFile(fileFmt, 0x50204);
        bool ok = false;
        if (file && file->Truncate(TheArchive->GetArkfileNumBlocks(idx) * kArkBlockSize)) {
            ok = true;
        }
        if (file) {
            delete file;
            if (!ok) FileDelete(fileFmt);
        }
        pendingArkfiles.erase(max);
    }
    for (int i = 0; i < numArkfiles; i++) {
        const char *fileFmt = MakeString(mFileFmt.c_str(), i);
        File *read = NewFile(fileFmt, 0x50002);
        File *write = NewFile(fileFmt, 0x50004);
        if (!read || !write || read->Fail() || write->Fail()) {
            if (read) delete read;
            read = NULL;
            if (write) delete write;
            write = NULL;
        }
        mReadArkFiles[i] = (ArkFile *)read;
        mWriteArkFiles[i] = (ArkFile *)write;
    }
}

void HDCache::UnlockCache() {
    CritSecTracker cst(mCritSec);
    MILO_ASSERT(mLockId == CurrentThreadId(), 0xF9);
    if (--unk34 == 0)
        mLockId = 0;
}

void HDCache::Poll() {
    if (unk1c) {
        int done;
        if (mHdr[mHdrIdx]->WriteDone(done)) {
            UnlockCache();
            if (mHdr[mHdrIdx]->Fail()) {
                TheDebug << MakeString("HDCache Write Header Failed\n");
            }
            unk1c = false;
        }
    }
    if (unk24 && !unk1c) {
        if (unk24 > 0x400 || SystemMs() - unk28 > 60000) {
            WriteHdr();
        }
    }
}

void HDCache::WriteHdr() {
    int done;
    if (!mHdr[mHdrIdx]->Fail()) {
        if (LockCache()) {
            MILO_ASSERT(mHdr[mHdrIdx]->WriteDone(done), 0x143);
            CSHA1 sha;
            mHdrBuf->Seek(0, BinStream::kSeekBegin);
            mHdrBuf->EnableWriteEncryption();
            *mHdrBuf << 2;
            HxGuid guid;
            TheArchive->GetGuid(guid);
            *mHdrBuf << guid;
            int numArkfiles = TheArchive->mNumArkfiles;
            *mHdrBuf << numArkfiles;
            for (int i = 0; i < numArkfiles; i++) {
                int blockSize = 0;
                if (mBlockState[i]) {
                    int arkBlocks = TheArchive->GetArkfileNumBlocks(i);
                    int numBlocks = arkBlocks + 0x1F;
                    blockSize = (numBlocks / 32) * 4;
                }
                *mHdrBuf << blockSize;
                if (blockSize > 0) {
                    mHdrBuf->Write(mBlockState[i], blockSize);
                    sha.Update((const unsigned char *)mBlockState[i], blockSize);
                }
            }
            char buf[256];
            memset(buf, 0, 256);
            sha.Final()->ReportHash(buf, 0);
            mHdrBuf->Write(buf, 256);
            mHdrBuf->DisableEncryption();
            unk24 = 0;
            int finalSize = HdrSize();
            MILO_ASSERT(mHdrBuf->Size() <= finalSize, 0x175);
            char zeroPad[0x80];
            memset(zeroPad, 0, 0x80);
            while (mHdrBuf->Size() < finalSize) {
                int size = finalSize - mHdrBuf->Size();
                if (size > 0x80U)
                    size = 0x80;
                mHdrBuf->Write(zeroPad, size);
            }
            MILO_ASSERT(mHdrBuf->Size() == finalSize, 0x182);
            int oldSize = mHdr[mHdrIdx]->Size();
            int newSize = mHdrBuf->Size();
            MILO_ASSERT(oldSize == newSize, 0x185);
            unk1c = true;
            mHdr[mHdrIdx]->Seek(0, 0);
            mHdr[mHdrIdx]->WriteAsync(mHdrBuf->Buffer(), mHdrBuf->Size());
        }
    }
}

bool HDCache::ReadDone() {
    int done;
    ArkFile *readArkFile = mReadArkFiles[unk20];
    if (readArkFile == 0) {
        return true;
    } else {
        readArkFile->ReadDone(done);
    }

    return;
}

bool HDCache::ReadAsync(int arkfileNum, int blockNum, void *ptr) {
    MILO_ASSERT(ReadDone(), 0x190);
    if (mBlockState[arkfileNum]) {
        MILO_ASSERT(blockNum < TheArchive->GetArkfileNumBlocks(arkfileNum), 0x195);
        if ((mBlockState[arkfileNum][(blockNum / 32)] & (1 << (blockNum % 32)))) {
            int blockSize = kArkBlockSize;
            MILO_ASSERT(mReadArkFiles[arkfileNum]->Size() >= ((blockNum + 1) * blockSize), 0x19c);
            unk20 = arkfileNum;
            mReadArkFiles[arkfileNum]->Seek(blockNum * blockSize, 0);
            return mReadArkFiles[unk20]->ReadAsync(ptr, blockSize);
        }
    }
    return false;
}

bool HDCache::ReadFail() {
    ArkFile *readArkFile = mReadArkFiles[unk20];
    if (readArkFile != nullptr) {
        if (readArkFile->Fail()) {
            TheDebug << MakeString("HDCache Read %d failed\n", unk20);
            return true;
        }
    }
    return false;
}

bool HDCache::WriteDone() {
    int done;
    std::vector<ArkFile *> &_ref0 = mWriteArkFiles;
    if (unk18 >= 0 && _ref0[mWriteFileIdx]->WriteDone(done)) {
            MILO_ASSERT(mReadArkFiles[mWriteFileIdx]->Size() == _ref0[mWriteFileIdx]->Size(), 0x1F2);
            UnlockCache();
            if (_ref0[mWriteFileIdx]->Fail()) {
                TheDebug << MakeString("HDCache Write %d.%d failed\n", mWriteFileIdx, unk18);
            } else {
                if (++unk24 == 1) {
                    unk28 = SystemMs();
                }
                mBlockState[mWriteFileIdx][unk18 / 32] |= 1 << (unk18 % 32);
            }
            unk18 = -1;
        }
    return unk18 + 1 == 0;
}

bool HDCache::LockCache() {
    CritSecTracker cst(mCritSec);
    if (mLockId == 0 || mLockId == CurrentThreadId()) {
        mLockId = CurrentThreadId();
        unk34++;
        return true;
    } else
        return false;
}

int HDCache::HdrSize() {
    int blockStateSize = 32;
    int numArkfiles = TheArchive->mNumArkfiles;
    for (int i = 0; i < numArkfiles; i++) {
        if (TheArchive->GetArkfileCachePriority(i) >= 0) {
            int numBlocks = TheArchive->GetArkfileNumBlocks(i) + 0x1F;
            blockStateSize += 4;
            blockStateSize += (numBlocks / 32) * 4;
        }
    }
    blockStateSize += 0x100;
    int remainder = blockStateSize % 4096;
    if (remainder != 0) {
        blockStateSize += 0x1000 - remainder;
    }
    return blockStateSize;
}

FileStream *HDCache::OpenHeader() {
    if (mHdrFmt.mStr[0] == '\0') {
        return nullptr;
    } else {
        const char *str = nullptr;
        int i = 0;
        for (; i < 2; i++) {
            str = MakeString(mHdrFmt.mStr, 0);
            if (FileExists(str, 0x10000) != 0) {
                break;
            }
        }
        if (i == 2)
            return nullptr;
        return new FileStream(str, FileStream::kReadNoArk, true);
    }
}

