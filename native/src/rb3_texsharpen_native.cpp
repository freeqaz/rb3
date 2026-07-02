// rb3 native/web — progressive in-session texture-sharpen DRIVER (research/13 T1).
//
// The engine sharpen MANAGER (milo-native-engine RB3TexSharpen.cpp) does the GPU
// work: parse a `.sharpen` sidecar's bytes, match each entry to a loaded venue
// RndTex by recomputed TexFingerprint, swap the RndBitmap up to full-res, and
// re-invoke the upload so the texture is RECREATED at full size (a new view that
// the cached material bind group rebuild key picks up automatically). This TU is
// the I/O + gameplay-gating DRIVER around it — the part that knows the asset
// roots, the venue path, and (web) the async low-priority fetch.
//
// WHAT IT DOES, per gameplay frame (RB3TexSharpenPoll):
//   1. Gate: only once GAMEPLAY IS RUNNING (songMs > 0) — A4 already got us to
//      gameplay fast; the sharpen runs AFTER, off the critical path, so it never
//      delays time-to-gameplay and (web) competes with mogg streaming at LOW
//      priority. Audio is non-negotiable; sharpen is cosmetic and may take minutes.
//   2. Resolve the venue: the same venue EnterVenue loaded (ComputeVenueMiloPath /
//      DirLoader::Find), and its loaded ObjectDir.
//   3. Derive the sidecar server path: <venuedir>/gen/<venue>.milo_xbox.sharpen
//      (the platform-cached form + ".sharpen"; the downscale server shadows it).
//   4. FETCH (web): WebAssetsEnsureResidentAsync(sidecarRel) — idempotent, LOW
//      priority, dedupes in-flight — then poll WebAssetsIsResident. NATIVE: the
//      sidecar is a local file (no network); FileExists gates it directly.
//   5. READ the resident bytes (NewFile NOARK → Size → Read) ONCE and hand them to
//      RB3SharpenLoadSidecar(venueDir, bytes, len).
//   6. Each subsequent frame, RB3SharpenStep(RB3SharpenPerFrame()) sharpens a few
//      textures — the incremental scheduler that keeps the recreate/upload bursts
//      from hitching gameplay. Stop once RB3SharpenComplete().
//
// FLAG. RB3_PROGRESSIVE_SHARPEN (engine-side, default ON; opt-out keeps the A4
// stripped venue stripped). RB3_SHARPEN_PER_FRAME tunes the budget. The web fetch
// arm is #ifdef __EMSCRIPTEN__ (never HX_WEB). State resets on song unload
// (RB3TexSharpenReset, called from RB3GameWarmReset).
//
// SCOPE. native+web glue (HX_NATIVE). It calls only public engine entry points
// (RB3Sharpen* + the File API + DirLoader). No render pass is open at Game::Poll
// time, so RB3SharpenStep (which drives GPU texture (re)creation) is safe here.

#ifdef HX_NATIVE

#include "platform/RB3TexSharpen.h"   // the engine manager

#include "obj/Dir.h"                  // ObjectDir
#include "obj/DirLoader.h"            // DirLoader::Find / GetDir
#include "os/File.h"                  // NewFile / FileExists / FILE_OPEN_NOARK
#include "os/Debug.h"                 // MILO_LOG
#include "os/System.h"               // PlatformSymbol(Platform)
#include "utl/FilePath.h"
#include "utl/MakeString.h"
#include "utl/Loader.h"               // TheLoadMgr.GetPlatform()

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include "platform/WebAssets.h"
#include <sys/stat.h>   // mkdir (MEMFS) for the assembled-sidecar write
#include <cstdio>       // fopen/fwrite (MEMFS)
#endif

// ComputeVenueMiloPath() lives in rb3_gamewarm_native.cpp (resolves the SAME venue
// EnterVenue loads: override → world `venue` prop → small_club_01). We reuse it so
// the sharpen targets exactly the venue dir the reveal loaded. Declared extern to
// avoid duplicating the resolution logic.
std::string RB3ComputeVenueMiloPathForSharpen();

