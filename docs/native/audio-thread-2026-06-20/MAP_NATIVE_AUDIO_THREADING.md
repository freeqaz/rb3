# MAP-NATIVE — native audio threading + snd-aloop real-device verification

Scope: map the NATIVE audio pipeline's threading (is mix already off-main? how
deep is the decode-ahead?), and specify EXACTLY how to wire snd-aloop / loopback
capture so rb3-native's *real device* output can be recorded and benchmarked.
This is the reference baseline for the web off-main-thread work (web is the
stall-prone side; native is already mostly stall-immune — quantified below).

Date: 2026-06-20. Engine pin `884ab17` (rb3 `native/CMakeLists.txt:`MILO_ENGINE_PIN).
Read-only mapping + empirical loopback probing on the dev host. No code changed
in the engine or shared decomp; one new self-contained harness script added
(`scripts/native/capture_realdevice_audio.py`).

---

## 1. Native threading — MIX IS ALREADY OFF-MAIN (confirmed)

The native build mixes on the **miniaudio audio thread**, not the main/render
thread. miniaudio runs its `dataCallback` on its own backend I/O thread; our
callback synchronously calls `MixSources`.

- `engine/src/audio/AudioDevice.cpp:146` `MaDataCallback(ma_device*, void* output, …)`
  → `ad->MixSources((float*)output, frameCount)`. Registered as
  `config.dataCallback = MaDataCallback` at `AudioDevice.cpp:186`, with
  `config.periodSizeInFrames = 512` (`:188`, ~10.7 ms at 48 kHz).
- `engine/src/audio/AudioDevice.cpp:367` `MixSources()` does the additive stem
  mix + the stereo-linked one-pole peak limiter + soft-clip, entirely inside the
  audio-thread callback. It takes `mSourceMutex` (`:376`) — the ONLY main-thread
  contention point (AddSource/RemoveSource/Suspend at `:343/:348/:356`), held for
  microseconds.

Consequence: a stalled/janky MAIN thread cannot starve the native mix the way it
starves web. On web the producer (`PumpAudio`) runs once per `requestAnimationFrame`
on the main thread (`rb3/src/App.cpp:569` `AudioDevice::GetInstance().PumpAudio()`);
on native there is NO `PumpAudio` — the audio thread pulls directly. So the web
"feed the buffer from a context that survives main-thread stalls" goal is, on
native, **already satisfied for MIX**.

### The ONE native starvation risk: DECODE falling behind

The audio thread pulls PCM from a per-channel ring that the **producer
(main/Poll thread)** fills by decoding the mogg/vorbis stems. If the main thread
stalls long enough that decode can't keep the ring fed, the audio thread runs
the ring dry and conceals it (hold-last + ramp). So native's residual risk is
decode latency, gated by how deep the ring is filled. Quantified next.

### Per-channel ring depth (the native decode-ahead)

Native plays the StreamReceiver `mBuffer` array directly as the ring (one ring
PER channel; a song has 11–15 stems → 11–15 receivers, each its own ring;
`AudioDevice` mixes them — `rb3/native/src/rb3_stream_receiver_native.cpp:72`).

- Physical ring array: `unsigned char mBuffer[0xC0000]` under `HX_NATIVE`
  (`rb3/src/system/synth/StreamReceiver.h:66`). `0xC0000` = 786,432 bytes.
- Logical ring size: `mRingSize = chunks * 0xC000`, `chunks = clamp(numBuffers, 2,
  sizeof(mBuffer)/0xC000 = 16)` (`rb3/src/system/synth/StreamReceiver.cpp:21-35`,
  the `#ifdef HX_NATIVE` block). So a fully-deep ring is **16 chunks × 0xC000 =
  786,432 bytes** max; the Wii build stays at the fixed 2-chunk `0x18000`.
- PCM format in the ring: **16-bit MONO** (`bytesPerFrame = 2`,
  `rb3_stream_receiver_native.cpp:293`). So 786,432 bytes = 393,216 frames.
- Decode-ahead in seconds (per channel, at 44100 Hz mix rate):
  - 16-chunk max ring: 393,216 / 44100 ≈ **8.9 s**.
  - 1 chunk (`0xC000` = 49,152 bytes = 24,576 frames): ≈ **0.557 s**.
  - The header comment cites `0xC0000 = 16 chunks ~= 9.1s` (`StreamReceiver.h:65`)
    — consistent.
