// rb3 native — loading-dwell GPU warm driver + reveal-frame drain pre-kick.
//
// (incremental-load-perf Wave 5, task T2 — see
//  docs/native/incremental-load-perf/research/09-gamescreen-firstframe-plan.md)
//
// THE PROBLEM (research/09 §1). When the loading vignette (tv3_*) finally flips
// to game_screen, that single "reveal" RunOneFrame does ALL of the venue/track/
// char world's structural first-frame work synchronously:
//   (a) a final synchronous dir drain — native trace shows lpu~=52 ms via
//       PollUntilLoaded (a ForceGetLoader-shaped drain), pend 0->1 that frame,
//       whose per-object PostLoad is venue textures (objMs~=39 ms, worst object
//       RndTex:floor_wood02_NORM.tex), then
//   (b) the venue/track/char world's FIRST DRAW: ~97 RndTex uploads
//       (texN=97) + ~113 mesh VB/IB creates + the full CPU vertex unpack of all
//       113 meshes.
// Native reveal dt ~= 108 ms (3x the 33 ms budget on an -O2 host); web -O0 the
// same structural work balloons to ~0.7-1.0 s of game-content freeze.
//
// THE FIX (research/09 §2, levers L2 + L3). The vignette dwell that precedes the
// reveal is measured ~idle (native ~1710 frames p50 4.2 ms = ~21 s headroom; web
// ~236 frames p50 5.7 ms = ~2.2 s; it GROWS at low bandwidth where the venue milo
// is still downloading). So move the reveal frame's GPU work INTO the dwell:
//
//   L2 — once UIPanel::IsLoaded() is true (the song + venue milos have parsed),
//   sweep the gameplay dir roots through BandRnd::WarmGpuForDir(root, <=8 ms),
//   which walks ObjDirItr<RndTex>/<RndMesh> (incl. subdirs) and pushes every
//   not-yet-resident texture/mesh through the SAME upload paths DrawMesh uses
//   (UploadRndTexIfNeeded + the shared unpack + VB/IB upload, writing the same
//   sMeshGpu/sTexGpu cache entries with the same keys). The reveal frame's draw
//   then hits the cache instead of paying the upload + CPU-unpack stall.
//   Uploading is a VISUAL NO-OP by construction (no draws, no state changes —
//   same buffers the first draw would create one frame later). We HOLD
//   GamePanel::IsLoaded() false while the sweep still reports work (bounded by a
//   ~2 s max-hold safety so a never-draining dir can't wedge the transition).
//
//   L3 — the reveal frame still synchronously creates+drains ONE loader
//   (Dir::SyncSubDir -> LoadMgr::ForceGetLoader at the venue/world proxy sync on
//   Enter; native lpu~=52 ms). We CAPTURE that FilePath during the dwell (the
//   loader is added on the reveal frame; we observe it via a file-open probe and
//   via the world_panel proxy file) and PRE-KICK it async (AddLoader kLoadFront)
//   so by the time Enter's ForceGetLoader runs, GetLoader(fp) finds it already
//   loaded and PollUntilLoaded no-ops.
//
// FLAG. RB3_GAMEWARM_OFF=1 disables BOTH levers (warm sweep + pre-kick); the
// transition then behaves exactly as before (reveal pays the full stall). Default
// ON, native + web. getenv-once static. Diagnostics: RB3_GAMEWARM_DBG=1 logs the
// per-frame sweep counts + the captured drain FilePath.
//
// SCOPE / OWNERSHIP. This TU is native+web glue (HX_NATIVE / HX_WEB both define
// the engine-port path). The matched-TU GamePanel.cpp arms are all inside
// #ifdef HX_NATIVE (Wii byte-identical). All work here reuses existing public
// engine entry points: BandRnd::WarmGpuForDir (engine T1) and the LoadMgr loader
// API. No render-pass is open during the dwell poll, so WarmGpuForDir is safe to
// call (its contract requires GPU-ready + outside an open pass).

