#include "utl/NetLoader.h"
#include "utl/HttpWii.h"
#include "utl/MakeString.h"
#include "utl/NetCacheMgr.h"
#include "os/Debug.h"
#include "os/Timer.h"

NetLoaderWii::NetLoaderWii(const String &str)
    : NetLoader(str), mHandle(-1), mDataBuffer(nullptr), mUrl() {
    mUrl = MakeString(
        "%s%s",
        MakeString(
            "https://%s:%u%s",
            TheNetCacheMgr->GetServer(),
            TheNetCacheMgr->GetPort(),
            TheNetCacheMgr->GetServerRoot()
        ),
        str
    );
    SetState(kDispatchRequest);
}

NetLoaderWii::~NetLoaderWii() {
    if (mHandle >= 0) {
        TheHttpWii.CancelAsync(mHandle);
    }
    if (mDataBuffer != nullptr) {
        _MemFree(mDataBuffer);
        mDataBuffer = nullptr;
    }
}

bool NetLoaderWii::SendRequest() {
    mContentLength = 0;
    mHandle = TheHttpWii.GetFileAsync(mUrl.c_str(), nullptr, 0);
    return mHandle >= 0;
}

bool NetLoaderWii::GetContentLength() {
    unsigned long contentLen = 0;
    int status = TheHttpWii.CompleteAsync(mHandle, contentLen);
    if (status < 0) {
        mHandle = -1;
        return false;
    }
    if (status == 100) {
        mContentLength = contentLen;
    }
    return true;
}

bool NetLoaderWii::DispatchDownload() {
    mReceived = 0;
    mHandle = TheHttpWii.GetFileAsync(mUrl.c_str(), nullptr, 0);
    return mHandle >= 0;
}

bool NetLoaderWii::ReceiveResponse() {
    unsigned long received = 0;
    int status = TheHttpWii.CompleteAsync(mHandle, received);
    if ((unsigned int)status <= 99) {
        return true;
    }
    if (status == 100) {
        long long size = TheHttpWii.mDataBufferSize;
        mContentLength = size;
        mReceived = size;
        mHandle = -1;
        return true;
    }
    mHandle = -1;
    return false;
}

void NetLoaderWii::FinishTransaction() {
    if (!IsLoaded()) {
        AttachBuffer((char *)mDataBuffer);
        mDataBuffer = nullptr;
        SetSize(mContentLength);
        PostDownload();
    }
}

void NetLoaderWii::SetState(State state) { mState = state; }

void NetLoaderWii::PollLoading() {
    Timer::Sleep(5);
    switch (mState) {
    case kSendRequest:
        if (SendRequest()) {
            SetState(kGetContentLength);
        } else {
            SetState(kFailure);
        }
        break;
    case kGetContentLength:
        if (GetContentLength()) {
            if (mContentLength != 0) {
                SetState(kDispatchRequest);
            }
        } else {
            SetState(kFailure);
        }
        break;
    case kDispatchRequest:
        if (DispatchDownload()) {
            SetState(kReceiveRequest);
        } else {
            if (mDataBuffer != nullptr) {
                _MemFree(mDataBuffer);
                mDataBuffer = nullptr;
            }
            SetState(kFailure);
        }
        break;
    case kReceiveRequest:
        if (ReceiveResponse()) {
            if (mReceived == mContentLength && mReceived != 0) {
                mDataBuffer = TheHttpWii.mDataBuffer;
                SetState(kDone);
            }
        } else {
            if (mDataBuffer != nullptr) {
                _MemFree(mDataBuffer);
                mDataBuffer = nullptr;
            }
            SetState(kFailure);
        }
        break;
    case kDone:
        FinishTransaction();
        break;
    default:
        break;
    }
}

bool NetLoaderWii::HasFailed() {
    return mState == kFailure || mState == kFailureComplete;
}

bool NetLoaderWii::IsSafeToDelete() const { return true; }
