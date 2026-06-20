# SPIKE RESULT — off-main-thread WASM audio vs JSPI: **VIABLE**

**Date:** 2026-06-20
**Worktree:** rb3 `wt-jspi-audio-spike` + engine `wt-jspi-audio-spike` (engine base `884ab17`)
**emcc:** 5.0.2 (dc80f645)
**Question (from MAP-EMSCRIPTEN):** Can off-main audio (Emscripten Audio Worklets =
`-sAUDIO_WORKLET` + `-sWASM_WORKERS`, which flip the wasm module to **shared memory**)
**coexist with the single-threaded `-sJSPI` build** — at configure, compile, link, AND
boot — or does shared memory break JSPI as prior notes feared?

## VERDICT: VIABLE

`-sJSPI` + `-sAUDIO_WORKLET` + `-sWASM_WORKERS` + `-sALLOW_MEMORY_GROWTH=1` +
`-sMAXIMUM_MEMORY=2GB` **configure, compile, link, and BOOT together** on emcc 5.0.2.
The shared-memory-vs-JSPI incompatibility the matrix flagged as the make-or-break risk
(issues #24302 / #19287) **did not manifest** — at the isolated level OR on the full
rb3-web bundle running in a real browser. The build-flag spike is **closed: not a dead
end.** The remaining work is engineering the off-main mixer, not fighting the toolchain.

---

## Evidence (real output, four escalating levels)

### Level 1 — isolated link (the crux: does shared memory + JSPI link?)
Trivial TU, exact production flags. `emcc … -sJSPI -sAUDIO_WORKLET=1 -sWASM_WORKERS=1
-sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2GB` → **LINK EXIT=0**. Only warning is the
cosmetic `-sJSPI (ASYNCIFY=2) is still experimental [-Wexperimental]` (also present in
today's baseline build). The produced wasm **imports `env.memory` with the memory-section
limits flag `0x03` = SHARED + has-max**, i.e. a *growable shared* memory — and the JS glue
constructs `WebAssembly.Memory({… "shared": true})`. So the module really is shared, and
emcc emitted **no growth/shared warning** — the matrix's flagged `ALLOW_MEMORY_GROWTH +
shared + 2GB` tension is RESOLVED on this emcc.

### Level 2 — real JSPI suspend/resume *inside* the shared-memory module
A TU that performs an actual JSPI suspension (`EM_ASYNC_JS` → `await new Promise(setTimeout)`
→ return) built with the same shared-memory+worklet flags, run in node:
```
before-suspend
after-suspend r=42      <-- suspended across an awaited JS promise and RESUMED correctly
RUN EXIT=0
```
This is the #24302 failure surface (suspend under a shared module). It **works**. The
realtime C Audio-Worklet API (`emscripten/webaudio.h`, `AudioSampleFrame`, the wait-free
`bool Process(...)` callback signature an off-main `MixSources` would use) also **compiles +
links + boots** under the same flags (LINK EXIT=0, run prints `aw-main`).

### Level 3 — the FULL rb3-web bundle compiles + links shared+JSPI
`MILO_WEB_AUDIO_WORKLET=ON scripts/web/build.sh --debug --reconfigure` — the entire
band3 + Milo-engine TU graph (static initializers, 2GB growable heap, emdawnwebgpu) built
and **deployed a 29 MB wasm** (`native/web/build/debug/rb3-web.{js,wasm}`). Confirmed on the
deployed artifacts:
- wasm `env.memory: flags=0x03 min=256pages max=32768pages` → **SHARED=YES**, growable, 2 GB.
- emdawnwebgpu (WebGPU port) **recompiled cleanly for shared memory**
  (`libemdawnwebgpu-…-O0-shared_memory.a`) → **WebGPU coexists with shared memory.**
- JS glue carries BOTH worlds at once: `WebAssembly.promising` + `JSPI` + `Asyncify`
  (JSPI) **and** `AudioWorkletGlobalScope` + `registerProcessor` + `_emscripten_wasm_worker`
  + `emscripten_create_wasm_audio_worklet` (off-main audio).
- Only link warnings are the **pre-existing** undefined-symbol stubs (`AXSetCompressor`,
  `Bink*`, `Store*`, …) — identical to the normal build, NOT introduced by the spike.

### Level 4 — the bundle BOOTS in a real (headless) browser
`scripts/web/audio-worklet-spike-boot.mjs --port 8569` (Playwright/Chromium, server.py's
COOP/COEP), two consecutive runs, **GATE_EXIT=0 / `SPIKE-BOOT: PASS`**:
```
crossOriginIsolated = true
SharedArrayBuffer available = true
wasm memory is SharedArrayBuffer at runtime = true     <-- running on shared mem in-browser
boot summary: booted=true frame=32 screen='intro_movie_screen' errors=0 fatal-pattern-hits=0
SPIKE-BOOT: PASS  (coi=true sab=true booted=true frame=32 sharedMem=true fatals=0)
```
`booted=true frame=32` reaching `intro_movie_screen` means the **JSPI async `.milo` fetch
path ran on the shared-memory module** (that screen needs the JSPI on-demand loads). Zero
SuspendError / SharedArrayBuffer / `Cannot enlarge memory` / `Aborted(` / worklet / Atomics
errors. The console "error"-typed lines are the usual benign NOTIFY / skinned-mesh-reexport
/ `sphere.milo_xbox` 404 noise the normal build also prints.

---

## Why this contradicts the "risky/likely-BLOCKED" prior framing

Prior notes deferred this as a risky spike on the theory that flipping to shared memory
would break JSPI module-init (#24302 suspend-in-ctors, #19287 proxied-entry wiring). Two
reasons it didn't bite here:
1. **We use plain `-sJSPI` (ASYNCIFY=2) with `_main`/`rb3MainLoopTick` as the real entry —
   not `PROXY_TO_PTHREAD`.** #19287's proxied-entry `_main_thread` rewiring never applies
   because `main()` stays on the UI thread; AUDIO_WORKLET/WASM_WORKERS spawn *extra* wasm
   contexts without moving `main`. So the heavy half of the JSPI×threads friction is simply
   not in this configuration.
2. **emcc 5.0.2's growable shared memory is mature** — it emitted shared+max memory with no
   warning and JSPI suspend/resume worked inside it (Level 2). The 2023-era hazards are not
   reproducing on the late-2025 toolchain.

This means the matrix's RANK-2 option (a) is **unblocked**, and a *true realtime audio
thread* (AUDIO_WORKLET, audio-clock-driven wasm callback) is on the table — not just the
RANK-1 pure-JS-worker fallback (d).

---

## What this spike did NOT do (honest scope)

- It did **not** port `MixSources`/the decoders into the audio-worklet callback, did not
  move the SAB-write off main, and did not measure under-runs against
  `STALL_BENCH_BASELINE.md`. The spike answers *"can the toolchain do it?"* (yes), not
  *"is the dropout fixed?"* (that's the MVP).
- It did **not** stress boot memory headroom under a full heavy song-load with shared
  memory on (boot reached intro_movie_screen cleanly; the 2 GB growable-shared heap is
  present, but the heaviest song-load phase wasn't driven). Low-risk, but verify during MVP.
- `-pthread`/`PROXY_TO_PTHREAD` (matrix option c) was deliberately **not** used — AUDIO_WORKLET
  pulls its own WASM_WORKERS threading; adding pthreads would re-introduce the #19287 entry
  rewiring for no audio benefit. Keep it off.

## Effort to a real MVP (VIABLE_WITH_WORK at the *implementation* layer)

The toolchain is VIABLE today; the MVP is **~3–5 days of focused engineering** (medium):
1. **Move mix off-main into the C audio-worklet callback** (`emscripten_create_wasm_audio_
   worklet_processor_async` + a wait-free `Process()` that drains the per-channel decoded-PCM
   rings and writes the output). `MixSources` is already wait-free/import-free (MAP-WEB §
   "smallest set"), so the body ports cleanly. **Constraint:** the callback must call NO
   JSPI/Asyncify import (no async fetch, no `emscripten_sleep`) — it only touches shared
   memory. The decoded-PCM rings (`StreamReceiver.mBuffer`, ~9 s) already live in the (now
   shared) wasm heap and the cursor protocol is already SPSC-atomic (MAP-WEB §"off-main
   targets" / `rb3_stream_receiver_native.cpp:62-69`), so the worklet can read them directly.
2. **Keep decode on main** behind the ~9 s ring (shape A in MAP-WEB) — JSPI decode stays on
   the rAF thread; only the millisecond-tight mix+ring-write moves off. This is what makes a
   main-thread longtask unable to starve the SAB.
3. **Drive the adaptive-latency target via a shared int** the worklet reads (the control law
   stays main-side, slow).
4. **Re-run `scripts/web/audio-stall-bench.mjs`** against the MVP and beat
   `STALL_BENCH_BASELINE.md`: ~0 % under-run through the 800 ms stall AND at a low fixed
   latency floor (not riding the 500 ms ceiling).

Open MVP risks (none are toolchain blockers): (i) boot/song-load memory under shared growable
heap at full load; (ii) the existing pure-JS `audio-worklet.js` drainer vs a new wasm
audio-worklet processor — pick one drainer (likely replace the JS one with the wasm callback,
or keep JS drainer + a Wasm-Worker producer); (iii) capture/debug taps that touch the mix
buffer must become shared-memory-safe.

---

## Files (worktree `wt-jspi-audio-spike`)

Engine (`milo-native-engine` branch `wt-jspi-audio-spike`):
- `CMakeLists.txt` — new `option(MILO_WEB_AUDIO_WORKLET … OFF)`; when ON adds
  `-sAUDIO_WORKLET=1 -sWASM_WORKERS=1` to **both** compile + link option lists in
  `milo_engine_apply_web_target_options`, and defines `MILO_WEB_AUDIO_WORKLET=1`. Default OFF
  → normal build byte-identical. Match-neutral (web-only helper, not in the Wii `.o` set).

rb3 (branch `wt-jspi-audio-spike`):
- `scripts/web/build.sh` — passes `-DMILO_WEB_AUDIO_WORKLET=${MILO_WEB_AUDIO_WORKLET:-OFF}`
  to cmake (env knob, reconfigure-aware) AND honors a pre-exported `MILO_ENGINE_PATH` so a
  paired engine **worktree** is actually used (the default probe resolved the
  `../../milo-native-engine` symlink to the MAIN engine repo, silently ignoring worktree
  engine edits — a real gotcha worth keeping).
- `scripts/web/audio-worklet-spike-boot.mjs` — NEW self-contained boot gate: loads the
  bundle, asserts `crossOriginIsolated` + `SharedArrayBuffer` + runtime shared wasm memory,
  waits for boot+first-frame, scans console for the fatal-pattern set, prints `SPIKE-BOOT:
  PASS/FAIL` + exit code. Reusable to re-verify any future shared-memory build.
- `docs/native/audio-thread-2026-06-20/02-spike-offmain-jspi-RESULT.md` — this doc.

Reproduce:
```bash
tools/setup-worktree.sh <name> --engine
cd .claude/worktrees/<name>
export EMSDK=/home/free/emsdk MILO_WEB_AUDIO_WORKLET=ON MILO_ENGINE_PATH="$(cat .engine-path)"
scripts/web/build.sh --debug --reconfigure                       # builds shared+JSPI bundle
python3 native/web/server.py --port $(cat .worktree-port) &
cd scripts/web && node audio-worklet-spike-boot.mjs --port $(cat ../../.worktree-port) --secs 25
```
