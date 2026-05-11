#include "os/AsyncFile.h"
#include "os/AsyncFileHolmes.h"
#include "os/AsyncFileCNT.h"
#include "os/Debug.h"
#include <string.h>
#include "os/Endian.h"
#include "os/File.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "os/System.h"
#include "os/Archive.h"

class AsyncFileWii : public AsyncFile {
public:
    AsyncFileWii(const char *, int);
    virtual ~AsyncFileWii();
    virtual int GetFileHandle(DVDFileInfo *&);
    virtual void _OpenAsync();
    virtual bool _OpenDone();
    virtual void _WriteAsync(const void *, int);
    virtual bool _WriteDone();
    virtual void _SeekToTell();
    virtual void _ReadAsync(void *, int);
    virtual bool _ReadDone();
    virtual void _Close();
    static bool GetUseDVDRoot();
    void *operator new(size_t t) { return _MemAllocTemp(t, 0); }

    char mWiiPad[0x80 - 0x38]; // AsyncFileWii is 0x80 total; current AsyncFile sizeof is 0x38
};

extern bool HolmesClientCacheFile(char *, const char *);

template <>
void EndianSwapEq(int &i) {
    EndianSwapEq((unsigned int &)i);
}

static int gBufferSize = 0x20000;

void PrintDiscFile(const char *cc) {
    const char *gen = "gen/";
    const char *path = FileGetPath(cc, 0);
    const char *base = FileGetBase(cc, 0);
    const char *ext = FileGetExt(cc);
    int miloCmp = strncmp(ext, "milo", 4);
    if (miloCmp == 0) {
        DataArray *found = SystemConfig()->FindArray("force_milo_inline", false);
        if (found) {
            for (int i = 1; i < found->Size(); i++) {
                if (FileMatch(cc, found->Str(i)))
                    return;
            }
        }
    } else {
        if (strncmp(ext, "wav", 3) == 0 || strncmp(ext, "bmp", 3) == 0
            || strncmp(ext, "png", 3) == 0) {
            if (strstr(cc, "_keep") == 0)
                return;
        } else {
            if (strncmp(ext, "dta", 3) == 0) {
                gen = "";
            } else
                ext = "dtb";
        }
    }
    String fullPath(MakeString("%s/%s%s.%s", path, gen, base, ext));
    unsigned int last = fullPath.find_last_of('_');
    bool lastFound = last != String::npos;
    if (lastFound) {
        Symbol plat = PlatformSymbol(TheLoadMgr.GetPlatform());
        lastFound = plat == fullPath.c_str() + last + 1;
    }
    fullPath = (lastFound) ? fullPath.substr(0, last) : fullPath;
    TheDebug << MakeString("AsyncFile:   '%s'\n", fullPath);
}

extern bool UsingHolmes(int);

AsyncFile *AsyncFile::New(const char *cc, int i) {
    AsyncFile *result = NULL;
    char buf[256];
    if (Archive::DebugArkOrder())
        PrintDiscFile(cc);
    if (UsingHolmes(4) && (i & 4) && !FileIsLocal(cc)) {
        result = new AsyncFileHolmes(cc, i);
    } else if (!UsingCD() && !FileIsLocal(cc)) {
        if (AsyncFileWii::GetUseDVDRoot() && HolmesClientCacheFile(buf, cc)) {
            cc = buf;
        } else {
            result = new AsyncFileHolmes(cc, i);
        }
    }
    bool curCD = UsingCD();
    if (!result) {
        if (FileIsLocal(cc)) {
            result = new AsyncFileCNT(cc, i);
        } else {
            result = new AsyncFileWii(cc, i);
            SetUsingCD(true);
        }
    }
    result->Init();
    SetUsingCD(curCD);
    return result;
}

AsyncFile::AsyncFile(const char *c, int i)
    : mMode(i), mFail(false), unk9(0), mFilename(c), mTell(0), mOffset(0), mBuffer(0),
      mData(0), mBytesLeft(0) {}

int AsyncFile::Read(void *iBuf, int iBytes) {
    ReadAsync(iBuf, iBytes);
    if (mFail)
        return 0;
    else
        while (!ReadDone(iBytes))
            ;
    return iBytes;
}

int AsyncFile::Write(const void *iBuf, int iBytes) {
    int ret;
    WriteAsync((void *)iBuf, iBytes);
    if (mFail)
        return 0;
    else
        while (!WriteDone(ret))
            ;
    return iBytes;
}

bool AsyncFile::ReadAsync(void *iBuff, int iBytes) {
    MILO_ASSERT(iBytes >= 0, 299);
    MILO_ASSERT(mMode & FILE_OPEN_READ, 0x12D);
    if (mFail)
        return false;
    else {
        if (!mBuffer) {
            _ReadAsync(iBuff, iBytes);
        } else {
            if (mTell + iBytes > mSize) {
                iBytes = mSize - mTell;
            }
            MILO_ASSERT(iBytes >= 0, 0x13F);
            mData = (char *)iBuff;
            mBytesLeft = iBytes;
            mBytesRead = 0;
            ReadDone(iBytes);
        }
        return mFail == 0;
    }
}

