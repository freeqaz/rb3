# Loader Performance — Kill Long Load Times + Per-Frame Stalls

**One-line verdict:** The native loader does all file I/O as a *blocking `fread` on the
render thread* and drains the whole queue per frame, so boot (`PollUntilEmpty`) and per-song
loads (tv3→game_screen) freeze the frame loop and background char-merger loads peg the splash
screen at ~14 fps; the existing `PLAN_LOADER_ASYNC.md` is the correct long-term fix, but three
quick wins (read buffering, time-budgeted draining, dropping the redundant boot character preload)
ship most of the win in < 1 day each.

---

## 1. Current state — what works vs what's stubbed/missing

### 1.1 The byte-IO layer is synchronous-blocking (root cause)

`native/src/native_file.cpp` is the only native `File` backend. Its `ReadAsync` is a blocking
`Read` (which is `fread`), and `ReadDone` always reports "done":

- `native/src/native_file.cpp:148-152` — `Read()` → `std::fread(buf, 1, n, mFp)` (blocking).
- `native/src/native_file.cpp:153-156` — `ReadAsync(buf, n)` → just calls `Read()` synchronously
  and stores `mLastReadBytes`. (NOTE: the design doc's line numbers 69-72 are stale; this is the
  live location.)
- `native/src/native_file.cpp:199-203` — `ReadDone(result)` → returns `true` immediately.
- `native/src/native_file.cpp:189-197` — `Size()` does `fseek(END)`+`ftell`+`fseek(back)` on
  *every* call; `OpenFile` calls it once per file, `Eof()` calls it on the position test.

Because `ReadAsync` finishes instantly, the entire async cooperation machinery the matched-fork
loader is built around collapses into a synchronous straight-line run on the render thread:

- `ChunkStream::Eof()` (`src/system/utl/ChunkStream.cpp:203-285`) — the `kReading`/`kDecompressing`
  double-buffer state machine never sees an in-flight read; `ReadDone(x)` at line 207/268 is always
  true, so `DecompressChunkAsync` + `ReadChunkAsync` + `PollDecompressionWorker` all run back-to-back
  in one call. Decompression (`gDecompressionQueue`) is drained on the *same* thread
  (`ChunkStream.cpp:271, 384-393`).
- The `TempEof` yield (`FileLoader::LoadStream` `src/system/utl/Loader.cpp:506-510`, `DirLoader::LoadHeader`
  `src/system/obj/DirLoader.cpp:457-461`) — designed as the "bytes not here yet, come back next Poll"
  cooperation point — never trips, so the loader never yields mid-file.

### 1.2 The loader drains to completion per `Poll` (no time budget)

- `LoadMgr::Poll` HX_NATIVE arm `src/system/utl/Loader.cpp:289-307` sets `unk1c = 1e30f` (disables
  the per-state `CheckSplit()` budget) and loops `while (!mLoading.empty()) PollFrontLoader()`. So a
  single `TheLoadMgr.Poll()` from `SystemPoll` blocks the frame for the *entire* queue.
- `FileLoader::PollLoading` HX_NATIVE arm `src/system/utl/Loader.cpp:486-497` and
  `DirLoader::PollLoading` HX_NATIVE arm `src/system/obj/DirLoader.cpp:242-259` each advance one state
  step per call but with `CheckSplit()` disabled the calling `Poll` loop keeps spinning them to
  `DoneLoading`.
- `PollUntilEmpty` `src/system/utl/Loader.cpp:212-222` (boot) and `PollUntilLoaded`
  `src/system/utl/Loader.cpp:190-210` (per-asset) both also set `unk1c = 1e30f`.

### 1.3 Measured behavior (live, read-only run on port 9148, this session)

Clean single-instance headless run (`RB3_GAME=1 MILO_HEADLESS=1`):

- **Boot cost:** HTTP server (which comes up only *after* the App-ctor `PollUntilEmpty` at
  `src/App.cpp:428`) became reachable ~4.5 s after process start on a warm disk cache.
