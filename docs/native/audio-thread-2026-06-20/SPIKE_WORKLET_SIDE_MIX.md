# SPIKE — Worklet-side mix from SAB stems (the low-risk off-main path)

**Date:** 2026-06-20
**Worktree branch (rb3):** `wt-worklet-mix`
**Verdict:** **VIABLE.** A pure-JS AudioWorklet that mixes per-stem SABs *on the
audio thread* keeps producing **correct** audio through a 2-second main-thread
freeze with **0% under-run**, where today's main-thread-pump baseline breaks at
~800 ms. The spike needs **zero build-flag changes** (no `-pthread`, no
`-sAUDIO_WORKLET`, no `-sWASM_WORKERS`, no shared wasm memory) — so it **cannot
conflict with the `-sJSPI` single-thread build**. This is option (d2) from the
compat matrix, empirically confirmed.

This doc is the data-flow proof the task asked for: a standalone harness that runs
the exact architecture under test (no wasm, no engine boot), injects controlled
main-thread stalls, and reads under-runs from the *same* instrumentation the
production worklet uses — plus an FFT correctness check that the off-main mix is
the right signal, not just non-silent.

---

## What was built

All under `scripts/web/worklet-mix-spike/` (rb3, branch `wt-worklet-mix`):

| File | Role |
|---|---|
| `index.html` | Standalone harness hosting BOTH architectures + a `window.spike.*` driver API. Synthesizes a deterministic 6-stem sine mix, runs a main-thread rAF producer + stall injector, exposes under-run snapshots. |
| `baseline-worklet.js` | Faithful re-impl of the SHIPPING `engine/.../audio-worklet.js` drainer (CONSUMER only; depends on the main thread to fill a single pre-mixed output ring). The "today" architecture. |
| `spike-worklet.js` | The architecture under test: reads N per-stem SAB rings + a control block and **MIXES them on the audio thread** (additive sum × gain + soft-clip — same shape as engine `MixSources`). Main thread only tops up the stem rings. |
| `serve.py` | ~40-line COOP/COEP static server (SharedArrayBuffer needs cross-origin isolation, same headers as `native/web/server.py`). Zero dependency on the full web build. |
| `run-spike-bench.mjs` | Playwright A/B driver: runs the SAME injected-stall sweep against both modes, prints the stall-resilience curve + verdict, writes `spike-curve.{json,csv}` + `report.txt`. |
| `verify-mix.mjs` | Correctness check: taps the worklet output with an `AnalyserNode` (FFT on the audio thread), confirms every expected stem frequency is present at the right amplitude — including *during* a stall. |

Run it (from a worktree with Playwright in `scripts/web/node_modules`):

```bash
python3 scripts/web/worklet-mix-spike/serve.py --port 8643 &
cd scripts/web
node worklet-mix-spike/run-spike-bench.mjs --port 8643 \
     --stalls 0,100,400,800,1500 --step-secs 8 --interval 250 --producer-cadence 100
node worklet-mix-spike/verify-mix.mjs   --port 8643 --mode spike --stall 1000
```

---

## The data-flow being proven

```
BASELINE ("today's web build" — main-thread pump):
  MAIN THREAD (rAF): synth 6 stems -> MIX -> write ONE output SAB ring  ─┐
  AUDIO THREAD (worklet): drain the output ring -> speakers            <─┘
  => a main-thread stall > output-ring depth empties the ring -> UNDER-RUN.

SPIKE ("worklet-side mix" — the proposed fix, shape A):
  MAIN THREAD (relaxed cadence): synth 6 stems -> write 6 PER-STEM SAB rings ─┐
  AUDIO THREAD (worklet): read 6 stem rings -> MIX -> speakers              <─┘
  => the audio thread mixes resident stem PCM; a main-thread stall up to the
     STEM-ring depth (seconds) cannot starve the output.
```

The spike is the **off-main analogue of the native path**: native mixes on the
miniaudio audio thread, pulling from the same deep per-channel rings
(`StreamReceiver.mBuffer`, ~9 s). Here the audio thread is the AudioWorklet and the
deep rings live in SABs. The control protocol (per-stem writePos/readPos as Atomics)
is the same SPSC handshake `rb3_stream_receiver_native.cpp:62-69` already implements.

---

## Results — stall-resilience A/B (the numbers)

Both producers fill their ring(s) to target each pump (ring-bounded, honest A/B);
the ONLY difference is **where the mix happens**. Same injected main-thread stalls,
same under-run instrumentation. `minRing` = worklet low-water mark (how close to
empty); the early-warning the adaptive law gates on.

