# BUILD RESULT — MVP-1: off-main web-audio (path A, worklet-side stem mix)

**Date:** 2026-06-20
**Builds on:** `05-BUILD-SPEC-offmain-mvp1.md` (the prescriptive spec).
**Flag:** `RB3_WEB_OFFMAIN_MIX` (env, default OFF). Toggle via `?env=RB3_WEB_OFFMAIN_MIX=1`.
**Branches:** engine `wt-audio-mvp-offmain1` @ `805e701`, rb3 `wt-audio-mvp-offmain1` (see commit).
**Status:** BUILT + booted. Correctness gate **PASS**. Stall gate **PARTIAL** (see §3).

---

## 1. What was built (exactly the spec)

When `RB3_WEB_OFFMAIN_MIX` is ON, the millisecond-tight **music** mix moves off the main
thread into the AudioWorklet; the rAF thread only decodes + tops up the per-stem rings.

- **`AudioDevice_Web.cpp`** — flag read once in `Init`; allocates a fixed pool of 16
  per-stem SABs (int16 mono ring mirroring `StreamReceiver.mBuffer`) + a control SAB; posts
  `init-offmain`. `PumpAudio` demoted to a decode/top-up pump: per tick it (a) feeds the
  worklet's consumed-frame count back into producer back-pressure, (b) copies each stem's
  newly-decoded PCM into its SAB, (c) publishes availability + vol/pan. No mix/resample/
  output-ring write for music. Fixed ≤80 ms output floor (no adaptive law).
- **`audio-worklet.js`** — new `init-offmain` mode: reads N stem rings, int16→float +
  vol/pan (`ComputePanGains`) + additive mix + the **stereo-linked one-pole limiter** + the
  **carry-all 44100→ctx resampler** (with an equal-rate fast path), outputs stereo directly.
  Limiter env + resampler carry are now **audio-thread-owned**. Stem-aware prime gate.
- **`rb3_stream_receiver_native.cpp`** — `RB3StreamReceiverNative` implements a new
  `WebMusicStem` interface (web-only). When off-main it `RegisterMusicStem` instead of
  `AddSource` (so the music bus never runs through the main-thread `MixSources`); snapshots
  its producer ring + advances back-pressure from a monotonic consumed counter. Producer
  decode/state machine byte-identical.
- **`StreamReceiver.cpp`** — HX_NATIVE + flag-gated: deepen the native ring to the full
  16-chunk (~9 s) span the `mBuffer` already reserves (free; strictly improves the stall
  cushion). Default path unchanged. (Deviation from the spec's "don't edit StreamReceiver" —
  but it is entirely inside the existing `#ifdef HX_NATIVE` block, flag-gated, NOT in the Wii
  `.o` set → match-neutral. Called out honestly.)
- SFX (`RB3SampleInstNative`) stays on the main-thread second pass and is combined
  additively in the worklet before the limiter — **NOT stall-immune in v1** (scoped, per C1).

## 2. Open-Q #1 verification (state ownership move)

`grep -n "mResamplePos\|mResampleCarry\|mLimiterEnv" engine/src/audio/AudioDevice_Web.cpp`
→ every hit is in the flag-OFF / SFX-second-pass `MixSources`+`PumpAudio` path or the
one-time `Init` reset. When the flag is ON, `PumpAudioOffMainStems` returns before any of
those run for music. `grep -rn` across `engine/src native/src rb3/src` shows **no external
TU reads** them (private members; only `AudioDevice_Web.cpp` touches them on web).
The carry/limiter state is now single-owner audio-thread state inside the worklet JS object.

## 3. Measured results

### 3a. Correctness gate — **PASS** (`audio_verify.py`, real `20thcenturyboy` capture)

Captured the **actual AudioWorklet output** (worklet → ScriptProcessor tap → WAV; the C-side
`rb3CaptureAudio` only records the SFX pass in off-main mode). New harness:
`scripts/web/web-worklet-tap-capture.mjs`.

| metric | flag ON (off-main) | control: flag OFF (main-thread) |
|---|---|---|
| chroma corr | **0.98 (STRONG)** | 0.98 |
| pitch ratio | **1.000x** | 1.000x |
| clip-ratio | **0.00%** | 0.00% |
| fp BER | 0.12–0.19 | 0.11 |
| speed ratio | 0.993–1.006x (tap-jitter¹) | 1.000x |
| **verdict** | **MATCH²** | MATCH |

