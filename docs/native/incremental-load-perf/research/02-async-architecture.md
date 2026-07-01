# 02 — Milo's Loader Was Built Async: Architecture, DC3 Prior Art, and the Minimal Correctness Set

_Read-only investigation, 2026-06-10. Companion to the incremental-load-perf wave._
_Sources: `src/system/utl/Loader.{h,cpp}`, `src/system/obj/DirLoader.{h,cpp}`,
`src/system/obj/Dir.h` (ObjDirPtr), `src/system/char/FileMerger.{h,cpp}`,
`src/system/rndobj/Tex.cpp`, `src/system/synth/MoggClip.cpp`,
`native/src/native_file.cpp`, `milo-native-engine/src/platform/WebAssets.cpp`,
DC3 `docs/native/LOADING_ARCHITECTURE.md` / `FILEMERGER_CONVERGENCE.md` /
`docs/sessions/2026-03-20-sync-load-hang.md` /
`docs/sessions/convergence/05-filemerger-async-pipeline.md`, rb3 commits
`ad3b7e61` + `585ad0f8` (prewarm), `55c2d8e0` (JSPI yield throttle)._

---

## TL;DR — the single biggest insight

**Milo's loader is a fully cooperative async pipeline that the native port
quietly de-asynced at the `File` seam.** Every layer above the `File` class —
`FileLoader`/`DirLoader` state machines, `ChunkStream`'s `TempEof`,
`CheckSplit()` time-slicing, the front-of-queue preemption discipline,
`Loader::Callback`, FileMerger/Organizer — is *already* structured to tolerate
"bytes not here yet, come back next poll". On Wii, `File::ReadAsync()` returned
immediately (DVD DMA in flight) and `ReadDone()` was polled; `ChunkStream::Eof()`
returned `TempEof` until a chunk arrived, and every loader state function loops
`while (Eof() != NotEof) { if (CheckSplit()) return; }` — a *poll-and-yield* path
that is **dormant dead code on native/web today**, because
`NativeStdioFile::ReadAsync()` does the read inline (`native_file.cpp:300`) and
the constructor does a **blocking sync XHR** on a MEMFS miss
(`native_file.cpp:258` → `WebAssetsFetchSync`, `WebAssets.cpp:339-340`
`xhr.open(GET, url, false)`). Restoring the Wii I/O contract on web (async fetch
started at open, `ReadDone`/`TempEof` until it lands) re-activates the engine's
own async machinery with near-zero changes to game code — the 17
`PollUntilLoaded` call sites then mostly *backstop already-finished loads*
instead of serializing multi-MB network fetches inside one `RunOneFrame`.

The 17 sync call sites are NOT the architecture; they are **backstops and
convenience shortcuts** layered on an async design (several even log
`MILO_WARN("... blocked while loading ...")` when the backstop actually has to
block — the original authors considered hitting it a performance bug).

---

## (a) The async machinery as designed

### LoadMgr: one queue, one front, budgeted polls

`LoadMgr` (Loader.h:46) keeps two lists: `mLoaders` (every live loader, used by
`GetLoader`/`DirLoader::Find` for sharing/adoption) and `mLoading` (the work
queue). Per frame, `SystemPoll()` (System.cpp:678) calls `TheLoadMgr.Poll()`,
which repeatedly calls `PollFrontLoader()` until either the queue empties or the
period budget (`mPeriod`, 10 ms stock; `unk1c` is the live slice value) trips
(`mTimer.Split()` / Wii arm at Loader.cpp:536-546). **Only the front loader is
ever polled** — this single-lane discipline is load-bearing (see hazards, §c).

`PollFrontLoader()` (Loader.cpp:549):
- saves/restores `mLoaderPos` and sets it to `front->mPos` — so any loaders
  *created during* this poll know what position discipline to inherit;
- `MemPushHeap(front->mHeap)` … `MemPopHeap()` — every allocation a loader makes
  lands on the heap recorded at the loader's *construction* (`Loader::Loader`
  captures `GetCurrentHeapNum()`), regardless of which frame the work actually
  runs on. This is what makes deferring work across frames heap-correct by
  construction.

