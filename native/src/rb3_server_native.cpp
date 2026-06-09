#ifdef HX_NATIVE
// rb3_server_native.cpp — faithful native OFFLINE online-server (TheServer).
//
// `extern Server &TheServer;` (network/net/Server.h) is bound on Wii to
// `gWiiServer` in the platform-EXCLUDED Server_Wii.cpp; natively it was only a
// weak ZEROED data stub (band3_link_stubs.s `.weak TheServer`). Any virtual call
// on it (TheServer.GetPlayerID / IsConnected / ...) derefs a null vtable ptr ->
// SIGSEGV at (nil). This is latent today because the offline paths that touch
// TheServer are gated behind "is there a (primary) profile?" — but the guest-
// profile hack (rb3_guestprofile_native.cpp / roadmap C11) makes a primary
// profile exist, so MainHubPanel::ReloadMessages reaches
// `TheServer.GetPlayerID(pad)` and crashes. The same landmine sits on the
// gameplay scoring path (MetaPerformer) once a profile is present.
//
// FAITHFUL OFFLINE BEHAVIOR: with no online login, GetPlayerID() returns 0 (no
// online player id) and IsConnected()/IsLoggingIn() are false — so the ticker /
// leaderboard / online-score paths short-circuit exactly as they do on a real
// console that isn't signed into online play. We provide a minimal concrete
// Server whose vtable is real.
//
// LINKING: Server.cpp (the base ctor + the non-inline base virtuals Handle/Init)
// is not compiled natively. Defining Server::Handle here (Server's KEY function —
// its first non-inline virtual) emits `vtable for Server` in this TU, which
// resolves the reference the base ctor needs; the pure-virtual slots fill with
// __cxa_pure_virtual (never reached — the concrete subclass overrides them).
// Strong defs win over the weak band3_link_stubs.s `TheServer` data stub.
#include "network/net/Server.h"
#include "obj/Data.h"

Server::Server() : mLoginState(0) {}
DataNode Server::Handle(DataArray *, bool) { return DataNode(0); }
void Server::Init() {}

namespace {
// Minimal offline server: not connected, no online player ids.
class NativeOfflineServer : public Server {
public:
    NativeOfflineServer() : Server() {}
    virtual ~NativeOfflineServer() {}
    virtual void Poll() {}
    virtual void Login() {}
    virtual void Logout() {}
    virtual int GetPlayerID(int) { return 0; }
};
NativeOfflineServer gNativeOfflineServer;
} // namespace

Server &TheServer = gNativeOfflineServer;

#endif // HX_NATIVE
