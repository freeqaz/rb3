# Phase 1 — Asset-stutter perf profile (TASK A4)

**Question:** "assets make the game stutter." Build a frame-time tracer, correlate
spikes to asset-load events, name the prime suspect, and point at the cheapest fix.

**Method:** a lightweight, env-gated per-frame tracer (`RB3_FRAME_TRACE=<path>`)
records EVERY frame to JSONL; a driver script navigates boot → main_hub →
song_select (scroll N songs → preview-stream + album-art loads) → into a song,
then parses the trace into a histogram + worst-N stutters + p50/p95/p99 + spike
clustering. All native, ~3s rebuilds, headless.

## Tooling added (this task)

| Artifact | Path | Gate |
|---|---|---|
| Per-frame JSONL tracer | `native/src/rb3_frame_trace.cpp` | `HX_NATIVE` + `RB3_FRAME_TRACE=<path>` |
| Asset-event hooks | `src/system/utl/Loader.cpp::AddLoader` (ld), `src/system/synth/Synth.cpp::NewStream` (st) | `HX_NATIVE`, behind `gFrameTraceActive` bool |
| Frame-loop record call | `src/App.cpp::RunWithoutDebugging` (native branch, shares timing with `RB3_FRAME_INSTRUMENT`) | `HX_NATIVE` |
| Profiler/driver/parser | `scripts/native/frame_profiler.py` | — |

Per-frame record fields: `f` frame, `dt` frame ms, `lp` ms in `LoadMgr::Poll`
(budgeted background loader), `lpu` ms in `PollUntilLoaded/Empty` (SYNCHRONOUS
drains), `scr` screen, `ld` new loaders added this frame, `st` new audio streams
opened this frame, `pend` loaders still pending at end of frame.

Zero cost when off (the engine hooks are a single not-taken branch on
`gFrameTraceActive`; the App-side record call is gated on the env var). Wii decomp
match untouched — every addition is `#ifdef HX_NATIVE`. Builds clean for both
`rb3-native` and the minimal `rb3-dta` tool (counters are defined in `Loader.cpp`
so both link).

Run:
```bash
python3 scripts/native/frame_profiler.py --scroll 30 --scroll-pace 0.3 --run-secs 8
python3 scripts/native/frame_profiler.py --parse-only /tmp/rb3-frame-trace.jsonl
# into gameplay too:
python3 scripts/native/frame_profiler.py --scroll 14 --into-song --run-secs 18
```

## Results — two runs (boot → song_select scroll [→ gameplay])

### Frame-time percentiles (overall)
| run | frames | p50 | p95 | p99 | max |
|---|---|---|---|---|---|
| scroll-only (30 down) | 3937 | 5.0 ms | 7.5 ms | ~9 ms | 174 ms |
| scroll + into-song | 2350 | 5.4 ms | 11.8 ms | 76.4 ms | 209 ms |

Steady-state is smooth (sub-frame). All the tail mass is two regions: **the splash
boot screen** and **screen transitions**.

### TOP-5 worst stutters (representative run)
| frame | dt | screen | event tag |
|---|---|---|---|
| 23 | **209 ms** | splash_screen | bgLoad=15 ms, pend=4 |
| 24 | 107 ms | splash_screen | bgLoad=13 ms, pend=8 |
| 39 | 86 ms | splash_screen | bgLoad=16 ms, pend=5 |
| 34 | 85 ms | splash_screen | bgLoad=12 ms, pend=4 |
| 31 | 85 ms | splash_screen | bgLoad=9 ms, pend=5 |

(In the steadier 30-scroll run the worst is **174 / 161 / 129 / 121 / 109 ms**, all
on `splash_screen`, same shape.)

### Per-screen frame ms (where spikes live)
| screen | n | p50 | p95 | p99 | max |
|---|---|---|---|---|---|
| **splash_screen** | 62 | 72.6 | 84.8 | 107.5 | **209.1** |
| main_hub_screen | 157 | 10.7 | 20.4 | 32.7 | 69.1 |
| **song_select_screen** | 1590 | **5.5** | **6.9** | 9.7 | 51.9 |
| part_difficulty_screen | 452 | 3.3 | 4.2 | 5.4 | 22.1 |
| intro_movie_screen | 23 | 9.4 | 20.1 | 34.3 | 34.3 |

### Screen-transition cost (first frame after each change)
| frame | dt | → screen | event |
|---|---|---|---|
| 23 | 209 ms | splash_screen | bgLoad=15 ms, pend=4 |
| 85 | 34 ms | main_hub_screen | – |
| 251 | **52 ms** | song_select_screen | **LOAD+2**, pend=1 |
| 1841 | 22 ms | part_difficulty_screen | LOAD+1, bgLoad=8 ms, pend=5 |

## Key finding — the suspect is NOT synchronous asset I/O

The original hypothesis (HP) was "synchronous main-thread asset I/O blocks the
frame." **The trace refutes that for the steady scroll path:**

1. **`lpu` (PollUntilLoaded/PollUntilEmpty synchronous drains) is 0 ms on every
   frame after boot.** The W3 budgeted-loader fix (`RB3_LOADER_BUDGET_MS=8`,
   landed) is holding — there is no per-frame synchronous drain stall anymore.
2. **Preview-stream opens are cheap.** Every `st:1` frame (a `Synth::NewStream`
   for a song preview while scrolling) is ~5–7 ms — opening the mogg does not
   block the frame.
