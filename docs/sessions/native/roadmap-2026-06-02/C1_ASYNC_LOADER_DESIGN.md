# C1 — Async Loader: File I/O Off the Render Thread

**Status:** design only. Implementable by a future agent with no extra context.
**Grounding:** `docs/sessions/native/roadmap-2026-06-02/loader-performance.md` (LW-1),
`docs/native/PLAN_LOADER_ASYNC.md`, `src/system/utl/Loader.cpp`,
`src/system/obj/DirLoader.cpp`, `src/system/utl/ChunkStream.cpp`,
`native/src/native_file.cpp`, `src/system/world/Dir.cpp` (force-poll hack).

> QW-1 (budgeted drain, `Loader.cpp:293-352`) and QW-2 (`setvbuf` + cached `Size()`,
> `native_file.cpp:148-160`) ARE ALREADY LANDED. They bound the per-frame stall but do
> NOT overlap I/O with render. This doc is LW-1: the only fix that overlaps load with
> render and lets the `world/Dir.cpp` force-poll hack be removed.

## 1. Where the synchronous read stalls the frame (root cause)

The matched-fork loader is structured for genuine async: `ReadAsync` returns
immediately, `ReadDone` is a non-blocking completion poll, and the state machine
yields the main thread via `TempEof` whenever bytes aren't here yet.

- `ChunkStream::Eof()` (`ChunkStream.cpp:204-206`, `:268-271`) returns `TempEof` when
  `mFile->ReadDone(x) == 0`.
- The state funcs loop on it and yield:
  `FileLoader::LoadStream` (`Loader.cpp:551-555`, `:571-575`),
  `DirLoader::LoadHeader` (`DirLoader.cpp:458-459`),
  `DirLoader::CreateObjects`/`LoadObjs` (`:554`, `:567`, `:660`, `:685`) —
  all `while (Eof()==TempEof) { if (CheckSplit()) return; }`.

On native this collapses because `native/src/native_file.cpp:174-177`
`NativeStdioFile::ReadAsync` is a **blocking `fread`** and `ReadDone` (`:224-228`)
**always returns true**. So `TempEof` never trips, the state machine runs
straight through, and every byte is a host-disk round-trip on the render thread
inside `App::RunOneFrame → SystemPoll → TheLoadMgr.Poll`.

During the `game_screen` background load this monopolizes the frame, so
`Rnd::BeginDrawing` (the only site that advances `mProcCmds`) is rarely reached,
`kProcessPost` starves, and `WorldDir::Poll`'s vignette sequencer almost never
ticks. That is exactly what the `world/Dir.cpp` `IsTv3Dir` HX_NATIVE force-poll
block works around. Fix the I/O → the hack becomes removable.

## 2. Worker-thread architecture

```
Main thread (render)                 Loader thread (NEW, single, "rb3-loader")
  App::RunOneFrame                     while (!terminating):
   SystemPoll                            sem_wait(work)
    TheLoadMgr.Poll  ← now SHORT:        for req in queue (mutex only to deq):
      drives state machines,               fseek(req.offset); fread(req.buf,req.len)
      blocks only on a mutex (µs)          req.result_bytes = got
   TheUI/Task/Synth.Poll                   release-store req.complete = true
   Rnd Begin/Draw/End                      sem_post(completion)
        │ Submit (enqueue+signal)   ▲ Poll/WaitForAny (completion flag)
        └────────────────────────────┘
```

**One thread, not a pool.** Each Loader is a linear chunk-at-a-time pipeline and
`mLoading` is a single-front serial queue; two readers on one disk buy nothing and
add a fairness policy. (Pool is a future option.)

### API (RB3-private, `native/src/rb3_async_loader.{h,cpp}`)
```cpp
struct AsyncReadRequest {
    File *file; void *buffer; int length; long offset;
    volatile bool complete; int result_bytes;
};
namespace AsyncLoaderThread {
    void Init();                      // pthread_create + sem_init (RB3_LOADER_ASYNC)
    void Terminate();                 // set flag, post, join
    void Submit(AsyncReadRequest*);   // lock, push, sem_post(work)
    bool Poll(AsyncReadRequest*);     // non-blocking: returns req->complete (acquire)
    void WaitForAny();                // park on completion cv (PollUntilLoaded backstop)
}
extern bool g_loader_async_enabled;
```
Idioms to copy: `milo-native-engine/src/platform/ThreadCall_Native.cpp`
(pthread + `sem_t`, `#ifndef __EMSCRIPTEN__`, terminate-by-flag-then-post) and
`Skeleton_Native.h:8` (`volatile bool` completion flag — `<atomic>` is unusable with
clang + GCC15 headers in this toolchain; use `volatile bool` + a barrier).