- `numBuffers` per receiver comes from the engine's StandardStream config; the
  clamp guarantees ≥ 2 chunks (≥ ~1.11 s) even for a minimal stream. So native
  buffers between ~1.1 s (2-chunk floor) and ~8.9 s (16-chunk cap) of PCM ahead
  of the play cursor — orders of magnitude more headroom than the web SAB ring's
  ~140 ms adaptive floor. A main-thread stall would have to exceed the ring depth
  (seconds) to underrun, vs. the web ring's ~140 ms.

### Ring producer/consumer + the built-in underrun probe

- Producer (main/Poll thread): decoder → `StreamReceiver::WriteData()`
  (`StreamReceiver.cpp:48`) advances `mRingWritePos`/`mRingWrittenSpace`; the
  cursor-driven refill-send loop is `StreamReceiver::Poll` (`:130-211`).
- Consumer (audio thread): `RB3StreamReceiverNative::RenderAudio`
  (`rb3_stream_receiver_native.cpp:283`). Available bytes =
  `mRingWrittenSpace - ((mAudioReadPos - mRingReadPos) mod ringSize)` (`:314-321`).
  When `available < needed` it renders what it has and conceals the rest with
  hold-last + a ~3 ms fade ramp (`:380-387`) — the native analogue of the web
  worklet's concealment.
- Built-in low-water/underrun telemetry, env-gated **`RB3_AUDIO_UNDERRUN_LOG=1`**
  (`rb3_stream_receiver_native.cpp:154`), prints an atexit summary
  (`DumpUnderrunSummary`, `:102`):
  `[UNDERRUN-SUMMARY] activeCallbacks=… underrunEvents=… (…% of callbacks)
   underrunFrames=… minAvailFrames=… maxUnderrunRunFrames=…`.
  `minAvailFrames` is the low-water ring depth in frames — divide by the sample
  rate for the smallest decode-ahead margin observed in a run. Use this to
  CHARACTERIZE how far ahead the ring actually stays during gameplay (run a real
  song with `RB3_AUDIO_UNDERRUN_LOG=1` and read the summary). It is the native
  counterpart to the web `audio-stall-measure.mjs` instrumentation.

---

## 2. How rb3-native picks its OUTPUT device (file:line)

`AudioDevice::Init` (`engine/src/audio/AudioDevice.cpp:162`) builds a playback
`ma_device_config` (`:182`) and on Linux pins the backend to **ALSA** by default
(`ma_backend_alsa`, `:227`), overridable via `MILO_AUDIO_BACKEND=pulseaudio|alsa|
null|default` (`:211-229`). It then calls `ma_device_init(mContext, &config,
mDevice)` (`:237`).

CRITICAL: **`config.playback.pDeviceID` is NEVER set** — there is no device-id
selection anywhere in `AudioDevice.cpp` (grep for `pDeviceID` → 0 hits). So
miniaudio always opens its DEFAULT device. miniaudio's ALSA backend opens the
ALSA PCM named **`"default"`** first (`engine/src/audio/miniaudio.h:28393`,
`:28709`; shared-mode tries `dmix` first which doesn't exist as a PipeWire PCM,
so it falls through to `"default"`).

