// rb3_loaddet_probe.cpp — Wave-12 Lane A (W0.3d-b) A-S1 attribution instrument.
//
// DIAGNOSIS-ONLY, getenv-gated (RB3_LOADDET_PROBE), default-OFF. Probe-only: no
// behavioural change; a normal boot never calls any of this (the flag is read
// once and cached; every hook early-returns when off). HX_NATIVE-only, so the
// MWCC match build is byte-identical.
//
// PURPOSE. Wave-11 BOOTRNG proved the boot-varying state is the global gRand
// STREAM POSITION at a pinned capture (~11,231-draw spread across 12 fixed-clock
// boots) and that every render-side SELECTION (preset pick, grade, light
// color/type) is already deterministic. The open question is the MECHANISM by
// which the stream position diverges under RB3_FIXED_CLOCK. WAVE12 binding
// correction A2 refuted the "insertion order -> gRand consumption order" model
// (SortPolls name-sorts; completion callbacks assert MainThread and draw no
// gRand; the spread is a COUNT axis a permutation cannot produce) and named the
// prime suspect: completion-FRAME TIMING — async ThreadCall / DataLoader parse
// completions landing on DIFFERENT sim frames run-to-run, so the set of live,
// Polling, gRand-consuming objects differs per frame and the per-frame draw
// COUNT diverges, accumulating to the ~11k spread.
//
// This instrument logs two co-registered streams to stderr so a harness can
// align two boots frame-by-frame and find the FIRST frame at which they diverge:
//
//   [LOADDET] frame=<N> gdraw=<cumulative RB3GRandDrawCount at frame N start>
//   [LOADDET] complete frame=<N> gdraw=<cum> kind=<dir|data> name=<file>
//
// The frame tap is called from RB3TraceSetFrame (native/src/rb3_session_trace.cpp),
// which runs once per RunOneFrame at frame start under RB3_FIXED_CLOCK. The
// completion taps fire at the two loader-completion transitions:
//   - DirLoader::LoadObjs  (src/system/obj/DirLoader.cpp, mState=DoneLoading)
//   - DataLoader::ThreadDone (src/system/obj/DataFile.cpp, ptmf=DoneLoading) —
//     the async-worker-completion point (MainThread-asserted), the H-TIMING hook.

#ifdef HX_NATIVE
#include <cstdio>
#include <cstdlib>

// Global-stream draw counter (native/src/../src/system/math/Rand.cpp, C++ linkage).
unsigned long RB3GRandDrawCount();
// Current sim frame index (native/src/rb3_session_trace.cpp), set per frame.
extern int gRB3TraceFrame;

namespace {
    int sLoadDetOn = -1;  // -1 = unread, 0 = off, 1 = on
    inline bool LoadDetOn() {
        if (sLoadDetOn < 0)
            sLoadDetOn = getenv("RB3_LOADDET_PROBE") ? 1 : 0;
        return sLoadDetOn == 1;
    }
}

// Per-frame stream-position sample. Cumulative gRand draw count at the START of
// frame N (RB3TraceSetFrame runs before this frame's polls). The per-frame delta
// gdraw[N+1]-gdraw[N] is the draws consumed during frame N.
void RB3LoadDetFrameTap(int frame) {
    if (!LoadDetOn())
        return;
    fprintf(stderr, "[LOADDET] frame=%d gdraw=%lu\n", frame, RB3GRandDrawCount());
}

// A loader reached its DoneLoading transition on the current sim frame. `kind`
// is "dir" (DirLoader — a whole .milo ObjectDir went live) or "data" (DataLoader
// — a milo/DTA file finished parsing on the ThreadCall worker). `name` is the
// loader's FilePath.
void RB3LoadDetComplete(const char *name, const char *kind) {
    if (!LoadDetOn())
        return;
    fprintf(stderr, "[LOADDET] complete frame=%d gdraw=%lu kind=%s name=%s\n",
            gRB3TraceFrame, RB3GRandDrawCount(), kind, name ? name : "?");
}
#endif
