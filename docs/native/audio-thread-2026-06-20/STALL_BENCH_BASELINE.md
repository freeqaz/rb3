# Web audio stall-resilience benchmark — BASELINE (current main-thread-pump architecture)

**Date:** 2026-06-20
**Tool:** `scripts/web/audio-stall-bench.mjs`
**Architecture measured:** main-thread `PumpAudio` (once per `requestAnimationFrame`) +
SAB ring + AudioWorklet consumer, with the **140 ms adaptive latency floor** (engine
`884ab17`, the SHA rb3 currently pins). This is the band-aid build the user is "done with".
**Purpose:** the objective A/B yardstick. This curve is the **baseline every
off-main-thread MVP must beat.**

---

## What the tool does

Loads the web build, drives boot → song_select → `game_screen` (real MOGG stream for
`20thcenturyboy`), then **actively injects controlled main-thread longtasks** and reads
the under-run rate straight from the worklet's instrumentation.

- **Injection:** a self-rescheduling `requestAnimationFrame` hook busy-spins the main
  thread (`while (Date.now() < until)`, no yields) for `stallMs`, no more often than
  every `intervalMs`. Because the engine's `PumpAudio` is driven from the same rAF loop
  (`App::RunOneFrame`), the spin withholds exactly the cycle that would have refilled the
  ring — i.e. it reproduces the real-world jank (GC pause / shader compile / asset decode)
  that starves the producer.
- **Measurement:** per sweep step the under-run counters are **differenced over the
  window only** (`window._rb3Audio.underruns`, posted by `audio-worklet.js` ~every 0.5 s),
  and the injector is reset to 0 between steps with a 3 s recovery gap.
- **Outputs the stall-resilience curve:** under-run %, under-run events/s, worst audible
  gap (ms, from the largest per-poll under-run-frame burst), worklet **min ring depth**
  (the low-water mark — the early-warning the adaptive law gates on), and the measured
  rAF p99/max (sanity that the injected stall actually landed).

### Run command (exact)

```bash
# in a worktree (or main repo) that has built the web bundle:
scripts/web/build.sh --debug                      # deploys build/debug/
python3 native/web/server.py --port 8686 &        # serve the build
cd scripts/web && node audio-stall-bench.mjs \
    --port 8686 --stalls 0,50,100,200,400,800 --step-secs 12 --interval 250 \
    --out /tmp/rb3-audio-stall-bench
# (defaults to the debug build via /?debug=true; pass --release if you built release/)
```

Artifacts: `/tmp/rb3-audio-stall-bench/curve.{json,csv}` + the stdout table below.

> Note: Playwright resolves from `scripts/web/node_modules`, so run the script with
> `cd scripts/web` (the main repo's copy is fine — the script only needs a running
> server on `--port`).

---

## BASELINE CURVE (the numbers to beat)

```
ctxRate=44100Hz  ring=32768 frames (~743ms)  step=12s  inject-interval=250ms  song=20thcenturyboy

  stall(ms) | underrun% | events/s | worstGap(ms) | minRing(ms) | rafP99 | rafMax | injected
  ----------------------------------------------------------------------------------------
         0  |     0.00  |     0.0  |           0  |       59.8  |  16.7  |  33.3  | 0x/0ms
        50  |     0.00  |     0.0  |           0  |       70.3  |    50  |  66.7  | 48x/2405ms
       100  |     0.00  |     0.0  |           0  |       23.9  |   100  |   100  | 48x/4800ms
       200  |     0.00  |     0.0  |           0  |       15.3  | 216.7  | 216.7  | 48x/9600ms
       400  |     0.00  |     0.0  |           0  |       73.3  | 433.3  | 433.3  | 29x/11600ms
       800  |    38.43  |   100.0  |         880  |        0.8  | 816.6  | 833.3  | 15x/12000ms
```

(Machine-readable: `curve.csv` / `curve.json` in the out dir.)

---

## Reading the curve — what it proves