### The whole byte-IO behavior change (`native_file.cpp`)
```cpp
bool NativeStdioFile::ReadAsync(void *buf, int n) override {
    if (g_loader_async_enabled) {
        mPending = { this, buf, n, ftell(mFp), /*complete*/false, 0 };
        AsyncLoaderThread::Submit(&mPending);
        return true;                       // "submitted; check ReadDone"
    }
    mLastReadBytes = Read(buf, n); return mLastReadBytes == n;  // existing path
}
bool NativeStdioFile::ReadDone(int &result) override {
    if (g_loader_async_enabled && mPending.file) {
        if (!AsyncLoaderThread::Poll(&mPending)) return false;  // → TempEof
        result = mPending.result_bytes; mPending.file = nullptr; return true;
    }
    result = mLastReadBytes; mLastReadBytes = 0; return true;   // existing path
}
```
That is the entire change at the byte layer. `ChunkStream::Eof` and the
`LoadStream`/`LoadFile`/`LoadHeader` `TempEof` loops already do the right thing.

### `LoadMgr::Poll` (async-on sub-arm, inside the existing HX_NATIVE arm)
Today's QW-1 budgeted drain loops until the budget. Async-on: break after one
**no-progress** pass (a pass where the front loader did not advance because it is
blocked on `ReadDone`=false). The bytes land while the next frame draws; `Poll`
re-enters next frame. `PollUntilEmpty`/`PollUntilLoaded` keep their synchronous
contract by calling `AsyncLoaderThread::WaitForAny()` on a no-progress pass instead
of busy-spinning, then returning only when `IsLoaded()`.

## 3. The WEB story (parity, no pthreads by default)

Emscripten is single-threaded by default. **Web keeps its already-landed cooperative
slice** (`Loader.cpp:257-291` Poll, `:138-193` PollUntilLoaded): arm an ~8 ms
`unk1c` split, `mTimer.Restart()` per poll, `PollFrontLoader()`, `emscripten_sleep(0)`
to yield an event-loop turn under JSPI, hard iteration cap as a safety valve. This is
the same "yield the main thread between slices" shape as native off-thread, achieved
cooperatively — so native and web stay in parity behaviorally even though only native
has a real second thread.

- All pthread code is `#ifndef __EMSCRIPTEN__`; on web `AsyncLoaderThread::Init` is a
  no-op and `g_loader_async_enabled` stays false, so `ReadAsync` keeps the sync body
  and the web cooperative arm drives yielding. The `native/src/native_file.cpp`
  `__EMSCRIPTEN__` on-demand-fetch path (`:101-141`) is unaffected.
- **Preprocessor ordering invariant:** HX_WEB implies HX_NATIVE; the `#ifdef HX_WEB`
  arm MUST stay BEFORE the `#ifdef HX_NATIVE` arm in both `Poll` and `PollUntilLoaded`
  so web wins. Do not reorder.
- **Feasibility note (out of scope for V1):** emscripten pthreads + SharedArrayBuffer
  + a worker would let web use the SAME `rb3_async_loader.cpp` thread, but requires
  COOP/COEP headers, a pthread-pool build flag, and SAB-safe MEMFS access — a separate
  effort. Cooperative-slice is the parity story for V1.

## 4. Files by layer — and what avoids the matched fork

RB3 **excludes** the engine's `AsyncFile_Native.cpp` / `File_Native.cpp` /
`ThreadCall_Native.cpp` (`native/CMakeLists.txt:153-164`, header-shape mismatch) and
routes `NewFile` → `HmxNativeOpenFile` in `native/src/native_file.cpp`
(`os/File.cpp:155,168`). **Therefore the byte-IO change and the worker thread live
entirely in RB3-private `native/src/**` (layer c) and require NO engine-pin bump.**

| File | Layer | Change |
|---|---|---|
| `native/src/rb3_async_loader.{h,cpp}` | c | NEW worker thread (pthread). RB3-private, clang. |
| `native/src/native_file.cpp` | c | `ReadAsync`/`ReadDone` async branch + `mPending` member. No asm-match. |
| `src/system/utl/Loader.cpp` | a | Async-on sub-arm in the existing HX_NATIVE `Poll`; `WaitForAny` in `PollUntilLoaded`/`PollUntilEmpty`. Additive in existing `#ifdef`. |
| `src/system/obj/DirLoader.cpp` | a | Async-on sub-arm in existing HX_NATIVE `PollLoading`. Additive. |
| `src/system/os/System.cpp` | a | `AsyncLoaderThread::Init()` after `ThreadCallInit()` (`:717`); Terminate exit-callback. Additive in HX_NATIVE. |
| `src/system/world/Dir.cpp` | a | FINAL: remove the `IsTv3Dir` force-poll block. |

All `src/**` edits are ADDITIVE inside existing `#ifdef HX_NATIVE` arms with a
byte-identical Wii `#else`; the Wii MWCC build is unaffected. **Do NOT edit
`src/system/rndobj/Dir.cpp`** (base `RndDir::Poll`, concurrently edited — it is just a
loop over `mPolls` and is not the hack). The hack is `WorldDir::Poll` in
`src/system/world/Dir.cpp`. `main_native.cpp` is concurrently edited — prefer owning
Init/Terminate in `System.cpp` to avoid collision.

**Engine-shared alternative (NOT recommended for V1):** put the worker in
`milo-native-engine/src/platform/AsyncLoaderThread_Native.cpp` next to
`ThreadCall_Native.cpp`. This requires registering it in `MILO_ENGINE_PLATFORM_SOURCES`
AND removing it from RB3's `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`, plus an engine-pin
bump — and only buys sharing with DC3, which already has its own AsyncFile. Keep it
RB3-private.