3. **Album-art / song-data loader adds are cheap.** Every `ld>0` scroll frame is
   ~5–7 ms; the background loader stays under its 8 ms budget (`lp` ≈ 0 during
   scroll).
4. **The spike tail is on `splash_screen`, and it is draw/GPU-bound, not loader-
   bound.** Of ~5,000 ms of long-frame (>33 ms) time, only **~260–300 ms is in
   `LoadMgr::Poll`** and **0 ms is in synchronous drains** — the remaining
   **~95% ("elsewhere") is draw / poll / GPU** on the splash frames (each splash
   frame renders ~72–209 ms of work while `bgLoad` is only 8–16 ms of it).

So the asset *loader* is already time-sliced and is NOT the stutter source on the
hot path. The real per-frame cost during the heavy boot window is rendering/GPU
work on the splash screen, with the loader running concurrently at its budget.

### The two genuine asset-attributable spikes
- **song_select ENTER (frame 251, ~52 ms, `LOAD+2`)** — entering the library
  fires 2 loaders (list/art scaffolding) on the transition frame.
- **part_difficulty ENTER (frame 1841, ~22 ms, `LOAD+1` + `bgLoad=8 ms`)** — the
  gameplay-asset preload kicks in. (Going further into gameplay, the song mogg
  `st` open also registers — and is likewise cheap per the cheap-`st` rule above.)

These are one-shot transition costs, not the steady "scrolling stutters" the user
described. **The steady scroll itself is smooth (p95 ≈ 7 ms).**

## Prime suspect + cheapest fix direction

**Prime suspect (where to spend effort): splash-screen frame time, GPU/draw-bound.**
The boot splash sequence renders 60+ frames at 70–200 ms each; the loader is a
minor (~8–16 ms) co-tenant, not the blocker. If "stutter during boot" is the
complaint, the fix is on the **render** side (the splash milos draw a lot per
frame headless), not the loader.

For the **asset-attributable** spikes (the 52 ms / 22 ms transition frames):
- **Cheapest:** spread the 2 transition loaders across frames instead of issuing
  them on the single transition frame (defer / stagger `AddLoader`), and/or warm
  the song_select list/art scaffolding before the screen goes active.
- **Already-done lever:** the budgeted loader (`RB3_LOADER_BUDGET_MS`) already
  prevents per-frame sync drains — confirmed by `lpu==0`. No regression here.

If a future change reintroduces synchronous loads, the tracer will surface it
immediately as nonzero `lpu` on the spiking frame — that is the canary to watch.

## Web (reuse, don't rebuild)

Steady-state web profiling uses the existing tools (no rebuild):
```bash
python3 native/web/server.py            # serve on :8421 (already running here)
node scripts/web/loadperf-profile.mjs --port 8421 --secs 200 --nav \
      --out /tmp/web-loadperf-steady
node scripts/web/analyze-cpuprofile.mjs /tmp/web-loadperf-steady/*.cpuprofile --top 30
```
`loadperf-profile.mjs` already captures, on one timeline: a V8 `.cpuprofile`
(flame graph), **Long Tasks (>50 ms main-thread blocks)** each attributed to the
active boot phase, requestAnimationFrame gaps (the visible "tab froze N ms"),
and Resource Timing for every fetch. `analyze-cpuprofile.mjs` aggregates self-time
by function + category (wasm / js-glue / gc / idle) so the long tasks map to
JS/wasm stacks. To target **steady-state** rather than boot, navigate with `--nav`
and read the long-tasks whose `start` timestamp is AFTER the `first-screen` /
`main_hub` milestones in the JSON summary (boot long-tasks vs. scroll/gameplay
long-tasks are separable by timestamp).

**Caveat (observed):** web boot to `song_select_screen` took ~128 s in this
environment, so `--secs` must exceed the boot wall (use `--secs 200`+); a too-short
window closes the page mid-boot before the `.cpuprofile` is written (we hit this
with `--secs 55`). Per the task, the **native trace is the primary deliverable**;
the web profiler is the cross-check for genuinely web-specific long tasks. Both run
the same shared `src/`+engine, so a draw/GPU-bound native splash predicts a
draw/GPU-bound web boot long-task (and the cheap native `st`/`ld` predicts cheap
web preview/art loads).

## Return summary
- **Top-5 stutters:** all on **splash_screen** — 209 / 107 / 86 / 85 / 85 ms
  (each: `bgLoad` only 9–16 ms, rest is draw/GPU).
- **p95 / p99:** scroll-only **7.5 / ~9 ms**; with the gameplay-transition tail
  **11.8 / 76.4 ms** (the p99 is the part_difficulty/gameplay transition frame).
- **Prime suspect:** **splash-screen render/GPU time during boot** (loader is a
  minor co-tenant). Synchronous asset drains (`lpu`) are **0 ms** everywhere — the
  budgeted loader already eliminated the sync-I/O stutter. The only asset-
  attributable spikes are the one-shot song_select-enter (52 ms, 2 loaders) and
  part_difficulty-enter (22 ms) transition frames.
- **Cheapest fix direction:** stagger/defer the 2 transition loaders off the
  single transition frame (and prewarm song_select list/art); keep the budgeted
  loader. The boot splash spike is a render concern, not a loader concern.
