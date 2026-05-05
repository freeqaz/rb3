#include "utl/NetCacheMgr.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/FileCache.h"
#include "os/System.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/NetCacheLoader.h"
#include "utl/NetLoader.h"
#include "utl/Std.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "utl/Symbols4.h"

NetCacheMgr *TheNetCacheMgr = 0;

void NetCacheMgrInit() {
    MILO_ASSERT(TheNetCacheMgr == NULL, 0x22);
    TheNetCacheMgr = new NetCacheMgrWii();
}

NetCacheMgrWii::~NetCacheMgrWii() {}

void NetCacheMgrTerminate() {
    delete TheNetCacheMgr;
    TheNetCacheMgr = NULL;
}

NetCacheMgr::NetCacheMgr()
    : mState(kNCMS_Nil), mHasFailed(0), mFailType(kNCMFT_Unknown), mServiceId(0),
      mLoadCacheSize(0), mCache(0), mLoadCount(0) {
    SetName("net_cache_mgr", ObjectDir::sMainDir);
}

NetCacheMgr::~NetCacheMgr() {}

void NetCacheMgr::Unload() {
    mLoadCount--;
    if (mLoadCount < 0) {
        MILO_WARN("NetCacheMgr::Unload() called more times than NetCacheMgr::Load()!\n");
        mLoadCount = 0;
    } else {
        SetState(kNCMS_UnloadWaitForWrite);
    }
}

bool NetCacheMgr::IsDoneLoading() const { return 1; }

bool NetCacheMgr::IsDoneUnloading() const { return 1; }

void NetCacheMgr::LoadInit() { return; }

void NetCacheMgr::ReadyInit() { return; }

void NetCacheMgr::UnloadInit() { return; }

bool NetCacheMgr::IsUnloaded() const { return mState != kNCMS_UnloadWaitForWrite; }
bool NetCacheMgr::IsReady() const { return (mState == kNCMS_Ready && !mHasFailed && mLoadCount == 1); }
NetCacheMgrFailType NetCacheMgr::GetFailType() const { return mFailType; }

void NetCacheMgr::OnInit(DataArray *pData) {
    MILO_ASSERT(pData, 0x48);
    mServiceId = pData->FindArray(xlsp_service_id)->Int(1);
    mStrXLSPFilter = pData->FindStr(xlsp_filter);
    DataArray *serverArr = pData->FindArray(servers);
    MILO_ASSERT(mServers.empty(), 0x58);
    for (int i = 1; i < serverArr->Size(); i++) {
        ServerData serverData;
        DataArray *curArr = serverArr->Array(i);
        serverData.type = curArr->Sym(0);
        serverData.server = gNullStr;
        bool verifySSL = true;
        curArr->FindData(verify_ssl, verifySSL, false);
        serverData.verifySSL = verifySSL;
        bool isLocal = false;
        curArr->FindData(local, isLocal, false);
        serverData.local = isLocal;
        const char *serverStr = nullptr;
        curArr->FindData(server, serverStr, false);
        serverData.server = serverStr;
        int serverPort = 0;
        curArr->FindData(port, serverPort, false);
        serverData.port = (unsigned short)serverPort;
        serverData.root = curArr->FindStr(root);
        mServers.push_back(serverData);
    }
    mServerType = pData->FindArray(default_server)->Sym(1);
    FOREACH (it, mServers) {
        // ok then
    }
}

BEGIN_HANDLERS(NetCacheMgr)
    HANDLE_ACTION(init, OnInit(_msg->Array(2)))
    HANDLE_ACTION(debug_clear_cache, DebugClearCache())
    HANDLE_EXPR(cheat_next_server, CheatNextServer())
    HANDLE_EXPR(server_type, mServerType)
    HANDLE_CHECK(0x2f3)
END_HANDLERS

const NetCacheMgr::ServerData &NetCacheMgr::Server() const {
    std::list<ServerData>::const_iterator s = mServers.begin();
    for (; s != mServers.end() && s->type != mServerType; s++)
        ;
    MILO_ASSERT(s != mServers.end(), 0x2D7);
    return *s;
}

unsigned short NetCacheMgr::GetPort() const { return Server().port; }
const char *NetCacheMgr::GetServerRoot() const { return Server().root; }
bool NetCacheMgr::IsServerLocal() const { return Server().local; }