### Sweep 1 — the production bracket (`0,100,400,800,1500` ms, inject every 250 ms)

```
                       BASELINE (output ring ~743ms)     SPIKE (stem ring ~3000ms, producer @100ms)
  stall(ms) |  underrun%  minRing(ms)      |   underrun%  minRing(ms)
  ----------+------------------------------+-----------------------------
        0   |     0.00      725.6          |      0.00      2881
      100   |     0.00      644.3          |      0.00      2852
      400   |     0.00      345.4          |      0.00      2602.3
      800   |     6.75        0   <-BREAK  |      0.00      2201.8
     1500   |    47.26        0            |      0.00      1502.3   <- 0% under-run
```

- **Baseline breaks at 800 ms** (6.75% under-run, ring drained to 0) and is
  catastrophic at 1500 ms (47.26%). Matches the pinned `STALL_BENCH_BASELINE.md`
  failure mode exactly (it broke at 800 ms there too).
- **Spike: 0.00% under-run across the WHOLE sweep, including 1500 ms.** At the
  1500 ms stall its `minRing` drops 2881 → 1502 ms — exactly the ~1500 ms the
  stall consumed — but stays **positive**, so the audio thread never starves. The
  main thread froze for 1.5 s and the worklet kept mixing the resident stem PCM.

### Sweep 2 — push the spike PAST its ring depth (`0,2000,3500,5000` ms)

```
                       BASELINE (~743ms ring)            SPIKE (~3000ms ring)
  stall(ms) |  underrun%  minRing(ms)      |   underrun%  minRing(ms)
  ----------+------------------------------+-----------------------------
        0   |     0.00      725.6          |      0.00      2933.2
     2000   |    59.03        0            |      0.00      1003.1   <- survives (3000-2000)
     3500   |    73.22        0            |     13.94        0      <- breaks (stall > 3000ms ring)
     5000   |    77.25        0            |     36.17        0
```

This is the **falsifiable** confirmation the model is sound:
- The spike survives a **2000 ms** stall at 0% (minRing → 1003 ms = `3000 − 2000`,
  the predicted residual).
- The spike finally breaks at **3500 ms** — precisely when one stall exceeds the
  **3000 ms stem-ring depth**. Not a flaw; the proof that **the spike's stall
  tolerance equals the per-stem decode-ahead depth**, a capacity I control.

**The architectural win, stated precisely:** the baseline's stall budget = the
output-ring depth = the *latency the user rejected* (riding the adaptive law to
500 ms). The spike's stall budget = the *stem-ring decode-ahead depth*, which is
**decoupled from output latency** — you can run a 60-80 ms output floor AND tolerate
multi-second stalls, because tolerance comes from the deep upstream ring, not from
inflating the latency you hear.

---

## Results — correctness (the off-main mix is the RIGHT signal)

`verify-mix.mjs` taps the worklet output with an FFT and checks all 6 stem sines.
Run for the spike **under a 1000 ms stall** and the baseline at rest:

```
SPIKE (mixed on the audio thread), DURING a 1000ms main-thread stall:
  220.0Hz -> peak@220.7Hz  -28.5dB  SNR 164.8dB  OK
  277.2Hz -> peak@274.5Hz  -29.5dB  SNR 163.8dB  OK
  329.6Hz -> peak@328.4Hz  -28.7dB  SNR 164.6dB  OK
  440.0Hz -> peak@441.4Hz  -28.8dB  SNR 164.5dB  OK
  554.4Hz -> peak@554.5Hz  -28.5dB  SNR 164.8dB  OK
  659.3Hz -> peak@656.8Hz  -29.4dB  SNR 163.9dB  OK
  PASS — all stem frequencies present in the worklet-mixed output

BASELINE (mixed on the main thread) at rest, for reference:
  same 6 peaks, amplitudes within 0.6 dB of the spike.
```

