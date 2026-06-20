# Off-Main-Thread Web Audio under the single-threaded JSPI build — Compatibility Matrix

**Task:** MAP-EMSCRIPTEN. Scope which spike(s) are worth running to move web audio
PRODUCTION off the main thread so a stalled/janky main thread can't starve the
SharedArrayBuffer (SAB) audio ring.

**Verdict up front (TL;DR):**
- **SharedArrayBuffer is already on** — `server.py` already sends COOP/COEP on every
  response, and today's worklet *already* uses a SAB. No header work needed.
- The cleanest, lowest-risk off-main move is **(d) a JS/Worker producer that fills the
  SAB the AudioWorklet drains** — it needs **zero build-flag changes** and **cannot
  conflict with JSPI** because no wasm thread is involved.
- **(a) Emscripten Audio Worklets** and **(b) Wasm Workers** both *force the whole wasm
  module to SHARED memory* (they're the same feature underneath). That SHARED-memory
  module is the part with a **documented, unresolved JSPI interaction** (SuspendError in
  static ctors / `__wasm_call_ctors`, export-wiring during init). That is the real spike
  risk — not the worklet API itself.
- **(c) pthreads / PROXY_TO_PTHREAD** is the heaviest lift and shares (a)/(b)'s
  shared-memory + JSPI-init risk; it buys nothing audio-specific over (d).

This doc is grounded in the actual build config (read below) + Emscripten 5.0.x docs +
the two relevant open Emscripten issues. Build flags quoted are the ones live in the tree
today.

---

## Current build reality (what we actually ship)

Read from `native/CMakeLists.txt`, `milo-native-engine/CMakeLists.txt`
(`milo_engine_apply_web_target_options`), `scripts/web/build.sh`, `native/web/server.py`.

| Fact | Value | Source |
|---|---|---|
| emcc version | **5.0.2** (dc80f645, late-2025) — modern stable JSPI | `emcc --version` |
| Async model | `-sJSPI` + `-sJSPI_EXPORTS=["_main","_rb3MainLoopTick","rb3MainLoopTick"]` | engine CMake L595; rb3 CMake L959 |
| Threading | **NONE** — no `-pthread`, no `-sSHARED_MEMORY`, no `-sWASM_WORKERS`, no `-sAUDIO_WORKLET`, no `-sPROXY_TO_PTHREAD` | grep across both CMakeLists = 0 hits |
| Memory | `-sALLOW_MEMORY_GROWTH=1`, `-sMAXIMUM_MEMORY=2GB` (rb3 override; engine default 512MB), `-sSTACK_SIZE=4MB` | rb3 CMake L934; engine L573–575 |
| Exceptions | `-fwasm-exceptions` | engine L569/588 |
| GPU | `--use-port=emdawnwebgpu` | engine L568 |
| **COOP/COEP** | **already sent on EVERY response**: `Cross-Origin-Opener-Policy: same-origin` + `Cross-Origin-Embedder-Policy: require-corp` + `Cross-Origin-Resource-Policy: cross-origin` | `server.py` `end_headers()` L410–415 |
| Main loop | `async function tick(){ await Module._rb3MainLoopTick(); requestAnimationFrame(tick); }` — ONE suspendable wasm call per rAF | `main_web.cpp` L983–989 |
| Audio TU | **`AudioDevice_Web.cpp`** (bespoke JS-worklet + SAB ring). miniaudio's `MA_ENABLE_AUDIO_WORKLETS` web path is **NOT** engaged → we own the whole producer/consumer | engine CMake L406; rb3 CMake L221 |

**Key consequence:** `-sJSPI` today runs on a **non-shared (unshared) `WebAssembly.Memory`**.
The whole compatibility question reduces to: *what happens to JSPI when we flip the module
to shared memory?* — because (a), (b), and (c) all do that, and (d) does not.

---

## Today's data flow (what runs where)

```
MAIN THREAD (rAF, JSPI-suspendable):
  rb3MainLoopTick()  ──► App::RunOneFrame (App.cpp:504)
      ├─ engine Poll()  ─────────────► StreamReceiver::Poll() decodes song PCM
      │                                  (synth/vorbis) into an INTERNAL ring
      │                                  (rb3_stream_receiver_native.cpp)
      └─ AudioDevice::PumpAudio() (App.cpp:569)
            ├─ MixSources(buf,N)  (AudioDevice_Web.cpp:499)
            │     └─ for each AudioSource: RenderAudio() pulls from its decoder
            │        (StreamReceiver reads its internal ring; SampleInst reads PCM)
            │        + master limiter
            ├─ 44100→ctx-rate resample (when ctx != 44100)
            └─ js_audio_ring_write() ──► SAB RING  (writePos, Atomics)

AUDIO THREAD (real-time clock, JS AudioWorklet):
  MiloAudioProcessor.process()  (audio-worklet.js)
      reads SAB ring (readPos, Atomics) ──► speakers
      + prime gate + hold-last/ramp underrun concealment + underrun telemetry
```

**The starvation mechanism:** every producer step above runs *inside one rAF tick on the
main thread*. A longtask (asset load, GC, big `Poll()`) > buffered ring depth empties the
ring before the next tick refills it → the worklet underruns → click (now concealed by the
band-aid ramp, but the dip is real).

**What crosses the SAB boundary today:** only finished, mixed, ctx-rate **interleaved
stereo f32 PCM** + two Int32 cursors (`writePos`/`readPos`) via `Atomics`. The worklet is
pure JS — it never touches wasm memory or the engine. *This is the SPSC ring pattern from
Chrome's "Audio Worklet Design Pattern" and padenot/ringbuf.js — we already have the right
shape; the producer is just on the wrong thread.*

---

## What must move off-main to keep the buffer fed during a stall

This is subtle and is the single most important finding for scoping. Moving **only**
`MixSources` off-main is **NOT sufficient**, because the song-audio source it mixes is fed
by a *separate* producer that also runs on the main thread:

1. **`AudioDevice::PumpAudio` / `MixSources` / resample / ring-write**
   (`AudioDevice_Web.cpp`) — the immediate producer. Self-contained: touches only
   `mSources` (`vector<AudioSource*>` under `mSourceMutex`), `mMixBuffer`, `mLimiterEnv`.
   **Does NOT touch the scene graph, GPU, or DataArray.** Cleanly isolatable. ✅

2. **The decoders behind `AudioSource::RenderAudio`** — for song audio this is
   `StreamReceiver` (`rb3_stream_receiver_native.cpp`), whose internal PCM ring is filled
   by **`StreamReceiver::Poll()`**, which is driven by the **engine main-loop `Poll()`**,
   NOT by `PumpAudio`. The file says so explicitly (L62–68):
   > *"mRingWritePos is written by the producer (synth/main thread)… On web both run on the
   > SAME thread (PumpAudio): the producer Poll() runs, then RenderAudio() runs, with no
   > overlap."*

   So a main-thread stall freezes the **decode refill** too. If we move only the mixer,
   the mixer will faithfully drain the StreamReceiver ring and then *that* ring runs dry
   during the same stall.

**Therefore the off-main target is one of:**
- **Move the whole audio subsystem** (AudioDevice mixer **+** the AudioSource decoders **+**
  the synth/`StreamReceiver::Poll` refill) into the off-main context, with a deeper SAB so
  it coasts. This is the correct, complete fix but pulls a chunk of engine into the worker.
- **OR** keep decode on main but make **both** rings (StreamReceiver internal ring AND the
  output SAB) deep enough that the off-main mixer can coast across the worst main-thread
  longtask without either drying up. Simpler, but it's *latency* spent to hide *jank* — the
  same trade the user just rejected for the output ring. Only worth it if the residual
  stalls are short.

The realistic plan: **off-main mixer (drains the SAB at audio cadence) + off-main pull from
a deepened StreamReceiver ring**, with synth-decode `Poll` either also moved or buffered
ahead far enough that a frame-or-two main stall is invisible.

---

## Compatibility Matrix

Ranked by (compatibility-with-JSPI × implementation-effort). "Shared module" = the option
forces `WebAssembly.Memory` `shared:true` for the whole wasm, which is where the JSPI risk
lives.

### (d) Pure-JS / Worker producer filling the SAB — **RANK 1 (do this)**

| Dimension | Finding |
|---|---|
| Needs SAB / COOP-COEP? | **SAB yes (already used today); COOP/COEP already sent by server.py.** No new headers. |
| Shared wasm module? | **No.** The producer runs in plain JS (in a `Worker` or even an `AudioWorkletGlobalScope`), reading/writing the SAB via `Atomics`. The wasm module stays single-threaded + unshared. |
| JSPI compatibility | **Cannot conflict.** No wasm thread, no shared memory → JSPI's module/threading model is untouched. This is the *only* option with zero JSPI risk. |
| What the audio code looks like | Two sub-variants: **(d1) "marshal PCM to a JS Worker"** — main-thread wasm still runs `MixSources` but `postMessage`s nothing; instead it writes mixed PCM into a SAB that a **Worker** owns, and the Worker (or the worklet directly) drains to the output SAB. This does NOT move production off-main, so it does NOT fix starvation — **reject.** **(d2) "decode+mix in a Worker"** — port the AudioDevice mixer + AudioSource decoders to a **second wasm instance loaded in a Worker** (separate `Module`, its own unshared memory), fed song PCM over a SAB the main thread fills from `StreamReceiver`, mixing on a `setInterval`/timer or `Atomics.wait` cadence, writing the output SAB the worklet drains. The worklet stays the pure-JS drainer we already have. |
| Build-flag delta from today | **(d2): none to the main module.** The Worker-side mixer is a *separate, small* emcc artifact (could even be `-sSINGLE_FILE`, no JSPI, no threads) OR hand-written JS. The main rb3-web build is unchanged. |
| Effort | **Medium.** No build-system risk; the cost is refactoring the mixer + decoders into a standalone unit and defining the main→worker SAB hand-off for song PCM. Biggest design question = whether `StreamReceiver::Poll` (synth decode) moves too or stays main-side behind a deep ring (see "off-main targets" above). |
| Risk | **Lowest.** Fully decoupled from JSPI. Worst case it's "just" engineering, never a build-spike dead end. |

### (a) Emscripten Audio Worklets (`-sAUDIO_WORKLET -sWASM_WORKERS`) — **RANK 2 (spike-gated)**

| Dimension | Finding |
|---|---|
| Needs SAB / COOP-COEP? | **Yes, mandatory.** Docs: AUDIO_WORKLET *"implicitly add[s] worklet to the ENVIRONMENT, but additionally depends on WASM_WORKERS and Wasm SharedArrayBuffer."* COOP/COEP already sent. |
| Shared wasm module? | **Yes** — `-sWASM_WORKERS` makes the module's `WebAssembly.Memory` **shared** so workers share the address space. This is the crux. |
| JSPI compatibility | **UNKNOWN — this is the spike.** Docs are silent on JSPI×AUDIO_WORKLET. The hazard is *shared-memory module init under JSPI*: issue **#24302** ("trying to suspend without WebAssembly.promising") shows JSPI suspends triggered from **static ctors during `__wasm_call_ctors`** already break on a *plain* JSPI build; a shared module adds a second instantiation (per worker) and per-thread ctor runs, multiplying that surface. Issue **#19287** shows JSPI×PROXY_TO_PTHREAD needed manual `Asyncify.instrument*` + extra `JSPI_EXPORTS` wiring to even start. **Net: not proven-incompatible, but proven-fragile at module-init.** |
| What the audio code looks like | Replace `AudioDevice_Web.cpp`'s JS init with `emscripten_create_wasm_audio_worklet_processor_async` + a C `process` callback (`createWasmAudioWorkletProcessor` path). The callback runs ON the audio thread in wasm. **Constraint:** the callback *"should execute as quickly as possible and be non-blocking… spinning a custom for(;;) loop is not possible"* — so the mixer must be wait-free and **must not call any JSPI/ASYNCIFY import** (no `emscripten_sleep`, no async file fetch) from inside it. Our `MixSources` is already wait-free and import-free, so the mixer body ports cleanly; the decoders feeding it must also be made non-blocking on that thread. |
| Build-flag delta from today | **+`-sAUDIO_WORKLET -sWASM_WORKERS`** → flips module to shared memory. **Interaction with `-sALLOW_MEMORY_GROWTH=1`:** a *growable shared* memory is allowed in modern engines/emcc but is the classic footgun (every grow must propagate to all threads; historically `ALLOW_MEMORY_GROWTH`+threads warned/penalized). With `-sMAXIMUM_MEMORY=2GB` we may need to **drop growth and pre-reserve** a fixed shared heap, which collides with the 2GB ceiling we set for the heavy song-load phase. This is a real, measurable spike cost, not a flag flip. |
| Effort | **High.** Build-flag spike (shared mem + growth + JSPI coexistence) **plus** porting the mixer/decoders to the C worklet callback **plus** resolving how the JSPI main module instantiates alongside a shared module. |
| Risk | **High at the build layer** (the exact thing prior notes flagged). The DSP port itself is straightforward. |

### (b) Wasm Workers (`-sWASM_WORKERS`, no AUDIO_WORKLET) — **RANK 3**

| Dimension | Finding |
|---|---|
| Needs SAB / COOP-COEP? | **Yes** — *"the Memory object… can be shared across multiple Workers"*; docs explicitly require COOP/COEP. Already sent. |
| Shared wasm module? | **Yes** — same shared-memory module as (a). *"each Wasm Worker shares… the same WebAssembly Memory address space of the main thread."* |
| JSPI compatibility | **Same UNKNOWN/fragile as (a)** — identical shared-module-under-JSPI init hazard (#24302, #19287). No audio-thread realtime priority either (that's AUDIO_WORKLET's value-add), so a Wasm Worker mixer would push to the SAB on a `setInterval`/`Atomics.wait` cadence — *not* the audio clock. |
| What the audio code looks like | Spawn a Wasm Worker (`emscripten_malloc_wasm_worker` / `emscripten_create_wasm_worker`), run the mixer loop there driven by a timer or `emscripten_wasm_worker_*` futex, write the output SAB the existing JS worklet drains. The pure-JS worklet stays as-is. |
| Build-flag delta from today | **+`-sWASM_WORKERS`** → shared memory; same growth/2GB tension as (a). |
| Effort | **High** (shared-mem build spike) for **less benefit than (a)** — no realtime audio thread; the mixer still races the OS scheduler instead of the audio callback. |
| Risk | High build risk, mediocre payoff. Strictly dominated by (a) if you accept shared memory, and by (d) if you don't. |

### (c) pthreads / PROXY_TO_PTHREAD (`-pthread` [+ `-sPROXY_TO_PTHREAD`]) — **RANK 4 (avoid)**

| Dimension | Finding |
|---|---|
| Needs SAB / COOP-COEP? | **Yes** — pthreads = SharedArrayBuffer + Atomics. Already sent. |
| Shared wasm module? | **Yes**, and the heaviest variant — full POSIX thread pool, TLS, futexes. |
| JSPI compatibility | **Known-problematic but not impossible.** Issue **#19287** is *specifically* JSPI×PROXY_TO_PTHREAD: with `PROXY_TO_PTHREAD` the real entry becomes emscripten's `_main_thread` (not your `main`), so **that** symbol must be JSPI-exported, and `invoke_*`/`Asyncify.instrument*` need manual handling. It was made to work *with workarounds*; it is not turnkey. Plus #24302's static-ctor-suspend hazard applies per-thread. |
| What the audio code looks like | Run the audio producer on a dedicated pthread (`pthread_create`), shared `mSources`/rings via normal C++ atomics/mutex (already have `mSourceMutex`), push to the output SAB. Still need the JS worklet (or AUDIO_WORKLET) as the actual audio-clock drainer — a pthread is *not* the audio thread. |
| Build-flag delta from today | **+`-pthread` (compile+link)** and likely **+`-sPROXY_TO_PTHREAD`**, **+`-sPTHREAD_POOL_SIZE`**; shared memory; same growth/2GB tension; **plus** the `JSPI_EXPORTS` re-wiring for the proxied entry point. Largest delta of all four. |
| Effort | **Highest.** Build spike + entry-point JSPI rewiring + thread-safety audit of the whole audio path. |
| Risk | **Highest.** Most moving parts, documented JSPI friction, and a pthread still isn't the realtime audio thread — you'd pair it with (a) or (d) anyway. No reason to choose this for audio. |

---

## Cross-cutting facts that decide the ranking

1. **COOP/COEP + SAB are already in production.** `server.py` sends them unconditionally;
   the current worklet already allocates a `SharedArrayBuffer`. None of the four options
   needs server/header changes. (This retires a worry from prior notes.)

2. **The single shared-vs-unshared decision dominates.** (a)(b)(c) all flip the *main*
   wasm module to shared memory. The *only* documented JSPI failure modes we found
   (#24302 static-ctor suspend, #19287 proxied-entry export wiring) bite **at module
   instantiation of a JSPI build**, and a shared module multiplies instantiation
   (per-worker) + per-thread ctor runs. (d) keeps the JSPI main module unshared and puts
   audio in a *separate* unshared/JS context → it sidesteps the entire hazard class.

3. **`ALLOW_MEMORY_GROWTH=1` + shared memory + `MAXIMUM_MEMORY=2GB` is a live tension.**
   We *need* growth + a 2GB ceiling for the heavy song-load phase (rb3 CMake comment,
   L926–934). Shared growable memory is permitted on modern emcc but is the canonical
   footgun and may force pre-reserving the heap — directly fighting the 2GB headroom we
   rely on. **Any spike into (a)/(b)/(c) must measure boot + song-load memory, not just
   "does audio play".**

4. **The mixer is already off-main-ready; the decoder refill is the catch.** `MixSources`
   is wait-free and import-free (ports to a realtime callback as-is). But song audio is fed
   by `StreamReceiver::Poll` (synth decode) driven by the **engine main loop**, so a true
   stall-immune fix must also move/deepen that refill (see "off-main targets"). This is
   independent of which transport (a/b/c/d) you pick.

5. **emcc 5.0.2 is recent enough that JSPI is the stable `-sJSPI`** (not legacy
   `ASYNCIFY=2`), and AUDIO_WORKLET/WASM_WORKERS are mature. The issues cited are from
   4.0.8–6.0.x and remain *open*, so they are current, not stale-toolchain artifacts.

---

## Recommended spike order (what's worth running)

1. **Spike (d2) first — highest ROI, lowest risk.** Build the off-main mixer as a
   self-contained unit (separate tiny wasm `Module` in a Worker, or JS) reading a main→worker
   PCM SAB and writing the existing output SAB. **Zero changes to the JSPI main build.**
   Measure under-runs with `scripts/web/audio-stall-measure.mjs` while forcing main-thread
   longtasks. This either fixes starvation outright or proves the residual is decode-refill,
   which scopes step 2.

2. **Only if (d2) is insufficient or too heavy, run the (a) build spike** — flip
   `-sAUDIO_WORKLET -sWASM_WORKERS` on a **throwaway worktree** and answer the ONE empirical
   question prior notes deferred: *does the rb3-web JSPI module still boot + load a song with
   shared memory on?* Gate it on: (i) it compiles + links; (ii) `_main`/`_rb3MainLoopTick`
   still suspend correctly (no #24302 SuspendError in ctors); (iii) boot + song-load memory
   stays under the 2GB/browser ceiling with growth resolved. If all three pass, (a) gives a
   true realtime audio thread; if any fails, (d2) is the answer and (a)/(b)/(c) are closed.

3. **Skip (b) and (c) for audio.** (b) is (a) without the realtime thread; (c) is the most
   build-fragile and still needs (a)/(d) as the actual drainer.

---

## Benchmark reference (native real-device, snd-aloop)

Native already mixes on the miniaudio audio thread (off-main, stall-immune by construction)
and is the correctness/latency reference. With `snd-aloop` loaded (`aplay -l` → `hw:Loopback`),
point rb3-native's miniaudio at the Loopback **playback** device, capture the Loopback
**capture** side (`arecord -D plughw:Loopback,1,0 -f S16_LE -r 48000 -c 2`), and run
`scripts/native/audio_verify.py` on it. That gives the "what clean off-main audio sounds
like on a real device" baseline the web fix should converge to. (This verification is a
residual from the last wave and is tracked here but is a sibling task to the matrix above.)

---

## Source index

Build config (read in-tree):
- `native/CMakeLists.txt` (L881–965: rb3 web link options, `MILO_WEB_AUDIO_NS=rb3`, 2GB override, JSPI_EXPORTS)
- `milo-native-engine/CMakeLists.txt` (L557–610: `milo_engine_apply_web_target_options`, `-sJSPI`, memory, worklet JS deploy)
- `scripts/web/build.sh` (toolchain activation; no threading flags)
- `native/web/server.py` (L410–415: COOP/COEP/CORP headers — SAB already enabled)
- `native/src/main_web.cpp` (L979–993: JSPI rAF loop)
- `rb3/src/App.cpp` (L504, L569: RunOneFrame → PumpAudio main-thread pump site)
- `milo-native-engine/src/audio/AudioDevice_Web.cpp` (L499 MixSources, L560 PumpAudio, L575+ adaptive latency)
- `milo-native-engine/src/platform/web/assets/audio-worklet.js` (pure-JS SAB drainer + concealment + telemetry)
- `rb3/native/src/rb3_stream_receiver_native.cpp` (L62–68 thread-safety note: decode refill is main-thread, separate from PumpAudio)

Emscripten docs (5.0.x–6.0.1):
- Wasm Audio Worklets API — requires `-sAUDIO_WORKLET -sWASM_WORKERS`; non-blocking callback; WASM_WORKERS+SAB dependency; ENVIRONMENT=worklet escape hatch
- Wasm Workers API — shared `WebAssembly.Memory`; COOP/COEP required; hybrid `-pthread` mode
- Pthreads support — `PROXY_TO_PTHREAD` moves `main()` off the UI thread; SAB + Atomics
- Asyncify/JSPI porting — JSPI as stable async model; silent on threads (→ the spike)
- settings_reference — JSPI / JSPI_IMPORTS / JSPI_EXPORTS (no documented thread restriction)

Emscripten issues (current/open):
- #24302 — JSPI "trying to suspend without WebAssembly.promising": suspend from static
  ctors during `__wasm_call_ctors`; standalone export-wiring hazard, worsened by per-thread
  ctor runs on a shared module. emcc 4.0.8+, open.
- #19287 — JSPI×PROXY_TO_PTHREAD: proxied entry `_main_thread` must be JSPI-exported;
  `invoke_*`/`Asyncify.instrument*` need manual handling. Worked-around, not turnkey. Open.

Pattern references:
- Chrome "Audio Worklet Design Pattern" + padenot/ringbuf.js — the SPSC SAB-ring producer→
  worklet pattern we already implement (producer just on the wrong thread).
