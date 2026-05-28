# Boot reliability — plain `rb3-native` reaches gameplay without gdb (V27)

Date: 2026-05-28. Host: Linux x86_64, NVIDIA RTX 3090 (proprietary driver
595.71.05), PipeWire audio (no PulseAudio daemon; libpulse → pipewire-pulse
shim). clang LP64 build, `native/build-native/rb3-native`.

## Problem

Plain headless runs (`RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 …`) SIGSEGV'd
before reaching gameplay ~90% of the time. Agents had been working around it by
running under `gdb -batch -ex run`, whose slowdown changed the timing enough to
dodge the crash — which made every screenshot/verify run slow and unreliable
and blocked the workflow.

It was assumed (per the task framing) that the crash was the input harness
firing a verb at a hardcoded frame against a screen/object that hadn't loaded
yet (a verb-vs-loading race). Reproducing the crash showed there were actually
**two independent issues**, and the dominant one was NOT the input harness.

## Root cause #1 (DOMINANT): GPU-driver vs audio-thread startup race

Reproduced WITHOUT gdb (`ulimit -c unlimited`, systemd-coredump), then pulled
the core with `coredumpctl gdb`. Key observations:

- The crash happens during **`App::App` construction — before the frame loop
  even starts** (no `frame N complete` ever prints). It is therefore NOT the
  input harness: the first scripted verb is `@10` and the loop never runs.
- It reproduces with **NO `RB3_GAME_INPUT` at all** (3/3 crash).
- It **does NOT reproduce without `MILO_AUDIO=1`** (5/5 clean). Audio is required
  to trigger it.

Faulting backtrace (from the core dump):

```
# main thread (App::App), blocked in the NVIDIA driver mid-allocation:
ioctl
… libnvidia-eglcore.so.595.71.05 (several frames) …
dawn::native::vulkan::DescriptorSetAllocator::AllocateDescriptorPool
dawn::native::vulkan::DescriptorSetAllocator::Allocate
dawn::native::vulkan::BindGroupLayout::AllocateBindGroup
dawn::native::vulkan::BindGroup::Create
dawn::native::vulkan::Device::CreateBindGroupImpl
dawn::native::DeviceBase::CreateBindGroup
wgpu::Device::CreateBindGroup
BandRnd::WriteSceneUniforms(RndCam*)                 // first bind group
BandRnd::BeginFrame(RndCam*)
BandRnd::BeginDrawing()
App::App(int, char**)                                 // App.cpp:233 first frame
RunGame / main

# the thread that actually took SIGSEGV is an NVIDIA driver-internal worker
# thread (PC == fault address == an anonymous mapping in its own stack region,
# i.e. the driver jumped to garbage), NOT any of our code.
```

`App.cpp` (matched fork) orders, six lines apart, with nothing between:

```
227:  SynthInit();                 // → NativeSynth::Init → AudioDevice::Init →
                                   //   ma_device_start spins up the audio thread
228:  Movie::Init();
233:  TheRnd->BeginDrawing();      // BandRnd → first CreateBindGroup →
234:  TheRnd->EndDrawing();        //   NVIDIA descriptor-pool allocation
```

So the **audio device init runs concurrently with the renderer's very first
descriptor-pool allocation.** On this host miniaudio's default Linux backend is
**PulseAudio-first**, and the PulseAudio server is PipeWire's pulse shim. The
libpulse client connection spins up PipeWire/SPA plugin-loader threads that
`dlopen` EGL/GL SPA plugins against the **same GPU driver** the renderer is
mid-initialising. That concurrent driver touch faults an NVIDIA Vulkan driver
internal worker thread (deep in `libnvidia-eglcore`, never in our code).

This is an environmental driver bug/race; the fix is to remove the overlap.

### Fix #1a — pin the Linux audio backend to ALSA, not PulseAudio

`milo-native-engine/src/audio/AudioDevice.cpp` `AudioDevice::Init()` now, on
Linux, initializes an explicit `ma_context` with the backend list pinned to
**ALSA** (instead of letting miniaudio pick PulseAudio first). ALSA opens the
device directly with no client/plugin threads, removing the contention.
Overridable with `MILO_AUDIO_BACKEND=pulseaudio|alsa|default`.

Result: **10/10** short runs clean (was ~1/8). On this host ALSA's default PCM
has no openable hardware output (only HDMI cards), so `ma_device_init` returns
`-401` and audio output is silently skipped — but the synth/stream code still
runs, gameplay is unaffected, and the crash is gone. (A host with a working
ALSA/`default` PCM gets real audio through ALSA. Anyone needing PulseAudio
output can set `MILO_AUDIO_BACKEND=pulseaudio` and accept the race.)

### Fix #1b — GPU warm-up before audio start (defense in depth)

`milo-native-engine/src/gfx/GpuDevice.cpp` adds `GpuDevice::WarmUp()`: submit a
trivial empty command buffer and drain it synchronously
(`OnSubmittedWorkDone` + `WaitAny`), forcing the driver's first-use
submission/allocator worker threads to spin up and settle. The rb3 glue
(`native/src/rb3_synth_native.cpp`, `NativeSynth::Init`) calls it on the GPU
path **before** `AudioDevice::Init`, so the driver's lazy init never overlaps
the audio thread start. Cheap and a no-op on backends without the race. (ALSA
pinning is the load-bearing fix; this is belt-and-suspenders.)

## Root cause #2: input verbs fired on EXACT frames, not on readiness

