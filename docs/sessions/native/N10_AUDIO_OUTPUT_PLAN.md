# N10 — Make game audio AUDIBLE on this host (plan)

> Read-only diagnostic + plan. No source edited, no commits. Investigated
> 2026-05-29 at engine pin / rb3 HEAD as built in `native/build-native/`.
>
> **One-line verdict:** This headless box has **no usable real audio output**
> (all sinks are HDMI with no display attached → `eld_valid 0`). The realistic
> N10 win is "prove the audio pipeline opens a device and mixes samples" via the
> miniaudio **null backend**, which is compiled in but currently unreachable
> from the env knob. Audibility is deferred to a host with real output.

---

## 1. Host audio reality — is audible output even possible here?

**No.** Every output on this machine is an HDMI port with nothing plugged in.

`/proc/asound/cards` — three cards, all HDA NVidia / HD-Audio Generic:
```
 0 [NVidia_1 ]: HDA-Intel - HDA NVidia
 1 [NVidia   ]: HDA-Intel - HDA NVidia
 2 [Generic  ]: HDA-Intel - HD-Audio Generic
```

`/proc/asound/pcm` — **every PCM is `HDMI N : playback 1`**. There is no analog
/ line-out / speaker / headphone PCM anywhere on any card.

**ELD (the decisive fact):** `/proc/asound/card{0,1,2}/eld#0.0` all report
```
eld_valid   0
```
`eld_valid 0` = no monitor/receiver connected to that HDMI output. ALSA
enumerates the HDMI PCMs but **cannot open** them (no downstream sink) → this is
the direct cause of the `-401` below.

**PipeWire is running** (user `free`, socket `/run/user/1000/pipewire-0`) but
exposes **zero `Audio/Sink` nodes** — `pw-cli ls Node` shows only
`Dummy-Driver` and `Freewheel-Driver`. So PipeWire offers no real sink either.

**No PulseAudio server:** `pactl info` → `Connection refused`; `pipewire-pulse`
is **not installed** (`which pipewire-pulse` → not found) and
`/run/user/1000/pulse/` is empty. So `MILO_AUDIO_BACKEND=pulseaudio` has no
server to connect to.

**Virtual-sink options (require root — not done here):** `snd-aloop.ko.zst` and
`snd-dummy.ko.zst` exist on disk but `modprobe` needs root (we are uid 1000).
These would create a real openable ALSA PCM, but loading them is a host-admin
action outside this read-only task.

> Bottom line: there is **no openable real output device** for an unprivileged
> process on this host today.

---

## 2. Engine audio-init analysis (where/why -401, what knobs exist)

All audio init lives in the **engine layer (b)**:
`milo-native-engine/src/audio/AudioDevice.cpp` (miniaudio vendored at
`milo-native-engine/src/audio/miniaudio.h`).

- **`MILO_AUDIO=1`** forces audio on even under `MILO_HEADLESS` /
  `DC3_NO_AUDIO` — `AudioDevice.cpp:146-151`.
- **`-401` = `MA_FAILED_TO_OPEN_BACKEND_DEVICE`** (`miniaudio.h:4272`). The
  device enumerates but won't open — exactly the HDMI-with-no-ELD case.
- **Backend selection (Linux), `AudioDevice.cpp:171-210`:** the default is
  **pinned to ALSA, single backend, no fallback list** (`backends[1]`,
  `backendCount = 1`, `backends[0] = ma_backend_alsa`). This pin exists on
  purpose — the in-source comment (lines 171-183) documents that PulseAudio's
  libpulse/SPA-plugin loader threads `dlopen` EGL/GL plugins against the same
  NVIDIA driver the renderer is initialising, SIGSEGV'ing ~90% of headless
  boots deep in `libnvidia-eglcore`.
- **`MILO_AUDIO_BACKEND` accepts exactly three values** (`AudioDevice.cpp:184-193`):
  - `alsa` (or unset / anything-else) → `ma_backend_alsa`, single backend.
  - `pulseaudio` → `ma_backend_pulseaudio`, single backend.
  - `default` → `backendCount = 0` → calls `ma_device_init(nullptr, …)` so
    miniaudio walks its **full default order** (pulse → alsa → jack → … → null).
  - There is **no `null` value and no way to reach the null backend directly.**
- **Fallback path (`AudioDevice.cpp:203-206`):** if the explicit context fails
  to init, it falls back to `ma_device_init(nullptr, …)` (default order). This
  is why the `pulseaudio` run below logs `backend=default`.
- miniaudio's **null backend is compiled in** — the engine only defines
  `MA_NO_ENCODING` and `MA_NO_GENERATION` (`AudioDevice.cpp:10-13`); it does
  **not** define `MA_NO_NULL`. `ma_backend_null` is always openable and
  produces a real ~realtime clock, so the data callback (`MixSources`) runs and
  the WAV-dump path (`DC3_DUMP_AUDIO`) works against it.

---

## 3. What I tried + result (success = `ma_device_init` opens a device)

Reproducer = the task's command, trimmed to `MILO_MAX_FRAMES=400`.

| `MILO_AUDIO_BACKEND` | log line | `ma_device_init` | process |
|---|---|---|---|
| *(unset → alsa)* | `ma_device_init failed: -401` | **FAIL** (-401) | clean exit 0, audio skipped |
| `pulseaudio` | `initialized — 44100 Hz … backend=default` | **SUCCESS** (fallback) | **SIGSEGV (139)** right after init |
| `default` | `initialized — 44100 Hz … backend=default` | **SUCCESS** (default order) | **SIGSEGV (139)** right after init |

Key observations:
- ALSA (the default) genuinely can't open a device here → `-401`, then audio is
  cleanly skipped and the sim runs to completion. **Current behavior is correct
  and non-regressing.**
