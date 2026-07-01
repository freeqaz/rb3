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
- 2 runs per condition c1–c4 (variance <5 % on appBoot/miloSerialΣ; c3's two
  appBooted values identical to 0.01 s, c4's to 0.01 s); c5 = 1 valid full run
  with an extended 30-min cap (the standard 420 s cap cannot contain the
  journey at 1.5 Mbps) — single-run, but its boot/splash timings were
  independently corroborated by a concurrent agent's partial run (footnote ‡).
- **Caveat:** `splash→hub` / `hub→songsel` *wall* numbers include scripted
  key-press pacing (±5–15 s run-to-run); they are directionally right but noisy.
  `appBooted`, `miloSerialΣ`, per-chunk durations, and the rAF stats are the
  trustworthy columns.
- **Frame-trace counters did NOT capture on web.** In the **deployed build that
  every matrix run measured** (built 09:16 from `d71aadc3`), the recorder
  `RB3FrameTraceRecord` is called only from `App::RunWithoutDebugging`
  (src/App.cpp:809), while the web loop calls `sApp->RunOneFrame()` directly
  (the committed `native/src/main_web.cpp:711`) — so `RB3_FRAME_TRACE` never
  opens its file and the harness read of `/trace.jsonl` returned
  `ERR:ErrnoError: No such file or directory` in every run. (Note: an
  **uncommitted working-tree edit by a concurrent Wave-3 agent** adds the wiring
  at main_web.cpp:819-833; it is NOT in the measured build, and this doc makes
  no claims about it.) **Consequence for this study:** per-frame
  fetch/dta/obj/prime/tex/mesh/pipe attribution counters were unavailable, so
  the gap decomposition below uses the CDP network timeline + rAF correlation +
  code reading instead — flagged as ranked fix #4.

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
| | 2† | 84.31 | 194.9 | 44.1 | 2163 | 37.0 / 0 / 0 | 89.9 / 0 | yes |
| **c5 — 1.5 Mbps / 300 ms** (3G-class) | 1‡ | 215.54 | 286.9 (through splash only) | **135.9** | — (song_select never reached) | — (journey DNF) | — | **no — DNF** |

† **c4-run2 is a re-run.** The batch's original c4-run2 was invalidated when
the matrix server (port 8441) died mid-run (`netSummary.failed = 336`, mass
ECONNREFUSED; quarantined as `c4-run2-INVALID-serverdied/` — its
boot-only `appBooted=84.31` already corroborated run1). The same server death
made the batch's two 420 s-capped c5 attempts fail instantly. The server log
ends cleanly (no traceback/MemoryError); the box runs concurrent agents and an
external kill is the most consistent explanation. A health-check watchdog
(auto-restart) was armed and run2 + c5 were re-run cleanly (1 failed request
each — an album-art 404; the watchdog never fired). The re-run reproduces run1
almost exactly (appBoot 84.31 vs 84.30; chunk p50 2163 vs 2161 ms).

‡ **c5 = journey Did-Not-Finish** (complete artifacts in
`/tmp/rb3perf-netmatrix/c5-run2/`, 30-min cap; 1 failed request total — clean
run, server watchdog never fired). appBooted **215.5 s** (10.9× the c1
baseline); intro at 216.2 s; splash (title) at **298.8 s**; intro→splash alone
took 82.6 s wall; **a single venue milo took 135.9 s**; main_hub was NEVER
reached — the harness's 180 s main_hub wait expired mid splash→hub transition
(~85 MB of serial milos at 1.5 Mbps needs ≳450 s of transfer), so hovers and
song-start never ran. The canvas stayed live the whole time (longest rAF gap
397.8 ms across the 215 s boot; one gap >250 ms total). A concurrent agent's
independent partial run (own server on :8445) corroborates: intro 216.7 s /
splash 306.7 s, still inside splash→hub at +120 s when observed. Per-1 MB mogg
chunk at this condition ≈ 5.6 s (1 MB/1.5 Mbps + 0.3 s RTT), so preview audio
would need ~17 s to start and playback (mogg ≈ 1.2 Mbps) cannot sustain —
3G-class is effectively unplayable regardless of any client fix short of
drastically smaller assets.

**Packet loss (condition 6): SKIPPED.** CDP cannot emulate loss;
`sudo tc qdisc … netem` requires a password in this environment
(`sudo -n true` fails) — per the task rules I did not fight the sandbox.

### Reading the matrix

- **The shipped gate re-baselines cleanly** (c1 ≈ the E3 results: cold hover
  longest ≤36 ms, frozen-Σ 0) → the deployed build is NOT stale.
- **frozen-Σ = 0 and #gaps>250 ms = 0 in every post-boot phase (transitions,
  hovers, song start) at every condition.** The wave-1
  `RB3_LOADER_MIN_YIELD_MS=16` yield gate (src/system/utl/Loader.cpp:198-300)
  genuinely keeps the canvas compositing at any bandwidth. The only >100 ms
  gaps anywhere are a fixed boot-time cluster (~390/210/110 ms; max 1 gap
  >250 ms) whose magnitude is **RTT/bandwidth-invariant across all five
  conditions** — first-render GPU pipeline-compile class, not network.
