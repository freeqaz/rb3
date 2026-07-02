# Lane A handoff — rb3-dta link failure FIXED (research/14 §Lane A)

**Status: LANDED** (2026-07-02). rb3-dta links again, 138-song parse gate green,
rb3-native + rb3-tests unaffected.

## Root cause (confirmed = spec's diagnosis)

The session-telemetry wave added HX_NATIVE hooks to three shared fork TUs that
rb3-dta compiles:

- `src/system/obj/Task.cpp:43,394-396` → `gRB3TraceFrame`, `RB3ReplayDtForFrame`,
  `RB3ReplayFixedClock`, `RB3ReplayActive`
- `src/system/os/Debug.cpp:34,52` → `gRB3TraceActive`, `RB3RecordLog`
- `src/system/os/System.cpp:394,397` → `RB3ReplaySeed`, `RB3TraceSetSeed`

The definitions live in `native/src/rb3_session_trace.cpp` +
`native/src/rb3_replay.cpp`, which the rb3-dta source list (main_dta +
DTA_FORK_SOURCES + DTA_LEXER + NATIVE_SHIMS) did not include → undefined refs at
link.

## Fix taken: spec preference (1) — REAL TUs, not stubs

Dependency audit said the real TUs qualify as "dependency-light":

- `rb3_session_trace.cpp`: std-only (`<cstdio>/<string>/<vector>/<chrono>`…,
  `<emscripten.h>` fully `#ifdef __EMSCRIPTEN__`-guarded). Its only externs are
  the `gFrameTrace*` attribution counters, all DEFINED in
  `src/system/utl/Loader.cpp` — already in DTA_FORK_SOURCES. No HTTP / GLFW /
  render pulls.
- `rb3_replay.cpp`: std-only, same emscripten guard. ONE extern outside that:
  `RB3TraceCurrentSongMs()` from `native/src/rb3_trace_taps.cpp` — and THAT TU
  drags band3 Game/Player/BandUI/UIScreen types rb3-dta does not compile. So the
  taps TU stays out; the one symbol gets a stub.

Changes (rb3 master, commit below):

1. **`native/CMakeLists.txt`** (rb3-dta `add_executable`, ~line 415): appended
   `src/rb3_session_trace.cpp`, `src/rb3_replay.cpp`, and the new
   `src/rb3_trace_taps_stub.cpp`, with a comment block explaining the why.
2. **`native/src/rb3_trace_taps_stub.cpp`** (NEW, rb3-dta-only):
   `__attribute__((weak)) float RB3TraceCurrentSongMs() { return -1.0f; }`
   — -1.0f is the real tap's exact "engine not booted" (TheGame == NULL) value,
   which `rb3_replay.cpp`'s audio-start latch explicitly handles (falls back to
   the recorded-curve signal). Weak so the real tap wins if any future target
   compiles both. A `.s` no-op stub was NOT usable here: the shared
   `__hmx_native_noop_stub` zeroes %eax, but a float returns in %xmm0 → garbage.

No matched Wii TU touched (Task.cpp/Debug.cpp/System.cpp unmodified). No engine
change, no pin bump. rb3-web untouched (the rb3-dta block is inside
`if(NOT EMSCRIPTEN)`; web already compiled both real TUs). Tracing/replay stay
INERT in rb3-dta unless `RB3_SESSION_TRACE`/`RB3_REPLAY` env vars are set — as a
side effect rb3-dta can now actually record a session trace, which is harmless
and arguably useful.

## Gate results

- `cmake --build native/build-native --target rb3-dta` → **links** (previously
  9 undefined refs).
- `./native/build-native/rb3-dta orig-assets/extracted/songs/songs.dta 138`
  (from repo root; note README's `orig-assets/` path is relative to where the
  assets were extracted, which is `rb3/orig-assets/`, not `rb3/native/`) →
  "**Done. Showed 138 song(s).**", exit 0.
- `--target rb3-native --target rb3-tests` → both build + link green.

## Notes for the integrator

- Footprint: exactly 2 files (`native/CMakeLists.txt` +15 lines,
  `native/src/rb3_trace_taps_stub.cpp` new). Nothing else staged.
- Lane B's chunked sharpen fetch does not interact with this — rb3-dta does not
  compile rb3_texsharpen_native.cpp.
