// rb3_frame_trace.cpp — lightweight per-frame stutter tracer (HX_NATIVE only).
//
// TASK A4 (audio-perf-investigation): correlate frame-time spikes to asset-load
// events. This is a SEPARATE, env-gated instrument from the older
// RB3_FRAME_INSTRUMENT MILO_LOG path in App.cpp (which only logs LONG frames to
// the engine log). This one writes a machine-readable JSONL trace of EVERY frame
// so a profiler script (scripts/native/frame_profiler.py) can build a full
// frame-time histogram + cluster spikes against the asset events that happened
// on the same frame.
//
// Activation:  RB3_FRAME_TRACE=<path>   (no env var => zero cost, never opens a
//              file; the record fn early-returns on the first call).
//
// Per-frame record fields (one JSON object per line):
//   f    frame index
//   dt   wall-clock ms for the whole RunOneFrame (the stutter metric)
//   lp   ms spent in TheLoadMgr.Poll() this frame (budgeted background loader)
//   lpu  ms spent in PollUntilLoaded/PollUntilEmpty this frame (SYNC drains —
//        the prime stutter suspect: a synchronous asset drain blocking the frame)
//   scr  current UIScreen name (where the spike happened)
//   ld   # of NEW loaders ADDED this frame (LoadMgr::AddLoader — a file load /
//        DirLoader / texture / milo mount started this frame)
//   st   # of NEW audio streams opened this frame (Synth::NewStream — song
//        preview or gameplay mogg). The big "new preview" spike marker.
//   pend # of loaders still pending in the queue at end of frame (backlog depth)
//
// Incremental-load-perf (PLAN.md T1) attribution counters — every >16ms frame
// becomes self-attributing (dt ≈ lp + lpu + the buckets below + residue):
//   fetchMs/fetchN/fetchB  blocking sync XHR (engine WebAssets, web only)
//   dtaMs   synchronous DTA parse ms. RESERVED: RB3 stubs out ThreadCall
//           (dta_link_stubs.s -> ThreadCallPoll no-op) and parses DTA inline via
//           DataReadFile/yylex in src/system/obj/DataFile.cpp (NOT the engine's
//           DC3-shaped ThreadCall_Native.cpp, which RB3 excludes). DataFile.cpp
//           is outside T1's file ownership, so this stays 0 until a later wave
//           wraps DataReadFile — the field + reset are wired so that's a 2-line add.
//   objMs   per-object PreLoad+PostLoad ms (DirLoader). Most DTA-as-object parse
//           cost (config/script .dta loaded as DataArray objects) shows up here.
//   objWMs / objWNm   slowest single object this frame + its "class:name"
//   primeMs StandardStream::Play Vorbis prime pump ms
//   texMs/texN   UploadRndTexIfNeeded decode+upload ms / count
//   meshMs/meshN DrawMesh needUpload VB/IB write ms / count
//   pipeMs/pipeN PipelineManager::GetPipeline cache-miss compile ms / count
//   inflMs  ChunkStream inflate (zlib/lzx byte-shovel) ms
//
// The event/timer counters are incremented from the cheapest reliable choke
// points in the shared engine + rb3 fork, behind a single global-bool gate
// (gFrameTraceActive) so they cost one predicted-not-taken branch when tracing
// is off. They are read + zeroed by the recorder after each frame is written.

#ifdef HX_NATIVE

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// Event counters — DEFINED in src/system/utl/Loader.cpp (always linked into any
// target that compiles the engine TUs that increment them — rb3-native AND the
// minimal rb3-dta tool). We only `extern` them here. The increments live behind
// the gFrameTraceActive bool so the hooks are a single predicted-not-taken
// branch when tracing is disabled.
// ---------------------------------------------------------------------------
extern bool gFrameTraceActive;
extern int  gFrameTraceLoaderAdds;
extern int  gFrameTraceStreamOpens;

// Incremental-load-perf (PLAN.md T1) per-frame attribution counters. Strong
// defs live in src/system/utl/Loader.cpp (engine TUs carry matching weak defs);
// we only extern them here. All gated by gFrameTraceActive like the above.
extern float  gFetchSyncMsThisFrame;
extern int    gFetchSyncCountThisFrame;
extern double gFetchSyncBytesThisFrame;
extern float  gDtaParseMsThisFrame;
extern float  gObjLoadMsThisFrame;
extern float  gObjLoadWorstMs;
extern char   gObjLoadWorstName[64];
extern float  gAudioPrimeMsThisFrame;
extern float  gTexUploadMsThisFrame;
extern int    gTexUploadCountThisFrame;
extern float  gMeshUploadMsThisFrame;
extern int    gMeshUploadCountThisFrame;
extern float  gPipelineCreateMsThisFrame;
extern int    gPipelineCreateCountThisFrame;
extern float  gStreamReadMsThisFrame;

namespace {
FILE *sTraceFile = nullptr;
int   sTraceState = 0;               // 0=unchecked, 1=open, -1=disabled
}

