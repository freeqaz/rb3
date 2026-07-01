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
#include <chrono>      // steady_clock (RB3_FAKE_ASYNC_OPEN_MS timer)
#ifndef __EMSCRIPTEN__
#include <cstdlib>      // getenv
#include <sys/stat.h>   // stat (overlay existence probe)
#endif

#ifdef __EMSCRIPTEN__
#include "platform/WebAssets.h"
#include <emscripten/em_asm.h>
#include <emscripten/emscripten.h> // emscripten_sleep (JSPI sync-read fallback)
#include <cstdlib> // getenv (RB3_BOOT_IO_STATS)
#include <string>
#include <map>     // WebRangeFile chunk cache
#include <unordered_map> // cross-open chunk cache (N2: shared across WebRangeFile instances)
#include <list>    // cross-open LRU ordering
#include <memory>  // shared_ptr (copy-on-read chunk lifetime across eviction)
#include <vector>  // WebRangeFile chunk bytes
#include <cstdint> // uint8_t

// --- Boot I/O instrumentation (Step 0, handoff 02-boot-sync-read) ----------
// Env-gated (RB3_BOOT_IO_STATS=1) counters that attribute every READ-mode open
// to the path it took, so we can quantify how much of the ~7s App-ctor idle is
// blocking sync-XHR fetches vs already-resident MEMFS reads. Plain global ints
// (web build is single-threaded). main_web.cpp dumps them at appctor_done via
// RB3BootIoStatsDump(). Always compiled but zero-cost unless the env flag is set
// (the counters tick regardless — they're a handful of int adds; the gate only
// controls whether they're printed).
int gBootIoOpensRead = 0;       // total read-mode opens seen by this backend
int gBootIoFirstTry = 0;        // fopen succeeded on the first try (already in MEMFS)
int gBootIoResidentSkip = 0;    // Fix A: FS.analyzePath said resident → fopen, no fetch
int gBootIoCacheHit = 0;        // cacheTryHit() warm IDB hit → fopen
int gBootIoFetchSync = 0;       // WebAssetsFetchSync() blocking sync-XHR → fopen
int gBootIoStillFail = 0;       // all paths exhausted, open still failed