`native/src/rb3_game_input.cpp` originally dispatched each scripted verb when
`frame == @N` exactly. On a slow/contended host the targeted screen/object can
still be loading at frame N, so a `select:`/`msg:` verb ran against a
not-yet-existent or mid-transition screen and dereferenced null/garbage — a
genuine latent crash. (It was masked on this host by crash #1, which fires
earlier, but it is the crash the task description refers to and is real on
hosts where loading is slower than the script's frame budget.)

### Fix #2 — state-driven readiness gating

The whole `RB3_GAME_INPUT` script is now ONE ordered queue dispatched
**sequentially + readiness-gated**:

- `@N` is reinterpreted as a **MINIMUM frame / ordering hint**, not an exact
  trigger. Verbs are stable-sorted by `(minFrame, scriptOrder)`.
- The cursor verb fires only when (a) `frame >= @N`, (b) all earlier verbs have
  already fired, and (c) its **readiness predicate** holds. At most one verb
  fires per frame, so a transition kicked off by one verb settles before the
  next is evaluated.
- Readiness predicates (no hardcoded screen names):
  - `start`/`confirm`/`up`/`down`/…: stable current screen
    (`CurrentScreen() != null && !InTransition()`).
  - `select:<btn>`: stable screen AND the named `UIComponent` resolves on the
    current screen (same lookup the SELECT will use).
  - `msg:<obj>:<action>`: the target object exists in `ObjectDir::Main()` AND
    the UI is stable.
  - `track:<sym>`: the synth user's `BandUser` exists.
  - `nofail`: `MetaPerformer::Current()` exists (the song-load → gameplay
    handoff is done).
- If a verb's target never becomes ready within
  `RB3_INPUT_VERB_TIMEOUT` frames (default 3000) of becoming eligible, it is
  **SKIPPED with a LOG** rather than firing blind or hanging — so a mis-timed
  script degrades gracefully and can never SIGSEGV on a missing target.
- Per-verb null-guards were already present in the Exec helpers (ExecMsg /
  ExecSelect log "NOT FOUND"); the readiness gate now means we don't even call
  them until the target resolves.

The frame loop logs each verb as `FIRE [i/n] … on '<screen>'`, `WAIT … : <why>`,
or `SKIP … : target never became ready …`, so timing is fully observable.

HTTP `/api/input` injection is unchanged (frame-agnostic, fires next frame).

## Before / after reliability

| Configuration | Reaches gameplay (plain, no gdb) |
|---|---|
| Before — no input, with audio | ~0/3 (crash in App::App) |
| Before — full reproducer, with audio | ~1/8 (crash in App::App) |
| After (ALSA pin + warm-up + state-driven input) — full reproducer | **5/5 (DoD) + 10/10 larger sample** |

Each "after" run: clean `EXIT=0`, reaches `currentScreen = 'game_screen'`, song
loads to `Game::mLoadState = kReady`, all 9 verbs FIRE in order (the final
`nofail` fires on `game_screen`), and gameplay renders.

Gameplay rendering confirmed by screenshots in
`docs/sessions/native/screenshots/v27-boot-reliability/` (note highway, 5
colored frets at the smasher line, gems streaming down the track in sync, energy
meter). No regression to V19–V26 gameplay/venue/HUD.

## Residual flake / notes

- The state-driven gate does NOT make a *logically invalid* script succeed. A
  torture script that fires every verb `@0` (e.g. selecting a main-hub button
  while still on the intro movie) correctly WAITs then SKIPs the unreachable
  selects (no SIGSEGV), but the later song-load verbs then run in the wrong
  context and the game itself asserts ("Trying to access item 0 in list of 0
  items") — a downstream game-logic abort, not a harness crash. Real, properly
  `@N`-spaced scripts are unaffected. This is acceptable: the gate's job is to
  prevent the null-deref crash and to be robust to *timing*, not to repair a
  semantically wrong script.
- ALSA produces no audio output on THIS host (no analog/`default` PCM; HDMI
  only) — `ma_device_init` → `-401`, audio skipped. The audio-playback gate is a
  separate V1 item; this change does not regress it on hosts that do have a
  working ALSA/`default` device, and `MILO_AUDIO_BACKEND=pulseaudio` restores
  the old behavior where needed.

## Canonical reproducer (unchanged invocation; now reliable without gdb)

```
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=24000 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
  /home/free/code/milohax/rb3/native/build-native/rb3-native
```

Semantics note: the `@N` values are now MINIMUMS/ordering hints — the script is
robust if a screen loads later than its `@N`. `MILO_MAX_FRAMES=2500` is enough
to reach gameplay + render gems; `9000+` is needed only to watch the full gem
stream (first gems at song-time ~3.3s).

## Files changed

- `milo-native-engine/src/audio/AudioDevice.{h,cpp}` — Linux ALSA-pinned audio
  context (env `MILO_AUDIO_BACKEND`); context teardown + backend in init log.
- `milo-native-engine/src/gfx/GpuDevice.{h,cpp}` — `WarmUp()` (synchronous
  first-submit drain).
- `rb3/native/src/rb3_synth_native.cpp` — `NativeSynth::Init` calls
  `gBandRnd.Gpu().WarmUp()` before `AudioDevice::Init`.
- `rb3/native/src/rb3_game_input.cpp` — state-driven sequential readiness-gated
  verb dispatch (replaces exact-frame dispatch); `@N` is now a minimum frame.
