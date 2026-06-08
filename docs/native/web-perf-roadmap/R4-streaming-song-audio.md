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

Two complementary layers; ship (A) first (it removes the freeze with the least risk),
keep (B) as the latency/footprint upgrade.

### Layer A — Async prefetch on song selection (primary, low risk)

Make the 37 MB arrive **off the critical path**, overlapped with the time the player
spends on `song_select` / `part_difficulty`, so that by the time `Game::LoadSong`
(`src/band3/game/Game.cpp:242`) opens the mogg the bytes are **already resident in
MEMFS** and `WebAssetsFetchSync` is a no-op (the `fopen` hits).

Data/control flow:

- A new **async prefetch driver** (web-only) owns a small queue of "warm this asset"
  requests and pumps the engine's existing async fetch (`WebAssetsFetch` /
  `onFetchSuccess`, `milo-native-engine/src/platform/WebAssets.cpp:118`), which writes
  the file into MEMFS exactly where `NativeStdioFile::fopen` will later look (`/data/<rel>`).
- A **hook on song highlight/selection** enqueues the highlighted song's mogg path.
  The natural hook is `MusicLibrary::CheckSongPreview`
  (`src/band3/meta_band/MusicLibrary.cpp:490`) — it already resolves the highlighted
  node to a song and fires the preview after `mSongPreviewDelay`. We enqueue the prefetch
  **immediately on highlight** (before the preview delay), so the download starts the
  instant the cursor lands on a song, not when the (full-mogg) preview opens.
- The prefetch path string mirrors `NewStreamFile`: `<SongInfo::GetBaseFileName()>.mogg`.
- The driver is pumped once per frame from the boot/run loop
  (`native/src/main_web.cpp` `BOOT_RUNNING` → `RunOneFrame`), so completions are
  reaped without blocking. No JSPI needed — `emscripten_fetch` is genuinely async.

Net effect: N blocking opens become 1 overlapped background download that finishes
during dwell time. Because the **preview already opens the full mogg**
(`SongPreview::PrepareSong` → `NewStream`, `src/system/meta/SongPreview.cpp:301`; there
is **no separate `_prev` mogg** on disk — preview seeks into the full file via
`mStartMs`), prefetch-on-highlight *also* removes the preview's own sync-XHR hitch.

This reuses the **IDB warm cache** for free: `cachePutAfterFetch`
(`native/src/native_file.cpp:100`) only runs on the sync path, but we add the same
"put after async success" so a prefetched mogg is written through to IndexedDB and
**warm on the next visit** (window.__rb3CachePut, `native/web/rb3_pre.js:225`).

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
`Read`/`ReadAsync(0x4000)` calls by issuing a **range XHR for the missing window** —
but driven through a JSPI yield (`emscripten_sleep(0)`, already used in
`LoadMgr::PollUntilLoaded`, `src/system/utl/Loader.cpp:219`) so each small range fetch
returns to the event loop instead of freezing. VorbisReader's existing 16 KB cadence
becomes the streaming granularity unchanged.

`Synth::NewStreamFile` selects `WebRangeFile` for the song mogg (a flag/threshold:
files over ~4 MB, or specifically the `.mogg` ext) and falls back to the normal
`NativeStdioFile`/MEMFS path otherwise. Layer A's prefetch and Layer B compose: prefetch
warms MEMFS so most range reads hit locally; range-streaming bounds the **worst case**
(cold open, no dwell) to header-latency instead of whole-file-latency.

## Implementation plan

Phased; each phase independently shippable and measurable.

### Phase A1 — Async prefetch driver (web-only)
- Add `native/src/rb3_asset_prefetch.{h,cpp}` (HX_NATIVE/`__EMSCRIPTEN__`):
  - `void RB3PrefetchAsset(const char *serverRelPath)` — dedup against an in-flight /
    completed set + a MEMFS-exists probe (`stat`), then call the engine
    `WebAssetsFetch(serverRelPath)` (declare in `platform/WebAssets.h`).
  - `void RB3PrefetchPoll()` — reap completions, run the IDB write-through
    (mirror `cachePutAfterFetch`), pop the dedup set.
  - Keep a tiny bounded queue (1–2 concurrent fetches) so a giant mogg doesn't starve
    the smaller per-screen milos.
- Pump `RB3PrefetchPoll()` once per frame from `native/src/main_web.cpp` `BOOT_RUNNING`
  (right around `sApp->RunOneFrame`).

