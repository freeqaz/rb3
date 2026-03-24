#include "NetCacheMgr.h"
#include "os/Debug.h"
#include "utl/Std.h"
#include "utl/Str.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "utl/Symbols4.h"

// this is actually a NetCacheMgrWii so change this when that is implemented
NetCacheMgr *TheNetCacheMgr = 0;

NetCacheMgr::NetCacheMgr() { SetName("net_cache_mgr", ObjectDir::sMainDir); }

void NetCacheMgr::Unload() {
    // impl
}

bool NetCacheMgr::IsDoneLoading() const { return 1; }

bool NetCacheMgr::IsDoneUnloading() const { return 1; }

void NetCacheMgr::LoadInit() { return; }

void NetCacheMgr::ReadyInit() { return; }

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

#pragma push
#pragma dont_inline on
BEGIN_HANDLERS(NetCacheMgr)
    HANDLE_ACTION(init, OnInit(_msg->Array(2)))
    HANDLE_ACTION(debug_clear_cache, DebugClearCache())
    HANDLE_ACTION(cheat_next_server, CheatNextServer())
    HANDLE_EXPR(server_type, mServerType);
    HANDLE_CHECK(0x2f3)
END_HANDLERS
#pragma pop

void NetCacheMgrInit() {
    MILO_ASSERT(TheNetCacheMgr == NULL, 0x22);
    TheNetCacheMgr = new NetCacheMgr();
}

void NetCacheMgrTerminate() {
    delete TheNetCacheMgr;
    TheNetCacheMgr = NULL;
}