void NetCacheMgr::Poll() {
    PollLoaders();
    switch (mState) {
    case kNCMS_Load:
        if (IsDoneLoading()) {
            SetState(kNCMS_Ready);
        }
        break;
    case kNCMS_UnloadWaitForWrite:
        if (IsUnloadStateDone()) {
            SetState(kNCMS_Nil);
        }
        break;
    default:
        break;
    }
}

bool NetCacheMgr::IsLocalFile(const char *file) const {
    bool ready = (mState == kNCMS_Ready && !mHasFailed && mLoadCount == 1);
    if (!ready)
        return false;
    return mCache->FileCached(file);
}

void NetCacheMgr::SetFail(NetCacheMgrFailType n) {
    mHasFailed = true;
    mFailType = n;
}

void NetCacheMgr::EnterLoadState() {
    mHasFailed = false;
    LoadInit();
    if (!mHasFailed) {
        MILO_ASSERT(!mCache, 0x2aa);
        MILO_ASSERT(mLoadCacheSize, 0x2ab);
        mCache = new FileCache(mLoadCacheSize, kLoadStayBack, true);
        mLoadCacheSize = 0;
    }
}

void NetCacheMgr::EnterUnloadState() {
    UnloadInit();
    FOREACH (it, mNetLoaderRefs) {
        NetLoaderRef &cur = *it;
        if (cur.mRefCount > 0) {
            MILO_WARN(
                "Loader for %s has %d reference(s) left unaccounted for!",
                cur.mName,
                cur.mRefCount
            );
            cur.mRefCount = 0;
        }
    }
    delete mCache;
    mCache = 0;
}

bool NetCacheMgr::IsUnloadStateDone() const {
    for (std::list<NetLoaderRef>::const_iterator it = mNetLoaderRefs.begin();
         it != mNetLoaderRefs.end(); ++it) {
        const NetLoaderRef &ref = *it;
        if (ref.mCacheLoader && ref.mCacheLoader->mState == NetCacheLoader::kS_0x2) {
            MILO_LOG("NetCacheMgr::IsUnloadStateDone: %s still busy\n", ref.mName.c_str());
            ref.mCacheLoader->Poll();
            return false;
        }
    }
    return IsDoneUnloading() && mNetLoaderRefs.empty();
}

void NetCacheMgr::DeleteNetLoader(NetLoader *nl) {
    if (nl) {
        FOREACH (it, mNetLoaderRefs) {
            NetLoaderRef &ref = *it;
            if (ref.mNetLoader == nl) {
                ref.ReleaseRef();
                return;
            }
        }
    }
}

void NetCacheMgr::DeleteNetCacheLoader(NetCacheLoader *ncl) {
    if (ncl) {
        FOREACH (it, mNetLoaderRefs) {
            NetLoaderRef &ref = *it;
            if (ref.mCacheLoader == ncl) {
                ref.ReleaseRef();
                return;
            }
        }
    }
}

Symbol NetCacheMgr::CheatNextServer() {
    std::list<ServerData>::iterator s = mServers.begin();
    for (; s != mServers.end() && s->type != mServerType; s++)
        ;
    MILO_ASSERT(s != mServers.end(), 0x22a);
    std::list<ServerData>::iterator _tmp2 = mServers.end();
    if (s == _tmp2) {
        s = mServers.begin();
    }
    mServerType = s->type;
    static Symbol local_sym("local");
    if (UsingCD() && mServerType == local_sym) {
        CheatNextServer();
    }
    return mServerType;
}

void NetCacheMgr::Load(NetCacheMgr::CacheSize cs) {
    mLoadCount++;
    MILO_ASSERT(mLoadCount <= 2, 0x120);
    if (mState == kNCMS_Load && !mHasFailed) {
        MILO_WARN("NetCcaheMgr::Load() called before previous load had finished.");
    }
    mLoadCacheSize = cs == (CacheSize)0 ? 0x100000 : 0x500000;
    if (mLoadCount == 1 && mState == kNCMS_Nil) {
        SetState(kNCMS_Load);
    }
}