#include "platform/Rnd_Wgpu_RB3.h"   // gBandRnd, BandRnd::WarmGpuForDir
#include "obj/Dir.h"                  // ObjectDir, ObjDirItr
#include "obj/DirLoader.h"           // DirLoader::Find / GetDir (resident venue dir)
#include "obj/Object.h"
#include "rndobj/Tex.h"              // RndTex (warm-coverage probe)
#include "rndobj/Mesh.h"            // RndMesh (warm-coverage probe)
#include "ui/UI.h"                    // TheUI, UIScreen
#include "ui/UIPanel.h"              // UIPanel::LoadedDir
#include "utl/Loader.h"              // TheLoadMgr, LoadMgr, kLoadFront
#include "utl/FilePath.h"
#include "os/Debug.h"               // MILO_LOG
#include "os/Timer.h"               // Timer (wall clock for the max-hold safety)

#include <list>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Flag + dbg (getenv-once static, house style).
// ---------------------------------------------------------------------------
static bool RB3GameWarmEnabled() {
    static int s = -1;
    if (s < 0) s = (getenv("RB3_GAMEWARM_OFF") != nullptr) ? 0 : 1;
    return s != 0;
}
static bool RB3GameWarmDbg() {
    static int s = -1;
    if (s < 0) s = (getenv("RB3_GAMEWARM_DBG") != nullptr) ? 1 : 0;
    return s != 0;
}

// Per-call warm budget (ms). The vignette runs on ~5 ms frames, so 8 ms of warm
// work keeps the dwell frame under the 16.6 ms / 60 fps line with margin while
// still draining the ~210 (97 tex + 113 mesh) resident-uploads in a handful of
// frames. Overridable for tuning.
static float RB3GameWarmBudgetMs() {
    static float ms = -1.0f;
    if (ms < 0.0f) {
        const char* e = getenv("RB3_GAMEWARM_BUDGET_MS");
        ms = e ? (float)atof(e) : 8.0f;
        if (ms <= 0.0f) ms = 8.0f;
    }
    return ms;
}

// Max wall time (ms) we will hold IsLoaded() false while the sweep drains, so a
// never-draining root (or a WarmGpuForDir that always reports > 0) can never
// wedge the meta->game transition. ~2 s default (research/09 budget).
static float RB3GameWarmMaxHoldMs() {
    static float ms = -1.0f;
    if (ms < 0.0f) {
        const char* e = getenv("RB3_GAMEWARM_MAXHOLD_MS");
        ms = e ? (float)atof(e) : 2000.0f;
        if (ms <= 0.0f) ms = 2000.0f;
    }
    return ms;
}

// How many CONSECUTIVE idle (uploaded==0) dwell frames we tolerate before
// releasing the hold. The warm can only pre-upload resources that are RESIDENT at
// poll time; in flows where the venue/track geometry is loaded by the reveal
// frame's own Enter drain (not earlier), the sweep legitimately finds nothing to
// warm. Rather than block the transition pointlessly (delaying audio-start), we
// release after this many idle frames once no pre-kicked load is still in flight.
// Small enough to not stall, large enough to span a kicked load completing and
// its resources becoming walkable.
static int RB3GameWarmIdleReleaseFrames() {
    static int n = -1;
    if (n < 0) {
        const char* e = getenv("RB3_GAMEWARM_IDLE_FRAMES");
        n = e ? atoi(e) : 4;
        if (n < 1) n = 1;
    }
    return n;
}

// Whether to HOLD the meta->game transition (return IsLoaded()==false) while the
// dwell warm/pre-kick runs. DEFAULT OFF: in the measured native flow the venue
// (per-song arena_*) geometry + the post-sync track-draw INSTANCES are created by
// the reveal frame's own Enter drain — NOT resident at the warm point — so the
// warm uploads different object instances than the reveal draws (cache miss) and
// holding only delays audio-start for no reveal-frame win (Enter-state-dependent,
// research/09 §2-L3 fallback). With the hold OFF we still run the opportunistic
// warm + L3 pre-kick across whatever natural dwell frames occur (timing-neutral,
// no audio-start delay). RB3_GAMEWARM_HOLD=1 re-enables the blocking hold for
// flows where the gameplay dirs ARE resident during the dwell (e.g. a future web
// path with a longer venue-resident dwell) or for experimentation.
static bool RB3GameWarmHoldEnabled() {
    static int s = -1;
    if (s < 0) s = (getenv("RB3_GAMEWARM_HOLD") != nullptr) ? 1 : 0;
    return s != 0;
}

