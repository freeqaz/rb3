// rb3_web_globals.cpp — web-only definitions for pointer globals that, on the
// native build, are supplied as weak zeroed symbols by the x86 GAS link-stub
// files (band3_link_stubs.s / dta_link_stubs.s). Those .s files are NOT
// assembled under emcc, and their owning C++ TUs (network/, rndwii/, Synth.cpp,
// the un-globbed Tour/SongDB/SessionMgr globals) are excluded from the web
// build — so each of these globals would otherwise be an UNDEFINED data symbol.
//
// With -sERROR_ON_UNDEFINED_SYMBOLS=0 the wasm linker resolves an undefined data
// reference to a GARBAGE address: a `TheFoo = x` write lands in a phantom slot
// (lost), and the next `TheFoo->Method()` read returns junk and the runtime
// traps ("table index is out of bounds") on the vcall — or, with a non-null
// garbage pointer, sails past a `if (TheFoo)` / `MILO_ASSERT(!TheFoo)` guard and
// then derefs the junk. This was the W3a App-boot wall: the ctor spun/trapped in
// SessionMgr::Init → BandMatchmaker/Tour ctors deref-ing garbage TheNetSession /
// TheSynth / TheSessionMgr / TheTour.
//
// Defining them here (web-compiled TU) gives each pointer real BSS storage so
// writes/reads round-trip and the null-guards work. WEAK so that if a real
// strong definition IS linked (the owning TU is in the web set after all), the
// real one wins and this is a harmless fallback. Forward-declared classes only
// (these are all pointers — no full type needed).
//
// W3a is audio/online/Wii-free: every object these point at stays null on web.
// W3c (audio) / a future online phase link the real impls and these fall away.
//
// NB: TheNetSession is defined (non-weak) in rb3_netsession_native.cpp and
// TheSynth in main_web.cpp; they are intentionally NOT repeated here.

#ifdef HX_WEB

class Tour;
class SessionMgr;
class SongDB;
class SyncStore;
class VoiceChatMgr;
class NgStats;
class SaveLoadManager;
class Splash;

#define RB3_WEB_WEAK_NULL(type, name) type *name __attribute__((weak)) = nullptr

// Boot-path pointer globals with no C++ storage def on web (stub-only on native).
RB3_WEB_WEAK_NULL(Tour, TheTour);
RB3_WEB_WEAK_NULL(SessionMgr, TheSessionMgr);
RB3_WEB_WEAK_NULL(SongDB, TheSongDB);
// network/ + rndwii/ globals (their TUs are excluded from the web build).
RB3_WEB_WEAK_NULL(SyncStore, TheSyncStore);
RB3_WEB_WEAK_NULL(VoiceChatMgr, TheVoiceChatMgr);
RB3_WEB_WEAK_NULL(NgStats, TheNgStats);
// meta_band / movie pointer globals: their owning TUs (SaveLoadManager.cpp,
// Splash.cpp) are NOT in the web source set (verified: their .o is absent),
// so the `extern T* TheFoo;` declarations resolve to undefined data symbols.
// The boot path null-checks both (e.g. `if (TheSaveLoadMgr)`), so null is the
// correct value — exactly what native's zeroed .s reservation supplies.
RB3_WEB_WEAK_NULL(SaveLoadManager, TheSaveLoadMgr);
RB3_WEB_WEAK_NULL(Splash, TheSplasher);

#undef RB3_WEB_WEAK_NULL

// --- Object-VALUE globals (declared `extern T TheFoo;`, NOT a pointer) -------
// These are full object instances whose owning TUs are excluded from the web
// build (network/ Net/NetMessage/Server, Wii Memcard/ContentMgr/Friend/
// Messenger/FX, meta StoreMetadataManager). On native the x86 GAS link stubs
// reserve a ZEROED .bss block for each (`.zero 256`, 4096 for the big ones), and
// the offline/Wii code paths that read their members find all-zero (the guards
// like `if (mLoggedIn)` / `if (handle != kNoHandle)` then skip the live work).
//
// Under emcc those .s blocks don't exist, so each is an UNDEFINED data symbol →
// garbage address → a member read returns junk → trap. Reproduce native's
// behaviour exactly: give each symbol a zeroed, over-aligned byte buffer aliased
// to the global's (unmangled, extern-"C"-style) symbol name. This mirrors DC3's
// `__attribute__((weak, used)) char TheDxRnd[8192] = {};` pattern. WEAK so a real
// strong def (if its TU is ever added to the web set) wins. `used` so LTO/GC
// can't drop the storage even though nothing in this TU references it.
//
// 4 KiB is comfortably larger than any of these objects; matching native's exact
// sizeof isn't required because every byte the code reads is zero either way.
#define RB3_WEB_ZERO_OBJ(name) \
    extern "C" { __attribute__((weak, used)) alignas(16) char name[4096] = {}; }

RB3_WEB_ZERO_OBJ(TheStoreMetadata)     // StoreMetadataManager (meta, store)
RB3_WEB_ZERO_OBJ(TheNet)               // Net (network)
RB3_WEB_ZERO_OBJ(TheNetMessageFactory) // NetMessageFactory (network)
RB3_WEB_ZERO_OBJ(TheMC)                // MemcardWii (os, Wii)
RB3_WEB_ZERO_OBJ(TheMemcardMgr)        // MemcardMgr (meta, Wii)
RB3_WEB_ZERO_OBJ(TheWiiContentMgr)     // WiiContentMgr (os, Wii)
RB3_WEB_ZERO_OBJ(TheWiiFriendMgr)      // WiiFriendMgr (network, Wii)
RB3_WEB_ZERO_OBJ(TheWiiFX)             // WiiFX (synthwii, Wii)
RB3_WEB_ZERO_OBJ(TheWiiMessenger)      // WiiMessenger (network, Wii)
// TheServer is `extern Server &TheServer;` — a REFERENCE, lowered by the ABI to
// a hidden pointer variable holding the referent address. Point it at a zeroed
// block so any `TheServer.member` read is zero (same as native's zeroed
// reservation). The pointer must be non-null to satisfy code that takes the
// reference's address; the zeroed block backs it.
extern "C" { __attribute__((weak, used)) alignas(16) char TheServer_storage[4096] = {}; }
extern "C" { __attribute__((weak, used)) void *TheServer = (void *)TheServer_storage; }

#undef RB3_WEB_ZERO_OBJ

#endif // HX_WEB
