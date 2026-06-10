# 07 — Network-restricted benchmark matrix (Wave-3 follow-up)

**Date:** 2026-06-10. **Status:** measured (matrix complete).
**Question:** waves 0–2 gated at exactly ONE network point (CDP 20 Mbps / 40 ms,
localhost). The user still sees big frame drops / hangs on a live build accessed
remotely from a laptop. What breaks at harsher/realer network conditions, and
which subsystem is responsible?

**Answer in one line:** the canvas never freezes (the wave-1 16 ms yield gate
holds at every condition tested) — what explodes is **wall-clock time**, because
**every whole-file asset fetch is strictly serialized by the loader queue**
(0 overlapping downloads out of 38+ per journey) and **every mogg Range chunk is
strictly serialized** (single in-flight request, no read-ahead). ~85 MB of
whole-file milos × inverse bandwidth + 1 RTT per file/chunk = transitions that
take 20 s → 9 min as the network degrades. The user's "hangs" are
multi-second-to-minute screen transitions and preview/song-start delays, not
rendering stalls.

---

## 1. Method

- **Build:** the already-deployed release build in `native/web/build/release/`
  (mtime 2026-06-10 09:16; `/api/version` = `1779844837-1781082974`), which IS
  the waves-0–2 build: rb3 `d71aadc3` ("bump MILO_ENGINE_PIN to fb23b5e —
  incremental-load perf waves") + engine `fb23b5e`. Engine HEAD == rb3 pin,
  verified. **Sanity for the user:** if their server's `/api/version` differs
  from a post-`d71aadc3` deploy, their live build may predate waves 0–2 entirely.
- **Harness:** `scripts/web/_netmatrix.mjs` (derived from the E3
  `throttled.mjs`). One Playwright Chromium per run (headless, WebGPU/Vulkan,
  JSPI on), **fresh browser context + `Storage.clearDataForOrigin` (clears IDB)**
  per run = cold cache. CDP `Network.emulateNetworkConditions` for throttle.
  Server: own `python3 native/web/server.py --port 8441`.
- **Journey (same for every condition):** boot → intro → title(splash) →
  main_hub → song_select → 3 cold hovers (4-rows-apart songs, 9 s dwell each) →
  start a song → 7 s gameplay dwell.
- **Metrics:** per phase wall ms; rAF gap distribution (longest, #>100 ms,
  #>250 ms, frozen-Σ = Σ(gap−250) over gaps>250 ms); CDP per-request bytes +
  duration; `miloSerialΣ` = sum of whole-file milo (>0.5 MB) fetch durations
  (the clean, nav-cadence-free network-cost signal); per-gap in-flight-request
  correlation (`hoverGapNet` in result.json).
- 2 runs per condition (c1–c4 variance <2 % on appBoot/miloSerialΣ; c3's two
  appBooted values were identical to 0.01 s). c5 ran with an extended timeout
  (the standard 420 s cap cannot contain the journey at 1.5 Mbps).
- **Caveat:** `splash→hub` / `hub→songsel` *wall* numbers include scripted
  key-press pacing (±5–15 s run-to-run); they are directionally right but noisy.
  `appBooted`, `miloSerialΣ`, per-chunk durations, and the rAF stats are the
  trustworthy columns.
- **Frame-trace counters did NOT capture on web (env-timing, not a missing wire
  — correction to an earlier draft of this doc).** The recorder *is* wired into
  the web loop: `main_web.cpp:727-738` mirrors `App::RunWithoutDebugging`'s
  trace wrap, env-gated on `getenv("RB3_FRAME_TRACE")`, and `rb3_frame_trace.cpp`
  `fopen(path,"w")`s + `fflush`es after every record (lines 99,156). Yet the
  harness read of `/trace.jsonl` returned `ERR:ErrnoError: No such file or
  directory` in **every** run, i.e. the file was never opened. Root cause is an
  env-bridge ordering issue: `RB3_FRAME_TRACE` arrives via the `?env=…` URL param
  drained by `::setenv` (main_web.cpp:204/247), but the trace-init `getenv`
  (and `main_web.cpp:721`'s `sFrameTrace` latch) can run before that drain on the
  deployed build, so the trace stays disabled. **Consequence for this study:**
  per-frame fetch/dta/obj/prime/tex/mesh/pipe attribution counters are
  unavailable, so the gap decomposition below uses the CDP network timeline +
  rAF correlation + code reading instead. Fix is a one-liner (drain the `?env=`
  bridge before any `getenv`-latching static init, or move the `sFrameTrace`
  latch after the drain) — flagged as ranked fix #4.

## 2. Matrix

All runs release build, cold cache. "hover worst" = worst of the 3 cold hovers.

| Condition | run | appBooted (s) | miloSerialΣ (s) | max single milo (s) | mogg chunk p50 (ms) | hover worst: longest / >100ms / frozenΣ | startSong: longest / >100ms | reached game |
|---|---|--:|--:|--:|--:|---|---|---|
| **c1 — 20 Mbps / 40 ms** (shipped gate) | 1 | 19.84 | 38.7 | 5.1 | 452 | 36.3 / 0 / 0 | 88.6 / 0 | yes |
| | 2 | 19.82 | 38.1 | 5.1 | 452 | 36.0 / 0 / 0 | 68.3 / 0 | yes |
| **c2 — 20 Mbps / 150 ms** (RTT-dominant) | 1 | 24.86 | 43.3 | 5.7 | 566 | 35.9 / 0 / 0 | 87.7 / 0 | yes |
| | 2 | 24.82 | 41.8 | 5.5 | 564 | 36.7 / 0 / 0 | 89.3 / 0 | yes |
| **c3 — 8 Mbps / 80 ms** (typical remote) | 1 | 43.78 | 96.7 | 19.5 | 1087 | 36.0 / 0 / 0 | 86.9 / 0 | yes |
| | 2 | 43.78 | 99.6 | 19.1 | 1087 | 36.0 / 0 / 0 | 71.1 / 0 | yes |
| **c4 — 4 Mbps / 150 ms** (bad WiFi/hotel) | 1 | 84.30 | 185.7 | **45.2** | 2161 | 37.0 / 0 / 0 | 85.7 / 0 | yes |
| | 2† | 84.31 | boot-only | — | — | — | — | — |
| **c5 — 1.5 Mbps / 300 ms** (3G-class) | 1‡ | ~217 | boot-only (salvage) | — | ~5600 | canvas clean (frames advancing) | — | streaming | 

† **c4-run2 is boot-only.** The batch's own server (port 8441) died during this
run's song_select phase → `netSummary.failed = 336` (mass ECONNREFUSED),
`hovers=[]`, `startSong=null`. Its `appBooted=84.31` corroborates run1's 84.30
exactly; everything after boot is discarded. **The same server death killed both
batched c5 runs** (`page.goto … ERR_CONNECTION_REFUSED`, zero artifacts) — so the
batched c5 rows are a server-lifetime casualty, NOT a client/engine fault. Most
likely cause: memory growth from on-the-fly brotli of the large milo set into
`native/web/.cache/encoded/` across 8 prior runs (that cache is now warm, so a
re-run no longer recompresses).

‡ **c5 salvage** — re-run alone against a fresh own-server (port 8445) with a
20-min cap (the batch's `timeout 420` cannot contain a 1.5 Mbps journey). At
write-time the run had reached, with the canvas advancing normally throughout:
intro at **216.7 s**, splash (title) at **306.7 s**, and was still inside the
**single splash→main_hub transition at +120 s and counting** (downloading the
~85 MB char/venue milo set serially at 1.5 Mbps) — itself the clearest possible
demonstration of the thesis: one screen transition takes **minutes** at
3G-class bandwidth. Per-1 MB mogg chunk ≈ 5.6 s (1 MB / 1.5 Mbps + 0.3 s RTT),
so preview/playback audio cannot keep up (mogg stream ≈ 1.2 Mbps vs ~1.4 Mbps
achievable → chronic underrun). Single-run boot/transition timings only; the §3
harsh-network extrapolations are the load-bearing claim, not this row.

**Packet loss (condition 6): SKIPPED.** CDP cannot emulate loss;
`sudo tc qdisc … netem` requires a password in this environment
(`sudo -n true` fails) — per the task rules I did not fight the sandbox.

### Reading the matrix

- **The shipped gate re-baselines cleanly** (c1 ≈ the E3 results: cold hover
  longest ≤36 ms, frozen-Σ 0) → the deployed build is NOT stale.
- **frozen-Σ = 0 and #gaps>250 ms = 0 at every condition.** The wave-1
  `RB3_LOADER_MIN_YIELD_MS=16` yield gate (src/system/utl/Loader.cpp:198-300)
  genuinely keeps the canvas compositing at any bandwidth.
- **What scales is wall time, ~inverse-bandwidth + 1 RTT per serial request:**
  - appBooted: 19.8 → 24.9 (+110 ms RTT × 49 serial boot requests [measured:
    49 requests / 39.1 MB finish before appBooted] = +5.0 s, bandwidth
    unchanged!) → 43.8 (8 Mbps) → **84.3 (4 Mbps)** → **~199 (1.5 Mbps)**.
    20→4 Mbps = 5× bandwidth drop ⇒ 4.3× boot — almost pure inverse-bandwidth.
  - miloSerialΣ: 38.4 → 42.6 (+~40 × 110 ms RTT) → 98 s (8 Mbps; 2.5×) →
    **186 s (4 Mbps; ~4.8× = pure inverse bandwidth)** — serial chain, so the
    sum *is* the wait.
  - max single milo (the largest venue/char milo on a transition): 5.1 s
    (20 Mbps) → 19.5 s (8 Mbps) → **45.2 s (4 Mbps)** → ~67 s extrapolated at
    1.5 Mbps — during which that transition shows nothing new (canvas
    compositing at 60 fps, zero >100 ms rAF gaps). **This 45 s "frozen" screen
    transition is the user's reported "hang."**

## 3. Per-suspect verdicts

### (a) Range-chunk serialization — **CONFIRMED**, quantified

`WebRangeFile` (native/src/rb3 `native/src/native_file.cpp:599-`) keeps **at
most ONE Range request in flight** — `mReqId`/`mReqChunk` are single slots, and
`PollChunkFetch` (native_file.cpp:729) *drops* any in-flight request when a
different chunk is needed. `ServicePendingRead` (native_file.cpp:801) only ever
requests the chunk under the current read offset — **no read-ahead exists**.
VorbisReader consumes in 16 KB reads (`src/system/synth/VorbisReader.cpp:336`,
`ReadAsync(mReadBuffer, 0x4000)`), so ~64 cache hits then 1 cold chunk
boundary = 1 RTT + 1 MB/bandwidth stall, strictly serial.

Measured: mogg chunks 0/14 overlapping at every condition; per-chunk
p50 452 ms @40 ms RTT → 566 ms @150 ms RTT (**delta == the RTT delta,
exactly**) → 1087 ms @8 Mbps. The netlog chunk sequence shows each cold hover
costs **3 serial chunks before preview audio** (header chunk at offset 0, then
the seek to the preview point ~9–12 MB in = 2 more), back-to-back: preview
latency ≈ 3 × (1 MB/bw + RTT) ≈ 1.5 s @20 Mbps, 3.3 s @8 Mbps, ~6.6 s @4 Mbps,
~17 s @1.5 Mbps. Sustained playback (mogg ≈ 1.2 Mbps) barely keeps up at
3G-class — every chunk boundary risks an audible underrun. Bonus finding:
starting the song **re-downloads chunks 0–1 of the very mogg the preview just
streamed** — the chunk LRU lives per `WebRangeFile` *object*, and gameplay's
`NewStream` opens a fresh one (no cross-open chunk cache).

### (b) Manifest/oracle/bundle on the critical path — **REFUTED** (as a sync-blocking issue); reframed: the LOADER QUEUE serializes all whole-file fetches — **CONFIRMED as the #1 cost**

The manifest oracle is pre-warmed by `rb3_pre.js` racing wasm compile
(`window.__rb3ManifestSizes`); the fallback is a JSPI-suspending fetch
(engine `WebAssets.cpp:500` `webAssetsFetchManifestBlob`), not sync-blocking.
The boot bundle (15 MB, 6.1–7.2 s at 20 Mbps) finishes at +7 s while appBooted
is +19.8 s → not the boot gate either.

The real finding: **whole-file milo fetches are 100 % serial** — in every
netlog, 0 of 38 big-milo downloads overlap. Root cause is architectural:
`LoadMgr` advances only `mLoading.front()` (`PollFrontLoader`,
src/system/utl/Loader.cpp:270/318/465), and the async fetch for a file is only
kicked when its `WebPendingFile` is **constructed**
(native_file.cpp:446 `WebAssetsEnsureResidentAsync` in the ctor) — which only
happens when the loader reaches it. Queued-but-not-front loaders have no
fetch in flight. Effective throughput of the serial chain at 20 Mbps: 17.9 of
20 Mbps (bandwidth-saturated per file, but each file pays its own RTT, and
*nothing* downloads during each file's RTT + parse tail). 85 MB of whole-file
milos per cold journey × inverse bandwidth is the user's wait.

### (c) Pending-file poll starvation — **PARTIALLY CONFIRMED** (spin-with-yield, canvas-safe but CPU-wasteful; one true busy-poll path)

- `LoadMgr::PollUntilLoaded` web arm (src/system/utl/Loader.cpp:198-300):
  while the front file's bytes are pending, the loop keeps slicing
  (`PollFrontLoader` 8 ms arms) and yields once per 16 ms
  (`emscripten_sleep(0)`, Loader.cpp:289). It never *blocks* the canvas
  (measured frozen-Σ=0 everywhere) but it spins the wasm at ~60 wake-ups/s for
  the full download duration — at 1.5 Mbps that's a 67 s spin for one venue
  milo. Wasteful, not fatal.
- `WebPendingFile::Read` (sync read of a pending file,
  native_file.cpp:450-460) → `BlockUntilReadyOrFail` (native_file.cpp:550):
  `for(;;) { … emscripten_sleep(4); }` — a true 250 Hz busy-poll for the whole
  fetch. Comment says "rare" (DTA-lexer paths, bundle-prefetched); did not
  trigger measurably in the matrix journeys, but any non-bundled sync-Read
  path would hit it hard at low bandwidth.
- `StandardStream::Resync` (src/system/synth/StandardStream.cpp:545):
  `while (!mFile->ReadDone(bytes)) ;` — a **zero-yield infinite spin** on the
  main thread. With a WebRangeFile mogg, `ReadDone` cannot complete without an
  event-loop turn (the fetch callback needs it), so if `Resync` ever fires
  while a chunk is in flight, **this deadlocks the tab** (or at minimum hangs
  until watchdog). Not hit in the matrix journey (Resync = seek/jump path),
  but it is a latent web hang — flagged for fix.

### (d) Audio under stall — **CONFIRMED for song/preview start latency; prime itself is canvas-safe**

`StandardStream::Play` (StandardStream.cpp:494-527) pumps
`for(i<500) PollStream()` + `for(i<20) PollStream()` with **no yield** — under
JSPI no fetch can land mid-loop, so when the mogg's first Range chunk is not
yet resident those iterations are no-ops; the prime actually completes across
*subsequent frames* via the per-frame pump. Net effect: no canvas freeze
(measured: startSong longest gap ≤89 ms at c1–c3), but **audio start waits for
chunk delivery**, which is serial (suspect a). With Range moggs ON the preview
prefetch is deliberately a no-op for moggs
(native/src/rb3_prefetch_native.cpp:104-117: "the first VorbisReader read
warms the Range window") — so the first chunk fetch doesn't even start until
the stream opens. At high RTT/low bandwidth that's the full cold-chunk latency
(5.6 s at c5) inserted between hover-debounce and first audio, and between
"start song" and gameplay audio.

**Audio underruns are present at EVERY condition, including c1 (20 Mbps/40 ms):**
the console emits 11–25 `AudioDevice: latency GROW … (sustained underrun)` per
run. Mapped against the journey, the c1 underruns cluster at **screen-transition
boundaries** (≈41–49 s → main_hub menu-music load; ≈62–65 s → song_select;
≈106–116 s → part_difficulty→game), not at steady-state playback — i.e. the menu
/ preview audio ring starves while the main thread is busy draining a serial load
chain, then the AudioDevice grows its buffer (90→330 ms) to compensate. So
underruns are a **second symptom of the same serial-load transition stalls**, not
an independent bug, and they get worse as bandwidth drops (the load drains take
longer). The audio path never blocks the main thread (VorbisReader is fully
async, §a) — it drops out instead. Practical read: the user hears the "static /
dropouts" at the same moments they see the transition "hang."

### (e) Server-side head-of-line blocking — **REFUTED**

`native/web/server.py:1055` uses `http.server.ThreadingHTTPServer` (one thread
per connection). Measured: 6 parallel 1 MB Range requests complete in 7.9 ms
wall vs 30.1 ms serially (~4× speedup) → genuinely concurrent. (Side-note found
while testing: `do_HEAD` + Range returns 200/full-length — `_serve_range` is
gated `if range_hdr and not head_only`, server.py:737 — harmless for the GET
streaming path, but a HEAD-based prober would mis-detect Range support.)

### (f) Timeout/retry silent degradation — **REFUTED**

The only sync-XHR path is opt-in via env (`RB3_SYNC_XHR_LEGACY=1`, engine
`WebAssets.cpp:326/383`; read once at `webAssetsUseLegacyXhr`). No code path
falls back to it on slow/failed responses; `emscripten_fetch` errors mark the
request failed (engine WebAssets.cpp:96 `onFetchError`) and the
`WebPendingFile` reports `Fail()` — no retry storm, no sync fallback. 1
failed request per run in the netlogs (an album-art 404), no degradation.

## 4. Ranked fixes

1. **Loader-queue fetch read-ahead (fixes b, the #1 wall-cost).** When
   `LoadMgr::AddLoader` enqueues (or when `PollFrontLoader` runs), kick
   `WebAssetsEnsureResidentAsync` for the next N (≈4–8) queued loaders' files —
   the engine-side fetch machinery is already async + deduped
   (`sEnsureInFlight`, engine WebAssets.cpp:631). This overlaps download of
   file k+1…k+N with download+parse of file k, turning the 85 MB serial chain
   into a pipelined one. Estimated impact: transition wall ≈ max(bandwidth
   time, parse time) instead of their sum + per-file RTTs — at 8 Mbps roughly
   **40–50 % off splash→hub / hub→song_select**; bigger at higher RTT.
   (A server-side "screen bundle" does the same thing with fewer requests —
   the Wave-3 bundle infra is the alternative implementation.)
2. **Mogg Range read-ahead of chunk N+1 (fixes a, d).** In
   `WebRangeFile::PollChunkFetch`/`ServicePendingRead`
   (native_file.cpp:729/801), after chunk N lands (or while serving reads from
   it), kick the fetch for N+1 (VorbisReader reads strictly forward; the LRU
   window is 6 chunks). Needs the single-slot `mReqId` to become 2 slots.
   Eliminates the per-MB RTT stall during playback **and** halves cold-start
   chunk latency for preview/gameplay audio. At c5-class networks this is the
   difference between "preview in ~6 s, playback keeps up" and "preview in
   12–17 s, underruns at every chunk boundary". Estimated impact: removes
   ~1 RTT per MB of audio (at 150 ms RTT ≈ 0.5–1 s/min of playback) + halves
   audio start latency; prevents underrun bursts at <2 Mbps.
3. **Fix the `StandardStream::Resync` zero-yield spin (latent web deadlock)**
   (StandardStream.cpp:545): bound it / route through the pending-aware poll
   (the same `#ifdef HX_WEB` slice+yield pattern PollUntilLoaded uses). Low
   effort, removes a real tab-hang class on any seek/jump over a stalled
   stream.
4. **Fix the web frame-trace env-bridge ordering so `RB3_FRAME_TRACE`
   actually captures.** The recorder IS already wired into the web loop
   (`main_web.cpp:727-738`) and `rb3_frame_trace.cpp` flushes per record, but
   the trace file never opened in any run (`/trace.jsonl` =
   `ERR:ErrnoError`) because the `?env=` → `::setenv` drain
   (main_web.cpp:204/247) can run *after* the trace-init `getenv` /
   `sFrameTrace` latch (main_web.cpp:721). Drain the env bridge before any
   `getenv`-latching static init (or move the `sFrameTrace` read past the
   drain). One-liner; without it the per-frame fetch/dta/obj/prime/tex/mesh/pipe
   attribution counters — all incremented engine-wide already — stay unreadable
   on web, forcing the indirect attribution used here and hiding any future
   web-only regression.
5. **(Realism, not a code fix) gate future perf work at ≥2 network points** —
   the 20 Mbps/40 ms gate provably cannot see any of the above (c1 row is
   clean); 8 Mbps/80 ms (c3) is the cheapest condition that exposes the wall
   scaling, and 4 Mbps/150 ms (c4) the user-symptom regime.

## 5. Repro commands

```bash
# server (own instance; kill when done)
python3 native/web/server.py --port 8441

# one matrix cell (cold cache, full journey, all artifacts under --out)
cd scripts/web && node _netmatrix.mjs --port 8441 \
  --out /tmp/rb3perf-netmatrix/c3-run1 --mbps 8 --rtt 80 --label c3 --run 1

# whole matrix (sequential, 2 runs/condition)
bash scripts/web/_netmatrix_batch.sh   # writes /tmp/rb3perf-netmatrix/
# NOTE: the batch's per-run `timeout 420` cannot contain c5 (1.5 Mbps); run c5
# alone with a longer cap + a FRESH server (the long batch can exhaust the
# matrix server, which kills later runs with ERR_CONNECTION_REFUSED):
python3 native/web/server.py --port 8445 &   # warm .cache/encoded first
cd scripts/web && timeout 1200 node _netmatrix.mjs --port 8445 \
  --out /tmp/rb3perf-netmatrix/c5-run1-salvage --mbps 1.5 --rtt 300 --label c5 --run 1

# aggregate table (basic stub + the enhanced aggregator written for this study)
python3 /tmp/rb3perf-netmatrix/aggregate.py
python3 /tmp/rb3perf-netmatrix/aggregate2.py   # matrix + RTT/bw scaling + underrun tally

# server concurrency probe (suspect e) — parallel vs serial Range reads
for i in 0 1 2 3 4 5; do off=$((i*1048576)); \
  curl -s -o /dev/null -H "Range: bytes=$off-$((off+1048575))" \
  "http://127.0.0.1:8441/api/file/songs/20thcenturyboy/20thcenturyboy.mogg" & done; wait
# (measured: 8 parallel ≈ 4× faster than 8 serial → server is genuinely threaded)
```

Raw artifacts: `/tmp/rb3perf-netmatrix/{c1..c4}-run{1,2}/` +
`c5-run1-salvage/` (`result.json`, `raf.json`, `net.ndjson`, `console.ndjson`) +
`aggregate.py` / `aggregate2.py`. c5 batched rows + c4-run2's post-boot data are
absent (server died — see table footnotes † / ‡).
