# R4 — Streaming song audio: kill the 37 MB mogg blocking fetch

> Roadmap item R4. Companion data: `docs/native/web-netperf-findings-2026-06-08.md`.
> Verification tool: `scripts/web/netperf-suite.mjs`.

## Problem & data

Entering gameplay fetches the song's mogg as a **single synchronous XHR** that freezes
the wasm main thread for the whole transfer. From the measured `nav` waterfall
(`docs/native/web-netperf-findings-2026-06-08.md`):

- `part_difficulty → game` at 50 Mbit/s: **21.2 s wall, 11.4 s blocked, 6.7 s worst single freeze**.
- The dominant offender is `songs/20thcenturyboy/20thcenturyboy.mogg` —
  **37.4 MB → 6.76 s frozen** (verified on disk: `orig-assets/extracted-xbox-full/songs/20thcenturyboy/20thcenturyboy.mogg` = 37 350 466 bytes).
- At 200 Mbit/s the same fetch is still **~3.6 s blocked / 2.3 s dead frame**.

The mogg is the single worst hitch in the entire session, and it is Ogg/Vorbis
(already compressed) so wire-compression (R6) buys ~nothing — only **async delivery**
or **progressive streaming** helps.

Two facts make this fixable cheaply:

1. **The decode path is already chunked.** `VorbisReader::DoFileRead`
   (`src/system/synth/VorbisReader.cpp:202`, HX_NATIVE branch) reads the mogg in
   **0x4000 (16 KB)** increments via `mFile->ReadAsync`, caps queued input at 0x10000,
   decrypts (AES-CTR) per chunk, and feeds ogg. Nothing requires the whole file up
   front *for decode*. The only thing that forces full-file residency is **the open**.
2. **The open is where the 37 MB lands.** `Synth::NewStreamFile`
   (`src/system/synth/Synth.cpp:565`) does `NewFile("<song>.mogg", 2)` →
   `HmxNativeOpenFile` → `NativeStdioFile` ctor → on a MEMFS `fopen` miss it calls
   `WebAssetsFetchSync(fetchPath)` (`native/src/native_file.cpp:163`), which downloads
   the **entire** file synchronously into MEMFS before any read happens.

So the freeze is entirely an **open-time, whole-file, synchronous** artifact bolted in
front of an already-streaming reader.

## Architecture

Two complementary layers. **Ship (A) first — it is the higher-value, lower-risk slice**
(kills the single worst 6.76 s freeze, no audio-correctness risk; Wave 2). Keep (B) — the
range-streamed open — as a **Wave 3 latency/footprint polish** for the cold no-dwell /
very-slow-link case; it carries genuine audio-correctness risk and only bounds a tail case.

### Layer A — Async prefetch on song selection (primary, low risk)

Make the 37 MB arrive **off the critical path**, overlapped with the time the player
spends on `song_select` / `part_difficulty`, so that by the time `Game::LoadSong`
(`src/band3/game/Game.cpp:242`) opens the mogg the bytes are **already resident in
MEMFS** and `WebAssetsFetchSync` is a no-op (the `fopen` hits).

Data/control flow:

- A new **async prefetch driver** (web-only) owns a small queue of "warm this asset"
  requests and pumps the engine's existing async fetch (`WebAssetsFetch` /
  `onFetchSuccess`, `milo-native-engine/src/platform/WebAssets.cpp:118`), which writes
  the file into MEMFS at `/data/<serverPath>` — the path `NativeStdioFile::fopen` will
  later look at. **Caveat (verified):** `onFetchSuccess`
  (`WebAssets.cpp:60`) writes `req->memfsPath = "/data/" + serverPath` **verbatim** and
  does **not** resolve `..` components — only `onBundleSuccess` (`:192`) and the sync
  `WebAssetsFetchSync` (`:261`) strip/resolve `..`. Song-mogg paths have no `..`, so the
  mogg hook is safe, but the **generic driver shared with R2** must normalize `..` itself
  (see "Shared driver" under Dependencies) or it writes `..`-bearing milo paths to the
  wrong MEMFS location and the later fopen misses.
- The **hook fires when the song is highlighted**, not when the preview opens.
  `SetHighlightIx` (`src/band3/meta_band/MusicLibrary.cpp:535`) runs the instant the
  cursor lands on a song node and calls `StartSongPreview` (`:483`), which starts
  `mSongPreviewTimer`. `CheckSongPreview` (`:490`) only *fires the preview* once that
  timer exceeds `mSongPreviewDelay` — so hooking `CheckSongPreview` would start the
  prefetch **at** the delay boundary (zero pre-overlap, defeating the point). We hook
  **`SetHighlightIx`** (or equivalently `StartSongPreview`) so the download begins the
  moment the cursor rests on the song, overlapping the full `mSongPreviewDelay` + dwell.