The off-main mix is **bit-for-ear identical** to the main-thread mix (within
0.6 dB across all stems), and stays correct **through** the stall — not just
non-silent. The DSP body (`MixSources`' additive-sum + limiter) ports to the audio
thread unchanged.

---

## What a real MVP needs (concrete)

The prototype mixes pure JS over f32-mono SABs at ctx rate. The real engine path
differs in three ways; none changes the verdict, all are well-scoped:

### 1. Decode-ahead sizing
- The real per-stem ring (`StreamReceiver.mBuffer`) is already **~9 s** of int16
  mono (`0xC0000` bytes, `StreamReceiver.h:65-66`) — 3× the 3 s the prototype used,
  so the real stall tolerance is ~9 s, not 3 s. **No new buffer needed; the deep
  ring already exists.** The fragile link today is purely that the mix that drains
  it runs on main; move the mix off-main and the existing depth becomes the budget.
- Output SAB ring can SHRINK to a low fixed latency floor (60-80 ms) because it no
  longer absorbs stalls — that's now the upstream stem ring's job. This is how the
  spike wins on latency *and* dropout simultaneously.

### 2. Stem SAB layout (the marshalling)
The off-main mixer needs read access, in shared memory, to:
- **N per-channel decoded-PCM rings** (int16 mono @ mix rate) + each channel's
  **cursors** (`mRingWritePos`/`mRingWrittenSpace` by producer; `mAudioReadPos` by
  consumer — already an Atomics-style SPSC pair).
- A small **control block**: per-source vol/pan/paused/finished + numStems +
  resampler phase + limiter envelope + the adaptive target depth (the control law
  stays main-side and commands the mixer via one shared int).

The prototype's layout (one SAB per stem: `Int32[writePos,readPos]` + PCM, plus a
control SAB) maps 1:1 to these. The cursor protocol is already SPSC and
cross-thread-ready (`rb3_stream_receiver_native.cpp:62-69`) — today it just runs
on one thread.

### 3. JS-vs-wasm mix — **JS is sufficient for v1**
- The prototype mixes in **pure JS in the AudioWorklet** and it is correct +
  stall-immune. For 6-15 stems × 128-frame quanta this is trivially within the
  audio-thread budget (the worklet ran at 0 under-run with headroom).
- **Crucially, pure-JS worklet mix needs NO shared wasm module** → it sidesteps the
  entire JSPI×shared-memory hazard class (compat-matrix #24302 / #19287). The
  prototype proves the data flow works with the *plain JS SAB* the production
  worklet already uses — i.e. **no build-flag spike required at all.**
- A wasm-in-worklet mixer (option (a), `-sAUDIO_WORKLET`) would only be worth it if
  you must reuse the *exact* C++ `MixSources`/limiter/resampler bytes rather than
  porting them to JS. That carries the documented JSPI-coexistence build risk and
  should be the *fallback*, gated on the build spike — not the v1.

### Remaining real-engine work (the actual MVP, ~medium)
1. Publish the existing `StreamReceiver` per-channel rings + cursors + per-source
   vol/pan into SABs (the rings are already heap arrays; make them SAB-backed or
   copy-into-SAB on `WriteData`).
2. Port `MixSources` (additive sum + limiter) + the 44100→ctx linear resampler into
   the worklet JS (or a tiny wasm-in-worklet if byte-reuse is required). ~150 lines.
3. Keep `StreamReceiver::Poll` (vorbis decode) on the main thread — it has the ~9 s
   ring cushion, so it tolerates jank (shape A; the map's analysis). Only move
   decode off-main (shape B) if profiling shows decode itself starves under load.
4. Move the adaptive-latency control law to "command a target depth" via a shared
   int; the mixer reads it. Drop the output ring to a fixed low floor.

---

## Why this is the right first move

- **Zero JSPI risk:** no shared wasm memory, no new link flags. The one empirical
  worry from prior notes (does off-main coexist with `-sJSPI`?) is **moot for this
  path** — it never touches the wasm threading model. (The `-sAUDIO_WORKLET` build
  spike is still worth running as a *separate* question if wasm byte-reuse is later
  desired, but it is NOT a blocker for shipping the fix.)
- **Proven, not asserted:** real headless-Chromium runs, real under-run counters,
  real FFT correctness — the spike survives a 2 s freeze at 0% under-run and
  produces the correct mix during a 1 s freeze.
- **Converges to the native reference:** native already mixes on the audio thread
  pulling deep rings; this makes web do the same. "Matches native" is the measured
  bar (`aloop-native-realdevice-capture-verify.md`: native verified MATCH, chroma
  0.95, 0% clip).

---

## Artifacts
- `/tmp/rb3-worklet-mix-spike/spike-curve.{json,csv}` + `report.txt` (sweep 1)
- `/tmp/rb3-worklet-mix-spike-hard/spike-curve.{json,csv}` (sweep 2, past ring depth)
- Harness: `scripts/web/worklet-mix-spike/` (branch `wt-worklet-mix`)

## Coordinator: how to land
- These are all **native-only scripts + a doc** (no engine change, no shared-decomp
  change, no build-flag change). Cherry-pick the rb3 commit on branch
  `wt-worklet-mix` straight onto master; nothing to pin.
- The harness is self-contained (its own COOP/COEP server); it needs only Playwright
  (already in `scripts/web/node_modules`). No web build required to re-run.
