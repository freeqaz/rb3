#include "HttpWii.h"
#include "system/os/Debug.h"
#include "system/os/File.h"
#include "system/os/System.h"
#include "system/utl/MemMgr.h"
#include "system/utl/Symbol.h"
#include "system/utl/Symbols2.h"
#include <RevoEX/nhttp/nhttp.h>

HttpWii TheHttpWii;

void *CommerceEcAlloc(int sz, int al) {
    void *r = NULL;
    if (sz != 0)
        r = _MemAlloc(sz, al);
    return r;
}

void *CommerceEcFree(void *ptr) {
    if (ptr == NULL)
        return;
    _MemFree(ptr);
}

const char *HttpWii::GetNHTTPErrorString(NHTTPError err) {
    if (err == NHTTP_ERROR_SYSTEM) {
        return "NHTTP_ERROR_SYSTEM";
    } else if ((unsigned)err < NHTTP_NUM_ERRORS) {
        static const char *table[NHTTP_NUM_ERRORS] = { "NHTTP_ERROR_NONE",
                                                       "NHTTP_ERROR_ALLOC",
                                                       "NHTTP_ERROR_TOOMANYREQ",
                                                       "NHTTP_ERROR_SOCKET",
                                                       "NHTTP_ERROR_DNS",
                                                       "NHTTP_ERROR_CONNECT",
                                                       "NHTTP_ERROR_BUFFULL",
                                                       "NHTTP_ERROR_HTTPPARSE",
                                                       "NHTTP_ERROR_CANCELED",
                                                       "NHTTP_ERROR_REVOLUTIONSDK",
                                                       "NHTTP_ERROR_REVOLUTIONWIFI",
                                                       "NHTTP_ERROR_UNKNOWN",
                                                       "NHTTP_ERROR_DNS_PROXY",
                                                       "NHTTP_ERROR_CONNECT_PROXY",
                                                       "NHTTP_ERROR_SSL",
                                                       "NHTTP_ERROR_BUSY" };
        return table[err];
    } else {
        MILO_FAIL("unknown NHTTPError! (added recently?)");
        return "unknown (internal error)";
    }
}

HttpWii::HttpWii() {}
HttpWii::~HttpWii() {}

void HttpWii::Init() {
    mDataBuffer = NULL;
    Symbol serverName;
    DataArray *arr =
        SystemConfig(Symbol("store"))->FindArray(Symbol("netcache_init"), false);
    if (arr) {
        arr->FindData(default_server, serverName, true);
        mServerInfo = SystemConfig(
            Symbol("store"), Symbol("netcache_init"), Symbol("servers"), serverName
        );
        mUseFileLoad = false;
        mUseSSL = true;
        String keyPath;
        mServerInfo->FindData(Symbol("key_path2"), keyPath, false);
        mServerInfo->FindData(Symbol("verify_ssl"), mUseSSL, false);
        if (mUseSSL) {
            mCertSize = 0;
            File *file = NewFile(keyPath.c_str(), 2);
            if (file) {
                mCertSize = file->Size();
                if (mCertSize) {
                    mRootCA = (char *)_MemAlloc(mCertSize, 0x20);
                    file->Read(mRootCA, file->Size());
                }
                delete file;
            }
        }
        mStatus = 0;
    }
}

void HttpWii::Start() {
    if (mStatus != 1) {
        NHTTPStartup(CommerceEcAlloc, CommerceEcFree, 0x11);
        mStatus = 1;
    }
}

void HttpWii::CleanupCallback() { TheHttpWii.mStatus = 0; }

bool gWaitingOnCancelToComplete;

void HttpWii::Stop() {
    if (mStatus == 1) {
        CancelAsyncAll();
        mStatus = 2;
        gWaitingOnCancelToComplete = false;
    }
}

int HttpWii::GetStatus() {
    if (mStatus == 2) {
        for (int i = 0; i < NUM_HTTPWII_HANDLES; i++) {
            if (mHandles[i] != NULL) {
                unsigned long r = 0;
                if (CompleteAsync(i, r) == 0)
                    return mStatus;
            }
        }
        if (gWaitingOnCancelToComplete == false) {
            NHTTPCleanupAsync(CleanupCallback);
            gWaitingOnCancelToComplete = true;
        }
    }
    return mStatus;
}

int HttpWii::CancelAsync(int task) {
    BOOL r;
    if (mHandles[task] == NULL) {
        r = false;
    } else {
        r = (NHTTPCancelConnection(mHandles[task]) == 0);
    }
    return r;
}

int HttpWii::CancelAsyncAll() {
    for (int i = 0; i < NUM_HTTPWII_HANDLES; i++) {
        CancelAsync(i);
    }
    return TRUE;
}