- The prefetch path string mirrors `NewStreamFile`: `<SongInfo::GetBaseFileName()>.mogg`,
  resolved from the highlighted node's token via `TheSongMgr.SongAudioData(token)`
  (`BandSongMgr::SongAudioData(Symbol)`, `src/band3/meta_band/BandSongMgr.h:94`) — the
  same `BandSongMgr&` `MusicLibrary` already uses in `CheckSongPreview` (`:511`). This
  makes the prefetch key === the eventual `NewStreamFile`/`MasterAudio::Load`
  (`src/system/beatmatch/MasterAudio.cpp:129`) key.
- The driver is pumped once per frame from the boot/run loop
  (`native/src/main_web.cpp` `BOOT_RUNNING` → `RunOneFrame`, `~:553`), so completions are
  reaped without blocking. No JSPI needed — `emscripten_fetch` is genuinely async.

**Sync-vs-async coordination (required, not optional).** The async prefetch and the
eventual synchronous open (`Game::LoadSong` → `MasterAudio::Load` → `NewStreamFile` →
`WebAssetsFetchSync`) target the *same* MEMFS path but through *different* code. A song
picked **faster than the prefetch completes** must NOT issue a second blocking 37 MB XHR
on top of the in-flight async fetch. Two cooperating mechanisms:
1. The driver tracks an **in-flight set** keyed by `serverPath`. Before any fetch
   (sync or async) the open path consults it.
2. On the sync miss (`NativeStdioFile::fopen` miss, `native_file.cpp:163`), if the same
   path is **already in flight async**, the open must **wait on the existing fetch**
   (drive `RB3PrefetchPoll` under a bounded JSPI yield / spin on `WebAssetsFetchDone(id)`)
   and then `fopen` the now-resident file — never start a parallel `WebAssetsFetchSync`.
   Absent this, a quick pick double-downloads 37 MB and reintroduces the freeze.

Net effect: N blocking opens become 1 overlapped background download that finishes
during dwell time, with the cold/quick-pick case degrading to *waiting on the existing
async fetch* rather than a fresh sync XHR.

**Preview interaction is conditional, not guaranteed.** Whether prefetching the *full*
mogg also removes the preview's own fetch depends on `SongPreview`'s branch
(`SongPreview::PrepareSong`, `src/system/meta/SongPreview.cpp:283`):
- When `unk71 == true` the preview opens the **full** mogg with a `mStartMs` seek
  (`:301`) — the same file the prefetch warms, so prefetch removes that hitch too. This
  is the path taken for **extracted on-disc songs** where `ContentName` returns null and
  `Poll` sets `unk71 = true` (`SongPreview.cpp:184`).
- When `unk71 == false` the preview instead opens a **separate `<base>_prev` stream**
  (`:296`–`:297`) — a *different* file the full-mogg prefetch does **not** warm. For
  **DLC** songs there is an additional `dlc/`-prefix path rewrite (`:290`–`:295`).
So the "no separate `_prev` file; preview seeks the full mogg" simplification is **true
for the on-disc songs the port currently ships, conditional in general, and diverges for
DLC**. The prefetch is still correct for gameplay regardless (gameplay always opens the
full `<base>.mogg` via `MasterAudio::Load`); only the *preview-hitch bonus* is
conditional. Do not gate the prefetch on the preview path.

This reuses the **IDB warm cache**: `cachePutAfterFetch`
(`native/src/native_file.cpp:100`) only runs on the sync path today; we add the same
"put after async success" so a prefetched mogg is written through to IndexedDB and
**warm on the next visit** (`window.__rb3CachePut`, `native/web/rb3_pre.js:225`).
**Cost to account for:** `cachePutAfterFetch` does a whole-file `FS.readFile` + JS copy +
IDB `put`; on a 37 MB mogg that is itself a *new* (smaller) main-thread stall plus a
transient ~2× heap spike. Defer/chunk it (e.g. run the write-through off the hot frame,
or skip it when memory-pressed) so the item doesn't reintroduce a smaller version of the
freeze it exists to remove.

### Layer B — Progressive range-streamed open (upgrade, medium risk)

