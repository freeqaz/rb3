// rb3_netsession_native.cpp — native TheNetSession (the `session` DTA object).
//
// PROBLEM: `NetSession *TheNetSession;` and the concrete session impl
// (NetSession_RV / NetSession::New) live in src/network/, which is NOT globbed
// onto the rb3-native link (Quazal/DWC online stack — Nintendo WFC is dead and
// the port is offline single-player for now). So TheNetSession is null, and the
// pervasive `TheNetSession->AddSink(this)` / `IsLocal()` derefs across ~28
// compiled TUs (LockStepMgr ctor, BandUI/NetSync/SessionMgr Init, many panels)
// fault. NetSession is abstract (10 pure virtuals), so it can't be `new`ed
// directly, and per-call-site gating would be whack-a-mole.
//
// FAITHFUL FIX (construct-the-real-global; DTA_MANAGER_STUBS.md §4 outcome): a
// minimal concrete native subclass whose ctor builds the real MsgSource base
// (so AddSink/RemoveSink walk a valid empty sink list) and an idle, offline
// session state. We deliberately skip the Quazal/JobMgr-online init the real
// NetSession ctor does. The DTA `session` messages the boot path needs answer
// offline-correctly: `is_local`→true (idle + online-disabled, exactly
// NetSession::IsLocal's real semantics), `is_in_game`/`is_busy`→false,
// `num_users`→0. We provide STRONG native defs of the otherwise weak-stubbed
// non-virtual NetSession query methods so those return the right offline values.
#ifdef HX_NATIVE

#include "net/NetSession.h"
#include "obj/Dir.h"
#include "utl/Symbols.h"
#include "game/BandUser.h" // AddUserResultMsg
#include "meta_band/BandNetGameData.h"
#include "os/User.h"
#include <algorithm>

// --- strong native NetSession base ctor/dtor ---
// CRITICAL: the real NetSession::NetSession() (network/net/NetSession.cpp:89) is
// not compiled, so it's a weak no-op stub. A derived ctor that called that stub
// would leave the MsgSource base (mSinks) UNCONSTRUCTED — re-introducing the
// AddSink fault. Defining the base ctor here (strong, wins over the weak stub)
// makes the compiler emit the MsgSource + member base ctor calls, so mSinks is a
// valid empty list. We init only the offline-meaningful scalars and skip the
// Quazal/SessionSettings/online init the Wii ctor does. mJobMgr (value member,
// JobMgr(Hmx::Object*)) is constructed via the mem-init list.
NetSession::NetSession()
    : mData(0), mUsers(), mLocalHost(0), mJoinData(0), mSettings(0), mJobMgr(this),
      mCurrentStateJobID(-1), mGameState(kInLobby), mRevertingJoinResult(0),
      mStillArbitrating(), mGameStartTime(0), mGameStartDelay(0), mState(kIdle),
      mOnlineEnabled(false), mQNet(0) {}

NetSession::~NetSession() { TheNetSession = nullptr; }

// Define NetSession's non-pure, out-of-line virtuals here so clang emits the REAL
// NetSession typeinfo (_ZTI10NetSession) + vtable instead of leaving only the zeroed
// DATA stub. WHY THIS MATTERS: BandUI::Init iterates ALL of sMainDir with
// ObjDirItr<UIScreen>, dynamic_cast<UIScreen*>'ing every object — including the
// registered `session` object (RB3NativeNetSession). That cast walks the
// RB3NativeNetSession -> NetSession -> MsgSource RTTI chain; with a zeroed
// _ZTI10NetSession the __si_class_type_info walk derefs garbage and crashes. The
// first non-inline non-pure virtual (Handle) is NetSession's key function, so
// defining it here forces the real RTTI/vtable emission. These bodies are offline
// no-ops (the real online ones live in the un-globbed network/ NetSession.cpp).
DataNode NetSession::Handle(DataArray *msg, bool warn) {
    return MsgSource::Handle(msg, warn);
}
void NetSession::Poll() {}

// NetSession::AddLocalUser — the non-virtual entry SessionMgr::AddLocalUserImpl
// calls (mSession->AddLocalUser). Its real body lives in the un-globbed
// network/net/NetSession.cpp, so on native it was a weak no-op → the local-user
// join never completed → SessionMgr never fired AddLocalUserResultMsg → the
// overshell slot never reached an input-allowing "joined" state → the splash
// kSplashScreen_WaitOvershell gate (overshell_allowing_input TRUE) never fired
// → splash never advanced to main_hub. Mirror the real IsHost() path: add the
// user to the session's local list and fire AddUserResultMsg(success). Offline
// is always single local host (IsHost()==true), so the request/Quazal branch is
// not needed.
void NetSession::AddLocalUser(LocalUser *newUser) {
    MILO_ASSERT(newUser, 0x2FA);
    if (std::find(mUsers.begin(), mUsers.end(), newUser) == mUsers.end())
        mUsers.push_back(newUser);
    static AddUserResultMsg successMsg(1);
    Handle(successMsg, false);
}

