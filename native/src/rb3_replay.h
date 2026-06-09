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

// The recorded sim dt (SECONDS) to advance the menu/UI clock by at `frame` —
// the `sdt` of the recorded fr at-or-before `frame` (carry-forward; fr rows are
// decimated so this is the nearest preceding sample). 0 before the first fr or
// when no fr carried `sdt`. Seam 1 (Task.cpp) accumulates this each frame.
float RB3ReplayDtForFrame(int frame);

// The recorded song-ms to feed straight into TheTaskMgr.SetSeconds at `frame` —
// the `sm` of the recorded fr at-or-before `frame` (carry-forward). -1 when the
// nearest preceding fr had no `sm` (i.e. menus / not in a song). Seam 2
// (Game.cpp) feeds this into SetSeconds, bypassing live audio + DeJitter.
float RB3ReplaySongMsForFrame(int frame);

#endif // HX_NATIVE
#endif // RB3_REPLAY_H
