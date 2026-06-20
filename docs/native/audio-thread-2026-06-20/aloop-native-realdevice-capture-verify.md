# Native real-device audio capture + verify (snd-aloop / PipeWire loopback)

**Date:** 2026-06-20
**Worktree branch:** `wt-aloop-capture` (both rb3 and milo-native-engine)
**Status:** WORKS — native real-device playback verifies as the correct, un-clipped song.

## TL;DR

The native miniaudio output path had **never** been verified on a real audio device
(every prior native audio check used the internal `DC3_DUMP_AUDIO` float tap or the
`null` miniaudio backend). This wave builds a self-contained harness that routes
rb3-native's miniaudio stream to a **real, capturable audio sink**, records what the
sink actually received off the live audio clock, and runs `audio_verify.py` on it.

**Result:** `VERDICT: MATCH` — chroma corr **0.95** (same song), speed **1.001x** (no
chipmunk), pitch **1.000x**, clip-ratio **0.00%** (un-clipped), fingerprint BER **0.18**
(same recording). The native real-device audio path is correct end to end.

## What the device situation actually is on this host

The task said "point miniaudio at `hw:Loopback`, capture with `arecord`." That turned
out to be blocked, for a concrete reason worth recording:

- `snd-aloop` IS loaded — card 3 "Loopback", `/dev/snd/pcmC3D0p`/`pcmC3D0c` (dev 0) and
  `pcmC3D1p`/`pcmC3D1c` (dev 1).
- **`arecord`/`aplay`/`alsa-utils` are NOT installed.** Only `ffmpeg` + PipeWire tools
  (`pw-record`, `pw-cat`, `pw-cli`, `pactl`, `wpctl`) are available.
- **The run user `free` is NOT in the `audio` group**, and `/dev/snd/*` is `root:audio`
  mode `0660`. So a direct ALSA `hw:Loopback` open is **"Permission denied"**, and any
  `hw:CARD=Loopback` name resolution fails with *"Cannot get card index for Loopback"*
  (the default ALSA `ctl` is routed to PipeWire, which exposes no raw cards).
- PipeWire + WirePlumber run, but WirePlumber **claimed zero hardware cards** (same
  audio-group gap), and there is **no pulse socket** (`pactl info` -> connection
  refused), so miniaudio's PulseAudio backend can't connect either. miniaudio has **no
  native PipeWire backend** (only PulseAudio/ALSA/JACK).

### The path that DOES work (no audio-group, no hardware)

Create a **software PipeWire null-audio-sink** (no hardware, no audio-group needed),
point miniaudio's **ALSA backend** at the ALSA `pipewire` PCM with
`PIPEWIRE_NODE=<sink>` so the device-callback PCM frames land in that sink, and capture
the sink's monitor with `pw-record -P stream.capture.sink=true`. The audio still travels
the full **miniaudio data-callback -> ALSA pipewire plugin -> PipeWire graph** path on a
real ~realtime audio clock — i.e. the real device pump, into a software sink we are
allowed to read.

Proven primitive (440 Hz tone round-trips through the sink, Goertzel confirms exactly
440 Hz at the captured side; max level -21 dB).

> If a future host grants `audio` group access, pass `--device "hw:CARD=Loopback,DEV=0"`
> and capture `hw:Loopback,1` with arecord/ffmpeg instead — the engine's new
> `MILO_AUDIO_DEVICE` selector handles a literal ALSA hwid identically.

## Engine change — `MILO_AUDIO_DEVICE` device selector

`engine/src/audio/AudioDevice.cpp` previously only ever opened the **default** device
(`config.playback.pDeviceID == NULL`). Added an explicit selector:

```
MILO_AUDIO_DEVICE=<id>   # e.g. "pipewire", "default", "hw:CARD=Loopback,DEV=0"
```

When set, `Init()` enumerates the context's playback devices (`ma_context_get_devices`)
and matches `<id>` against each device's `id.alsa` string (exact, else substring), then
passes `&selectedId` to `ma_device_init`. Logs the pick:

```
AudioDevice: MILO_AUDIO_DEVICE=pipewire -> picked ALSA device [1] id='pipewire' name='PipeWire Sound Server'
```

Native-only (Linux `#if defined(__linux__) && !defined(__EMSCRIPTEN__)` block, runs only
when an explicit backend context exists). No effect on the Wii build or on hosts that
don't set the env. Match-neutral (engine/src/audio is not in the Wii `.o` set).

## The harness — `scripts/native/aloop-capture-verify.py`

Self-contained. Run from the rb3 repo root:

```bash
python3 scripts/native/aloop-capture-verify.py \
    --bin native/build-native/rb3-native \
    --data orig-assets/extracted \
    --song-target antibodies --secs 24
```

