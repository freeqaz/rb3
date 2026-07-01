# Track C — the ~295 ms song-start GPUTask, ATTRIBUTED + FIXED (2026-06-20)

**Role:** Track C of the frame-stall workflow. The audio reviewer surfaced a
~295 ms "GPUTask" longtask at t≈1.44 s that survived the console-log fix and
exceeds the audio ring depth — a real song-start under-run risk, left
UNATTRIBUTED. This doc attributes it precisely and lands a fix.

**Engine worktree branch:** `wt-fstall2-gputask` @ `7ee6850` (milo-native-engine).
**rb3 worktree branch:** `wt-fstall2-gputask` @ `eef0da1` (harnesses only).
Pin NOT bumped, nothing pushed (per workflow rules).

---

## TL;DR

The "GPUTask" is **`PipelineManager::PreWarm()`** compiling all **240 enumerated
draw-time render pipelines in a single Dawn wire flush**, run one-shot from
`BandRnd::BeginFrame` on the first rendered frame (~rAF frame 8, very early — the
t≈1.44 s the reviewer saw is boot/first-render, where menu/preview audio is
already playing). On web that one flush compiles on the GPU process as **ONE
465–550 ms GPUTask** — the single largest GPU longtask in the entire
boot→game_screen session (next-largest is ~18 ms; it is 22 % of all GPU time in
one frame). It dwarfs the ~90–140 ms audio ring depth → guaranteed under-run.

**Fix (landed):** chunk the pre-warm — create a bounded **count** of pipelines
per frame (default 12, `RB3_PIPELINE_PREWARM_PER_FRAME`) so each per-rAF flush
carries only that chunk and the GPU compiles ~12 at a time instead of all 240.

**Measured (web debug, real Vulkan, `20thcenturyboy`, env-flag A/B on ONE build):**

| variant | biggest GPUTask | GPUTasks >100 ms | how the 240 compiles flush |
|---|---|---|---|
| one-shot (`RB3_PIPELINE_PREWARM_NOCHUNK=1`) | **464.9 ms** | 1 | 131+58+51 in ~2–3 flushes |
| **chunked (default, 12/frame)** | **79.9 ms** | **0** | 26 flushes of exactly 12 |

Visual no-op (identical pipeline set, only *when* created — game_screen renders;
screenshot verified). Native/web-only gfx backend; Wii match surface untouched.

---

## How it was attributed (method — reproducible)

The standard frame-stall harnesses (`_framestall-songstart.mjs`) trace only
`devtools.timeline` + `v8` — they do **not** capture the GPU process. So a GPUTask
is invisible to them. New tool:

**`scripts/web/_framestall-gputrace.mjs`** — same boot→hub→song_select→
part_difficulty→game_screen nav, but `Tracing.start` adds the GPU-process
categories `gpu` / `disabled-by-default-gpu.dawn` / `disabled-by-default-gpu.device`
/ `toplevel`. `--whole-session` arms the trace at page load (not at song-kick) so
it catches the one-shot burst that fires ~frame 8. `--env 'RB3_*=...'` passes
flags through the `?env` bridge. `--min-ms`, `--out`.

```bash
# server on the worktree port
(setsid python3 native/web/server.py --port 8564 &)
# whole-session GPU trace
node scripts/web/_framestall-gputrace.mjs --port 8564 --whole-session \
     --profile-secs 6 --out /tmp/fstall2-gpu --min-ms 30
```

Then analyze the saved `gputrace.json` (the in-script summary's clock anchor is
unreliable — read the raw trace): filter `ph==='X' && name==='GPUTask'` on the GPU
Process pid, sort by `dur`. Decompose a big GPUTask by intersecting the Dawn
slices (`DeviceBase::APICreateRenderPipeline`, `ShaderModuleVk::GetHandleAndSpirv`,
`Queue::Submit`, `DawnCommands`) whose `ts` fall inside `[lo, hi]`.

### Key gotchas (don't repeat)
- **GPU backend matters enormously.** On a real GPU (this harness uses
  `--use-angle=vulkan` → hardware `vkQueueSubmit`), individual GPUTasks are tiny
  (≤ 9 ms) in *steady* play; the ONE big one is the prewarm. A song-start trace
  anchored at the song-kick (part_difficulty) **misses it** — the burst already
  fired at boot. You must trace the whole session.
- **GPUTask is a GPU-process slice, not a main-thread longtask.** It does not
  directly block `PumpAudio`; but the matching main-thread `RunTask` (the wasm
  issuing the 240 `CreateRenderPipeline` wire commands) co-locates with it, and
  the GPU compile backs up the queue. The audio risk is real.
- **`CreateRenderPipeline` is ASYNC over the Dawn wire on web.** The client call
  is ~0.3 ms (`RB3_PREWARM_DBG` logs "created 240 pipelines in 4.4 ms"); the real
  ~550 ms is GPU-process SPIR-V/pipeline compile at the *flush*. ⇒ a wall-time
  budget on the client cannot bound it — chunk by **count**, not time. (My first
  attempt used a time budget; it created all 240 in frame 1 because the client
  side never tripped the budget. The count-based chunk is the working fix.)
- **Web build engine-path trap.** `scripts/web/build.sh` resolves the engine from
  `native/../../milo-native-engine` which in a `.claude/worktrees/<name>/` rb3
  worktree points at the WRONG engine checkout (a sibling worktree, not yours).
  Symptom: edits don't take effect (the `[A5]` log still says the legacy text).
  Fix: build with `MILO_ENGINE_PATH_OVERRIDE="$(cat .engine-path)"
  scripts/web/build.sh --debug --reconfigure`. Verify via `RB3_PREWARM_DBG=1`:
  the chunked build logs "pipeline pre-warm (chunked): +12 this frame …".

