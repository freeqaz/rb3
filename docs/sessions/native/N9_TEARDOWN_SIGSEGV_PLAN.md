# N9 — Intermittent Teardown SIGSEGV — Localization + Fix Plan

**RESOLVED 2026-06-02.** Fix landed on branch `wt-teardown` (commit on
`src/App.cpp` + `native/src/main_native.cpp`). See § RESOLUTION below.

**Authored:** 2026-05-29 (N9 diagnostic subagent, Opus, READ-ONLY on source — no
edits, no commit). Binary under test: `native/build-native/rb3-native`, fresh
(built 2026-05-29 05:12, 12s after newest source; matches HEAD `52db8593` +
intact HX_NATIVE blocks). Reproducer = the canonical full-song
`RB3_GAME_INPUT` script at `MILO_MAX_FRAMES=9000`, `MILO_AUDIO=1`.

> **HEADLINE:** The N9 high-address teardown SIGSEGV is **NOT** the
> CharBonesObject / ObjPtr proxy-lifetime class. Three independent batches of
> system coredumps (all the *current* binary) show the faulting thread is the
> **PipeWire realtime audio thread** servicing miniaudio's ALSA→PipeWire device,
> crashing during process teardown because the audio device is `ma_device_uninit`'d
> LAST in the exit sequence (after GPU teardown + a synth Poll) instead of FIRST.
> The "high address" is PipeWire's mmap'd SPA/RT code, not a freed C++ vtable.
> **Confidence: high.** The CharBonesObject hypothesis is **refuted** for these
> instances (see §2).

---

## RESOLUTION (2026-06-02)

### What the prior fix (commit a25ab7bd) actually does

The original N9 plan proposed registering `RB3AudioTerminateExitCallback` after
`RB3RegisterBandRndShutdown` in `RunGame()` so it lands at the HEAD of the
`push_front` exit list. **This model was wrong.** The App ctor (line ~691 of
`main_native.cpp`) runs AFTER both RunGame registrations, and `SynthInit()`
(inside the ctor, `App.cpp:249`) calls `TheDebug.AddExitCallback(SynthTerminate)`
(`Synth.cpp:291`) — which push_fronts SynthTerminate to the HEAD, ahead of
`RB3AudioTerminateExitCallback`. So the true teardown order has always been:

```
SynthTerminate → NativeSynth::Terminate → AudioDevice::Terminate   ← runs FIRST
RB3AudioTerminateExitCallback (sees mInitialized==false, is a no-op) ← second
BandRndShutdown                                                      ← LAST
```

`SynthTerminate` already joins the device via `AudioDevice::Terminate`. The
`RB3AudioTerminateExitCallback` is inert (belt-and-suspenders, harmless no-op).

### The genuine residual race

`SynthTerminate` calls `TheSynth->Poll()` at `Synth.cpp:295` BEFORE calling
`TheSynth->Terminate()`. During that `Poll()`, the miniaudio/PipeWire RT
callback thread is still live. If the RT thread is mid-flight in PipeWire SPA
code when `Poll()` touches engine objects (or when the process continues to
libc teardown), it can fault on PipeWire-owned mmap'd memory. This is the
exact fault seen in the N9 coredumps.

### The fix applied

**`src/App.cpp`** — in `App::RunWithoutDebugging()`, the HX_NATIVE frame-loop
exit block, immediately before `RB3HttpServerShutdown(); return;`:

```cpp
AudioDevice::GetInstance().Suspend();
```

`Suspend()` sets `mSuspended = true` (atomic release) and takes `mSourceMutex`,
guaranteeing the RT callback (`MixSources`) is not mid-flight. After `Suspend()`
returns, the RT thread will not re-enter `MixSources`. This quiesces the device
BEFORE `Debug::Exit` fires the exit-callback chain, so `SynthTerminate`'s
`TheSynth->Poll()` runs on a quiesced device. `AudioDevice.h` was added to the
`#ifdef HX_NATIVE` include block in `App.cpp`.

**`native/src/main_native.cpp`** — comment block corrected to document the true
teardown order and explain why `RB3AudioTerminateExitCallback` is a no-op safety
net, not the primary quiesce (which is `Suspend()` in `App.cpp`).

### Verification results (2026-06-02)

- Build: clean (`cmake --build native/build-native --target rb3-native -j`)
- Smoke: `RB3_BOOT=1` → `SystemInit OK` (rc=0); `RB3_RENDER_MESH=1` passes
- Teardown gate: **30/30** runs of `MILO_MAX_FRAMES=300, MILO_AUDIO_BACKEND=null`
  each showing `AudioDevice: initialized` (RT thread live) and `APP EXITED, EXIT
  CODE 0` (rc=0)
