# PLAN: Move TheLoadMgr off the render thread

**Status:** design only, pre-implementation
**Author:** native-port team
**Last touched:** 2026-05-29
**Origin commit:** `acdfc69f` (native: tv3 transition cinematic default-on)
**Sibling docs:** `NATIVE_PORT_ROADMAP.md`, `NATIVE_PORT_INVENTORY.md`,
`../sessions/native/VIGNETTE_RENDER_SCOPE.md`

## 0. TL;DR

The `WorldDir::Poll` HX_NATIVE force-poll block at
`src/system/world/Dir.cpp:147-164` is a workaround. It bypasses the retail
`kProcessPost` throttle for `tv3` / `/transition/` WorldDirs because, during
the `game_screen` background load, `TheLoadMgr.Poll()` consumes most of the
frame budget on the main thread, so `Rnd::BeginDrawing` is rarely called and
the `kProcessPost` cycle that drives `vignette_transition`'s sequencer almost
never fires (observed ~70-130 frames out of 1500-2400, far below the nominal
1/3).

The right convergence is the same architectural model retail Wii uses:
**file I/O off the render thread**. Retail Wii is genuinely asynchronous (see
§2 for the evidence — `AsyncFileCNT` spawns a per-read `OSCreateThread`,
`AsyncFileWii` uses Wii DVD-hardware async completion), not "fast enough that
it doesn't matter". The matched-fork `Loader::PollLoading` is structured
around the assumption that `File::ReadAsync` returns immediately and
`File::ReadDone` is a non-blocking poll.

On native we converted `ReadAsync` into a blocking `fread` (see
`native/src/native_file.cpp:69-72`), so every byte read on the loader's
state-machine tick is a host-disk round-trip on the render thread. To
restore retail parity we must:

1. Run the **bytes-to-memory + decompress** work on a dedicated loader
   thread.
2. Keep the **object construction / `PreLoad` / `PostLoad`** work on the
   main thread (this is where the matched-fork code is full of "current
   heap", "current dir", "TempEof yield" assumptions that are hostile to
   off-thread execution).
3. Repurpose the existing `BinStream::Eof()` `TempEof` yield as the
   main-thread cooperation point — exactly what it was designed for.

Estimated effort: **8–14 person-days** (high confidence on the file-IO
half, lower confidence on rooting out all the "load runs in this heap" /
"don't load while polling" implicit invariants — see §6).

Once landed, the WorldDir::Poll HX_NATIVE block, the
`FileLoader::PollLoading` HX_NATIVE state-step block
(`src/system/utl/Loader.cpp:487-502`), the `DirLoader::PollLoading`
HX_NATIVE state-step block (`src/system/obj/DirLoader.cpp:243-259`), and
the `LoadMgr::Poll` HX_NATIVE drain block
(`src/system/utl/Loader.cpp:289-307`) can all be reverted, or at least
narrowed to one tightly-scoped place. The plan is structured so that the
revert is the LAST phase, and the on/off seam is a runtime env var
(`RB3_LOADER_ASYNC=1`) so the new path can be canary'd next to the old.

---

## 1. Current native loader architecture

### 1.1 The participants

| Thread | Owns | Today |
|---|---|---|
| Main (render) | `App::RunOneFrame`, all `*::Poll`, all `*::Draw`, `TheLoadMgr.Poll`, all `*Loader::PollLoading`, `BinStream` reads, `File::Read*` | every blocking call lives here |
| miniaudio RT | `MaDataCallback → AudioDevice::MixSources → src->RenderAudio` (`milo-native-engine/src/audio/AudioDevice.cpp:119`) | a real second thread; reads audio source state under no explicit lock |
| ThreadCall worker | `ThreadCall_Native.cpp:39 WorkerMain` — single worker, semaphore-fed ring buffer of 12 jobs; HX_NATIVE only starts it from `SystemInit` (System.cpp:717) | EXISTS but underused; AsyncFileCNT-style off-thread pattern is already imported |
| Skeleton reader | `Skeleton_Native.cpp` — `std::thread` + `std::mutex` + double-buffer | reference pattern for new code |

### 1.2 The data flow on a single Poll

The single-frame anatomy of a load step today (everything below runs on the
main thread inside `App::RunOneFrame → SystemPoll → TheLoadMgr.Poll`):

```
LoadMgr::Poll                        utl/Loader.cpp:248-319
  unk1c = 1e30f                      utl/Loader.cpp:299  (HX_NATIVE drain)
  while (!mLoading.empty())
    PollFrontLoader                  utl/Loader.cpp:321-345
      front->PollLoading             dispatches to FileLoader or DirLoader
        FileLoader::PollLoading      utl/Loader.cpp:486-502
          (this->*mState)()          mState = OpenFile / LoadStream / LoadFile / DoneLoading
            FileLoader::OpenFile     utl/Loader.cpp:413-446
              NewFile                routes to NativeStdioFile (native_file.cpp:132)
              fopen("rb")            * BLOCKS on host disk *
              mFile->ReadAsync       native_file.cpp:69 → fread BLOCKS
            FileLoader::LoadFile     utl/Loader.cpp:452-463
              mFile->ReadDone        native_file.cpp:115 — already complete
        DirLoader::PollLoading       obj/DirLoader.cpp:242-264
          (this->*mState)()          OpenFile / LoadHeader / CreateObjects / LoadResources / LoadDir / LoadObjs / DoneLoading
            DirLoader::OpenFile      obj/DirLoader.cpp:357-412
              new ChunkStream        creates the ChunkStream + opens its underlying File
                File::ReadAsync      first 0x800-byte chunk-info header read; BLOCKS
            DirLoader::LoadHeader    obj/DirLoader.cpp:455-511
              mStream->Eof loop      utl/Loader.cpp inner — waits for chunk to arrive
                ChunkStream::Eof     utl/ChunkStream.cpp:203-285
                  mFile->ReadDone    polls the (already complete) read
                  DecompressChunkAsync utl/ChunkStream.cpp:356-382 — enqueues into gDecompressionQueue (in-main-thread)
                  ReadChunkAsync     utl/ChunkStream.cpp:179-201 — starts next chunk's read (BLOCKS)
                  PollDecompressionWorker utl/ChunkStream.cpp:384-393 — drains gDecompressionQueue ON THE MAIN THREAD
            DirLoader::CreateObjects obj/DirLoader.cpp:513-576
              while (mCounter--):
                *mStream >> classSym, ReadString    — reads from ChunkStream; each ChunkStream::Eof can pull a new chunk (BLOCKS)
                Hmx::Object::NewObject              — ALLOCATIONS in the loader's heap
            DirLoader::LoadObjs      obj/DirLoader.cpp:654-694
              while (!mObjects.empty()):
                obj->PreLoad, obj->PostLoad         — object-graph deserialization; allocations, factory lookups, ObjectDir-relative ptr fixups
                ReadDead                            — scan past 0xADDEADDE
```

### 1.3 Who polls `TheLoadMgr` today

`grep -rn TheLoadMgr\.Poll src/`:

| Caller | Where | Behavior |
|---|---|---|
| `SystemPoll` | `os/System.cpp:678` | Called every frame from `App::RunOneFrame:475`. On native this drains the entire queue (HX_NATIVE `unk1c = 1e30f` at Loader.cpp:299), so it may run for tens of milliseconds when the queue is non-empty. |
| `LoadMgr::PollUntilEmpty` | `utl/Loader.cpp:212-222` | Sets `mPeriod = 1e30f` then calls `Poll()`. The matched-fork already disables CheckSplit here; the HX_NATIVE drain block is a no-op duplicate. |
| `LoadMgr::PollUntilLoaded(ldr1, ldr2)` | `utl/Loader.cpp:129-210` | Synchronous wait for a specific loader. 20+ call sites — see grep below. Drains via repeated `PollFrontLoader` calls until `ldr1->IsLoaded()`. |
| `Loader::ForceGetLoader` | `utl/Loader.cpp:76-92` | Adds a loader and synchronously waits for it. |

All `PollUntilLoaded` / `ForceGetLoader` call sites are critical to preserve:
they ARE the synchronous "give me this asset NOW" API. From the grep
(`grep -rn TheLoadMgr.PollUntilLoaded src/`):

- `App.cpp:427` — `PollUntilEmpty` at App ctor end.
- `system/ui/UI.cpp:259` — auto-load milo on UI init.
- `system/ui/UIPanel.cpp:295` — `OnLoad` action with `wait` flag.
- `system/bandobj/PatchDir.cpp:836` — sticker patch dir load.
- `system/bandobj/BandDirector.cpp:56` — director sub-load.
- `system/beatmatch/BeatMaster.cpp:61` — beat data load.
- `system/synth/MoggClip.cpp:200` — audio asset load.
- `system/synth/BinkClip.cpp:212` — video/audio container.
- `system/rndobj/Tex.cpp:175` — texture load.
- `system/obj/Dir.cpp:171, 503` + `system/obj/Dir.h:125` — generic ObjectDir proxy load.
- `system/obj/DirLoader.cpp:141` — `LoadObjects` synchronous helper.
- `system/obj/DataFile.cpp:768` — DTA include load.
- `system/utl/Loader.cpp:88` — `ForceGetLoader`.
- `band3/bandtrack/TrackPanel.cpp:276` — track scrubber.
- `band3/meta_band/CharData.cpp:81` — `TheLoadMgr.Poll()` (not PollUntil; equivalent to dump-the-queue).
- `system/char/FileMerger.cpp:124` — `TheLoadMgr.Poll()`.
- `system/utl/NetLoader.cpp:94` — net asset.
- `system/world/LightHue.cpp:56` — lighting preset.

These callers are the constraint: the new async path MUST preserve the
"synchronous from the caller's POV" contract — a `PollUntilLoaded(L)` call
must return with `L->IsLoaded()` true. The plan in §4 keeps the API identical
and only changes WHERE the work actually executes.

### 1.4 The frame-budget collision during `game_screen` load

`UI.cpp:582-587` flips `TheLoadMgr.SetLoaderPeriod`:

```cpp
if (!mTransitionScreen->CheckIsLoaded()
    && (!mCurrentScreen || !mCurrentScreen->Exiting())) {
    TheLoadMgr.SetLoaderPeriod(26.67f); // 60Hz-ish during transition
} else { ...
    TheLoadMgr.SetLoaderPeriod(10.0f);   // 30Hz-ish steady state
}
```

But the HX_NATIVE drain at `Loader.cpp:299` IGNORES `mPeriod` — it sets
`unk1c = 1e30f` (which is what CheckSplit gates on) so `LoadMgr::Poll`
drains the queue to completion in one main-thread call. That's the
"competition" the tv3 force-poll comment refers to: `TheLoadMgr.Poll` is
called inside `SystemPoll` inside `RunOneFrame`, and on a frame where the
queue is non-empty (which is constant during `game_screen` background load)
it can run for >>16ms, so `TheRnd->BeginDrawing` either skips that frame's
`ProcCounter::ProcCommands` advance or `mProcCmds` falls behind the
"nominally 1/3 frames see kProcessPost" cycle.

**The diagnostic is mechanical**: instrument `SystemPoll` to log
`TheLoadMgr.Poll` wall-clock per frame and `(mProcCmds & kProcessPost)`
counter advance per frame across a hold range. The expected signature is
`Poll`-time spikes correlated 1:1 with frames where `kProcessPost` is 0.
(See §6 Phase 0.)

### 1.5 Other Poll sites that compete

During the tv3 hold, the main thread also runs:

- `TheTaskMgr.Poll` (`App.cpp:478`) — script tasks, including the
  `vignette_transition`'s own task graph if/when it's installed.
- `TheUI.Poll` (`App.cpp:476`) — drives UIPanel transitions, `OnLoad`
  callbacks, etc. Each `OnLoad(true)` is a re-entrant
  `PollUntilLoaded`.
- `TheSynth->Poll` (`App.cpp:480`) — audio non-RT bookkeeping
  (BinkClip->SynthPoll, MoggClip state advance). The audio bytes
  themselves are mixed on the miniaudio RT thread (`MaDataCallback`),
  not here.
- `RB3GameInputPoll` (`App.cpp:477`) — input event queue drain; cheap.
- `RB3HttpServerPoll` (`App.cpp:663`) — debug server; cheap.

So the contention picture is dominated by `LoadMgr::Poll`. The 5+
ms-per-frame loss to LoadMgr drowns out everything else on the main thread
when a `game_screen` load is active.

---

## 2. Retail Wii loader architecture (is it truly async?)

**Yes, retail Wii is truly async.** The evidence:

### 2.1 AsyncFileCNT spawns a real thread per read

`os/AsyncFileCNT.cpp:92-110` (decompiled, matched-fork):

```cpp
void AsyncFileCNT::_ReadAsync(void *buffer, int len) {
    if (mOpenHandles > -1) {
        MILO_ASSERT(!mReadInProgress, 146);
        mReadInProgress = true;
        mReadResultCNT = -1;
        mTempBufferCNT = buffer;
        mReadSizeCNT = (len + 0x1F & 0xFFFFFFE0);
        OSCreateThread(
            &mCNTReadThread,
            ReadAsyncCNT,           // calls CNTRead synchronously
            this,
            mCNTThreadReadStack + sizeof(mCNTThreadReadStack),
            sizeof(mCNTThreadReadStack),
            15, 1
        );
        mCNTReadThread.specific[0] = (void *)"CNTReadThread";
        OSResumeThread(&mCNTReadThread);
    }
}
```

This is exactly the model the native plan adopts: spawn a worker that does
the blocking I/O, and let `_ReadDone()` be a non-blocking poll for the
worker's completion flag (`mReadResultCNT != -1`).

### 2.2 AsyncFileWii uses Wii DVD-hardware async completion

The `AsyncFileWii::_ReadAsync` / `_ReadDone` virtuals are declared in
`os/AsyncFile.cpp:14-31` but implemented in a TU outside the matched-fork
build (Wii DVD-hardware code; not relevant here). The DVD API (`DVDReadAsync`
+ callback) is hardware-DMA — Wii's DVD controller does the read while CPU
runs.

### 2.3 ChunkStream's TempEof yield IS the cooperation point

The matched-fork code is full of patterns like (`utl/Loader.cpp:504-510`,
`obj/DirLoader.cpp:457-461`):

```cpp
while (t = mStream->Eof(), t != NotEof) {
    MILO_ASSERT(t == TempEof, 0x2A8);
    if (TheLoadMgr.CheckSplit())
        return;
}
```

`TempEof` means "chunk not yet ready; come back next Poll". On Wii, `ReadAsync`
returns immediately and `ReadDone` polls a hardware completion flag — so
this loop NATURALLY yields the main thread back to the frame loop when
bytes haven't arrived yet. That's the retail design.

