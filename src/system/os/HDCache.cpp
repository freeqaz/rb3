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

void HDCache::OpenFiles(int numFilesToOpen) {}

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
    ArkFile *writeArkFile = mWriteArkFiles[mWriteFileIdx];
    if (writeArkFile != nullptr) {
    }
    return false;
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

bool HDCache::LockCache() {
    CritSecTracker cst(mCritSec);
    if (mLockId == 0 || mLockId == CurrentThreadId()) {
        mLockId = CurrentThreadId();
        unk34++;
        return true;
    } else
        return false;
}

void HDCache::UnlockCache() {
    CritSecTracker cst(mCritSec);
    MILO_ASSERT(mLockId == CurrentThreadId(), 0xF9);
    if (!unk34--)
        mLockId = 0;
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