Net: to point rb3-native at a specific device today you must make that device the
ALSA **`"default"`** PCM (via PipeWire's default sink or an `~/.asoundrc`). There
is no env knob for a device id. **A clean enhancement (future, low-risk,
native-only):** read `getenv("MILO_AUDIO_DEVICE")`, enumerate via
`ma_context_get_devices`, and set `config.playback.pDeviceID` — would let the
harness target `hw:Loopback` directly without touching ALSA config.

Booting the full app with real audio (not the bare `.milo` loader): set
`RB3_GAME=1 RB3_HTTP=1 MILO_HEADLESS=1 MILO_AUDIO=1 MILO_AUDIO_BACKEND=alsa
RB3_DATA=<extracted>` (mirrors `scripts/native/capture_gameplay_audio.py:76` but
swaps `MILO_AUDIO_BACKEND` from `null`→`alsa`). `MILO_AUDIO=1` overrides the
headless audio-skip (`AudioDevice.cpp:173-178`). The bare `rb3-native` with no
`.milo` arg just exits — it is a loader tool; the app needs `RB3_GAME=1`.

---

## 3. snd-aloop / loopback wiring — EMPIRICAL RESULTS on this host

`snd_aloop` is loaded and the card is present:

```
/proc/asound/cards:  3 [Loopback ]: Loopback - Loopback
device nodes:        /dev/snd/pcmC3D0p (playback dev0), pcmC3D1c/pcmC3D0c (capture), controlC3
                     -> card INDEX is 3 (the NAME "Loopback" resolves to index 3)
Loopback PCMs:       pcm0p/pcm0c (device 0) and pcm1p/pcm1c (device 1), 8 subdevices each.
```

Loopback semantics: what you WRITE to `hw:Loopback,0,N` appears on the CAPTURE
side of `hw:Loopback,1,N` (and 1↔0). So the textbook recipe is:
play → `hw:Loopback,0,0`, record → `hw:Loopback,1,0`.

### BLOCKER found: raw `hw:Loopback` is permission-denied on this host

`alsa-utils` is NOT installed (no `aplay`/`arecord`). I built a minimal libasound
loopback probe (`/tmp/aloop_probe.c`, gcc + `-lasound`) — the same lib miniaudio
uses — and it FAILS to open the loopback at every name form:

```
hw:Loopback,0,0 / hw:3,0,0 / hw:CARD=Loopback,DEV=0 / plughw:3,0
  -> "Cannot get card index for …" ; snd_card_get_index("Loopback") = -19 (ENODEV)
  -> snd_ctl_open("hw:3") = -2
```

Root cause (proven):
- `id` → user `free` is in groups `free, qbittorrent, wheel` — **NOT in `audio`**.
- `/dev/snd/*` are `root:audio` mode `0660`; `getfacl` shows **no `user:free` ACL**
  (the SSH/login sessions have **no Seat** per `loginctl` → logind grants no
  device ACL). So `head -c0 /dev/snd/controlC3` → "Permission denied".
- Therefore any direct ALSA `hw:` open of the loopback is EACCES, and
  `snd_config_get_card` (which opens the control node to map name→index) fails.

This is an ENVIRONMENT permission gap, NOT a code/wiring gap. To use the raw
snd-aloop `hw:Loopback` path you must EITHER add `free` to the `audio` group
(`sudo usermod -aG audio free` + re-login) OR run with a seat/ACL. With that,
the textbook command works:
```
arecord -D hw:Loopback,1,0 -f S16_LE -r 48000 -c 2 capture.wav   # (needs alsa-utils + perms)
```

### WORKING path on this host: PipeWire null-sink loopback (no root, no /dev/snd)

The ALSA `"default"` and `"pipewire"` PCMs DO open here (proven:
`snd_pcm_open("default"/"pipewire", PLAYBACK) = 0`) — they route through the
user-session PipeWire socket, needing no `/dev/snd` perms. And a PipeWire
null-sink **`rb3_loop`** already exists and is set as the **default audio sink**:

```
pw-cli ls Node     -> node.name = "rb3_loop"  (media.class = Audio/Sink, id 43)
pw-metadata        -> default.audio.sink = {"name":"rb3_loop"}
```

So: rb3-native (`MILO_AUDIO_BACKEND=alsa` → miniaudio opens ALSA `"default"` →
PipeWire default sink) plays INTO `rb3_loop`; capture `rb3_loop`'s MONITOR with
`pw-record`. End-to-end PROVEN with a generated tone:
```
pw-record --target rb3_loop -P '{ stream.capture.sink=true }' cap.wav   # capture monitor
pw-play   --target rb3_loop tone.wav                                    # play into sink
-> cap.wav: 192512 frames @ 48000 Hz, RMS 0.0439 (non-zero = real signal flowed)
```
(A bare/empty capture reads 44 bytes / RMS 0; a real signal reads non-zero — the
detector works.)

If `rb3_loop` is ever missing, recreate it once:
```
pw-cli create-node adapter '{ factory.name=support.null-audio-sink \
    node.name=rb3_loop media.class=Audio/Sink object.linger=true \
    audio.position=[FL,FR] }'
wpctl set-default <id-of-rb3_loop>     # make it ALSA "default"
```

---

## 4. The verification harness (new, self-contained)

`scripts/native/capture_realdevice_audio.py` — runs rb3-native against a REAL
miniaudio device (`MILO_AUDIO_BACKEND=alsa`) and records what the device played:

- Auto-detects transport: if `pw-record` exists AND PipeWire default sink ==
  `--sink` (default `rb3_loop`) → **pipewire** (records the sink monitor); else
  → **aloop** (`arecord`/`pw-record` on `hw:Loopback,1,0`). Forceable via
  `--transport`. Verified: on this host auto-detect picks `pipewire`
  (`default sink = rb3_loop`, `pw-record` present, `arecord` absent).
- Prints capture WAV stats (frames/rate/RMS/peak) and the AudioDevice init line,
  then the `audio_verify.py --rank` command to rate identity vs. the song mogg.
- This is the missing REAL-DEVICE counterpart to `capture_gameplay_audio.py`,
  which only dumps the engine's INTERNAL mix buffer (`MILO_AUDIO_BACKEND=null` +
  `DC3_DUMP_AUDIO` → `MixSources` WriteWav at `AudioDevice.cpp:449`).