- `pulseaudio` / `default` *do* open a device — but the explicit pulse context
  fails, the code falls back to `nullptr`/default order, and a background
  SIGSEGV fires immediately after `AudioDevice: initialized`
  (`/tmp/n10_pulse.log` tail: crash at `0x7fc88802d09d`, a shared-lib mmap
  address, not our code — i.e. the exact libpulse/SPA-vs-NVIDIA fault the
  ALSA-pin comment warns about). GpuDevice has already fully initialised
  ("GpuDevice: initialized") before audio, so the contention window is real.
- So today there is **no env value that both opens a device AND survives** on
  this host: `alsa` = no device, `pulseaudio`/`default` = device but crash.

I did **not** load `snd-aloop`/`snd-dummy` (needs root) and did **not** start
`pipewire-pulse` (not installed). The miniaudio **null backend was never
reached** because the selection code has no path to it.

---

## 4. Explicit plan (recommended fix)

**This is NOT pure-config-solvable on this host today** — every config knob
either fails to open a device or crashes. The clean win needs a small, additive,
opt-in **engine (layer b)** change to expose the always-openable null backend,
plus an optional host-admin runbook for a real virtual sink.

### 4a. Engine change (recommended) — add a `null` backend value

File: `milo-native-engine/src/audio/AudioDevice.cpp`, in the
`MILO_AUDIO_BACKEND` parse block at **lines 184-193**. Add one branch:

```cpp
else if (beEnv && strcmp(beEnv, "null") == 0)
    backends[0] = ma_backend_null;   // always-openable dummy clock; proves pipeline
```

- Layer: **(b) engine**, additive, opt-in. Default (unset → ALSA) is unchanged,
  so the current "skip audio cleanly if no device" behavior does **not** regress.
- Result: `MILO_AUDIO=1 MILO_AUDIO_BACKEND=null` → `ma_device_init` SUCCEEDS
  against the null device → `MixSources` runs on miniaudio's null clock → the
  full mix/synth pipeline executes and `DC3_DUMP_AUDIO=/tmp/out.wav
  DC3_DUMP_SECONDS=20` captures real mixed PCM to disk. That is the measurable
  "audio pipeline proven" success criterion, with **no GPU-driver contention**
  (null backend opens no plugin threads), so **no SIGSEGV**.
- This is tiny enough it likely needs **no worktree** — a one-line additive edit
  plus a rebuild of `native/build-native`.

### 4b. Optional second engine tweak — ALSA→null auto-fallback (defensive)

Still in `AudioDevice.cpp`: when the explicit-context path returns
`-401`/`MA_FAILED_TO_OPEN_BACKEND_DEVICE` (lines 199-207), instead of bailing,
optionally retry once with `ma_backend_null` **only when `MILO_AUDIO=1` is set**.
This makes `MILO_AUDIO=1` mean "I really want the audio pipeline running even
with no hardware," while leaving the no-`MILO_AUDIO` headless default to skip
cleanly. Keep this gated so normal headless test runs never silently spin a null
device. (Lower priority than 4a; 4a alone satisfies N10.)

### 4c. Host runbook for ACTUAL audible output (no engine change)

For a host with real output (or to make THIS box audible), the pure-config path
is:
1. Install + run a Pulse server: `pipewire-pulse` (Arch pkg `pipewire-pulse`),
   then `systemctl --user start pipewire-pulse`. With a server present,
   `MILO_AUDIO=1 MILO_AUDIO_BACKEND=pulseaudio` opens the *explicit* pulse
   context (not the crashy fallback) — though the NVIDIA/SPA contention risk in
   the source comment still applies on headless GPU boots; test it.
2. **OR** create a real ALSA virtual sink as root:
   `sudo modprobe snd-aloop` (or `snd-dummy`) → gives an openable PCM →
   `MILO_AUDIO=1 MILO_AUDIO_BACKEND=alsa` succeeds and `DC3_DUMP_AUDIO` captures
   it; route the loopback capture side to anything that records.
3. **OR** plug a real HDMI display/receiver into a port — `eld_valid` flips to
   `1`, the HDMI PCM becomes openable, and `MILO_AUDIO_BACKEND=alsa` works and is
   genuinely audible.

---

## 5. Honest assessment

This headless server **genuinely has no usable audio output** for an
unprivileged process: all PCMs are HDMI with no display attached
(`eld_valid 0`), PipeWire has no sink node, and there is no Pulse server. So
"audible on this box" is **not achievable without host-admin action** (load
`snd-aloop`/`snd-dummy` as root, install `pipewire-pulse`, or attach an HDMI
display).

What IS achievable and is the right N10 deliverable: **prove the audio pipeline
works** by routing to the miniaudio **null backend** (4a). That opens a device,
runs `MixSources` on a real clock, and lets `DC3_DUMP_AUDIO` write a verifiable
WAV — distinguishing "the engine produces correct mixed audio" from "you can
hear it through speakers." The dump WAV is the audible-equivalent artifact you
can copy off-box and play.

- **Config-only fix?** No — current knobs can't both open AND survive here.
- **Engine change scope?** One additive line at
  `milo-native-engine/src/audio/AudioDevice.cpp:184-193` (layer b), opt-in via
  `MILO_AUDIO_BACKEND=null`; optional defensive fallback at lines 199-207.
- **Confidence:** High on the diagnosis (-401 = HDMI/no-ELD; pulse/default open
  but SIGSEGV; null compiled in but unreachable — all directly observed).
  High that the `null` branch will make `ma_device_init` succeed and the WAV
  dump capture real PCM. The pulse-context-without-crash path (4c.1) is
  unverified on this box (no server installed) — medium confidence there.