Prefetch still transfers the whole 37 MB before the *last* few seconds of the song are
guaranteed present; for a song picked with no dwell time, or a very slow link, play
should start after only the **header + first seconds** land. server.py already serves
HTTP **206 range** on `/api/file` (`native/web/server.py:399 _serve_range`,
`Accept-Ranges: bytes`), so we can back the mogg `File` with on-demand range reads
instead of one whole-file fetch.

Shape: a new web-only `File` subclass (`WebRangeFile`, native/src) that, instead of
fetching the whole mogg at open, fetches only **range [0, firstChunk)** synchronously
(tiny — covers the HMX header `mHdrSize` + first ogg pages), reports the real `Size()`
from the 206 `Content-Range`/`Content-Length`, and serves subsequent
`Read`/`ReadAsync(0x4000)` calls against a sparse local buffer, issuing a **range XHR for
the missing window** when the data isn't yet resident.

**The async contract is the crux, and the obvious approach is wrong.** The native `File`
interface is **synchronous-fronted**: `NativeStdioFile::ReadAsync` calls `Read` inline
(`native_file.cpp:201`–`204`) and `ReadDone` returns `true` immediately (`:251`). The
consumer of this is `VorbisReader::DoFileRead` (`VorbisReader.cpp:202`) — the audio
**producer** — and it runs on **whatever thread pumps `VorbisReader::Poll`**, which on
the single-threaded web build is the **main thread** (the same thread the mixer and JSPI
suspension live on). So a naive `emscripten_sleep(0)` *inside* `WebRangeFile::Read`
suspends the producer mid-refill: the decode pipeline stops feeding the ring while the
range XHR is in flight, the ring drains, and the mixer **underruns → audible dropout**.
The starvation is **producer-side on the main thread, not an output-thread problem.**

The correct shape is an **async-completing `ReadDone`**, not a blocking read:
- `ReadAsync(buf, 0x4000)` kicks the range XHR for the missing window and returns
  immediately (data not yet present).
- `ReadDone(result)` returns **`false` (pending)** until the range XHR has landed, then
  `true` with the byte count — exactly the pending contract `DoFileRead` already tolerates
  (it only consumes the buffer `if (... mFile->ReadDone(bytes) ...)`, otherwise loops).
- The frame loop keeps running between `Poll` calls, so the XHR completes off the hot
  path and the next `Poll`'s `DoFileRead` finds the data ready — no in-read suspension.
This keeps the suspension out of the producer entirely. (`emscripten_sleep(0)`, the JSPI
yield used in `LoadMgr::PollUntilLoaded` (`src/system/utl/Loader.cpp:219`), is fine for a
*loader* that nobody is feeding audio from, but must **not** be put inside the mogg read
path.) VorbisReader's existing 16 KB cadence and 0x10000 queue cap become the streaming
granularity unchanged.

`Synth::NewStreamFile` selects `WebRangeFile` for the song mogg (a flag/threshold:
files over ~4 MB, or specifically the `.mogg` ext) and falls back to the normal
`NativeStdioFile`/MEMFS path otherwise. Layer A's prefetch and Layer B compose: prefetch
warms MEMFS so most range reads hit locally; range-streaming bounds the **worst case**
(cold open, no dwell) to header-latency instead of whole-file-latency.

## Implementation plan

Phased; each phase independently shippable and measurable.

### Phase A1 — Generic async prefetch driver (web-only, shared with R2)
- Add `native/src/rb3_asset_prefetch.{h,cpp}` (HX_NATIVE/`__EMSCRIPTEN__`):
  - `void RB3PrefetchAsset(const char *serverRelPath)` — dedup against an **in-flight set**
    (keyed by `serverPath`) + completed set + a MEMFS-exists probe (`stat`), then call the
    engine `WebAssetsFetch(serverRelPath)` (declare in `platform/WebAssets.h`), recording
    the returned fetch id.
  - **Normalize `..` before fetching.** `WebAssetsFetch`→`onFetchSuccess`
    (`WebAssets.cpp:60`) writes `/data/<serverPath>` *verbatim* and does **not** resolve
    `..` (only `onBundleSuccess`/`WebAssetsFetchSync` do). Song moggs have no `..`, but the
    generic driver R2 reuses for milos **must** collapse `..`/`.` components itself (mirror
    the resolver in `onBundleSuccess`, `WebAssets.cpp:192`–`221`) so the MEMFS write lands
    where the later `fopen` looks. Put the normalization in `RB3PrefetchAsset` so every
    caller is covered.
  - `bool RB3PrefetchInFlight(const char *serverRelPath)` + `void RB3PrefetchWait(const
    char *serverRelPath)` — the **sync/async coordination** surface (below): query the
    in-flight set, and block-until-done on an already-in-flight fetch by driving
    `RB3PrefetchPoll` / `WebAssetsFetchDone(id)` under a bounded yield (no second XHR).
  - `void RB3PrefetchPoll()` — reap completions (`WebAssetsFetchDone(id)`), run the IDB
    write-through (Phase A3), move ids from in-flight → completed.
  - Track **its own** fetch-id set; do **not** key completion on the process-global boot
    gate (`sPending`/`WebAssetsAllDone`). Keep a tiny bounded queue (1–2 concurrent
    fetches) so a giant mogg doesn't starve the smaller per-screen milos.