- **Boot character burst:** the log shows a huge load burst *between* "frame 1 complete" and
  "frame 2 complete" — 17 distinct `char/*` milos deserialized (`char/crowd/crowd_{male,female}0[1-4]`,
  `char/extras/{male,female}_extras*`, `char/main/shared/colorpalettes.milo`), 187 skinned-mesh
  re-export NOTIFYs. The 20 MB `char/main/shared/gen/colorpalettes.milo_xbox` is in this set
  (confirmed `du`: 20,838,030 bytes).
- **Per-frame stall on splash:** the game sits on `splash_screen` for 690+ frames at a steady
  ~14-15 fps (≈68 ms/frame). `RunOneFrame` (`src/App.cpp:473-544`) has *no* sleep, so ~68 ms is real
  work: `SystemPoll` (→ `TheLoadMgr.Poll`, draining background crowd/extras FileMerger loads) +
  `TheUI.Draw`. Steady-state splash has *no* recurring load chatter once the burst finishes, so the
  residual ~68 ms there is render-bound (out of this area's scope — see §6), but the burst frames and
  the per-song transition are loader-bound.
- **Per-song load:** entering a song goes `splash → tv3_a_screen (songMs 0, held) → game_screen
  (songMs 0, held ~tens of frames) → songMs advances ~70 fps`. The held-at-songMs-0 window is the
  per-song load (game_screen PanelDir + venue + band chars + .mid/.mogg) draining on the render thread
  — this is exactly the `WorldDir::Poll` force-poll workaround's motivating case
  (`src/system/world/Dir.cpp:138-155`).

### 1.4 What works

- Loads *do* complete and the game is playable — correctness is fine.
- The web build already has a cooperative-slice + `emscripten_sleep(0)` yield arm in `LoadMgr::Poll`
  / `PollUntilLoaded` (`src/system/utl/Loader.cpp:253-287, 134-189`) — proof the budgeted-drain shape
  works and a template for the native quick win.
- `ThreadCall_Native.cpp` worker thread already exists (started from `SystemInit`), and
  `Skeleton_Native.{h,cpp}` is a working `std::thread`+mutex+double-buffer reference — the async
  infra is half-built.

### 1.5 What is missing / stubbed

- No off-render-thread I/O. No `AsyncFile_Native.cpp` / `File_Native.cpp` engine override for rb3
  (DC3 has `AsyncFileNative.cpp`; rb3 uses `NativeStdioFile` directly).
- No per-poll time/byte budget on native; the queue drains in one call.
- No read buffering — every `fread` is an unbuffered host syscall sized exactly to the chunk.
- A redundant full character-roster preload runs at boot (the crowd/extras burst), most of which is
  not needed before the main hub renders.

---

## 2. Goal — desired/retail behavior

Retail Wii is **genuinely async** (`os/AsyncFileCNT.cpp:92-110` spawns a per-read `OSCreateThread`;
`AsyncFileWii` uses DVD-hardware DMA completion). `ReadAsync` returns immediately, `ReadDone` polls a
completion flag, and `LoadMgr::Poll` runs only the CPU-bound state work that fits a per-frame budget
(`CheckSplit()` on `mPeriod`), yielding to the frame loop when bytes are in flight (the `TempEof`
loop). Target native behavior:

- **Boot:** first interactive screen (main hub) in ~1-2 s, not ~4.5 s, by not blocking the whole
  preload synchronously and by skipping unneeded preloads.
- **Per-song load:** the game_screen/venue/char load overlaps with frame rendering so the transition
  vignette animates at a real frame rate, and the `kProcessPost`-driven sequencer is not starved —
  letting the `WorldDir::Poll` force-poll hack (`src/system/world/Dir.cpp:138-155`) be removed.
- **Per-frame:** no frame should lose >1 frame-budget to the loader; background loads (deferred-binding
  textures, crowd chars) trickle in over many frames instead of one stall.

---

## 3. Proposed approach — phased, layer-tagged

Distinguish the three cost classes; each gets its own fix:

| Cost class | Where | Fix | Priority |
|---|---|---|---|
| One-time BOOT | App-ctor `PollUntilEmpty` (`App.cpp:428`) + crowd/extras burst | QW-3 (drop redundant preload) + QW-2 (buffer) | P1 |
| Per-SONG LOAD | tv3→game_screen drain on render thread | LW-1 (async I/O off thread) is the real fix; QW-1 (time budget) softens it | P0 for QW-1, P1 for LW-1 |
| Per-FRAME STALL | background FileMerger crowd loads during splash | QW-1 (time-budget the drain) | P1 |

### Phase QW-1 — Time-budget the native drain (quick win) — layer (a)

Replace the unconditional `unk1c = 1e30f` drain in `LoadMgr::Poll` HX_NATIVE arm
(`src/system/utl/Loader.cpp:289-307`) with the SAME cooperative-slice loop the web arm already uses
(`Loader.cpp:253-287`): set `unk1c = kSliceMs` (start ~8 ms), `mTimer.Restart()` before each
`PollFrontLoader()`, and `break` once the elapsed budget is exceeded — but WITHOUT the
`emscripten_sleep`. The frame loop returns from `SystemPoll`, draws a frame, and re-enters `Poll`
next frame; bytes still arrive synchronously but the per-frame loss is bounded.
- Keep `PollUntilEmpty` / `PollUntilLoaded` draining to completion (they are synchronous-contract
  callers) — only the global `SystemPoll → TheLoadMgr.Poll` becomes budgeted.
- Gate behind an env (`RB3_LOADER_BUDGET_MS`, default 8) so it is tunable/reversible. This is
  HX_NATIVE-additive inside the existing `#ifdef HX_NATIVE` arm — the Wii `#else` path
  (`Loader.cpp:308-318`) is the time-budgeted source of this shape, so this is a faithful port.
- RISK: a fresh-front loader whose state func lacks a `CheckSplit() → return` guard could spin within
  one `PollFrontLoader`. The matched-fork state funcs (`DirLoader::CreateObjects` `DirLoader.cpp:554,567`,
  `LoadObjs` `DirLoader.cpp:685`) DO have these guards — but with `unk1c` now small they will fire,
  which is the intended yield. Validate with a frame-time histogram (see §8).

### Phase QW-2 — Buffer reads + cache file size (quick win) — layer (c)

In `native/src/native_file.cpp`:
- `setvbuf(mFp, nullptr, _IOFBF, 1<<16)` after `fopen` so the kernel-syscall count drops (chunk
  reads are 64 KiB; a fully-buffered stdio stream coalesces). Layer (c), pure native shim.
- Cache `Size()`: store the file length once (in the ctor via one `fseek/ftell`) instead of the
  three-`fseek` dance on every `Size()`/`Eof()` call. `Eof()` (`native_file.cpp:175-187`) and
  `OpenFile` both hammer `Size()`. Add `long mSize;` member, compute once.
- Low risk; no semantic change.

### Phase QW-3 — Defer/skip the redundant boot character preload (quick win) — layer (a) or (c)

The 17-milo crowd/extras + 20 MB colorpalettes burst (§1.3) happens before the first interactive
screen. Originates from the character/crowd `FileMerger` path (`src/system/char/FileMerger.cpp:94-128`
`StartLoadInternal` → synchronous `while (!mFilesPending.empty()) TheLoadMgr.Poll();` when `mAsyncLoad`
is false at line 123-125) and the `PrefabChar` portrait loads (`CharData.cpp:78-89`).
- Investigate whether the crowd/extras roster is required before `main_hub_screen`. If the merger is
  invoked with `b1 == false` (synchronous) at boot, flipping it to async (`AddFileMerger` path,
  `FileMerger.cpp:119-120`) lets it trickle in over frames. This is an HX_NATIVE-additive guard at the
  StartLoad call site (find via `grep -rn StartLoad src/band3 src/system/char`).
- Cheaper variant: gate the crowd preload behind "first hub reached" so boot doesn't pay for it.
- RISK: a downstream consumer may assume the roster is resident; verify the main-hub character
  preview still renders. Stage behind an env (`RB3_DEFER_CROWD_PRELOAD`).

### Phase LW-1 — File I/O off the render thread (larger work) — layers (b)+(a)

This is `PLAN_LOADER_ASYNC.md` restated concretely; it is the real fix for per-song load.

The cooperation point is **`ReadDone` returning `false` → `ChunkStream::Eof`/`BinStream::Eof`
returns `TempEof` → the state func's `if (CheckSplit()) return;` yields the main thread**
(`src/system/utl/Loader.cpp:506-510`; `src/system/obj/DirLoader.cpp:457-461`). Today that loop never
yields because `ReadDone` is always true. The plan makes `ReadAsync` enqueue to a loader thread and
`ReadDone` poll a completion flag, so `TempEof` finally fires.

- (b) NEW `milo-native-engine/src/platform/AsyncLoaderThread_Native.{h,cpp}`: one pthread, a FIFO
  `AsyncReadRequest` queue (`{File*, buf, len, offset, volatile bool complete, int result_bytes}`),
  `Submit`/`Poll`/`Init`/`Terminate`. Copy the pthread+semaphore idiom from `ThreadCall_Native.cpp`
  and the `volatile bool` completion idiom from `Skeleton_Native.h`. (b) is SHARED by 3 decomps — but
  it is a new file, so additive; only register it in the engine CMake platform list.
- (a) `native/src/native_file.cpp` `ReadAsync`/`ReadDone`: new branch gated on
  `g_loader_async_enabled` (env `RB3_LOADER_ASYNC=1`) that parks/`Submit`s the request and reports
  `ReadDone` from the completion flag; the existing synchronous body stays as the `#else` / disabled
  path. native_file.cpp has no matched-fork analog, so no asm-match concern.
- (a) `src/system/utl/Loader.cpp` + `src/system/obj/DirLoader.cpp`: the existing HX_NATIVE arms in
  `LoadMgr::Poll` / `FileLoader::PollLoading` / `DirLoader::PollLoading` get an async-on sub-arm that
  *honors* `TempEof` (returns from the state func when `ReadDone` is false) instead of draining. The
  legacy drain is the async-off fallback. All inside existing `#ifdef HX_NATIVE` brackets.
- (a) `src/system/os/System.cpp`: `AsyncLoaderThread::Init()` after `ThreadCallInit` (SystemInit,
  ~`System.cpp:717`), `Terminate()` in the exit-callback chain (template:
  `main_native.cpp` audio-terminate exit callback).
- **Main-thread-only invariants that make this hard** (do NOT move off-thread): the loader heap
  context (`MemPushHeap(front->mHeap)` / `MemPopHeap`, `Loader.cpp:332,343`) and "current dir"
  (`ObjectDir`-relative ptr fixups in `PreLoad`/`PostLoad`, `DirLoader::LoadObjs`) stay on the main
  thread. Only the raw `fread`-into-buffer moves. Decompression (`gDecompressionQueue`) is CPU work,
  also stays main-thread in V1 (defer; `PLAN_LOADER_ASYNC.md §5.8`).
- Then Phase 4 of the design doc: remove the `WorldDir::Poll` force-poll block
  (`src/system/world/Dir.cpp:138-155`).

### Phase LW-2 (optional) — Decompression off-thread — layer (a)

Only if §8 instrumentation shows decompression (not I/O) dominates `LoadMgr::Poll` time. Move
`gDecompressionQueue` draining to a worker. Larger refactor; defer (`PLAN_LOADER_ASYNC.md §3.5`).

---

## 4. Key files

- `native/src/native_file.cpp` — the only native `File` backend; blocking `ReadAsync` (153-156),
  always-true `ReadDone` (199-203), 3-fseek `Size()` (189-197). **QW-2 + LW-1 edits here.**
- `src/system/utl/Loader.cpp` — `LoadMgr::Poll` HX_NATIVE drain (289-307), web budgeted arm
  (253-287, the QW-1 template), `PollUntilLoaded`/`PollUntilEmpty`/`PollFrontLoader`,
  `FileLoader::PollLoading` HX_NATIVE arm (486-497), `LoadStream`/`LoadFile` `TempEof` loops.
- `src/system/obj/DirLoader.cpp` — `DirLoader::PollLoading` HX_NATIVE arm (242-259); state funcs
  `OpenFile`(357) `LoadHeader`(455) `CreateObjects`(513) `LoadObjs`(654) with their `CheckSplit()`
  guards (554, 567, 459, 685).
- `src/system/utl/ChunkStream.cpp` — `Eof()` (203-285) async double-buffer state machine,
  `ReadChunkAsync`(179), `DecompressChunkAsync`(356)/`PollDecompressionWorker`(384) main-thread
  decompress queue. This is the machinery the LW-1 `TempEof` revival re-activates.
- `src/system/char/FileMerger.cpp` — `StartLoadInternal` (94-128) synchronous boot drain at 123-125;
  `LaunchNextLoader`(213)/`FinishLoading`(365)/`PostMerge`(397). **QW-3 origin.**
- `src/band3/meta_band/CharData.cpp` — `PrefabChar::PollLoadingPortrait` `TheLoadMgr.Poll()` (81).
- `src/system/world/Dir.cpp` — `WorldDir::Poll` force-poll workaround (138-155); removable after LW-1.
- `src/App.cpp` — App ctor `PollUntilEmpty` (428, the BOOT cost); `RunOneFrame` (473-544, no sleep,
  the per-frame anatomy).
- `docs/native/PLAN_LOADER_ASYNC.md` — the 55 KB design doc; LW-1 is its concrete restatement.
- `milo-native-engine/src/platform/{ThreadCall_Native.cpp,Skeleton_Native.h}` — reference idioms for
  the new `AsyncLoaderThread`.
- `native/src/rb3_http_handlers.cpp:349-356` — `/api/health` `songMs` source (verification harness).

---

## 5. Quick wins (< 1 day each) vs larger work

**Quick wins (ship < 1 day each, low-to-medium risk):**
- **QW-1** Time-budget `LoadMgr::Poll` native drain (port the web arm, no sleep). Bounds every
  per-frame loader stall to ~8 ms. ~30 LOC, HX_NATIVE-additive. **Highest value/effort.**
- **QW-2** `setvbuf` 64 KiB + cache `Size()` in `native_file.cpp`. Cuts syscall count + removes the
  per-`Eof` triple-fseek. ~15 LOC, layer (c), near-zero risk.
- **QW-3** Defer/skip the boot crowd/extras + colorpalettes preload until the first hub. Removes the
  biggest single boot burst from the critical path. ~20 LOC + an audit; env-gated.

**Larger work:**
- **LW-1** Async I/O off the render thread (`PLAN_LOADER_ASYNC.md`, ~8-14 person-days). The only fix
  that makes per-song load overlap rendering and lets the tv3 force-poll hack be removed.
- **LW-2** Decompression off-thread (optional, gated on §8 measurement).

---

## 6. Dependencies & risks

- The shared build dir + matched fork are touched by concurrent agents; QW-1/QW-3 edit matched-fork
  `src/` files (HX_NATIVE-additive only) — coordinate / use a worktree.
- **Scope boundary:** the steady ~14 fps on `splash_screen` *after* the boot burst is render-bound,
  not loader-bound (no recurring load chatter in the log). It is a separate render/draw-cost area;
  this spec only owns the load bursts and per-song transition. Do not chase the splash residual here.
- QW-1 risk: a state func without a `CheckSplit()` yield could spin within one `PollFrontLoader`
  iteration. Mitigation: a max-iterations safety valve like the web cap (`Loader.cpp:152`).
- QW-3 risk: a downstream consumer assuming the crowd roster is resident at boot. Mitigation:
  env-gate + verify main-hub char preview renders.
- LW-1 risk: races on the read buffer (TSan), unaudited `AddLoader`-then-deref-next-frame callers,
  termination ordering (join loader thread before `NativeStdioFile` teardown). All enumerated in
  `PLAN_LOADER_ASYNC.md §5`.
- Web build: LW-1 engine pthread paths must be `#ifndef __EMSCRIPTEN__`; web keeps its
  `emscripten_sleep` arm. QW-1 must not regress the web arm (web arm comes first in the `#ifdef`
  chain — keep that ordering).

---

## 7. Effort & priority

- **QW-1** — P0 — 0.5 day. Biggest immediate win on per-frame + per-song stalls; reversible env-gate.
- **QW-2** — P1 — 0.25 day. Cheap syscall reduction.
- **QW-3** — P1 — 0.5-1 day (incl. audit). Removes the largest boot burst.
- **LW-1** — P1 — 8-14 person-days (per design doc). The architectural fix; sequence after the QWs
  confirm the I/O-vs-CPU split (its Phase 0 == §8 here).
- **LW-2** — P2 — 3-5 days, only if measured.

Recommended order: QW-2 → QW-1 → (instrument, §8) → QW-3 → LW-1.

---

## 8. Verification plan (native harness; read-only repro pattern used this session)

Launch (unique port, headless):
```
RB3_GAME=1 RB3_HTTP=1 RB3_HTTP_PORT=<9100+> MILO_HEADLESS=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  native/build-native/rb3-native > /tmp/rb3_loader.log 2>&1 &
```
Always `ss -tlnp | grep <port>` first — multiple agents bind the same fixed ports under
SO_REUSEADDR and `curl` will round-robin between instances (observed this session: frame counter
oscillated between two runs). Use a port no one else holds, and kill ONLY your own PID.

**Boot cost (QW-3):** time process-start → `/api/health` first responds (server comes up after
App-ctor `PollUntilEmpty`). Baseline this session ≈4.5 s warm. Target: lower after QW-3.
`grep -c "Skinned mesh needs to be re-exported" /tmp/rb3_loader.log` and
`grep -o '(char/[^)]*\.milo)' ... | sort -u | wc -l` quantify the boot char burst (baseline: 187
NOTIFYs / 17 milos incl. colorpalettes). After QW-3 these should be absent or deferred.

**Per-frame stall (QW-1):** poll `/api/health` once/sec and compute frame-delta-per-second
(frames/Δt). Baseline this session: ~14-15 fps held across the splash/boot-burst window. After QW-1
the load-burst frames should no longer drop below a configurable floor; steady-state splash is
render-bound and unaffected (that is the negative control proving QW-1 hit the loader, not the
renderer).

**Per-song load (QW-1 + LW-1):** watch the `currentScreen` + `songMs` progression
`splash → tv3_a_screen → game_screen (songMs 0 held) → songMs advancing`. Time the
songMs-held-at-0 window (the load). After QW-1 it shrinks; after LW-1 the tv3 vignette should
animate during it (the `kProcessPost` starvation that motivated `world/Dir.cpp:138-155` is gone).

**Phase-0 instrumentation (gate for LW-1, `PLAN_LOADER_ASYNC.md §6 Phase 0`):** add HX_NATIVE-additive
wall-clock timers (env `RB3_LOADER_INSTRUMENT=1`) around `LoadMgr::Poll`, per-`NativeStdioFile::Read`
byte/time counters, and a per-`kProcessPost`-frame counter. Confirm the I/O fraction of `LoadMgr::Poll`
is >50% (if not, LW-2 is needed first). This decides whether LW-1 alone is sufficient.

**Regression:** screenshot the splash → tv3 → in-song sequence (`/api/screenshot`) and diff vs a
pre-change golden; full boot + song-load with each env unset (zero behavior change) then set.
