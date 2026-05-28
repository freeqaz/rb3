// rb3_waitinguser_gate_native.cpp — native WaitingUserGate ctor/dtor/Handle.
//
// PROBLEM: WaitingUserGate.cpp is in _NATIVE_FORK_EXCLUDE (CMakeLists.txt) — it
// uses MWCC-style `std::vector<T, unsigned short>` which clang's libstdc++ won't
// instantiate, and pulls in NetMessage / LockStepMgr deps that don't compile
// cleanly yet either. The link stubs (band3_link_stubs.s) provide weak no-op
// stubs for `WaitingUserGate::Init()`, `Poll()`, and `WaitingUserGate()` (the
// C1 ctor). Those let the link succeed but produce a broken runtime object:
//
//   `BandUI::Init()` does `mWaitingUserGate = new WaitingUserGate()`.
//
// `operator new` allocates memory; the ctor (weak stub `xorl %eax,%eax; ret`)
// does NOT set the vtable pointer. The first 8 bytes of the object are
// uninitialized garbage from malloc. When `BandUI::Terminate()` later does
// `RELEASE(mWaitingUserGate)` -> `delete mWaitingUserGate`, the deleting-
// destructor virtual dispatch reads that garbage as a vtable pointer and calls
// `*(vtable + 0x8)` — SIGSEGVing at a wild code address. This is the V4
// shutdown crash: 1500 gameplay frames run cleanly to songMs ~1027, then
// BandUI::Terminate() SIGSEGVs at the `RELEASE(mWaitingUserGate)` line.
//
// FAITHFUL FIX (same pattern as BandNetGameData in rb3_netsession_native.cpp):
// define WaitingUserGate's ctor/dtor/Handle as STRONG natives. clang then emits
// the real `vtable for WaitingUserGate` (the strong ctor properly sets up the
// vtable pointer; the strong destructor's vtable entry is a real code address),
// and the deleting destructor in BandUI::Terminate dispatches correctly.
//
// Handle() forwards to Hmx::Object::Handle (the real online handlers — for
// LockStepStartMsg / LockStepCompleteMsg / ProcessedJoinRequestMsg — are never
// reached offline; no multiplayer/lock-step path runs single-player). Poll() is
// non-virtual and stays a link-stub no-op (BandUI::Poll calls it but no state
// to advance). Init() is non-virtual static and likewise stays a stub (it would
// register two NetMessage subclasses we never deliver offline).
//
// Strong ctor wins over the weak stub at C1 because object-file resolution
// prefers strong over weak. The strong ctor also requires the strong dtor to
// be defined (else the implicit destructor's vtable slot remains undefined).
#ifdef HX_NATIVE

#include "meta_band/WaitingUserGate.h"
#include "obj/Data.h"

WaitingUserGate::WaitingUserGate() : mLockStepMgr(nullptr) {}

WaitingUserGate::~WaitingUserGate() {}

// Forward Handle() to the base Object::Handle so unhandled DTA messages take
// the Hmx::Object default path (the property/script handlers). The three real
// OnMsg overloads (lock-step + join-request) sit on online flows that the
// offline boot never enters; falling through to the base is the offline
// equivalent of "no real handler subscribed".
DataNode WaitingUserGate::Handle(DataArray *msg, bool warn) {
    return Hmx::Object::Handle(msg, warn);
}

#endif // HX_NATIVE
