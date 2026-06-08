# R1 — Async on-demand fetch: remove `WebAssetsFetchSync` main-thread blocking

**Status:** design (handoff to implementation agent)
**Owner doc:** this file
**Data source:** `docs/native/web-netperf-findings-2026-06-08.md`
**Verification tool:** `scripts/web/netperf-suite.mjs`

---

## Problem & data

Every on-demand asset miss in the web build is fetched through a **synchronous XHR
that freezes the wasm main thread for the entire transfer**. The block is invisible
on loopback (bytes arrive instantly) but scales straight with bandwidth on a real
link: `freeze ≈ bytes ÷ throughput`.

The single chokepoint is `WebAssetsFetchSync()` in
`milo-native-engine/src/platform/WebAssets.cpp:261`, which does
`xhr.open(GET, url, false)` (synchronous) inside an `EM_ASM_INT` block
(`WebAssets.cpp:287-323`). It is called from exactly one place in RB3:
`native/src/native_file.cpp:163`, inside `NativeStdioFile`'s constructor, after a
MEMFS `fopen` miss on a read-mode open.

Measured cost (from the findings doc, cold IndexedDB cache, 50 Mbit/s "low" profile):

| transition | main-thread **blocked** | worst single freeze |
|---|---|---|
| boot → main_hub | 15.5 s | 3.7 s |
| main_hub → song_select | 5.9 s | 2.2 s |
| song_select → part_difficulty | 1.7 s | 1.3 s |
| **part_difficulty → game** | **11.4 s** | **6.7 s** |

Boot main-thread frozen scales `0.9 s (unbounded) → 2.9 s (200 Mbit) → 10.0 s (50 Mbit)`
for the same ~71 MB / 98 sync XHRs. Named worst offenders (each one blocking XHR):
`20thcenturyboy.mogg` 37 MB → 6.76 s; `colorpalettes.milo_xbox` 21 MB → 3.7 s;
`small_club_01.milo_xbox` 19 MB → 3.5 s; shell vignette milos `sv*_a.milo_xbox`
~12 MB each. The findings doc's own conclusion: **"bandwidth is fine; synchrony is
the bug."**

R1 is the **root-cause fix**: replace the synchronous miss path with an async fetch
that cooperatively suspends *only the requesting loader* (via the JSPI yield that
already exists in the loader spine), so the browser keeps compositing and other work
keeps running while bytes land.

### Why the infra is already there

- `MILO_WEB_ASYNC` is **ON by default** (`milo-native-engine/CMakeLists.txt:152`),
  which adds `-sJSPI` (`CMakeLists.txt:595`), defines `MILO_WEB_ASYNCIFY=1`
  (`CMakeLists.txt:593`), and enables `-sFETCH=1` (`CMakeLists.txt:581`). RB3's
  web target extends the JSPI export set (`native/CMakeLists.txt:876-880`).
- `emscripten_sleep(0)` already runs as the JSPI suspend/resume point in
  `LoadMgr::PollUntilLoaded` (`src/system/utl/Loader.cpp:219`) and
  `LoadMgr::Poll` (`Loader.cpp:405`). The cooperative time-slice loop that returns
  mid-load is already the working pattern (`Loader.cpp:185-220`).
- `WebAssets.cpp` already has a **fully async fetch path**: `WebAssetsFetch()`
  (`WebAssets.cpp:118`) issues an `emscripten_fetch` with `onFetchSuccess`/`onFetchError`
  callbacks that write the bytes into MEMFS, plus `WebAssetsFetchDone(id)`
  (`WebAssets.cpp:145`) to poll a single fetch by id. **This is the exact primitive
  R1 needs — it is currently used only for the boot bundle/manifest, never for the
  on-demand miss path.**

So R1 is mostly *rewiring the miss path from the existing sync primitive to the
existing async primitive*, plus a small state-machine change so a not-yet-landed
fetch reports "not done" instead of blocking.

---

## Architecture

### The two layers that must change

The miss happens deep inside a synchronous open. Two layers sit above it:

1. **`FileLoader` state machine** (`src/system/utl/Loader.cpp:556-672`) — the
   loader the milo dependency walk creates per file. It runs
   `OpenFile → LoadFile → DoneLoading` driven by `PollLoading()`. `OpenFile()`
   (`Loader.cpp:583`) calls `NewFile(fname, mFlags|2)` (`Loader.cpp:594`), which
   reaches `NativeStdioFile`'s ctor → the sync XHR. `OpenFile()` then immediately
   calls `mFile->ReadAsync(...)` and advances to `LoadFile`; `LoadFile()`
   (`Loader.cpp:622`) calls `mFile->ReadDone(asdf)` and, when done, advances to
   `DoneLoading`. `IsLoaded()` returns `mState == &DoneLoading` (`Loader.cpp:654`).

2. **`NativeStdioFile`** (`native/src/native_file.cpp:119-267`) — RB3's own `File`
   backend. Its **constructor** is where the open happens, and where the IDB-cache
   try + sync XHR fall-through live (`native_file.cpp:137-168`). `Read()`/`ReadAsync()`
   are plain `fread` from MEMFS (`native_file.cpp:196-204`); `ReadDone()` always
   returns `true` (`native_file.cpp:251`).

### The control-flow change

Today, `NewFile()` is a blocking call: it does not return until the file is in
MEMFS. R1 splits the open into **kick-off** and **completion-poll**, and threads
"still pending" back up through the loader's existing poll cadence:

```
FileLoader::OpenFile()              (was: NewFile blocks on sync XHR)
  └─ NewFile() → NativeStdioFile ctor
        MEMFS fopen miss?
          ├─ IDB cache hit  → write MEMFS, fopen succeeds   (unchanged, sync, instant)
          └─ miss           → WebAssetsFetch(rel)  [ASYNC, returns fetchId]
                              record fetchId on the File, mark "pending open"
                              return a File that reports Fail()==false but Pending()==true
  └─ if file Pending → DO NOT call ReadAsync; stay in OpenFile state, return.

FileLoader::PollLoading()  (next poll, after emscripten_sleep yielded the browser)
  └─ OpenFile() re-entered:
        WebAssetsFetchDone(fetchId)?
          ├─ no   → still pending, return (loader stays not-loaded; LoadMgr yields)
          └─ yes  → fopen the now-resident MEMFS file, size it,
                    ReadAsync(buffer), advance to LoadFile  (unchanged from here)
```

The crucial property: **between the fetch kick-off and its completion, the wasm
stack unwinds back to `LoadMgr::Poll`/`PollUntilLoaded`, which calls
`emscripten_sleep(0)` (already present), yielding a full browser event-loop turn.**
The browser downloads the bytes off-thread (`emscripten_fetch` runs the XHR async),
the page keeps compositing the loading overlay, and the next poll tick re-enters
`OpenFile()` to check completion. No single frame is frozen for the transfer; the
freeze becomes a sequence of cheap "still pending?" polls.

### Where the async state lives

`NativeStdioFile` already exists per-open and is the natural home for the pending
fetch id. Add three members and two methods (`Pending()`, and a re-probe the ctor's
miss path is refactored into). The `FileLoader` does **not** need new members —
re-entrancy of `OpenFile()` keyed on `mFile->Pending()` carries the state. But
`OpenFile()` must become re-enterable: today it unconditionally creates `mFile`
and advances state. It must instead: if `mFile` already exists and is pending,
re-probe; only create-or-advance when there is no pending fetch.

### Interaction with the IDB cache and loader budget

- **IDB cache** (`native_file.cpp:66-113`, `cacheTryHit`/`cachePutAfterFetch`): the
  cache hit path stays **synchronous and instant** (it's a local JS-Map read +
  MEMFS write, no network). Only the *miss* path goes async. After an async fetch
  lands, `cachePutAfterFetch` runs in the completion branch (same as today).
- **Per-frame loader budget** (`RB3_LOADER_BUDGET_MS`, `Loader.cpp:339`/`432`): a
  pending-fetch poll is near-zero CPU, so it never trips the budget — the loop just
  spins cheaply on `WebAssetsFetchDone()` and `emscripten_sleep`-yields. This is
  strictly *better* than today, where one open burns `bytes÷throughput` of frame time.
