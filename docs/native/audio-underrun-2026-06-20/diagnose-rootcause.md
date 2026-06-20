# Audio UNDER-RUN — root-cause diagnosis (reproduced + measured)

**Date:** 2026-06-20
**Engine pin:** `4087e92e76c64830d9d7d23cfdaa1398312d6f38` (rb3/native/CMakeLists.txt:74) — verified == engine HEAD.
**Author:** diagnose agent. Read the three scout docs first (scout-web / scout-native / scout-history).

---

## VERDICT (one line)

The residual click/static is a **CONSUMER-SIDE under-run on the WEB path**: the AudioWorklet
drains the SAB ring at the real-time audio clock, but the producer (`PumpAudio` inside
`RunOneFrame`) only refills once per rAF frame, so **any main-thread longtask longer than the
currently-buffered depth empties the ring → the worklet zero-pads** (a hard step to 0 and back =
the click). Measured **4.05% of all output frames silence-padded** over a 45 s run; under-run
bursts correlate **1:1 with main-thread stalls**; the adaptive-latency law oscillates (grow→shrink,
never converging) and lets the ring low-water dive to **133 frames (~3 ms)**. Producer output is
**clean** (proven natively) — the bug is entirely on the ring/consumer side, exactly as the brief predicted.

Path: **WEB** (conclusively). Native real-device path is the same mechanism in principle but
**could not be reproduced on this host** (no audio access — see §4). Confidence: **HIGH** for web.

---

## 1. Producer is CLEAN (negative control — bug is NOT producer-side)

Native null-backend mixer dump (`capture_gameplay_audio.py`, 25 s of real gameplay):

```
clip ratio (>0.999): 0.0 %
peak abs          : 0.5134      (limiter holding; nowhere near rail)
rms               : 0.0301
broadband HF      : none (the 08ec1442 decimation static is GONE)
```

The only long silence runs are structural (intro lead-in @frame 0; trailing silence @867840 =
song section). **No clipping, no static, no spurious dropouts in the producer mix.** This isolates
the bug to the ring/consumer side — as the brief says the null backend would (it has no real-time
consumer thread, so it can never show a consumer starvation; it's the right tool to prove the
producer is clean, not to repro the under-run).

Repro: `python3 scripts/native/capture_gameplay_audio.py /tmp/producer.wav --secs 25 --port 8533`

---

## 2. Consumer under-run REPRODUCED + MEASURED (web)

Live server already up on :8421 with a fresh deployed build (release+debug 2026-06-20 04:37–04:38);
the deployed `audio-worklet.js` is **byte-identical** to the pinned engine source (verified). No
rebuild needed.

### 2a. `audio-stall-measure.mjs --play-secs 45`  → the hard numbers

```
underrunEvents = 1523   underrunFrames = 193751   totalFrames = 4783104
underrun rate  = 4.05% of frames silence-padded
ctx sampleRate = 44100   ring bufFrames = 32768   (743 ms depth)
rAF gaps: p50=33.3ms  p90=50.0ms  p99=83.3ms  max=200ms
longtasks (>50ms): 334
```

**Correlation table (decisive — each stall produces a proportional under-run burst):**

| main-thread longtask | under-run burst in the ±2 s window |
|---|---|
| 381 ms @1.37 s | **+519 events / +66432 frames** |
| 201 ms @0.84 s | +346 events / +44288 frames |
| 114 ms @62.79 s | +180 events / +22953 frames |
| 127 ms @71.52 s | +131 events / +16684 frames |
| 125 ms @71.98 s | +131 events / +16684 frames |
| 154 ms @94.79 s | +39 events / +4934 frames |
| 129 ms @105.34 s | +32 events / +4005 frames |

### 2b. Two regimes (from the per-interval timeline of the same run)

