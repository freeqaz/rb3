// native_file.cpp — a plain stdio-backed File for the native (clang LP64) build.
// The RB3-Wii File backends (ArkFile / AsyncFile / FileCache / CDReader) target
// the disc/ARK and aren't on the DTA-parse path. NewFile() (os/File.cpp, under
// HX_NATIVE) routes here for synchronous reads from the host filesystem.
//
// DTA OVERLAY ENGINE (ported from DC3's platform/File_Native.cpp):
//   A tracked `native/dta/` directory in the repo mirrors the archive's path
//   structure. When the engine opens a file for READ, this backend checks the
//   overlay dir FIRST: if `native/dta/<rel>` exists on disk, it opens that copy
//   instead of the on-disc/extracted original. This lets the port ship small,
//   git-tracked DTA patches (e.g. config/joypad.dta with a `button_meanings`
//   block the Xbox-flavoured config lacks) without modifying the gitignored
//   extracted assets. See docs/native/DTA_OVERLAY_ENGINE.md.
//
//   RB3's native NewFile() (os/File.cpp) routes straight here with the path the
//   matched-fork passes — relative to the data dir cwd (the native boot chdir's
//   to RB3_DATA, so `config/joypad.dta` etc. are relative). DTA #include resolves
//   the include name against the including file's dir (DataFile.cpp:64
//   FileMakePath(FileGetPath(gFile), c)) before calling NewFile, so includes such
//   as `#include joypad.dta` from config/band_keep.dta arrive here as the relative
//   `config/joypad.dta` — exactly the overlay key. The intercept is therefore the
//   single chokepoint covering both top-level DTA loads and #include opens.
//
//   On web (__EMSCRIPTEN__) the overlay is served at a higher layer: the dev
//   server (native/web/server.py) prefers `native/dta/<path>` over the extracted
//   asset in both /api/bundle and /api/file, so the bytes that reach MEMFS are
//   already the overlay copy. The C++ disk-overlay below is therefore native-only.
#ifdef HX_NATIVE

#include "os/File.h"
#include <cstdio>
#include <cstring>
#ifndef __EMSCRIPTEN__
#include <cstdlib>      // getenv
#include <sys/stat.h>   // stat (overlay existence probe)
#endif

#ifdef __EMSCRIPTEN__
#include "platform/WebAssets.h"
#include <emscripten/em_asm.h>
#include <string>

namespace {

// W4b — IndexedDB asset cache shim. The JS pre-warm in native/web/rb3_pre.js
// loads cached (path -> bytes) rows into window.__rb3IdbCache at boot. We
// check that map BEFORE issuing a sync XHR so warm boots skip the network
// entirely. On a miss, we call the engine's WebAssetsFetchSync (which does
// the sync XHR + writes MEMFS), then read the bytes back and queue an async
// IDB write.
//
// The cache key is the server-relative path (the same string the engine
// passes to /api/file/<rel>). We replicate the engine's normalization
// (WebAssets.cpp WebAssetsFetchSync header) here so the keys agree.
const char *cacheRelFromMemfsPath(const char *memfsPath) {
    const char *rel = memfsPath;
    if (strncmp(rel, "/data/", 6) == 0) return rel + 6;
    if (strncmp(rel, "/../", 4) == 0) return rel + 4;
    if (rel[0] == '/') return rel + 1;
    return rel;
}

// Sync-read the JS cache and (on hit) write the cached bytes straight into
// MEMFS. Returns 1 on hit (file ready at memfsPath), 0 on miss. Safe to call
// when the cache hasn't pre-warmed yet — returns 0.
int cacheTryHit(const char *cacheKey, const char *memfsPath) {
    return EM_ASM_INT({
        try {
            if (!window.__rb3IdbReady) return 0;
            if (!window.__rb3IdbCache) return 0;
            var key = UTF8ToString($0);
            var memfsPath = UTF8ToString($1);
            var bytes = window.__rb3IdbCache.get(key);
            if (!bytes) {
                window.__rb3CacheStats.misses++;
                return 0;
            }
            // Mkdir parents and write the cached bytes — mirrors
            // WebAssetsFetchSync's MEMFS write block.
            var parts = memfsPath.split('/');
            var dir = '';
            for (var i = 0; i < parts.length - 1; i++) {
                if (parts[i] === '') continue;
                dir += '/' + parts[i];
                try { FS.mkdir(dir); } catch (e) {}
            }
            FS.writeFile(memfsPath, bytes);
            window.__rb3CacheStats.hits++;
            window.__rb3CacheStats.bytesFromCache += bytes.byteLength;
            return 1;
        } catch (e) {
            console.log('[rb3-idb] cache-hit write failed: ' + e);
            return 0;
        }
    }, cacheKey, memfsPath);
}

// Read a freshly-fetched file from MEMFS and put it in the JS cache (which
// async-writes it through to IDB). Called after a successful network fetch.
void cachePutAfterFetch(const char *cacheKey, const char *memfsPath) {
    EM_ASM({
        try {
            if (!window.__rb3CachePut) return;
            var key = UTF8ToString($0);
            var memfsPath = UTF8ToString($1);
            var bytes = FS.readFile(memfsPath);
            window.__rb3CacheStats.bytesFetched += bytes.byteLength;
            window.__rb3CachePut(key, bytes);
        } catch (e) {
            console.log('[rb3-idb] cache-put failed: ' + e);
        }
    }, cacheKey, memfsPath);
}

} // namespace
#endif