void NetSession::AddLocalToSession(LocalUser *) {}
void NetSession::AddRemoteToSession(RemoteUser *) {}
void NetSession::RemoveLocalFromSession(LocalUser *) {}
void NetSession::RemoveRemoteFromSession(RemoteUser *) {}

// NetSession::HasUser — real body is in the un-globbed network/net/NetSession.cpp
// (a weak no-op stub otherwise → always false). Mirror it exactly: the session
// has a user iff it is in mUsers (which AddLocalUser populates). Needed so the
// quickplay song-select flow's NetSync::SyncScreen leader-user gate
// (u->IsLocal() && TheNetSession->HasUser(u)) passes — otherwise the
// song_select_enter_screen transition is silently BLOCKED offline.
bool NetSession::HasUser(const User *user) const {
    MILO_ASSERT(user, 0x470);
    return std::find(mUsers.begin(), mUsers.end(), user) != mUsers.end();
}

// V3 — NetSession::StartGame native impl. The real impl lives in the un-globbed
// network/net/NetSession.cpp (weak no-op stub otherwise). It drives the
// kInLobby -> kStartingGame -> (NetSession::Poll watches the start-time clock) ->
// EnterInGameState() -> SyncStartGameMsg flow. On offline single-player there is
// no Quazal session clock — IsLocal() short-circuits the timer (delay=0) — but
// NetSession::Poll is also stubbed here so EnterInGameState never fires and the
// SyncStartGameMsg the SyncGameStartPanel waits on (mState 4 -> 5) is never
// delivered. The downstream effect is the part_difficulty -> tv3_* -> game_screen
// kTransitionTo stalls forever on game_screen->CheckIsLoaded() (the
// sync_audio_net_panel never reaches mState==5), so GamePanel::Poll never runs,
// Game::Go() never fires, and MasterAudio::Play() never flips StandardStream to
// kPlaying — audio never plays. Mirror the offline path of the real StartGame:
// transition to kStartingGame, then synchronously call EnterInGameState() the way
// the real NetSession::Poll would (IsLocal()->delay=0->mGameStartTime stays
// nullptr->b1 true on the next Poll). EnterInGameState sets mGameState =
// kInLocalGame (the !mOnlineEnabled branch) and Handle()s the SyncStartGameMsg —
// SyncGameStartPanel::OnMsg sees it and sets mState = 5, unblocking
// game_screen->CheckIsLoaded().
void NetSession::StartGame() {
    if (mGameState != kInLobby) return; // already started — idempotent.
    mGameState = kStartingGame;
    EnterInGameState();
}

// NetSession::EnterInGameState real body lives in un-globbed network/net/NetSession.cpp
// (weak no-op stub otherwise). Mirror the offline (!mOnlineEnabled) branch.
void NetSession::EnterInGameState() {
    mGameState = kInLocalGame;
    static SyncStartGameMsg start;
    Handle(start, false);
}

// --- offline query methods (otherwise weak-stubbed → 0) ---
// Mirror NetSession::IsLocal's real logic: local iff idle and not online.
bool NetSession::IsLocal() const { return mState == kIdle && !mOnlineEnabled; }
bool NetSession::IsInGame() const { return false; }
bool NetSession::IsBusy() const { return false; }
bool NetSession::IsHost() const { return true; }      // single local machine
bool NetSession::IsOnlineEnabled() const { return false; }
bool NetSession::IsJoining() const { return false; }
bool NetSession::IsStartingGame() const { return false; }