(Use a venv python with numpy/scipy, e.g.
`/home/free/code/milohax/dc3-decomp/venv/bin/python3`, for the `audio_verify` step.)

Steps:
1. `pw-cli create-node` a persistent `rb3_loop` null-sink (idempotent; reused if present).
2. Launch rb3-native with `MILO_AUDIO=1 MILO_AUDIO_BACKEND=alsa MILO_AUDIO_DEVICE=pipewire
   PIPEWIRE_NODE=rb3_loop` + the boot-to-song_select nav.
3. **Deterministically scroll** the Music Library to `--song-target` over `/api/dta/eval`
   (`{{music_library get_highlighted_node} get_token}`), select it, commit guitar/expert
   + nofail + autohit, wait for `songMs > 2000` (gameplay underway).
   - **Important gotcha found:** the canonical "down once then select" nav lands on the
     `random_song` node, which picks a *nondeterministic* song each run — fatal for a
     reference verify (it produced WRONG-SIGNAL / ambiguous identity until fixed). Always
     target a named song.
4. `pw-record --target rb3_loop -P '{ stream.capture.sink=true }'` for `--secs` of live
   gameplay -> WAV.
5. Run `audio_verify.py --song <target> --section gameplay` (or `--rank-mode` for the
   discriminator). Cleans up the sink.

Default target `antibodies` (its `extracted/songs/antibodies/antibodies.mogg` is a symlink
to a real full xbox mogg, so `decode_reference.py` can build a ground-truth reference).
Other ready songs: `25or6to4`, `crosstowntraffic`.

## Verifier fix — `scripts/native/audio_verify.py` `loud_s` silence gate

The first MATCH-grade capture was falsely rejected `SILENT/ERROR` despite chroma 0.95 /
clip 0% / RMS 3377. Root cause: the silence gate used `clip_metrics()`'s `loud_s`, which
came from `active_bounds()` — the **single longest contiguous** loud run. A real-device
capture of a dynamic live mix dips below the relative loud threshold dozens of times per
song (measured **40 separate loud runs** in 18 s; longest contiguous run only 1.2 s) even
though **100% of windows are above the silence floor** and 76% above the relative loud
threshold. After the `skip_s=0.5` trim, `loud_s` fell to 0.7 s < 1.0 -> false SILENT.

Fix: added `loud_total_s()` (sums **all** loud windows, contiguity-independent) and use it
for `worst["loud_s"]`. `active_bounds()`'s longest-run span is still used to TRIM the
distortion region (which wants one clean span). The synthetic `--selftest` still passes
**6/6** (the genuine `silent` case still returns SILENT/ERROR). This is a general
robustness fix for any dynamic capture, not specific to the loopback.

## Empirical numbers (final integrated run)

```
capture   : /tmp/aloop_cap.wav  (24 s recorded, 3.4 MB, 48 kHz stereo s16)
mean vol -14.9 dB, max -0.8 dB, 0% near-silent windows, overall RMS ~3377
align lag/peak  +6.9s / 0.79      (locked)
chroma corr     0.95              (>=0.65 same song)  [STRONG at 18s overlap]
fingerprint BER 0.18              (<=0.3 same recording)
speed ratio     1.001x           (no chipmunk)
pitch ratio     1.000x
clip-ratio      0.00% / flat-top 0
crest           16.3 dB
VERDICT: MATCH   (exit 0)
```

## Why this matters for the broader audio-thread mission

Native already mixes on the miniaudio audio thread (off-main, stall-immune) — this is the
**reference benchmark** the off-main-thread WEB producer must reach. We have now proven,
on a real audio device, that the native mix/limiter/decode chain produces the correct song
un-clipped at the right rate. So when the web AudioWorklet/WasmWorker producer is built and
verified, "matches native" is a concrete, measured bar — not an assumption.

## Files

- Engine: `engine/src/audio/AudioDevice.cpp` — `MILO_AUDIO_DEVICE` selector (branch `wt-aloop-capture`).
- Harness: `scripts/native/aloop-capture-verify.py` (branch `wt-aloop-capture`).
- Verifier fix: `scripts/native/audio_verify.py` — `loud_total_s()` + silence-gate rewire (branch `wt-aloop-capture`).

## Coordinator: how to land

1. Cherry-pick the engine commit onto engine main, then bump `MILO_ENGINE_PIN` in
   `native/CMakeLists.txt`.
2. Cherry-pick the rb3 commit (harness + audio_verify fix) onto rb3 master.
3. The harness needs `pw-cli`/`pw-record` at runtime; document that (or fall back to a
   literal ALSA hwid if the run host grants `audio` group access).