- Pump `RB3PrefetchPoll()` once per frame from `native/src/main_web.cpp` `BOOT_RUNNING`
  (right around `sApp->RunOneFrame`, `~:553`).

### Phase A2 — Hook prefetch on song highlight + wire the sync/async coordination
- In `MusicLibrary::SetHighlightIx` (`src/band3/meta_band/MusicLibrary.cpp:535`) — the
  hook that fires the instant the cursor lands on a song (it calls `StartSongPreview`,
  `:483`), **not** `CheckSongPreview` (which fires only *at* the `mSongPreviewDelay`
  boundary, giving zero pre-overlap). When the highlighted `node->GetType() == kNodeSong`,
  resolve `SongInfo *info = TheSongMgr.SongAudioData(node->GetToken())`
  (`BandSongMgr::SongAudioData(Symbol)`, `BandSongMgr.h:94`) and call
  `RB3PrefetchAsset(MakeString("%s.mogg", info->GetBaseFileName()))` — guarded
  `#ifdef HX_NATIVE` + behind an env/URL opt-out (e.g. `RB3_NO_MOGG_PREFETCH`). This makes
  the prefetch key === the eventual `MasterAudio::Load`/`NewStreamFile` key (`<base>.mogg`).
- **Coordinate the eventual sync open** so a quick pick can't double-fetch: in
  `NativeStdioFile`'s `fopen`-miss path (`native_file.cpp:163`), *before* calling
  `WebAssetsFetchSync`, check `RB3PrefetchInFlight(fetchPath)`; if true, call
  `RB3PrefetchWait(fetchPath)` (drains the existing async fetch under a bounded yield) and
  retry `fopen` instead of starting a parallel sync XHR.
- Optional: also prefetch from the `part_difficulty` enter (the screen *after* select) as
  a backstop if highlight-prefetch is opted out.

### Phase A3 — IDB write-through for async fetches
- Factor the `cachePutAfterFetch` body (`native/src/native_file.cpp:100`) so the
  prefetch driver can call it after an async success, keeping prefetched moggs warm for
  return visits. **Mind the cost** (whole-file `FS.readFile` + JS copy + IDB put on 37 MB
  = a new smaller main-thread stall + ~2× transient heap): run the write-through off the
  hot frame (e.g. one per `RB3PrefetchPoll`, low priority) and allow skipping it under
  memory pressure, so A3 never reintroduces a freeze.

### Phase B1 — Range-streamed mogg File (web-only)
- Add `native/src/rb3_web_range_file.{h,cpp}`: a `File` subclass that
  - opens by range-fetching `[0, kFirstWindow)` (e.g. 256 KB ≥ `kMaxHeader` 60000 +
    first ogg pages), parses `Content-Range` total → `mSize`;
  - implements `Read`/`ReadAsync`/`Seek`/`Tell`/`Size`/`Eof`/`ReadDone` against a
    sparse local buffer, issuing range XHRs (`Range: bytes=a-b`) for missing windows;
  - uses an **async-completing `ReadDone`**: `ReadAsync` kicks the range XHR and returns;
    `ReadDone(result)` returns **`false` until the window has landed**, then `true` with
    the byte count. The frame loop drives completion between `VorbisReader::Poll` calls.
    Do **NOT** `emscripten_sleep(0)` inside `Read`/`ReadAsync` — that suspends the audio
    producer on the main thread mid-refill and underruns the ring (see Architecture/Layer
    B and Risks). `DoFileRead` already tolerates a pending `ReadDone` (it only consumes
    the buffer when `ReadDone` is true), so this needs no VorbisReader change.