### Phase A2 — Hook prefetch on song highlight
- In `MusicLibrary::CheckSongPreview` (`src/band3/meta_band/MusicLibrary.cpp:490`),
  when the highlighted node resolves to a real song, call
  `RB3PrefetchAsset("<GetBaseFileName()>.mogg")` for the highlighted song — guarded
  `#ifdef HX_NATIVE` and behind an env/URL opt-out (e.g. `RB3_NO_MOGG_PREFETCH`).
  Resolve `SongInfo`/`GetBaseFileName()` the same way the preview does
  (`mSongMgr.SongAudioData(sym)`), so the prefetch key === the eventual
  `NewStreamFile` key (`<base>.mogg`).
- Optional: also prefetch from the `part_difficulty` enter (the screen *after* select)
  as a backstop if highlight-prefetch is opted out.

### Phase A3 — IDB write-through for async fetches
- Factor the `cachePutAfterFetch` body (`native/src/native_file.cpp:100`) so the
  prefetch driver can call it after an async success, keeping prefetched moggs warm
  for return visits.

### Phase B1 — Range-streamed mogg File (web-only)
- Add `native/src/rb3_web_range_file.{h,cpp}`: a `File` subclass that
  - opens by range-fetching `[0, kFirstWindow)` (e.g. 256 KB ≥ `kMaxHeader` 60000 +
    first ogg pages), parses `Content-Range` total → `mSize`;
  - implements `Read`/`ReadAsync`/`Seek`/`Tell`/`Size`/`Eof`/`ReadDone` against a
    sparse local buffer, issuing range XHRs (`Range: bytes=a-b`) for missing windows;
  - yields via `emscripten_sleep(0)` per range fetch so it never freezes the frame.
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
- `src/system/synth/Synth.cpp:565` — `Synth::NewStreamFile`: `NewFile("<song>.mogg", 2)`. **Layer B routing point.**
- `src/system/synth/Synth.cpp:536` / `:550` — `Synth::NewStream` → `new StandardStream(file, ...)`.
- `src/system/beatmatch/MasterAudio.cpp:129` — `mSongStream = TheSynth->NewStream(info->GetBaseFileName(), 0, 0, false)` (gameplay).
- `src/system/meta/SongPreview.cpp:297` / `:301` — `mStream = TheSynth->NewStream(...)` (preview; opens the **full** mogg, no `_prev` file on disk).
- `native/src/native_file.cpp:119` — `NativeStdioFile` ctor; `:163` — the synchronous `WebAssetsFetchSync(fetchPath)` whole-file fetch on `fopen` miss. **The freeze.**
- `milo-native-engine/src/platform/WebAssets.cpp:261` — `WebAssetsFetchSync` (blocking `xhr.open(...,false)`).
- `milo-native-engine/src/platform/WebAssets.cpp:118` — `WebAssetsFetch` (async `emscripten_fetch`) + `:60` `onFetchSuccess` (writes MEMFS). **Layer A engine primitive (reuse).**

Decode path (already chunked — verified):
- `src/system/synth/VorbisReader.cpp:202` — `DoFileRead` (HX_NATIVE): 0x4000 chunk read, 0x10000 queue cap, per-chunk decrypt.
- `src/system/synth/VorbisReader.cpp:250` — `Poll`: pump loop (no decode thread on native).
- `src/system/synth/VorbisReader.cpp:612` — `DoRawSeek`: AES-CTR re-key (range-seek correctness, 16-aligned assert at `:633`).

Hook + plumbing (verified):
- `src/band3/meta_band/MusicLibrary.cpp:490` — `CheckSongPreview` (highlight→song resolution). **Layer A hook.**
- `native/src/main_web.cpp` `BOOT_RUNNING` (~`:547`–`:555`, `RunOneFrame`). **Prefetch poll pump.**
- `src/system/utl/Loader.cpp:219` — `emscripten_sleep(0)` JSPI yield pattern (reuse for Layer B).
- `native/web/server.py:399` — `_serve_range` (HTTP 206, `Accept-Ranges: bytes`). **Layer B server support, already present.**
- `native/src/native_file.cpp:100` — `cachePutAfterFetch` + `native/web/rb3_pre.js:225` `__rb3CachePut` (IDB write-through; reuse for Phase A3).
- `native/CMakeLists.txt` web `NATIVE_SHIMS` / source lists (~`:740`–`:749`) — add the new `rb3_asset_prefetch.cpp` / `rb3_web_range_file.cpp` here.

## Risks & tradeoffs

- **Prefetch wasted bandwidth.** Highlight-prefetch downloads a 37 MB mogg the player
  may never play (cursoring through the list). Mitigate: debounce — only enqueue after
  the cursor rests N ms (reuse `mSongPreviewDelay`, which already gates the preview),
  and cap concurrency to 1–2. The preview already opens the full mogg on the same
  delay, so prefetch-on-preview-delay adds **no** net traffic over today's behavior; it
  only makes that traffic non-blocking and earlier.