- **Boot spine** (`native/src/main_web.cpp`): boot's heavy loads run through
  `PollUntilEmpty`/`PollUntilLoaded` during the App ctor and per-screen transitions.
  Those already drain-with-yield (`Loader.cpp:163-255`, `257-279`). A pending async
  open simply means `IsLoaded()` stays false for a few more poll iterations; the
  existing `while (!theLdr->IsLoaded())` loop with `emscripten_sleep(0)` already
  handles "not loaded yet" — it just never had a non-blocking *open* to wait on
  before. The iteration cap (`maxIter`, `Loader.cpp:186`) is a safety valve; verify
  it is generous enough for a slow 37 MB fetch at 50 Mbit (~6 s of poll spins at
  ~1 ms/iter = ~6000 iters, well under the 200000 cap).

### Concurrency note

`emscripten_fetch` with one in-flight request per requesting loader is the minimal
correct model. Because the loader spine is single-threaded and processes the front
loader to completion before the next, only one async open is typically in flight at
a time. R1 does **not** need to parallelise fetches to win — converting blocking to
non-blocking already collapses the freeze (the throughput is unchanged, the *frame
ownership* changes). Parallel prefetch of the next screen's working set is **R2/R3**,
which build on R1's async primitive; R1 deliberately stops at "one async open, not
blocking."

---

## Implementation plan

Phased so each step is independently testable with `netperf-suite.mjs`.

### Phase 1 — Add an async fetch-by-path that the File layer can poll

`WebAssets.cpp` already has `WebAssetsFetch(serverPath)` → `int id` and
`WebAssetsFetchDone(id)`. Two gaps to close for the File layer:

- `WebAssetsFetch` takes a **server-relative** path (`config/foo.dta`) and writes
  to `/data/<serverPath>`. `NativeStdioFile` works with the path the loader hands
  it (often relative, anchored to `/data` via the existing `fetchPath` logic at
  `native_file.cpp:153-155`). Add a thin `WebAssetsFetchAsync(const char *memfsPath)`
  that mirrors `WebAssetsFetchSync`'s path-normalisation header
  (`WebAssets.cpp:267-274`) and returns the fetch id — so the sync and async miss
  paths agree on URL/MEMFS-path derivation. (Or: reuse `WebAssetsFetch` after
  normalising in `native_file.cpp`; either is fine, but keep one normalisation
  source of truth.)
- Ensure `onFetchSuccess` writes to the **same** absolute MEMFS path the subsequent
  `fopen` will resolve (the sync path already solved this; mirror it). The bundle
  unpacker's `..`-resolution (`WebAssets.cpp:196-214`) is not needed here — on-demand
  paths don't carry `..`.

Files/symbols: `WebAssets.cpp` (+ `WebAssets.h` decl) — add `WebAssetsFetchAsync` /
`WebAssetsFetchDone` is already declared.

### Phase 2 — Make `NativeStdioFile` open non-blocking on a miss

In `native/src/native_file.cpp`:

- Add members: `int mFetchId = 0;` `bool mPending = false;` and the original open
  `mode`/`path` (already captured locally; promote to members so a re-probe can
  re-`fopen`).
- Refactor the ctor's miss block (`native_file.cpp:137-168`): keep the IDB-cache
  try (instant). On cache miss, instead of `WebAssetsFetchSync(...)`, call
  `WebAssetsFetchAsync(fetchPath)`, store `mFetchId`, set `mPending = true`, and
  **leave `mFp == nullptr` with `mFail = false`** (do not set fail for a pending
  fetch — a pending open is not a failed open).
- Add `bool Pending()` and a `bool TryFinishOpen()` method: if `mPending`, check
  `WebAssetsFetchDone(mFetchId)`; if done, `fopen` the now-resident file, run the
  `setvbuf`/`mSize` caching block (`native_file.cpp:174-188`), run
  `cachePutAfterFetch`, clear `mPending`, and return true. If still pending, return
  false. Set `mFail` only if the fetch completed but `fopen` still missed (real 404).