---

## The evidence

Whole-session GPU trace (page-load → game_screen, one-shot/baseline build):

```
Total GPUTasks: 1673   sum 2541 ms
Top GPUTasks (ms): 550.6, 18.2, 15.0, 13.9, 13.1, ...     <- ONE outlier
GPUTasks > 50 ms: 1   > 20 ms: 1
The 1 big one = 22% of ALL GPU time, in one frame, at rAF frame ~8 (only 7
frames rendered before it).
```

Decomposing the 550.6 ms GPUTask (Dawn slices inside its window):

```
WebGPU / CommandBuffer::Flush / OnAsyncFlush / PutChanged   550 ms (the flush)
DeviceBase::APICreateRenderPipeline      sum 543.6 ms  n=240  max 11.0 ms
ShaderModuleVk::GetHandleAndSpirv        sum 323.7 ms  n=176  max  5.4 ms
tint::spirv::writer::Generate()            6.3 ms
```

`n=240` = exactly `PipelineManager::PreWarm`'s sweep: 3 passes × 8 blend × 5 zMode
× 2 alphaCut = 240 keys (`PipelineManager.cpp` `BuildPreWarmKeys`). The source of
truth.

The call site (`Rnd_Wgpu_RB3.cpp` `BandRnd::BeginFrame`, the A5 prewarm block) had
a comment claiming "on web CreateRenderPipeline is async so it is a ~4 ms dispatch
that warms the pipelines off-thread" — **the dispatch is ~4 ms but the GPU compile
it triggers is ~550 ms**; the comment under-counted the GPU-side cost.

---

## The fix (engine `7ee6850`)

- `PipelineManager`: factored the key enumeration into `BuildPreWarmKeys()` (shared
  by the old synchronous `PreWarm` and the new chunked path, so both warm the
  identical 240-key superset). Added `PreWarmStep(mainFmt, rtFmt, maxThisCall,
  budgetMs=0)` — creates ≤ `maxThisCall` pipelines from a persistent cursor,
  returns keys remaining (0 == done). Member state `mPreWarmKeys / mPreWarmCursor /
  mPreWarmStarted`.
- `BandRnd::BeginFrame`: the one-shot `PreWarm` call is replaced by a per-frame
  `PreWarmStep(…, RB3PipelinePrewarmPerFrame())` that runs until it reports 0
  remaining, then latches `mPipelinesPrewarmed`. Knobs:
  - `RB3_PIPELINE_PREWARM_PER_FRAME` (default 12) — pipelines per frame.
  - `RB3_PIPELINE_PREWARM_NOCHUNK=1` — restore the legacy one-shot (for A/B).
  - `RB3_PIPELINE_PREWARM_OFF=1` — existing: disable prewarm entirely.
  - `RB3_PREWARM_DBG=1` — per-frame progress log.

At 12/frame the 240-key set warms over ~20 frames (~0.33 s of the idle splash
dwell) at ~0.3 ms client + ~40 ms GPU compile per frame — under the audio ring
depth, no single catastrophic flush.

### Why not finer (6/frame)?
Tried: 6/frame gave WORSE peak GPUTasks (98.5 / 87.4 ms) than 12/frame (79.9 /
59.8 ms) in the A/B. The GPU compile is not purely linear in count (per-flush
overhead + boot-time GPU-process contention + run-to-run variance ~±15 %). 12 is a
good default; the knob lets a future tune.

---

## Residual / honest feasibility notes

- The chunked per-flush GPUTask is still ~40–80 ms (12 pipelines × ~3.3 ms compile
  each). That is **under** the ~90–140 ms ring depth (the under-run threshold) but
  still over a 16.7 ms frame. It lands during the idle splash/boot dwell where
  there is frame slack, so it does not drop gameplay frames. Driving it lower would
  require **compiling fewer pipelines** — the 240 sweep is a deliberate superset;
  the nav only ever requests ~22 distinct main-pass keys (per
  `PipelineManager.cpp` PreWarm comment). A follow-up could prewarm only the
  observed common set and let the rare keys compile on-demand (1–2 pipelines,
  ~5 ms, well under budget). Not done here — keeping the superset is safer (a
  venue/material a given run didn't exercise still finds its pipeline warm); the
  current fix already removes the catastrophic single 550 ms task, which was the
  Track-C ask.
- A separate **~650 ms main-thread `BlinkScheduler_PerformMicrotaskCheckpoint`**
  task appears once during boot — that is the wasm App-ctor async-I/O/JSPI frame
  (the known ~12 s boot bottleneck, `web-loadperf-findings-2026-06-03.md`), **0
  pipelines created in it**. Out of scope for Track C and for this workflow (boot
  axis, not per-frame gameplay).
- Native: same `BeginFrame` path; on native `CreateRenderPipeline` is synchronous,
  so 12/frame spreads the ~700 ms native compile over ~20 load-screen frames
  (~35 ms each) instead of one 700 ms frame — strictly ≥ neutral. Builds + links
  clean (`rb3-native` target).

---

## Artifacts
- `/tmp/fstall2-gpu-whole/gputrace.json` — baseline whole-session GPU trace (the
  550 ms attribution).
- `/tmp/fstall2-gpu-fix3/gputrace.json` — chunked (12/frame): max GPUTask 79.9 ms,
  26 flushes of 12.
- `/tmp/fstall2-gpu-noch/gputrace.json` — one-shot A/B on the SAME build (464.9 ms).
- `/tmp/fstall2-game.png` — game_screen renders correctly with the fix.
- Harnesses: `scripts/web/_framestall-gputrace.mjs`,
  `scripts/web/_framestall-prewarm-ab.mjs`.