- **A song picked faster than the prefetch completes** still hits the sync XHR at
  `Game::LoadSong` (Layer A only narrows, doesn't eliminate, the cold case). Layer B is
  the real fix for that worst case; until B lands, A converts the common case (dwell on
  the song) from a 6.76 s freeze to ~0.
- **Range-streaming correctness.** AES-CTR is a stream cipher; per-chunk decrypt at
  arbitrary 16-aligned offsets is already what `DoRawSeek` does, so range reads are
  safe — but the HMX header (`CheckHmxHeader`, `mHdrBuf` 60000-byte slurp,
  `VorbisReader.cpp:400`) must be in the first window; size `kFirstWindow ≥ kMaxHeader`.
- **JSPI re-entrancy.** Layer B's per-read `emscripten_sleep(0)` suspends inside
  `SynthPoll`/`PollStream`; the audio mixer runs on the same single thread, so a long
  range stall can underrun the ring (audible dropout). Mitigate with a prebuffer
  threshold (don't transition `kBuffering→kReady` until ≥ M seconds resident) and keep
  Layer A prefetch on so most range reads hit MEMFS, not the network.
- **MEMFS memory.** A prefetched 37 MB mogg sits in MEMFS for the session (plus the
  decoded ring). That is the status quo today (the sync path already loads it whole);
  Layer B *reduces* peak residency by only keeping the streamed window. Consider an LRU
  evict of the previous song's mogg from MEMFS on song change (R7-adjacent).
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
- Audio still correct after change: `/audio-verify` skill / `scripts/native/audio_verify.py --rank`
  on a native capture (proves the same song still decodes at the right pitch/speed —
  guards against a range-seek / CTR-counter regression). Also run the web capture path
  (`scripts/web/web-song-preview-audio.mjs`) once web boot is healthy.
- Warm-visit check: second `nav` run in the **same** browser context (or after the IDB
  write-through lands) should show the mogg served from `window.__rb3CacheStats`
  (`bytesFromCache`), zero network for it.
- `rb3-tests` (gtest) stays green; add a small unit around the prefetch dedup/queue
  if it's pure-logic.

## Effort, impact & dependencies

- **Effort:** Layer A = **M** (new prefetch driver + one hook + IDB write-through, all
  reusing existing primitives). Layer B = **L** (new range-backed `File`, JSPI-yielded
  reads, prebuffer tuning, audio-correctness verification). Combined R4 = **L**.
- **Impact:** **critical** — removes the single worst freeze in the game
  (6.76 s dead frame at 50 Mbit/s, ~2.3 s at 200 Mbit/s). Layer A alone reclaims the
  bulk of the `part_difficulty → game` stall for any normal dwell time.
- **Dependencies / relationships:**
  - **Shares the async-prefetch driver with R3** (per-screen milo prefetch: the venue /
    colorpalettes milos are the same blocking-XHR class). Build Phase A1's
    `RB3PrefetchAsset`/`RB3PrefetchPoll` generically so R3 reuses it for `.milo_xbox`.
    If R3 lands the generic driver first, R4 Layer A is just the song-mogg hook (S).
  - **Independent of R6** (wire compression) — moggs are already-compressed Ogg, so R6
    does not help the worst case; R4 is the correct lever for audio.
  - **Composes with the W4b IDB warm cache** (already shipped) via Phase A3.
  - Layer B depends only on server 206 support, **already present** (`_serve_range`).

## Open questions

- **Prefetch trigger granularity:** highlight-with-debounce vs. only on `part_difficulty`
  enter. Highlight is earlier (more overlap) but risks wasted fetches while scrolling;
  the existing `mSongPreviewDelay` debounce likely makes highlight safe — confirm with a
  `nav` run that scrolls the list.
- **Concurrency budget:** how many simultaneous async fetches before the mogg starves
  the per-screen milos (or vice-versa)? Start at 1 dedicated lane for the mogg + 1 for
  milos; measure.
- **Layer B prebuffer depth:** how many seconds resident before `kReady` to avoid
  dropouts on a 50 Mbit/s link mid-song (interacts with `RB3_STREAM_BUF_SECS` ring depth,
  `src/system/synth/StandardStream.cpp:144`). Needs an empirical sweep.
- **Eviction:** do we evict the previous song's 37 MB mogg from MEMFS on song change, or
  rely on the session ending? (Memory-pressure question; defer unless OOM observed.)
- **DLC / non-extracted moggs:** `SongPreview::PrepareSong` has a `dlc/` path-rewrite
  branch (`src/system/meta/SongPreview.cpp:290`). Confirm the prefetch key matches what
  `NewStreamFile` ultimately opens for DLC songs (the port currently ships extracted
  on-disc songs, so likely moot, but verify before assuming).