namespace {

// Fix A — residency short-circuit. Before paying for a cache lookup or a
// blocking sync XHR, ask MEMFS whether the file is already present (it usually
// is: the eager boot bundle wrote the boot-critical set through to MEMFS before
// the App ctor). If resident, the caller just re-fopen's and skips both slow
// paths. Cannot regress the warm IDB cache: the bundle's own onBundleSuccess
// writes bundled files through to IDB independently of this block, and the
// per-file cachePutAfterFetch only runs on the WebAssetsFetchSync miss path
// (which a resident file never reaches). Returns 1 if resident, 0 otherwise
// (including on any JS error → fall through to the existing paths).
int memfsResident(const char *memfsPath) {
    // A/B gate (measurement only): RB3_BOOT_NO_RESIDENCY_SKIP=1 disables Fix A so
    // the same build can measure the pre-Fix-A open distribution. Default = on.
    static int sDisabled = -1;
    if (sDisabled < 0) {
        const char *e = ::getenv("RB3_BOOT_NO_RESIDENCY_SKIP");
        sDisabled = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (sDisabled)
        return 0;
    return EM_ASM_INT({
        try {
            var p = UTF8ToString($0);
            var a = FS.analyzePath(p);
            return (a && a.exists) ? 1 : 0;
        } catch (e) {
            return 0;
        }
    }, memfsPath);
}

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

// ===========================================================================
// A1 (PLAN.md T6/T7) — async-open File backends.
//
// Today every MEMFS miss blocks the wasm thread on a synchronous XHR inside the
// NativeStdioFile ctor (the single canvas-freeze chokepoint). T6 replaces that
// for manifest-known files with a WebPendingFile that kicks the engine's async
// fetch and reports ReadDone()=false until the bytes land — the frame loop keeps
// running. T7 additionally serves *.mogg via HTTP Range so a 31-36 MB mogg is
// never whole-filed into MEMFS just to play a 15 s preview.
//
// Both are forward-declared here; their factory (MaybeOpenAsync) and the
// flag readers follow the class bodies. The plain-synchronous NativeStdioFile
// below stays the fast path for already-resident files (boot bundles!) and the
// manifest-miss fallback, so boot does not regress.
// ===========================================================================

// RB3_ASYNC_OPEN_OFF=1 restores the sync-XHR open path wholesale (T6 + T7 off).
static bool AsyncOpenEnabled() {
    static int sEnabled = -1;
    if (sEnabled < 0) {
        const char *e = ::getenv("RB3_ASYNC_OPEN_OFF");
        sEnabled = (e && e[0] && e[0] != '0') ? 0 : 1;
    }
    return sEnabled != 0;
}

// RB3_MOGG_RANGE_OFF=1 falls back to T6's whole-file pending fetch for moggs
// (no Range streaming). Default ON.
static bool MoggRangeEnabled() {
    static int sEnabled = -1;
    if (sEnabled < 0) {
        const char *e = ::getenv("RB3_MOGG_RANGE_OFF");
        sEnabled = (e && e[0] && e[0] != '0') ? 0 : 1;
    }
    return sEnabled != 0;
}

// N2 (matrix fix #2, part A): RB3_MOGG_READAHEAD_OFF=1 disables the read-ahead
// slot — chunk N+1 is no longer prefetched while chunk N is being consumed, so
// every chunk boundary pays a full RTT + transfer serially (the pre-N2 behavior).
// Default ON.
static bool MoggReadAheadEnabled() {
    static int sEnabled = -1;
    if (sEnabled < 0) {
        const char *e = ::getenv("RB3_MOGG_READAHEAD_OFF");
        sEnabled = (e && e[0] && e[0] != '0') ? 0 : 1;
    }
    return sEnabled != 0;
}

// N2 (matrix fix #2, part B): cross-open chunk cache byte budget in MB. Shared
// across WebRangeFile instances for the same URL so gameplay's fresh stream reuses
// the chunks the preview already fetched (chunks 0-1 + the seek window). 0 disables
// the cross-open cache entirely (each instance is back to its own per-object LRU).
// Default 24 MB. env RB3_MOGG_CACHE_MB.
static long MoggCacheBytes() {
    static long sBytes = -1;
    if (sBytes < 0) {
        const char *e = ::getenv("RB3_MOGG_CACHE_MB");
        long mb = (e && e[0]) ? std::atol(e) : 24;
        if (mb < 0)
            mb = 0;
        sBytes = mb * (1L << 20);
    }
    return sBytes;
}

} // namespace

// Loader-side yield counters (defined in src/system/utl/Loader.cpp HX_WEB arm).
// Declared here so the boot-I/O dump can print the full picture in one place.
extern int gLoaderPollYields;            // Poll()/PollUntilEmpty throttled yields
extern double gLoaderPollYieldMs;        // wall-ms spent in those yields
extern int gLoaderPullSliceYields;       // PollUntilLoaded per-slice yields
extern double gLoaderPullSliceYieldMs;   // wall-ms spent in those yields
extern int gLoaderPullCalls;             // PollUntilLoaded invocations

// Called from main_web.cpp at appctor_done. Gated on RB3_BOOT_IO_STATS so it is
// silent on a normal boot but prints the Step-0 attribution when measuring.
extern "C" void RB3BootIoStatsDump(const char *tag) {
    const char *e = ::getenv("RB3_BOOT_IO_STATS");
    if (!e || !e[0] || e[0] == '0')
        return;
    std::fprintf(stderr,
        "[boot-io-stats %s] reads=%d firstTry=%d residentSkip=%d cacheHit=%d "
        "fetchSync=%d stillFail=%d | loaderPollYields=%d (%.0fms) "
        "pullSliceYields=%d (%.0fms) pullCalls=%d\n",
        tag ? tag : "appctor_done",
        gBootIoOpensRead, gBootIoFirstTry, gBootIoResidentSkip, gBootIoCacheHit,
        gBootIoFetchSync, gBootIoStillFail,
        gLoaderPollYields, gLoaderPollYieldMs,
        gLoaderPullSliceYields, gLoaderPullSliceYieldMs, gLoaderPullCalls);
}
#else  // !__EMSCRIPTEN__
// Native (non-web) build: the boot-I/O stats are a web-only concept (the sync
// XHR / IDB cache paths don't exist), so provide a no-op so main_web.cpp can
// call it unconditionally. (main_web.cpp is only compiled for the web target,
// but keep the symbol resolvable for any shared TU.)
#include <cstdio>
extern "C" void RB3BootIoStatsDump(const char *) {}
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
        if (readMode)
            ++gBootIoOpensRead;
        if (mFp && readMode)
            ++gBootIoFirstTry; // already resident in MEMFS — no fetch needed
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
            // Fix A (handoff 02): if the file is ALREADY resident in MEMFS,
            // re-fopen and skip both the IDB lookup and the blocking sync XHR.
            // The first fopen above used the original (possibly relative) `path`;
            // residency is keyed off the anchored `fetchPath` (== /data/<rel>),
            // which is where the boot bundle / a prior fetch wrote it. This is a
            // pure short-circuit: when not resident it falls through unchanged.
            if (memfsResident(fetchPath.c_str())) {
                mFp = std::fopen(path, m);
                if (mFp)
                    ++gBootIoResidentSkip;
            }
            if (!mFp && cacheTryHit(cacheKey, fetchPath.c_str())) {
                mFp = std::fopen(path, m);
                if (mFp)
                    ++gBootIoCacheHit;
            } else if (!mFp && WebAssetsFetchSync(fetchPath.c_str())) {
                mFp = std::fopen(path, m);
                if (mFp) {
                    ++gBootIoFetchSync;
                    cachePutAfterFetch(cacheKey, fetchPath.c_str());
                }
            }
            if (!mFp && readMode)
                ++gBootIoStillFail;
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

#ifdef __EMSCRIPTEN__
// ===========================================================================
// WebPendingFile — A1 (T6): the async-open seam fix.
//
// On a MEMFS miss for a manifest-known file, HmxNativeOpenFile returns one of
// these instead of blocking the wasm thread on a sync XHR. It:
//   * kicks the engine's async fetch (WebAssetsEnsureResidentAsync) at ctor;
//   * answers Size()/Fail() IMMEDIATELY from the boot-loaded manifest oracle
//     (so ChunkStream's ctor `mFail = mFile->Fail()` and DirLoader::OpenFile's
//     `mStream->Fail()` check see "exists, not failed" while bytes are pending,
//     and FileLoader::OpenFile's Size()+AllocBuffer() get the real size up
//     front — the accepted v1 cost is a pending 36 MB mogg holding its buffer
//     during the fetch, but moggs take the WebRangeFile path below, not this);
//   * reports ReadDone()=false until the fetch lands (WebAssetsEnsureStatus),
//     then lazily opens the real NativeStdioFile and satisfies reads from MEMFS;
//   * if the fetch fails (status 2), flips mFail so the loader cleans up.
//
// The cooperative loader stack (FileLoader LoadFile poll, ChunkStream TempEof,
// VorbisReader ReadDone poll — all the Wii ReadAsync contract) was BUILT for
// this and is exercised by the E1 fake-async probe; this just supplies real
// bytes instead of a timer.
// ===========================================================================
class WebPendingFile : public File {
public:
    // openPath: the path NewFile was called with (for the eventual real open).
    // serverRel: the server-relative key for the manifest + async fetch.
    // size: the manifest-reported byte length (>= 0).
    WebPendingFile(const char *openPath, const std::string &serverRel, long size)
        : mOpenPath(openPath ? openPath : ""), mServerRel(serverRel), mSize(size),
          mReal(nullptr), mFail(false), mPendingResult(0), mReadPending(false),
          mPendingBuf(nullptr), mPendingLen(0) {
        // Kick the async fetch now (idempotent / deduped in the engine). If it is
        // already resident (a prefetch or prior open landed it), open immediately.
        if (WebAssetsIsResident(mServerRel.c_str()))
            TryOpenReal();
        else
            WebAssetsEnsureResidentAsync(mServerRel.c_str());
    }
    ~WebPendingFile() override { delete mReal; }