- **What scales is wall time, ~inverse-bandwidth + 1 RTT per serial request:**
  - appBooted: 19.8 → 24.9 (+110 ms RTT × 49 serial boot requests [measured:
    49 requests / 39.1 MB finish before appBooted] = +5.0 s, bandwidth
    unchanged!) → 43.8 (8 Mbps) → **84.3 (4 Mbps)** → **215.5 (1.5 Mbps)**.
    20→4 Mbps = 5× bandwidth drop ⇒ 4.3× boot — almost pure inverse-bandwidth.
  - miloSerialΣ: 38.4 → 42.6 (+~40 × 110 ms RTT) → 98 s (8 Mbps; 2.5×) →
    **186 s (4 Mbps; ~4.8× = pure inverse bandwidth)** — serial chain, so the
    sum *is* the wait.
  - max single milo (the largest venue/char milo on a transition): 5.1 s
    (20 Mbps) → 19.5 s (8 Mbps) → **45.2 s (4 Mbps)** → **135.9 s measured at
    1.5 Mbps** — during which that transition shows nothing new (canvas
    compositing at 60 fps, zero >100 ms rAF gaps). **This 45–136 s "frozen"
    screen transition is the user's reported "hang."** At 1.5 Mbps the journey
    outright DNFs (main_hub unreachable within a 180 s wait).

## 3. Per-suspect verdicts

### (a) Range-chunk serialization — **CONFIRMED**, quantified

`WebRangeFile` (`native/src/native_file.cpp:599-845`) keeps **at
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
the console emits 17–29 `AudioDevice: latency GROW … (sustained underrun)` lines
per full-journey run. Mapped against the journey, the c1 underruns cluster at **screen-transition
boundaries** (≈41–49 s → main_hub menu-music load; ≈62–65 s → song_select;
≈106–116 s → part_difficulty→game), not at steady-state playback — i.e. the menu
/ preview audio ring starves while the main thread is busy draining a serial load
chain, then the AudioDevice grows its buffer (90→330 ms) to compensate. So
underruns are a **second symptom of the same serial-load transition stalls**, not
an independent bug. Measured GROW-event *counts* are flat across conditions
(17–29 per run at c1→c4, no bandwidth trend): the underruns are tied to the transition *events*
(which occur at every bandwidth), while what scales with bandwidth is how long
each starved transition lasts. The audio path never blocks the main thread
(VorbisReader is fully async, §a) — it drops out instead. Practical read: the
user hears the "static / dropouts" at the same moments they see the transition
"hang."

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
4. **Wire `RB3FrameTraceRecord` into the web frame loop and verify it
   captures.** In the deployed build the recorder lives only in
   `App::RunWithoutDebugging` (src/App.cpp:809); the web loop
   (main_web.cpp:711) bypasses it, so the per-frame
   fetch/dta/obj/prime/tex/mesh/pipe attribution counters — all incremented
   engine-wide already — are unreadable on web. That gap forced the indirect
   attribution used in this study and will hide any future web-only
   regression. A concurrent agent's uncommitted working-tree edit
   (main_web.cpp:819-833) already adds this wiring — land it, then confirm
   `?env=RB3_FRAME_TRACE=/trace.jsonl` produces records (and that the `?env=`
   → `::setenv` drain at main_web.cpp:204/247 runs before the recorder's
   first `getenv` latch).
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

# whole matrix (sequential, 2 runs/condition; c1-c4 only — 420 s/run cap)
bash scripts/web/_netmatrix_batch.sh   # writes /tmp/rb3perf-netmatrix/
# slow conditions need extended caps (c4 900 s, c5 1800 s):
bash scripts/web/_netmatrix_slow.sh c4 c5

# aggregate table (basic + the enhanced aggregator)
python3 /tmp/rb3perf-netmatrix/aggregate.py
python3 /tmp/rb3perf-netmatrix/aggregate2.py   # matrix + RTT/bw scaling + underrun tally

# server concurrency probe (suspect e) — parallel vs serial Range reads
for i in 0 1 2 3 4 5; do off=$((i*1048576)); \
  curl -s -o /dev/null -H "Range: bytes=$off-$((off+1048575))" \
  "http://127.0.0.1:8441/api/file/songs/20thcenturyboy/20thcenturyboy.mogg" & done; wait
# (measured: 6 parallel = 7.9 ms vs 6 serial = 30.1 ms → genuinely threaded)
```

Raw artifacts: `/tmp/rb3perf-netmatrix/{c1..c5}-run*/` (`result.json`,
`raf.json`, `net.ndjson`, `console.ndjson`) + `aggregate.py` / `aggregate2.py`.
The server-death casualty is quarantined as `c4-run2-INVALID-serverdied/`
(see footnote †); the valid c4 run2 is the re-run.
