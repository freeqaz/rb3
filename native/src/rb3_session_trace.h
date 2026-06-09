// rb3_session_trace.h — unified session telemetry recorder (HX_NATIVE only).
//
// M0 RECORDER CORE. Subsumes the old rb3_frame_trace.cpp single-purpose frame
// tracer. One time-ordered, versioned NDJSON event stream folding frame metrics
// + input + nav + boot + song + log + marks into a bounded in-memory ring with
// a string interner, behind a single predicted branch when tracing is OFF.
//
// Authoritative wire/behavior contract: docs/native/SESSION_TELEMETRY_DESIGN.md
// "Locked v1 contract" (wins over §3–§9). This TU implements the C++ recorder
// core (D1) + the v1 NDJSON schema (D2). The engine taps (App.cpp frame tap,
// JoypadPoll input tap, UIScreenChangeMsg nav sink, Debug::Print log tap, song
// hooks) are Wave 2 — this TU exposes the public API + the back-compat shim so a
// native run already emits hdr+fr with NO src/ edit.
//
// Activation (parsed once in RB3TraceInit):
//   RB3_SESSION_TRACE=<path|1>   master toggle; <path> => file sink, "1" =>
//                                default path. Opens the NDJSON sink + arms
//                                gRB3TraceActive AND the engine's
//                                gFrameTraceActive (so Loader.cpp's ld/st
//                                counters increment).
//   RB3_FRAME_TRACE=<path>       BACK-COMPAT ALIAS — same as RB3_SESSION_TRACE
//                                but forces full-frame capture (decimate=1) so
//                                scripts/native/frame_profiler.py keeps working.
//   RB3_TRACE_RING=<n>           ring capacity (default 16384).
//   RB3_TRACE_FRAME_MS=<f>       long-frame "always emit" threshold (default 20).
//   RB3_TRACE_FRAME_DECIMATE=<n> baseline 1/N fr sample (default 30; 1 = every
//                                frame; 0 = long-frames-only).
//   RB3_BUILD_SHA=<str>          stamped into hdr.build.git (default "").

#ifndef RB3_SESSION_TRACE_H
#define RB3_SESSION_TRACE_H

#ifdef HX_NATIVE

#include <cstdint>

// ---------------------------------------------------------------------------
// Event kinds (in-memory). The wire `k` string is derived in the serializer.
// ---------------------------------------------------------------------------
enum RB3TraceKind {
    TK_HDR = 0,   // session header (emitted once, first line)
    TK_BOOT,      // boot phase mark
    TK_FRAME,     // per-frame metrics (fr)
    TK_INPUT,     // input edge (in)
    TK_NAV,       // screen transition (nav)
    TK_SONG,      // song lifecycle (song)
    TK_AU,        // audio underrun (au) — web, deferred
    TK_LOG,       // diagnostic log line (log)
    TK_MARK,      // user/bug marker (mark)
    TK_CHK        // M4 checkpoint (chk) — replay-divergence state hash + raw fields
};

// Max per-player scores carried RAW in a `chk` event (the hash uses the exact
// sum of all active players, but the raw score[] array is capped for the fixed
// POD). 4 covers a full RB3 band (4 instruments); excess players still fold into
// the hashed scoreSum (so the hash is correct even past 4 players).
enum { RB3_CHK_MAX_SCORES = 4 };

// ---------------------------------------------------------------------------
// Toggle gate + frame counter. One exported global each, mirroring the existing
// gFrameTraceActive pattern: every public Record* is a thin inline whose first
// statement is `if (!gRB3TraceActive) return;` — the same not-taken branch the
// engine chokepoints already pay when tracing is off.
// ---------------------------------------------------------------------------
extern bool gRB3TraceActive;   // false by default; set by RB3TraceInit when armed
extern int  gRB3TraceFrame;    // current frame index, written by the frame tap

// ---------------------------------------------------------------------------
// Public API (Locked v1 contract). All Record* are no-ops until RB3TraceInit
// arms the sink. cs (client_seq) + t (monotonic ms) + the sid are minted inside
// the recorder; callers never supply them.
// ---------------------------------------------------------------------------

// Resolve env toggles, mint sid, open the file sink, set gRB3TraceActive +
// gFrameTraceActive. Idempotent: a second call while armed is a no-op.
void RB3TraceInit();

// Frame tap calls this first thing each frame (Wave 2). Sets gRB3TraceFrame.
void RB3TraceSetFrame(int frame);

// Per-frame metrics. dt = frame ms, lp = LoadMgr.Poll ms, lpu = PollUntil ms,
// scr = screen name, pend = pending loaders. ld/st are read+zeroed from the
// engine counters inside the recorder. Subject to fr decimation (§4.7).
void RB3RecordFrame(float dt, float lp, float lpu, const char *scr, int pend);

// Input edge (edge-only: dropped internally if bits == last recorded bits).
// whammy/tilt are normalized floats; serialized sparsely as ax{wh,ti} = round(x*1000)
// and only when whammy||tilt is non-zero. v1 captures ONLY whammy + tilt.
void RB3RecordInput(int pad, uint32_t bits, uint32_t dn, uint32_t up,
                    float whammy, float tilt);