    int Read(void *buf, int n) override {
        // Synchronous Read on a still-pending file: block until resident (rare —
        // DTA-lexer-style paths, which are bundle-prefetched anyway). The engine's
        // ensure-async already kicked; busy-poll status (the loader/JSPI yields
        // elsewhere keep the tab alive). Bounded by the fetch completing.
        if (!mReal && !mFail)
            BlockUntilReadyOrFail();
        if (mReal)
            return mReal->Read(buf, n);
        return -1;
    }

    bool ReadAsync(void *buf, int n) override {
        if (mReal)
            return mReal->ReadAsync(buf, n);
        // Not resident yet: record the request; ReadDone() will service it once
        // the fetch lands. Mirrors the Wii DVD ReadAsync→ReadDone(false) contract.
        mPendingBuf = buf;
        mPendingLen = n;
        mReadPending = true;
        return false;  // not full yet
    }

    bool ReadDone(int &result) override {
        if (mReal) {
            // Already opened: delegate. If a read was queued while pending, run it
            // now (one-shot) so the first post-landing poll returns the bytes.
            if (mReadPending) {
                mReal->ReadAsync(mPendingBuf, mPendingLen);
                mReadPending = false;
            }
            return mReal->ReadDone(result);
        }
        if (mFail) {
            result = 0;
            return true;  // done (failed) — caller checks Fail()
        }
        int st = WebAssetsEnsureStatus(mServerRel.c_str());
        if (st == 1) {
            TryOpenReal();
            if (mReal) {
                if (mReadPending) {
                    mReal->ReadAsync(mPendingBuf, mPendingLen);
                    mReadPending = false;
                }
                return mReal->ReadDone(result);
            }
            // open failed despite residency → fail
            mFail = true;
            result = 0;
            return true;
        }
        if (st == 2) {
            mFail = true;
            result = 0;
            return true;
        }
        // st == 0: still pending. Re-kick is cheap + deduped; covers the
        // not-yet-ensured edge.
        WebAssetsEnsureResidentAsync(mServerRel.c_str());
        result = 0;
        return false;
    }

    int Write(const void *, int) override { return -1; }
    int Seek(int offset, int whence) override {
        if (!mReal && !mFail)
            BlockUntilReadyOrFail();
        return mReal ? mReal->Seek(offset, whence) : -1;
    }
    int Tell() override { return mReal ? mReal->Tell() : 0; }
    void Flush() override {
        if (mReal)
            mReal->Flush();
    }
    bool Eof() override {
        // Pre-open: not at EOF (bytes are coming). Post-open: delegate.
        return mReal ? mReal->Eof() : false;
    }
    bool Fail() override { return mFail; }
    int Size() override {
        if (mReal)
            return mReal->Size();
        return mSize >= 0 ? (int)mSize : 0;  // manifest oracle
    }
    int UncompressedSize() override { return Size(); }
    int GetFileHandle(DVDFileInfo *&info) override {
        info = nullptr;
        return 0;
    }

private:
    void TryOpenReal() {
        mReal = new NativeStdioFile(mOpenPath.c_str(), 2 /*read*/);
        if (mReal->Fail()) {
            delete mReal;
            mReal = nullptr;
        }
    }
    void BlockUntilReadyOrFail() {
        // Suspend-and-retry until the fetch resolves. emscripten_sleep keeps the
        // tab compositing (JSPI) — same primitive the loader's yield throttle
        // already uses; only reached on a rare synchronous Read of a pending file.
        for (;;) {
            int st = WebAssetsEnsureStatus(mServerRel.c_str());
            if (st == 1) {
                TryOpenReal();
                if (!mReal)
                    mFail = true;
                return;
            }
            if (st == 2) {
                mFail = true;
                return;
            }
            WebAssetsEnsureResidentAsync(mServerRel.c_str());
            emscripten_sleep(4);
        }
    }

    std::string mOpenPath;   // path to fopen once resident
    std::string mServerRel;  // server-relative key for manifest + fetch
    long mSize;              // manifest size (>= 0)
    File *mReal;            // lazily-opened resident file (null while pending)
    bool mFail;
    int mPendingResult;
    bool mReadPending;
    void *mPendingBuf;
    int mPendingLen;
};

// ===========================================================================
// MoggChunkCache — N2 (matrix fix #2, part B): process-wide, cross-open chunk
// cache keyed by (serverRel, chunkIdx).
//
// The per-WebRangeFile LRU dies with the instance. The preview stream and the
// gameplay stream open SEPARATE WebRangeFile objects for the same mogg, so the
// gameplay stream re-fetched chunks 0-1 (header) + the seek window the preview
// already pulled (07-network-matrix.md §3a, "bonus finding"). This static cache
// lets the second open reuse the first's chunks.
//
// LIFETIME / EVICTION SAFETY: chunk bytes are held by shared_ptr<const vector>.
// A live WebRangeFile that has pulled a chunk holds its OWN shared_ptr; the cache
// holds another. Eviction only drops the cache's reference — bytes a live reader
// is using stay alive until that reader drops them (copy-on-read via refcount, no
// deep copy). So eviction can NEVER free a chunk a live instance is reading.
// ===========================================================================
typedef std::shared_ptr<const std::vector<uint8_t> > ChunkBytes;

class MoggChunkCache {
public:
    static MoggChunkCache &Instance() {
        static MoggChunkCache sInst;
        return sInst;
    }