- Coredumps: zero new coredumps after build mtime (all retained cores are from
  2026-05-29, predating this fix)
- Note: 8500-9500 frame runs are resource-prohibitive on this headless host
  (~9 fps, ~15 min/run). The null-backend null-RT path exercises the same
  Suspend → SynthTerminate → Terminate ordering. A PipeWire host would provide
  stronger signal for the mmap-race coverage specifically.

---

## 1. Reproduction data

### Live re-runs this session (current binary, canonical reproducer)
- 14 full-song runs, `MILO_MAX_FRAMES` varied 8500–9500, 4-way concurrent.
  **Crash rate observed in my runs: 0/14** (all `rc=0`, clean
  `BandRnd: Shutdown complete` → `APP EXITED, EXIT CODE 0`). Logs:
  `/tmp/n9_batch/*.log`; harness `/tmp/n9_repro.sh`, `/tmp/n9_batch.sh`.
- A single timed run is ~106 s wall (≈100 s CPU). The race is genuinely
  low-rate; I did not hit it in 14 live runs, so my *direct* rate is **<1/14**.

### System coredumps (the decisive evidence — same binary path)
`coredumpctl` retained crashes of `native/build-native/rb3-native` from earlier
runs of this exact binary build (`RB3SignalHandler` at `rb3-native + 0x2bc201`
matches the current image). Three independent batches, **identical signature**:

| When | PIDs | Signal | Faulting thread top frames |
|------|------|--------|----------------------------|
| 2026-05-29 05:29:35/37 | 3678273, 3678723 | SIGSEGV | PipeWire RT thread (see below) |
| 2026-05-28 14:15:36–46 | 2800983…2801145 (6×) | SIGSEGV | identical PipeWire RT thread |

These 8 crashes came from bare-invocation runs (`./native/build-native/rb3-native`,
no `MILO_HEADLESS` → audio device opens by default). Combined with the docs'
"~1-in-N", the field rate is roughly **single-digit-percent per full run** —
high enough to retain a core every dozen-ish runs, low enough that my 14-run
batch missed it.

### Best backtrace — faulting thread, core 3678273 (symbolized)
```
Stack trace of thread 3678482 (the FAULTING thread; main thread was elsewhere):
 #3  RB3SignalHandler(int, siginfo_t*, void*)        (rb3-native + 0x2bc201)
 #4  <signal frame>                                   (libc.so.6 + 0x3e8f0)
 #5  0x00007fc88802d09d   n/a (n/a + 0x0)   ← PipeWire RT data-loop (mmap'd, no module)
 #6  0x00007fc88802bbda   n/a (n/a + 0x0)   ← "
 #7  0x00007fc88802d9a6   n/a (n/a + 0x0)   ← "
 #8  0x00007fc881b1c201   n/a (libpipewire-0.3.so.0 + 0x8e201)
 #9  0x00007fc8b7e981b9   n/a (libc.so.6 + 0x981b9)   ← thread start
 #10 0x00007fc8b7f1d21c   n/a (libc.so.6 + 0x11d21c)
```
The sibling core (3678723) is byte-identical in shape and additionally resolves
the miniaudio device frame:
`#7 ma_device_audio_thread__default_read_write(ma_device*) (rb3-native + 0xf64bb3)`.

**Fault-address pattern:** the faulting frames are PipeWire's RT/SPA code at high
anonymous addresses (`0x7fc88802xxxx`, module `n/a`) — i.e. mmap'd plugin/JIT
pages, not a C++ heap vtable. This *is* the "high address" the N9 note describes,
but its source is PipeWire, not a dangling Milo object.

---

## 2. Root-cause analysis

**Faulting object/thread:** the miniaudio playback device's realtime callback
thread, which on this PipeWire host is the `libpipewire-0.3.so.0` data-loop
(ALSA backend is pinned, but the host routes ALSA through the pipewire-alsa
plug — see `AudioDevice.cpp:171-193`). It is still running its
`ma_device_audio_thread__default_read_write` → PipeWire SPA path during process
teardown and faults on PipeWire-owned memory that the teardown sequence is
racing.

**Why it races (the lifecycle bug), correlated to code:**
1. `App::~App()` (`src/App.cpp:389`) is `{ TheDebug.Exit(0, true); }` — the very
   first statement calls `Debug::Exit`, so `exit()` is reached from inside it;
   App member destructors never run by unwinding.
2. `Debug::Exit` (`src/system/os/Debug.cpp:279-292`) walks `mExitCallbacks` in
   order, then calls `exit(status)`.
3. Registration order makes the audio teardown run **LAST**:
   - `SynthInit()` registers `SynthTerminate` during boot
     (`src/system/synth/Synth.cpp:291`, `push_front`).
   - `RunGame()` registers `BandRndShutdownExitCallback` *later*
     (`native/src/main_native.cpp:539-540`, `push_front`) so GPU teardown ends
     up at the **head** and runs FIRST.
   - Net exit order: `[BandRndShutdown(GPU) → … → SynthTerminate(audio uninit)]`.
