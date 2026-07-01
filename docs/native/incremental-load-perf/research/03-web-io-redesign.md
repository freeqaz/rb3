# 03 — Web I/O redesign: eliminating main-thread blocking network I/O

**Date:** 2026-06-10 · **Status:** research / design (read-only investigation, no code changed)
**Scope:** replace the MEMFS-miss → synchronous-XHR path so the game frame loop keeps
running while asset bytes arrive over the network.

---

## 1. The single chokepoint (verified)

Every incremental-load stall funnels through **one line of code**: the
`NativeStdioFile` constructor in `native/src/native_file.cpp:258` —
`WebAssetsFetchSync(fetchPath)` → engine `src/platform/WebAssets.cpp:340`
`xhr.open("GET", url, false)` — a **blocking synchronous XHR on the main thread**.

All three load paths converge on it via `NewFile()` (`src/system/os/File.cpp:168`
→ `HmxNativeOpenFile`):

| Path | Open site | Read mechanism after open |
|---|---|---|
| Milo scenes (`.milo_xbox`) | `DirLoader::OpenFile` (`src/system/obj/DirLoader.cpp:392`) → `new ChunkStream` → `NewFile` | ChunkStream buffered reads, **already cooperative** (`TempEof` polling in every consumer loop) |
| Raw file loads (moggs via `MoggClip`, textures, dta) | `FileLoader::OpenFile` (`src/system/utl/Loader.cpp:652`) → `NewFile` | `ReadAsync`/`ReadDone` **state machine, already async-shaped** (`LoadFile` state polls `ReadDone`) |
| Song preview / gameplay audio streams | `Synth::NewStreamFile` (`src/system/synth/Synth.cpp:569`) → `NewFile` | `VorbisReader` polls `ReadAsync(16 KB)`/`ReadDone` per chunk (`VorbisReader.cpp:208-216,329-345`), seeks via the mogg seek map — **the Wii async-DVD contract, intact** |

**Key structural fact:** the entire Milo loader stack was designed for async I/O
(Wii DVD `ReadAsync` + `ReadDone(false)` = "not yet"). The *reads* are all
poll-friendly today. Only the **open** blocks, because `NativeStdioFile`'s ctor must
hand back a working `FILE*` synchronously, and a MEMFS miss means the bytes aren't
local yet.

### What the open-blocking costs per stall

- **Background (`kLoadBack`) loads** — `LoadMgr::Poll()` is frame-budgeted
  (`RB3_LOADER_BUDGET_MS=8`), but the budget can't split a sync XHR: a single
  `OpenFile` for a 2 MB milo blocks the frame for the whole network round trip.
  → the "general frame drops whenever content loads".
- **Sync-contract drains** (`PollUntilLoaded` / `PollUntilEmpty` /
  `ForceGetLoader`, 17 call sites) — the canvas freezes for
  (network time + CPU), tab kept alive only by JSPI `emscripten_sleep(0)` yields.
  → title→main_hub 5-10 s, cinematic→title 2-5 s.
- **Song preview hover** — `SongPreview::PrepareSong` → `TheSynth->NewStream`
  → `NewFile("songs/<x>/<x>.mogg")`. On-disc songs take the `unk71=true` branch
  (no `_prev` sidecar exists in the extracted assets — verified, zero `*_prev*`
  files), so the miss fetches the **full song mogg: 31–36 MB**
  (`orig-assets/extracted-xbox-full/songs/*/*.mogg`, measured). → 2-5 s freeze.

### The mogg fetch is even worse than "network time"

The sync-XHR fallback (`WebAssets.cpp:341-352`) can't use
`responseType="arraybuffer"` (illegal for sync XHR), so it pulls the body as a
**binary JS string** and converts with a per-byte `charCodeAt` loop — for a 36 MB
mogg that's a ~72 MB JS string plus 36 M loop iterations of pure CPU **after** the
download finishes. Then:

1. 36 MB written to MEMFS (retained for the session),
2. `cachePutAfterFetch` copies 36 MB into the IDB write queue (`rb3_pre.js
   __rb3CachePut` takes another full copy),
3. **next boot's IDB pre-warm (`loadAllRows`, `rb3_pre.js:185-206`) loads every
   cached row into a JS `Map` in the renderer heap — uncapped.** Hover ten songs
   today and tomorrow's boot pre-warms ~360 MB into JS memory before wasm even
   starts. This is a latent OOM (the same class of bug as the documented
   "OOM FIX" for runtime puts at `rb3_pre.js:227`), and it should be fixed
   regardless of which option below ships (size-cap `cachePutAfterFetch` /
   exclude `.mogg`, or lazy-load large rows on demand).

### What's cached where today (answering the "second hover" question)

- **Same session, same song:** MEMFS-resident → first `fopen` succeeds
  (`gBootIoFirstTry`); **fast**. (`MoggClip::UnloadData` frees the decode buffer
  but the MEMFS file persists.)
- **Next session:** IDB pre-warm map hit (`cacheTryHit`) → MEMFS write + fopen,
  no network; fast-ish, but see the pre-warm OOM above.
- So the pain is **first touch of each asset per asset-version**, plus the
  per-touch CPU overhead of the binary-string sync XHR.

Existing async machinery already in the engine (building blocks, currently used
only at boot): `WebAssetsFetch(serverPath)` — `emscripten_fetch` async, writes
MEMFS in `onFetchSuccess`, returns an id pollable via `WebAssetsFetchDone(id)`
(`WebAssets.cpp:118-150`). `-sFETCH=1` is already linked (engine CMakeLists
`milo_engine_apply_web_target_options`).

Server (`native/web/server.py`) already provides everything the options below
need: `/api/manifest` (every asset path **with size**, lines 414-435),
**HTTP Range / 206** on `/api/file` (lines 736-790, `Accept-Ranges: bytes`),
per-screen-bundle-capable `_emit_bundle` (shared by `/api/bundle` and
`/api/bundle/boot`), wire compression (br/gzip, Range requests bypass it),
COOP/COEP (lines 106-110).

---

## 2. Options

### Option 1 — Async open: a "pending File" honoring the Wii ReadAsync contract  ⭐ recommended core

**Mechanism.** Stop blocking in the open. On MEMFS miss, `HmxNativeOpenFile`
returns a `WebPendingFile : File` that:

- kicks `WebAssetsFetch()` (the *existing* async fetch → MEMFS write path),
  deduping in-flight fetches by path;
- answers `Size()` / `Fail()` **synchronously from a boot-loaded manifest map**
  (`/api/manifest` already returns `{path,size}` for every asset; load it into a
  JS `Map` in `rb3_pre.js` alongside the IDB pre-warm — one ~1 MB JSON, and it
  doubles as a synchronous *existence oracle* so a genuine 404 reports
  `Fail()=true` immediately, with zero network);
- `ReadAsync(buf,n)` records the request; `ReadDone(result)` returns **false**
  until `WebAssetsFetchDone` fires and the MEMFS write lands, then opens the
  real `FILE*`, performs the read, returns true. Subsequent calls delegate to
  the inner `NativeStdioFile`;
- synchronous `Read()` on a still-pending file (rare: DTA lexer paths, which are
  bundle-prefetched anyway) falls back to a JSPI-suspended wait
  (`emscripten_sleep(4)` loop) — no worse than today, and it can use async
  `fetch()` + arraybuffer instead of the binary-string sync XHR.

**Why it slots in cleanly (verified against the code):**
- `FileLoader::OpenFile` does `mBufLen = mFile->Size(); ReadAsync(...); mState =
  LoadFile` — `LoadFile` then polls `ReadDone` across frames. Failure-after-open
  is already handled (`LoadFile` frees the buffer on `mFile->Fail()`).
- `ChunkStream` (DirLoader/milo path) reads through the same `ReadAsync` contract
  and surfaces `TempEof`, which every consumer loop already spins on
  cooperatively (`FileLoader::LoadStream:734-757`, DirLoader states).