What broke on native: `native_file.cpp:69-72` made `ReadAsync` itself
blocking:

```cpp
bool ReadAsync(void *buf, int n) override {
    mLastReadBytes = Read(buf, n);
    return mLastReadBytes == n;
}
```

So the matched-fork `TempEof` loop never trips — `ReadAsync` already
finished — and the loader's per-state machinery runs straight through
without ever yielding, on the main thread, blocking the frame.

### 2.4 The Audio thread is already separate on Wii

The Wii audio backend (DSP-side) is already on its own thread; the synth
mixer talks to it via the same kind of producer/consumer ring buffer that
miniaudio uses on native. So **retail Wii already runs three threads:**
main (game + render), audio (DSP), and per-async-read I/O threads. The
"two-thread native model" is actually the simplified version.

### 2.5 Why we believed otherwise

The HX_NATIVE state-step blocks in `FileLoader::PollLoading` and
`DirLoader::PollLoading` say:

> "Native = synchronous loading (mirrors DC3's ...). On a fast host the
> LoadMgr 10ms `mPeriod` budget is exhausted by earlier loaders ..."

These comments are correct as a description of the SYMPTOM (a fast host's
time-slice machinery is too coarse) but wrong as a description of retail.
Retail isn't unsliced-and-fast; it's TRULY ASYNC, and the time-slice
machinery on Wii exists for the CPU-bound parts (object construction,
PreLoad/PostLoad) not the I/O parts. The DC3 native port (which is what
the comment cites) made the same conflation. Cf. `dc3-decomp/src/system/utl/Loader.cpp:293-298`:

```cpp
#ifdef HX_NATIVE
void LoadMgr::PollFrontLoader() {
    if (!mLoading.empty()) {
        mLoading.front()->PollLoading();
    }
}
```

DC3 native cuts away the time-slice + glitch-report machinery and runs
each PollLoading to completion. It works for DC3 because DC3 doesn't have
a `kProcessPost`-driven data sequencer in the critical path. RB3 does.

---

## 3. What competes during the tv3 hold

This is the symptom map; numbers from `RB3_TV3SEQ_DBG` instrumentation
already in tree.

### 3.1 Concurrent loads during `game_screen` background load

When the user picks a song and the UI transitions to `game_screen`, the
tv3 transition vignette runs in the foreground (held panel) while the
following queue up on `TheLoadMgr`:

- The `game_screen` PanelDir itself (`ui/game/game_screen.milo_xbox`).
- The chosen song's `.mid` (`MidiInstrument::Load` path).
- The chosen song's `.mogg` (`MoggClip::EnsureLoaded` →
  `MoggClip.cpp:200` `PollUntilLoaded`).
- The venue milo for the song (`small_club_01` typically; pulled via
  `EnterVenue → mCurWorld` change).
- The band character + outfit milos (`band/char/*.milo_xbox`).
- Per-instrument HUD widget milos.
- All deferred-binding texture files referenced by the above.

This is a tens-of-megabytes payload in dozens of files. On native that's
many seconds of `fopen + fread` on the main thread.

### 3.2 The race we observe

`Rnd::BeginDrawing` (`rndobj/Rnd.cpp:599-601`) is the ONLY site that
advances `mProcCounter`. So `mProcCmds & kProcessPost` only becomes true
when:

1. `App::RunOneFrame` reaches `TheRnd->BeginDrawing()` (`App.cpp:489`).
2. Which only happens after `SystemPoll → TheLoadMgr.Poll` has drained
   the queue (or aborted on time-slice — and we disabled time-slice).
3. AND the `ProcCounter` cycle (`PostProc.cpp:684-722`) lands on the
   `count=2` rung (`retCmd = 2 = kProcessPost`).

When `TheLoadMgr.Poll` consumes the whole frame, step 1 is never reached,
so `mProcCmds` doesn't advance, so the `WorldDir::Poll` `kProcessPost`
gate at `world/Dir.cpp:144` stays false. The
`vignette_transition`'s `select_camera` handler (which advances
`trans_index` to step through the sub-shots) never fires. Hence the
force-poll hack at `Dir.cpp:147-164`.

### 3.3 Audio is NOT the problem

`AudioDevice::MixSources` (`audio/AudioDevice.cpp:335`) runs on the
miniaudio RT thread, which is independent. Audio buffers don't draw on
LoadMgr; they're populated by `RenderAudio` from already-loaded
`MoggClip` / `BinkClip` buffers. The ONE place audio could compete is if
a streaming clip tries to `PollUntilLoaded` a missing chunk — but
streaming clips eagerly load the whole asset via
`MoggClip::EnsureLoaded` BEFORE play start. So during the tv3 hold the
audio thread is mostly idle (or only mixing the already-loaded preview
clip), not interleaved with LoadMgr work.

### 3.4 Quantitative budget (estimated, to be confirmed in Phase 0)

For a typical 60Hz native frame budget of 16.7ms, the breakdown during
tv3 hold today is roughly:

- `SystemPoll` non-LoadMgr: ~0.5ms
- `TheUI.Poll`: ~1ms
- `TheTaskMgr.Poll`: ~0.5ms
- `TheSynth->Poll`: ~0.5ms
- `TheRnd->BeginDrawing`: ~0.5ms
- `TheUI.Draw`: ~5ms (background world + held vignette)
- `TheRnd->EndDrawing`: ~1ms (present)
- **`TheLoadMgr.Poll`: 30-200ms when active** (bursty; depends on which
  asset is in front)

So we're frame-pacing at 5-20 effective FPS during loads. Phase 0 must
confirm this with real numbers.

---

## 4. Proposed off-thread design

### 4.1 Thread model

```
┌──────────────────────────────────────────────────────────────────────┐
│ Main thread (existing)                                               │
│   App::RunOneFrame                                                    │
│   ├─ SystemPoll                                                       │
│   │   └─ TheLoadMgr.Poll  ←──── now SHORT: drives state machines,    │
│   │                              only blocks on locks (microseconds) │
│   ├─ TheUI.Poll                                                       │
│   ├─ TheTaskMgr.Poll                                                  │
│   ├─ TheSynth.Poll                                                    │
│   └─ TheRnd→Begin/Draw/End                                            │
└──────────────────────────────────────────────────────────────────────┘
                          │       ▲
                          │ submit│ completion (poll on main thread)
                          ▼       │
┌──────────────────────────────────────────────────────────────────────┐
│ Loader thread (NEW, single — name pthread "rb3-loader")               │
│   while (!terminating)                                                │
│     wait_on(work_cv)                                                  │
│     for each AsyncReadRequest in queue:                               │
│       fopen / fread / fclose (BLOCKING is fine here)                 │
│       memcpy into caller's buffer                                     │
│       atomic_store(req.complete, true)                                │
│       signal completion_cv                                            │
└──────────────────────────────────────────────────────────────────────┘
                          ▲
                          │ work_cv signal
                          │
┌──────────────────────────────────────────────────────────────────────┐
│ miniaudio RT thread (existing, untouched)                             │
│   MaDataCallback                                                      │
│   └─ AudioDevice::MixSources                                          │
└──────────────────────────────────────────────────────────────────────┘
```

**Why one loader thread and not a pool**: each `Loader` is logically a
linear pipeline of File reads (one chunk at a time via `ChunkStream`), and
the load queue is itself serial (`mLoading` is a list with a single
`front()`). Two concurrent file readers competing on the same physical
disk + filesystem cache buy nothing on a HDD/SSD; on an NVMe with multiple
queues it would, but RB3's asset payload is bursty enough that 1 reader
keeps the I/O pipe full. The simplicity (no read-reordering, no fairness
policy) is worth it. The thread pool variant is a future option (see §7).