namespace {
class NativeStdioFile : public File {
public:
    NativeStdioFile(const char *path, int mode)
        : mFp(nullptr), mFail(true), mLastReadBytes(0), mReadMode(false), mSize(-1) {
        // mode bit 2 (0x2) = read on Wii; otherwise treat as write/append.
        bool readMode = (mode & 2) != 0;
        mReadMode = readMode;
        const char *m = readMode ? "rb" : ((mode & 0x800) ? "ab" : "wb");
        mFp = std::fopen(path, m);
#ifdef __EMSCRIPTEN__
        // On-demand asset fetch: under emcc the assets live on the dev server,
        // not in MEMFS. If a READ open misses (the milo dependency graph pulls
        // sibling .milo_xbox / texture files lazily as DirLoader::LoadObjects
        // walks it), fetch the file into MEMFS via a synchronous XHR and retry.
        // RB3 excludes the engine's AsyncFile_Native.cpp (which normally hosts
        // this hook for the DC3 build), so it must live here. Pass `path`
        // unchanged to both calls — WebAssetsFetchSync writes to the same path
        // it opens (DC3's AsyncFileNative::_OpenAsync uses the same contract).
        if (!mFp && readMode) {
            // WebAssetsFetchSync writes the fetched bytes into MEMFS using the
            // path it is handed: it runs `FS.mkdir` for each ABSOLUTE parent dir
            // (`/ui`, `/ui/resource`, …) then `FS.writeFile(path)`. If `path` is
            // RELATIVE (FileRoot() is "." on web, so most resource milos —
            // fonts, icons — arrive as `ui/resource/.../foo.milo_xbox`), the
            // mkdir loop creates `/ui/...` while writeFile resolves the relative
            // path against the FS cwd (`/data`), i.e. `/data/ui/...`, whose
            // parent dirs were never made → `ErrnoError: No such file or
            // directory` and a failed fetch (e.g. pentatonic_regularsmall.milo
            // for the HUD BandLabels → UILabel.cpp:522 ResourceDir() abort).
            // Anchor a relative path under /data (the MEMFS asset root, == cwd)
            // so the fetch's mkdir/writeFile and this fopen agree on one
            // absolute location. The subsequent fopen still uses the original
            // (relative) `path`, which resolves to the same `/data/...` file.
            // Guarded out of the matched Wii/native asm (no __EMSCRIPTEN__).
            std::string fetchPath = path;
            if (!fetchPath.empty() && fetchPath[0] != '/')
                fetchPath = "/data/" + fetchPath;
            // W4b — try the IDB cache first (sync via window.__rb3IdbCache). On
            // a hit the bytes are written straight to MEMFS and the subsequent
            // fopen succeeds with no network round-trip. On a miss, fall
            // through to the engine's sync XHR + queue an async write-back.
            const char *cacheKey = cacheRelFromMemfsPath(fetchPath.c_str());
            if (cacheTryHit(cacheKey, fetchPath.c_str())) {
                mFp = std::fopen(path, m);
            } else if (WebAssetsFetchSync(fetchPath.c_str())) {
                mFp = std::fopen(path, m);
                if (mFp)
                    cachePutAfterFetch(cacheKey, fetchPath.c_str());
            }
        }
#endif
        mFail = (mFp == nullptr);
        if (mFp) {
            // QW-2 (loader-performance.md): coalesce the loader's 64 KiB chunk
            // reads into fewer host syscalls with a fully-buffered 64 KiB stdio
            // buffer. setvbuf must run before any I/O on the stream.
            std::setvbuf(mFp, nullptr, _IOFBF, 1 << 16);
            // QW-2: cache the file length once for read-mode files (the loader
            // hot path opens read-only and never writes), so Size()/Eof() skip
            // the per-call fseek(END)/ftell/fseek(back) dance. Write/append files
            // keep the live computation (mSize == -1) since their length grows.
            if (mReadMode) {
                long cur = std::ftell(mFp);
                if (std::fseek(mFp, 0, SEEK_END) == 0) {
                    long end = std::ftell(mFp);
                    std::fseek(mFp, cur < 0 ? 0 : cur, SEEK_SET);
                    if (end >= 0)
                        mSize = end;
                }
            }
        }
    }
    ~NativeStdioFile() override {
        if (mFp)
            std::fclose(mFp);
    }