// Screen transition. wentBack = the UIScreenChangeMsg Bool(2) (back-nav flag).
void RB3RecordNav(const char *from, const char *to, const char *focus,
                  bool wentBack);

// Boot phase mark (e.g. "engine_init_done").
void RB3RecordBootMark(const char *phase);

// ---------------------------------------------------------------------------
// Typed per-kind recorders (D2 §4.3 wire schema). These replace the Wave-1
// generic RB3RecordEvent stub: each emits exactly the reader-expected keys
// (scripts/telemetry/trace-report.py + native/web/telemetry/db.py).
// ---------------------------------------------------------------------------

// Song lifecycle. ev ∈ load/start/end; id/track/diff are interned strings.
// score/pct are emitted ONLY when >= 0 (callers pass -1 to omit — the exact
// score/pct accessor is OQ7-deferred to M1/M3). pct is a 0..1 fraction (the
// report renders pct*100); score is the raw points value.
void RB3RecordSong(const char *ev, const char *id, const char *track,
                   const char *diff, float score, float pct);

// Audio underrun (web; native may omit). under = underrun-event count this
// row, frames = underrun-frame count. Both always serialized.
void RB3RecordAudio(int under, int frames);

// Diagnostic log line. lvl ∈ warn/assert/error/info; msg is the message text;
// src (optional file:line) is emitted only when non-null.
void RB3RecordLog(const char *lvl, const char *msg, const char *src);

// User/bug marker. tag is the marker tag; note (optional) is emitted only when
// non-null.
void RB3RecordMark(const char *tag, const char *note);

// ---------------------------------------------------------------------------
// M4 (Tier-2) replay CHECKPOINT (chk). Additive — does NOT bump hdr.v. The
// caller (the engine-typed tap in rb3_trace_taps.cpp) samples the state vector
// (screen/focus/clocks/scores) under null guards, then hands the already-pulled
// scalars here; the recorder computes the FNV-1a fast-equality hash + emits the
// chk event carrying BOTH the hash and the RAW fields (for field-level ε
// classification on a hash mismatch). The hashed tuple (per the M4 design /
// task contract) is:
//   [ scr, focus, q(taskSec,1ms), q(beat,0.01), q(songMs,1ms),
//     scoreSum (exact int), nPlayers ]
// Quantize-before-hash absorbs benign x86-vs-x86 float drift; the exact integer
// scoreSum + nPlayers stay un-quantized so a 1-point score divergence trips it.
//
//   scr/focus : current screen + focus-component name ("" in null/menu states).
//   taskSec   : TheTaskMgr.Seconds(kRealTime).
//   beat      : TheTaskMgr.Beat().
//   songMs    : the in-song clock (< 0 in menus -> hashed as -1, raw sm omitted).
//   scoreSum  : exact sum of every active Player::GetScore().
//   scores    : the first nScores per-player exact scores (RAW, capped at
//               RB3_CHK_MAX_SCORES; the hash still uses the full scoreSum).
//   nScores   : count of valid entries in scores[] (<= nPlayers).
//   nPlayers  : active-player count (0 in menus).
//   pct       : Performer::GetPercentComplete() of player 0 (-1 when unknown);
//               RAW only (not hashed — it is derivable from the score timeline).
void RB3RecordCheckpoint(const char *scr, const char *focus,
                         float taskSec, float beat, float songMs,
                         long scoreSum, const int *scores, int nScores,
                         int nPlayers, int pct);

// Set the current song-ms (D2 §4.5). Frame/input (and all) events pick this up
// for the envelope `sm`, which is emitted only when ms >= 0 (menus pass < 0 to
// omit `sm` entirely). Wave 2 wires the real GetBeatMaster()->GetAudio()->
// GetTime() chain into this; the core defaults to "not in a song".
void RB3TraceSetSongMs(float ms);

// Set the current sim dt in SECONDS — the menu/UI clock advance for this frame,
// derived by the frame tap from the TaskMgr.mTime cycle delta. Stamped into the
// fr row as the optional `sdt` field so RB3_REPLAY_FIXED_CLOCK (seam 1, Task.cpp)
// can replay the recorded per-frame menu-clock advance deterministically. The
// core defaults to 0 (omitted from the wire) until the frame tap sets it.
void RB3TraceSetSimDt(float seconds);

// Append the ring to the file sink + fflush, so a SIGTERM mid-run leaves valid
// NDJSON. Cheap no-op when not armed.
void RB3TraceFlush();

// Test-only: flush, close the sink, and reset all recorder state (sid, cs, ring,
// interner, env-parse cache) so a gtest can RB3TraceInit a fresh file repeatedly.
void RB3TraceShutdown();

// ---------------------------------------------------------------------------
// Back-compat shim. The existing call at src/App.cpp:809 still links against
// this exact signature. It forwards to RB3RecordFrame (reading+zeroing the
// engine ld/st counters), so a native run emits hdr + fr with NO App.cpp edit.
// ---------------------------------------------------------------------------
void RB3FrameTraceRecord(int frame, float dtMs, float loadPollMs,
                         float loadPollUntilMs, const char *screen,
                         int pendingLoaders);

#endif // HX_NATIVE
#endif // RB3_SESSION_TRACE_H