// ---------------------------------------------------------------------------
// Flags / dbg.
// ---------------------------------------------------------------------------
static bool RB3SharpenDriverDbg() {
    static int s = -1;
    if (s < 0) s = (getenv("RB3_SHARPEN_DBG") != nullptr) ? 1 : 0;
    return s != 0;
}

// (web) sidecar fetch chunk size in KB (research/14 Lane B). Default 256 KB —
// small enough that a mogg Range fetch starting mid-chunk waits at most ~1.4s
// at 1.5 Mbps, large enough that the whole ~5.4 MB sidecar is ~21 requests.
// 0 = legacy single whole-file fetch (the pre-hardening behavior, kept as the
// escape hatch / A-B arm).
static int RB3SharpenChunkKB() {
    static int kb = -1;
    if (kb < 0) {
        const char* e = getenv("RB3_SHARPEN_CHUNK_KB");
        kb = e ? atoi(e) : 256;
        if (kb < 0) kb = 0;
        if (kb > 0 && kb < 16) kb = 16;     // floor: don't spam tiny requests
        if (kb > 8192) kb = 8192;
    }
    return kb;
}

namespace {

// Per-song driver state. One sharpen session per venue load.
struct SharpenDriver {
    bool        started = false;     // a session is loaded into the engine manager
    bool        finished = false;    // RB3SharpenComplete() seen (stop polling)
    bool        fetchKicked = false; // (web) legacy async fetch issued
    std::string venueMiloPath;       // world/venue/.../<venue>.milo (logical)
    std::string sidecarRel;          // server-relative gen/<v>.milo_xbox.sharpen
#ifdef __EMSCRIPTEN__
    // Chunk-pump state (research/14 Lane B — chunked, mogg-yielding fetch).
    int  chunkReqId = 0;             // in-flight Range request id (0 = none)
    int  chunkReqLen = 0;            // bytes requested for the in-flight chunk
    long chunkOffset = 0;            // next byte offset == bytes assembled so far
    long sidecarSize = -2;           // -2 unqueried; -1 manifest doesn't know; >=0 known
    int  chunkErrs = 0;              // consecutive errors at the current offset
    bool chunkFallback = false;      // permanent fallback to the legacy single fetch
    std::vector<uint8_t> assembly;   // chunks assembled in order
#endif
};
SharpenDriver gDrv;

// Build "world/venue/<cls>/<v>/gen/<v>.milo_xbox.sharpen" from the logical venue
// milo path "world/venue/<cls>/<v>/<v>.milo" — the CachedPath gen/<plat> transform
// (DirLoader::CachedPath) plus the ".sharpen" sidecar suffix. The downscale tree
// mirrors this exact key (server.py downscale_path), so the same string is the web
// fetch key AND the native local path.
std::string SidecarRelFromVenueMilo(const std::string& miloPath) {
    if (miloPath.empty()) return std::string();
    // Split into dir + base.<ext>.
    size_t slash = miloPath.find_last_of('/');
    std::string dir  = (slash == std::string::npos) ? std::string() : miloPath.substr(0, slash);
    std::string file = (slash == std::string::npos) ? miloPath : miloPath.substr(slash + 1);
    size_t dot = file.find_last_of('.');
    std::string base = (dot == std::string::npos) ? file : file.substr(0, dot);
    std::string ext  = (dot == std::string::npos) ? std::string() : file.substr(dot + 1);
    if (ext != "milo") return std::string(); // only sharpen platform-cached milos
    Symbol platSym = PlatformSymbol(TheLoadMgr.GetPlatform());
    const char* plat = platSym.Str();
    if (!plat || !plat[0]) plat = "xbox";
    return std::string(MakeString("%s/gen/%s.milo_%s.sharpen",
                                  dir.c_str(), base.c_str(), plat));
}

// Read a whole resident file's bytes via the engine File API (a READ + NO-ARK open
// works on native local files AND web MEMFS once resident). Returns false on any
// failure (caller then retries next frame or gives up). Caps the read at a sane
// ceiling so a corrupt size can't blow out memory.
//
// CRITICAL: the Wii READ bit is 0x2 — WITHOUT it, NativeStdioFile opens the path
// in WRITE mode ("wb"), which TRUNCATES the sidecar to zero (Size()==0 → this
// returns false, and the sidecar is destroyed on disk). `FILE_OPEN_NOARK` alone is
// not a read mode. So open READ | NO-ARK (0x2 | 0x10000). FileExists uses the same
// read bit (NewFile(path, iMode | 0x40002)).
static const int kFileOpenRead = 0x2;
bool ReadWholeFile(const char* path, std::vector<uint8_t>& out) {
    File* f = NewFile(path, kFileOpenRead | FILE_OPEN_NOARK);
    if (!f) return false;
    bool ok = false;
    if (!f->Fail()) {
        int sz = f->Size();
        if (sz > 0 && sz < (64 * 1024 * 1024)) {
            out.resize((size_t)sz);
            int got = f->Read(out.data(), sz);
            ok = (got == sz);
            if (!ok) out.clear();
        }
    }
    delete f;
    return ok;
}

// (web) is the sidecar resident in MEMFS yet? Native: always "resident" — the file
// either exists on disk or it doesn't (FileExists), and a missing sidecar simply
// means this venue wasn't downscaled (no-op, no error).
bool SidecarResident(const std::string& rel) {
#ifdef __EMSCRIPTEN__
    return WebAssetsIsResident(rel.c_str());
#else
    return FileExists(rel.c_str(), 0);
#endif
}

// (web) kick the low-priority async fetch. Idempotent (the engine dedupes an
// in-flight fetch for the same path). Native: no-op (local file).
void KickSidecarFetch(const std::string& rel) {
#ifdef __EMSCRIPTEN__
    // WebAssetsEnsureResidentAsync is the low-priority, in-flight-deduped async
    // fetch (the WebPendingFile counterpart). It does NOT block and does NOT
    // whole-file anything else — exactly the cosmetic, can-take-minutes priority
    // the sharpen wants (mogg Range streaming keeps its own higher-priority lane).
    WebAssetsEnsureResidentAsync(rel.c_str());
#else
    (void)rel;
#endif
}

#ifdef __EMSCRIPTEN__
// ---------------------------------------------------------------------------
// Research/14 Lane B — chunked, mogg-yielding sidecar fetch.
//
// The single whole-file fetch above shares the network with mogg Range
// streaming for the sidecar's entire ~29s (at 1.5 Mbps) transfer. The chunk
// pump replaces it: one RB3_SHARPEN_CHUNK_KB (default 256 KB) Range request at
// a time, kicked ONLY on frames where WebAssetsRangeInFlightCount() == 0 — our
// own chunk is never in flight at check time, so any live Range fetch is the
// mogg's, and we strictly yield. Worst-case interference: the mogg starts
// mid-chunk and waits out ONE chunk (~1.4s at 1.5 Mbps).
//
// SIZE DISCOVERY (verified against the live downscale server 2026-07-02):
// /api/manifest walks only ASSETS_DIR — the downscale tree (where .sharpen
// sidecars live) is a per-request shadow — so WebAssetsManifestSize() returns
// -1 for sidecars and SHORT-READ EOF DETECTION is the working terminator:
// the server clamps a Range at EOF (206 with fewer bytes than asked = final
// chunk) and 416s a request starting past EOF (the exact-chunk-multiple case;
// surfaces as fetch error after progress → treat as EOF). The manifest query
// is still made and honored when >= 0 so a future manifest union gets the
// authoritative size for free.
//
// INTEGRITY: a persistent mid-file error also lands in the "EOF after
// progress" arm after kChunkErrMax retries; the assembled blob is then finalized and the
// SHRP parser is the integrity gate — a truncated blob fails its structural
// bounds checks → RB3SharpenLoadSidecar matches 0 → clean cosmetic no-op
// (never a corrupt upload).
//
// RANGE-IGNORING SERVERS (review B1): RFC 9110 §14.2 lets a server ignore
// Range and 200 the whole body (python -m http.server; Range-stripping
// proxies). Detected per-landing via taken > chunkReqLen (a 206 can never
// exceed the request): at offset 0 the body IS the file → accept + finalize;
// mid-assembly it's at the wrong offset → discard + legacy single fetch.
// Either way the assembly is additionally capped at kAssemblyCap (64 MB,
// ReadWholeFile's ceiling) so no topology can grow the heap unboundedly.
// ---------------------------------------------------------------------------

// Consecutive-error budget at one offset before deciding (EOF-after-progress /
// fallback-at-zero). Each retry is a FRESH Range request one frame apart — a
// genuine 416 (exact-multiple EOF) fails all of them in ~5 RTTs; a transient
// connection blip usually recovers within one or two.
static const int kChunkErrMax = 5;

// Assembly ceiling — the same 64 MB sanity cap ReadWholeFile enforces. A server
// that ignores Range (or any runaway body) can never grow the assembly past it
// (review B1 belt-and-braces).
static const long kAssemblyCap = 64L * 1024 * 1024;

// mkdir -p + write the assembled bytes to MEMFS at /data/<rel> — the exact
// residency key WebAssetsIsResident stats — so the existing ReadWholeFile →
// RB3SharpenLoadSidecar path runs UNCHANGED. (WebAssets' own dir-ensure helper
// is a TU-static, not exposed; MEMFS mkdir via stdio is the documented
// fallback.) Returns false on any failure.
bool WriteAssembledSidecarToMemfs(const std::string& rel,
                                  const std::vector<uint8_t>& bytes) {
    std::string path = std::string("/data/") + rel;
    // mkdir -p every parent component under /data/ (EEXIST is fine).
    for (size_t i = 6 /* skip "/data/" */; i < path.size(); i++) {
        if (path[i] == '/')
            mkdir(path.substr(0, i).c_str(), 0777);
    }
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t n = bytes.empty() ? 0 : fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);
    return n == bytes.size();
}