    int Read(void *buf, int n) override {
        if (!mFp || n < 0)
            return -1;
        return (int)std::fread(buf, 1, (size_t)n, mFp);
    }
    bool ReadAsync(void *buf, int n) override {
        mLastReadBytes = Read(buf, n);
        return mLastReadBytes == n;
    }
    int Write(const void *buf, int n) override {
        if (!mFp || n < 0)
            return -1;
        return (int)std::fwrite(buf, 1, (size_t)n, mFp);
    }
    int Seek(int offset, int whence) override {
        // Wii: 0=set, 1=cur, 2=end (matches SEEK_SET/CUR/END).
        if (!mFp)
            return -1;
        if (std::fseek(mFp, offset, whence) != 0)
            return -1;
        return (int)std::ftell(mFp);
    }
    int Tell() override { return mFp ? (int)std::ftell(mFp) : -1; }
    void Flush() override {
        if (mFp)
            std::fflush(mFp);
    }
    bool Eof() override {
        // The DTA lexer (YY_INPUT) reads 1 byte at a time and checks Eof()
        // BEFORE each read. feof() only trips AFTER a read past end, so use the
        // position-vs-size test to report EOF when no bytes remain — otherwise
        // the final 0-byte read sets the stream's fail flag and DataInput
        // asserts (DataFile.cpp:566).
        if (!mFp)
            return true;
        if (std::feof(mFp))
            return true;
        long cur = std::ftell(mFp);
        return cur >= 0 && cur >= Size();
    }
    bool Fail() override { return mFail; }
    int Size() override {
        if (!mFp)
            return 0;
        // QW-2: cached for read-mode files (set once in the ctor); fall back to
        // the live fseek dance for write/append files whose length changes.
        if (mSize >= 0)
            return (int)mSize;
        long cur = std::ftell(mFp);
        std::fseek(mFp, 0, SEEK_END);
        long end = std::ftell(mFp);
        std::fseek(mFp, cur, SEEK_SET);
        return (int)end;
    }
    int UncompressedSize() override { return Size(); }
    bool ReadDone(int &result) override {
        result = mLastReadBytes;
        mLastReadBytes = 0;
        return true;
    }
    int GetFileHandle(DVDFileInfo *&info) override {
        info = nullptr;
        return 0;
    }

private:
    std::FILE *mFp;
    bool mFail;
    int mLastReadBytes;
    bool mReadMode; // QW-2: only read-mode files cache mSize
    long mSize;     // QW-2: cached file length (-1 = not cached / recompute live)
};
} // namespace