// ---------------------------------------------------------------------------
// Dwell-state (one per meta->game transition; reset when the vignette re-arms).
// ---------------------------------------------------------------------------
namespace {
struct GameWarmState {
    bool   active = false;       // sweep has been armed (UIPanel::IsLoaded seen)
    bool   drained = false;      // WarmGpuForDir returned 0 (fully warm)
    int    frames = 0;           // dwell frames we have swept
    int    totalUploaded = 0;    // cumulative #resources warmed
    int    idleFrames = 0;       // consecutive frames with uploadedThisFrame == 0
    double startMs = 0.0;        // wall clock when the sweep armed (max-hold)
    bool   prekicked = false;    // L3: the drain FilePath has been kicked
};
GameWarmState gWarm;

// Captured candidate drain FilePaths (L3). The reveal-frame drain is the
// world/venue proxy dir synced at Enter (Dir::SyncSubDir -> ForceGetLoader). We
// collect the proxy files of the gameplay dir roots during the dwell and kick
// them async so the Enter drain no-ops. A small set (typically 1-3 paths).
std::vector<std::string> gPrekickPaths;
std::vector<std::string> gKickedPaths;  // paths already AddLoader'd (dedup)

// Monotonic process-relative wall clock in ms (for the max-hold safety). A single
// static Timer is started once and accumulates via Split(); mCycles is the total
// elapsed cycles, which CyclesToMs converts to ms.
double NowMs() {
    static Timer sBase;
    static bool sInit = false;
    if (!sInit) { sBase.Restart(); sInit = true; }
    sBase.Split();
    return (double)Timer::CyclesToMs(sBase.mCycles);
}
} // namespace

// ---------------------------------------------------------------------------
// L3 — pre-kick: collect a dir's proxy/source FilePath so Enter's ForceGetLoader
// finds it already loaded. ObjectDir exposes its backing file via Dir() path
// machinery; the world panel's PanelDir is loaded from a proxy .milo. We kick the
// non-empty file of each gameplay root.
// ---------------------------------------------------------------------------
static void CollectPrekick(ObjectDir* dir) {
    if (!dir) return;
    // ObjectDir::GetPathName() returns the dir's source file (set by DirLoader),
    // as a C string. If non-empty and not already queued, remember it for the
    // async kick.
    const char* c = dir->GetPathName();
    if (!c || !c[0]) return;
    for (size_t i = 0; i < gPrekickPaths.size(); ++i)
        if (gPrekickPaths[i] == c) return;
    gPrekickPaths.push_back(c);
}

// Kick any collected pre-kick path that doesn't already have a loader, marking it
// kicked so we never AddLoader the same file twice. Called incrementally during
// the dwell (DoPrekickEarly) AND a final time at drain (DoPrekick) — idempotent.
static void KickPending() {
    for (size_t i = 0; i < gPrekickPaths.size(); ++i) {
        const std::string& path = gPrekickPaths[i];
        bool already = false;
        for (size_t k = 0; k < gKickedPaths.size(); ++k)
            if (gKickedPaths[k] == path) { already = true; break; }
        if (already) continue;
        gKickedPaths.push_back(path);
        FilePath fp(path.c_str());
        if (fp.empty()) continue;
        // AddLoader(kLoadFront) starts an async load the dwell's LoadMgr.Poll()
        // drives; when Enter's ForceGetLoader runs, GetLoader(fp) finds it resident
        // -> PollUntilLoaded no-ops. Skip if a loader already exists (queued).
        if (!TheLoadMgr.GetLoader(fp)) {
            TheLoadMgr.AddLoader(fp, kLoadFront);
            if (RB3GameWarmDbg())
                MILO_LOG("RB3_GAMEWARM: L3 pre-kick AddLoader(kLoadFront) %s\n", fp.c_str());
        } else if (RB3GameWarmDbg()) {
            MILO_LOG("RB3_GAMEWARM: L3 %s already has a loader (no kick)\n", fp.c_str());
        }
    }
}
// Incremental kick during the dwell (new proxy files discovered as subdirs sync).
static void DoPrekickEarly() { KickPending(); }