- `Fail()` (`native_file.cpp:236`) must return false while `mPending` (so the loader
  doesn't treat a pending open as a load failure). Add a `Pending()` accessor to the
  `File` vtable **or** keep it off-vtable and have the loader query via a
  web-only downcast/free function (see Phase 3 note).

### Phase 3 — Make `FileLoader::OpenFile` re-enterable on a pending open

In `src/system/utl/Loader.cpp`, guarded by `HX_WEB`:

- After `mFile = NewFile(fname, mFlags|2)` (`Loader.cpp:594`), if the returned file
  reports **pending** (not failed, not yet open), **do not** call `ReadAsync` /
  advance to `LoadFile`. Leave `mState = &OpenFile` and return. The loader stays
  not-loaded; `LoadMgr::Poll`/`PollUntilLoaded` will re-`PollLoading()` it next tick.
- On re-entry to `OpenFile()` with an existing pending `mFile`: call
  `TryFinishOpen()`. If still pending, return. If finished, fall into the existing
  `if (mFile && !mFile->Fail())` block (`Loader.cpp:608`) → `ReadAsync` → advance.
- **How does the loader ask "pending?"** Cleanest: add a non-pure virtual
  `bool File::Pending() { return false; }` to `src/system/os/File.h` (default false
  → zero behaviour change for every other backend and for the Wii matched build,
  since nothing reads it there). `NativeStdioFile::Pending()` overrides it. Gate the
  `OpenFile` re-entry logic on `#ifdef HX_WEB`. **Caution:** adding a virtual to
  `File` shifts the vtable — but `File` is RB3-native/web-only here (the matched Wii
  asm of `File` methods is not what the port matches), and the added virtual goes at
  the *end* of the declared virtuals, after `Truncate` (`File.h:33`). Confirm no
  matched TU depends on `File`'s vtable layout (it shouldn't — File is engine glue);
  if it does, fall back to a `#ifdef HX_WEB` free function
  `bool NativeFileIsPending(File*)` that `dynamic_cast`/type-tags instead of a vtable
  slot.

### Phase 4 — Verify the boot spine and budget interplay

- Confirm `PollUntilLoaded` (`Loader.cpp:163`) and `PollUntilEmpty` (`Loader.cpp:257`)
  correctly spin on the now-pending loader. They already loop `while (!IsLoaded())`
  with `emscripten_sleep(0)` — a pending open just means more iterations. No code
  change expected; **add a poll counter / log** to confirm the pending loader is
  re-entered (reuse the existing `polls % 200` log at `Loader.cpp:209`).