// Called once per frame from App::RunWithoutDebugging (after RunOneFrame).
// All heavy values are passed in by the caller (already computed for the
// existing instrument), so this fn only formats + writes a line.
void RB3FrameTraceRecord(int frame, float dtMs, float loadPollMs,
                         float loadPollUntilMs, const char *screen,
                         int pendingLoaders) {
    if (sTraceState == 0) {
        const char *path = getenv("RB3_FRAME_TRACE");
        if (path && path[0]) {
            // Ensure the parent directory exists so fopen("w") doesn't fail
            // with ENOENT on MEMFS when the path is e.g. /tmp/rb3/trace.jsonl.
            // For the common /trace.jsonl case the parent is "/" which always
            // exists; this is a no-op. For deeper paths it creates only the
            // immediate parent (one level — callers use simple paths).
            {
                char parentBuf[512];
                strncpy(parentBuf, path, sizeof(parentBuf) - 1);
                parentBuf[sizeof(parentBuf) - 1] = '\0';
                char *slash = strrchr(parentBuf, '/');
                if (slash && slash != parentBuf) {
                    *slash = '\0';
                    mkdir(parentBuf, 0777);  // ignore EEXIST
                }
            }
            sTraceFile = fopen(path, "w");
            if (sTraceFile) {
                sTraceState = 1;
                gFrameTraceActive = true;
                // JSONL — one object per line. Header comment line (JSON-array
                // parsers skip it; our parser is line-oriented and ignores '#').
                fprintf(sTraceFile,
                        "# rb3 frame trace: f=frame dt=frameMs lp=loadPollMs "
                        "lpu=pollUntilMs scr=screen ld=loaderAdds st=streamOpens "
                        "pend=pendingLoaders fetchMs/fetchN/fetchB dtaMs objMs "
                        "objWMs/objWNm primeMs texMs/texN meshMs/meshN "
                        "pipeMs/pipeN inflMs\n");
                fflush(sTraceFile);
            } else {
                sTraceState = -1;   // open failed; never retry
            }
        } else {
            sTraceState = -1;       // not requested
        }
    }
    if (sTraceState != 1) {
        // Tracing off: still reset the per-frame counters so they don't grow.
        gFrameTraceLoaderAdds = 0;
        gFrameTraceStreamOpens = 0;
        return;
    }

    if (!screen) screen = "?";
    // Sanitize the worst-object name for JSON (it's "class:name" from arbitrary
    // asset data — a stray '"' or '\\' would corrupt the line). Copy, replacing
    // those two chars with '_'; everything else in class/object names is JSON-safe.
    char wname[64];
    for (int i = 0; i < (int)sizeof(wname) - 1; i++) {
        char c = gObjLoadWorstName[i];
        if (c == '"' || c == '\\') c = '_';
        wname[i] = c;
        if (c == '\0') break;
    }
    wname[sizeof(wname) - 1] = '\0';

    fprintf(sTraceFile,
            "{\"f\":%d,\"dt\":%.3f,\"lp\":%.3f,\"lpu\":%.3f,"
            "\"scr\":\"%s\",\"ld\":%d,\"st\":%d,\"pend\":%d,"
            "\"fetchMs\":%.3f,\"fetchN\":%d,\"fetchB\":%.0f,"
            "\"dtaMs\":%.3f,\"objMs\":%.3f,\"objWMs\":%.3f,\"objWNm\":\"%s\","
            "\"primeMs\":%.3f,\"texMs\":%.3f,\"texN\":%d,"
            "\"meshMs\":%.3f,\"meshN\":%d,\"pipeMs\":%.3f,\"pipeN\":%d,"
            "\"inflMs\":%.3f}\n",
            frame, dtMs, loadPollMs, loadPollUntilMs, screen,
            gFrameTraceLoaderAdds, gFrameTraceStreamOpens, pendingLoaders,
            gFetchSyncMsThisFrame, gFetchSyncCountThisFrame, gFetchSyncBytesThisFrame,
            gDtaParseMsThisFrame, gObjLoadMsThisFrame, gObjLoadWorstMs, wname,
            gAudioPrimeMsThisFrame, gTexUploadMsThisFrame, gTexUploadCountThisFrame,
            gMeshUploadMsThisFrame, gMeshUploadCountThisFrame,
            gPipelineCreateMsThisFrame, gPipelineCreateCountThisFrame,
            gStreamReadMsThisFrame);
    // Flush each frame so a SIGTERM mid-run still leaves a complete trace.
    fflush(sTraceFile);

    gFrameTraceLoaderAdds = 0;
    gFrameTraceStreamOpens = 0;
    gFetchSyncMsThisFrame = 0.0f;
    gFetchSyncCountThisFrame = 0;
    gFetchSyncBytesThisFrame = 0.0;
    gDtaParseMsThisFrame = 0.0f;
    gObjLoadMsThisFrame = 0.0f;
    gObjLoadWorstMs = 0.0f;
    gObjLoadWorstName[0] = '\0';
    gAudioPrimeMsThisFrame = 0.0f;
    gTexUploadMsThisFrame = 0.0f;
    gTexUploadCountThisFrame = 0;
    gMeshUploadMsThisFrame = 0.0f;
    gMeshUploadCountThisFrame = 0;
    gPipelineCreateMsThisFrame = 0.0f;
    gPipelineCreateCountThisFrame = 0;
    gStreamReadMsThisFrame = 0.0f;
}

#endif // HX_NATIVE