- Decryption is unaffected: `VorbisReader::DoFileRead` decrypts the chunk it just read,
  and AES-CTR is seekable (the reader already re-keys the CTR counter on seek,
  `VorbisReader::DoRawSeek`, `src/system/synth/VorbisReader.cpp:612`) — so range reads
  at arbitrary offsets decode correctly as long as ranges are 16-aligned for the
  seek case (already asserted at `:633`).

### Phase B2 — Route the song mogg through the range File
- In `Synth::NewStreamFile` (`src/system/synth/Synth.cpp:565`), under
  `#ifdef HX_NATIVE` + `__EMSCRIPTEN__`, construct the `WebRangeFile` for the mogg
  (gate on ext == `mogg` and/or a size threshold; env opt-out `RB3_NO_MOGG_STREAM`),
  else fall through to `NewFile`. Keep the existing fallback chain
  (`<base>.mogg` → `<base>` → fake BufFile) intact.

## Key files & call sites

Open / fetch path (verified):
- `src/system/synth/Synth.cpp:565` — `Synth::NewStreamFile` (`NewFile("<song>.mogg", 2)` at `:568`). **Layer B routing point.**
- `src/system/synth/Synth.cpp:536` / `:550` — `Synth::NewStream` → `new StandardStream(file, ...)`.
- `src/system/beatmatch/MasterAudio.cpp:129` — `mSongStream = TheSynth->NewStream(info->GetBaseFileName(), 0, 0, false)` (gameplay; always opens the full `<base>.mogg` — prefetch key target).
- `src/system/meta/SongPreview.cpp:283` — `PrepareSong`: branch on `unk71` — `:301` opens the **full** mogg w/ `mStartMs` seek (`unk71==true`, the on-disc path, set at `:184`); `:296`–`:297` opens a **separate `<base>_prev`** stream (`unk71==false`); `:290`–`:295` DLC `dlc/` rewrite. So "no `_prev` file" is **conditional** (true for shipped on-disc songs, not in general).
- `native/src/native_file.cpp:119` — `NativeStdioFile` ctor; `:163` — the synchronous `WebAssetsFetchSync(fetchPath)` whole-file fetch on `fopen` miss. **The freeze + the sync/async coordination point** (check `RB3PrefetchInFlight` here before the sync XHR).
- `native/src/native_file.cpp:201`–`:204` / `:251`–`:255` — `ReadAsync` (inline `Read`) + `ReadDone` (returns `true` immediately): the **synchronous-fronted** `File` contract that makes the VorbisReader producer run on the calling (main) thread. **Layer B must replace this with an async-completing `ReadDone` for the range file.**
- `milo-native-engine/src/platform/WebAssets.cpp:261` — `WebAssetsFetchSync` (blocking `xhr.open(...,false)`; strips `/../`).
- `milo-native-engine/src/platform/WebAssets.cpp:118` — `WebAssetsFetch` (async `emscripten_fetch`, `memfsPath = "/data/" + serverPath` **verbatim**) + `:60` `onFetchSuccess` (writes MEMFS, **no `..` resolution**). **Layer A engine primitive (reuse) — driver must normalize `..`.**
- `milo-native-engine/src/platform/WebAssets.cpp:192`–`:221` — `onBundleSuccess` `..`-resolution loop (the resolver to mirror in the generic driver).

Decode path (already chunked — verified):
- `src/system/synth/VorbisReader.cpp:202` — `DoFileRead` (HX_NATIVE): 0x4000 chunk read, 0x10000 queue cap, per-chunk decrypt; **consumes the buffer only when `mFile->ReadDone(bytes)` is true** — already pending-tolerant for Layer B's async `ReadDone`.
- `src/system/synth/VorbisReader.cpp:250` — `Poll`: pump loop (no decode thread on native — runs on the main/poll thread).
- `src/system/synth/VorbisReader.cpp:612` — `DoRawSeek`: AES-CTR re-key (range-seek correctness, 16-aligned assert at `:633`).