**1. The architecture survives 0–400 ms stalls only because of a huge 743 ms ring +
an adaptive law that's already pinned at its 500 ms ceiling.**
Under-run % is 0 up to 400 ms, but that is *not* comfortable headroom — the `minRing`
low-water mark tells the real story:

- At **200 ms stall**, the ring drains to **15.3 ms** of buffered audio (one bad cycle
  from empty). The console shows the adaptive law frantically growing the target
  `200 → 260 → 292 → 352 → 412 → 472 → 500 ms` (its ceiling) and logging
  `latency HIGH ~500 ms (near ceiling) — holding ring headroom`.
- **This high latency is exactly the user's complaint.** Surviving a 200 ms stall costs
  ~half a second of audio latency — unacceptable for a rhythm game. The "0% under-run"
  at 200/400 ms is bought with the very latency we're trying to eliminate.

**2. The hard wall is at 800 ms: 38.43 % of frames silence-padded, 100 under-run
events/s, an 880 ms worst-case audible gap, ring drained to 0.8 ms.**
Once a single main-thread stall exceeds the ring depth, the producer cannot catch up and
audio breaks catastrophically. An 800 ms main-thread freeze is not exotic — a GC pause, a
WebGPU pipeline compile, or a large asset decode hit easily produces one.

**3. The curve is history-dependent (a fragility in itself).**
`minRing` is non-monotonic (400 ms shows 73.3 ms but 200 ms shows 15.3 ms) because the
adaptive latency law **carries state between steps** — by the 400 ms window it had already
grown the target to the 500 ms ceiling, so it entered with more headroom. The architecture's
resilience depends on *prior* jank having already inflated the buffer. Resilience that
depends on having recently glitched is not resilience.

### One-line summary

> The current main-thread-pump architecture holds 0 % under-runs only up to ~400 ms
> single-stalls, and only by riding its adaptive latency law up to the **500 ms ceiling**
> (the latency the user is rejecting). A single **≥ 800 ms** main-thread freeze produces
> **38 % dropout / 880 ms audible gap**. The injection clearly lands (rafP99/max track the
> injected stall 1:1; 15–48 injections/window).

---

## The bar for an off-main-thread MVP

A producer that runs **off the main thread** (Audio Worklet / Wasm Worker / pthread feeding
the SAB ring) does not depend on `requestAnimationFrame`, so injected main-thread stalls
**cannot** starve it. The MVP must demonstrate:

- **~0 % under-run across the WHOLE sweep, including 800 ms** (the baseline breaks here).
- It can do so **at a low, fixed latency floor** (e.g. back down near 60–80 ms) instead of
  riding the adaptive law to 500 ms — i.e. win on *both* dropout **and** latency.

Re-run the identical command against any candidate build and compare `curve.csv` directly.

---

## Caveats / notes for the next agent

- **Sweep choice:** `0,50,100,200,400,800` with a 250 ms inject-interval is the sweet spot
  that brackets the break (clean ≤400, catastrophic at 800). Tighten `--interval` (e.g.
  `--interval 120`) to make back-to-back stalls outpace refill at *smaller* stall sizes if
  you want to stress a candidate harder without a single 800 ms spike.
- **Recovery between steps** is 3 s; the adaptive law still carries latency state across
  steps (see history-dependence above). For a clean per-step comparison of a *fixed-floor*
  candidate this matters less; for the current adaptive build it explains the non-monotonic
  `minRing`.
- **worstGap(ms)** is a conservative proxy: the largest single-poll jump in
  `underrunFrames` (poll window ~0.25–0.5 s) converted to ms. A real contiguous gap is
  `≤` this. It reads 0 whenever under-run % is 0 (no padded frames = no gap).
- The bench loads the **debug** build by default (`/?debug=true`) because
  `build.sh --debug` only deploys `build/debug/`. Debug is `-O0 -g2`; absolute rAF cost is
  higher than release, but the *injected* stall dominates and the comparison is apples-to-
  apples as long as MVP and baseline are both measured at the same opt level.
```