- **Boot/song-start burst (t≈2–7 s): ~1009 events / 129,111 frames ≈ 85% of ALL the run's
  under-run frames.** The ring isn't primed when the worklet starts draining, AND the two biggest
  longtasks (381 ms, 201 ms) land here. This is the song-start / transition starvation (scout
  H-SECONDARY) — the single biggest contributor by frame count.
- **Sporadic mid-gameplay bursts** tracking each render/GC/asset-decode longtask (t=65 s +180,
  t=70 s +150, t=99 s +39 …). **These are the residual clicks the user hears during sustained play.**

### 2c. `audio-jitter-profile.mjs --play-secs 35`  → steady-state + the adaptive law's failure

```
STEADY (gameplay) UNDERRUNS: 3.563 events/s,  449.1 frames/s   (sustained, NOT just boot)
RING LOW-WATER (frames): min=133  p5=443  p50=1760  max=5711   ← 133 frames = ~3 ms left in a 743 ms ring
rAF jitter (steady): p50=33.3  p95=50  p99=83.3  max=133.3 ms
CRUX: urIncrements near a GC pause = 0.667  (two-thirds of under-run bumps coincide with GC)
```

The adaptive-latency GROW log shows the target **oscillating and never converging**:
`96→136→138→178→218 ms` … then it shrinks back to ~110 and climbs again `110→150→190`, repeatedly,
for the whole run, while `minDepth` keeps diving (671, 287, **133** frames). The control loop is
chasing its tail.

---

## 3. ROOT CAUSE (exact mechanism, file:line)

Single-producer/single-consumer SAB ring. **Producer cadence is the rAF frame loop; consumer
cadence is the real-time audio clock.** They decouple whenever a frame is long.

1. **Producer = `AudioDevice::PumpAudio()`**, called once per frame from `App::RunOneFrame`
   (`rb3/src/App.cpp:569`, `#ifdef __EMSCRIPTEN__`). On a single-threaded (no-pthread / JSPI)
   wasm build, a long frame (GC pause, WebGPU pipeline compile, `.milo`/DTA parse, JSPI fetch —
   all documented multi-hundred-ms on this build) means **the producer does not run for that
   whole interval.** rAF gaps measured up to 200 ms; longtasks up to 381 ms.

2. **Consumer = AudioWorklet `process()`**
   (`engine/src/platform/web/assets/audio-worklet.js:58`) runs every 128-frame quantum on the
   real audio thread and **must** fill the quantum. When `available < frames` it pads with literal
   zeros:
   ```js
   // engine/src/platform/web/assets/audio-worklet.js:88-92
   // Pad with silence on underrun
   for (let i = toRead; i < frames; i++) { left[i] = 0; right[i] = 0; }
   ```
   That hard step to 0 (and back when data returns) is the audible click. No hold-last / fade.

3. **The adaptive-latency law cannot outrun a per-frame stall.**
   (`engine/src/audio/AudioDevice_Web.cpp:603-738`.) It keeps only `sTargetFrames` queued ahead
   (`AudioDevice_Web.cpp:740-745`), reacts **only after** the worklet's per-0.5 s window reports
   underruns/near-misses (`:660-724`), and **multiplicatively shrinks 25% toward a 50 ms floor on
   every clean window** (`:714-722`, `kShrinkPctNum=25`, `RB3_AUDIO_LAT_MIN_MS` default 50). So it
   structurally sits low, gets caught by the next stall, clicks, grows, shrinks, repeats — exactly
   the grow↔shrink oscillation logged in §2c. A single render frame longer than the *current*
   target depth under-runs before the per-window law can react.

**Net:** consumer drains at 44100 Hz; producer refills once per rAF; stall > buffered-depth ⇒
ring empties ⇒ `audio-worklet.js:88-92` zero-pads ⇒ click. The adaptive law is a bandage that
keeps the buffer too shallow and reacts too late.

---

## 4. NATIVE real-device path — same mechanism, NOT reproducible on THIS host (honest limitation)