#ifndef __EMSCRIPTEN__
// --- DTA overlay directory resolution (native disk overlay) ----------------
//
// Mirrors DC3's NativeDetectOverlayDir(): find `native/dta/` relative to the
// data dir / repo. On RB3 the boot chdir's to RB3_DATA (default
// <repo>/orig-assets/extracted), so the overlay lives at
// "<RB3_DATA>/../../native/dta". We also honour an explicit RB3_DTA_OVERLAY env
// override and probe a couple of CWD-relative fallbacks. Detection runs lazily
// on the first open and is cached. Empty cache string => no overlay (every open
// falls through to the plain extracted-asset read, exactly as before this TU).
namespace {

bool FileExistsRaw(const char *p) {
    struct stat st;
    return p && *p && ::stat(p, &st) == 0;
}

const char *OverlayDir() {
    static char sDir[1024];
    static int sResolved = 0; // 0 = not yet, 1 = resolved (sDir may be empty)
    if (sResolved)
        return sDir[0] ? sDir : nullptr;
    sResolved = 1;
    sDir[0] = '\0';

    // 1) Explicit override.
    if (const char *env = ::getenv("RB3_DTA_OVERLAY"); env && *env) {
        if (FileExistsRaw(env)) {
            std::snprintf(sDir, sizeof(sDir), "%s", env);
            std::fprintf(stderr, "RB3 native: DTA overlay dir = %s (RB3_DTA_OVERLAY)\n", sDir);
            return sDir;
        }
    }

    // 2) Relative to RB3_DATA: "<RB3_DATA>/../../native/dta". The native boot
    //    chdir's INTO RB3_DATA, so this also works as a relative CWD probe
    //    ("../../native/dta") — but resolve against the env value when present so
    //    detection is independent of when (before/after chdir) it first runs.
    if (const char *data = ::getenv("RB3_DATA"); data && *data) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), "%s/../../native/dta", data);
        if (FileExistsRaw(buf)) {
            std::snprintf(sDir, sizeof(sDir), "%s", buf);
            std::fprintf(stderr, "RB3 native: DTA overlay dir = %s\n", sDir);
            return sDir;
        }
    }

    // 3) CWD-relative fallbacks (running from the repo root, or post-chdir into
    //    orig-assets/extracted which is two dirs deep under the repo root).
    static const char *cands[] = {
        "../../native/dta", // post-chdir into orig-assets/extracted
        "native/dta",       // repo root
        nullptr,
    };
    for (const char **c = cands; *c; ++c) {
        if (FileExistsRaw(*c)) {
            std::snprintf(sDir, sizeof(sDir), "%s", *c);
            std::fprintf(stderr, "RB3 native: DTA overlay dir = %s\n", sDir);
            return sDir;
        }
    }

    return nullptr; // no overlay (optional) — log once for clarity
}

// If `path` (a read-mode relative path) is shadowed by an overlay file, return
// the overlay path in `out` and true; else false. Absolute paths and write-mode
// opens never use the overlay. Paths containing ".." are rejected: the overlay
// is keyed on the archive-relative layout (e.g. "config/joypad.dta"), so a path
// like "../../system/run/config/macros.dta" would, joined to the overlay dir,
// `..`-escape back out to the repo's real system/run tree — a false-positive
// "hit" that opens the same bytes the plain path would. Reject `..` so the
// overlay only shadows files that genuinely live INSIDE native/dta/.
bool ResolveOverlay(const char *path, char *out, size_t outSize) {
    if (!path || !*path || path[0] == '/')
        return false; // absolute or empty: not an overlay candidate
    if (std::strstr(path, ".."))
        return false; // escapes the overlay tree — not a real overlay key
    const char *dir = OverlayDir();
    if (!dir)
        return false;
    char buf[1024];
    std::snprintf(buf, sizeof(buf), "%s/%s", dir, path);
    if (!FileExistsRaw(buf))
        return false;
    std::snprintf(out, outSize, "%s", buf);
    return true;
}

} // namespace
#endif // !__EMSCRIPTEN__

File *HmxNativeOpenFile(const char *path, int mode) {
#ifndef __EMSCRIPTEN__
    // DTA overlay: for READ opens (Wii mode bit 0x2), if native/dta/<path>
    // exists, open that copy instead. Write/append opens are never overlaid.
    if ((mode & 2) != 0 && path) {
        char overlayPath[1024];
        if (ResolveOverlay(path, overlayPath, sizeof(overlayPath))) {
            NativeStdioFile *of = new NativeStdioFile(overlayPath, mode);
            if (!of->Fail()) {
                std::fprintf(stderr, "RB3 native: DTA overlay HIT: '%s' -> %s\n",
                             path, overlayPath);
                return of; // overlay hit
            }
            delete of; // overlay stat'd but open failed — fall through
        }
    }
#endif
    NativeStdioFile *f = new NativeStdioFile(path, mode);
    return f; // NewFile callers check ->Fail() and release on failure
}

#endif // HX_NATIVE