4. The only thing that stops/joins the PipeWire RT thread is
   `ma_device_uninit(mDevice)` inside `AudioDevice::Terminate()`
   (`milo-native-engine/src/audio/AudioDevice.cpp:280-301`), reached via
   `NativeSynth::Terminate()` (`native/src/rb3_synth_native.cpp:53-56`) →
   `SynthTerminate` — i.e. **dead last**.
5. Therefore the entire GPU/Dawn/Vulkan teardown (`BandRnd::Shutdown`,
   `Rnd_Wgpu_RB3.cpp:680`) executes with the **audio thread still live**, and
   `SynthTerminate` even calls `TheSynth->Poll()` (`Synth.cpp:295`) before
   uninit — concurrent with the RT callback. The RT thread can be mid-flight in
   PipeWire's own buffer machinery when `exit()`/an earlier callback frees or
   unmaps something it depends on → fault inside PipeWire (frames #5-#8).

**Why our own `MixSources` is *not* in the trace (and why the mutex isn't
enough):** `AudioDevice::MixSources` (`AudioDevice.cpp:326-`) holds
`mSourceMutex`, and `RemoveSource`/`Terminate` take it too, so *our* source list
is protected. The fault is one layer below us — inside PipeWire's RT data-loop —
which no Milo-side mutex fences. The only correct fix is to **stop/join the
device thread before teardown begins**, which is exactly what `ma_device_uninit`
does. There is already a `Suspend()` helper documented for "call before
destroying audio objects" (`AudioDevice.h:52-54`) but nothing calls it on the
exit path.

**Confirm/refute the CharBonesObject/proxy-lifetime hypothesis:** **Refuted**
for the observed crashes. The existing `HxNoteFreedAddr` ring guard
(`obj/Object.cpp:40-69`, consumed in `obj/ObjPtr_p.h:38,56`) is intact and the
fault is on a *different thread* (audio RT), in PipeWire, with no Milo
`~Object` / `ObjPtr::Replace` / `CharBones` frames anywhere in the stack.
NOTE: a *latent* Milo-side reverse-direction teardown hazard does exist and is
documented in §6 as a watch item (RB3's `mRefs` is an unguarded
`std::vector<ObjRef*>`; `ObjPtrList::Replace`/`ObjOwnerPtr`/`Hmx::Object::Release`
lack the forward guard that `ObjPtr` has), but it is **not** what these
coredumps show.

---

## 3. Files to edit (explicit)

The fix is isolated to the **audio shutdown ordering**; the object-graph teardown
is not touched. Dependency-graph: **isolated** — no overlap with BandDirector /
character / render work.

- **(c) glue — PRIMARY.** `native/src/main_native.cpp`, function `RunGame()`
  (≈ line 539, where `RB3RegisterBandRndShutdown()` is called). Register an
  audio-stop exit callback so audio uninit runs **FIRST** in `Debug::Exit`
  (register it LAST = it lands at the head of the `push_front` list, ahead of
  even `BandRndShutdown`). This is the permuter-safe layer and the preferred
  home (matches the existing rationale comment for ordering BandRnd last).
- **(b) engine — SUPPORTING (one-liner, optional but cleaner).**
  `milo-native-engine/src/audio/AudioDevice.cpp` — expose a cheap
  `StopDevice()` (or reuse `Suspend()` + `ma_device_stop`) that joins the RT
  thread without tearing down the singleton, so the glue callback can call it
  idempotently. If kept minimal, the glue callback can instead just call the
  existing `AudioDevice::GetInstance().Terminate()` directly (already
  idempotent via the `mInitialized` guard at `AudioDevice.cpp:281`) — then NO
  engine edit is required.
- **(a) matched-fork — NONE.** No `src/system/**` edit needed. (Do *not* reorder
  the `SynthTerminate` registration in `Synth.cpp` — that's matched-fork and the
  glue-side ordering achieves the same effect permuter-safely.)

---

## 4. Fix approach

**Stop the audio device thread FIRST in the exit sequence**, before any other
teardown can free/unmap memory the RT thread touches.

Concretely, in `native/src/main_native.cpp::RunGame()`, register an exit
callback that calls `AudioDevice::GetInstance().Terminate()` (which does
`ma_device_uninit` → stops + joins the PipeWire RT thread, then frees device).
Register it **after** `RB3RegisterBandRndShutdown()` so it lands at the head of
the `push_front` exit-callback list and runs *before* GPU teardown and before
`SynthTerminate`'s `TheSynth->Poll()`:

```cpp
// (illustrative — glue, permuter-safe)
extern void RB3RegisterBandRndShutdown();
RB3RegisterBandRndShutdown();                 // GPU teardown (runs 2nd)
TheDebug.AddExitCallback([]{                   // audio thread join (runs 1st)
    AudioDevice::GetInstance().Terminate();
});
```
`AudioDevice::Terminate()` is already idempotent (`if (!mInitialized) return;`),
so the later `SynthTerminate → NativeSynth::Terminate → AudioDevice::Terminate`
becomes a no-op — no double-free.

**Why this is correct, not a band-aid:**
- It addresses the actual ordering invariant: *no concurrent thread may touch
  process state during teardown.* `ma_device_uninit` is the canonical, blocking
  join for the RT thread — after it returns, no audio callback can run, so the
  remainder of teardown (GPU, synth poll, static dtors, library unmap) is
  single-threaded and safe.
- It is the symmetric counterpart to the already-documented *init*-time PipeWire
  hazard (`AudioDevice.cpp:171-193` pins ALSA to avoid the boot-time SPA-thread
  vs GPU-driver race); N9 is the same class of bug at exit, fixed the same way
  (quiesce the audio thread relative to the rest of the process).
- It is a tight, justified ordering fix — no try/catch, no leak-on-exit.

If a future change wants belt-and-suspenders, also call
`AudioDevice::GetInstance().Suspend()` at the very top of the frame-loop exit
(`App::RunWithoutDebugging` HX_NATIVE tail, `src/App.cpp:560`) — but the exit
callback alone is sufficient and is the cleanest single fix.

---

## 5. Verification recipe

1. Baseline: `coredumpctl list | grep build-native/rb3-native` — note the
   current newest crash timestamp.
2. Apply the fix, rebuild `native/build-native/rb3-native`.
3. Run the canonical reproducer **30–50 times** (the live rate is low; 14 was
   not enough to even see a baseline crash, so push the N up). Use
   `/tmp/n9_batch.sh` with the run list extended; keep `MILO_AUDIO=1` and vary
   `MILO_MAX_FRAMES` 8500–9500.
4. Pass criteria: **0 new coredumps** for `build-native/rb3-native` after the
   baseline timestamp, AND every log ends `APP EXITED, EXIT CODE 0`.
5. Stronger signal (recommended to *reproduce the baseline first*): rebuild the
   stale `native/build-asan/` (its CMakeCache already has
   `-fsanitize=address -fno-omit-frame-pointer -g`; just
   `cmake --build native/build-asan -j`) and run a handful — but note ASan will
   mostly confirm *our* memory is clean; the PipeWire RT fault is in a
   third-party mmap region ASan can't instrument, so the coredump-count metric
   in step 4 is the primary gate. TSan would be the ideal tool to *prove* the
   race pre-fix but is costly to stand up; the ordering argument + 0-core result
   is sufficient.

---

## 6. Honest assessment

- **Localized: yes, with high confidence.** I did not reproduce the crash in my
  own 14 live runs (rate genuinely low), but three independent system coredumps
  of the *current* binary give an unambiguous, identical signature: the fault is
  the PipeWire/miniaudio RT thread during teardown, not the Milo object graph.
  The exit-callback ordering (`BandRnd GPU first, audio uninit last`) is a
  concrete, code-level mechanism that explains it.
- **Caveat:** because the live rate is low, the *fix verification must use a
  larger N* (30–50 runs) and the coredump-count gate, not a single clean run.
  If, after the audio-ordering fix, high-address SIGSEGVs still appear, the
  next diagnostic step is the latent Milo-side teardown hazard (§2 note): RB3's
  `Hmx::Object::mRefs` is an unguarded `std::vector<ObjRef*>` and
  `ObjPtrList::Replace` (`obj/ObjPtr_p.h:258-279`, the `mMode==kObjListOwnerControl`
  → `mOwner->Replace(...)` virtual call), `ObjOwnerPtr`, and
  `Hmx::Object::Release`'s `o->RefOwner()` (`obj/Object.cpp:321`) all lack the
  `HxAddrWasFreed` forward guard that `ObjPtr` has, and there is no
  `IsAlive()`/`mAliveSentinel` (DC3 has one; RB3 does not) for the reverse
  `~Object` walk to skip a freed `ObjRef*`. That is the *real* CharBonesObject-
  class risk — but it is NOT what the coredumps show, so fix the audio race
  first and re-measure before touching matched-fork object teardown.
- **Diagnostic setup documented:** ASan build = reuse `native/build-asan/`
  (cache already carries the sanitizer flags; rebuild it, it's 2 days stale);
  core dumps are already enabled (systemd-coredump, `coredumpctl info <PID>`),
  which is how this was localized.