Hook + plumbing (verified):
- `src/band3/meta_band/MusicLibrary.cpp:535` — `SetHighlightIx` (fires on cursor-lands; calls `StartSongPreview` `:483`). **Layer A hook (correct pre-overlap point).**
- `src/band3/meta_band/MusicLibrary.cpp:490` — `CheckSongPreview` (fires the preview only *at* `mSongPreviewDelay`; resolves `node->GetType()==kNodeSong` + uses `TheSongMgr`). **NOT the prefetch hook** — kept only as the song-resolution reference.
- `src/band3/meta_band/BandSongMgr.h:94` / `:116` — `BandSongMgr::SongAudioData(Symbol)` + `extern BandSongMgr &TheSongMgr` (mogg-path resolution; → `SongInfo::GetBaseFileName()`).
- `native/src/main_web.cpp` `BOOT_RUNNING` (~`:553`, `RunOneFrame`). **Prefetch poll pump.**
- `src/system/utl/Loader.cpp:219` — `emscripten_sleep(0)` JSPI yield pattern (OK for `RB3PrefetchWait` and loaders; **must NOT** go inside the mogg `Read`).
- `native/web/server.py:399` — `_serve_range` (HTTP 206, `Accept-Ranges: bytes`). **Layer B server support, already present.**
- `native/src/native_file.cpp:100` — `cachePutAfterFetch` + `native/web/rb3_pre.js:225` `__rb3CachePut` (IDB write-through; reuse for Phase A3, mind the 37 MB cost).
- `native/CMakeLists.txt` web `NATIVE_SHIMS` / source lists (~`:740`–`:749`) — add the new `rb3_asset_prefetch.cpp` / `rb3_web_range_file.cpp` here.

## Risks & tradeoffs

- **Prefetch wasted bandwidth.** Highlight-prefetch downloads a 37 MB mogg the player
  may never play (cursoring through the list). Because we hook `SetHighlightIx` rather
  than the preview, the prefetch starts *earlier* than the preview's own fetch — so
  debounce it: only enqueue after the cursor rests N ms. Reuse the existing
  `mSongPreviewTimer`/`mSongPreviewDelay` gate (e.g. enqueue from `CheckSongPreview`'s
  resolution but **kick the async fetch on highlight** once the rest-timer crosses a
  short threshold), and cap concurrency to 1–2. Net traffic over today's behavior is
  unchanged for any song the player dwells on; it only becomes non-blocking and earlier.
- **A song picked faster than the prefetch completes** must not double-fetch. Layer A's
  **sync/async coordination** (Phase A2: `RB3PrefetchInFlight`/`RB3PrefetchWait` at the
  `native_file.cpp:163` miss) handles this — the sync open *waits on* the in-flight async
  fetch instead of starting a second 37 MB XHR. The remaining cold case (no dwell at all)
  is Layer B's job; until B lands, A converts the common case (dwell on the song) from a
  6.76 s freeze to ~0 and the quick-pick case to "wait on the already-running fetch."
- **Range-streaming correctness.** AES-CTR is a stream cipher; per-chunk decrypt at
  arbitrary 16-aligned offsets is already what `DoRawSeek` does, so range reads are
  safe — but the HMX header (`CheckHmxHeader`, `mHdrBuf` 60000-byte slurp,
  `VorbisReader.cpp:400`) must be in the first window; size `kFirstWindow ≥ kMaxHeader`.
- **Audio underrun is producer-starvation on the main thread — design the read async.**
  The `VorbisReader` refill (`DoFileRead`) is the audio **producer** and runs on the
  single web thread that also hosts the mixer + JSPI suspension. If Layer B blocks the
  read (an `emscripten_sleep(0)` *inside* `Read`), the producer suspends mid-refill, the
  ring drains, and the mixer underruns — a producer-side, **not** output-thread, failure.
  Mitigations: (1) use the **async-completing `ReadDone`** contract (Phase B1) so no
  suspension happens inside the read; (2) a prebuffer threshold (don't transition
  `kBuffering→kReady` until ≥ M seconds resident); (3) keep Layer A prefetch on so most
  range reads hit MEMFS, not the network. **Verify with `/audio-verify`** after any range
  change — a CTR-counter / range-seek regression shows up as wrong-pitch or noise, and the
  underrun shows up as dropouts.
- **MEMFS memory + IDB write-through.** A prefetched 37 MB mogg sits in MEMFS for the
  session (plus the decoded ring) — status quo today (the sync path already loads it
  whole). The added Phase A3 `cachePutAfterFetch` does a whole-file `FS.readFile` + JS
  copy + IDB put, so it transiently ~2×'s the mogg's heap and is itself a (smaller)
  main-thread stall — throttle/defer it (see Phase A3). Layer B *reduces* peak residency
  by only keeping the streamed window. Consider an LRU evict of the previous song's mogg
  from MEMFS on song change (R7-adjacent).
- **No console-build impact.** Every change is `#ifdef HX_NATIVE`/`__EMSCRIPTEN__`; the
  Wii decomp match build is byte-identical (same discipline as the existing native hooks).

## Verification