- `VorbisReader` polls `ReadDone` per 16 KB chunk and checks `Fail()` after every
  poll — designed for exactly this.

**What changes where:** `native/src/native_file.cpp` (new `WebPendingFile`,
~150 lines, web-only `#ifdef`); `rb3_pre.js` (manifest pre-warm map +
`window.__rb3AssetSize(path)`); small additions to `WebAssets.cpp/h` (in-flight
dedupe table, `WebAssetsEnsureResidentAsync(path)` helper, optional: write-through
to IDB on async fetch success — the hook `bundleCacheWriteThrough` already
exists). **No changes to matched Wii code** (`Loader.cpp` / `DirLoader.cpp` /
`Synth.cpp` untouched).

**Latency model.** Background loads: frame loop **keeps running**; the mLoading
queue naturally amortizes across frames; network overlaps render. Sync-drain
sites (`PollUntilLoaded`): canvas still frozen for the *duration*, but the
duration improves two ways — (a) parallelism: when a drain pulls a DirLoader
chain, all `OpenFile`s in the queue can now have fetches in flight
*concurrently* (today each milo in a chain is fetched serially, one blocking XHR
at a time — a chain of 20 milos pays 20 × RTT serially); (b) most of those
drains are for things that an earlier `kLoadBack` pass now had time to finish.

**JSPI interaction:** none required on the hot path — the fetch completion is an
ordinary event-loop callback; the loader just sees `ReadDone()==false` and
returns to the frame loop. JSPI remains only in `PollUntilLoaded`'s existing
yield throttle and the rare sync-`Read()` fallback.

**Risk:** medium-low. The contract is the Wii contract; failure paths exist.
Main hazards: (i) a caller that opens and *synchronously* `Read()`s a missed
file (fallback covers it; `RB3_BOOT_IO_STATS` counters can find them); (ii)
manifest staleness vs server assets (version-bust with `/api/version`, fall back
to a HEAD request on map miss); (iii) `MemDoTempAllocations` scope around
`NewFile` in `FileLoader::OpenFile` — the pending object allocates little, fine.

**Effort:** ~2-3 days incl. tests.

### Option 1b — Range-backed streaming File for `.mogg` (pairs with 1)  ⭐ recommended for audio

36 MB moggs should never be fully fetched to start a 15 s preview. Add a
`WebRangeFile : File` selected by extension (`.mogg`) on MEMFS miss:

- `Size()` from the manifest map; `Seek/Tell` tracked locally;
- `ReadAsync(buf,n)` issues `fetch(url, {headers:{Range: bytes=pos..}})` for a
  chunk (e.g. 256 KB granularity, cached in a sparse MEMFS-backed or in-heap
  chunk table, with one-chunk readahead); `ReadDone` false until the chunk lands;
- server side already done: `/api/file` serves 206 with `Accept-Ranges` and
  correctly bypasses wire-compression for Range requests.

`VorbisReader`'s access pattern fits perfectly: 60 KB header read
(`VorbisReader.cpp:103`), `DoSeek` → seek-map → `DoRawSeek(byteOffset)`, then
sequential 16 KB chunked reads. A preview touches header + seek table + ~15 s of
pages (~2-4 MB) instead of 36 MB; **gameplay song start** stops paying a 36 MB
stall too (progressive download with readahead during play; underrun shows as
`TempEof`-style wait, mitigate with a larger readahead window once playing).
This also kills the 36 MB MEMFS+IDB retention per hovered song.

Risk: medium (chunk cache correctness, encrypted-CTR seek already proven
chunk-friendly per the wave-05/08 audio work). Effort ~2-3 days. Note
`MoggClip::LoadFile` (FileLoader full-buffer path, used for menu music loops)
still wants the whole file — fine, it goes through Option 1's pending path.

### Option 2 — Web Worker fetcher + SAB mailbox