namespace {
// Minimal concrete NetSession: implement the pure virtuals as offline no-ops so
// the class is instantiable. None is reached on the offline boot path (they are
// the host/join/arbitration online state-machine entry points).
class RB3NativeNetSession : public NetSession {
public:
    RB3NativeNetSession() : NetSession() {
        // Base ctor (above) built the MsgSource base + idle offline state. Bind
        // the DTA name and the TheNetSession global (the real ctor does both).
        TheNetSession = this;
        SetName("session", ObjectDir::Main());
    }
    virtual ~RB3NativeNetSession() {}
    // Override EVERY NetSession virtual (pure + non-pure) as an offline no-op so
    // the subclass vtable references only our slots — never the un-compiled
    // NetSession::X bodies (which would be undefined at link). Online-only entry
    // points; none is reached on the offline boot path.
    // The real NetSession::Handle (un-compiled network/NetSession.cpp) has a HANDLE
    // table for the session-control DTA messages the menu sends ({session clear},
    // disconnect, end_game, ...). Our stub must intercept those offline-no-op
    // messages here, else they fall through to MsgSource::Handle -> Hmx::Object's
    // property handlers (`clear`/`remove`/...) which do _msg->Array(2) on a 2-element
    // {session clear} and MILO_FAIL out-of-range. The query messages (is_local/
    // is_in_game/num_users) are answered by the typed methods above via the real
    // NetSession HANDLE_EXPRs — those live in the un-compiled TU too, so answer them
    // here as well to keep the offline DTA flow consistent.
    virtual DataNode Handle(DataArray *msg, bool warn) {
        // DTA message form is {obj msg_type args...}; the message symbol is at
        // index 1 (index 0 is the receiver object), per BEGIN_HANDLERS.
        Symbol type = msg->Size() > 1 ? msg->Sym(1) : Symbol("");
        if (type == Symbol("clear") || type == Symbol("disconnect")
            || type == Symbol("end_game") || type == Symbol("start_game")
            || type == Symbol("register_online") || type == Symbol("update_settings"))
            return DataNode(0); // offline session-control no-ops
        if (type == Symbol("is_local"))
            return DataNode(IsLocal() ? 1 : 0);
        if (type == Symbol("is_in_game"))
            return DataNode(IsInGame() ? 1 : 0);
        if (type == Symbol("is_busy"))
            return DataNode(IsBusy() ? 1 : 0);
        if (type == Symbol("num_users"))
            return DataNode((int)mUsers.size());
        if (type == Symbol("is_host"))
            return DataNode(IsHost() ? 1 : 0);
        if (type == Symbol("is_online_enabled"))
            return DataNode(IsOnlineEnabled() ? 1 : 0);
        return MsgSource::Handle(msg, warn);
    }
    virtual void Poll() {}
    virtual void WriteStats(const std::vector<UserStat> &) {}
    virtual void SetInvitesAllowed(bool) {}
    virtual void InviteFriend(Friend *, const char *, const char *) {}
    virtual Job *PrepareRegisterHostSessionJob() { return nullptr; }
    virtual void AddLocalToSession(LocalUser *) {}
    virtual void AddRemoteToSession(RemoteUser *) {}
    virtual void RemoveLocalFromSession(LocalUser *) {}
    virtual void RemoveRemoteFromSession(RemoteUser *) {}
    virtual void StartSession() {}
    virtual void EndSession(bool) {}
    virtual void DeleteSession() {}
    virtual Job *PrepareConnectSessionJob() { return nullptr; }
    virtual void FinishJoin(const JoinResponseMsg &) {}
    virtual Job *PrepareRegisterArbitrationJob() { return nullptr; }
    virtual void UpdateSettings() {}
    virtual void OnSetPublic(bool) {}
    virtual bool OnMsg(const VoiceDataMsg &) { return false; }
};
} // namespace

// --- native BandNetGameData ---
// There is NO BandNetGameData.cpp anywhere in the decomp (only the header), so
// its ctor + virtuals are weak no-op stubs. But SessionMgr's ctor does
// `mBandNetGameData(new BandNetGameData())` and SessionMgr::Handle's last
// HANDLE_MEMBER_PTR(mBandNetGameData) forwards unhandled messages to it. With a
// no-op ctor the object's vtable is garbage → `mBandNetGameData->Handle(...)`
// SIGSEGVs (faults at vtable+0x38). This bit when the local-user join's
// AddLocalUserResultMsg ("add_local_user_result") falls through every explicit
// SessionMgr handler down to the member-ptr forward. Provide a minimal native
// impl: construct the Hmx::Object base (valid vtable + empty sink list) and
// answer Handle as unhandled (the message is meant for SessionMgr's Export
// sinks, e.g. the overshell, not for the net-game-data member). The pure
// virtuals are offline no-ops (online net-game-data is not used single-player).
BandNetGameData::BandNetGameData() : NetGameData(), Hmx::Object() {}
BandNetGameData::~BandNetGameData() {}
int BandNetGameData::GetNumPlayersAllowed() const { return 4; }
void BandNetGameData::GetEndGameStats(std::vector<UserStat> &) const {}
int BandNetGameData::PublicID() const { return 0; }
void BandNetGameData::AuthenticationData(BinStream &, const User *) const {}
bool BandNetGameData::AuthenticateJoin(BinStream &, int &) const { return true; }
DataNode BandNetGameData::Handle(DataArray *msg, bool warn) {
    return Hmx::Object::Handle(msg, warn);
}
void BandNetGameData::Poll() {}

// Construct the native session global. Called from the native SystemInit, after
// Main()/sMainDir exists (SetName needs it).
void RB3InitNativeNetSession() {
    if (!TheNetSession)
        new RB3NativeNetSession(); // ctor sets TheNetSession = this
}

#endif // HX_NATIVE
