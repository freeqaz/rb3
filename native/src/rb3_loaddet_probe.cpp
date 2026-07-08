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
#include <dlfcn.h>
#include <map>

// Global-stream draw counter (native/src/../src/system/math/Rand.cpp, C++ linkage).
unsigned long RB3GRandDrawCount();
// Current sim frame index (native/src/rb3_session_trace.cpp), set per frame.
extern int gRB3TraceFrame;

// R4 (Wave-17 Lane L) ATTRIBUTION arm. gRB3LoadDetAttribOn is read ONCE at static
// init (getenv, before any draw) and consulted by the four Rand.cpp free-function
// wrappers via a single inlined `if (gRB3LoadDetAttribOn)` load+branch: flag-OFF
// (the normal case AND the M2/M3 gate arms, which set RB3_LOADDET_PROBE not
// _ATTRIB) is one predicted-not-taken branch, no call, no __builtin_return_address
// eval. Only when RB3_LOADDET_ATTRIB=1 do the wrappers pass their caller PC
// (__builtin_return_address(0), captured IN the wrapper so it resolves to the real
// consumer) to RB3LoadDetAttribRecord below. HX_NATIVE-only -> MWCC match build
// byte-identical (G3).
int gRB3LoadDetAttribOn = 0;

namespace {
    int sLoadDetOn = -1;  // -1 = unread, 0 = off, 1 = on
    // Attribution implies the frame/anchor/complete taps are live so the harness
    // can align boots by frame and window.
    inline bool LoadDetOn() {
        if (sLoadDetOn < 0)
            sLoadDetOn =
                (getenv("RB3_LOADDET_PROBE") || getenv("RB3_LOADDET_ATTRIB")) ? 1 : 0;
        return sLoadDetOn == 1;
    }

    // Wave-19 T1 (Lane F) FRAME-ASSIGNMENT TIMING tracer, attribution mode.
    // Separate cached flag RB3_LOADDET_TIMELINE, default-OFF, so the NEW per-frame
    // filearrive/songms markers do NOT perturb the marker stream that R4's ledger
    // (order axis), white_regrade, and wash v1 parse when they run with only
    // RB3_LOADDET_PROBE=1 — a byte-identical stream for every existing consumer.
    // NOTE (naming box): these markers feed the frameAssign/songClock axes, which
    // bind each event to its frame index; that is a DIFFERENT thing from R4's
    // completion-SEQUENCE order axis (already 10/10). See loaddet_gate.py --timeline.
    int sTimelineOn = -1;  // -1 = unread, 0 = off, 1 = on
    inline bool TimelineOn() {
        if (sTimelineOn < 0)
            sTimelineOn = getenv("RB3_LOADDET_TIMELINE") ? 1 : 0;
        return sTimelineOn == 1;
    }

    // Per-frame caller-PC -> gRand-draw count, flushed and cleared at each frame
    // boundary. std::map keeps the flush order stable (address-sorted) so boot A
    // vs boot B diff cleanly per caller. Main-thread only (the RandomInt/Float
    // wrappers MILO_ASSERT MainThread), so no locking needed.
    std::map<void *, unsigned> sAttribCounts;

    // Arm the attribution flag once at static-init time (environ is live before
    // C++ static ctors run). Deterministic, single getenv.
    struct AttribInit {
        AttribInit() { gRB3LoadDetAttribOn = getenv("RB3_LOADDET_ATTRIB") ? 1 : 0; }
    } sAttribInit;
}

// Record one gRand draw attributed to caller PC `pc`. Only reached when
// gRB3LoadDetAttribOn (the wrappers guard the call), so no re-check of the flag.
void RB3LoadDetAttribRecord(void *pc) { ++sAttribCounts[pc]; }

// Flush the per-frame attribution map: one line per distinct caller PC that drew
// gRand during the frame that just ENDED (RB3LoadDetFrameTap runs at the START of
// `frame`, so the accumulated draws belong to frame-1). Emits raw pc + module
// offset (for offline addr2line, robust to static/inlined symbols) AND the dladdr
// symbol name when the linker exported it.
static void FlushAttrib(int frame) {
    if (sAttribCounts.empty())
        return;
    for (std::map<void *, unsigned>::iterator it = sAttribCounts.begin();
         it != sAttribCounts.end(); ++it) {
        void *pc = it->first;
        const char *sym = "?";
        const char *fname = "?";
        unsigned long off = 0;
        Dl_info info;
        if (dladdr(pc, &info)) {
            if (info.dli_fname)
                fname = info.dli_fname;
            if (info.dli_fbase)
                off = (unsigned long)((char *)pc - (char *)info.dli_fbase);
            if (info.dli_sname)
                sym = info.dli_sname;
        }
        fprintf(stderr,
                "[LOADDET] attrib frame=%d pc=%p off=0x%lx sym=%s mod=%s draws=%u\n",
                frame - 1, pc, off, sym, fname, it->second);
    }
    sAttribCounts.clear();
}