// Finalize the assembly: write to MEMFS (flips WebAssetsIsResident true) and
// free the buffer. On a write failure, fall back to the legacy single fetch.
void FinalizeSidecarAssembly() {
    bool ok = !gDrv.assembly.empty() &&
              WriteAssembledSidecarToMemfs(gDrv.sidecarRel, gDrv.assembly);
    if (RB3SharpenDriverDbg())
        MILO_LOG("RB3_SHARPEN: chunk assembly %s — %ld bytes -> /data/%s\n",
                 ok ? "COMPLETE" : "WRITE FAILED",
                 (long)gDrv.assembly.size(), gDrv.sidecarRel.c_str());
    std::vector<uint8_t>().swap(gDrv.assembly);
    if (!ok)
        gDrv.chunkFallback = true;   // legacy single fetch takes over next frame
}

// Per-frame chunk pump. Runs until the sidecar is resident in MEMFS (the caller
// checks residency). At most ONE chunk in flight; a chunk is kicked only when
// no other Range fetch (i.e. the mogg's) is live.
void PumpSidecarFetchWeb() {
    const int chunkKB = RB3SharpenChunkKB();
    if (chunkKB == 0 || gDrv.chunkFallback) {
        // Legacy single whole-file fetch (pre-hardening behavior / escape hatch).
        if (!gDrv.fetchKicked) {
            KickSidecarFetch(gDrv.sidecarRel);
            gDrv.fetchKicked = true;
        } else if (WebAssetsEnsureStatus(gDrv.sidecarRel.c_str()) == 2) {
            // Fetch finished without residency (e.g. no sidecar for this venue
            // — a venue with no strippable textures emits none): clean no-op
            // instead of polling forever.
            gDrv.finished = true;
            if (RB3SharpenDriverDbg())
                MILO_LOG("RB3_SHARPEN: sidecar fetch failed/absent %s — no-op\n",
                         gDrv.sidecarRel.c_str());
        }
        return;
    }

    // One-time size query (manifest oracle; -1 today for sidecars, see above).
    if (gDrv.sidecarSize == -2) {
        gDrv.sidecarSize = WebAssetsManifestSize(gDrv.sidecarRel.c_str());
        if (RB3SharpenDriverDbg())
            MILO_LOG("RB3_SHARPEN: chunk pump start %s (manifest size %ld, chunk %d KB)\n",
                     gDrv.sidecarRel.c_str(), gDrv.sidecarSize, chunkKB);
    }

    // Poll the in-flight chunk (at most one exists).
    if (gDrv.chunkReqId != 0) {
        int gotBytes = 0;
        bool ok = false;
        if (!WebAssetsRangeDone(gDrv.chunkReqId, &gotBytes, &ok))
            return;   // still on the wire — nothing else to do this frame
        if (ok && gotBytes > 0) {
            size_t base = gDrv.assembly.size();
            // Belt-and-braces (review B1): never assemble past ReadWholeFile's
            // 64 MB ceiling — no body, however misbehaved the server, can grow
            // the wasm heap unboundedly through this path.
            if ((long)base + (long)gotBytes > kAssemblyCap) {
                WebAssetsRangeDrop(gDrv.chunkReqId);   // done → freed immediately
                gDrv.chunkReqId = 0;
                std::vector<uint8_t>().swap(gDrv.assembly);
                gDrv.chunkFallback = true;
                if (RB3SharpenDriverDbg())
                    MILO_LOG("RB3_SHARPEN: chunk assembly would exceed %ld B cap — "
                             "falling back to single fetch\n", kAssemblyCap);
                return;
            }
            gDrv.assembly.resize(base + (size_t)gotBytes);
            int taken = WebAssetsRangeTake(gDrv.chunkReqId,
                                           gDrv.assembly.data() + base, gotBytes);
            gDrv.chunkReqId = 0;   // RangeTake freed the request
            if (taken != gotBytes)
                gDrv.assembly.resize(base + (size_t)(taken > 0 ? taken : 0));
            // Review B1: a body LARGER than requested means the server IGNORED
            // Range (RFC 9110 §14.2 allows it — python -m http.server, Range-
            // stripping proxies — replying 200 with the WHOLE file). A 206 can
            // never exceed the request, so taken > chunkReqLen is a reliable
            // tell. Without this check the short-read terminator never fires
            // (taken >= reqLen, sidecarSize -1) and every retry appends another
            // whole file at the wrong offset -> unbounded heap growth.
            if (taken > gDrv.chunkReqLen) {
                gDrv.chunkErrs = 0;
                if (base == 0) {
                    // The 200 body started at offset 0, so it IS the entire
                    // file — accept it and finalize (same bytes the legacy
                    // single fetch would have landed).
                    gDrv.chunkOffset = taken;
                    if (RB3SharpenDriverDbg())
                        MILO_LOG("RB3_SHARPEN: chunk 0 got whole body (%d B > %d "
                                 "requested — server ignored Range), accepting\n",
                                 taken, gDrv.chunkReqLen);
                    FinalizeSidecarAssembly();
                } else {
                    // Whole body appended mid-assembly at the wrong offset —
                    // the assembly is garbage. Discard it and fall back
                    // permanently to the legacy single whole-file fetch (which
                    // such a server serves fine).
                    std::vector<uint8_t>().swap(gDrv.assembly);
                    gDrv.chunkFallback = true;
                    if (RB3SharpenDriverDbg())
                        MILO_LOG("RB3_SHARPEN: server ignored Range mid-assembly "
                                 "(%d B > %d requested @ %ld) — discarding, "
                                 "falling back to single fetch\n",
                                 taken, gDrv.chunkReqLen, (long)base);
                }
                return;
            }
            gDrv.chunkOffset += (taken > 0 ? taken : 0);
            gDrv.chunkErrs = 0;
            if (RB3SharpenDriverDbg())
                MILO_LOG("RB3_SHARPEN: chunk landed %d B @ %ld (total %ld)\n",
                         taken, gDrv.chunkOffset - taken, gDrv.chunkOffset);
            // Terminators: short read == server clamped at EOF; manifest size
            // (when known) reached.
            if (taken < gDrv.chunkReqLen ||
                (gDrv.sidecarSize >= 0 && gDrv.chunkOffset >= gDrv.sidecarSize))
                FinalizeSidecarAssembly();
            // Next chunk kicks next frame (paced 1/frame; yield check applies).
            return;
        }
        // Error (or empty success — treat alike). Drop and decide.
        WebAssetsRangeDrop(gDrv.chunkReqId);
        gDrv.chunkReqId = 0;
        gDrv.chunkErrs++;
        if (gDrv.chunkErrs < kChunkErrMax)
            return;   // transient? retry the same offset next frame
        if (gDrv.chunkOffset > 0) {
            // Persistent error AFTER progress: the exact-chunk-multiple EOF
            // (416 past EOF, verified) or a genuinely dead connection. Finalize
            // either way — the SHRP parser rejects a truncated blob (no-op).
            if (RB3SharpenDriverDbg())
                MILO_LOG("RB3_SHARPEN: chunk error after %ld B — treating as EOF\n",
                         gDrv.chunkOffset);
            FinalizeSidecarAssembly();
        } else {
            // Can't fetch even the first chunk (404 / dead server): permanent
            // fallback to the legacy single fetch. (A Range-STRIPPING proxy
            // never lands here — it 200s with the whole body, which the
            // taken > chunkReqLen whole-body arm above accepts directly.)
            gDrv.chunkFallback = true;
            if (RB3SharpenDriverDbg())
                MILO_LOG("RB3_SHARPEN: chunk 0 failed x%d — falling back to single fetch\n",
                         gDrv.chunkErrs);
        }
        return;
    }

    // No chunk in flight. STRICT YIELD: any live Range fetch is the mogg's
    // (ours is provably not in flight here) — don't compete, skip this frame.
    if (WebAssetsRangeInFlightCount() > 0)
        return;

    long want = (long)chunkKB * 1024;
    if (gDrv.sidecarSize >= 0) {
        long remain = gDrv.sidecarSize - gDrv.chunkOffset;
        if (remain <= 0) { FinalizeSidecarAssembly(); return; }
        if (want > remain) want = remain;
    }
    gDrv.chunkReqId = WebAssetsRangeFetch(gDrv.sidecarRel.c_str(),
                                          gDrv.chunkOffset, (int)want);
    gDrv.chunkReqLen = (int)want;
    if (gDrv.chunkReqId == 0) {
        // Immediate refusal — count it like an error at this offset.
        gDrv.chunkErrs++;
        if (gDrv.chunkErrs >= kChunkErrMax && gDrv.chunkOffset == 0)
            gDrv.chunkFallback = true;
    }
}
#endif // __EMSCRIPTEN__

} // namespace