### 4.2 Sync points and API surface

The public contract stays IDENTICAL. Every existing caller of
`PollUntilLoaded(L, _)` keeps working: when the caller returns,
`L->IsLoaded()` is true.

```cpp
// PUBLIC (unchanged):
class Loader {
    virtual bool IsLoaded() const = 0;
    virtual void PollLoading() = 0;
};

class LoadMgr {
    void Poll();                           // unchanged signature
    void PollUntilLoaded(Loader *, Loader *); // unchanged signature
    // ...
};

// PRIVATE (new, in os/AsyncLoaderThread_Native.cpp / .h — see §4.6):
struct AsyncReadRequest {
    File           *file;
    void           *buffer;
    int             length;
    int             offset;            // -1 = use file's current pos
    volatile bool   complete;          // set by loader thread
    int             result_bytes;      // set by loader thread
};

class AsyncLoaderThread {
public:
    static void  Init();                          // pthread_create + sem_init
    static void  Terminate();
    static void  Submit(AsyncReadRequest *req);   // queue + signal
    static bool  Poll(AsyncReadRequest *req);     // non-blocking; returns req->complete
private:
    static void *WorkerMain(void *);
    static pthread_t        sThread;
    static pthread_mutex_t  sQueueMutex;
    static pthread_cond_t   sQueueCv;
    static std::deque<AsyncReadRequest *> sQueue; // FIFO; main thread enqueues, worker drains
    static volatile bool    sTerminate;
};
```

The piece that changes is what `File::ReadAsync` actually does on native:

```cpp
// native_file.cpp (NEW HX_NATIVE branch, gated on RB3_LOADER_ASYNC):
bool NativeStdioFile::ReadAsync(void *buf, int n) override {
    if (g_loader_async_enabled) {
        // 1) park the request on this File; the loader thread fills it.
        mPending.file = this;
        mPending.buffer = buf;
        mPending.length = n;
        mPending.offset = std::ftell(mFp);
        mPending.complete = false;
        mPending.result_bytes = 0;
        AsyncLoaderThread::Submit(&mPending);
        return true;  // "submitted; check ReadDone"
    }
    // existing synchronous path:
    mLastReadBytes = Read(buf, n);
    return mLastReadBytes == n;
}

bool NativeStdioFile::ReadDone(int &result) override {
    if (g_loader_async_enabled && mPending.file) {
        if (!AsyncLoaderThread::Poll(&mPending))
            return false;                      // not ready -> TempEof
        result = mPending.result_bytes;
        mPending.file = nullptr;
        return true;
    }
    result = mLastReadBytes;
    mLastReadBytes = 0;
    return true;
}
```

That's the WHOLE behavior change at the byte-IO layer.

`ChunkStream::Eof` (`utl/ChunkStream.cpp:203-285`) and the matched-fork
`FileLoader::LoadStream` / `LoadFile` already handle `ReadDone` returning
`false` correctly (they return `TempEof` upward, and `PollLoading` then
returns from its state func). The state machinery is FINE for async; we
just unbroke `ReadAsync`.

### 4.3 What HAPPENS in `TheLoadMgr.Poll` after the change

The main thread's `Poll` becomes essentially a state-machine pump:

```
LoadMgr::Poll (new HX_NATIVE arm, gated on RB3_LOADER_ASYNC=1):
  while (!mLoading.empty()):
    front = mLoading.front()
    front->PollLoading()
      // advance state machine:
      //   - if data not ready (ReadDone=false), state func reads
      //     TempEof from ChunkStream/BinStream and returns
      //   - if data ready, state func consumes it and falls through
      // CPU-bound work (NewObject, PreLoad, PostLoad) DOES run here.
    if (front->IsLoaded()) mLoading.pop_front()
    if (!progress_this_iter) break    // (NEW) no more main-thread work; bytes pending
```

Key property: the new HX_NATIVE arm returns from `Poll` after one
no-progress pass. The bytes that hadn't arrived will trigger a
re-poll next frame, when `SystemPoll` runs again. The MAIN thread
spends bounded time on `Poll` (CPU work for the chunks already read)
and the I/O bytes overlap with the next frame's `TheUI.Draw`.

This matches retail Wii's behavior. The "drain to completion" HX_NATIVE
block at `Loader.cpp:289-307` is replaced by this "drain CPU work,
yield to caller when blocked on I/O" arm.

### 4.4 What happens in `PollUntilLoaded`

The "wait for THIS loader" path:

```
LoadMgr::PollUntilLoaded(ldr1, ldr2):
  while (!ldr1->IsLoaded()):
    if (made_no_main_thread_progress):
      AsyncLoaderThread::WaitForAny()   // park main thread on completion_cv
                                          // (microseconds when I/O is fast,
                                          //  bounded by SLA below)
    PollFrontLoader()
    // ... same loop body as today ...
```

When the main thread is "waiting" inside a `PollUntilLoaded`, it can:

(a) Spin on `AsyncLoaderThread::Poll` — wasteful but simple, and OK if
the I/O is fast.
(b) Park on a condition variable signalled when ANY pending read
completes. Cleaner.

For the V1 implementation we pick (a), with a 1ms backoff sleep if no
progress for N iters. (b) is a refinement.

**Note on TheUI.Poll re-entrancy**: a `UIPanel::OnLoad(true)` call from
inside `TheUI.Poll` re-enters `PollUntilLoaded` — but `TheUI.Poll`
runs on the MAIN thread, between two `RunOneFrame` iterations. So this
re-entry happens while the loader thread is concurrently making
progress, fine. No deadlock.

### 4.5 Completion handling: the `Loader::Callback` path

`Loader::Callback::FinishLoading` / `FailedLoading` are called from
inside `DirLoader::Cleanup` / `LoadObjs`, on the main thread, after the
state machine reaches `DoneLoading`. This is post-CPU-work, so
nothing changes — the callback still fires on the main thread, the
caller still owns the heap context, no thread-context surprises.

### 4.6 Engine-side vs rb3-side changes

| Layer | Change |
|---|---|
| `milo-native-engine/src/platform/AsyncLoaderThread_Native.{cpp,h}` | NEW. The thread implementation. Pure POSIX (pthread + cond var). |
| `milo-native-engine/src/platform/ThreadCall_Native.cpp` | Reuse the worker-init pattern. New file is the cleaner option; we don't want to bolt I/O queue management onto the generic ThreadCall ring. |
| `rb3/native/src/native_file.cpp` | MODIFY `ReadAsync` and `ReadDone` (the new branch, gated on `g_loader_async_enabled`). Keep the synchronous path under `#else`. |
| `rb3/src/system/utl/Loader.cpp` | MODIFY HX_NATIVE arms in `Poll`, `PollUntilLoaded`, `FileLoader::PollLoading`. New arms call out to the same matched-fork state funcs but DO honor `TempEof`. Old HX_NATIVE drain blocks become the `RB3_LOADER_ASYNC=0` fallback. |
| `rb3/src/system/obj/DirLoader.cpp` | MODIFY HX_NATIVE arm in `PollLoading`. Same shape: honor `TempEof` from the state func. |
| `rb3/src/system/world/Dir.cpp` | REMOVE the force-poll HX_NATIVE block at lines 147-164 (last phase). |
| `rb3/src/App.cpp` | No change. The `SystemPoll → TheLoadMgr.Poll` site is preserved. |
| `rb3/src/system/os/System.cpp` | MODIFY `SystemPreInit` (HX_NATIVE branch) to call `AsyncLoaderThread::Init` after `ThreadCallInit`. Modify `SystemTerminate` to call `AsyncLoaderThread::Terminate` BEFORE `ThreadCallTerminate`. |