// Per-frame stream-position sample. Cumulative gRand draw count at the START of
// frame N (RB3TraceSetFrame runs before this frame's polls). The per-frame delta
// gdraw[N+1]-gdraw[N] is the draws consumed during frame N.
void RB3LoadDetFrameTap(int frame) {
    if (!LoadDetOn())
        return;
    fprintf(stderr, "[LOADDET] frame=%d gdraw=%lu\n", frame, RB3GRandDrawCount());
    if (gRB3LoadDetAttribOn)
        FlushAttrib(frame);
}

// A-S2 anchor marker. Fires from GamePanel::StartGame at the is_playing 0->1 edge
// (the reseed anchor), gated ONLY on RB3_LOADDET_PROBE (NOT the determinism flag)
// so it lands in BOTH the seam-ON and seam-OFF gate arms. Logs the anchor sim
// frame + cumulative gdraw so the gate harness can align both arms by frame index
// and read the POST-ANCHOR draw delta (gdraw@[anchor+K] - gdraw@anchor) — the
// re-based stream position the seam is supposed to collapse.
void RB3LoadDetAnchorMark() {
    if (!LoadDetOn())
        return;
    fprintf(stderr, "[LOADDET] anchor frame=%d gdraw=%lu\n", gRB3TraceFrame,
            RB3GRandDrawCount());
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

// ---------------------------------------------------------------------------
// Wave-19 T1 (Lane F) — FRAME-ASSIGNMENT TIMING markers (attribution only).
// Gated on the NEW RB3_LOADDET_TIMELINE flag (NOT PROBE/ATTRIB) so existing
// consumers see a byte-identical stream (§2.1). Distinct keywords `filearrive`
// and `songms` — neither matches loaddet_gate.py's COMPLETE_RE/ATTRIB_RE, so the
// order axis and attrib parse are untouched. Both key on gRB3TraceFrame (the one
// frame axis, current under RB3_FIXED_CLOCK; see R4 A-2: RB3HttpServerPoll runs
// post-RunOneFrame at App.cpp:909, same-iteration frame equality).
// ---------------------------------------------------------------------------

// An async FileLoader observed its bytes-arrival flip (ReadDone true ->
// DoneLoading) on the current sim frame. `name` is the loader FilePath. On native
// AsyncFile is synchronous (AsyncFile_Native.cpp:77 _ReadDone()==true) so the flip
// fires the same poll the read issued; the run-to-run frame variance is queue-issue
// timing, a SECONDARY/completeness frameAssign signal (the primary carrier under
// jitter is the DataLoader "data" completion). Covers only the ReadDone arrival
// path; the open-fail (Loader.cpp:924) and LoadStream (:1016) DoneLoading entries
// are known-uncovered (R6.1) — frameAssign is not total-arrival coverage.
void RB3LoadDetFileArrive(const char *name) {
    if (!TimelineOn())
        return;
    fprintf(stderr, "[LOADDET] filearrive frame=%d gdraw=%lu name=%s\n",
            gRB3TraceFrame, RB3GRandDrawCount(), name ? name : "?");
}

// Per-frame audio-clock sample. `ms` is the null-guarded
// MasterAudio::GetTime() (-1 until a song is live). Sampled at the clean
// RB3HttpServerPoll site (rb3_http_handlers.cpp, A3 option-b — never touches the
// dirty rb3_session_trace.cpp). Frame key is gRB3TraceFrame, NOT the caller's
// frame arg, so songms shares the one frame axis. This is the songClock axis
// source; it sits behind the RB3_HTTP guard, so songClock requires RB3_HTTP=1
// (R4 c) and a 0-sample songClock is "no signal", never PASS.
void RB3LoadDetSongMs(float ms) {
    if (!TimelineOn())
        return;
    fprintf(stderr, "[LOADDET] songms frame=%d ms=%.1f\n", gRB3TraceFrame, ms);
}
#endif