## 5. Phased rollout + acceptance test (headless harness)

Harness: `RB3_GAME=1 RB3_HTTP=1 RB3_HTTP_PORT=<unique 9100+> MILO_HEADLESS=1
RB3_DATA=<repo>/orig-assets/extracted native/build-native/rb3-native`.
Always `ss -tlnp | grep <port>` first; kill only your own PID. Probe `/api/health`
(`currentScreen`, `songMs`, frame counter), `/api/screenshot`, `/api/dta/eval`.
Build ONLY in a worktree (`tools/setup-worktree.sh`), never the main build dir.

- **Phase 0 — instrument (gate).** Add `RB3_LOADER_INSTRUMENT` (does not exist yet
  despite the env name) HX_NATIVE-additive timers around `LoadMgr::Poll`, per-`Read`
  byte/time counters, per-frame `kProcessPost`-advance counter. CONFIRM I/O fraction
  of `Poll` > 50% (else LW-2 decompression-off-thread is needed first). Acceptance:
  histogram printed on exit. Risk: none (measurement).
- **Phase 1 — land worker, disabled.** `rb3_async_loader.{h,cpp}` + Init/Terminate;
  self-test reads a known small file off-thread on Init. Acceptance: boots cleanly with
  `RB3_LOADER_ASYNC` unset (zero change) AND set (thread idle, clean join at
  `MILO_MAX_FRAMES=10`).
- **Phase 2 — route ONE load.** Path-filter `ReadAsync` to divert only
  `game_screen`-matching files. Add the async-on sub-arms in Loader/DirLoader.
  Acceptance: with `RB3_LOADER_ASYNC=1`, the Phase-0 `kProcessPost`-per-second counter
  goes UP during the song-load hold (force-poll hack STILL in place); screenshots
  through splash→tv3→in-song match the pre-change golden.
- **Phase 3 — route all.** Drop the path filter. Acceptance: full boot + multi-song
  sweep, no crashes/visual regressions; web build (HX_WEB) compiles+runs unchanged.
- **Phase 4 — remove force-poll.** Delete the `world/Dir.cpp` `IsTv3Dir` block.
  Acceptance (THE headline metric): with `RB3_LOADER_ASYNC=1` and the hack gone, the
  song-load `songMs`-held-at-0 window shrinks AND the tv3 vignette visibly animates
  during it (per-second frame delta from `/api/health` stays above a floor, not the
  ~14 fps baseline); `RB3_TV3_PLAY_OFF` becomes dead.
- **Phase 5 — default on.** Flip `RB3_LOADER_ASYNC=1` default; env becomes the bail-out.

Reversible at every phase via `RB3_LOADER_ASYNC` unset.

## 6. Thread-safety hazards — what MUST stay main-thread

The engine is single-threaded by assumption. The worker touches ONLY:
`AsyncReadRequest::buffer` bytes, `result_bytes`, and the `complete` flag (one
release-store on the worker, one acquire-load on main — the canonical async-IO
handoff). Everything else stays main-thread:

- **Heap context** — `MemPushHeap(front->mHeap)`/`MemPopHeap` (`Loader.cpp:377,388`).
  The worker never allocates; it freads into a buffer the main thread already
  allocated. Keep push/pop main-thread.
- **Object construction + fixups** — `NewObject`, `PreLoad`/`PostLoad`,
  ObjectDir-relative ptr fixups, factory lookups (`DirLoader::CreateObjects`/`LoadObjs`)
  stay main-thread (the whole CPU half of `Poll`).
- **GPU upload** — any `RndTex`/mesh upload to the WgpuRnd backend MUST stay
  main-thread; the worker does file bytes only, never GPU.
- **Decompression** — `gDecompressionQueue` (`ChunkStream.cpp` `DecompressChunkAsync`/
  `PollDecompressionWorker`) is CPU work, stays main-thread in V1 (LW-2 only if Phase 0
  shows decompress dominates).
- **Buffer race** — safe by the matched-fork contract: `ChunkStream` does not swap
  `mCurBufferIdx` to a buffer until `ReadDone` returns true, so main never reads a
  buffer the worker is writing.
- **`IsLoaded()`/`mState`** — only main-thread `Poll` advances `mState`; the worker
  never touches it. Race-free by construction.
- **Termination order** — `AsyncLoaderThread::Terminate()` MUST join the worker BEFORE
  any `NativeStdioFile` is destroyed and before `ThreadCallTerminate`. Register via the
  exit-callback chain (template: `RB3AudioTerminateExitCallback`, `main_native.cpp:620`).
- **Audio thread** — independent (miniaudio RT); clips are `PollUntilLoaded`'d before
  play, so it never touches the loader. No new sync.
- **Nested loads / re-entrant `PollUntilLoaded`** — same risk profile as today (all on
  main thread); the `WaitForAny` spin just yields cycles to background loads. No new
  deadlock; cap the spin as a safety valve.
