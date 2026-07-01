// rb3_prefetch_native.cpp — see rb3_prefetch_native.h for the contract.
//
// Web arm reuses the engine's existing async fetch machinery (WebAssetsFetch ->
// emscripten_fetch -> MEMFS write at /data/<serverPath>, WebAssets.cpp:119-144),
// which writes the exact path the synchronous open's residency check looks at
// (native_file.cpp anchors a relative read path to /data/<rel> and short-circuits
// on FS.analyzePath().exists). So a completed prefetch makes the subsequent
// NewStream -> NewFile open hit the resident fast path with no blocking XHR.
//
// Native arm is a no-op: the host mogg is already on disk, so RB3MoggResident
// returns true immediately and the SongPreview fetch-pending state advances in
// one frame (behavior identical to pre-T3).
#ifdef HX_NATIVE

#include "rb3_prefetch_native.h"

#include <cstdlib> // getenv
#include <cstring>
#include <string>

#ifdef __EMSCRIPTEN__
#include "platform/WebAssets.h"
#include <emscripten/em_asm.h>
#endif

// ---------------------------------------------------------------------------
// Flag: RB3_PREVIEW_PREFETCH_OFF (default ON). Read once.
// ---------------------------------------------------------------------------
bool RB3PreviewPrefetchEnabled() {
    static int sEnabled = -1;
    if (sEnabled < 0) {
        const char *e = ::getenv("RB3_PREVIEW_PREFETCH_OFF");
        sEnabled = (e && e[0] && e[0] != '0') ? 0 : 1;
    }
    return sEnabled != 0;
}

// T7 coordination (PLAN.md): when the Range-backed mogg File is active
// (RB3_ASYNC_OPEN + RB3_MOGG_RANGE, both default ON), the preview must NOT
// whole-file the 31-36 MB mogg into MEMFS — the WebRangeFile (native_file.cpp)
// streams it from a few-MB Range window instead. So the preview prefetch becomes
// a no-op for moggs and the residency gate is satisfied by manifest knowledge
// (the stream construction is non-blocking once the open returns a WebRangeFile).
// Mirrors native_file.cpp's AsyncOpenEnabled()/MoggRangeEnabled() (same env, read
// once). Off web, or with either flag disabled, the legacy whole-file prefetch
// path below is unchanged.
static bool RB3MoggRangeActive() {
    static int sActive = -1;
    if (sActive < 0) {
        const char *ao = ::getenv("RB3_ASYNC_OPEN_OFF");
        const char *mr = ::getenv("RB3_MOGG_RANGE_OFF");
        bool asyncOn = !(ao && ao[0] && ao[0] != '0');
        bool rangeOn = !(mr && mr[0] && mr[0] != '0');
        sActive = (asyncOn && rangeOn) ? 1 : 0;
    }
    return sActive != 0;
}

#ifdef __EMSCRIPTEN__

namespace {

// Single tracked in-flight prefetch. The preview path warms at most one mogg at
// a time (the hovered song); a new hover supersedes the previous one. Tracking
// the path lets us dedupe a repeat hover of the same song (debounce can re-fire
// StartSongPreview while the fetch is still pending) without issuing a second
// emscripten_fetch.
std::string sInFlightPath; // server-relative (no /data/, no leading slash)
int sInFlightId = 0;       // WebAssetsFetch id (0 = none)

// Normalize to the server-relative key WebAssetsFetch expects: strip a leading
// "/data/" or "/" so the MEMFS write lands at /data/<rel> and the residency
// check (which probes /data/<rel>) agrees. Callers pass already-relative paths,
// but be defensive.
std::string toServerRel(const char *p) {
    if (!p || !p[0])
        return std::string();
    std::string s(p);
    if (s.compare(0, 6, "/data/") == 0)
        return s.substr(6);
    if (!s.empty() && s[0] == '/')
        return s.substr(1);
    return s;
}

// MEMFS residency probe — identical predicate to native_file.cpp's
// memfsResident() so the prefetch's notion of "done" matches the sync open's
// "skip the XHR" short-circuit.
int memfsExists(const char *memfsPath) {
    return EM_ASM_INT(
        {
            try {
                var a = FS.analyzePath(UTF8ToString($0));
                return (a && a.exists) ? 1 : 0;
            } catch (e) {
                return 0;
            }
        },
        memfsPath);
}

} // namespace

void RB3PrefetchMogg(const char *serverRelMoggPath) {
    if (!RB3PreviewPrefetchEnabled())
        return;
    // T7: with the Range-backed mogg File active, do NOT whole-file the mogg.
    // The WebRangeFile streams it on demand; this avoids the 31-36 MB MEMFS+IDB
    // write the legacy prefetch did. (The first VorbisReader read warms the Range
    // header window — one ~64 KB-2 MB 206 request — instead.)
    if (RB3MoggRangeActive())
        return;
    std::string rel = toServerRel(serverRelMoggPath);
    if (rel.empty())
        return;

    // Already resident? Nothing to fetch (warm hover within the session).
    std::string memfs = std::string("/data/") + rel;
    if (memfsExists(memfs.c_str()))
        return;

    // In-flight dedupe: same path still pending => don't double-fetch.
    if (sInFlightId != 0 && sInFlightPath == rel && !WebAssetsFetchDone(sInFlightId))
        return;

    sInFlightPath = rel;
    sInFlightId = WebAssetsFetch(rel.c_str());
}

bool RB3MoggResident(const char *serverRelMoggPath) {
    std::string rel = toServerRel(serverRelMoggPath);
    if (rel.empty())
        return true; // never wedge on a bad path
    // T7: with Range active, the stream construction is non-blocking (NewFile
    // returns a WebRangeFile that streams over 206), so SongPreview should NOT
    // wait for whole-file MEMFS residency — it would wait forever, since the
    // mogg is never whole-filed. Treat "manifest knows it" (or already resident
    // from a prior whole-file path) as ready. The manifest oracle is loaded at
    // WebAssetsInit; if it isn't ready yet, fall through to the residency check
    // (the preview just defers one more frame, harmless).
    if (RB3MoggRangeActive() && WebAssetsManifestSize(rel.c_str()) >= 0)
        return true;
    std::string memfs = std::string("/data/") + rel;
    return memfsExists(memfs.c_str()) != 0;
}

void RB3PrefetchCancel() {
    // emscripten_fetch can't be aborted cleanly mid-flight, but we drop our
    // tracking so a later hover dedupes/refetches without confusion. The bytes,
    // if the orphaned fetch completes, simply land in MEMFS (a warm cache) and
    // are harmless.
    sInFlightPath.clear();
    sInFlightId = 0;
}

#else // !__EMSCRIPTEN__ — native: host file is already resident.

void RB3PrefetchMogg(const char *) {}
bool RB3MoggResident(const char *) { return true; }
void RB3PrefetchCancel() {}

#endif // __EMSCRIPTEN__

#endif // HX_NATIVE
