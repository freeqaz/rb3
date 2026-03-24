#include "HDCache.h"
#include "os/Debug.h"
#include "os/OSFuncs.h"
#include "os/Archive.h"
#include "os/System.h"
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
        int numarkfiles = TheArchive->mNumArkfiles;
        mReadArkFiles.resize(numarkfiles);
        mWriteArkFiles.resize(numarkfiles);
        FileStream *fs = OpenHeader();
        if (fs) {
            if (fs->Tell() == 0) {
            }
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
            blockStateSize += ((TheArchive->GetArkfileNumBlocks(i) + 0x1F) / 32 + 1) * 4;
        }
    }
    int ret = blockStateSize + 0x100;
    int remainder = ret % 4096;
    if (remainder != 0) {
        ret = ret - remainder + 0x1000;
    }
    return ret;
}

FileStream *HDCache::OpenHeader() {
    if (mHdrFmt.mStr[0] == '\0') {
        return nullptr;
    } else {
        const char *str;
        for (int i = 0; i < 2; ++i) {
            str = MakeString(mHdrFmt.mStr, 0);
            if (FileExists(str, 0x10000) != 0) {
                return new FileStream(str, FileStream::kReadNoArk, true);
            }
        }
        return nullptr;
    }
}