- Confirm the per-frame budget (`Loader.cpp:382-408`) returns promptly when the
  front loader is pending (it should: a pending poll is sub-millisecond, the
  `budgetTimer` won't trip, but `mLoading.front()` won't be `IsLoaded()` so it won't
  pop — the loop keeps calling `PollFrontLoader` which keeps returning fast; the
  `sinceYield` gate (`Loader.cpp:403`) yields every `sYieldMs`). **Risk to check:** a
  pending front loader could spin the budget loop tightly without yielding until
  `sYieldMs` — acceptable (it's cheap), but consider yielding immediately when the
  front loader is pending rather than busy-spinning `maxIter` times. Add an
  early `emscripten_sleep(0); break;` when `mLoading.front()` is web-pending so the
  budget loop hands the browser a turn the instant it hits a pending fetch.

### Phase 5 — Remove (or keep as fallback) `WebAssetsFetchSync`

Once the async path is verified, `WebAssetsFetchSync` is dead on the RB3 path. Keep
it compiled (DC3's `AsyncFile_Native.cpp:40` still calls it, and it's a useful
last-resort) but ensure RB3's `native_file.cpp` no longer calls it on the hot path.
Optionally gate a `RB3_SYNC_FETCH=1` env to restore the old blocking path for A/B.

---

## Key files & call sites

Verified against the working tree (2026-06-08):

- `milo-native-engine/src/platform/WebAssets.cpp:261` — `WebAssetsFetchSync` (the
  blocking XHR, `xhr.open(...,false)` at `:292`). **The thing being bypassed.**
- `milo-native-engine/src/platform/WebAssets.cpp:118` — `WebAssetsFetch(serverPath)`
  → async `emscripten_fetch`; `:145` `WebAssetsFetchDone(id)`. **The primitive R1 reuses.**
- `milo-native-engine/src/platform/WebAssets.cpp:60` `onFetchSuccess` /
  `:92` `onFetchError` — async callbacks that write MEMFS + bump counters.
- `milo-native-engine/src/platform/WebAssets.h` — add `WebAssetsFetchAsync` decl.
- `rb3/native/src/native_file.cpp:119` — `NativeStdioFile` (ctor at `:121`,
  miss path `:137-168`, the `WebAssetsFetchSync` call at `:163`). **Primary edit.**
- `rb3/native/src/native_file.cpp:66` `cacheTryHit` / `:100` `cachePutAfterFetch` —
  IDB cache (keep instant; reuse in completion branch).
- `rb3/native/src/native_file.cpp:196` `Read`/`:201` `ReadAsync`/`:236` `Fail`/
  `:251` `ReadDone` — make `Fail()` honour pending; add `Pending()`/`TryFinishOpen()`.
- `rb3/native/src/native_file.cpp:363` `HmxNativeOpenFile` — the `NewFile` body that
  constructs `NativeStdioFile`; returns the (possibly pending) handle.
- `rb3/src/system/utl/Loader.cpp:583` `FileLoader::OpenFile` — make re-enterable on
  pending; `:611` `ReadAsync`; `:622` `LoadFile`; `:656` `PollLoading`; `:654`
  `IsLoaded`. **Primary loader edit (HX_WEB-gated).**
- `rb3/src/system/utl/Loader.cpp:163` `PollUntilLoaded` (yield at `:219`),
  `:257` `PollUntilEmpty`, `:305` `Poll` (budget loop `:382-408`, yield `:405`) —
  the JSPI yield spine; verify pending-loader interplay.
- `rb3/src/system/os/File.h:8` `class File` — add `virtual bool Pending()` (default
  false) after `Truncate` (`:33`).
- `rb3/src/system/os/File.cpp:158` `NewFile` (HX_NATIVE → `HmxNativeOpenFile`).
- `milo-native-engine/CMakeLists.txt:152` `MILO_WEB_ASYNC` ON, `:581` `-sFETCH=1`,
  `:593` `MILO_WEB_ASYNCIFY=1`, `:595` `-sJSPI` — infra already enabled.
- `rb3/native/src/main_web.cpp:686` `MILO_WEB_ASYNCIFY` tick loop
  (`requestAnimationFrame(tick)` + `await Module._rb3MainLoopTick()`) — the JSPI
  driver that makes `emscripten_sleep` suspend/resume work.

Reference (do **not** edit, model only):
- `dc3-decomp/native/src/platform/AsyncFile_Native.cpp:29` `_OpenAsync` /
  `:70` `_ReadAsync` / `:77` `_ReadDone` — DC3's async-file shape. **NB:** its open
  is *not* actually async — `AsyncFile::Init()`
  (`rb3/src/system/os/AsyncFile.cpp:303-305`) spins `while (!_OpenDone());`
  synchronously, and `_OpenAsync` itself calls the blocking `WebAssetsFetchSync`
  (`AsyncFile_Native.cpp:40`). So DC3 gives the *interface* (`_OpenAsync`/`_OpenDone`
  poll split) but **not** a non-blocking implementation — R1 must supply the real
  non-blocking behaviour at the `FileLoader` level, because `AsyncFile::Init`'s
  internal spin can't be made cooperative without a deeper rewrite. RB3 routes file
  opens through `NativeStdioFile` (not `AsyncFile`), which is *why* R1 can land the
  fix cleanly in `native_file.cpp` + `Loader.cpp` without touching `AsyncFile`.

---

## Risks & tradeoffs

1. **`File` vtable change.** Adding `virtual bool Pending()` shifts the vtable. `File`
   is engine glue, not a matched-asm target on the native/web build, so this is
   expected to be safe — but **confirm no matched Wii TU's objdiff regresses** after
   the header change (the memory note "BandCharacter::Filter header regression"
   warns header edits can ripple). Mitigation: the new virtual is appended after
   `Truncate`, and the whole pending mechanism is `#ifdef HX_WEB`; if any regression
   appears, switch to a free-function `NativeFileIsPending(File*)` with no vtable change.

2. **Re-enterable `OpenFile` correctness.** `OpenFile()` currently assumes one-shot.
   Making it re-enter on pending must not double-allocate `mBuffer` or re-issue the
   fetch. Guard strictly: re-entry only probes `TryFinishOpen()`; allocation/`ReadAsync`
   happen exactly once, after the fetch resolves. Cover with a unit assert that
   `mFetchId` is issued at most once per loader.

3. **A genuinely missing file (404) must still fail, not spin forever.** Today a sync
   XHR 404 returns false → `mFail` → loader goes to `DoneLoading` with an empty
   buffer (the boot tolerates missing DTA includes — `File.cpp:158` comment). The
   async path must map `onFetchError` → fetch "done" + "not resident" so
   `TryFinishOpen()` sets `mFail` and the loader advances to `DoneLoading`. Verify
   `WebAssetsFetchDone(id)` returns true after `onFetchError` (it does — `onFetchError`
   sets `req->done = true`, `WebAssets.cpp:95`).

4. **DTA lexer path.** Some opens are DTA `#include`s read byte-by-byte by the flex
   lexer through the `File` directly (`File.cpp` NewFile comment, `native_file.cpp`
   header). Those are tiny and almost always boot-bundle-resident; if one ever misses
   on-demand, the lexer would see a not-yet-open file. **Verify** the DTA path goes
   through `PollUntilLoaded` (it does for milo loads) or, if a DTA include can be
   opened *outside* a FileLoader (synchronous direct `NewFile`), keep those on the
   sync fallback — a DTA include is small (sync XHR cost negligible) and must be
   resident before the lexer reads. **Decision:** gate the async path to **large
   binary opens** (milo/mogg/texture) and keep DTA/.dtb on the existing sync path
   (they're boot-bundled anyway, so misses are rare and cheap). Distinguish by
   extension or by `mFlags`/caller, mirroring how the bundle already special-cases
   `.dta/.dtb` (`server.py:285`).

5. **JSPI stack growth.** Each `emscripten_sleep` suspend keeps the C++ stack alive.
   A deep dependency chain (DirLoader → sub-milo FileLoader → …) suspending at the
   open could grow the asyncify/JSPI stack. `-sSTACK_SIZE=4194304` is set
   (`CMakeLists.txt:575`); JSPI uses real stack switching (cheaper than old
   Asyncify). Monitor for stack-overflow asserts (`-sSTACK_OVERFLOW_CHECK=2`,
   `CMakeLists.txt:583`) under the deepest load (part_difficulty→game).

6. **No throughput win, only latency/jank win.** R1 does not download fewer bytes or
   faster — it stops *freezing the frame*. The wall-clock to "interactive" may barely
   move (bytes still take `bytes÷throughput`); what collapses is **main-thread-blocked
   ms** and **worst RAF gap**. Set expectations accordingly in verification (the win
   is the blocked/gap columns, not the wall column). R2/R3 (prefetch/bundle) cut the
   wall-clock.

---

## Verification

Primary tool: `scripts/web/netperf-suite.mjs` (CDP-throttled boot + per-transition
profiler). Run before/after on the **same profiles**:

```bash
scripts/web/build.sh --debug                 # fast iterate; async path compiles under MILO_WEB_ASYNC
python3 native/web/server.py &               # serve
node scripts/web/netperf-suite.mjs --scenario nav --profiles low,normal --runs 3
```

**Pass criteria (the win is in the blocked/gap columns, not wall):**

- `part_difficulty → game` **main-thread blocked** drops from ~11.4 s → low single
  digits at 50 Mbit; **worst RAF gap** from 6.7 s → well under 1 s (target: no single
  freeze > ~200 ms, since each poll tick yields).
- `boot → main_hub` blocked drops from 15.5 s toward the CPU floor.
- Boot **main-thread blocked** collapses toward the loopback `~0.9 s` floor across
  all three throttle tiers (the findings doc's stated goal: "throughput tiers
  collapse toward the loopback numbers").
- Total **bytes / request count unchanged** (same 71 MB / ~98 fetches) — confirms R1
  changed *delivery model*, not *what* is fetched. (Bytes-down is R2/R3/R5.)
- `--trace`/`--cpuprofile` on the `game` transition shows the long blocking
  `WebAssetsFetchSync` task replaced by many short poll tasks interleaved with RAF.

**Negative/regression checks:**

- Cold vs warm IDB: warm boot (returning user) must stay fast (IDB-hit path is
  unchanged/instant). Run a second pass in the same context to confirm the cache
  short-circuit still works.
- A 404'd asset (rename one on the server) must still settle to `DoneLoading` without
  hanging the loader (Risk 3).
- Native build (`cmake --build native/build-native --target rb3-native`) must still
  build and pass — the async path is `__EMSCRIPTEN__`/`HX_WEB`-gated, native is
  untouched (`native_file.cpp` native arm at `:33`/`:270` unchanged).
- `rb3-tests` gtest target (`docs` note `native_hack_audit_testsuite`) builds green —
  the `File::Pending()` header addition must not break the engine test link.
- Matched-asm: spot-check a few `File`-touching TUs via objdiff after the `File.h`
  virtual is added (Risk 1). Expect no movement (File is engine glue, HX_NATIVE).

---

## Effort, impact & dependencies

- **Effort:** **M.** The async primitive (`WebAssetsFetch`/`WebAssetsFetchDone`)
  already exists; the work is (a) a re-enterable `OpenFile` (HX_WEB), (b) a
  pending-aware `NativeStdioFile`, (c) one new `WebAssetsFetchAsync` wrapper, (d) the
  `File::Pending()` hook. No new infra (JSPI/FETCH already on). Bounded, but touches
  the loader spine, so careful verification is the bulk of the cost.
- **Impact:** **critical.** This is *the* root-cause fix for mid-session jank and
  boot freeze named in the findings doc. It directly removes the 6.7 s / 3.7 s /
  3.5 s single-frame freezes. Every other roadmap item (prefetch, bundle, compress)
  reduces *how much* is fetched; R1 fixes *that fetching freezes the tab at all*, and
  is the foundation prefetch (R2/R3) suspends on.
- **Risk:** **medium.** Loader-spine re-entrancy + a `File` vtable addition are the
  two real hazards (Risks 1, 2); both have clean fallbacks. The JSPI yield machinery
  it depends on is already shipping and proven (used by the existing
  `PollUntilLoaded` web arm).
- **Dependencies:** none upstream — R1 is the base. It **unblocks**:
  - **R2 (idle prefetch of next screen)** and **R3 (per-screen async bundle)** —
    both need the non-blocking fetch primitive R1 exposes to overlap downloads with
    interactivity instead of blocking on them.
  - **R4 (progressive mogg streaming)** — depends on R1's async-open model to fetch
    the 37 MB mogg without a blocking open (and then stream/decode it incrementally).
  - **R5 (wire compression of `/api/file` milos)** and **R6 (asset format/BC)** are
    *independent* of R1 (they cut bytes), but compose with it: R1 makes the transfer
    non-blocking, R5/R6 make it smaller. Combined, the 50 Mbit tier approaches the
    loopback numbers.

---

## Open questions

1. **DTA/`.dtb` opens — async or keep sync?** Proposed: keep small boot-path DTA on
   the sync fallback (they're boot-bundled, misses are rare and tiny, and the flex
   lexer reads the `File` directly so a pending DTA would complicate the lexer). Gate
   async to large binary extensions. **Confirm** no large/late DTA exists on a hot
   transition (grep the `nav` waterfall for `.dta`/`.dtb` misses — the findings show
   the offenders are all `.milo_xbox` + `.mogg`, so this is likely safe).

2. **`File::Pending()` virtual vs. free-function downcast.** Default-false virtual is
   cleanest but touches the `File` vtable. Decide after a quick objdiff spot-check
   (Risk 1). If any matched TU moves, use `NativeFileIsPending(File*)` (HX_WEB free
   function, no vtable change).

3. **Budget-loop yield on pending front loader.** Should `LoadMgr::Poll`'s budget loop
   `emscripten_sleep(0); break;` *immediately* when the front loader is web-pending
   (Phase 4), or rely on the existing `sinceYield` gate? Immediate-yield is more
   responsive but adds a web-only branch in a hot loop. Measure both with
   `netperf-suite` (`--profiles low`) and pick the lower worst-RAF-gap.

4. **Multiple in-flight fetches.** R1 keeps one async open at a time (the loader spine
   serialises). Is there a transition where two independent front loaders could each
   want a different large file concurrently (e.g. DirLoader spawning sibling
   FileLoaders)? If so, R1 still works (each suspends independently) but won't overlap
   their downloads — that overlap is R2/R3. Confirm the loader processes strictly
   front-to-back (it does: `mLoading.front()` only, `Loader.cpp:493`) so there's no
   correctness issue, only a missed-overlap opportunity deferred to R2/R3.

5. **JSPI stack depth under the deepest chain.** Does suspending at the open inside a
   deep DirLoader→FileLoader chain ever approach `-sSTACK_SIZE`? Instrument the
   part_difficulty→game path with `-sSTACK_OVERFLOW_CHECK=2` under the `low` profile
   and watch for asserts (Risk 5).