    static bool Dbg() {
        static int s = -1;
        if (s < 0) { const char *e = ::getenv("RB3_MOGG_CACHE_DBG"); s = (e && e[0] && e[0] != '0') ? 1 : 0; }
        return s != 0;
    }

    // Look up a cached chunk. Returns the bytes (shared, refcounted) or null on
    // miss. On hit, the entry is moved to MRU.
    ChunkBytes Get(const std::string &url, int chunk) {
        if (MoggCacheBytes() <= 0)
            return ChunkBytes();
        Key k(url, chunk);
        auto it = mMap.find(k);
        if (it == mMap.end()) {
            if (Dbg()) printf("[moggcache] MISS %s#%d (have %d entries)\n", url.c_str(), chunk, (int)mMap.size());
            return ChunkBytes();
        }
        mOrder.splice(mOrder.begin(), mOrder, it->second.pos);  // touch -> MRU
        if (Dbg()) printf("[moggcache] HIT  %s#%d\n", url.c_str(), chunk);
        return it->second.bytes;
    }

    // Insert (or refresh) a chunk. Evicts LRU entries to stay within budget.
    void Put(const std::string &url, int chunk, const ChunkBytes &bytes) {
        long budget = MoggCacheBytes();
        if (budget <= 0 || !bytes)
            return;
        Key k(url, chunk);
        auto it = mMap.find(k);
        if (it != mMap.end()) {
            // Replace bytes, move to MRU.
            mBytes -= (long)it->second.bytes->size();
            it->second.bytes = bytes;
            mBytes += (long)bytes->size();
            mOrder.splice(mOrder.begin(), mOrder, it->second.pos);
        } else {
            mOrder.push_front(k);
            Entry e;
            e.bytes = bytes;
            e.pos = mOrder.begin();
            mMap[k] = e;
            mBytes += (long)bytes->size();
            if (Dbg()) printf("[moggcache] PUT  %s#%d (now %d entries, %ld KB)\n", url.c_str(), chunk, (int)mMap.size(), mBytes / 1024);
        }
        // Evict LRU until within budget. Bytes a live reader still references stay
        // alive via its own shared_ptr; we only drop the cache's reference.
        while (mBytes > budget && !mOrder.empty()) {
            Key victim = mOrder.back();
            auto vit = mMap.find(victim);
            if (vit != mMap.end()) {
                mBytes -= (long)vit->second.bytes->size();
                mMap.erase(vit);
            }
            mOrder.pop_back();
        }
    }

private:
    MoggChunkCache() : mBytes(0) {}
    typedef std::pair<std::string, int> Key;
    struct KeyHash {
        size_t operator()(const Key &k) const {
            return std::hash<std::string>()(k.first) ^ (std::hash<int>()(k.second) * 2654435761u);
        }
    };
    struct Entry {
        ChunkBytes bytes;
        std::list<Key>::iterator pos;  // position in mOrder (MRU front)
    };
    std::list<Key> mOrder;  // front = MRU, back = LRU
    std::unordered_map<Key, Entry, KeyHash> mMap;
    long mBytes;  // total resident bytes in the cache
};

// ===========================================================================
// WebRangeFile — Q3 (T7): HTTP Range-backed *.mogg File.
//
// A 31-36 MB mogg should never be whole-filed into MEMFS just to play a preview.
// VorbisReader's access pattern (60 KB header read, seek-map jump via
// Seek(byteOffset), then sequential 0x4000 reads — VorbisReader.cpp:103,208,631)
// touches only a few MB. This File serves reads from a chunked LRU cache of
// fixed-size byte windows fetched over HTTP 206, kicking a range fetch on a miss
// and reporting ReadDone()=false until it lands. Size() comes from the manifest.
// Bytes are returned exactly at the requested offset, byte-identical to a
// full-file read — the AES-CTR decrypt downstream in VorbisReader is unaffected
// (it decrypts whatever bytes it receives at whatever file offset it sought to).
//
// NOTE on the read contract: callers do ReadAsync(buf,n) then poll ReadDone().
// We service the read out of the cache when the covering chunk(s) are resident;
// on a miss we kick the fetch and ReadDone() stays false until the chunk lands,
// then the SAME ReadAsync is retried internally and completed.
// ===========================================================================
class WebRangeFile : public File {
    static const int kChunkSize = 1 << 20;   // 1 MB range windows
    static const int kMaxChunks = 6;         // LRU window (~6 MB resident)

public:
    WebRangeFile(const std::string &serverRel, long size)
        : mServerRel(serverRel), mSize(size), mPos(0), mFail(false),
          mPendingBuf(nullptr), mPendingLen(0), mPendingDone(0),
          mReadActive(false), mLru(0) {
        // Slot 0 = primary (the chunk the reader is consuming); slot 1 = read-ahead
        // (chunk N+1, kicked while N is being served). Both are dropped on teardown.
        for (int i = 0; i < kNumSlots; ++i) {
            mSlot[i].reqId = 0;
            mSlot[i].chunk = -1;
        }
    }
    ~WebRangeFile() override {
        // Teardown of BOTH slots. For each slot:
        //  - if its fetch already LANDED, harvest the bytes into the cross-open
        //    cache before releasing (so a read-ahead chunk that completed but was
        //    never consumed — e.g. the preview ends right after the header read —
        //    is preserved for the next open instead of being re-fetched).
        //  - if still in flight, WebAssetsRangeDrop abandons it safely (engine
        //    fb23b5e RangeRequest::abandoned) so the running fetch self-reclaims
        //    without UAF (preview-cancel).
        for (int i = 0; i < kNumSlots; ++i) {
            if (!mSlot[i].reqId)
                continue;
            HarvestOrDropSlot(i);
        }
    }

