// rb3_replay.h — Tier-1 session replay (HX_NATIVE only).
//
// Re-drives a recorded session's INPUT through the real joypad chokepoint so a
// captured bug reproduces. The recorded `in` events (from rb3_session_trace's
// NDJSON, see docs/native/SESSION_TELEMETRY_DESIGN.md §8 "Tier 1") are loaded
// into an ordered (frame, bits) table; the JoypadPoll hook in
// rb3_joypad_native.cpp asks RB3ReplayBitsForFrame(gRB3TraceFrame) for the
// held bitmask each poll and feeds it to SendButtonMessages(0, bits) — the SAME
// one-true chokepoint a live keyboard/USB-gamepad/`pad:<bit>` press drives.
//
// WHY the raw joypad feed, NOT the verb layer (Wave-2 gate finding): the `in`
// recorder tap fires ONLY at the JoypadPoll/SendButtonMessages chokepoint. The
// RB3_GAME_INPUT named-verb path (start/confirm/...) injects a ButtonDownMsg
// straight into TheUI.Handle, bypassing JoypadPoll — it produces NO `in` rows
// and misses the gameplay GuitarController. So replay MUST re-assert the held
// bitmask through SendButtonMessages: it re-derives mNewPressed/mNewReleased
// from mButtons^bits internally, regenerating identical down/up edges, and the
// live recorder tap then re-records the replayed input as fresh `in` rows. That
// record->replay->compare round-trip is the Tier-1 acceptance bar.
//
// EDGE RECONSTRUCTION: input is recorded edge-only (an `in` fires on a bitmask
// change, carrying `b` = the full held bitmask AFTER the edge). The held state
// at frame N is therefore the `b` of the last `in` with f <= N, default 0 before
// the first event — a simple carry-forward over the sorted (frame, bits) table.
//
// Activation:
//   native:  RB3_REPLAY=<path.jsonl>   parse the trace file's `in` events.
//   web:     ?replay=<sid>  -> rb3_pre.js fetches GET /api/telemetry/<sid> into
//            window.__rb3ReplayData (a single NDJSON string); RB3ReplayInit reads
//            it via EM_ASM. (Browser run-verification DEFERRED — compile-guarded.)

#ifndef RB3_REPLAY_H
#define RB3_REPLAY_H

#ifdef HX_NATIVE

// Parse the replay source (RB3_REPLAY file on native; window.__rb3ReplayData on
// web) into the ordered (frame, bits) table and arm replay. Idempotent: a second
// call is a no-op. Safe to call before the first JoypadPoll (the JoypadPoll hook
// invokes it through a lazy-once guard, so no App.cpp edit is needed).
void RB3ReplayInit();

// True once RB3ReplayInit has loaded at least one `in` event and replay is armed.
// When true, the JoypadPoll hook OVERRIDES live input with the replayed bitmask.
bool RB3ReplayActive();

// The held JoypadButton bitmask at this frame: the `b` of the last recorded `in`
// with f <= frame, or 0 before the first event. (Carry-forward, edge-only.)
unsigned int RB3ReplayBitsForFrame(int frame);

// The frame index of the LAST recorded `in` event (so a driver can run the engine
// at least this many frames to replay the whole input timeline). 0 if none.
int RB3ReplayLastFrame();

// ── Tier-2 fixed-clock replay (M4) ──────────────────────────────────────────
// When RB3_REPLAY_FIXED_CLOCK is set AND replay is active, two HX_NATIVE clock
// seams (Task.cpp seam 1, Game.cpp seam 2) drive the sim clock from the RECORDED
// per-frame {sdt, sm} instead of wall-clock / live audio, making gameplay a
// deterministic function of (recorded input + recorded song-ms). See
// docs/native/SESSION_TELEMETRY_DESIGN.md "M4 (Tier-2)".

// True iff RB3_REPLAY_FIXED_CLOCK is set (parsed once). Only MEANINGFUL when
// RB3ReplayActive() — the seams gate on (RB3ReplayFixedClock() && RB3ReplayActive()).
bool RB3ReplayFixedClock();

// ── W0.3b — Trace-free fixed sim clock (headless-determinism harness) ─────────
// A TRACE-FREE cousin of RB3ReplayFixedClock: it engages on a plain boot with NO
// recorded input trace loaded, so a fresh headless boot can advance its sim clock
// by a CONSTANT per-frame timestep instead of wall-clock / live audio. This makes
// the splash/boot scene a deterministic function of the absolute frame index,
// which is what the draw-log integration golden (W0.3) needs to diff green.
// Purely a determinism harness — OFF in all shipping runs. When a real replay
// trace IS active the recorded-clock path takes precedence; this only fills the
// no-trace gap that RB3ReplayActive() excludes today.