The native consumer has the identical shape: producer decodes a few ms once per `RunOneFrame`
(`StandardStream::PollStream`, budget `GetLastMs()*throttle`), consumer drains on miniaudio's
real-time thread, and on starvation **zero-fills** at
`rb3/native/src/rb3_stream_receiver_native.cpp:305-309`. The deep 4 s / 9.1 s ring usually absorbs
steady play, so the native residual is the song-start / screen-transition tail.

**But I could not exercise it here:** this host has `/dev/snd` hardware but
- **no PulseAudio/PipeWire server** (`pactl info` → connection refused), and
- **the process user is NOT in the `audio` group** (PCM devices are `crw-rw---- root audio`;
  only `brltty` is a member), so direct ALSA `hw:` access is denied.

Result: `MILO_AUDIO=1 MILO_AUDIO_BACKEND=alsa` logs **`AudioDevice: ma_device_init failed: -7`**
and the game runs with **no audio device at all** (no real-time consumer thread) — so a native
consumer under-run cannot manifest on this machine. This is an *environment* limit, not a code
fault. A box with PipeWire/Pulse or `audio`-group membership would reproduce it via the native
counter below.

### Native instrumentation I added (DIAGNOSIS-ONLY, UNCOMMITTED, gated off by default)

I added an aggregate consumer under-run counter to
`rb3/native/src/rb3_stream_receiver_native.cpp` (file-static atomics + an `atexit` summary),
gated behind **`RB3_AUDIO_UNDERRUN_LOG=1`** (default OFF, zero behavior change otherwise). It
prints on exit:
`[UNDERRUN-SUMMARY] activeCallbacks=… underrunEvents=… underrunFrames=… minAvailFrames=… maxUnderrunRunFrames=…`.
It is in the working tree but **NOT committed** (per the brief). The implement agent can keep it
(useful when run on a host with real audio) or drop it. NOTE: `atexit` does not fire on SIGTERM —
to read it, let the process exit cleanly or add a SIGTERM trap / live HTTP readout.
Repro driver scaffold: `/tmp/underrun_repro.py` (reuses the canonical nav-to-gameplay script with
`MILO_AUDIO_BACKEND=alsa`).

---

## 5. fixProposal (for the implement agent — ENGINE-side, match-neutral)

All edits are in `engine/src/audio/*` and `engine/src/platform/web/*` (native-only, NOT in the Wii
`.o` set → edit freely; the coordinator bumps `MILO_ENGINE_PIN`). Ranked by ROI:

**A. Consumer-side under-run CONCEALMENT (cheapest, biggest perceptual win, never tried).**
Replace the hard zero-pad in `audio-worklet.js:88-92` (and mirror in
`rb3_stream_receiver_native.cpp:305-309`) with **hold-last-sample + a short (≈1–2 ms) linear
ramp-to-zero** when the ring runs dry, and ramp back up from 0 on recovery. This turns the SAME
dip from an audible step-discontinuity into an inaudible smear *without changing the buffer math* —
it directly attacks the user's symptom. Do this first.

**B. Make the buffer deeper + the law less self-defeating** (attacks the *cause*, not just the symptom):
1. Raise the floor: `RB3_AUDIO_LAT_MIN_MS` default 50 → ~150–200 ms. The p99 stall is 83 ms and
   max 200 ms (§2a) — a 50 ms floor is *below* the stalls it must ride out.
2. Slow / cap the shrink so the law doesn't dive back to the floor between stalls
   (`AudioDevice_Web.cpp:714-722`): bigger `kShrinkMinMs` hysteresis, or only shrink after N
   consecutive clean windows, or never shrink below a "recent worst stall" high-water.
3. **Proactive grow on a detected long frame.** The producer already knows when a long `dt` just
   happened (frame trace). Pre-grow `sTargetFrames` on a detected main-thread stall instead of
   waiting for the worklet's 0.5 s underrun report — the current loop is purely *reactive*
   (`:660-724`) and always a window late.