    int Read(void *buf, int n) override {
        // Synchronous read: block on each missing chunk (rare). Returns bytes
        // copied (clamped at EOF), or -1 on failure.
        if (mFail || n < 0)
            return mFail ? -1 : 0;
        long clamped = ClampLen(mPos, n);
        char *out = (char *)buf;
        long got = 0;
        while (got < clamped) {
            int chunk = (int)((mPos + got) / kChunkSize);
            if (!EnsureResidentFromCache(chunk)) {
                if (!BlockFetchChunk(chunk))
                    return mFail ? -1 : (int)got;
            }
            got += CopyFromChunk(chunk, mPos + got, out + got, clamped - got);
        }
        mPos += got;
        return (int)got;
    }

    bool ReadAsync(void *buf, int n) override {
        mPendingBuf = buf;
        mPendingLen = (int)ClampLen(mPos, n);
        mPendingDone = 0;
        mReadActive = true;
        // Try to satisfy immediately from cache; if a chunk is missing this kicks
        // the fetch and returns false (ReadDone keeps polling).
        return ServicePendingRead() && mPendingDone == mPendingLen;
    }

    bool ReadDone(int &result) override {
        if (!mReadActive) {
            result = 0;
            return true;
        }
        if (mFail) {
            result = mPendingDone;
            mReadActive = false;
            return true;
        }
        ServicePendingRead();
        if (mPendingDone >= mPendingLen) {
            mPos += mPendingDone;
            result = mPendingDone;
            mReadActive = false;
            return true;
        }
        result = 0;
        return false;
    }

    int Write(const void *, int) override { return -1; }
    int Seek(int offset, int whence) override {
        long base = (whence == 1) ? mPos : (whence == 2) ? mSize : 0;
        mPos = base + offset;
        if (mPos < 0)
            mPos = 0;
        return (int)mPos;
    }
    int Tell() override { return (int)mPos; }
    void Flush() override {}
    bool Eof() override { return mPos >= mSize; }
    bool Fail() override { return mFail; }
    int Size() override { return (int)mSize; }
    int UncompressedSize() override { return (int)mSize; }
    int GetFileHandle(DVDFileInfo *&info) override {
        info = nullptr;
        return 0;
    }

private:
    static const int kNumSlots = 2;  // [0] = primary, [1] = read-ahead (N+1)
    struct Chunk {
        ChunkBytes data;  // shared bytes for [index*kChunkSize, ..) (refcounted)
        long lru;
    };
    struct Slot {
        int reqId;  // in-flight range request id (0 = none)
        int chunk;  // chunk index the in-flight request covers (-1 = none)
    };

    long ClampLen(long pos, long n) const {
        if (n < 0)
            n = 0;
        if (pos + n > mSize)
            n = mSize - pos;
        if (n < 0)
            n = 0;
        return n;
    }

    // True if the chunk's bytes are usable directly from this instance's map.
    bool ChunkResident(int chunk) {
        return mChunks.find(chunk) != mChunks.end();
    }

    // Make `chunk` resident in mChunks if possible WITHOUT a network fetch: either
    // it's already here, or the cross-open cache (another WebRangeFile for the same
    // URL — e.g. the preview's chunks 0-1) has it. The cached bytes are shared by
    // refcount (no deep copy), so eviction from the cache never frees what we hold.
    bool EnsureResidentFromCache(int chunk) {
        if (ChunkResident(chunk))
            return true;
        ChunkBytes cached = MoggChunkCache::Instance().Get(mServerRel, chunk);
        if (!cached)
            return false;
        Chunk &c = mChunks[chunk];
        c.data = cached;
        c.lru = ++mLru;
        EvictIfNeeded();
        return true;
    }

    long ChunkByteLen(int chunk) const {
        long start = (long)chunk * kChunkSize;
        long len = kChunkSize;
        if (start + len > mSize)
            len = mSize - start;
        return len < 0 ? 0 : len;
    }

    int NumChunks() const {
        return (int)((mSize + kChunkSize - 1) / kChunkSize);
    }

    long CopyFromChunk(int chunk, long fileOff, char *dst, long want) {
        auto it = mChunks.find(chunk);
        if (it == mChunks.end() || !it->second.data)
            return 0;
        const std::vector<uint8_t> &bytes = *it->second.data;
        long chunkStart = (long)chunk * kChunkSize;
        long off = fileOff - chunkStart;
        long avail = (long)bytes.size() - off;
        long n = want < avail ? want : avail;
        if (n <= 0)
            return 0;
        memcpy(dst, bytes.data() + off, n);
        it->second.lru = ++mLru;
        return n;
    }

    // Poll the request in slot `i`. If it has landed, move the bytes into mChunks
    // AND publish them to the cross-open cache, then free the slot. Returns true if
    // a chunk landed (now resident), false otherwise. Sets mFail on fetch error.
    bool PollSlot(int i) {
        Slot &s = mSlot[i];
        if (s.reqId == 0)
            return false;
        int bytes = 0;
        bool ok = false;
        if (!WebAssetsRangeDone(s.reqId, &bytes, &ok))
            return false;  // still in flight
        if (!ok) {
            WebAssetsRangeDrop(s.reqId);
            s.reqId = 0;
            s.chunk = -1;
            // A primary-slot (i==0) error fails the read; a read-ahead-slot (i==1)
            // error is purely speculative — just free the slot. The primary slot
            // will re-fetch the chunk for real when the cursor reaches it.
            if (i == 0)
                mFail = true;
            return false;
        }
        // Landed: take the bytes into a shared buffer, store in both caches.
        std::shared_ptr<std::vector<uint8_t> > buf =
            std::make_shared<std::vector<uint8_t> >(bytes);
        WebAssetsRangeTake(s.reqId, buf->data(), bytes);
        ChunkBytes shared = buf;  // const view, shared by refcount
        int chunk = s.chunk;
        if (MoggChunkCache::Dbg()) printf("[moggcache] LANDED slot%d %s#%d bytes=%d\n", i, mServerRel.c_str(), chunk, bytes);
        Chunk &c = mChunks[chunk];
        c.data = shared;
        c.lru = ++mLru;
        MoggChunkCache::Instance().Put(mServerRel, chunk, shared);
        s.reqId = 0;
        s.chunk = -1;
        EvictIfNeeded();
        return true;
    }