The "engine-side" piece is small (~150 LOC of new pthread code in the
engine). The "rb3-side" piece is the HX_NATIVE-additive surgery on the
existing state machines.

### 4.7 HX_NATIVE additivity and asm-match

Every change is inside an existing `#ifdef HX_NATIVE` block or an
`#ifdef HX_NATIVE ... #else ... #endif` arm. The Wii MWCC build path
(no `HX_NATIVE` defined) is bit-identical before and after this work.
No matched-fork code is touched outside HX_NATIVE.

To be specific:

- `Loader.cpp` ALREADY has HX_NATIVE arms at lines 289, 487 — those
  arms are rewritten, not removed. New code goes inside the same
  preprocessor brackets.
- `DirLoader.cpp:243` HX_NATIVE arm is rewritten in place.
- `native_file.cpp` has NO matched-fork analog (it's a native-only
  shim), so changes there don't touch asm-match.
- `world/Dir.cpp:147-164` is HX_NATIVE; removed entirely in Phase 4.
  The Wii path stays.

The plan does NOT introduce a new `#ifdef HX_NATIVE` block anywhere
that didn't already have one, except in `os/System.cpp` for the
AsyncLoaderThread init/terminate calls.

---

## 5. Compatibility risks

### 5.1 "After PollLoadMgr returns, the load is done" assumptions

I grepped for callers that act on this assumption today. The candidates:

- **`PollUntilEmpty`** (Loader.cpp:212) — explicit; named for the
  behavior. The new path still drains until empty; it just doesn't
  hold the main thread captive while bytes are in flight. CALLERS:
  `App.cpp:427` (App ctor) and `PollUntilEmpty` itself. App-ctor case is
  OK because the App ctor is followed by `Run()` which spins
  `RunOneFrame` — bytes will arrive while the loop runs.

  **RISK**: anything between App ctor end and the first `RunOneFrame`
  that ASSUMES the assets are loaded. Audit needed: `App.cpp` between
  line 427 (`PollUntilEmpty`) and line 622 (`RunWithoutDebugging` body
  start). The HX_NATIVE arm starting at line 621 runs Frame 0 immediately,
  so this window is small. CONCRETELY check: does any of the boot
  spine between SystemInit and RunWithoutDebugging deref an asset
  loaded by `PollUntilEmpty`? Likely yes (config UI). The mitigation
  is to make `PollUntilEmpty` actually SPIN on `WaitForAny` if the queue
  isn't empty after a CPU-only pass — preserving the legacy semantics
  for THIS specific call without forcing every `Poll()` to spin.

- **`Loader::ForceGetLoader`** (Loader.cpp:76) — explicitly calls
  `PollUntilLoaded`; the new path waits, so this is preserved.

- **`MoggClip::EnsureLoaded`**, **`BinkClip::EnsureLoaded`**,
  **`RndTex::SetBitmap(FileLoader*)`**, etc. — all use
  `PollUntilLoaded`; preserved.

### 5.2 `IsLoaded()` flipping mid-frame

