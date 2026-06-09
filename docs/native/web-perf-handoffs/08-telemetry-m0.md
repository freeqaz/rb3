# 08 — Session telemetry M0 (P0.3) — pointer + delta summary

**Canonical spec: `docs/native/SESSION_TELEMETRY_DESIGN.md` ("Locked v1 contract",
lines 92–179 — VERIFIED present, authoritative). This file is only the perf-roadmap-
relevant summary + current-state deltas. A dedicated worktree exists:
`.claude/worktrees/session-telemetry` (branch `wt-session-telemetry`). This track has its
own established working mode (ultracode-delegated waves) — do not fold it into the other
handoffs' agents.**

> **STATUS (verified 2026-06-09): M0 is NOT a fresh to-do — it is already IMPLEMENTED on
> the worktree.** `wt-session-telemetry` is **9 commits ahead of master**, including
> `025b05e1 telemetry(M0): recorder core + report tool + SQLite ingest (Wave 1)` and
> `3458d68a telemetry(M0/M2): rich payloads + native engine taps + web egress (Wave 2)`,
> plus M1 (Tier-1 replay, `32410510`) and M4 (Tier-2 determinism prototype, Waves 5-8).
> On the worktree, `native/src/rb3_session_trace.{h,cpp}` already exist (the recorder TU is
> built; `rb3_frame_trace.cpp` was deleted/subsumed), and `native/web/server.py` +
> `native/web/telemetry/db.py` already carry `POST/GET /api/telemetry/<sid>` + the SQLite
> ingest. **The "M0 scope (locked order)" list below describes work that is, as of this
> verification, DONE on the worktree but NOT YET on master** — master still has the old
> `rb3_frame_trace.cpp` and no telemetry server routes. Treat the list below as the design
> contract / what to merge to master, not as unstarted work.

## Why the perf roadmap needs it

- Unifies the point tools (one NDJSON trace: hdr/fr/in/nav/boot/song/au/log per
  session, file sink native / POST web → SQLite).
- Closes measurement gaps: native boot/nav milestones (currently web-only in
  `native/src/main_web.cpp`), underrun→frame attribution (M2), session identity.

## M0 scope (locked order)

1. `sid` minted in C++ both platforms; web pre-js mirrors to `window.__rb3Sid`.
2. `client_seq` on every line + sparse axis schema (`ax:{wh}`; tilt reserved).
3. New recorder TU `native/src/rb3_session_trace.{h,cpp}` subsuming
   `rb3_frame_trace.cpp` (back-compat alias `RB3_FRAME_TRACE` → `RB3_SESSION_TRACE`;
   ring `RB3_TRACE_RING`, long-frame force-emit `RB3_TRACE_FRAME_MS`=20, decimation
   `RB3_TRACE_FRAME_DECIMATE`=30).
4. **Frame-tap relocation (critical)**: today the tap lives in
   `App::RunWithoutDebugging` (src/App.cpp — `RB3FrameTraceRecord` call at :833-836, in the
   wall-timed loop :812-840) which the WEB path bypasses (main_web.cpp:653 calls
   `RunOneFrame` directly). Move Split+record into `App::RunOneFrame`
   (function body :529-594 — the comment block above it starts ~:499), gated on a
   `gRB3TraceActive` bool. (NB: today's gate global is `gFrameTraceActive`, set inside
   `rb3_frame_trace.cpp`; the new recorder renames it `gRB3TraceActive`.)
5. Nav tap: sink `UIScreenChangeMsg` at its broadcast point (src/system/ui/UI.cpp:682)
   instead of polling-detection; record from/to/focus/wentBack.
6. Hoist BootMark + nav-check out of main_web.cpp so native headless emits boot/nav.
7. Server: `POST /api/telemetry/<sid>` → single-writer SQLite
   (`native/web/telemetry/`), `INSERT OR IGNORE` on `(sid, client_seq)`.

## Current-state caveats (verified 2026-06-09)

- `src/App.cpp` (master) carries **exactly 25 uncommitted lines** — VERIFIED via
  `git diff src/App.cpp` (`1 file changed, 25 insertions(+)`): a concurrent agent's
  `RB3RenderHeldFrame` RB3_FREEZE_FRAME test-harness helper (diff hunk `@@ -498,6
  +499,30 @@`, helper body :502-524) plus an `#include <ctime>` at :95. The M0 frame-tap
  edit lands in the same `App::RunOneFrame`/comment region — coordinate/rebase before
  landing; do NOT revert or absorb the other agent's lines.
- On MASTER, `rb3_frame_trace.cpp` is live and working (env `RB3_FRAME_TRACE`, gate
  `gFrameTraceActive`, no ring/decimation — writes every frame via `fprintf`). The
  `RB3_TRACE_RING` / `RB3_TRACE_FRAME_MS` / `RB3_TRACE_FRAME_DECIMATE` knobs in §M0 #3
  are NOT implemented in master's `rb3_frame_trace.cpp` — they belong to the new
  `rb3_session_trace.cpp` recorder (already built on the `wt-session-telemetry` worktree,
  where `rb3_frame_trace.cpp` is deleted/subsumed). Keep emitting identical `fr` rows for
  the old env alias so `frame_profiler.py` / `loadperf-frametail.py` keep working.
- App.cpp is a matched-build TU: tap code HX_NATIVE-gated, zero-cost when inactive.

## M0 acceptance (from design doc)

Native headless run with `RB3_SESSION_TRACE=/tmp/t.jsonl` produces a valid trace:
hdr (sid/platform/build), monotonic client_seq, fr per decimation policy (all
long frames present), exact nav transitions, boot marks (engine_init_done, gpu_ready,
appctor_start/done). Old `RB3_FRAME_TRACE` alias still feeds frame_profiler.py.
