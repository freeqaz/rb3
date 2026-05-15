#include "BufStreamNAND.h"
#include "meta/MemcardMgr_Wii.h"
#include "os/Debug.h"
#include "rndwii/Rnd.h"
#include <cstring>


BufStreamNAND::BufStreamNAND(void *buffer, int totalSize, char *filePath, bool b1)
    : FixedSizeSaveableStream(buffer, totalSize, b1), mChecksum(NULL), mBytesChecksummed(0) {
    mBuffer = (char *)buffer;
    MILO_ASSERT(!((int)mBuffer & 31), 0x21);
    mSize = totalSize;
    strcpy(mFilePath, filePath);
    mFileOpen = false;
    Clear();
}

BufStreamNAND::~BufStreamNAND() {
    DeleteChecksum();
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
    return (EofType)(mTell == mSize);
}

void BufStreamNAND::Clear() {
    mFail = mBuffer == 0;
    mTell = 0;
    mRunningTell = 0;
    mChunkSize = 0x40000;
    unk80 = 0;
    mResult = kMCNoError;
    float remainder = (float)fmod(mSize, mChunkSize);
    MILO_ASSERT(remainder == 0.0f, 0x39);
}

int BufStreamNAND::Open() {
    const char *funcName = __FUNCTION__;
    s32 file;
    MCResult result;
    SetGPHangDetectEnabled(false, funcName);
    result = kMCNoError;
    if(!mFileOpen) {
        file = NANDOpen(mFilePath, &mFileInfo, 3);
        result = HandleResultNAND(file);
        if(result)
            mFail = true;
        else
            mFileOpen = true;
    }
    SetGPHangDetectEnabled(true, funcName);

    return result;
}

int BufStreamNAND::Close() {
    s32 file;
    const char* funcName = __FUNCTION__;
    MCResult result;
    SetGPHangDetectEnabled(false, funcName);
    result = kMCNoError;
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
    SetGPHangDetectEnabled(true, funcName);
    return result;
}

int BufStreamNAND::DoSeek(int offset, BinStream::SeekType seekType) {
    const char *funcName = __FUNCTION__;
    SetGPHangDetectEnabled(false, funcName);
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
        SetGPHangDetectEnabled(true, funcName);
        return kMCAccessError;
    }

    if(offset < 0 || offset > mSize) {
        mFail = true;
        SetGPHangDetectEnabled(true, funcName);
        return kMCAccessError;
    }

    MILO_ASSERT(mFileOpen, 0x8D);

    s32 res = NANDSeek(&mFileInfo, offset, 0);
    MCResult result;
    if(res < 0) {
        if(res == NAND_RESULT_INVALID) {
            if(TheMemcardMgr.IsWriteMode()) {
                result = (MCResult)Pad(offset);
            } else {
                MILO_ASSERT(false, 0x9C);
                mFail = true;
                SetGPHangDetectEnabled(true, funcName);
                return kMCUnknownError;
            }
            res = NANDSeek(&mFileInfo, 0, 2);
        } else {
            result = HandleResultNAND(res);
        }
        if(result) {
            mFail = true;
            SetGPHangDetectEnabled(true, funcName);
            return result;
        }
    }

    MILO_ASSERT(res == offset, 0xAF);
    mRunningTell = offset;
    SetGPHangDetectEnabled(true, funcName);
    return kMCNoError;
}

MCResult BufStreamNAND::FinishStream() {
    MCResult result = (MCResult)Close();
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
    case NAND_RESULT_ALLOC_FAILED:
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
    case NAND_RESULT_NOTEMPTY:
        result = kMCFileNotFound;
        break;
    case NAND_RESULT_UNKNOWN:
        result = kMCUnknownError;
        break;
    default:
        if (nandResult == NAND_RESULT_ALLOC_FAILED) {
            MILO_WARN("BufStreamNAND: NAND_RESULT_ALLOC_FAILED %d", nandResult);
        } else if (nandResult == NAND_RESULT_FATAL_ERROR) {
            MILO_WARN("BufStreamNAND: NAND_RESULT_FATAL_ERROR %d", nandResult);
        }
        MILO_WARN("BufStreamNAND: unknown NAND result %d", nandResult);
        result = kMCGeneralError;
        break;
    }
    SetResult(result);
    return result;
}

void BufStreamNAND::ReadImpl(void *data, int count) {
    int bytes = count;
    if(!mFail) {
        if((mTell + count) > mChunkSize || !mRunningTell)
            LoadBufferFromNAND();

        int size = mSize;
        if((mRunningTell + bytes) > size || (mTell + bytes) > mChunkSize) {
            mFail = true;
            bytes = size - mTell;
        }
        memcpy(data, &mBuffer[mTell], bytes);
        mRunningTell += bytes;
        mTell += bytes;
        if(mChecksum) {
            if(!mFail) {
                mChecksum->Update((unsigned char*)data, bytes);
                mBytesChecksummed += bytes;
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
    int size = mSize;
    if(mRunningTell + count > size || mTell + count > mChunkSize) {
        mFail = true;
        count = size - mTell;
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
    delete mChecksum;
    mChecksum = 0;
}

int BufStreamNAND::LoadBufferFromNAND() {
    const char *funcName = __FUNCTION__;
    MCResult result;
    int v3;
    s32 file;
    SetGPHangDetectEnabled(false, funcName);
    file = Open();
    DoSeek(0, kSeekCur);
    v3 = mChunkSize;
    int v4 = mRunningTell;
    int v5 = mSize;

    if(v4 + v3 > v5)
        v3 = v5 - v4;
    s32 res = NANDRead(&mFileInfo, mBuffer, v3);

    if(res != v3) {
        mFail = true;
        result = HandleResultNAND(res);
        SetGPHangDetectEnabled(true, funcName);
        return result;
    }
    else {
        mTell = 0;
        SetGPHangDetectEnabled(true, funcName);
        return file;
    }
}

int BufStreamNAND::SaveBufferToNAND(bool b1) {
    const char *funcName = __FUNCTION__;
    s32 file;
    SetGPHangDetectEnabled(false, funcName);
    file = Open();
    s32 write = NANDWrite(&mFileInfo, mBuffer, mTell);
    if(write != mTell) {
        mFail = true;
        file = HandleResultNAND(write);
        SetGPHangDetectEnabled(true, funcName);
        return file;
    }
    memset(mBuffer, 0, mChunkSize);
    mTell = 0;
    if(b1 && mRunningTell == mSize) {
        file = Close();
        if(file) {
            mFail = true;
            SetGPHangDetectEnabled(true, funcName);
            return file;
        }
    }
    SetGPHangDetectEnabled(true, funcName);
    return file;
}

bool BufStreamNAND::FinishWrite() {
    bool result = SaveBufferToNAND(false);
    if(result)
        mFail = true;
    return result;
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