Primary (regression/win proof), per `web-netperf-findings-2026-06-08.md`:

```bash
python3 native/web/server.py &
node scripts/web/netperf-suite.mjs --scenario nav --profiles low --runs 3
```

Pass criteria, reading `summary.json` / `REPORT.md` for the `part_difficulty → game`
transition at the **low (50 Mbit/s)** profile:
- **main-thread blocked** drops from ~11.4 s toward the non-audio remainder
  (the venue milo, ~3.5 s, is R3's job; the mogg's ~6.76 s share should vanish).
- **worst RAF gap** for that transition drops from 6.7 s to < ~0.5 s.
- The `20thcenturyboy.mogg` request still appears in the waterfall but as an **async
  fetch overlapping song_select/part_difficulty**, not a blocking XHR inside the
  game transition window (check it lands in the earlier transition's byte bucket).

Secondary:
- **Audio correctness is a gating check, not a nice-to-have** — run `/audio-verify`
  (`scripts/native/audio_verify.py --rank`) on a native capture after **any** Layer B
  range/seek/`ReadDone` change: it proves the same song still decodes at the right
  pitch/speed and is not clipped/noise/silent, guarding the AES-CTR-counter / range-seek
  path. Also run the web capture path (`scripts/web/web-song-preview-audio.mjs`) once web
  boot is healthy. A producer-starvation underrun manifests as **dropouts** (gaps), a
  range-seek bug as **wrong pitch / noise** — `--rank` distinguishes them.
- **Sync/async coordination check (Layer A):** drive a *quick pick* (highlight then
  immediately advance, faster than the prefetch can finish) and confirm in the waterfall
  there is **exactly one** `20thcenturyboy.mogg` request — the async one — and **no**
  second blocking sync XHR for the same path inside the game transition.
- Warm-visit check: second `nav` run in the **same** browser context (or after the IDB
  write-through lands) should show the mogg served from `window.__rb3CacheStats`
  (`bytesFromCache`), zero network for it.
- `rb3-tests` (gtest) stays green; add a small unit around the prefetch dedup/queue +
  `..`-normalization if it's pure-logic.

## Effort, impact & dependencies

- **Effort:** Layer A = **M** (new prefetch driver + one hook + sync/async coordination +
  IDB write-through, all reusing existing primitives) — drops to **S** if R2 lands the
  generic driver first. Layer B = **L** (new range-backed `File`, async-completing
  `ReadDone`, prebuffer tuning, audio-correctness verification). Combined R4 = **L**.
- **Impact:** **critical** — removes the single worst freeze in the game
  (6.76 s dead frame at 50 Mbit/s, ~2.3 s at 200 Mbit/s). **Layer A is the higher-value,
  lower-risk slice** (it kills the worst single 6.76 s freeze for any normal dwell with no
  audio-correctness risk) — sequence it as **Wave 2**. **Layer B is Wave 3 polish**: it
  only bounds the *cold* no-dwell / very-slow-link worst case to header latency and carries
  genuine audio-correctness risk; defer it until the freeze is gone and the netperf harness
  confirms Layer A landed.
- **Dependencies / relationships:**
  - **Shares ONE async-prefetch driver with R2** (per-screen milo prefetch: the venue /
    colorpalettes milos are the same blocking-XHR class) — build Phase A1's
    `RB3PrefetchAsset`/`RB3PrefetchPoll` **once, generically**, so R2's manifest pump and
    R4's mogg hook are thin callers. If R2 lands the generic driver first, R4 Layer A is
    just the song-mogg hook (**S**). **Path-normalization caveat:** the generic driver
    fetches via `WebAssetsFetch`→`onFetchSuccess`, which writes `/data/<path>` **verbatim**
    and does **not** resolve `..` (only `onBundleSuccess`/`WebAssetsFetchSync` do, verified
    `WebAssets.cpp:60` vs `:192`/`:261`). Song moggs have no `..`, but some milo paths R2
    feeds do — so the driver must normalize `..`/`.` itself (mirror `onBundleSuccess`'s
    resolver) or those files land at the wrong MEMFS path and the later fopen misses. This
    is a build-once-correctly requirement, not an R4-only concern.
  - **R3 (boot bundle) is a *different* mechanism** (one `/api/bundle` fetch +
    `onBundleSuccess` MEMFS-unpack for the boot working set), not the per-file prefetch
    driver — R4 does not share code with R3.
  - **Independent of R6** (wire compression) — moggs are already-compressed Ogg, so R6
    does not help the worst case; R4 is the correct lever for audio.
  - **Composes with the W4b IDB warm cache** (already shipped) via Phase A3.
  - Layer B depends only on server 206 support, **already present** (`_serve_range`).

## Open questions

- **Prefetch trigger granularity:** `SetHighlightIx`-with-debounce (the chosen hook) vs.
  only on `part_difficulty` enter. Highlight is earlier (more overlap) but risks wasted
  fetches while scrolling; the existing `mSongPreviewTimer`/`mSongPreviewDelay` rest-gate
  likely makes highlight safe — confirm with a `nav` run that scrolls the list.
- **Concurrency budget:** how many simultaneous async fetches before the mogg starves
  the per-screen milos (or vice-versa)? Start at 1 dedicated lane for the mogg + 1 for
  milos; measure.
- **Layer B prebuffer depth:** how many seconds resident before `kReady` to avoid
  dropouts on a 50 Mbit/s link mid-song (interacts with `RB3_STREAM_BUF_SECS` ring depth,
  `src/system/synth/StandardStream.cpp:144`). Needs an empirical sweep.
- **Eviction:** do we evict the previous song's 37 MB mogg from MEMFS on song change, or
  rely on the session ending? (Memory-pressure question; defer unless OOM observed.)
- **DLC / non-extracted moggs:** gameplay always opens `<base>.mogg` (`MasterAudio::Load`),
  so the *gameplay* prefetch key is correct for DLC too. The divergence is preview-only:
  `SongPreview::PrepareSong` opens a separate `<base>_prev` stream when `unk71==false` and
  has a `dlc/` path-rewrite branch (`src/system/meta/SongPreview.cpp:290`–`297`). The port
  currently ships extracted on-disc songs (the `unk71==true` full-mogg path), so the
  preview-hitch bonus holds; **verify the `<base>.mogg` key still matches what
  `NewStreamFile` opens if/when DLC or `_prev` files are added** — but the gameplay
  prefetch needs no change either way.

## Review corrections applied

Verified against the code; folded into the design at source (not appended notes):

- **Hook moved `CheckSongPreview` → `SetHighlightIx`/`StartSongPreview`.** `CheckSongPreview`
  (`MusicLibrary.cpp:490`) fires only *at* the `mSongPreviewDelay` boundary (zero
  pre-overlap); `SetHighlightIx` (`:535`, calls `StartSongPreview` `:483`) fires on
  cursor-lands — the correct earliest prefetch trigger.
- **Added sync-vs-async coordination** (`RB3PrefetchInFlight`/`RB3PrefetchWait` at the
  `native_file.cpp:163` miss) so a fast pick waits on the in-flight async fetch instead of
  issuing a second blocking 37 MB XHR.
- **Qualified the `_prev`/`unk71` claim as conditional.** `SongPreview::PrepareSong:283`
  opens the full mogg only when `unk71==true` (on-disc, `:301`); otherwise a separate
  `<base>_prev` stream (`:296`) — and DLC rewrites `dlc/` (`:290`). Gameplay always opens
  `<base>.mogg`, so the prefetch is correct regardless; only the preview-hitch bonus is
  conditional.
- **Corrected Layer B's underrun cause to producer-starvation** (the main-thread
  `VorbisReader::DoFileRead` refill, given the synchronous-fronted `ReadAsync`/`ReadDone`
  contract at `native_file.cpp:201`/`:251`), **not an output thread.** Replaced the
  "`emscripten_sleep(0)` inside the read" mitigation with an **async-completing `ReadDone`**
  (pending until the range XHR lands; `DoFileRead` already tolerates it). Made
  `/audio-verify` a gating check.
- **Fixed the shared-driver dependency: R4 Layer A shares ONE driver with R2** (not R3),
  added the **`..`-normalization caveat** (`onFetchSuccess` `WebAssets.cpp:60` does not
  resolve `..`; only `onBundleSuccess` `:192` / `WebAssetsFetchSync` `:261` do), flagged
  R4-A as the higher-value/lower-risk Wave-2 slice and R4-B as Wave-3 polish, and accounted
  for the `cachePutAfterFetch` 37 MB main-thread stall + ~2× transient heap.
- **Corrected citation:** `NewFile("<song>.mogg", 2)` is `Synth.cpp:568` (function
  `NewStreamFile` opens at `:565`); song-resolution uses `TheSongMgr` /
  `BandSongMgr::SongAudioData(Symbol)` (`BandSongMgr.h:94`/`:116`), not a `mSongMgr` member.