    // Teardown/supersede helper: if slot `i`'s fetch has already LANDED, take its
    // bytes into the cross-open cache so the next open of this URL can reuse them
    // (without re-fetching); then release the request either way. Used by the dtor
    // and the supersede path so a completed-but-unconsumed read-ahead chunk is never
    // thrown away. Does NOT touch mChunks (safe to call mid-destruction).
    void HarvestOrDropSlot(int i) {
        Slot &s = mSlot[i];
        if (s.reqId == 0)
            return;
        int bytes = 0;
        bool ok = false;
        if (MoggCacheBytes() > 0 && WebAssetsRangeDone(s.reqId, &bytes, &ok) && ok &&
            bytes > 0) {
            std::shared_ptr<std::vector<uint8_t> > buf =
                std::make_shared<std::vector<uint8_t> >(bytes);
            int taken = WebAssetsRangeTake(s.reqId, buf->data(), bytes);  // frees req
            if (taken == bytes)
                MoggChunkCache::Instance().Put(mServerRel, s.chunk, buf);
        } else {
            // Still in flight (or cache disabled / error): abandon safely.
            WebAssetsRangeDrop(s.reqId);
        }
        s.reqId = 0;
        s.chunk = -1;
    }

    // Ensure a fetch is in flight (or resident) for `chunk` using slot `i`. If the
    // slot is busy with a DIFFERENT chunk it is superseded (VorbisReader reads
    // strictly forward within a window). Returns true if `chunk` is now resident.
    bool KickSlot(int i, int chunk) {
        if (EnsureResidentFromCache(chunk))
            return true;
        Slot &s = mSlot[i];
        if (s.reqId != 0 && s.chunk == chunk)
            return PollSlot(i);  // already fetching this chunk: advance it
        // If the OTHER slot is already fetching this chunk, don't start a duplicate
        // request — just poll the other slot (it may land here). Common case: the
        // read-ahead slot pre-fetched chunk N+1, then the cursor advances and the
        // primary slot is asked for N+1.
        int other = i ^ 1;
        if (other >= 0 && other < kNumSlots && mSlot[other].reqId != 0 &&
            mSlot[other].chunk == chunk) {
            return PollSlot(other);
        }
        if (s.reqId != 0) {
            // Slot busy with a stale chunk: if it already landed, harvest it into the
            // cross-open cache before releasing (otherwise abandon — UAF-safe).
            HarvestOrDropSlot(i);
        }
        long start = (long)chunk * kChunkSize;
        int len = (int)ChunkByteLen(chunk);
        if (len <= 0)
            return false;  // zero-length chunk at/after EOF — nothing to fetch
        s.reqId = WebAssetsRangeFetch(mServerRel.c_str(), start, len);
        s.chunk = chunk;
        if (s.reqId == 0) {
            mFail = true;
            return false;
        }
        return false;
    }

    // Primary-slot fetch for the chunk under the read cursor. Returns true if the
    // chunk is now resident.
    bool PollChunkFetch(int chunk) {
        // Advance ALL slots first so a previously-kicked read-ahead can land (it may
        // already hold the chunk we now want), then ensure the primary chunk.
        for (int i = 0; i < kNumSlots; ++i)
            if (mSlot[i].reqId)
                PollSlot(i);
        if (mFail)
            return false;
        if (ChunkResident(chunk))
            return true;
        return KickSlot(0, chunk);
    }

    // Read-ahead: while serving reads from chunk N, ensure N+1 is being fetched (or
    // already resident) on the read-ahead slot so the next chunk boundary never
    // costs a full RTT+transfer. No-op if N+1 is the chunk the primary slot is
    // already fetching, or past EOF, or read-ahead is disabled.
    void KickReadAhead(int currentChunk) {
        if (!MoggReadAheadEnabled())
            return;
        int next = currentChunk + 1;
        if (next >= NumChunks())
            return;
        if (ChunkResident(next))
            return;
        if (mSlot[0].reqId && mSlot[0].chunk == next)
            return;  // primary is already on it
        // Don't supersede an in-flight read-ahead fetch. If slot 1 is already busy
        // (e.g. it pre-fetched chunk K right before VorbisReader seeked forward),
        // let that fetch COMPLETE and be cached — abandoning it to chase the new
        // N+1 would (a) waste the bytes already on the wire and (b) drop a chunk the
        // next open (gameplay) is likely to need. Advance it first; only kick the
        // new read-ahead once the slot frees.
        if (mSlot[1].reqId) {
            PollSlot(1);          // try to land the in-flight read-ahead now
            if (mSlot[1].reqId)
                return;           // still in flight — leave it; don't supersede
        }
        KickSlot(1, next);  // slot free: kick the speculative N+1 (ignore result)
    }

    void EvictIfNeeded() {
        while ((int)mChunks.size() > kMaxChunks) {
            int victim = -1;
            long oldest = 0;
            for (auto &kv : mChunks) {
                if (victim < 0 || kv.second.lru < oldest) {
                    oldest = kv.second.lru;
                    victim = kv.first;
                }
            }
            if (victim < 0)
                break;
            mChunks.erase(victim);
        }
    }