// True while ANY pre-kicked proxy file still has an in-flight (not-yet-loaded)
// loader. We keep holding the vignette while these drain so the resources they
// bring resident can then be warmed (and so Enter's ForceGetLoader finds them
// loaded -> drain no-ops). Returns false once every kicked file is resident
// (GetLoader gone OR its loader reports IsLoaded()).
static bool AnyKickedStillLoading() {
    for (size_t i = 0; i < gKickedPaths.size(); ++i) {
        FilePath fp(gKickedPaths[i].c_str());
        if (fp.empty()) continue;
        Loader* ldr = TheLoadMgr.GetLoader(fp);
        if (ldr && !ldr->IsLoaded())
            return true;
    }
    return false;
}
// Final kick at drain (catch any path discovered on the last frame).
static void DoPrekick() {
    if (gWarm.prekicked) return;
    gWarm.prekicked = true;
    KickPending();
}

// ---------------------------------------------------------------------------
// File-open capture probe (RB3_GAMEWARM_PROBE): hook LoadMgr::sFileOpenCallback
// to log every file added after the warm arms. Reveals the exact FilePath the
// reveal-frame Enter drain loads (the venue milo), which is otherwise invisible
// (its dir doesn't exist at warm time). Diagnostic only — not part of the
// shipped path. Captured paths are also auto-added to the pre-kick set.
// ---------------------------------------------------------------------------
static bool RB3GameWarmProbe() {
    static int s = -1;
    if (s < 0) s = (getenv("RB3_GAMEWARM_PROBE") != nullptr) ? 1 : 0;
    return s != 0;
}
static void (*sPrevFileOpenCb)(const char*) = nullptr;
static bool sProbeInstalled = false;
static void GameWarmFileOpenProbe(const char* file) {
    if (file && file[0])
        MILO_LOG("RB3_GAMEWARM_PROBE: file opened: %s\n", file);
    if (sPrevFileOpenCb) sPrevFileOpenCb(file);
}

// ---------------------------------------------------------------------------
// Coverage probe (RB3_GAMEWARM_DBG): count walkable RndTex/RndMesh under a root
// so we can confirm the dir actually contains the venue resources at warm time
// (if it shows 0/0 the venue is still a not-yet-synced proxy subdir — L3 must
// kick its proxy file first). Recursive (incl. resident subdirs).
// ---------------------------------------------------------------------------
static void ProbeRootCounts(ObjectDir* dir, const char* label) {
    if (!dir) { MILO_LOG("RB3_GAMEWARM:   %s = (null)\n", label); return; }
    int nt = 0, nm = 0, nsub = 0;
    for (ObjDirItr<RndTex> it(dir, true); it != nullptr; ++it) nt++;
    for (ObjDirItr<RndMesh> it(dir, true); it != nullptr; ++it) nm++;
    // Count proxy subdirs (their geometry loads at Enter via SyncSubDir) so we can
    // tell whether the venue is still behind a proxy.
    for (ObjDirItr<ObjectDir> it(dir, true); it != nullptr; ++it)
        if (it->IsProxy() && !it->ProxyFile().empty()) nsub++;
    MILO_LOG("RB3_GAMEWARM:   %s: tex=%d mesh=%d proxySubdirs=%d path=%s\n",
             label, nt, nm, nsub, dir->GetPathName() ? dir->GetPathName() : "");
}

// ---------------------------------------------------------------------------
// L3 (recursive): collect the proxy FilePath of every proxy subdir under a root.
// The venue/world geometry is delivered as a proxy subdir that Dir::SyncSubDir
// (-> ForceGetLoader) drains synchronously on Enter (the reveal-frame lpu). If we
// AddLoader(kLoadFront) those proxy files during the idle dwell, they load async
// and Enter's drain finds them resident (no-op). Bounded depth via ObjDirItr's
// own subdir recursion.
// ---------------------------------------------------------------------------
static void CollectProxySubdirs(ObjectDir* dir) {
    if (!dir) return;
    for (ObjDirItr<ObjectDir> it(dir, true); it != nullptr; ++it) {
        ObjectDir* sub = it;
        if (!sub || !sub->IsProxy()) continue;
        FilePath& pf = sub->ProxyFile();
        if (pf.empty()) continue;
        const char* c = pf.c_str();
        if (!c || !c[0]) continue;
        bool dup = false;
        for (size_t i = 0; i < gPrekickPaths.size(); ++i)
            if (gPrekickPaths[i] == c) { dup = true; break; }
        if (!dup) gPrekickPaths.push_back(c);
    }
}

