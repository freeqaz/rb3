#include "NetLoader.h"
#include "obj/DataFile.h"
#include "os/File.h"
#include "utl/BufStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/NetCacheMgr.h"
#include "utl/Str.h"
#include <obj/Task.h>

const float NetLoaderStub::kNetSimKbPerSecond = 32.0f;
const float NetLoaderStub::kNetSimInitialDelay = 0.2f;

NetLoader::NetLoader(const String &pStrRemotePath)
    : mStrRemotePath(pStrRemotePath), mIsLoaded(false), mBuffer(nullptr), mSize(-1), unk_0x20(0) {
    MILO_ASSERT(TheNetCacheMgr, 0x30);
}

NetLoader::~NetLoader() {
    if (mBuffer != nullptr) {
        _MemFree(mBuffer);
        mBuffer = nullptr;
    }
}

bool NetLoader::IsLoaded() { return mIsLoaded; }

const char *NetLoader::GetRemotePath() const { return mStrRemotePath.c_str(); }

int NetLoader::GetSize() { return mSize; }

char *NetLoader::GetBuffer() {
    if (mIsLoaded != false) {
        return mBuffer;
    }
    return 0;
}

char *NetLoader::DetachBuffer() {
    if (mIsLoaded == false) {
        return 0;
    }
    char *buf = mBuffer;
    mBuffer = 0;
    return buf;
}

void NetLoader::AttachBuffer(char *pBuf) {
    if (mBuffer != 0) {
        MILO_ASSERT(mIsLoaded, 0x74);
        if (mBuffer != 0) {
            _MemFree(mBuffer);
            mBuffer = 0;
        }
    }
    mBuffer = pBuf;
}

void NetLoader::SetSize(int pSize) { mSize = pSize; }

void NetLoader::PostDownload() { mIsLoaded = mBuffer != 0; }

NetLoader *NetLoader::Create(const String &str) {
    if (TheNetCacheMgr->IsServerLocal()) {
        return new NetLoaderStub(str);
    } else {
        return new NetLoaderWii(str);
    }
}

NetLoaderStub::NetLoaderStub(const String &str) : NetLoader(str), mFileLoader(nullptr) {
    FilePath path(
        MakeString("%s/%s", TheNetCacheMgr->GetServerRoot(), mStrRemotePath.c_str())
    );
    mFileLoader =
        new FileLoader(path, path.c_str(), kLoadFront, 0, false, true, nullptr);
    MILO_ASSERT(mFileLoader, 0xa2);
    float sizeKb = mFileLoader->GetSize() * 0.0009765625f;
    mNetSimEndTime = kNetSimInitialDelay + TheTaskMgr.UISeconds() + sizeKb * 0.03125f;
}

NetLoaderStub::~NetLoaderStub() {
    RELEASE(mFileLoader);
}

bool NetLoaderStub::HasFailed() { return !mBuffer; }

bool NetLoaderStub::IsSafeToDelete() const { return 1; }

void NetLoaderStub::PollLoading() {
    MILO_ASSERT(mFileLoader, 0xb2);
    if (mIsLoaded == false) {
        if (mFileLoader->IsLoaded() == 0) {
            TheLoadMgr.Poll();
        }
        if (mFileLoader->IsLoaded() != 0) {
            float uiSeconds = TheTaskMgr.UISeconds();
            if (mNetSimEndTime <= uiSeconds) {
                int size = -1;
                const char *buf = mFileLoader->GetBuffer(&size);
                AttachBuffer((char *)buf);
                SetSize(size);
                PostDownload();
            }
        }
        return;
    }
}

DataNetLoader::DataNetLoader(const String &str) : mLoader(nullptr), unk_0x4(nullptr) {
    if (!TheNetCacheMgr) {
        MILO_FAIL("Tried to create a DataNetLoader, but TheNetCacheMgr is NULL.\n");
    } else {
        mLoader = TheNetCacheMgr->AddNetLoader(str.c_str(), (NetLoaderPos)0);
    }
}

DataNetLoader::~DataNetLoader() {
    if (mLoader) {
        TheNetCacheMgr->DeleteNetLoader(mLoader);
        mLoader = nullptr;
    }
    if (unk_0x4) {
        unk_0x4->Release();
        unk_0x4 = nullptr;
    }
}

void DataNetLoader::PollLoading() {
    if (mLoader) {
        if (mLoader->mIsLoaded) {
            int size = mLoader->mSize;
            char *buffer = nullptr;
            if (mLoader->mIsLoaded) {
                buffer = mLoader->mBuffer;
            }
            const char *remotePath = mLoader->mStrRemotePath.c_str();
            if (streq(FileGetExt(remotePath), "dtz")) {
                DataArray::SetFile(remotePath);
                unk_0x4 = LoadDtz(buffer, size);
            } else {
                BufStream bs(buffer, size, true);
                unk_0x4 = DataReadStream(&bs);
            }
        } else if (!mLoader->HasFailed()) {
            return;
        }
        TheNetCacheMgr->DeleteNetLoader(mLoader);
        mLoader = nullptr;
    }
}

bool DataNetLoader::IsLoaded() {
    bool loaderIsLoaded = true;
    if (mLoader != 0) {
        loaderIsLoaded = mLoader->mIsLoaded;
    }
    bool retVal = false;
    if ((loaderIsLoaded != false) && (unk_0x4 != 0)) {
        retVal = true;
    }
    return retVal;
}

bool DataNetLoader::HasFailed() {
    NetLoader *loader = mLoader;
    if (mLoader != 0) {
        return mLoader->HasFailed();
    }
    return unk_0x4 == 0;
}