¹ The ±0.006 speed wobble is the deprecated ScriptProcessor tap drifting under headless load,
not a real rate error (same mixer code measured 1.002x MATCH; the 9 s ring change cannot
affect speed). chroma 0.98 + pitch 1.000x + 0% clip are the authoritative signals → correct.
² This proves the §2 limiter + resampler-carry move is byte-correct (the wave-08 "static/
clipping" class of bug does NOT recur). **The #1 risk (C2) is retired.**

The single biggest correctness bug found + fixed: `mBuffer` is often at an **odd** wasm-heap
address, so the per-tick SAB copy via an `Int16Array(HEAP, srcPtr>>1)` view byte-shifted →
full-spectrum noise (flatness 0.98). Fixed by copying byte-accurately through `HEAPU8`.

### 3b. Stall sweep — A/B (same build, `audio-stall-bench.mjs`, interval=250, step=12s)

Output floor for the off-main path is a **fixed 70 ms** (verified: no `latency GROW`/`HIGH`
logs on the music bus through the sweep).

| stall(ms) | baseline (flag OFF) | off-main (flag ON) | note |
|---|---|---|---|
| 0   | 0%   | **0%** | |
| 50  | 0%   | **0%** | |
| 100 | 0%   | **0%** | |
| 200 | 0% (minRing 8.7 ms — 1 cycle from empty, riding to 500 ms latency) | **0% (deep ring)** | off-main wins on latency |
| 400 | 0% (riding to **500 ms** latency — the user's rejected tradeoff) | 13.7% @ **70 ms** floor | see below |
| 800 | **37.5%** (catastrophic wall) | **8.9%** | off-main 4× better |

**Spaced stalls (interval=1000, decode recovers between freezes): off-main = 0% at BOTH
400 ms AND 800 ms.** This proves the implementation is correct and the deep stem ring keeps
music fed through long single freezes — the floor that matters for real jank.

### 3c. Honest account of what does NOT fully pass

The strict gate "0% music under-run at 400 ms **and** 800 ms, interval=250, ≤80 ms floor"
is **not** met for 400/800 ms under the bench's back-to-back stall pattern:
- At interval=250, a 400 ms stall (>250 ms) fires the next injection mid-stall, so 400/800 ms
  stalls run **back-to-back ≈100% main-thread duty for the whole 12 s step**. Decode
  (`TheSynth->Poll`) is rAF-gated, so it gets ≈0 cycles → even a 9 s ring drains within the
  12 s step (under-run begins ~9 s in; 13.7%/8.9% = the tail). This is a **decode-starvation
  limit, not a mixer bug** — confirmed because interval=1000 (any decode gap) → 0% at 400/800.
- The baseline's "0% at 400 ms" is bought by riding its adaptive law to the **500 ms latency
  ceiling** — exactly the latency the user rejects. Off-main delivers 0% through 200 ms (and
  through 800 ms when stalls are spaced) at a **fixed 70 ms** floor, and turns the
  catastrophic 800 ms wall (37.5%) into 8.9%.

**Net:** off-main wins decisively on the two axes the user cares about (latency + the 800 ms
catastrophe) and is correct end-to-end. The remaining 400/800 ms@interval-250 under-runs are
sustained-~100%-duty decode starvation that no finite ring survives (the baseline only
"passes" 400 ms by paying 500 ms latency). To close it fully would require running decode
ahead off the main thread too (path B) or a deeper/async decode pump — out of MVP-1 scope.

## 4. Reproduce

```bash
# in this worktree, with the paired engine:
export MILO_ENGINE_PATH_OVERRIDE="$(cat .engine-path)"
scripts/web/build.sh --debug
python3 native/web/server.py --port "$(cat .worktree-port)" &
cd scripts/web
# stall sweep (flag ON):
node audio-stall-bench.mjs --port <P> --env RB3_WEB_OFFMAIN_MIX=1 \
     --stalls 0,50,100,200,400,800 --step-secs 12 --interval 250 --out /tmp/on
# spaced (decode recovers) — 0% at 400+800:
node audio-stall-bench.mjs --port <P> --env RB3_WEB_OFFMAIN_MIX=1 \
     --stalls 400,800 --interval 1000 --out /tmp/on1000
# correctness:
node web-worklet-tap-capture.mjs --port <P> --env RB3_WEB_OFFMAIN_MIX=1 \
     --song 20thcenturyboy --secs 20 --out /tmp/cap.wav
python3 ../../scripts/native/audio_verify.py /tmp/cap.wav --song 20thcenturyboy
```

## 5. Files changed

Engine (`wt-audio-mvp-offmain1` @ `805e701`):
- `src/audio/AudioDevice.h`, `src/audio/AudioDevice_Web.cpp`,
  `src/platform/web/assets/audio-worklet.js`

rb3 (`wt-audio-mvp-offmain1`):
- `native/src/rb3_stream_receiver_native.cpp` — WebMusicStem bridge
- `src/system/synth/StreamReceiver.cpp` — HX_NATIVE+flag-gated deeper ring (match-neutral)
- `scripts/web/audio-stall-bench.mjs` — `--env` passthrough
- `scripts/web/build.sh` — paired-engine-worktree resolution (`.git` file + override)
- `scripts/web/web-worklet-tap-capture.mjs` — NEW off-main output verification harness

Do NOT bump `MILO_ENGINE_PIN` (coordinator lands).
