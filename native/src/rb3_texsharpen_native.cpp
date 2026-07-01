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

namespace {

// Per-song driver state. One sharpen session per venue load.
struct SharpenDriver {
    bool        started = false;     // a session is loaded into the engine manager
    bool        finished = false;    // RB3SharpenComplete() seen (stop polling)
    bool        fetchKicked = false; // (web) async fetch issued
    std::string venueMiloPath;       // world/venue/.../<venue>.milo (logical)
    std::string sidecarRel;          // server-relative gen/<v>.milo_xbox.sharpen
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

    // Kick the async fetch once (web); native is a no-op.
    if (!gDrv.fetchKicked) {
        KickSidecarFetch(gDrv.sidecarRel);
        gDrv.fetchKicked = true;
    }

    // Wait for residency (web async; native local file).
    if (!SidecarResident(gDrv.sidecarRel)) {
        // Missing on native == this venue wasn't downscaled → nothing to sharpen.
        // (FileExists already returned false; treat as a clean no-op so we don't
        // poll forever.) On web, keep waiting for the async fetch.
#ifndef __EMSCRIPTEN__
        gDrv.finished = true;
        if (RB3SharpenDriverDbg())
            MILO_LOG("RB3_SHARPEN: no sidecar at %s (venue not downscaled) — no-op\n",
                     gDrv.sidecarRel.c_str());
#endif
        return;
    }

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
    gDrv = SharpenDriver();
}

#endif // HX_NATIVE