### LoaderPos semantics (Loader ctor, Loader.cpp:587-607)

```cpp
if (mPos == kLoadFront)        mLoading.push_front(this);          // 0
else if (mPos == kLoadStayBack) mLoading.push_back(this);          // 3
else { // kLoadBack(1), kLoadFrontStayBack(2)
    // scan from the back; insert AFTER the last loader with pos <= 1
    // (i.e. ahead of the trailing block of StayBack/FrontStayBack loaders)
}
```

- **kLoadFront (0)** — *preempt everything*. Used for sub-resources of the
  currently-loading dir (textures, samples, proxies) and for user-blocking loads.
  Front-insert means a freshly created sub-loader runs before its parent resumes.
- **kLoadBack (1)** — background FIFO, but still ahead of the stay-back block.
  This is what UIPanel uses by default (`UIPanel::Load`, pos = `kLoadBack`
  unless the panel's DTA `(file ... pos)` overrides) — **screen panel loads are
  background-async by design**, drained by the per-frame Poll budget.
- **kLoadFrontStayBack (2)** — "front *of the stay-back lane*": a sub-load
  spawned by a stay-back parent. It must run before its parent continues but
  must NOT preempt normal front/back traffic. `ObjDirPtr::LoadFile` and
  `LoadInlinedFile` (Dir.h:74-77, 95-98) auto-promote to this when
  `TheLoadMgr.GetLoaderPos()` says the *currently polled* loader is itself
  stay-back — i.e. position discipline is **inherited through nesting**.
- **kLoadStayBack (3)** — absolute lowest priority; always `push_back`, and
  other loaders' else-branch inserts ahead of it.

### The state machines and their three yield conditions

`FileLoader`: `OpenFile → LoadFile → DoneLoading` (or `LoadStream` for inlined
streams). `OpenFile` calls `NewFile` then **`mFile->ReadAsync(buffer, len)`**
and moves to `LoadFile`, which polls **`mFile->ReadDone(asdf)`** every
`PollLoading()` until the DVD read lands (Loader.cpp:641-691). On Wii the whole
multi-MB read overlapped rendering; `Poll()` just burned its budget spinning
`ReadDone` and returned to the frame loop.

`DirLoader`: `OpenFile → LoadHeader → CreateObjects → [LoadResources] → LoadDir
→ LoadObjs → DoneLoading` (DirLoader.cpp). Every state yields on up to three
conditions:

1. **Data not here yet**: `while (t = mStream->Eof(), t != NotEof) {
   MILO_ASSERT(t == TempEof); if (TheLoadMgr.CheckSplit()) return; }` —
   `TempEof` is ChunkStream's "chunk still in flight" signal. The state function
   *returns without advancing `mState`* and is re-entered on a later poll.
2. **Slice budget exceeded**: `if (TheLoadMgr.CheckSplit()) return;` after each
   object in `CreateObjects`/`LoadObjs` — CPU parse work is time-sliced at
   per-object granularity.
3. **Preempted**: `if (TheLoadMgr.GetFirstLoading() != this) return;` — a
   kLoadFront sub-loader was pushed in front of me; stop and let it run.

The `mPostLoad` flag (DirLoader.h:75) is the resume cookie for the elegant
**PreLoad/PostLoad split**: `LoadObjs` runs `obj->PreLoad(stream)` (which for a
resource-bearing object *creates a kLoadFront FileLoader* — e.g.
`RndTex::PreLoad`, Tex.cpp:321: `new FileLoader(..., kLoadFront, ...)` starts
the texture's own DVD read), sets `mPostLoad = true`, then checks
`GetFirstLoading() != this` → **yields to the texture's loader**, which gets
polled (async read in flight) until done and popped; the DirLoader becomes front
again, re-enters `LoadObjs` with `mPostLoad == true`, skips PreLoad, and runs
`obj->PostLoad(stream)` — where `RndTex::PostLoad → SetBitmap(mLoader)`'s
`PollUntilLoaded(fl, 0)` (Tex.cpp:181) finds the loader **already finished** and
falls straight through. *The blocking call is a backstop, not the mechanism.*

### PollUntilLoaded / PollUntilEmpty / ForceGetLoader — the synchronous contract

`PollUntilLoaded(ldr1, ldr2)` (Loader.cpp:176) arms `unk1c = 1e30f` (disables
CheckSplit) and pumps `PollFrontLoader()` until `ldr1->IsLoaded()`. The `ldr2`
parameter is the **circular-dependency guard**: `ObjDirPtr::PostLoad(loader)`
passes the loader doing the asking; if the asked-for loader is waiting on the
asker (`ldr2 == mLoading.front()`), `MILO_FAIL("circular dependency")`.
`PollUntilEmpty` sets `mPeriod = 1e30f` and drains the whole queue (App boot).
`ForceGetLoader(fp)` (Loader.cpp:112) = lookup-or-create at kLoadFront +
PollUntilLoaded — the "I need these bytes *now*" one-liner that Tex/CubeTex/
SynthSample/SyncSubDir use.

The web arms (current state): `Poll()` is per-frame budgeted
(`RB3_LOADER_BUDGET_MS=8`, returns to the frame loop — the landed fix);
`PollUntilLoaded`/`PollUntilEmpty` keep the synchronous contract but slice at
8 ms and `emscripten_sleep(0)` every `RB3_LOADER_MIN_YIELD_MS=16` of work so the
*tab* composites while the *game frame loop* stays suspended. That is exactly
the stall shape the user sees: canvas frozen, CSS overlay alive.

### Loader::Callback, DirLoader completion, SyncObjects timing

`Loader::Callback` (Loader.h:21) is the completion-event interface:
`FinishLoading(Loader*)` / `FailedLoading(Loader*)`. `DirLoader` fires it from
`LoadObjs` → `Cleanup(0)` (success) or `Cleanup(msg)` (failure). Ordering
guarantee callers get (DirLoader.cpp:688-694, 705-738): by the time
`FinishLoading` runs, the dir is fully deserialized **and
`mDir->SyncObjects()` has already run** (Cleanup calls it when `IsLoaded()`).
Proxy dirs (`mDeleteSelf`) self-delete inside Cleanup. `NullLoader`
(Loader.h:149) is the degenerate completes-next-poll loader used for empty
selections so the callback chain still fires.

Async consumption patterns already in the tree (the "how it's supposed to be
used" catalogue):

| Pattern | Where | Shape |
|---|---|---|
| Poll-the-loader | `UIPanel::PollForLoading`/`CheckIsLoaded` (UIPanel.cpp:209-241), `UI::Poll` transition gate (UI.cpp:556-603 `mTransitionScreen->CheckIsLoaded()`) | screen transitions are **already async**: UI polls per frame until panels finish |
| Callback | `BandDirector::VenueLoader` (BandDirector.cpp:50-65 — `PollUntilLoaded` only when `!async`), FileMerger | completion event drives next step |
| Deferred-deref backstop | `ObjDirPtr<T>` (Dir.h:63-130): `LoadFile(async=true)` stores `mLoader`; first deref/assign triggers `PostLoad` → `PollUntilLoaded` | async until someone actually needs the pointer |
| Batch-poll | `PatchDir::LoadStickerTex(push=true)` → `mStickersLoading` polled per frame (PatchDir.cpp:810-838) | sync arm exists but is opt-in |
| Pending-play | `SongPreview` state machine (SongPreview.cpp:161-228) polls `mStream->IsReady()` in `kPreparingSong` | audio prep is poll-gated downstream |

### FileMerger + Organizer: the async dir-merge pipeline

`FileMerger::StartLoadInternal(async, loading)` (FileMerger.cpp:94): fires the
`change_files` DTA message (handlers translate selections into file paths),
queues dirty mergers into `mFilesPending`, sorts by the Organizer's category
priority, then **branches**: `async=true` → `TheFileMergerOrganizer->
AddFileMerger(this)` (a `FileMergerOrganizerLoader` sits in the LoadMgr queue
and dispatches one merger at a time through normal per-frame polling);
`async=false` → `LaunchNextLoader(); while (!mFilesPending.empty())
TheLoadMgr.Poll();` — **the nested-poll drain that hung DC3** (§b).
`LaunchNextLoader` (FileMerger.cpp:213) picks kLoadFront vs kLoadFrontStayBack
based on whether the FileMerger's own dir is still loading at a stay-back pos —
nesting discipline again. Completion: `FinishLoading` → `NotifyFileLoaded`
(**`merger.Clear()` deletes the previous content here**, then `on_pre_merge`) →
`MergeDirs` → `PostMerge` (`on_post_merge`, launch next pending loader).

### What the Wii actually got out of all this

DVD reads were interrupt/DMA-driven (`ReadAsync`/`ReadDone`), so I/O fully
overlapped simulation+render; the ~10 ms `LoadMgr::Poll` budget bounded CPU
parse per frame; kLoadFront gave sub-resource dependency ordering without any
dependency graph (just queue position); and the sync drains existed for the
boot path (frame loop not running yet) and for genuinely-blocking moments
(`TrackPanel::Reload` mid-gameplay restart) — moments where the Wii showed a
loading spinner anyway. **Guarantee summary**: after `FinishLoading` or
`IsLoaded()`, the dir is complete + SyncObjects'd; before that, callers must not
touch it; the frame loop never stops.

---

## (b) DC3 prior art — what was tried, what broke, what's left

DC3 docs read: `LOADING_ARCHITECTURE.md`, `FILEMERGER_CONVERGENCE.md` (Phases
1-5 complete), `sessions/2026-03-20-sync-load-hang.md`,
`sessions/convergence/05-filemerger-async-pipeline.md`, `plans/web-port/{PLAN,AUDIO}.md`.

### History (pre-JSPI → now)

1. **ASYNCIFY rejected** for the audio path (AUDIO.md): instrumenting every
   function cost ~50 % wasm bloat. DC3 went SAB-ring + AudioWorklet ("No
   ASYNCIFY needed"), a *push* model. The same aversion shaped early loading
   work: instead of making waits suspendable, DC3 **bypassed the loader** —
   hand-rolled `DirLoader::LoadObjects()` venue loading in App.cpp, manual song
   merges in GamePanel, skipped `IsWorldLoaded()` gates (~60 HX_NATIVE guards).
2. **The sync-load-hang** (2026-03-20): `NativeVenueInit()` ran *inside
   `BeginDrawing()`* and called `StartLoad(false)` → the nested
   `while (!mFilesPending.empty()) TheLoadMgr.Poll();` loop. The process
   "ran" but the main loop never advanced: ~800 frames in 15 s, no frameCount++,
   no input, no UI/Task/Flow polls; chained re-loads from `FinishLoading →
   PostMerge → StartLoad(false)` made it effectively unbounded. On web the same
   path plus cascade `~ObjectDir` destruction overflowed the fixed 4 MB wasm
   stack.
3. **The convergence fix** (2026-03-17, Phases 1-5): *the engine pipeline
   already worked* — `world_panel` loads `world.milo`, `world.fm::PreLoad` fires
   `change_files`, DTA wires `mMerger`, the cascade self-drives. The fix was to
   **delete the bypasses and force `async = true` in `StartLoadInternal` on
   native** (plus removing the GamePanel/HamDirector/Game gate-skips so the
   *poll gates* — `IsWorldLoaded()` etc. — did the waiting). Runtime verified;
   ~60 guards → ~20-25.
4. **What still stutters in DC3**: async loading moves the *wait* off the
   frame loop but not the *completion work*. `FinishLoading` does
   `merger.Clear()` (bulk deletes), `MergeDirs`, hash-table fixups, then GPU
   uploads on first draw — all in the completion frame. And native "async" I/O
   is still synchronous underneath (05-filemerger doc: "On native, all file I/O
   is synchronous... DirLoader advances through all stages in a small number of
   PollLoading calls"), so a big milo's parse cost lands in few frames.

### Concrete lesson list (the "X before Y or Z" rules)

- **L1 — Never nest a drain inside the frame loop.** Any
  `while(pending) TheLoadMgr.Poll()` / `PollUntilLoaded` reached from inside
  `RunOneFrame` suspends *everything* (input, UI, tasks, frame counter). If a
  completion callback can issue another sync load, the nest chains unboundedly.
- **L2 — The DTA cascade must stay ordered, not synchronous.** `mMerger` must be
  wired (via `change_files`) before `OnLoadSong` selects into it;
  `IsWorldLoaded()` (venue + merger + moveMerger all pending-free) must gate
  gameplay start. These are *poll gates*, and polling them works — DC3's bugs
  came from *skipping* the gates, not from async itself.
- **L3 — `FileMerger::PreLoad`'s `StartLoadInternal(true, true)` is safe**: it
  runs inside the DirLoader state machine, which already owns the pipeline. The
  *dangerous* variant is `StartLoad(false)` from frame-loop code.
- **L4 — Completion-side spikes are the residual stutter**: Clear() deletes +
  MergeDirs + first-draw GPU upload all land in one frame. Async conversion
  without time-slicing completion work trades a freeze for a hitch.
- **L5 — Deep destruction cascades can overflow the 4 MB wasm stack**
  (recursive `~ObjectDir` during PostMerge old-content deletion). Moving
  deletion to a later frame doesn't reduce its depth.
- **L6 — Loader ownership is pointer-identity, not path-identity** (rb3 prewarm
  fix `585ad0f8`): `DirLoader::Find(fp)` can return *someone else's* loader for
  the same file (an in-flight `ObjDirPtr::LoadFile`, another panel's unpolled
  `mLoader`). Adopting/deleting a loader you didn't issue is a use-after-free.
  Any async/prewarm scheme needs an issued-set or owner field.
- **L7 — Desync turns into a hang on web**: `ReadDead`'s unbounded marker scan
  (DirLoader.cpp:594) loops forever on a desynced stream where Wii would hit
  RealEof; web needed a scan cap. Async streams must preserve *exact* byte
  positions or fail loudly.
- **L8 — JSPI suspends are not free**: each `emscripten_sleep(0)` is a 4-16 ms
  event-loop round trip. Yield-per-slice was pathological (~2,600 suspends ≈
  11 s of the App ctor); throttling to one per 16 ms of work fixed it
  (`55c2d8e0`). Any per-frame async scheme should prefer **returning from
  RunOneFrame** (free — RAF re-ticks) over mid-frame suspends.

---

## (c) Ordering/reentrancy hazards of making the 17 sync sites async

Inventory with the next-line dependency and existing still-loading handling:

| Site | What blocks | Next-line dependency | Async-ready machinery already there? |
|---|---|---|---|
| `Dir.cpp:171` `ObjectDir::Load` | proxy dir's pending loader | caller assumes proxy populated after `Load` returns | runs inside outer DirLoader poll (nested) — front-insert usually makes it near-done |
| `Dir.cpp:503` `SetProxyFile` | fresh kLoadFront DirLoader into `this` | caller (DTA `override_proxy`, milo editor flows) expects objects present | none — true sync contract |
| `Dir.cpp:903` `SyncSubDir` | `ForceGetLoader(fp)` | immediately `GetDir()` + FindObject merge walk | none |
| `DirLoader.cpp:141` `LoadObjects` (static) | **stack-local** DirLoader | returns `l.GetDir()` | none — loader dies at scope exit; cannot be deferred without API change |
| `UIPanel.cpp:373` `OnLoad` (DTA `{load TRUE}`) | panel's own loader | `MILO_ASSERT(CheckIsLoaded())` | YES — the panel state machine; the blocking arm is the DTA opt-in |
| `UI.cpp:259` `Automator::ToggleAuto` | DataLoader for auto script | `dl->Data()` | debug tool; leave sync |
| `Tex.cpp:181/222`, `CubeTex.cpp:29` | texture FileLoader | `GetBuffer` → bitmap create | YES in the dir-load path (PreLoad started the read; backstop normally no-ops). Direct `SetBitmap(FilePath)` callers are cold paths |
| `SynthSample.cpp:40` `Sync(sync0)` | `ForceGetLoader(mFile)` | `GetBuffer` → sample decode | sync3 arm shows the async shape (loader handed in, already loaded) |
| `MoggClip.cpp:200` `EnsureLoaded` | mogg FileLoader (multi-MB!) | `GetBuffer` → `Play()` makes the stream | partially — `LoadFile()` already issues the loader early (hover); `Play()` is the impatient consumer. **This is the preview freeze** (sync XHR of the .mogg + decrypt + vorbis prime in one frame) |
| `BinkClip.cpp:212` | same shape as MoggClip | stream create | same |
| `BeatMaster.cpp:61` | BeatMasterLoader when `b=true` | song data ready for gameplay | YES — `b=false` is the async arm + `LoaderPoll()` exists |
| `BandDirector.cpp:56` `VenueLoader::Load` | venue DirLoader when `!async` | — | YES — callback (`FinishLoading`) is the primary path |
| `TrackPanel.cpp:277` `Reload` | track panel dir | immediate `CheckIsLoaded`/`FinishLoad` + `ResetPlayers` | restart-after-win path; UIPanel machinery exists but callers expect completion |
| `PatchDir.cpp:836` `LoadStickerTex(push=false)` | sticker tex | `FinishLoad()` | YES — `push=true` arm is the per-frame batch poll |
| `DataFile.cpp:768` `~DataLoader` | own thread/parse completion | destructor must not leave in-flight thread | inherent — destructor drain; keep |
| `LightHue.cpp:56` `Sync` | hue .bmp FileLoader | pixel walk → keys | PostLoad-initiated; same PreLoad/PostLoad shape as Tex would fix it |

**Hazards any conversion must respect:**

1. **Front-of-queue is a mutual-exclusion lock.** Only the front loader runs;
   `FilePathTracker(mRoot)` (a *global* file-root swap), `gLoadingProxyFromDisk`,
   and `LOAD_REVS`' rev stack are per-front-loader global state. Two dir loaders
   interleaving at fine grain would corrupt each other. Time-slicing across
   frames is safe (state is re-established per `PollLoading` entry); polling
   loaders *concurrently* is not.
2. **kLoadFront insertion order is dependency order.** Sub-loaders must stay in
   front of the parent that spawned them or `LoadObjs`' `GetFirstLoading() !=
   this` resume logic deadlocks/misorders. Anything that reorders `mLoading`
   (priority queues!) must never move a child behind its parent. The
   `PollUntilLoaded(ldr1, ldr2)` circular-dep MILO_FAIL is the only cycle
   detector — async schemes lose it and need their own.
3. **Heap correctness rides on `Loader` ctor time.** `mHeap` is captured at
   creation and re-pushed per poll (`PollFrontLoader`), so *deferring polls* is
   heap-safe, but moving *loader creation* to a later frame changes
   `GetCurrentHeapNum()` — create loaders at the call site, defer only the wait.
4. **Callback/owner lifetime.** `Loader::Callback` owners (UIPanel, FileMerger,
   VenueLoader) destroy loaders in their own dtors; extending loader lifetime
   across frames widens the owner-died-first window. FileMerger's
   `DeleteCurLoader` sets `unk99` so `~DirLoader` fires `FailedLoading` —
   preserve that protocol. And L6: adoption/steal requires pointer-identity
   ownership (the prewarm UAF lesson, `585ad0f8`).
5. **`IsLoaded()`-gated accessors hard-assert.** `DirLoader::GetDir()`
   (`MILO_ASSERT(IsLoaded())`), `FileLoader::GetBuffer`, `DataLoader::Data` —
   every converted site needs an explicit not-ready early-out, not just a
   skipped drain.
6. **`GetBuffer` sets `mAccessed`** → ownership transfer of the buffer/dir.
   Conversions must keep exactly one access-then-release per loader
   (double-poll → double-free; zero → leak, except `~FileLoader`'s !mAccessed
   cleanup).
7. **Nested `unk1c`/`mTimer` clobber.** A `PollUntilLoaded` reached from inside
   `PollFrontLoader` (Tex backstop inside DirLoader::LoadObjs inside Poll)
   overwrites the slice budget of the outer Poll. Tolerated today; any new
   budget logic must stay reentrancy-tolerant (restore-on-exit or accept reset).
8. **The web sync XHR is below the budget's reach.** `FileLoader::OpenFile` →
   `NewFile` → `NativeStdioFile` ctor → `WebAssetsFetchSync` blocks for the
   whole network round trip *inside one PollFrontLoader call*; no CheckSplit can
   preempt it. Even a perfectly budgeted Poll() freezes for a multi-MB mogg.
   Fixing the call sites without fixing the File seam only moves the freeze.
9. **The prewarm work already landed** (`ad3b7e61` + `585ad0f8`,
   `RB3_PREWARM_SCREENS`, default OFF): UIScreen::Poll issues kLoadBack
   DirLoaders for the *next* screen's panel milos during idle; UIPanel::Load
   adopts a finished, **issued-set-verified** prewarm loader via
   `SetLoadedDir` + `delete` instead of re-parsing. Native A/B was neutral
   (native already budget-slices); the predicted win is the web path. This is
   the template for "warm it before the user asks".

---

## (d) Architecture options

### Option 1 — Minimal: convert the hot sites to poll-across-frames (use the machinery that's already there)

Scope: no engine-contract changes; per-site conversions using existing async
arms, all `#ifdef HX_NATIVE`/`HX_WEB`:

- **Preview (the 2-5 s hover freeze)**: make the MusicLibrary→MoggClip path
  fully deferred. `MoggClip::LoadFile` already issues the kLoadFront FileLoader
  at hover; add a *pending-play* state so `Play()` (or the SongPreview
  `kPreparingSong` gate upstream) early-outs while `mFileLoader &&
  !IsLoaded()` and is retried from Poll — `SongPreview` *already polls*
  `mStream->IsReady()`, so the natural seam is "don't construct the stream
  until the loader is done". Issue the loader at kLoadBack on web so it doesn't
  preempt panel loads. Also prefetch `_prev.mogg` for the hovered row via the
  JS async fetch (warms MEMFS before EnsureLoaded fires).
- **Screen transitions**: flip `RB3_PREWARM_SCREENS` default-ON for web after a
  web A/B; extend the prewarm map beyond main_hub→song_select.
- **TrackPanel/BeatMaster/BandDirector**: use their existing `async` arms +
  poll gates on the (already async) UIPanel/loading-screen flow.
- Leave the cold/debug/destructor sites (`Automator`, `~DataLoader`,
  `SetProxyFile`) synchronous.

*Invariants preserved*: everything — queue discipline, callbacks, heaps
untouched. *Regression risk*: per-site state-machine bugs (double-GetBuffer,
not-ready deref); preview behavior change if a song is selected before its
preview loaded (must cancel pending-play). *What it cannot fix*: any remaining
MEMFS-miss still sync-XHR-freezes inside Poll (hazard 8); first-visit
transitions still fetch serially. *Effort*: days; each site independently
landable + testable.

### Option 2 — Restore the Wii I/O contract at the File seam (recommended core)

Make web file I/O genuinely async so the **existing dormant machinery** does the
rest:

- `NativeStdioFile` (web): on MEMFS miss, *start* an `emscripten_fetch` (the
  engine's async API at WebAssets.cpp:4 already exists — `WebAssetsFetch` +
  completion callbacks) instead of `WebAssetsFetchSync`; the File enters an
  *opening* state. `ReadAsync` returns immediately; `ReadDone` reports false
  until the fetch lands and bytes are read; `Size()` defers until ready.
- `FileLoader::OpenFile` needs a small web arm: stay in OpenFile (return,
  re-poll next frame) while the File reports not-ready, instead of reading
  `Size()` immediately. `LoadFile` already polls `ReadDone` — unchanged.
- `ChunkStream` (DirLoader's stream): open path likewise; if its read layer can
  report "bytes not yet available" as `TempEof`, **every DirLoader state
  function already handles it** (`while Eof()!=NotEof { if CheckSplit return }`)
  — that code was written for exactly this and currently never fires.
- Keep the sync fast path when the file is MEMFS-resident or IDB-warm (boot
  relies on cheap sync reads; the 5.36 s App-ctor win must not regress —
  measure with `RB3_BOOT_IO_STATS`).
- `PollUntilLoaded` keeps its contract but now *overlaps* network with its JSPI
  yields instead of serializing sync XHRs — the remaining sync sites degrade
  to "spinner for one fetch's latency" instead of "freeze for fetch+parse".
- Cheap bonus (the useful 20 % of Option 3): since fetch-start is independent
  of the parse lock, **start fetches for queued non-front loaders at
  AddLoader time** (network parallelism without touching the single-lane CPU
  discipline).

*Invariants preserved*: front-of-queue parse exclusivity, heap push/pop,
callback ordering, `SyncObjects` timing — all untouched; only the File layer's
readiness reporting changes. *Regression risks*: Size()-before-ready callers
outside FileLoader (audit `NewFile` users — DTA lexer, ArkFS); Eof()
position-exactness (L7 — a desynced stream must fail, not spin); subtle
double-completion if fetch lands while a JSPI suspend is in flight.
*Effort*: ~1-2 weeks (engine `native_file`/WebAssets + small HX_WEB arms in
FileLoader/ChunkStream; no matched-code changes — the Wii arms already contain
the needed control flow).

### Option 3 — Better-than-stock loader: priority queue + budget-everything + completion events

On top of Option 2: (a) priority/reorderable `mLoading` with an explicit
parent→child dependency edge (replacing positional encoding); (b) deferred
completion dispatch — `FinishLoading` callbacks queued and run under their own
per-frame budget; (c) **time-sliced completion work** — split
`merger.Clear()` deletes, `MergeDirs`, and first-draw GPU uploads across frames
(this is DC3's residual stutter, L4, and rb3's song_select ENTER ~150 ms
hitch); (d) a prediction/prewarm manager generalizing `RB3_PREWARM_SCREENS`
(hover-adjacent songs, next-screen panels, gameplay milos during countdown).

*Invariants at risk*: reordering breaks the implicit child-in-front-of-parent
guarantee (hazard 2) — needs real dependency tracking; deferred callbacks change
"loaded means SyncObjects'd *and notified*" timing that DTA scripts may observe;
sliced MergeDirs exposes half-merged dirs to Find()/draw unless gated.
*Regression surface*: large (every load path). *Effort*: weeks, engine-level,
with a long verification tail. **Not the first move** — only (c) and (d) carry
value Option 2 doesn't, and both can be added incrementally behind env flags
afterwards.

### Recommendation

**Option 2 as the core, with Option 1's preview fix and prewarm flip landed
first** (they're independent, small, and each kills a named user-visible stall).
Then take Option 3(c) (slice the completion/merge/upload spikes) only where
measurement still shows hitches. This sequencing keeps every step inside the
engine's own design — the loader never needed to be made async; it needs its
I/O layer to stop lying to it about being synchronous.

---

## Minimal correctness set for going truly async (checklist)

1. Only the front loader's `PollLoading` may run CPU parse work per slice
   (FilePathTracker / gLoadingProxyFromDisk / rev-stack exclusivity).
2. A child loader spawned during a parent's poll must run to completion before
   the parent's state function resumes past its `GetFirstLoading() != this`
   check (queue position = dependency edge).
3. Loaders are created on the caller's heap at call time (`Loader::Loader`
   captures `mHeap`); only the *wait* may be deferred.
4. `GetDir`/`GetBuffer`/`Data` only after `IsLoaded()`; exactly one
   access-then-release per loader (`mAccessed` ownership transfer).
5. `FinishLoading` fires only after the dir is complete + `SyncObjects()` ran;
   `FailedLoading` must fire if the loader dies early (`unk99` protocol).
6. Never adopt/steal/delete a loader you didn't issue (pointer-identity
   ownership, L6).
7. No `PollUntilLoaded`/`PollUntilEmpty`/nested `Poll()` drain reachable from
   inside `RunOneFrame` on a path a user can trigger per-interaction; remaining
   drains must be boot-time or spinner-covered.
8. Stream byte positions are exact under async delivery (TempEof resumes at the
   same offset; ReadDead terminates).
9. Yield by returning from RunOneFrame, not by mid-frame JSPI suspends, wherever
   possible (suspends cost 4-16 ms each, L8).
10. Completion-side work (Clear deletes, MergeDirs, GPU upload) is bounded per
    frame or accepted as a one-frame hitch — decide explicitly per site.
