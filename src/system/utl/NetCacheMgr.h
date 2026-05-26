#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Dir.h"
#include "os/FileCache.h"
#include "utl/Symbols.h"
#include "utl/Cache.h"
#include "utl/NetLoader.h"
#include "utl/NetCacheLoader.h"
#include "utl/Str.h"
#include <list>

class NetCacheLoader;

enum NetCacheMgrFailType {
    kNCMFT_Unknown,
    kNCMFT_StoreServer,
    kNCMFT_NoSpace,
    kNCMFT_StorageDeviceMissing,
    kNCMFT_Max
};

enum NetCacheMgrState {
    kNCMS_Load,
    kNCMS_Ready,
    kNCMS_UnloadWaitForWrite,
    kNCMS_UnloadUnmount,
    kNCMS_Failure,
    kNCMS_Max,
    kNCMS_Nil = -1
};

enum LoadState {
    kLS_None,
    kLS_Mount,
    kLS_Delete,
    kLS_ReMount,
    kLS_Resync
};

struct NetLoaderRef {
    NetLoaderRef() {}
    NetLoaderRef(const String& name, int refCount, NetLoader* nl, NetCacheLoader* cl)
        : mName(name), mRefCount(refCount), mNetLoader(nl), mCacheLoader(cl) {}
    void Poll();
    bool NeedsToDownload();
    bool IsDownloading();
    bool IsLoadedOrFailed();
    bool IsSafeToDelete();
    void DeleteLoader();
    void AddRef();
    void ReleaseRef();
    bool IsValid() const {
        return (!mCacheLoader || !mNetLoader) && (mCacheLoader || mNetLoader);
    }
    inline NetLoaderRef &operator=(const NetLoaderRef &other) {
        mName = other.mName;
        mRefCount = other.mRefCount;
        mNetLoader = other.mNetLoader;
        mCacheLoader = other.mCacheLoader;
        return *this;
    }

    String mName; // 0x0
    int mRefCount; // 0xc
    NetLoader *mNetLoader; // 0x10
    NetCacheLoader *mCacheLoader; // 0x14
};

class NetCacheMgr : public Hmx::Object {
public:
    struct ServerData {
        Symbol type; // 0x0
        bool local; // 0x4
        const char *server; // 0x8
        unsigned short port; // 0xc
        const char *root; // 0x10
        bool verifySSL; // 0x14
    };

    enum RefType {
        kRT_CacheLoader = 0,
        kRT_NetLoader = 1,
    };

    enum CacheSize {
    };

    NetCacheMgr();
    virtual ~NetCacheMgr();
    virtual DataNode Handle(DataArray *, bool);
    virtual void Poll();
    virtual void LoadInit();
    virtual bool IsDoneLoading() const;
    virtual void ReadyInit();
    virtual void UnloadInit();
    virtual bool IsDoneUnloading() const;

    unsigned short GetPort() const;
    const char *GetServerRoot() const;
    const char *GetServer() const;
    bool IsServerLocal() const;
    NetCacheMgrFailType GetFailType() const;
    void SetState(NetCacheMgrState);
    void Unload();
    bool IsLocalFile(const char *) const;
    void OnInit(DataArray *);
    Symbol CheatNextServer();
    void DebugClearCache();
    void DeleteNetCacheLoader(NetCacheLoader *);
    void DeleteNetLoader(NetLoader *);
    void Load(CacheSize);
    bool IsUnloaded() const;
    bool IsReady() const;
    NetCacheLoader *AddNetCacheLoader(const char *, NetLoaderPos);
    NetLoader *AddNetLoader(const char *, NetLoaderPos);

    bool GetHasFailed() const { return mHasFailed; }
    bool UseSSL();

private:
    void EnterLoadState();
    bool IsUnloadStateDone() const;
    void EnterUnloadState();
    NetCacheMgr::ServerData const &Server() const;

protected:
    void SetFail(NetCacheMgrFailType);
    void PollLoaders();
    NetLoaderRef *AddLoaderRef(const char *, RefType, NetLoaderPos);

    NetCacheMgrState mState; // 0x1c
    bool mHasFailed; // 0x20
    NetCacheMgrFailType mFailType; // 0x24
    String mStrXLSPFilter; // 0x28
    int mServiceId; // 0x34
    std::list<ServerData> mServers; // 0x38
    Symbol mServerType; // 0x40
    unsigned int mLoadCacheSize; // 0x44
    FileCache *mCache; // 0x48
    std::list<NetLoaderRef> mNetLoaderRefs; // 0x4c
    int mLoadCount; // 0x54
};

class NetCacheMgrWii : public NetCacheMgr {
public:
    virtual ~NetCacheMgrWii();
};

extern NetCacheMgr *TheNetCacheMgr;
void NetCacheMgrInit();
void NetCacheMgrTerminate();
