#include "StoreEnumeration.h"
#include "StorePackedMetadata.h"

WiiEnumeration::WiiEnumeration(int i) : mLoading(true) {
    if (i != 0)
        mState = kEnumWaiting;
    else
        mState = kPreSuccess;
}

void WiiEnumeration::Start() {}

bool WiiEnumeration::IsSuccess() const { return mState == kSuccess; }

bool WiiEnumeration::IsEnumerating() const {
    return (mState != kSuccess && mState != kFail) ? true : false;
}

void WiiEnumeration::Poll() {
    if (mState == kEnumWaiting) {
        int loading = 1;
        unsigned int flags = TheStoreMetadata.mFlags;
        if (!(flags & 0x10) && !(flags & 0x20)) loading = 0;
        if (loading == 0) {
            if (TheStoreMetadata.LoadingFailed()) {
                mState = kPreFail;
            } else {
                mState = kPreSuccess;
            }
        }
    }
    if (mState == kPreSuccess) {
        mState = kSuccess;
    } else if (mState == kPreFail) {
        mState = kFail;
    }
}
