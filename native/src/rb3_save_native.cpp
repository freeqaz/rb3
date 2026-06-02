// RB3 Native Port — C2 save/profile/settings persistence (keystone, layer c glue)
//
// Persists ProfileMgr global options + profile-0 GameplayOptions across restart
// by writing the FixedSizeSaveableStream buffers (the exact buffer protocol the
// excluded SaveLoadManager states 0x32 read / 0x33 write use) straight to a host
// file, bypassing the Wii NAND/memcard async state machine entirely.
//
// Why this is the MVP cut (verified against source — see task scopeNote):
//   * SaveLoadManager.cpp is a 2267-line async state machine driven ONLY by
//     Poll(); the native frame loop (App::RunOneFrame) never polls it, so even a
//     perfectly-compiled SaveLoadManager would sit at idle forever and never
//     reach MemcardMgr.OnSaveGame/OnLoadGame. It stays EXCLUDED.
//   * ProfileMgr.cpp already compiles natively; its global-options and the
//     per-profile GameplayOptions serializers are clean and self-contained.
//
// The save side mirrors SaveLoadManager.cpp:1106-1120 (state 0x33); the load side
// mirrors SaveLoadManager.cpp:495-496 + :1095-1104 (state 0x32). The only
// substitution is memcard/cache write/read -> fwrite/fread to a host file.

#include "rb3_save_native.h"

#include "meta_band/ProfileMgr.h"
#include "meta_band/BandProfile.h"
#include "meta_band/GameplayOptions.h"
#include "meta/FixedSizeSaveableStream.h"
#include "meta/Profile.h"
#include "utl/MemMgr.h"
#include "os/Debug.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// Host-filesystem dir/path helpers (idioms ported from DC3's proven
// native/src/platform/MemcardMgr_Stub.cpp:16-53). Env override is RB3_SAVE_DIR;
// default is $XDG_STATE_HOME/rb3 || $HOME/.local/share/rb3/saves.
// ---------------------------------------------------------------------------
static const char *GetSaveDir() {
    static char sDir[512] = {};
    if (sDir[0])
        return sDir;

    const char *envDir = getenv("RB3_SAVE_DIR");
    if (envDir && envDir[0]) {
        snprintf(sDir, sizeof(sDir), "%s", envDir);
    } else if (const char *xdg = getenv("XDG_STATE_HOME"); xdg && xdg[0]) {
        snprintf(sDir, sizeof(sDir), "%s/rb3", xdg);
    } else {
        const char *home = getenv("HOME");
        if (!home)
            home = "/tmp";
        snprintf(sDir, sizeof(sDir), "%s/.local/share/rb3/saves", home);
    }
    return sDir;
}

static bool EnsureSaveDir() {
    const char *dir = GetSaveDir();
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    // Create parent directories recursively (mkdir -p).
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);

    struct stat st;
    return (stat(dir, &st) == 0 && S_ISDIR(st.st_mode));
}

static void GetSavePath(char *buf, size_t bufSize, const char *key) {
    snprintf(buf, bufSize, "%s/%s", GetSaveDir(), key);
}

// ---------------------------------------------------------------------------
// HostFsBackend — IPersistBackend over fopen/fread/fwrite. Read() enforces an
// EXACT size match (rejecting stale/foreign/short blobs) so a rev-asserting
// deserializer is never handed a bad buffer.
// ---------------------------------------------------------------------------
namespace {

class HostFsBackend : public IPersistBackend {
public:
    bool Read(const char *key, void *buf, int len) override {
        if (len <= 0)
            return false;
        char path[512];
        GetSavePath(path, sizeof(path), key);
        FILE *f = fopen(path, "rb");
        if (!f)
            return false;
        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        // EXACT size gate: a same-build blob is always exactly `len` bytes
        // (the size is rev-locked). Any mismatch -> reject (first-run/upgrade
        // path: caller skips Load, keeps ctor defaults, re-saves on exit).
        if (fileSize != (long)len) {
            fclose(f);
            if (fileSize >= 0)
                printf("rb3-native: persist '%s' size %ld != expected %d — "
                       "skipping load (first-run/upgrade)\n",
                       key, fileSize, len);
            return false;
        }
        size_t got = fread(buf, 1, (size_t)len, f);
        fclose(f);
        if ((int)got != len) {
            MILO_WARN("rb3-native: persist read %zu of %d bytes from '%s'\n", got, len, path);
            return false;
        }
        printf("rb3-native: persist loaded %d bytes from %s\n", len, path);
        return true;
    }

