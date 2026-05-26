#include "utl/NetCacheMgr.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/CommerceMgr_Wii.h"
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
const char *NetCacheMgr::GetServer() const { return Server().server; }
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

static Symbol local("local");

Symbol NetCacheMgr::CheatNextServer() {
    std::list<ServerData>::iterator s = mServers.begin();
    goto check;
    do {
        s++;
    check:
        bool keepGoing = false;
        if (s != mServers.end() && mServerType != s->type) keepGoing = true;
        if (!keepGoing) break;
    } while (true);
    MILO_ASSERT(s != mServers.end(), 0x22a);
    ++s;
    if (s == mServers.end()) {
        s = mServers.begin();
    }
    mServerType = s->type;
    if (UsingCD() && mServerType == local) {
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
        if (mState == kNCMS_UnloadWaitForWrite) {
            mHasFailed = false;
        }
        if (mState == kNCMS_Nil && state == kNCMS_UnloadWaitForWrite) {
            TheDebug.Fail(MakeString(
                "NetCacheMgr attempted to move straight from kNCMS_Nil to kNCMS_Unload!\n"
            ));
        }
        mState = state;
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
        case kNCMS_Nil:
            MILO_ASSERT(mNetLoaderRefs.empty(), 0x28A);
            if (mLoadCount > 0) {
                SetState(kNCMS_Load);
            }
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
    bool ready;
    if (*name == '\0') goto fail;
    ready = (mState == kNCMS_Ready && !mHasFailed && mLoadCount == 1);
    if (!ready) {
    fail:
        return NULL;
    }
    {
    NetLoaderRef *pNetLoaderRef = NULL;
    std::list<NetLoaderRef>::iterator it = mNetLoaderRefs.begin();
    for (; it != mNetLoaderRefs.end(); ++it) {
        NetLoaderRef &ref = *it;
        if (stricmp(ref.mName.c_str(), name) == 0) {
            if (kRT_CacheLoader == type && ref.mCacheLoader) {
                MILO_ASSERT(ref.mNetLoader == NULL, 0x17A);
                pNetLoaderRef = &ref;
                break;
            } else if (kRT_NetLoader == type && ref.mNetLoader) {
                MILO_ASSERT(ref.mCacheLoader == NULL, 0x180);
                pNetLoaderRef = &ref;
                break;
            } else {
                TheDebug << MakeString("Found loader for %s, but it was not type %d.\n", ref.mName.c_str(), type);
            }
        }
    }

    NetLoaderRef newRef;
    newRef.mRefCount = 0;
    newRef.mNetLoader = NULL;
    newRef.mCacheLoader = NULL;

    if (!pNetLoaderRef) {
        switch (type) {
        case kRT_CacheLoader: {
            NetCacheLoader *ncl = new NetCacheLoader(mCache, String(name));
            String s(name);
            NetLoaderRef tmp(s, 0, NULL, ncl);
            newRef = tmp;
            break;
        }
        case kRT_NetLoader: {
            NetLoader *nl = NetLoader::Create(String(name));
            String s(name);
            NetLoaderRef tmp(s, 0, nl, NULL);
            newRef = tmp;
            break;
        }
        default:
            MILO_FAIL("Unknown ref type %d.\n", type);
            break;
        }

        switch (pos) {
        case (NetLoaderPos)0:
            mNetLoaderRefs.insert(mNetLoaderRefs.begin(), newRef);
            pNetLoaderRef = &mNetLoaderRefs.front();
            break;
        case (NetLoaderPos)1: {
            std::list<NetLoaderRef>::iterator inserted = mNetLoaderRefs.insert(mNetLoaderRefs.end(), newRef);
            pNetLoaderRef = &*inserted;
            break;
        }
        default:
            MILO_FAIL("Unknown net loader pos %d.\n", pos);
            break;
        }

        MILO_ASSERT(pNetLoaderRef, 0x1C2);
    }
    pNetLoaderRef->AddRef();
    return pNetLoaderRef;
    }
}

void NetCacheMgr::PollLoaders() {
    bool firstDownload = true;
    NetLoaderRef *pDownloading = NULL;
    NetLoaderRef *pFirstToDownload = NULL;
    bool commerceBusy = (TheWiiCommerceMgr.mCommerceAsyncOpId != -1);
    std::list<NetLoaderRef>::iterator it = mNetLoaderRefs.begin();
    while (it != mNetLoaderRefs.end()) {
        NetLoaderRef *ref = &(*it);
        MILO_ASSERT(ref->IsValid(), 0xE9);
        if (ref->IsDownloading()) {
            pDownloading = ref;
            firstDownload = false;
        } else if (ref->NeedsToDownload()) {
            if (firstDownload) {
                pFirstToDownload = ref;
                firstDownload = false;
            }
        } else {
            ref->Poll();
        }
        ++it;
    }
    if (pDownloading) {
        pDownloading->Poll();
    } else if (pFirstToDownload && !commerceBusy) {
        pFirstToDownload->Poll();
    }
    it = mNetLoaderRefs.begin();
    while (it != mNetLoaderRefs.end()) {
        NetLoaderRef *ref = &(*it);
        if (ref->mRefCount < 1 && ref->IsSafeToDelete()) {
            ref->DeleteLoader();
            it = mNetLoaderRefs.erase(it);
        } else {
            ++it;
        }
    }
}

void NetLoaderRef::AddRef() { mRefCount++; }
void NetLoaderRef::ReleaseRef() { mRefCount--; }

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
        bool downloading = true;
        if (mCacheLoader) {
            int state = (int)mCacheLoader->mState;
            bool stateMatch = true;
            if (state != 1 && state != 2)
                stateMatch = false;
            if (!stateMatch)
                downloading = false;
        }
        if (!downloading)
            needsToDownload = false;
    }
    return needsToDownload;
}

bool NetLoaderRef::IsDownloading() {
    bool valid = (!mCacheLoader || !mNetLoader) && (mCacheLoader || mNetLoader);
    MILO_ASSERT(valid, 0x324);
    bool isDownloading = true;
    if (!mNetLoader) {
        bool isState2 = mCacheLoader && (int)mCacheLoader->mState == 2;
        if (!isState2)
            isDownloading = false;
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