void NetCacheMgr::SetState(NetCacheMgrState state) {
    if (mState != state) {
        while (true) {
            if (mState == kNCMS_UnloadWaitForWrite) {
                mHasFailed = false;
            }
            if (mState == kNCMS_Nil && state == kNCMS_UnloadWaitForWrite) {
                MILO_FAIL(
                    "NetCacheMgr attempted to move straight from kNCMS_Nil to kNCMS_Unload!\n"
                );
            }
            mState = state;
            if (state != kNCMS_Nil)
                break;
            MILO_ASSERT(mNetLoaderRefs.empty(), 0x28B);
            if (mLoadCount <= 0)
                return;
            state = kNCMS_Load;
            if (mState == kNCMS_Load)
                return;
        }
        switch (state) {
        case kNCMS_Load:
            EnterLoadState();
            break;
        case kNCMS_Ready:
            ReadyInit();
            break;
        case kNCMS_UnloadWaitForWrite:
            EnterUnloadState();
            break;
        default:
            break;
        }
    }
}

void NetCacheMgr::DebugClearCache() {
    bool ready = (mState == kNCMS_Ready && !mHasFailed && mLoadCount == 1);
    if (ready) {
        mCache->Clear();
    }
}

NetLoader *NetCacheMgr::AddNetLoader(const char *cc, NetLoaderPos pos) {
    NetLoaderRef *pNetLoaderRef = AddLoaderRef(cc, kRT_NetLoader, pos);
    if (pNetLoaderRef && pNetLoaderRef->mNetLoader)
        return pNetLoaderRef->mNetLoader;
    return nullptr;
}

NetCacheLoader *NetCacheMgr::AddNetCacheLoader(const char *cc, NetLoaderPos pos) {
    NetLoaderRef *pNetLoaderRef = AddLoaderRef(cc, kRT_CacheLoader, pos);
    if (pNetLoaderRef && pNetLoaderRef->mCacheLoader)
        return pNetLoaderRef->mCacheLoader;
    return nullptr;
}


NetLoaderRef *NetCacheMgr::AddLoaderRef(const char *name, RefType type, NetLoaderPos pos) {
    NetLoaderRef *pNetLoaderRef = NULL;
    if (*name == '\0') {
        return NULL;
    }
    if (!IsReady()) {
        return NULL;
    }
    std::list<NetLoaderRef>::iterator it = mNetLoaderRefs.begin();
    for (; it != mNetLoaderRefs.end(); ++it) {
        NetLoaderRef &ref = *it;
        if (stricmp(ref.mName.c_str(), name) == 0) {
            if (kRT_CacheLoader == type && ref.mCacheLoader) {
                MILO_ASSERT(ref.mNetLoader == NULL, 0x17A);
            } else if (kRT_NetLoader == type && ref.mNetLoader) {
                MILO_ASSERT(ref.mCacheLoader == NULL, 0x180);
            } else {
                TheDebug << MakeString("Found loader for %s, but it was not type %d.\n", ref.mName.c_str(), (int)type);
                continue;
            }
            pNetLoaderRef = &ref;
            break;
        }
    }

    NetLoaderRef newRef;
    newRef.mRefCount = 0;
    newRef.mNetLoader = NULL;
    newRef.mCacheLoader = NULL;

    if (!pNetLoaderRef) {
        if ((unsigned int)type == 1) {
            NetLoader *nl = NetLoader::Create(String(name));
            String s(name);
            NetLoaderRef tmp = { String(s), 0, nl, NULL };
            newRef = tmp;
        } else if ((unsigned int)type == 0) {
            NetCacheLoader *ncl = new NetCacheLoader(mCache, String(name));
            String s(name);
            NetLoaderRef tmp = { String(s), 0, NULL, ncl };
            newRef = tmp;
        } else {
            MILO_FAIL("Unknown ref type %d.\n", type);
        }

        if ((unsigned int)pos == 1) {
            mNetLoaderRefs.insert(mNetLoaderRefs.end(), newRef);
            pNetLoaderRef = &mNetLoaderRefs.back();
        } else if ((unsigned int)pos == 0) {
            std::list<NetLoaderRef>::iterator insertIt;
            for (insertIt = mNetLoaderRefs.begin(); insertIt != mNetLoaderRefs.end(); ++insertIt) {
                if (!insertIt->IsDownloading() && !insertIt->IsLoadedOrFailed()) {
                    break;
                }
            }
            std::list<NetLoaderRef>::iterator inserted = mNetLoaderRefs.insert(insertIt, newRef);
            pNetLoaderRef = &*inserted;
        } else {
            MILO_FAIL("Unknown net loader pos %d.\n", pos);
        }

        MILO_ASSERT(pNetLoaderRef, 0x1C2);
    }
    pNetLoaderRef->AddRef();
    return pNetLoaderRef;
}