**C. Song-start prime.** ~85% of the run's under-run frames are the boot/first-frames burst
(§2b). Ensure the ring is primed to the target depth *before* the worklet starts consuming (or
start the worklet only once primed), and bias the initial `kStartMs=120` higher for the first
few seconds. This removes the largest single contributor.

**D. (deep, BLOCKED) off-main-thread mix.** Moving `PumpAudio` off the rAF main thread (wasm
worker / -pthread + SHARED_MEMORY) eliminates the cause entirely, but is blocked on the
JSPI/single-thread build — repeatedly deferred. Do A+B+C first.

Do **not** touch: the resampler (carry-all, clean), the master limiter (exonerated), the decimation
fix — all already landed (see scout-history §3).

---

## 6. reproCommand + the metric to confirm the fix

**Primary metric — web under-run rate (consumer side):**
```bash
python3 native/web/server.py &          # if not already serving :8421
node scripts/web/audio-stall-measure.mjs --port 8421 --play-secs 45
```
Read **`AUDIO UNDERRUN SUMMARY → underrun rate: X% of frames silence-padded`** and the
**CORRELATION** block. Baseline (this diagnosis) = **4.05%** (1523 events / 193751 frames), with
big stalls each producing hundreds of under-run events. **Fix target: well under ~0.5%**, and no
audible-length silence bursts during steady gameplay; the per-interval timeline should show no
sustained mid-gameplay bursts (only the unavoidable first-frames region, which fix C should also
shrink).

**Secondary metric — steady-state low-water + sustained rate:**
```bash
node scripts/web/audio-jitter-profile.mjs --port 8421 --play-secs 35
```
Read **`RING LOW-WATER (frames): min=…`** (baseline **133 ≈ 3 ms**; fix target: min stays well
above one quantum, ideally > the new floor) and **`UNDERRUNS: …/s events, … frames/s`** (baseline
3.56 ev/s, 449 frames/s; fix target → ~0).

**Native (only on a host with real audio access):**
```bash
RB3_AUDIO_UNDERRUN_LOG=1 MILO_AUDIO=1 MILO_AUDIO_BACKEND=alsa <drive to gameplay 40s, exit cleanly>
# read the stderr [UNDERRUN-SUMMARY] line: underrunEvents should be ~0, minAvailFrames high.
```
(Won't run on the current box — no audio group / no Pulse server; see §4.)

**Producer-clean regression guard (must stay clean after any change):**
```bash
python3 scripts/native/capture_gameplay_audio.py /tmp/p.wav --secs 25
# clip 0%, peak < 1.0, no new long silence runs.
```

---

## 7. Confidence

**HIGH** for the WEB path: reproduced, measured (4.05% padded, 1:1 stall↔under-run correlation,
3 ms ring low-water, oscillating adaptive law), and the zero-pad click source is the exact code at
`audio-worklet.js:88-92`. **MEDIUM** that fix A alone fully satisfies the user (it conceals every
dip but B+C are needed to stop the dips during long stalls). Native real-device path is the same
mechanism by construction but **unproven on this host** (environment, not code) — flagged honestly.

## Key anchors
- `engine/src/platform/web/assets/audio-worklet.js:88-92` — **zero-pad on empty ring = the click.**
- `engine/src/audio/AudioDevice_Web.cpp:603-745` — adaptive law (floor 50 ms, 25% shrink, reactive grow, target-depth cap).
- `engine/src/audio/AudioDevice_Web.cpp:740-743` — keeps only `targetFrames` queued; returns if at target.
- `rb3/src/App.cpp:569` — `PumpAudio()` driven once per `RunOneFrame` (producer cadence == rAF).
- `rb3/native/src/rb3_stream_receiver_native.cpp:305-309` — native zero-fill on starvation (mirror fix A here).
- `rb3/native/src/rb3_stream_receiver_native.cpp` (top, `RB3_AUDIO_UNDERRUN_LOG`) — uncommitted native counter I added.