`Loader::IsLoaded` is read on the main thread. The new loader thread
NEVER mutates the `mState` pointer (that lives in DirLoader/FileLoader
which the loader thread doesn't touch). Only `Poll`-driven state-func
calls advance `mState`. Race-free by construction.

The fields the loader thread DOES touch are `AsyncReadRequest::buffer`
contents, `result_bytes`, and the `complete` flag. The complete flag
is written last and is read with `volatile bool` + a memory barrier on
read (or `std::atomic<bool>` if usable; see Skeleton_Native.h:9
note about volatile-vs-atomic in this toolchain — copy that idiom).

### 5.3 Race: main thread reads an asset while loader thread is writing it

The loader thread writes into the buffer the caller passed to
`File::ReadAsync` (the same buffer the caller will then `Read` from
via `ChunkStream::ReadImpl` etc). The matched-fork code's contract is
"don't read from the buffer until `ReadDone` returns true". The
`ChunkStream` honors this — it polls `ReadDone` before swapping
`mCurBufferIdx` to the new buffer. So as long as the new
`NativeStdioFile::ReadDone` correctly returns false until the loader
thread sets `complete=true`, there is NO concurrent buffer access.

The "I'm writing the buffer" → "main thread is allowed to read it"
handoff is a single byte (the `complete` flag). One release-store on
the loader thread, one acquire-load on the main thread. This is the
canonical async-IO completion pattern.

### 5.4 Audio thread interaction

The miniaudio RT thread reads from `MoggClip`'s internal buffer that
was loaded SYNCHRONOUSLY via `EnsureLoaded`'s `PollUntilLoaded` BEFORE
play started. So once a clip is playing, the audio thread doesn't
touch the loader at all. No new sync needed there.

The ONE corner case is `MoggClip::EnsureLoaded` running on the audio
thread (it shouldn't — but verify with `grep -n EnsureLoaded
src/system/synth/MoggClip.cpp` and check the call sites; if any are
from a Synth::Poll path that's only called from main, we're fine).
Reviewed call sites: all are main-thread.

### 5.5 Deadlock potential

Possible deadlock scenarios:

1. **Nested load: a `PreLoad/PostLoad` for object X triggers a
   sub-load Y, and the sub-load's `PollUntilLoaded(Y)` is called from
   inside object X's `PostLoad` from inside `LoadObjs` from inside
   `PollLoading` from inside `Poll`.** This is the
   `DirLoader::LoadResources → TheLoadMgr.AddLoader` pattern
   (`obj/DirLoader.cpp:584`). It works today (the front-loader changes
   under us; the matched fork's `GetFirstLoading() != this` re-entrancy
   guards handle it). The new async path doesn't make it any worse —
   the new `PollUntilLoaded(Y)` is itself just a "spin until done"
   that drives the SAME `Poll` loop, on the SAME main thread.
   **Verdict**: no new deadlock, but the spin can take longer because
   bytes for Y are now genuinely async. Mitigation: cap the spin with
   `WaitForAny` cv.

2. **Callback during Poll calls into LoadMgr.** `FinishLoading` /
   `FailedLoading` fire from inside `PollFrontLoader`. If the callback
   calls back into `AddLoader` or `PollUntilLoaded`, we recurse on the
   main thread. Same as today; no new deadlock.

3. **Message handlers that load.** Some
   `HandleType(some_msg)` script paths invoke `OnLoad` on a UIPanel which
   triggers `PollUntilLoaded`. If a HandleType is called from
   `TheUI.Poll`, OK (main thread, top of stack). If from
   `WorldDir::Poll`'s `select_camera_msg` path, OK (main thread, but
   nested below `Poll`). Same risk profile as today.

4. **Loader thread blocked on I/O while main thread holds the
   queue mutex.** The proposed code holds the queue mutex only for
   the enqueue/dequeue, NOT during the fread. So this can't happen.

5. **Termination order.** `AsyncLoaderThread::Terminate` MUST be
   called before `ThreadCallTerminate`-like teardown sequences, and
   crucially BEFORE the `NativeStdioFile` instances are destroyed.
   Today `App::~App` calls `TheDebug.Exit(0, true)` which drains exit
   callbacks. The `RB3AudioTerminateExitCallback` pattern in
   `main_native.cpp:543` is the template — register a similar
   `RB3LoaderAsyncTerminateExitCallback` that joins the loader thread.

### 5.6 Worst-case "asset not ready"

Today's HX_NATIVE drain guarantees the asset IS ready when the caller's
synchronous use site fires. After this change, a `PollUntilLoaded`
call might genuinely wait, but it WILL return only when loaded. So
synchronous callers see the same behavior; ONLY the global
`TheLoadMgr.Poll()` from `SystemPoll` changes semantics (it no
longer drains — it advances). Code paths that:

- Issued a `PollUntilLoaded(L)` and then dereffed L's data: unchanged.
- Issued `AddLoader(L)` then assumed L was ready next frame: BROKEN by
  the new path (asset may take multiple frames to land).

Grep for the latter pattern: `grep -rn AddLoader src/ | grep -v Poll`.
Most call sites are followed by a `PollUntilLoaded` or a deferred
binding (`UIPicture.cpp:121` is the prototype: `AddLoader` stores the
loader pointer, and `UIPicture` later calls `mLoader->IsLoaded()` in
its `Draw`/`Poll` path). Deferred bindings are FINE — they ARE the
async-loaded callers. The synchronous-assume callers all use the
`PollUntilLoaded` wrapper.

### 5.7 `MemPushHeap` / loader heap context

`PollFrontLoader` does `MemPushHeap(front->mHeap)` (`Loader.cpp:332`) and
`MemPopHeap` (`Loader.cpp:343`). This is main-thread heap context. The
loader thread does NOT touch the heap (it only does `fread` into a
buffer that the main thread allocated). So the heap-context push/pop
stays main-thread, behavior unchanged.

### 5.8 The `gDecompressionQueue` is already main-thread

`ChunkStream::DecompressChunkAsync` (`utl/ChunkStream.cpp:356-382`)
ENQUEUES decompression tasks into `gDecompressionQueue`, and
`PollDecompressionWorker` (`utl/ChunkStream.cpp:384-393`) DRAINS them
on the main thread. So "decompression" today is single-threaded.
Should we also move this off the main thread? **Not in V1**: it's CPU
work, not I/O work; the main-thread CPU budget is what `PollLoading`
is supposed to fit into via `CheckSplit`. Moving decompression
off-thread is a separate, larger refactor (and a real upside is
unclear because the chunks are tiny). Defer.

---

## 6. Migration plan

Each phase produces a green build and a passing test surface. The
plan is constructed so any phase can be rolled back at its commit
point with no engine-pin bump.

### Phase 0: Instrument the synchronous loader to confirm the model

**Goal**: prove with numbers that
(a) `TheLoadMgr.Poll` is dominating frame time during `game_screen`
    load,
(b) the I/O fraction (fopen + fread) is the majority of that, and
(c) the CPU fraction (NewObject + PreLoad + PostLoad) would fit in a
    single frame budget if the I/O went away.

**What to add**:

- Wall-clock timer around the entire `LoadMgr::Poll` body
  (`utl/Loader.cpp:299`), inside the HX_NATIVE arm.
- Counter for `(mProcCmds & kProcessPost) != 0` per frame
  (`App.cpp:489`, after `TheRnd->BeginDrawing()`).
- Per-loader-state timer breakdown via `MILO_LOG` inside
  `PollFrontLoader` HX_NATIVE arm: time spent in `OpenFile`,
  `LoadHeader`, `CreateObjects`, `LoadObjs`, `DoneLoading`.
- Per-File-Read counter and total wall-clock in `NativeStdioFile::Read`
  (`native/src/native_file.cpp:64`).
- Toggle via `RB3_LOADER_INSTRUMENT=1` so it's off by default.

**Files touched**:

- `rb3/src/system/utl/Loader.cpp` — HX_NATIVE-additive instrumentation.
- `rb3/src/system/obj/DirLoader.cpp` — likewise.
- `rb3/native/src/native_file.cpp` — counter + wall-clock.
- `rb3/native/src/main_native.cpp` (or new TU) — print summary on
  Debug::Exit.

**Expected diff size**: ~80 LOC of HX_NATIVE-additive instrumentation.

**Test plan**:

1. Run `RB3_GAME=1 MILO_MAX_FRAMES=3000 RB3_LOADER_INSTRUMENT=1 ...`
   through to song-select → song-pick → tv3 hold.
2. Confirm the `LoadMgr::Poll`-ms-per-frame histogram matches the
   §3.4 estimate (or correct the estimate).
3. Confirm `kProcessPost`-frames-per-second matches the
   ~70-130-out-of-2400 number reported in
   `world/Dir.cpp:147-156`.
4. Compute the I/O vs CPU split. If I/O is <50%, the off-thread plan
   gives less win than expected and we need to also tackle decompression
   off-thread (see §5.8).

**Rollback**: instrumentation is additive; remove the
HX_NATIVE-additive lines.

**Confidence**: HIGH. Pure measurement; nothing can break.

### Phase 1: Land the AsyncLoaderThread infrastructure (disabled)

**Goal**: get the loader thread compiling, running, and joinable
without yet routing any I/O through it.

**What to add**:

- `milo-native-engine/src/platform/AsyncLoaderThread_Native.{h,cpp}`
  containing the class in §4.2.
- A `RB3_LOADER_ASYNC` env var read once at `SystemInit` time, stored
  in a `bool g_loader_async_enabled`.
- A self-test on Init: submit a 1-byte read of a known file
  (`config/band_keep.dta`) through the loader thread and assert
  completion. If self-test fails, log and disable the path
  (fall back to synchronous).
- Engine pin bump on rb3 (to pull in the new engine-side files).

**Files touched (engine)**:

- NEW: `milo-native-engine/src/platform/AsyncLoaderThread_Native.{h,cpp}`
- `milo-native-engine/CMakeLists.txt` — register the new TU under the
  existing platform list.

**Files touched (rb3)**:

- `rb3/native/CMakeLists.txt` — bump `MILO_ENGINE_PIN` to new engine head.
- `rb3/src/system/os/System.cpp` — HX_NATIVE-additive
  `AsyncLoaderThread::Init` call after `ThreadCallInit` (line 717), and
  `AsyncLoaderThread::Terminate` in the exit-callback chain.

**Expected diff size**: ~200 LOC across engine + rb3.

**Test plan**:

1. `RB3_GAME=1` boots cleanly with `RB3_LOADER_ASYNC` UNSET — no
   behavior change.
2. `RB3_GAME=1 RB3_LOADER_ASYNC=1` boots cleanly — the loader thread
   starts, self-test passes, thread sits idle (no I/O routed through
   it yet).
3. `MILO_MAX_FRAMES=10 RB3_GAME=1 RB3_LOADER_ASYNC=1` exits cleanly
   (the join path works).

**Rollback**: revert the rb3 pin bump and the rb3 commit. The engine
files stay (they're harmless dead weight unless the rb3 side calls
them).

**Confidence**: HIGH. Thread-pool boilerplate, no game semantics yet.

### Phase 2: Route ONE isolated load through the async path

**Goal**: prove the round-trip works for a single, well-isolated
load — the tv3 transition vignette `game_screen` PanelDir load — and
that the rest of the game's loads are UNAFFECTED.

**What to add**:

- A path-match filter in `native_file.cpp`'s new `ReadAsync` arm:
  only divert to the async path if the file path matches a
  hardcoded substring (`game_screen` and friends). All other reads
  go through the existing synchronous code.
- This lets us verify the async path on a real load without
  destabilizing the whole boot.
- The HX_NATIVE arms in `LoadMgr::Poll`, `FileLoader::PollLoading`,
  `DirLoader::PollLoading` must be modified to honor `TempEof` —
  this is the §4.3 rewrite. The new arm is GATED on
  `g_loader_async_enabled`; the legacy drain arm is the `else`.

**Files touched**:

- `rb3/native/src/native_file.cpp` — branch in `ReadAsync` / `ReadDone`.
- `rb3/src/system/utl/Loader.cpp` — modified HX_NATIVE arms in `Poll`
  (line 289), `PollUntilLoaded` (no change, the spin still works),
  `FileLoader::PollLoading` (line 487).
- `rb3/src/system/obj/DirLoader.cpp` — modified HX_NATIVE arm in
  `PollLoading` (line 243).

**Expected diff size**: ~120 LOC HX_NATIVE-additive.

**Test plan**:

1. Regression sweep: full boot + song-load test
   (`MILO_MAX_FRAMES=9000 RB3_GAME=1`) with `RB3_LOADER_ASYNC=0` —
   confirm zero behavior change.
2. Same with `RB3_LOADER_ASYNC=1` — confirm the rest of the game's
   loads still go through the synchronous path (audit
   `LoadMgr::Poll`-ms histogram unchanged for non-game_screen loads).
3. Targeted test: enter song → confirm the tv3 transition still
   works (with the existing force-poll HX_NATIVE block STILL IN
   PLACE — we're testing async I/O, not removing the workaround yet).
   The Phase-0 `kProcessPost`-frames counter should go UP because
   `TheLoadMgr.Poll` no longer monopolizes the main thread during
   the game_screen load.
4. Visual regression: take screenshots through the song-select →
   tv3 hold → in-song sequence and compare to the pre-change golden.
   `MILO_SCREENSHOT_FRAMES` / `MILO_SCREENSHOT_DIR`.

**Rollback**: set `RB3_LOADER_ASYNC` unset by default; the new arm is
inert.

**Confidence**: MEDIUM-HIGH. The risk concentration here is the
`TempEof` path: any place in DirLoader/FileLoader where the matched-fork
state func didn't have a `CheckSplit() → return` guard, the new path
will infinite-loop the state func. Phase-0 instrumentation should
have surfaced any state func that's missing this. Mitigation: a
"max iterations per state pass" safety valve like the web port has
(`Loader.cpp:152` `maxIter = 200000`).

### Phase 3: Broaden to all "background while UI runs" loads

**Goal**: remove the path-match filter and route ALL `NativeStdioFile`
reads through the loader thread.

**What to add**:

- Drop the path-match filter in `native_file.cpp`.
- Audit `PollUntilLoaded` callers in §1.3 — verify that a "wait
  microseconds for I/O" is acceptable for each. Most are fine
  because they're called from main-thread `Poll` paths anyway.
- Special-case any `PollUntilLoaded` that runs deep in the boot
  spine before the loader thread is fully running (during
  `SystemPreInit` / `SystemInit`). The `band_preinit_keep.dta`
  load is the prototype: that's a `DataReadFile` synchronous path,
  not through `TheLoadMgr`, so it predates `AsyncLoaderThread::Init`
  and is naturally safe.

**Files touched**:

- `rb3/native/src/native_file.cpp` — drop the path filter.
- `rb3/src/system/os/System.cpp` — verify `AsyncLoaderThread::Init`
  is called AFTER `FileInit` but BEFORE the first `TheLoadMgr.Poll`
  trigger.

**Expected diff size**: ~30 LOC.

**Test plan**:

1. Full boot regression including `RB3_BOOT=1` (the headless DTA
   boot) and `RB3_GAME=1` end-to-end.
2. Web build (HX_WEB) regression — the engine-side files are guarded
   by `#ifndef __EMSCRIPTEN__` for pthread paths, so the web build
   should compile and run unchanged.
3. Run the full multi-song regression sweep (the
   `subagent-worktree-workflow` test set).

**Rollback**: re-add the path filter from Phase 2.

**Confidence**: MEDIUM. The audit is the risk — undetected
"AddLoader-then-deref-next-frame" callers will surface as crashes /
visual glitches. Mitigation: stage Phase 3 behind a screenshot
regression sweep.

### Phase 4: Remove the WorldDir::Poll force-poll HX_NATIVE block

**Goal**: revert `world/Dir.cpp:147-164` (and the `IsTv3Dir` / `Tv3SeqDbg`
helpers if unused). Confirm tv3 still works.

**Files touched**:

- `rb3/src/system/world/Dir.cpp` — delete the HX_NATIVE block at
  lines 147-164. Keep the `Tv3SeqDbg` instrumentation (it's debug-only,
  cheap, and useful for future regressions).

**Expected diff size**: ~20 LOC removed.

**Test plan**:

1. Full game-loop regression with `RB3_LOADER_ASYNC=1` and the new
   default (loader-async-on). Confirm the tv3 hold completes within
   the song's authored timing budget — measured in `Tv3SeqDbg`'s
   `trans_index` advancement rate.
2. Confirm `RB3_TV3_PLAY_OFF` env var no longer changes behavior
   (it's dead code now).
3. Screenshot regression.

**Rollback**: re-add the force-poll block (it's a 6-line change).

**Confidence**: MEDIUM-HIGH. The actual REMOVAL is one block; the
risk is that Phase 3 didn't recover the `kProcessPost` rate enough.
Phase 0 metrics will tell us in advance.

### Phase 5: Tidy up

- Make `RB3_LOADER_ASYNC=1` the default. The env var becomes the
  bail-out, not the opt-in (rename to `RB3_LOADER_SYNC=1`).
- Reduce the HX_NATIVE branches in Loader.cpp / DirLoader.cpp to a
  single canonical async-on path.
- Update the inline comments at `Loader.cpp:289-307` and
  `DirLoader.cpp:243-259` to reflect the new architecture.
- Update `NATIVE_PORT_INVENTORY.md` to mark the loader as
  retail-parity.
- Mark `RB3_TV3_PLAY_OFF` as removed in the env-var matrix in
  `main_native.cpp`.

---

## 7. Scope and effort estimate

| Phase | Days (P50) | Days (P90) | Confidence |
|---|---|---|---|
| 0: instrumentation | 0.5 | 1 | high |
| 1: thread infrastructure (disabled) | 1 | 2 | high |
| 2: single-load routing | 2 | 4 | medium-high |
| 3: broaden to all loads | 1.5 | 3 | medium |
| 4: remove force-poll | 0.5 | 1 | medium-high |
| 5: tidy | 0.5 | 1 | high |
| Buffer (regression hunt) | 2 | 4 | n/a |
| **Total** | **8 days** | **16 days** | **medium** |

**Unknowns that would change the estimate**:

- Whether Phase 3 surfaces broken `AddLoader`-and-defer callers we
  didn't anticipate. Adding a screenshot regression harness adds
  +2 days. Recommend including this in the buffer.
- Whether the decompression queue (`gDecompressionQueue`) needs to be
  moved off-thread too. If Phase 0 measurements show CPU
  decompression dominating the LoadMgr time, add Phase 3.5 (move
  decompression to a dedicated worker) at +3-5 days.
- Whether any third-party library (e.g., the Bink / FFmpeg movie
  reader) opens its own file handles outside `NativeStdioFile`. If
  so, those won't go through the new path automatically. Audit in
  Phase 0.

**Effort risks that could blow up the estimate**:

- Heisenbug-class races between the loader thread and any unaudited
  mutator of the read buffer. Unit tests with TSan should be added
  to Phase 1.
- A `PollUntilLoaded` deep in `PreLoad` that turns out to depend on
  full-drain semantics. The mitigation (capped spin) keeps it
  working but slow; finding it adds time.

---

## 8. Open questions / pre-implementation probes

### 8.1 Is retail Wii ACTUALLY async, or just fast?

**Answered above (§2): TRULY ASYNC.** AsyncFileCNT spawns a per-read
thread (CNT card reads); AsyncFileWii uses Wii's DVD hardware
async-completion API. The matched-fork code is structured around the
TempEof/ReadDone polling cooperation, which only makes sense for genuine
async I/O. The DC3-native comment that conflates "async" with "fast
enough" is wrong, and Phase 0 measurements should make this concrete.

### 8.2 What is the decompression CPU cost vs I/O cost?

**Probe**: Phase 0 instrumentation should split these per-chunk. The
ChunkStream `DecompressChunkAsync` enqueues into `gDecompressionQueue`,
and `PollDecompressionWorker` drains it (all main-thread). If
decompression dominates I/O for a typical milo, we need Phase 3.5.

### 8.3 Are there any `AsyncFile_Native.cpp`-style overrides I missed?

**Probe**: `find . -name 'AsyncFile_Native.cpp' -o -name 'File_Native.cpp'`
in the engine. (DC3 has `AsyncFileNative.cpp`; rb3 does NOT yet —
the loader uses `NativeStdioFile` directly from `native_file.cpp`.)
Confirm Pre-Phase-1 that no engine-side override is silently
intercepting `NewFile` for rb3.

### 8.4 What is the actual ChunkStream chunk size in practice?

`ChunkStream::ReadChunkAsync` (`utl/ChunkStream.cpp:179`) reads
`mChunkInfo.mChunks[i] & kChunkSizeMask` bytes per chunk. Typical
ChunkStream chunk size is 0x10000 (64KiB, set in `ChunkStream`
constructor's `mRecommendedChunkSize` arg). So each PreLoad pumps
`TempEof` ~ `file_size / 64KiB` times. For a typical 2MiB milo that's
~32 round-trips. The off-thread cost is bounded; the single-loader-thread
model handles this fine. Probe via Phase 0 logging.

### 8.5 Does `Game::LoadSong`'s `MoggClip::EnsureLoaded` call cause
priority inversion?

`MoggClip::EnsureLoaded` (`MoggClip.cpp:197-204`) creates a `FileLoader`
at `kLoadFront` and calls `PollUntilLoaded`. If this happens DURING a
game_screen load, the new `FileLoader` jumps the queue (kLoadFront), so
it's processed next. With async I/O the main thread spins on
`PollUntilLoaded` until the bytes arrive — same wall-clock behavior as
today, just yielding cycles to background loads in the queue. No
inversion risk.

### 8.6 Does anyone call `AddLoader` then `mLoading.front()` directly?

`grep -rn 'mLoading' src/ | grep -v 'Loader.cpp'`:

```
band3/meta_band/CharData.cpp: ... mLoading reference?
system/obj/DirLoader.cpp:81: TheLoadMgr.mLoaders (not mLoading)
```

No direct external `mLoading` deref. Safe.

### 8.7 Will Phase 0 numbers actually validate the model?

If Phase 0 shows that `LoadMgr::Poll` is mostly CPU (not I/O), the
entire premise of this plan is wrong and we should instead move
decompression / `LoadObjs` work off-thread. Phase 0 is the gate. The
expected outcome (based on the existing data: a 2-second blocking
file open on a cold host can be observed today) is that I/O dominates
when the host disk cache is cold and CPU dominates when it's warm.
Either way Phase 0 unblocks the next decision.

### 8.8 ThreadCall already exists — can we just reuse it?

`ThreadCall_Native.cpp` already has a single worker + a ring buffer
of 12 jobs. We COULD bolt I/O onto the existing ring. Pros: less new
code. Cons: ThreadCall is a "fire-and-forget with callback" idiom;
loader I/O needs a poll-for-completion idiom (no callback fires from
the loader thread — the main thread polls `ReadDone`). Mixing the
two would conflate two distinct patterns and complicate the ring.
**Recommendation**: NEW class. Reuse the pthread+semaphore IDIOM
from ThreadCall_Native.cpp, but a dedicated class. See §4.2.

### 8.9 Is the `gPollFrontLoaderTimer` `AutoTimer` going to misreport?

`utl/Loader.cpp:322` `const AutoTimer at(&gPollFrontLoaderTimer,
50.0f, ...)` will report shorter times for `PollFrontLoader` after
the change (because the I/O happens off-thread). Cosmetic, but
expected. Update or remove the 50.0f glitch threshold.

### 8.10 Does the LP64 path have additional ABI traps?

The shared loader buffer / `mPending` request struct is read by two
threads. `volatile bool` for the completion flag is per
`Skeleton_Native.h:9`'s note about `<atomic>` interaction with
GCC 15 headers under clang. Use the same idiom; verify with TSan.

---

## 9. References

- `src/system/world/Dir.cpp:147-164` — the force-poll HX_NATIVE block
  to be removed.
- `src/system/utl/Loader.cpp:289-307` — the HX_NATIVE drain in
  `LoadMgr::Poll`.
- `src/system/utl/Loader.cpp:487-502` — the HX_NATIVE state-step block
  in `FileLoader::PollLoading`.
- `src/system/obj/DirLoader.cpp:243-259` — the HX_NATIVE state-step
  block in `DirLoader::PollLoading`.
- `src/system/rndobj/Rnd.cpp:599-601` — where `mProcCmds` advances.
- `src/system/rndobj/PostProc.cpp:684-722` — `ProcCounter::ProcCommands`
  cycle.
- `src/system/os/AsyncFileCNT.cpp:92-110` — retail Wii's per-read
  thread template.
- `src/system/os/AsyncFile.cpp:14-31` — retail Wii's `_ReadAsync` /
  `_ReadDone` interface that defines the async contract.
- `src/system/utl/ChunkStream.cpp:179-285` — the `TempEof` /
  `ReadDone` cooperation that the matched fork is built around.
- `native/src/native_file.cpp:69-72` — the synchronous `ReadAsync` we
  need to make async.
- `native/src/main_native.cpp:543-545` — the exit-callback pattern
  for the audio thread teardown; template for the loader-thread
  teardown.
- `milo-native-engine/src/platform/ThreadCall_Native.cpp` — the
  pthread+semaphore idiom to copy.
- `milo-native-engine/src/platform/Skeleton_Native.h` — the
  `std::thread + std::mutex + volatile bool + double-buffer` pattern.
- `dc3-decomp/src/system/utl/Loader.cpp:293-298` — the DC3-native
  loader simplification (does NOT off-thread; just removes
  time-slice). The current rb3 native HX_NATIVE arms were modeled
  on this; this plan supersedes it.
- `rb3/docs/native/NATIVE_PORT_ROADMAP.md` — the broader roadmap.
- `rb3/docs/sessions/native/VIGNETTE_RENDER_SCOPE.md` — the
  vignette-rendering session that produced the tv3 force-poll
  workaround.