void NetCacheMgr::PollLoaders() {
    bool firstDownload = true;
    std::list<NetLoaderRef>::iterator it = mNetLoaderRefs.begin();
    while (it != mNetLoaderRefs.end()) {
        MILO_ASSERT(it->IsValid(), 0xE9);
        if (!it->NeedsToDownload() || it->IsLoadedOrFailed()) {
            it->Poll();
        } else if (firstDownload) {
            it->Poll();
            firstDownload = false;
        }
        if (it->mRefCount < 1 && it->IsSafeToDelete()) {
            it->DeleteLoader();
            it = mNetLoaderRefs.erase(it);
        } else {
            ++it;
        }
    }
}

void NetLoaderRef::AddRef() { mRefCount++; }
void NetLoaderRef::ReleaseRef() { mRefCount--; }

bool NetLoaderRef::IsValid() const {
    // mCacheLoader XOR mNetLoader
    return (!mCacheLoader || !mNetLoader) && (mCacheLoader || mNetLoader);
}

void NetLoaderRef::Poll() {
    bool valid = (!mCacheLoader || !mNetLoader) && (mCacheLoader || mNetLoader);
    MILO_ASSERT(valid, 0x32a);
    if (mCacheLoader) {
        mCacheLoader->Poll();
    } else {
        mNetLoader->PollLoading();
    }
}

bool NetLoaderRef::IsSafeToDelete() {
    bool valid = (!mCacheLoader || !mNetLoader) && (mCacheLoader || mNetLoader);
    MILO_ASSERT(valid, 0x33d);
    if (mCacheLoader) {
        return mCacheLoader->IsSafeToDelete();
    } else {
        return mNetLoader->IsSafeToDelete();
    }
}

void NetLoaderRef::DeleteLoader() {
    bool safeToDelete = IsSafeToDelete();
    MILO_ASSERT(safeToDelete, 0x343);
    delete mCacheLoader;
    mCacheLoader = 0;
    delete mNetLoader;
    mNetLoader = 0;
}

bool NetLoaderRef::NeedsToDownload() {
    bool valid = (!mCacheLoader || !mNetLoader) && (mCacheLoader || mNetLoader);
    MILO_ASSERT(valid, 0x31e);
    bool needsToDownload = true;
    if (!mNetLoader) {
        needsToDownload = true;
        if (mCacheLoader) {
            int state = (int)mCacheLoader->mState;
            bool stateMatch = (state == 1 || state == 2);
            if (!stateMatch)
                needsToDownload = false;
        }
    }
    return needsToDownload;
}

bool NetLoaderRef::IsDownloading() {
    bool valid = (!mCacheLoader || !mNetLoader) && (mCacheLoader || mNetLoader);
    MILO_ASSERT(valid, 0x324);
    bool isDownloading = true;
    if (!mNetLoader) {
        isDownloading = false;
        if (mCacheLoader && (int)mCacheLoader->mState == 2)
            isDownloading = true;
    }
    return isDownloading;
}

bool NetLoaderRef::IsLoadedOrFailed() {
    bool valid = (!mCacheLoader || !mNetLoader) && (mCacheLoader || mNetLoader);
    MILO_ASSERT(valid, 0x327);

    if (!mCacheLoader) {
        if (mNetLoader) {
            return true;
        }
        return false;
    }

    if (!mNetLoader) {
        return true;
    }

    bool loaded = mCacheLoader->IsLoaded();
    if (loaded) {
        return true;
    }

    char failed = mCacheLoader->HasFailed();
    return failed != '\0';
}

NetLoaderRef &NetLoaderRef::operator=(const NetLoaderRef &other) {
    mName = other.mName;
    mRefCount = other.mRefCount;
    mNetLoader = other.mNetLoader;
    mCacheLoader = other.mCacheLoader;
    return *this;
}
