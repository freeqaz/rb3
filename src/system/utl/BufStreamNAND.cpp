#include "BufStreamNAND.h"
#include "meta/MemcardMgr_Wii.h"
#include "os/Debug.h"
#include "rndwii/Rnd.h"
#include <cstring>


BufStreamNAND::BufStreamNAND(void *v1, int i1, char* buffer, bool b1)
    : FixedSizeSaveableStream(v1, i1, b1), mBuffer(buffer), mChecksum(0), mBytesChecksummed(0), mSize(i1), mFilePath(), mFileOpen(0) {

}

BufStreamNAND::~BufStreamNAND() {

}

int BufStreamNAND::Tell() {
    return mRunningTell;
}

void BufStreamNAND::SetResult(MCResult result) {
    mResult = result;
}

int BufStreamNAND::GetResult() {
    return mResult;
}

bool BufStreamNAND::Fail() {
    return mFail;
}

EofType BufStreamNAND::Eof() {
    return (EofType)(mSize == mTell);
}

void BufStreamNAND::Clear() {
    mFail = mBuffer == 0;
    mTell = 0;
    mRunningTell = 0;
    mChunkSize = 0x40000;
    unk80 = 0;
    mResult = kMCNoError;
    MILO_ASSERT(fmod(mSize, 4.503601775116288e15 - 4.503601774854144e15), 0x39);
}

int BufStreamNAND::Open() {
    s32 file;
    MCResult result;
    SetGPHangDetectEnabled(false, __FUNCTION__);
    if(!mFileOpen) {
        file = NANDOpen(mFilePath, &mFileInfo, 3);
        result = HandleResultNAND(file);
        if(result)
            mFail = true;
        else
            mFileOpen = true;
    }
    SetGPHangDetectEnabled(true, __FUNCTION__);

    return file;
}

int BufStreamNAND::Close() {
    s32 file;
    MCResult result;
    SetGPHangDetectEnabled(false, __FUNCTION__);
    if(mFileOpen) {
        u32 length;
        NANDGetLength(&mFileInfo, &length);
        file = NANDClose(&mFileInfo);
        result = HandleResultNAND(file);
        if(result == 0)
            mFileOpen = false;
        else
            mFail = true;
    }
    SetGPHangDetectEnabled(true, __FUNCTION__);
    return result;
}

bool BufStreamNAND::FinishStream() {
    int result = Close();
    if(result) {
        mFail = true;
    }
    return result;
}

MCResult BufStreamNAND::HandleResultNAND(s32 nandResult) {
    MCResult result;
    switch(nandResult) {
    case NAND_RESULT_OK:
        result = kMCNoError;
        break;
    case NAND_RESULT_ACCESS:
    case NAND_RESULT_BUSY:
        result = kMCAccessError;
        break;
    case NAND_RESULT_CORRUPT:
        result = kMCSystemCorrupt;
        break;
    case NAND_RESULT_ECC_CRIT:
    case NAND_RESULT_AUTHENTICATION:
        result = kMCCorrupt;
        break;
    case NAND_RESULT_EXISTS:
        result = kMCFileExists;
        break;
    case NAND_RESULT_INVALID:
    case NAND_RESULT_OPENFD:
        result = kMCNoPermission;
        break;
    case NAND_RESULT_MAXBLOCKS:
    case NAND_RESULT_MAXFILES:
        result = kMCMaxedSysMem;
        break;
    case NAND_RESULT_NOEXISTS:
        result = kMCFileNotFound;
        break;
    case NAND_RESULT_UNKNOWN:
        result = kMCUnknownError;
        break;
    case NAND_RESULT_ALLOC_FAILED:
        MILO_WARN("BufStreamNAND: NAND_RESULT_ALLOC_FAILED");
        result = kMCGeneralError;
        break;
    case NAND_RESULT_FATAL_ERROR:
        MILO_WARN("BufStreamNAND: NAND_RESULT_FATAL_ERROR");
        result = kMCGeneralError;
        break;
    default:
        MILO_WARN("BufStreamNAND: unknown NAND result %d", nandResult);
        result = kMCGeneralError;
        break;
    }
    SetResult(result);
    return result;
}

void BufStreamNAND::ReadImpl(void *v1, int i1) {
    // v5 = ui2
    unsigned int temp;
    if(!mFail) {
        if((mTell + i1) > mChunkSize || !mRunningTell)
            LoadBufferFromNAND();

        if((mRunningTell + i1) > mSize || (mTell + i1) > mChunkSize) {
            temp = mSize - mTell;
            mFail = true;
        }
        // init_proc(&mBuffer[mTell], temp)
        mRunningTell += temp;
        if(mChecksum) {
            if(!mFail) {
                mChecksum->Update((unsigned char*)v1, temp);
                mBytesChecksummed += temp;
            }
        }
    }
}