// True iff RB3_FIXED_CLOCK is set (non-empty, non-"0"), parsed once. INDEPENDENT
// of any trace (unlike RB3ReplayFixedClock, which is only meaningful under
// RB3ReplayActive()). Web: mirrors the replay pattern via window.__rb3FixedClock.
bool RB3FixedClockActive();

// The constant per-frame sim dt (SECONDS) to advance the menu/UI clock by when
// RB3FixedClockActive() and no trace is driving the clock. Default 1/60s so the
// animation PROGRESSES deterministically to a well-defined frame-N state.
// Overridable via RB3_FIXED_CLOCK_DT_MS (milliseconds; any finite value >= 0 is
// honoured, including 0.0 = a true freeze holding animation at t=0). Parsed once.
float RB3FixedClockDt();

// The recorded sim dt (SECONDS) to advance the menu/UI clock by at `frame` — the
// `sdt` of the recorded PER-FRAME `clk` sample at frame N (the un-decimated clock
// stream -> an EXACT per-frame value). Falls back to the decimated `fr` table
// (carry-forward, nearest preceding sample) for older traces with no clk stream.
// 0 before the first sample. Seam 1 (Task.cpp) accumulates this each frame.
float RB3ReplayDtForFrame(int frame);

// The recorded song-ms to feed straight into TheTaskMgr.SetSeconds at `frame` —
// the `sm` of the recorded PER-FRAME `clk` sample at frame N (un-decimated, so the
// replay's song clock tracks the recording frame-for-frame with NO carry-forward
// staleness). Falls back to the decimated `fr` table (carry-forward) for older
// traces with no clk stream. -1 in menus / not in a song. Seam 2 (Game.cpp) feeds
// this into SetSeconds, bypassing live audio + DeJitter.
float RB3ReplaySongMsForFrame(int frame);

// ── M4 GAP 1 — boot RNG seed re-seed ─────────────────────────────────────────
// The engine's global gRand is boot-seeded from wall-clock time (System.cpp
// HX_NATIVE SeedRand), so its consumers (InterstitialMgr venue-cut selection,
// etc.) diverge run-to-run -> recorded nav != replayed nav. On replay the boot
// path asks RB3ReplaySeed for the RECORDED seed (captured from the trace hdr's
// `seed` field) and feeds it to SeedRand instead of the live time, so every
// gRand consumer reproduces. This SELF-ARMS replay (idempotently calls
// RB3ReplayInit) because it runs during boot, before the lazy JoypadPoll init.
//
// Returns true + writes *out iff the loaded trace carried a `seed` (a replay run
// of a seed-bearing trace); false otherwise (no replay, or an older trace with no
// seed) -> the boot path keeps its live time-derived seed.
bool RB3ReplaySeed(int *out);

// ── M4 GAP 2 — run-aid (autohit/nofail) re-application ───────────────────────
// The recorded run aids are out-of-band (HTTP verb / script), NOT replayable `in`
// edges, so replay must re-apply them itself. The recorder emitted each as a
// one-shot mark{tag:"aid",note:<name>} at the frame it was applied. The native
// game-input poll calls RB3ReplayPendingAids(frame, ...) each frame to collect
// the aids whose recorded frame has been reached (and not yet re-applied this
// run), then re-applies each via the same ExecAutohit/ExecNoFail path the live
// HTTP verb drives — reproducing the autoplay -> same gem hits -> same score.
//
// Fills outAids[0..n) with up to maxAids aid-name C-strings (owned by the replay
// table; valid for the process lifetime) that just became due, marks them applied
// (one-shot), and returns n. Cheap no-op (returns 0) when the trace has no aids.
int  RB3ReplayPendingAids(int frame, const char **outAids, int maxAids);

// Latch a pending aid as APPLIED once it actually took effect (one-shot). The
// caller calls this only after the re-apply confirmed success (e.g. ExecAutohit
// armed >=1 player); an aid that no-ops on an early frame (players not ready yet)
// stays pending and is re-offered next frame until it lands. Idempotent.
void RB3ReplayMarkAidApplied(const char *aid);

// True iff the loaded trace carried at least one run-aid marker. Lets the poll
// skip the per-frame check entirely for aid-free traces.
bool RB3ReplayHasAids();

#endif // HX_NATIVE
#endif // RB3_REPLAY_H