    bool BlockFetchChunk(int chunk) {
        for (;;) {
            if (PollChunkFetch(chunk))
                return true;
            if (mFail)
                return false;
            emscripten_sleep(4);
        }
    }

    // Service the active ReadAsync from cache, copying as many contiguous bytes
    // as are resident and kicking a fetch for the first missing chunk. Returns
    // true if any progress was possible without an error.
    bool ServicePendingRead() {
        while (mPendingDone < mPendingLen) {
            long fileOff = mPos + mPendingDone;
            int chunk = (int)(fileOff / kChunkSize);
            if (!EnsureResidentFromCache(chunk)) {
                PollChunkFetch(chunk);  // poll slots / kick primary; not resident yet
                if (mFail) {
                    mReadActive = false;
                    return false;
                }
                KickReadAhead(chunk);  // overlap N+1 fetch with this stall
                return true;  // pending — ReadDone keeps polling
            }
            long n = CopyFromChunk(chunk, fileOff,
                                   (char *)mPendingBuf + mPendingDone,
                                   mPendingLen - mPendingDone);
            if (n <= 0)
                break;  // defensive
            mPendingDone += (int)n;
            KickReadAhead(chunk);  // chunk N is resident & being consumed: prefetch N+1
        }
        return true;
    }

    std::string mServerRel;
    long mSize;
    long mPos;
    bool mFail;
    Slot mSlot[kNumSlots];  // [0] primary, [1] read-ahead
    std::map<int, Chunk> mChunks;  // chunk index -> bytes
    // active ReadAsync state
    void *mPendingBuf;
    int mPendingLen;
    int mPendingDone;
    bool mReadActive;
    long mLru;
};
#endif // __EMSCRIPTEN__

// --- RB3_FAKE_ASYNC_OPEN_MS — A1 (pending-File async open) de-risk probe ------
//
// Dev-only experiment E1 (docs/native/incremental-load-perf/PLAN.md §4 E1, §5
// T2). DEFAULT OFF. When RB3_FAKE_ASYNC_OPEN_MS=<n> (n>0) is set, every READ-mode
// File returned by this backend is wrapped in a FakeAsyncFile: a delegating File
// that answers open / Size() / Fail() / Eof() / Seek() IMMEDIATELY from the real
// stdio file (mirroring what A1's WebPendingFile would learn from the manifest
// oracle), but whose ReadDone() returns false for n ms after each ReadAsync()
// before signalling completion. The actual bytes are read synchronously inside
// ReadAsync() (the host disk is instant) — we only DEFER the completion signal,
// which is exactly the observable the real async-fetch seam will introduce.
//
// This exercises, with NO network and NO web build, every loader path that polls
// the async-read contract:
//   * FileLoader::OpenFile/LoadFile  — Size()+Fail()+AllocBuffer() at open, then
//     ReadDone() poll (Loader.cpp:666-669, :682).
//   * ChunkStream::Eof()             — returns TempEof while mFile->ReadDone()==0
//     (ChunkStream.cpp:205-209, :268-281), feeding LoadStream's
//     MILO_ASSERT(t==TempEof) spins (Loader.cpp:735, :755).
//   * VorbisReader::Poll             — ReadAsync(hdr/4k) + ReadDone()/Fail()/Eof()
//     poll (VorbisReader.cpp:103-104, :208-218, :329-355, :405).
//
// PER-READ timing (not per-file): each ReadAsync starts a fresh n-ms timer. This
// best mimics a real async fetch where every read of a chunk completes after a
// round trip — the loader/stream machinery issues many ReadAsyncs over a file's
// life (ChunkStream reads each compressed chunk; VorbisReader streams 16 KB at a
// time), and we want each of them to spend time "in flight" so multi-frame opens
// AND multi-frame streaming are both stressed, not just the first read.

long FakeAsyncOpenMs() {
    // Read once via static init + getenv (Loader.cpp:217-226 pattern). <=0 / unset
    // => disabled (sentinel -1 distinguishes "not yet read"). Works on native
    // (getenv) and web (the T1 URL-param->ENV bridge surfaces it through getenv).
    static long sMs = -1;
    if (sMs == -1) {
        const char *e = ::getenv("RB3_FAKE_ASYNC_OPEN_MS");
        long v = (e && e[0]) ? std::atol(e) : 0;
        sMs = v > 0 ? v : 0;
    }
    return sMs;
}

// Delegating File that defers ReadDone() completion by FakeAsyncOpenMs() ms per
// read. Owns the wrapped delegate. Everything except ReadAsync/ReadDone forwards
// straight through, so open/stat/Size()/Fail()/Eof()/Seek()/Tell() are all exact
// and immediate — the only lie is WHEN the read reports done.
class FakeAsyncFile : public File {
public:
    explicit FakeAsyncFile(File *delegate, long delayMs)
        : mDelegate(delegate), mDelayMs(delayMs), mPendingResult(0),
          mPending(false) {}
    ~FakeAsyncFile() override { delete mDelegate; }

    class String Filename() const override { return mDelegate->Filename(); }
    int Read(void *buf, int n) override { return mDelegate->Read(buf, n); }

    bool ReadAsync(void *buf, int n) override {
        // Do the real read NOW (host disk is instant), but stash the result and
        // start the per-read timer; ReadDone() won't surface it until the delay
        // elapses. Mirrors a fetch that has the bytes queued but not "arrived".
        bool full = mDelegate->ReadAsync(buf, n);
        int result = 0;
        mDelegate->ReadDone(result); // drain the delegate's instant completion
        mPendingResult = result;
        mPending = true;
        mReadIssuedAt = std::chrono::steady_clock::now();
        return full;
    }

