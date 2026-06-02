# C1 — Async Loader: Implementation Result (NEGATIVE — do not merge as-is)

**Status:** implemented + validated on branch `wt-c1loader` (commit `636b387b`, off `245b8823`).
**Verdict:** the async-loader premise is **wrong on native** — same shape as the C3 finding.
Do **not** merge the off-thread reader as a perf fix. The valuable output is the
instrumentation + the measurement below, which redirects all future loader-perf work.

## What was built (Phases 0–2, dormant behind `RB3_LOADER_ASYNC`, default OFF)
- Phase 0: `RB3_LOADER_INSTRUMENT` env-gated timers (LoadMgr::Poll wall-time, per-Read
  byte/time histogram, kProcessPost-advance counter). Driver: `scripts/native/loader-instrument.py`.
- Phase 1: `native/src/rb3_async_loader.{h,cpp}` — single `rb3-loader` pthread + `sem_t`
  (ThreadCall_Native idiom; `volatile bool` + `__sync_synchronize()`, no `<atomic>`),
  Init after ThreadCallInit / join in SystemTerminate before FileTerminate (System.cpp).
- Phase 2: `NativeStdioFile::ReadAsync/ReadDone` divert + async-on sub-arms reviving the
  matched-fork `TempEof` yield in Loader.cpp/DirLoader.cpp.

All gated behind `RB3_LOADER_ASYNC` (default OFF → default build byte-behaviour-neutral;
the conservative default flag-ON `game_screen` filter also matches no real files, so even
flag-ON is inert). Wii `#else` byte-identical.

## The measurement (why it's the wrong lever)
With `RB3_LOADER_INSTRUMENT=1`, booting to gameplay and holding:
- **I/O is 12.0% of LoadMgr::Poll cost** (`io_wall=0.208s` vs `poll_wall=1.727s`).
- **4,083,540 of 4,085,298 reads are <2 bytes** — DTA-lexer 1-byte reads served from the
  64 KiB stdio buffer (QW-2), i.e. **no syscall**. The loader is **CPU-bound** (DTA parse +
  object-graph construction), not I/O-bound.
- Diverting every read (`RB3_LOADER_ASYNC_ALL=1`) **never reached gameplay in 180 s** —
  throughput collapse from one-read-per-frame yield granularity (not a deadlock; 20,290
  frames rendered). The async path is *functionally* correct (clean boot, clean pthread
  join, exit 0, regression `song-end-test.py` PASSES flag-off AND flag-on) but **slower**
  when it actually diverts.

## Recommendation (handoff)
- **Do not** pursue off-thread reads for native loader perf. Attack the **CPU side**:
  DTA parse, object-graph construction, and the per-frame budgeted `Poll` drain (QW-1).
- The off-thread worker would only pay off for a **genuinely blocking** I/O source (cold-disc
  handheld, or HX_WEB network assets — but web is single-threaded so this worker is a no-op
  there). If ever revisited, the divert must be **coarse/batched** (submit a whole
  dependency-chain's reads, yield once) to amortise the per-read penalty `ASYNC_ALL` exposed.
- The instrumentation (Phase 0) is worth cherry-picking from `wt-c1loader` if/when CPU-side
  loader work begins. The branch is preserved; nothing merged to master.

## Note on validation
Final relink was blocked mid-run by an unrelated concurrent-session engine edit
(`Rnd_Wgpu_RB3.cpp` calls `RndParticleSys::RelativeXfm()`, which exists only in that
session's uncommitted `Part.h`). All 8 C1 TUs compile; validation used the binary built
before that breakage landed.