Recommended benchmark run (once driven into a real song via `--nav` or the HTTP
input API):
```
python3 scripts/native/capture_realdevice_audio.py --secs 15 --out /tmp/rb3_dev.wav
python3 scripts/native/audio_verify.py --rank /tmp/rb3_dev.wav --data orig-assets/extracted
# and characterize ring margin:
RB3_AUDIO_UNDERRUN_LOG=1 <boot rb3-native into a song>  # read [UNDERRUN-SUMMARY] minAvailFrames
```

NOTE: a real-song capture requires driving into gameplay (nav script / HTTP
`/api/input`) — a bare boot only yields menu/preview audio. The device PATH is
already proven (tone end-to-end); the remaining step to a NUMBER is feeding a
nav-into-gameplay script, same as `capture_gameplay_audio.py` does for the
internal-dump path.

---

## 5. Takeaways for the web off-main-thread mission

- Native is the GOLD reference: MIX is on the audio thread (stall-immune), and
  the decode-ahead is **~1.1–8.9 s per channel** vs. web's ~140 ms SAB ring.
  That is the structural difference the web fix should aim to close — not by a
  bigger SAB ring, but by producing audio off the main thread so a long-task
  can't empty even a small ring.
- The web producer (`PumpAudio`, rAF, main thread) is the single point the web
  port must move off-main; native simply has no such site. Confirm the web
  spike's success by reproducing native's stall-immunity: a main-thread longtask
  injected during playback should NOT raise `underrunEvents` once production runs
  off-main (mirror `RB3_AUDIO_UNDERRUN_LOG` semantics into the worklet via
  `audio-stall-measure.mjs`).

## File:line citations
- Mix on audio thread: `engine/src/audio/AudioDevice.cpp:146,186,367,376`
- Period size 512: `AudioDevice.cpp:188`
- Backend pin / no device-id: `AudioDevice.cpp:211-229,237` (no `pDeviceID`)
- miniaudio opens "default": `engine/src/audio/miniaudio.h:28393,28709`
- Web pump site (main thread): `rb3/src/App.cpp:569`
- Ring physical size: `rb3/src/system/synth/StreamReceiver.h:66` (`mBuffer[0xC0000]`)
- Ring logical size / chunk clamp: `rb3/src/system/synth/StreamReceiver.cpp:21-35`
- 16-bit mono frame: `rb3/native/src/rb3_stream_receiver_native.cpp:293`
- Consumer available-bytes + concealment: `rb3_stream_receiver_native.cpp:314-321,380-387`
- Underrun probe: `rb3_stream_receiver_native.cpp:102,154` (`RB3_AUDIO_UNDERRUN_LOG=1`)
- Card index 3 = Loopback: `/proc/asound/cards`, `/dev/snd/pcmC3D0p`
- New harness: `scripts/native/capture_realdevice_audio.py`
