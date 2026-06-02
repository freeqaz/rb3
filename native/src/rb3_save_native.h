// RB3 Native Port — C2 save/profile/settings persistence (keystone, layer c glue)
//
// A tiny host-side blob-persistence interface that lets ProfileMgr's
// self-contained global-options + per-profile GameplayOptions serializers
// survive a restart, WITHOUT resurrecting the excluded Wii SaveLoadManager /
// MemcardMgr async state machine (which is never Poll()'d in the native frame
// loop — see scopeNote in the C2 task brief).
//
// The IPersistBackend abstraction keeps the storage layer swappable: the native
// build wires a HostFsBackend (fopen/fread/fwrite under an XDG/RB3_SAVE_DIR
// directory); a future web build (C10) can drop in an IndexedDB / IDBFS backend
// behind the same interface with no changes to the ProfileMgr-layer glue.
#pragma once

// Storage abstraction. Implementations persist an opaque, fixed-size byte blob
// keyed by a short filename-like string.
struct IPersistBackend {
    virtual ~IPersistBackend() {}
    // Read EXACTLY `len` bytes for `key` into `buf`. Returns false (and leaves
    // buf untouched) if the key is absent OR its stored size != len — the
    // size check is the first line of defense against feeding a stale/foreign/
    // short blob into a rev-asserting deserializer (LoadGlobalOptions etc).
    virtual bool Read(const char *key, void *buf, int len) = 0;
    // Write `len` bytes from `buf` under `key`. Returns false on I/O failure.
    virtual bool Write(const char *key, const void *buf, int len) = 0;
};

// Returns the currently-selected backend (HostFsBackend on native). Never null
// after first call; lazily constructs the default backend.
IPersistBackend *RB3PersistBackend();

// Override the backend (web build calls this with an IDBFS/IndexedDB backend
// before any load/save). Takes ownership conceptually; pass a static instance.
void RB3SetPersistBackend(IPersistBackend *backend);

// Boot-time LOAD: marks ProfileMgr's global-options save-state loaded, then —
// if a same-build blob exists — round-trips it through LoadGlobalOptions(); also
// restores profile-0 GameplayOptions if present. Safe to call once after the
// App ctor (ProfileMgr alive) and before the menu reads options. First run with
// no file just sets the save-state so defaults persist on the next save.
void RB3SaveLoadGlobalOptions();

// Exit-time SAVE: serializes ProfileMgr global options + profile-0
// GameplayOptions to the backend. Registered as a TheDebug exit callback; only
// touches ProfileMgr + the host FS (no GPU/synth), so it is teardown-safe.
void RB3SaveSaveGlobalOptions();