// ---------------------------------------------------------------------------
// Per-frame driver — called from Game::Poll (HX_NATIVE arm) once gameplay is
// running (songMs > 0 && !isGameOver). Game::Poll drives BOTH the native and the
// web (RunOneFrame) loops, and computes the gate locally, so this single call
// site covers both platforms. `gameplayRunning` is that gate; we additionally
// require the engine flag ON and a downscaled venue with a sidecar. Cheap no-op
// in the menus / when disabled / once the venue's session is complete.
//
// Runs at Game::Poll time — no render pass is open there — so RB3SharpenStep
// (which drives GPU texture (re)creation via UploadRndTexIfNeeded) is safe here.
// ---------------------------------------------------------------------------
extern "C" void RB3TexSharpenPoll(bool gameplayRunning) {
    if (!RB3ProgressiveSharpenEnabled()) return;
    if (gDrv.finished) return;               // session done — nothing more to do
    if (!gameplayRunning) return;            // off the critical path only

    // --- Phase 2: once a session is loaded, just step the incremental scheduler.
    if (gDrv.started) {
        if (RB3SharpenComplete()) {
            gDrv.finished = true;
            if (RB3SharpenDriverDbg()) {
                RB3SharpenStatus st = RB3SharpenGetStatus();
                MILO_LOG("RB3_SHARPEN: COMPLETE — %d/%d textures sharpened, %llu bytes\n",
                         st.sharpened, st.matched,
                         (unsigned long long)st.bytesUpgraded);
            }
            return;
        }
        int n = RB3SharpenStep(RB3SharpenPerFrame());
        if (RB3SharpenDriverDbg() && n > 0) {
            RB3SharpenStatus st = RB3SharpenGetStatus();
            MILO_LOG("RB3_SHARPEN: stepped %d (%d/%d) this frame\n",
                     n, st.sharpened, st.matched);
        }
        return;
    }

    // --- Phase 1: resolve the venue + sidecar, fetch, then load.
    // Resolve the venue milo path the reveal actually loaded.
    if (gDrv.venueMiloPath.empty()) {
        gDrv.venueMiloPath = RB3ComputeVenueMiloPathForSharpen();
        if (gDrv.venueMiloPath.empty()) return; // not resolved yet
        gDrv.sidecarRel = SidecarRelFromVenueMilo(gDrv.venueMiloPath);
        if (gDrv.sidecarRel.empty()) { gDrv.finished = true; return; }
        if (RB3SharpenDriverDbg())
            MILO_LOG("RB3_SHARPEN: venue=%s sidecar=%s\n",
                     gDrv.venueMiloPath.c_str(), gDrv.sidecarRel.c_str());
    }

    // The venue dir must be loaded (it is — gameplay is running) so the manager can
    // walk its RndTex objects.
    FilePath venueFp(gDrv.venueMiloPath.c_str());
    DirLoader* dl = DirLoader::Find(venueFp);
    ObjectDir* venueDir = (dl && dl->IsLoaded()) ? dl->GetDir() : nullptr;
    if (!venueDir) return; // venue not resident yet (shouldn't happen post-reveal)

    // Transfer + wait for residency.
#ifdef __EMSCRIPTEN__
    // Web: chunked, mogg-yielding pump (research/14 Lane B; RB3_SHARPEN_CHUNK_KB,
    // 0 = legacy single fetch). Runs one small step per frame until the sidecar
    // is assembled + written to MEMFS (or the legacy fallback lands it).
    if (!SidecarResident(gDrv.sidecarRel)) {
        PumpSidecarFetchWeb();
        if (gDrv.finished || !SidecarResident(gDrv.sidecarRel))
            return;   // still transferring (or clean no-op) — pump again next frame
    }
#else
    // Native: the sidecar is a local file. Missing == this venue wasn't
    // downscaled → nothing to sharpen (clean no-op so we don't poll forever).
    if (!SidecarResident(gDrv.sidecarRel)) {
        gDrv.finished = true;
        if (RB3SharpenDriverDbg())
            MILO_LOG("RB3_SHARPEN: no sidecar at %s (venue not downscaled) — no-op\n",
                     gDrv.sidecarRel.c_str());
        return;
    }
#endif

    // Read the sidecar bytes and load the session.
    std::vector<uint8_t> bytes;
    if (!ReadWholeFile(gDrv.sidecarRel.c_str(), bytes) || bytes.empty()) {
        // Read failed despite "resident" — give up (don't spin).
        gDrv.finished = true;
        if (RB3SharpenDriverDbg())
            MILO_LOG("RB3_SHARPEN: failed to read sidecar %s — giving up\n",
                     gDrv.sidecarRel.c_str());
        return;
    }

    int matched = RB3SharpenLoadSidecar(venueDir, bytes.data(), (uint32_t)bytes.size());
    gDrv.started = true;
    if (matched == 0) {
        gDrv.finished = true;
        if (RB3SharpenDriverDbg())
            MILO_LOG("RB3_SHARPEN: sidecar matched 0 textures in venue %s — done\n",
                     gDrv.venueMiloPath.c_str());
        return;
    }
    if (RB3SharpenDriverDbg())
        MILO_LOG("RB3_SHARPEN: session loaded — %d textures to sharpen (%zu sidecar bytes)\n",
                 matched, bytes.size());
    // Sharpen the first batch this same frame.
    RB3SharpenStep(RB3SharpenPerFrame());
}

// Reset on song unload so the next song re-sharpens its own venue. Called from
// RB3GameWarmReset (rb3_gamewarm_native.cpp) alongside the other per-song latches.
extern "C" void RB3TexSharpenReset() {
    RB3SharpenReset();      // engine session
#ifdef __EMSCRIPTEN__
    // Release an in-flight chunk request. WebAssetsRangeDrop detaches it from
    // the registry and, if the fetch is still running, marks it abandoned so
    // the completion callback self-reclaims (the UAF-safe detach pattern —
    // see WebAssets.cpp:RangeDrop). Never leaks a registry entry.
    if (gDrv.chunkReqId != 0)
        WebAssetsRangeDrop(gDrv.chunkReqId);
#endif
    gDrv = SharpenDriver();
}

#endif // HX_NATIVE