    bool Write(const char *key, const void *buf, int len) override {
        if (len <= 0)
            return false;
        EnsureSaveDir();
        char path[512];
        GetSavePath(path, sizeof(path), key);
        FILE *f = fopen(path, "wb");
        if (!f) {
            MILO_WARN("rb3-native: persist failed to open '%s' for writing\n", path);
            return false;
        }
        size_t wrote = fwrite(buf, 1, (size_t)len, f);
        fclose(f);
        if ((int)wrote != len) {
            MILO_WARN("rb3-native: persist wrote %zu of %d bytes to '%s'\n", wrote, len, path);
            return false;
        }
        printf("rb3-native: persist saved %d bytes to %s\n", len, path);
        return true;
    }
};

HostFsBackend gHostFsBackend;
IPersistBackend *gPersist = nullptr;

const char *kGlobalOptionsKey = "globaloptions.bin";
const char *kGameplayOptionsKey0 = "gameplayopts_0.bin";

} // namespace

IPersistBackend *RB3PersistBackend() {
    if (!gPersist)
        gPersist = &gHostFsBackend;
    return gPersist;
}

void RB3SetPersistBackend(IPersistBackend *backend) { gPersist = backend; }

// ---------------------------------------------------------------------------
// GameplayOptions (profile 0) — a trivial 9-byte blob (lefty/vocal-vol/style).
// Persisted as a second, independent file so a missing/short one never blocks
// global options (and vice-versa).
// ---------------------------------------------------------------------------
static void SaveGameplayOptions0() {
    BandProfile *profile = TheProfileMgr.GetProfileFromPad(0);
    if (!profile) {
        printf("rb3-native: persist — no profile-0; skipping gameplay-options save\n");
        return;
    }
    GameplayOptions *opts = profile->GetGameplayOptions();
    if (!opts)
        return;
    int sz = GameplayOptions::SaveSize(0); // == 9, rev-locked
    void *buf = _MemAllocTemp(sz, 0);
    {
        FixedSizeSaveableStream stream(buf, sz, true);
        opts->SaveFixed(stream);
    }
    RB3PersistBackend()->Write(kGameplayOptionsKey0, buf, sz);
    _MemFree(buf);
}

static void LoadGameplayOptions0() {
    BandProfile *profile = TheProfileMgr.GetProfileFromPad(0);
    if (!profile)
        return;
    GameplayOptions *opts = profile->GetGameplayOptions();
    if (!opts)
        return;
    int sz = GameplayOptions::SaveSize(0); // == 9
    void *buf = _MemAllocTemp(sz, 0);
    if (RB3PersistBackend()->Read(kGameplayOptionsKey0, buf, sz)) {
        FixedSizeSaveableStream stream(buf, sz, true);
        opts->LoadFixed(stream, 0);
    }
    _MemFree(buf);
}

// ---------------------------------------------------------------------------
// Global options. Mirrors SaveLoadManager state 0x33 (write) / 0x32 (read).
// ---------------------------------------------------------------------------
void RB3SaveSaveGlobalOptions() {
    // SaveGlobalOptions reads TheModifierMgr (gRev>=7) — alive in the standard
    // boot. The buffer protocol is byte-identical to SaveLoadManager.cpp:1110-1120.
    int sz = TheProfileMgr.GetGlobalOptionsSize();
    printf("rb3-native: persist saving global options (%d bytes)\n", sz);
    void *buf = _MemAllocTemp(sz, 0);
    {
        FixedSizeSaveableStream stream(buf, sz, true);
        TheProfileMgr.SaveGlobalOptions(stream);
    }
    RB3PersistBackend()->Write(kGlobalOptionsKey, buf, sz);
    _MemFree(buf);

    // Per-profile gameplay options (profile 0).
    SaveGameplayOptions0();
}

void RB3SaveLoadGlobalOptions() {
    // Mark the save-state loaded so GlobalOptionsNeedsSave()/asserts behave.
    // ctor inits mGlobalOptionsSaveState = kMetaProfileUnloaded, and
    // SetGlobalOptionsSaveState only asserts state != kMetaProfileUnchanged, so
    // passing kMetaProfileLoaded here is safe (mirrors SaveLoadManager:1138).
    TheProfileMgr.SetGlobalOptionsSaveState(kMetaProfileLoaded);

    int sz = TheProfileMgr.GetGlobalOptionsSize();
    printf("rb3-native: persist loading global options (expected %d bytes)\n", sz);
    void *buf = _MemAllocTemp(sz, 0);
    // Read()'s exact-size gate rejects stale/foreign/short files BEFORE we
    // construct the stream, so LoadGlobalOptions' ASSERT_REVS(7,2) only ever
    // sees a buffer this same build wrote. A first run (no file) simply skips
    // the Load — ctor defaults persist on the next exit save.
    if (RB3PersistBackend()->Read(kGlobalOptionsKey, buf, sz)) {
        FixedSizeSaveableStream stream(buf, sz, true);
        TheProfileMgr.LoadGlobalOptions(stream);
    }
    _MemFree(buf);

    // Per-profile gameplay options (profile 0).
    LoadGameplayOptions0();
}