Main thread posts a path; a plain JS worker fetches and either posts back bytes
or writes into a SAB; main thread polls completion. **Adds nothing over
`emscripten_fetch`-async (Option 1):** without wasm pthreads the main thread
cannot `Atomics.wait` (throws on the main thread), so completion is still an
event-loop poll — exactly what `WebAssetsFetchDone` already is. A worker only
helps if fetch *initiation* were CPU-heavy (it isn't) or to escape main-thread
network throttling (not observed). **Skip** — keep as fallback if Chrome ever
throttles main-thread `fetch` in background tabs (telemetry would show it).

### Option 3 — Predictive prefetch (manifest/screen bundles + hover prefetch)  ⭐ recommended latency-hider

Mechanism: generalize the proven `/api/bundle/boot` trick.
- **Per-screen bundles:** `server.py` `_emit_bundle` is already shared; add
  `/api/bundle/<name>` reading `native/web/<name>-assets.manifest` (derive each
  list with the existing waterfall tool `scripts/web/gen-boot-manifest.mjs`
  pointed at a title→main_hub / →song_select trace). Trigger from
  `native/src/main_web.cpp` (or a small native shim hooked on screen
  transitions) via `WebAssetsFetchBundle("/api/bundle/main_hub")` — fired
  *async* the moment the previous screen is up; bytes land in MEMFS (+IDB
  write-through, already implemented in `onBundleSuccess`) before the sync
  drain for the next screen runs. Turns 5-10 s freezes into ~0 on warm-path
  navigation **without touching the loader at all**.
- **Hover prefetch:** on song_select highlight change, async-prefetch the
  hovered (and ±1 adjacent) song's mogg Range-window/milo while the existing
  1 s preview debounce (`SongPreview::PreparePreview`,
  `mPreviewRequestedMs < 1.0f`) runs. With 1b, "prefetch" = warm the first
  ~3 MB of Range chunks.

Latency model: frame loop unaffected (async bundle fetch); hides first-touch
latency rather than removing the blocking primitive — so it **complements, not
replaces** Option 1. Risk: low (purely additive; mis-predicted prefetch wastes
bandwidth only). Effort: ~1-2 days server+trigger.

### Option 4 — pthreads I/O thread (`-pthread`)

Feasibility as of the 2026 toolchain, against this link line (engine
`CMakeLists` `milo_engine_apply_web_target_options`): would require recompiling
**everything** (incl. `USE_OGG/USE_VORBIS/USE_ZLIB` ports and emdawnwebgpu) with
atomics+bulk-memory and shared memory. Conflicts/costs:
- `-sALLOW_MEMORY_GROWTH=1` + threads = growable `SharedArrayBuffer`; works in
  Chrome (and COOP/COEP is already served) but forces JS-side view refresh
  overhead on every growth and bumps every JS↔heap access path;
- **emdawnwebgpu**: WebGPU objects are not usable across threads in the wasm
  port; all GPU work must stay on the creating thread — fine for an I/O-only
  thread but it means the thread buys *only* I/O;
- **JSPI × pthreads**: supported but per-thread and still maturing; our JSPI
  exports (`_main`, tick) stay main-thread — interaction is untested surface;
- an I/O pthread *can* do sync XHR legally off-main-thread, but the engine
  consumes files on the main thread anyway, so completion is a cross-thread
  queue — the same poll shape Option 1 gets for free without any of this.

**Verdict: skip.** All cost, no unique benefit for I/O. (Threads may be worth
revisiting later for decode/decompress CPU, not for network.)

### Option 5 — HTTP Range / streaming for moggs

Covered as **Option 1b** above (the server side already exists). The
full-buffer `FileLoader` contract (`MoggClip`) is left alone; only the
`StandardStream`/`VorbisReader` path streams.

### Cheap orthogonal fix — de-uglify the remaining sync fallback

Wherever a synchronous fetch survives (Option 1's sync-`Read()` fallback), use a
JSPI-suspended async `fetch()` returning an `arraybuffer` instead of the
`overrideMimeType` binary-string XHR: kills the 2× string blow-up + per-byte
convert loop, keeps the tab compositing. ~30 lines in `WebAssets.cpp`. (Also
applies to `rb3_xma_sidecar.h`'s `WebAssetsFetchSync` use — PCM sidecars are
small, but the same helper upgrade covers it.)

---

## 3. Recommendation & sequencing

1. **Option 1 (pending File + manifest size/existence map)** — removes the
   blocking primitive at the single chokepoint; loader queue amortizes
   naturally; no matched-code edits.
2. **Option 1b (Range File for `.mogg`)** — fixes the worst single stall (song
   hover, 31-36 MB) and the MEMFS/IDB memory blow-up.
3. **Option 3 (screen bundles + hover prefetch)** — hides first-touch latency on
   the known navigation paths; trivially safe.
4. Fix the **IDB pre-warm uncapped-row** hazard (cap or lazy rows; exclude
   `.mogg`) in the same wave.
5. Skip Options 2 and 4.

## 4. Fastest de-risking experiment

**Prove the pending-File contract against the real loader in one afternoon,
without the network:** add a dev-only `RB3_FAKE_ASYNC_OPEN_MS=<n>` mode to
`native_file.cpp` that wraps every read-open in a stub File whose
`ReadDone()` returns false until `n` ms have elapsed (timer, no fetch), then
delegates to the real `NativeStdioFile`. Run the **native** build (3 s rebuild,
headless HTTP API) through boot → main_hub → song_select → preview →
gameplay-start with `n`=50/200/1000. If nothing deadlocks/asserts
(watch: `MILO_ASSERT(t == TempEof)` in `FileLoader::LoadStream:735`,
DirLoader `Cleanup` paths, `VorbisReader` `mFail` checks, the 17
`PollUntilLoaded` sites, and `ForceGetLoader`'s loaded-expectation), the entire
design is validated — the remaining web work is plumbing `WebAssetsFetch` +
manifest into that proven seam. This tests the risky 90 % (loader-state-machine
tolerance of multi-frame opens) with the fast 10 % of the effort.

---

### Appendix — file/line index of every fact used

- Sync XHR: `milo-native-engine/src/platform/WebAssets.cpp:309-377` (binary-string convert :348-353)
- Async fetch building block: `WebAssets.cpp:118-150`; bundle unpack + IDB write-through :200-303
- Miss→fetch flow + IDB shim + counters: `rb3/native/src/native_file.cpp:196-289` (sync XHR call :258)
- `NewFile` routing: `rb3/src/system/os/File.cpp:151-168`
- FileLoader state machine: `rb3/src/system/utl/Loader.cpp:641-730`; LoadStream TempEof loop :732-765
- PollUntilLoaded web arm (JSPI yield throttle): `Loader.cpp:176-280`
- DirLoader open via ChunkStream: `rb3/src/system/obj/DirLoader.cpp:357-412`
- Preview chain: `rb3/src/system/meta/SongPreview.cpp:140-228` (Poll states), `:283-301` (PrepareSong → NewStream; `_prev` branch only when `!unk71`)
- Stream open: `rb3/src/system/synth/Synth.cpp:536-595`
- VorbisReader chunked async reads: `rb3/src/system/synth/VorbisReader.cpp:103,208-216,329-345,596-602`
- IDB pre-warm loads ALL rows: `rb3/native/web/rb3_pre.js:185-206`; OOM-fix comment :227-239
- Server: manifest w/ sizes `native/web/server.py:414-435`; Range/206 :736-790; bundle emitter :449-; COOP/COEP :106-110
- Link flags / JSPI: `milo-native-engine/CMakeLists.txt:145-152, 557-606`
- Mogg sizes: `orig-assets/extracted-xbox-full/songs/*/*.mogg` = 31-36 MB (measured); no `*_prev*` files exist
- XMA sidecar sync fetch: `rb3/native/src/rb3_xma_sidecar.h` (web branch)