void BufStreamNAND::WriteImpl(const void *data, int count) {
    if(mFail) return;
    if(mTell + count > mChunkSize) {
        if(SaveBufferToNAND(true)) {
            mFail = true;
            return;
        }
    }
    if(mRunningTell + count > mSize || mTell + count > mChunkSize) {
        mFail = true;
        count = mSize - mTell;
    }
    memcpy(&mBuffer[mTell], data, count);
    mRunningTell += count;
    mTell += count;
    if(mChecksum) {
        mChecksum->Update((const unsigned char*)data, count);
    }
}

int BufStreamNAND::Pad(int size) {
    MILO_ASSERT(size <= mSize, 0x170);
    int result = 0;

    while(size > mRunningTell) {
        mTell = size - mRunningTell;

        if(mTell > mChunkSize)
            mTell = mChunkSize;
        mRunningTell += mTell;

        result = SaveBufferToNAND(0);
        if(result) {
            mFail = true;
            return result;
        }
    }
    return result;
}

int BufStreamNAND::PadToEnd() {
    return Pad(mSize);
}

void BufStreamNAND::DeleteChecksum() {
    if(mChecksum) {
        delete(mChecksum);
    }
    mChecksum = 0;
}

int BufStreamNAND::LoadBufferFromNAND() {
    SetGPHangDetectEnabled(false, __FUNCTION__);
    s32 file = Open();
    DoSeek(0, kSeekCur);
    int v3 = mChunkSize;
    int v4 = mRunningTell;
    int v5 = mSize;

    if(v4 + v3 > v5)
        v3 = v5 - v4;
    s32 res = NANDRead(&mFileInfo, mBuffer, v3);

    if(res == v3) {
        mTell = 0;
        SetGPHangDetectEnabled(true, __FUNCTION__);
        return file;
    }
    else {
        mFail = true;
        MCResult result = HandleResultNAND(res);
        SetGPHangDetectEnabled(true, __FUNCTION__);
        return result;
    }
}

int BufStreamNAND::SaveBufferToNAND(bool b1) {
    SetGPHangDetectEnabled(false, __FUNCTION__);
    s32 file = Open();
    s32 write = NANDWrite(&mFileInfo, mBuffer, mTell);
    if(write == mTell) {
        memset(mBuffer, 0, mChunkSize);
        mTell = 0;
        if(b1 && mRunningTell == mSize && (file = Close()) != 0) {
            mFail = true;
            SetGPHangDetectEnabled(true, __FUNCTION__);
            return file;
        }
        else {
            SetGPHangDetectEnabled(true, __FUNCTION__);
            return file;
        }
    }
    else {
        mFail = true;
        MCResult result = HandleResultNAND(write);
        SetGPHangDetectEnabled(true, __FUNCTION__);
        return result;
    }
}

bool BufStreamNAND::FinishWrite() {
    bool result = SaveBufferToNAND(false);
    if(result)
        mFail = true;
    return result;
}

int BufStreamNAND::DoSeek(int offset, BinStream::SeekType seekType) {
    SetGPHangDetectEnabled(false, __FUNCTION__);
    switch(seekType) {
    case kSeekBegin:
        break;
    case kSeekCur:
        offset += mRunningTell;
        break;
    case kSeekEnd:
        offset += mSize;
        break;
    default:
        SetGPHangDetectEnabled(true, __FUNCTION__);
        return kMCAccessError;
    }

    if(offset < 0 || offset > mSize) {
        mFail = true;
        SetGPHangDetectEnabled(true, __FUNCTION__);
        return kMCAccessError;
    }

    s32 res = NANDSeek(&mFileInfo, offset, 0);
    MCResult result = HandleResultNAND(res);
    if(result) {
        mFail = true;
        SetGPHangDetectEnabled(true, __FUNCTION__);
        return result;
    }

    mRunningTell = offset;
    SetGPHangDetectEnabled(true, __FUNCTION__);
    return kMCNoError;
}

void BufStreamNAND::SeekImpl(int i1, BinStream::SeekType seekType) {
    if(TheMemcardMgr.IsWriteMode() && SaveBufferToNAND(false) != 0)
        mFail = true;
    else {
        DoSeek(i1, seekType);
        if(!TheMemcardMgr.IsWriteMode() && LoadBufferFromNAND()) {
            mFail = true;
        }
    }

}