bool AsyncFile::ReadDone(int &i) {
    if (mFail) {
        i = 0;
        return true;
    } else {
        if (mBuffer && mBytesLeft == 0) {
            i = mBytesRead;
            return true;
        } else {
            if (!_ReadDone()) {
                i = mBytesRead;
                return false;
            } else {
                if (!mBuffer)
                    return true;
                else {
                    if (mOffset + mBytesLeft > gBufferSize) {
                        int size = gBufferSize - mOffset;
                        memcpy(mData, mBuffer + mOffset, size);
                        mBytesRead += size;
                        mOffset = gBufferSize;
                        mTell += size;
                        mBytesLeft -= size;
                        mData += size;
                        FillBuffer();
                        i = mBytesRead;
                        return false;
                    } else {
                        memcpy(mData, mBuffer + mOffset, mBytesLeft);
                        mOffset += mBytesLeft;
                        int ret = mBytesRead + mBytesLeft;
                        mTell += mBytesLeft;
                        mBytesLeft = 0;
                        mBytesRead = ret;
                        i = ret;
                        return true;
                    }
                }
            }
        }
    }
}

bool AsyncFile::WriteDone(int &i) {
    if (mBuffer)
        return true;
    else
        return _WriteDone();
}

bool AsyncFile::WriteAsync(const void *v, int i) {
    MILO_ASSERT(mMode & FILE_OPEN_WRITE, 0x18E);
    if (mFail)
        return false;
    else {
        if (!mBuffer) {
            _WriteAsync(v, i);
        } else {
            do {
                if (mOffset + i > gBufferSize) {
                    int size = gBufferSize - mOffset;
                    memcpy(mBuffer + mOffset, v, size);
                    mOffset = gBufferSize;
                    v = (void *)((int)v + size);
                    mTell += size;
                    Flush();
                    i -= size;
                } else {
                    memcpy(mBuffer + mOffset, v, i);
                    mTell += i;
                    mOffset += i;
                    if (mSize < mTell)
                        mSize = mTell;
                    goto okthen;
                }

            } while (!mFail);
            return false;
        }
    okthen:
        return i != 0;
    }
}

int AsyncFile::Seek(int i, int j) {
    if (mFail)
        return mTell;
    else {
        if (mMode & FILE_OPEN_WRITE)
            Flush();
        else
            MILO_ASSERT(!mBytesLeft, 0x1CA);
        // stuff in between
        _SeekToTell();
        if (mBuffer && (mMode & FILE_OPEN_READ)) {
            mOffset = gBufferSize;
            FillBuffer();
        }
        return mTell;
    }
}

int AsyncFile::Tell() { return mTell; }

void AsyncFile::Flush() {
    if (!mFail && (mMode & FILE_OPEN_WRITE)) {
        _WriteAsync(mBuffer, mOffset);
        while (!_WriteDone())
            ;
        mOffset = 0;
    }
}

void AsyncFile::FillBuffer() {
    if (!mFail && (mMode & FILE_OPEN_READ)) {
        if (mOffset != gBufferSize)
            _SeekToTell();
        int newsize = mSize - mTell;
        _ReadAsync(mBuffer, Min<uint>(gBufferSize, newsize));
        mOffset = 0;
    }
}

bool AsyncFile::Eof() { return mTell == mSize; }

bool AsyncFile::Fail() { return mFail; }
int AsyncFile::Size() { return mSize; }
int AsyncFile::UncompressedSize() { return mUCSize; }

void AsyncFile::Init() {
    if (!(mMode & 0x40000)) {
        mBuffer = (char *)_MemAllocTemp(gBufferSize, 0x20);
    }
    MILO_ASSERT((mMode & (FILE_OPEN_READ | FILE_OPEN_WRITE)) != (FILE_OPEN_READ | FILE_OPEN_WRITE), 0xC4);
    if (!unk9) {
        if (mMode & FILE_OPEN_WRITE) {
            bool curCD = UsingCD();
            SetUsingCD(false);
            FileQualifiedFilename(mFilename, mFilename.c_str());
            SetUsingCD(curCD);
        } else {
            FileQualifiedFilename(mFilename, mFilename.c_str());
        }
    }
    _OpenAsync();
    while (!_OpenDone())
        ;
    if (mFail || strcmp(FileGetExt(mFilename.c_str()), "z") != 0 || !(mMode & FILE_OPEN_READ)
        || mSize < 4) {
        mUCSize = 0;
        goto next;
    }
    mTell = mSize - 4;
    _SeekToTell();
    _ReadAsync(&mUCSize, 4);
    while (!_ReadDone())
        ;
    mTell = 0;
    _SeekToTell();
    EndianSwapEq(mUCSize);
    mSize -= 4;
next:
    if (mMode & FILE_OPEN_READ && mBuffer) {
        mOffset = gBufferSize;
        FillBuffer();
    }
    if (mMode & 0x100) {
        Seek(0, 2);
    }
}

void AsyncFile::Terminate() {
    if (mMode & FILE_OPEN_WRITE) {
        Flush();
    }
    _Close();
    _MemFree(mBuffer);
}