// ---------------------------------------------------------------------------
// L2 — the per-frame dwell sweep. Returns true while the warm is still draining
// (caller holds IsLoaded() false); false when fully warm OR the max-hold safety
// fired OR the flag is off OR the GPU isn't ready.
//
// `selfDir`   — GamePanel's own PanelDir (LoadedDir()).
// `trackDir`  — the TrackPanel dir (GetTrackPanelDir()), or null.
// The world/venue dir is found via the world_panel UIPanel in the main dir.
// ---------------------------------------------------------------------------
extern "C" bool RB3GameWarmPollDwell(ObjectDir* selfDir, ObjectDir* trackDir) {
    if (!RB3GameWarmEnabled()) return false;
    // The warm sweep uploads through wgpu; only meaningful once the device is up.
    if (!gBandRnd.mGpuReady) return false;
    // Never sweep with an open render pass (WarmGpuForDir contract). The dwell
    // poll runs from GamePanel::PollForLoading, which is well outside BeginFrame/
    // EndFrame, so InPass() is false here — assert-guard anyway.
    if (gBandRnd.InPass()) return false;

    if (!gWarm.active) {
        gWarm.active = true;
        gWarm.drained = false;
        gWarm.frames = 0;
        gWarm.totalUploaded = 0;
        gWarm.idleFrames = 0;
        gWarm.startMs = NowMs();
        gWarm.prekicked = false;
        gPrekickPaths.clear();
        gKickedPaths.clear();
        if (RB3GameWarmProbe() && !sProbeInstalled) {
            sPrevFileOpenCb = LoadMgr::sFileOpenCallback;
            LoadMgr::sFileOpenCallback = GameWarmFileOpenProbe;
            sProbeInstalled = true;
        }
        if (RB3GameWarmDbg())
            MILO_LOG("RB3_GAMEWARM: armed (UIPanel::IsLoaded) — sweeping gameplay roots\n");
    }

    // DEFAULT PATH = TRUE NO-OP. With the blocking hold OFF (the shipped default),
    // BOTH levers are inert: the L2 warm sweep would upload object instances the
    // reveal frame doesn't draw (cache miss — Enter-state-dependent, see the
    // RB3GameWarmHoldEnabled() comment), and the L3 pre-kick re-AddLoader()s the
    // gameplay roots' proxy subdirs (trackpanel/chars/world subdirs) that are
    // ALREADY resident inside their parent dirs — TheLoadMgr.GetLoader(fp)
    // under-detects them (an inline-loaded proxy subdir has no standalone top-level
    // loader), so kLoadFront re-downloads + re-parses each one at the meta->game
    // transition. Measured: 23 redundant kicks → ~173 extra milo re-parse notifies
    // vs the OFF baseline, with the venue arena milo (the only thing the reveal
    // frame actually pays for) never on the kicked path → ZERO benefit, pure waste
    // on a saturated web pipe. So with the hold disabled we do NOTHING but mark the
    // dwell drained and release immediately — the transition behaves exactly as
    // RB3_GAMEWARM_OFF (no collect, no kick, no warm). Both levers are gated behind
    // RB3_GAMEWARM_HOLD=1 for flows where the gameplay dirs ARE resident across a
    // real dwell (web / future) or for experimentation.
    if (!RB3GameWarmHoldEnabled()) {
        gWarm.drained = true;
        if (RB3GameWarmDbg())
            MILO_LOG("RB3_GAMEWARM: hold disabled — no-op (no warm, no pre-kick), "
                     "releasing immediately\n");
        return false;
    }

    // Max-hold safety — never block the transition longer than the budget.
    double elapsed = NowMs() - gWarm.startMs;
    if (elapsed > RB3GameWarmMaxHoldMs()) {
        if (!gWarm.drained && RB3GameWarmDbg())
            MILO_LOG("RB3_GAMEWARM: max-hold (%.0f ms) hit after %d frames, "
                     "uploaded=%d — releasing\n",
                     elapsed, gWarm.frames, gWarm.totalUploaded);
        gWarm.drained = true;
        DoPrekick();
        return false;
    }

    if (gWarm.drained) return false;

    // Gather the gameplay dir roots. Coverage is verified empirically by the
    // reveal-frame texN/meshN dropping to ~0 (research/09 acceptance).
    //
    // Subtlety (native flow): at the warm point the venue geometry is NOT merged
    // into the walkable sMainDir/world_panel tree yet — it is loaded as the
    // `world/world.milo` DirLoader (resident) whose dir holds the venue, and the
    // per-song venue subdir is synced at Enter (the reveal lpu). So beyond the
    // panel dirs we ALSO add the resident world/track DirLoader dirs directly
    // (DirLoader::Find(fp)->GetDir()), reaching the venue resources before they're
    // merged. Each is warmed + its proxy subdirs pre-kicked.
    static const char* kResidentDirMilos[] = {
        "world/world.milo",            // venue base (+ per-song venue proxy subdir)
        "world/shared/director.milo",  // venue director / lighting
        "world/shared/chars.milo",     // band character meshes
        "ui/track/tracksystem_meshes.milo",
        "ui/track/gem_smasher_guitar_meshes.milo",
        "ui/track/gem_smasher_drum_meshes.milo",
        "ui/track/key_smasher_meshes.milo",
    };
    enum { kMaxRoots = 3 + (int)(sizeof(kResidentDirMilos)/sizeof(kResidentDirMilos[0])) };
    ObjectDir* roots[kMaxRoots] = { selfDir, trackDir, nullptr };
    if (UIPanel* worldPanel = ObjectDir::sMainDir
            ? ObjectDir::sMainDir->Find<UIPanel>("world_panel", true)
            : nullptr) {
        roots[2] = worldPanel->LoadedDir();
    }
    {
        int r = 3;
        for (size_t i = 0; i < sizeof(kResidentDirMilos)/sizeof(kResidentDirMilos[0]); ++i) {
            FilePath fp(kResidentDirMilos[i]);
            DirLoader* dl = DirLoader::Find(fp);
            if (dl && dl->IsLoaded() && r < kMaxRoots)
                roots[r++] = dl->GetDir();
        }
        while (r < kMaxRoots) roots[r++] = nullptr;
    }

    // First dwell frame: dump per-root coverage (dbg) so we can confirm the venue
    // resources are walkable here (vs still behind a not-yet-synced proxy subdir).
    if (gWarm.frames == 0 && RB3GameWarmDbg()) {
        ProbeRootCounts(roots[0], "gamePanelDir");
        ProbeRootCounts(roots[1], "trackPanelDir");
        ProbeRootCounts(roots[2], "worldDir");
        for (int i = 3; i < kMaxRoots; i++)
            if (roots[i]) ProbeRootCounts(roots[i], kResidentDirMilos[i - 3]);
        // Where do the reveal-frame venue textures actually live? Scan sMainDir's
        // immediate subdirs for a known venue texture so we can target the right
        // root. (PROBE-only — finds the dir to add to `roots`.)
        if (RB3GameWarmProbe() && ObjectDir::sMainDir) {
            for (ObjDirItr<ObjectDir> it(ObjectDir::sMainDir, true); it != nullptr; ++it) {
                int vt = 0;
                for (ObjDirItr<RndTex> t(it, false); t != nullptr; ++t) {
                    const char* tn = t->Name();
                    if (tn && (std::strstr(tn, "floor_wood02") || std::strstr(tn, "wainscoat")))
                        vt++;
                }
                if (vt > 0)
                    MILO_LOG("RB3_GAMEWARM_PROBE: venue tex found in dir '%s' (path=%s) count=%d\n",
                             it->Name() ? it->Name() : "?",
                             it->GetPathName() ? it->GetPathName() : "", vt);
            }
            // Dump the loader queue: is a venue/world loader already QUEUED (added
            // earlier) but not yet drained? If so we can drain it during the dwell.
            for (std::list<Loader*>::iterator li = TheLoadMgr.mLoaders.begin();
                 li != TheLoadMgr.mLoaders.end(); ++li) {
                Loader* ldr = *li;
                if (!ldr) continue;
                const char* fpc = ldr->mFile.c_str();
                MILO_LOG("RB3_GAMEWARM_PROBE: queued loader: %s loaded=%d\n",
                         fpc ? fpc : "?", (int)ldr->IsLoaded());
            }
        }
    }

    // L3: collect proxy-subdir files (the venue geometry is delivered as a proxy
    // subdir that Enter's SyncSubDir drains synchronously — the reveal lpu). Kick
    // them async NOW so they load during the remaining dwell and Enter's
    // ForceGetLoader finds them resident (drain no-ops).
    for (int i = 0; i < kMaxRoots; i++) {
        if (!roots[i]) continue;
        CollectPrekick(roots[i]);          // the root's own source file
        CollectProxySubdirs(roots[i]);     // + every proxy subdir's proxy file
    }
    DoPrekickEarly();                       // start the async loads (idempotent)

    // Split the per-frame budget across the roots (each WarmGpuForDir spends up to
    // its share, returns #uploaded this call; 0 == that root fully warm).
    int nRoots = 0;
    for (int i = 0; i < kMaxRoots; i++) if (roots[i]) nRoots++;
    float budgetEach = RB3GameWarmBudgetMs() / (nRoots > 0 ? (float)nRoots : 1.0f);

    // Run the GPU warm sweep. We only reach here with RB3_GAMEWARM_HOLD=1 (the
    // default no-op path returned above); the warm only makes sense when the
    // gameplay dirs are resident across a real dwell (web / future / experiment).
    int uploadedThisFrame = 0;
    for (int i = 0; i < kMaxRoots; i++) {
        if (!roots[i]) continue;
        uploadedThisFrame += gBandRnd.WarmGpuForDir(roots[i], budgetEach);
    }

    gWarm.frames++;
    gWarm.totalUploaded += uploadedThisFrame;
    if (uploadedThisFrame > 0) gWarm.idleFrames = 0;
    else                       gWarm.idleFrames++;

    // The venue/track geometry can be delivered behind proxy subdirs that aren't
    // resident at warm time (the reveal-frame Enter drain syncs them). A warm pass
    // reporting uploaded==0 can mean either "fully warm" OR "nothing resident YET".
    // We keep holding while we're (a) actively uploading, or (b) waiting on a
    // pre-kicked proxy load that may yet yield resources to warm. But if neither
    // holds for RB3GameWarmIdleReleaseFrames consecutive frames, we RELEASE —
    // the warm cannot pre-upload resources the Enter drain itself loads, and
    // blocking the transition further would only delay audio-start for no gain
    // (the reveal then pays its residual; documented Enter-state-dependent case).
    bool kickedLoading = AnyKickedStillLoading();

    if (RB3GameWarmDbg() && (gWarm.frames <= 3 || (gWarm.frames % 20) == 0))
        MILO_LOG("RB3_GAMEWARM: dwell frame %d roots=%d uploaded=%d (total=%d, "
                 "idle=%d, kickedLoading=%d, %.0f ms)\n",
                 gWarm.frames, nRoots, uploadedThisFrame, gWarm.totalUploaded,
                 gWarm.idleFrames, (int)kickedLoading, elapsed);

    bool fullyWarm = (uploadedThisFrame == 0 && !kickedLoading);
    bool idleGiveUp = (gWarm.idleFrames >= RB3GameWarmIdleReleaseFrames() && !kickedLoading);
    if (fullyWarm || idleGiveUp) {
        gWarm.drained = true;
        DoPrekick();
        if (RB3GameWarmDbg())
            MILO_LOG("RB3_GAMEWARM: %s after %d frames, uploaded=%d (%.0f ms) — "
                     "releasing IsLoaded() hold\n",
                     fullyWarm ? "DRAINED" : "IDLE-RELEASE",
                     gWarm.frames, gWarm.totalUploaded, elapsed);
        return false;
    }

    // Still uploading (or waiting on a kicked load) — hold one more dwell frame.
    return true;
}

// True while the warm sweep is actively holding the meta->game transition. Read
// from GamePanel::IsLoaded() to keep it false until the dwell drains. Cheap: no
// GPU work, just the latched state.
extern "C" bool RB3GameWarmShouldHoldLoaded() {
    if (!RB3GameWarmEnabled()) return false;
    return gWarm.active && !gWarm.drained;
}

// Reset the dwell state when the GamePanel unloads / a new song arms. Called from
// GamePanel::Unload() so a second song re-warms cleanly.
extern "C" void RB3GameWarmReset() {
    gWarm = GameWarmState();
    gPrekickPaths.clear();
    gKickedPaths.clear();
}
