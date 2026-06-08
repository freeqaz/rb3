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
// The two event counters (ld/st) are incremented from the cheapest reliable
// choke points in the shared engine (LoadMgr::AddLoader, Synth::NewStream),
// behind a single global-bool gate so they cost one branch when tracing is off.
// They are zeroed by the recorder after each frame is written.

#ifdef HX_NATIVE

#include <cstdio>
#include <cstdlib>

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
            sTraceFile = fopen(path, "w");
            if (sTraceFile) {
                sTraceState = 1;
                gFrameTraceActive = true;
                // JSONL — one object per line. Header comment line (JSON-array
                // parsers skip it; our parser is line-oriented and ignores '#').
                fprintf(sTraceFile,
                        "# rb3 frame trace: f=frame dt=frameMs lp=loadPollMs "
                        "lpu=pollUntilMs scr=screen ld=loaderAdds st=streamOpens "
                        "pend=pendingLoaders\n");
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
    fprintf(sTraceFile,
            "{\"f\":%d,\"dt\":%.3f,\"lp\":%.3f,\"lpu\":%.3f,"
            "\"scr\":\"%s\",\"ld\":%d,\"st\":%d,\"pend\":%d}\n",
            frame, dtMs, loadPollMs, loadPollUntilMs, screen,
            gFrameTraceLoaderAdds, gFrameTraceStreamOpens, pendingLoaders);
    // Flush each frame so a SIGTERM mid-run still leaves a complete trace.
    fflush(sTraceFile);

    gFrameTraceLoaderAdds = 0;
    gFrameTraceStreamOpens = 0;
}

#endif // HX_NATIVE