    bool ReadDone(int &result) override {
        if (!mPending) {
            // No read in flight (e.g. polled before any ReadAsync) — report the
            // delegate's idle state, which is "done, 0 bytes".
            return mDelegate->ReadDone(result);
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - mReadIssuedAt)
                           .count();
        if (elapsed < mDelayMs) {
            // Still "in flight": the caller must keep polling. result is left
            // untouched / 0 — callers (ChunkStream/VorbisReader/FileLoader) gate
            // on the bool and don't consume result until it returns true.
            result = 0;
            return false;
        }
        result = mPendingResult;
        mPending = false;
        return true;
    }

    int Write(const void *buf, int n) override { return mDelegate->Write(buf, n); }
    bool WriteAsync(const void *buf, int n) override {
        return mDelegate->WriteAsync(buf, n);
    }
    int Seek(int offset, int whence) override {
        return mDelegate->Seek(offset, whence);
    }
    int Tell() override { return mDelegate->Tell(); }
    void Flush() override { mDelegate->Flush(); }
    bool Eof() override { return mDelegate->Eof(); }
    bool Fail() override { return mDelegate->Fail(); }
    int Size() override { return mDelegate->Size(); }
    int UncompressedSize() override { return mDelegate->UncompressedSize(); }
    bool WriteDone(int &i) override { return mDelegate->WriteDone(i); }
    int GetFileHandle(DVDFileInfo *&info) override {
        return mDelegate->GetFileHandle(info);
    }
    int Truncate(int n) override { return mDelegate->Truncate(n); }

private:
    File *mDelegate;
    long mDelayMs;
    int mPendingResult;
    bool mPending;
    std::chrono::steady_clock::time_point mReadIssuedAt;
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

#ifdef __EMSCRIPTEN__
// A1 (T6/T7) async-open factory. For a READ open on web that MISSES MEMFS but is
// known to the manifest, return an async File (WebRangeFile for *.mogg with Range
// enabled, else WebPendingFile) instead of blocking the wasm thread on a sync
// XHR. Returns nullptr to signal "use the legacy synchronous path" (the file is
// already resident, the manifest doesn't know it — so a fallback root might —,
// or the async path is flag-disabled).
//
// The residency fast path is preserved: an already-resident file (boot bundle,
// prior fetch, IDB warm-write) opens synchronously below, so boot does NOT
// regress — only genuine network misses take the async path.
static File *MaybeOpenAsync(const char *path, int mode) {
    if ((mode & 2) == 0)         // write/append: never async
        return nullptr;
    if (!AsyncOpenEnabled())     // RB3_ASYNC_OPEN_OFF=1 → legacy sync path
        return nullptr;
    if (!path || !path[0])
        return nullptr;

    // Server-relative key: strip a leading "/data/" or "/" (mirrors the engine's
    // normalization; the manifest/fetch helpers re-strip defensively).
    std::string rel = path;
    if (rel.compare(0, 6, "/data/") == 0)
        rel = rel.substr(6);
    else if (!rel.empty() && rel[0] == '/')
        rel = rel.substr(1);

    // Already resident? Use the sync fast path (cheap fopen below). The engine's
    // residency probe keys off /data/<rel>, which is where the boot bundle and
    // prior fetches wrote it.
    if (WebAssetsIsResident(rel.c_str()))
        return nullptr;

    // Manifest oracle: a non-negative size means the file is a real ASSETS_DIR
    // asset → safe to serve async. A -1 is NOT a definitive 404 (fallback roots /
    // sidecars aren't in the manifest), so fall back to the legacy sync path,
    // which probes those and matches today's 404 semantics exactly.
    long size = WebAssetsManifestSize(rel.c_str());
    if (size < 0)
        return nullptr;

    // *.mogg → Range-backed streaming (touches a few MB, not the whole 31-36 MB),
    // unless RB3_MOGG_RANGE_OFF — then it falls through to the whole-file pending
    // fetch (WebPendingFile) like any other large asset.
    size_t n = rel.size();
    bool isMogg = n >= 5 && rel.compare(n - 5, 5, ".mogg") == 0;
    if (isMogg && MoggRangeEnabled())
        return new WebRangeFile(rel, size);

    return new WebPendingFile(path, rel, size);
}
#endif // __EMSCRIPTEN__

// E1 de-risk probe: wrap a successfully-opened READ-mode File so its ReadDone()
// reports completion only after RB3_FAKE_ASYNC_OPEN_MS ms (default off → no-op,
// returns the delegate unchanged). Never wrap a failed open (callers check
// Fail() and the wrapper must answer Fail() truthfully, which it does — but a
// failed open has nothing to delay) nor a write-mode open.
static File *MaybeWrapFakeAsync(File *f, int mode) {
    long delay = FakeAsyncOpenMs();
    if (delay <= 0 || !f)
        return f;
    if ((mode & 2) == 0) // write/append: not on the async-read contract
        return f;
    if (f->Fail()) // nothing to defer on a failed open
        return f;
    return new FakeAsyncFile(f, delay);
}

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
                return MaybeWrapFakeAsync(of, mode); // overlay hit
            }
            delete of; // overlay stat'd but open failed — fall through
        }
    }
#endif
#ifdef __EMSCRIPTEN__
    // A1 (T6/T7): async open for manifest-known MEMFS misses. nullptr => use the
    // sync path below (resident file, manifest miss, or flag-disabled).
    if (File *async = MaybeOpenAsync(path, mode))
        return async;  // already on the async ReadAsync/ReadDone contract; no FakeAsync wrap
#endif
    NativeStdioFile *f = new NativeStdioFile(path, mode);
    return MaybeWrapFakeAsync(f, mode); // NewFile callers check ->Fail() and release on failure
}

#endif // HX_NATIVE
