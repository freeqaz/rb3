# BUILD SPEC — MVP-1: off-main web-audio (path A, worklet-side stem mix)

**Date:** 2026-06-20
**Author:** spec agent (resolves every open question against real source for the build agent)
**Builds on:** `03-DECISION-offmain-web-audio.md` (path A, DECIDED) + `04-ADVERSARIAL-REVIEW-decision.md`
(C1/C2/C3) + `SPIKE_WORKLET_SIDE_MIX.md` (0% under-run through 1500 ms proof).
**Engine pin context:** `884ab17` (rb3 `native/CMakeLists.txt:MILO_ENGINE_PIN`).
**Status:** BUILD-READY. No re-design needed. Implement exactly this; measure against the
pinned gate.

> This spec is prescriptive. Where the spike (`spike-worklet.js`) and the real engine
> differ, the **real engine wins** and the difference is called out. The single biggest
> correction to the spike model is in §2: **the real stems are NOT lockstep** — each
> `RB3StreamReceiverNative` is an independent `AudioSource` with its own ring, own read
> cursor, own vol/pan, and (critically) its own producer back-pressure. The worklet must
> drive each stem's cursor protocol independently, exactly as `RB3StreamReceiverNative::
> RenderAudio` does today on the audio thread.

---

## 0. The named flag

**`RB3_WEB_OFFMAIN_MIX`** — runtime env flag, read via `getenv()` in C++, default **OFF**.

- It is **not** a CMake/emcc build option. The web build already plumbs `RB3_*` env into
  the wasm `getenv()` two ways: the URL `?env=RB3_WEB_OFFMAIN_MIX=1` bridge
  (`native/web/rb3_pre.js:130-157`, seeds `Module.ENV` → `environ` → `getenv`) and the
  fixed allowlist in `main_web.cpp`. Use the `?env=` bridge for the bench (no rebuild to
  toggle); optionally add it to the `main_web.cpp` allowlist if a persistent default is
  wanted later. **No new emcc flags. No `-pthread`/`-sAUDIO_WORKLET`/`-sWASM_WORKERS`.**
- The flag is read **once** in `AudioDevice::Init` (`AudioDevice_Web.cpp`) into a static
  `bool sOffMainMix`. Everything below branches on it.

**Exactly what the flag switches (and what it does NOT):**

| Component | OFF (shipping, untouched) | ON (MVP-1) |
|---|---|---|
| `js_audio_init` | creates ONE output SAB (`HEADER+RING_SAMPLES*f32`), posts `{type:'init', sab, bufFrames}` | additionally allocates N **stem SABs** + 1 **control SAB**; posts `{type:'init-offmain', stemSabs:[…], ctrlSab, ringFrames, mixRate, ctxRate, primeFrames}` |
| worklet `process()` | drains the one pre-mixed output ring (current code path) | mixes N stem rings on the audio thread (int16→float + vol/pan + limiter + resample) |
| `PumpAudio` | full path: adaptive law → `MixSources` → resample → `js_audio_ring_write` | **decode/top-up pump only**: copy each stem's freshly-decoded PCM into its stem SAB, publish vol/pan/state into the control SAB, write the shared target-depth int. NO mix, NO resample, NO output-ring write. |
| `MixSources` | called by `PumpAudio` | **not called by `PumpAudio`** (still compiled; SFX second pass may reuse a trimmed variant — see §4) |
| limiter (`mLimiterEnv`) | owned/updated in `MixSources` (main thread) | **moves into the worklet** (audio-thread-owned) |
| resampler carry (`mResamplePos`/`mResampleCarry*`/`mResampleCarryN`) | owned/updated in `PumpAudio` (main thread) | **moves into the worklet** (audio-thread-owned); main-side fields go **dead** (see §3) |
| output latency floor | 140 ms adaptive, rides to 500 ms ceiling | fixed **≤80 ms** worklet-internal prime/output floor; main side stops driving output depth |
| SFX (`RB3SampleInstNative`) | mixed in `MixSources` | **stays on a main-thread second pass** (NOT stall-immune) — §4 |

A single `MILO_WEB_AUDIO_NS`-namespaced worklet (`milo-audio-processor`) handles both modes
by branching on which init message it received (`init` vs `init-offmain`). This keeps DC3
(which only sends `init`) byte-unaffected.

---

## 1. Per-stem SAB layout (header + int16 ring + atomic cursors)

### 1.1 Decision: copy-into-SAB on `WriteData`, NOT SAB-backed `mBuffer` — FORCED by the memory model

Open-Q #3 is **not a tradeoff; it is forced**. The shipping web build is **single-threaded,
unshared wasm** (`native/CMakeLists.txt`: no `-pthread`, no `-sSHARED_MEMORY`, no
`-sAUDIO_WORKLET`/`-sWASM_WORKERS`; only `-sJSPI`). Therefore the wasm linear memory
(`HEAPU8`/`HEAP16`) is a plain `ArrayBuffer`, **not** a `SharedArrayBuffer`. `mBuffer`
(`StreamReceiver.h:66`, `unsigned char mBuffer[0xC0000]`) lives inside the C++ object on
that heap. The AudioWorklet runs on a different thread and can ONLY read a real `SAB`. So
the worklet **cannot** read `mBuffer` in place — there is no way to "SAB-back" a member
array without flipping the whole module to shared memory (that is path B's build risk,
explicitly out of scope). **Copy-into-SAB is the only option.** This is also the
zero-build-risk path the decision rests on.

The copy is cheap and bounded: the pump only copies the **newly decoded bytes** each tick
(the producer write delta), not the whole 9 s ring — see §3.2.

### 1.2 One SAB per stem (per `RB3StreamReceiverNative`)

Mirror the existing producer ring 1:1 so the worklet's drain is a direct port of
`RB3StreamReceiverNative::RenderAudio`. The PCM stays **int16 mono at the mix rate (44100)**
— do NOT pre-convert to float or pre-resample on the main thread (that work moves to the
audio thread). Ring length in frames = the producer's `mRingSize / 2` (int16 mono → 2
bytes/frame). For the full 16-chunk ring that is `0xC0000/2 = 393216` frames ≈ **8.9 s**.

```
Stem SAB byte layout (one per stem):
  Int32   [0]  writePos   (FRAMES; producer = main/pump thread; Atomics.store, release)
  Int32   [1]  readPos    (FRAMES; consumer = worklet;          Atomics.store, release)
  Int32   [2]  ringFrames (FRAMES; const after init, = mRingSize/2)
  Int32   [3]  generation (bumped by producer when this slot is (re)assigned to a stem;
                           lets the worklet detect a stem swap and reset its per-stem
                           resampler phase + fade state — see §1.4)
  Int16   [8 bytes header .. ]  ring PCM, mono, `ringFrames` int16 samples
```

Header = **16 bytes** (4 × Int32), then `ringFrames` × `Int16`. Total per stem ≈
`16 + 393216*2` ≈ **786 KB**. With up to `MAX_STEMS` slots (see §1.3) that is a few MB of
SAB total — acceptable (the decision and review both bless "a few MB").

> NOTE the unit change from bytes→frames. The producer side (`StreamReceiver`/
> `rb3_stream_receiver_native.cpp`) tracks the ring in **bytes** (`mRingWritePos`,
> `mAudioReadPos`, `mRingSize` are byte offsets, 2 bytes/mono-int16-frame). The SAB cursors
> are in **frames** to match the int16 sample index the worklet uses. The pump converts
> byte→frame when publishing `writePos`; the worklet converts frame→sample (×1) when
> reading. Keep one consistent unit per side; do the conversion only at the SAB boundary.

### 1.3 Fixed-slot stem table (control SAB) — avoids per-song SAB churn

Allocate a **fixed pool of `MAX_STEMS` stem SABs once at `Init`** (default `MAX_STEMS = 16`
— a song has ~6–15 stems; 16 covers worst case with headroom; bump if a song exceeds it).
Stems are assigned to slots dynamically (a song's `RB3StreamReceiverNative`s register via
`AddSource`; the pump maps each live stem source to a free slot). This avoids
allocating/freeing SABs and re-`postMessage`-ing the worklet on every song change (which
would race the audio thread). Re-using a slot for a new stem bumps that slot's `generation`
(§1.4).

```
Control SAB byte layout (one, shared, small):
  Int32   [0]  activeMask    (bitfield: bit s set => slot s is a live, playing stem)
  Int32   [1]  targetDepthFrames (the adaptive target the worklet uses for its prime/output
                                   floor; written by the pump — see §3.3; ctx-rate frames)
  Int32   [2]  mixRate       (const, 44100)
  Int32   [3]  ctxRate       (const, the AudioContext rate read back at init)
  // per-slot params, indexed by slot s in [0, MAX_STEMS):
  Float32 [4 + s*4 + 0]  gain     (linear; = source mVolume)
  Float32 [4 + s*4 + 1]  pan      (-1..+1; = source mPan)
  Int32   [4 + s*4 + 2]  flags    (bit0=paused, bit1=finished; advisory — worklet still
                                   reads availability from the stem ring cursors)
  Int32   [4 + s*4 + 3]  generation (mirror of the stem SAB's generation; lets the worklet
                                   pair a param update with the slot's current stem)
```

All control-SAB scalars are written by the pump (main thread) and read by the worklet
(audio thread) with `Atomics.load`/`Atomics.store`. They are **advisory** for correctness:
the worklet derives "how many frames can I play" purely from each stem's `writePos`/`readPos`
(the SPSC handshake), exactly like the native `RenderAudio` derives it from
`mRingWrittenSpace`/`mAudioReadPos`. Vol/pan are read per-quantum (a stale value for one
128-frame quantum is inaudible — same race-benign reasoning as the producer cursors).

### 1.4 Independent per-stem cursors — the spike's lockstep assumption is WRONG for the real path

The spike advanced **all** stems by the same `toRead` and used stem 0's readPos as a global
play cursor (`spike-worklet.js` `_minAvailable` + the single `rp0`). That is valid only
because the spike's synthetic producer wrote all rings in perfect lockstep. **The real
engine does not.** Each `RB3StreamReceiverNative`:
- has its **own** `mBuffer` ring filled by its **own** `StandardStream`/decode at its own
  rate (decode is per-channel; channels can be momentarily ahead/behind each other),
- has its **own** `mAudioReadPos` consumer cursor + its **own** producer back-pressure
  (`mRingReadPos` gated behind the play cursor via `SendDoneImpl`),
- can be individually **paused/finished** (e.g. a muted stem, or a stem that ends early).

Therefore the worklet MUST, per quantum:
1. For each active slot, compute `available_s = writePos_s - readPos_s (mod ringFrames_s)`.
2. Mix each stem for `min(quantumOutFrames_needed_in_mix_frames, available_s)`; a stem that
   is momentarily short contributes silence for the missing tail (its own hold-last/ramp),
   **without** starving the others. (A real under-run is "the SLOWEST stem ran dry"; but one
   stem ending or briefly lagging must not zero the whole mix.)
3. Advance each stem's `readPos_s` by exactly the mix frames it consumed.

This is the SAB analogue of `MixSources` iterating `mSources` and calling each
`RenderAudio` independently. **Do not collapse the stems to a single shared cursor.**
The `generation` field lets the worklet reset a slot's per-stem fade/phase when the slot is
reassigned to a different stem mid-session.

> Resampler note (see §3): because resampling is **post-mix** (one resampler on the bus,
> §3.1), the per-stem advance is in **mix-rate frames** and the resampler converts the mixed
> bus from mix→ctx rate. So the worklet's outer loop is: produce one 128-frame **ctx-rate**
> output quantum → that needs ≈ `128 * mixRate/ctxRate` **mix-rate** bus frames → pull that
> many mix-rate frames from each stem (additive sum + per-stem vol/pan) → run the post-mix
> resampler+limiter → emit 128 ctx frames. The number of mix-rate frames consumed per stem
> per quantum is identical across stems (they all advance by the bus frame count actually
> consumed by the resampler), so the per-stem `readPos` advances are equal **in count** but
> each is still bounded independently by that stem's own availability.

---

## 2. The worklet mixer (port of `MixSources` + limiter + resampler to the audio thread)

Add a new mode to `engine/src/platform/web/assets/audio-worklet.js`, gated by the
`init-offmain` message. Reuse `spike-worklet.js` as the structural template (prime gate,
hold-last+ramp concealment, the `underrun-stats` postMessage shape — all already match the
bench). Replace the synthetic pieces with the real DSP. The worklet emits **stereo**
(stems are mono → vol/pan spreads to L/R).

### 2.1 The mix body (per output quantum of `frames` ctx-rate frames)

```
ctxRate, mixRate            // from init message
step = mixRate / ctxRate    // mix-rate frames consumed per ctx-rate output frame
// 1) figure out how many mix-rate bus frames this quantum needs (carry-aware, §2.3)
//    needBusFrames = ceil(resamplePos + (frames-1)*step) + 2 - carryN
// 2) build the mix-rate stereo BUS for [carryN .. needBusFrames-1]:
busBuf[0..carryN-1] = resampleCarry      // frames carried from previous quantum
for f in [carryN .. needBusFrames-1]:
    accL = 0; accR = 0
    for each active slot s:
        avail_s = (writePos_s - readPos_s) mod ringFrames_s
        if (busFrameOffset(f) < avail_s):
            int16 m = stemPCM_s[(readPos_s + busFrameOffset(f)) mod ringFrames_s]
            float sample = m * (1/32768)
            gain_s = ctrlF[gain index]; pan_s = ctrlF[pan index]
            (vl, vr) = ComputePanGains(gain_s, pan_s)   // SAME as the native helper
            accL += sample * vl
            accR += sample * vr
        // else: this stem is short for this frame -> contributes 0 (its own dropout)
    busBuf[f*2+0] = accL; busBuf[f*2+1] = accR
// 3) resample the mix-rate bus -> ctx rate (linear, carry-all) into the 128-frame output
// 4) per output frame: apply the one-pole stereo-linked limiter (state persists on `this`)
// 5) apply hold-last + ramp concealment for any frames not produced (true under-run only)
// 6) advance each active stem's readPos by the bus frames actually consumed (§2.3)
```

`busFrameOffset(f)` is `f - 0` measured from each stem's own `readPos_s`; because all stems
advance by the same consumed-bus-frame count, a single integer offset indexes every stem's
ring (each modulo its own `ringFrames_s`). Track availability per stem so one lagging stem
doesn't poison the others (§1.4).

### 2.2 Limiter — moves to the worklet, audio-thread-owned (C2, the #1 risk)

Port the **exact** `MixSources` master-bus limiter (`AudioDevice_Web.cpp:536-555`) into the
worklet, byte-for-ear identical:
```
kLimThreshold = 0.90, kLimReleaseMs = 80.0, kSoftKnee = 0.95
aRel = exp(-1 / (ctxRate * (kLimReleaseMs/1000)))   // NOTE: at the CTX rate (output rate)
env persists on `this.limiterEnv` (init 1.0)         // audio-thread-owned now
per output (ctx-rate) frame:
  level = max(|L|,|R|); desired = level>thr ? thr/level : 1
  if (desired < env) env = desired                   // INSTANT attack
  else env = aRel*env + (1-aRel)*desired             // one-pole release
  L = SoftClip(L*env); R = SoftClip(R*env)
```
- **Where it runs:** the limiter runs **post-resample, at ctx rate** (the output rate), on
  the final stereo bus. Today's `MixSources` runs it at `mSampleRate` (mix rate) **before**
  the resample. Moving it post-resample is correct (it limits exactly what hits the
  speakers) and is the natural place in the worklet. Recompute `aRel` with `ctxRate`
  (release time in ms is rate-independent; the coefficient must use the rate it runs at).
  This is a deliberate, documented behavioral choice; verify via `audio_verify.py` (0% clip)
  that it still matches native (native limits at mix rate, but the limiter is content-
  adaptive and the audible difference at 44100 vs 48000 release coefficients is sub-perceptual
  — the gate is "MATCH chroma≥0.65, 0% clip", which it must clear).
- **`mLimiterEnv` on the main side goes DEAD when the flag is ON** (see §3 enumeration). It
  is still updated by `MixSources` for the SFX second pass (§4) and for the flag-OFF path,
  so do not delete the member — just stop relying on its value for the music bus.
- `SoftClip` and `ComputePanGains` must be reimplemented in JS in the worklet (they are tiny;
  match the C++ exactly — `ComputePanGains` is in BOTH `rb3_stream_receiver_native.cpp:117`
  and `rb3_sampleinst_native.cpp:79`, identical bodies).

### 2.3 Resampler carry — moves to the worklet, audio-thread-owned (C2, the #1 risk)

Port the **carry-all** linear resampler (`AudioDevice_Web.cpp:808-872`, the wave-08 fix that
killed the single-sample-drop "static/clipping" bug). State that moves into the worklet,
persisting on `this`:
```
this.resamplePos   (double, [0,1) read offset from carry[0])  // was mResamplePos
this.resampleCarry (Float32, kResampleCarryMax*2 stereo)      // was mResampleCarry[]
this.resampleCarryN(int)                                       // was mResampleCarryN
kResampleCarryMax = 8
```
The resample reads the **mix-rate stereo bus** (carry frames + freshly mixed frames) and
writes the **ctx-rate** 128-frame output. The carry/`resamplePos` bookkeeping is the SAME
algorithm as the C++ (carry every unconsumed bus frame stream-contiguous; `consumed =
floor(s)`; `leftover = needTotal - consumed`; clamp to `kResampleCarryMax`). **Do not carry
only one sample** — that is the exact regression the C++ comment warns about.

**The number of mix-rate bus frames CONSUMED this quantum = `consumed` from the resampler.**
That is the value each stem's `readPos_s` advances by. So step 6 of §2.1 is: for each active
stem, `readPos_s = (readPos_s + consumed) mod ringFrames_s`, `Atomics.store`. This keeps the
producer back-pressure correct (the pump reads these cursors to drive decode top-up, §3).

> Equal-rate fast path: if `ctxRate == mixRate` (browser honored 44100), `step == 1`, no
> resample — mix straight to output, limiter at mixRate==ctxRate. Keep this branch (matches
> the C++ `!resample` fast path) so the common-rate case is exact.

### 2.4 Prime gate ported to the stem-ring model (C3, named must-do) — §5

The worklet's start-up prime gate stays, but its "available" must be the **min across all
active stems** (the hungriest stem gates start), and the floor is fixed. See §5 for the full
prime-gate design.

### 2.5 Instrumentation parity (so the bench works unchanged)

The bench reads `window._rb3Audio.underruns` with fields `underrunEvents`, `underrunFrames`,
`totalQuanta`, `totalFrames`, `minRingDepthFrames`. The off-main worklet MUST post the same
`{type:'underrun-stats', …}` message shape (the JS `onmessage` in `js_audio_init` stashes it
to `window[key].underruns`). Define `minRingDepthFrames` here as the **min-across-stems
`available`** low-water mark (in ctx-rate-equivalent frames, i.e. `available_mixframes *
ctxRate/mixRate`, or just report mix-frames consistently — the bench converts via ctxRate, so
report in **ctx frames** to keep `minRing(ms)` correct). An under-run event = any quantum
where the hungriest active stem couldn't fill the needed bus frames AND sources were active.
Keep `_reportAccum >= (sampleRate>>1)` cadence identical.

---

## 3. Resampler/limiter state ownership move — every main-side read to remove (open-Q #1)

**This is the #1 correctness risk. Enumerate and verify ALL of these.** When
`RB3_WEB_OFFMAIN_MIX` is ON, these fields/uses must no longer drive the music bus on the
main thread. They are all in `engine/src/audio/AudioDevice_Web.cpp` + the header.

### 3.1 Resampler decision: POST-MIX, single instance on the audio thread (open-Q #2)

Resample the single mixed bus once (matches `MixSources`+resample shape today), NOT per-stem.
One resampler instance lives in the worklet. The spike confirmed pure-JS handles 6–15 stems ×
128-frame quanta with headroom. Per-stem resampling is rejected (N× the work, no benefit).

### 3.2 Main-side fields that go DEAD (music bus) when flag ON — REMOVE these reads from the music path

In `AudioDevice` (header `AudioDevice.h:108-119`) and `PumpAudio`/`MixSources`
(`AudioDevice_Web.cpp`):

| Field / read | Today (main thread) | Action when flag ON |
|---|---|---|
| `mResamplePos` | read+written in `PumpAudio` resample loop (`:825,847,862,872`) | **not touched by the off-main pump.** Worklet owns `this.resamplePos`. Main field stays initialized (Init `:423`) but unused for music. |
| `mResampleCarry[]` | read+written `:820-824, 867-870` | same — worklet owns `this.resampleCarry`. |
| `mResampleCarryN` | read+written `:820, 838, 871`; init `:424` | same — worklet owns `this.resampleCarryN`. |
| `mLimiterEnv` | read+written in `MixSources` limiter (`:539,554`) | **music bus limiter moves to the worklet.** `MixSources` is no longer called by `PumpAudio` for music. `mLimiterEnv` is still used by the SFX second pass (§4) and the flag-OFF path — keep the member, but it no longer limits the music. |
| `sMixBuffer` / `sOutBuffer` | mix + resample scratch in `PumpAudio` | not used by the off-main pump for music. (Reused by the SFX second pass if that path mixes via a trimmed `MixSources` — §4.) |
| the entire adaptive-latency law (`PumpAudio:575-767`) | computes `targetFrames` for the output ring | **replaced** by "publish `targetDepthFrames` to the control SAB" (§3.3). The output ring is no longer filled by the pump. |
| `js_audio_ring_write` | pushes mixed output to the one SAB ring | **not called for music.** (Still defined; the flag-OFF path uses it.) |

**Verification (do this, it is the gate for open-Q #1):**
1. `grep -n "mResamplePos\|mResampleCarry\|mLimiterEnv" engine/src/audio/AudioDevice_Web.cpp`
   and confirm every hit is either (a) inside the `if (!sOffMainMix)` flag-OFF branch, or
   (b) inside the SFX second pass (§4), or (c) the one-time `Init` reset. No music-bus read
   of these may remain on the main thread when the flag is ON.
2. Confirm no OTHER TU reads them: `grep -rn "mResamplePos\|mResampleCarry\|mLimiterEnv" \
   engine/src native/src rb3/src` — they are private members of `AudioDevice`, so the only
   legitimate readers are `AudioDevice_Web.cpp` (web) and `AudioDevice.cpp` (native, which is
   not compiled in the web build). Expect zero external readers. Document the grep result in
   the handoff.
3. There is **no cross-thread hazard** on these once moved: they become single-owner
   audio-thread state inside the worklet (a JS object field, never touched by the main
   thread). That is strictly safer than today.

### 3.3 `PumpAudio` demoted to a decode/top-up pump (flag ON)

```
PumpAudio() // flag ON:
  if (!initialized || !workletStarted) return
  // 1) For each live music source (RB3StreamReceiverNative), find its slot. Newly-added
  //    sources claim a free slot (bump generation, publish ringFrames/gen to its stem SAB
  //    + control SAB). Removed/finished sources free their slot (clear activeMask bit).
  // 2) For each active slot: publish the producer write frontier. The producer has decoded
  //    new PCM into mBuffer since last pump (via TheSynth->Poll() which already ran this
  //    rAF, App.cpp:562-563). Copy the NEW bytes [lastPublishedWrite .. mRingWritePos) from
  //    that stem's mBuffer into the stem SAB at the same ring position, then Atomics.store
  //    the stem SAB writePos = mRingWritePos/2 (byte->frame). Copy is bounded by the decode
  //    delta per tick (a few KB/stem at steady state), not the whole ring.
  // 3) Read back the worklet's readPos for each stem (Atomics.load) and feed it to the
  //    producer's back-pressure: the worklet's consume replaces the old RenderAudio. Two
  //    options (pick A; see §3.4):
  //      A) Make RB3StreamReceiverNative's mAudioReadPos a view of the stem SAB readPos
  //         (the pump copies worklet readPos*2 -> source mAudioReadPos each tick before
  //         the source's producer Poll runs), so SendDoneImpl/GetPlayCursor keep working
  //         unchanged. The source's own RenderAudio is no longer registered with the mixer.
  // 4) Publish per-slot gain/pan/flags into the control SAB (from source mVolume/mPan/paused).
  // 5) Compute the fixed target depth in ctx frames (§5 floor) and Atomics.store it to
  //     ctrl[targetDepthFrames]. No adaptive law.
  // NO MixSources for music. NO resample. NO output-ring write.
```

**Key consequence (the whole point):** during a main-thread stall, `PumpAudio` and
`TheSynth->Poll()` both freeze (same rAF tick, `App.cpp:562-569`), so the stem SABs stop
being topped up — but the worklet keeps mixing the **already-published** ~9 s of stem PCM.
The stall budget = the published stem-ring depth, decoupled from output latency. Exactly the
spike result.

### 3.4 How the off-main source registers with the mixer

When the flag is ON, `RB3StreamReceiverNative` must NOT be added to `AudioDevice::mSources`
as a `RenderAudio` source (the worklet renders it instead). Cleanest implementation
(native-only edit in `rb3_stream_receiver_native.cpp`, all under `#ifdef HX_NATIVE` and gated
on the same flag):
- Add a method `PublishToSAB(slot)` and `int* GetSABReadPosPtr()` used by the pump. The
  source keeps its existing producer state machine (`StartSendImpl`/`SendDoneImpl`/
  `GetPlayCursor`/`mAudioReadPos`) — the pump just **mirrors the worklet's SAB readPos into
  `mAudioReadPos`** before each producer Poll so back-pressure (`SendDoneImpl`,
  `mPlayedTotal`) advances exactly as if a local audio thread had consumed those frames.
- `PlayImpl` still arms the play cursor (`mAudioReadPos = mRingReadPos`), and the pump seeds
  the stem SAB readPos to the same value so the worklet starts at the song start (§5).
- Do NOT register `this` as an `AudioSource` when off-main (skip `AddSource`); the pump owns
  the slot lifecycle.

This keeps the producer/decoder loop **byte-identical** to today; only the consumer moves.

---

## 4. SFX stays on a main-thread second pass for v1 (C1, scoped out of stall-immunity)

`RB3SampleInstNative` (`rb3_sampleinst_native.cpp`) renders one-shots from a bank-resident
`mPCMData` + a fractional cursor — **not a deep ring**. Publishing every SFX instance's bank
PCM + cursor + lifecycle to the worklet is real work the spike never modeled. **v1 does NOT
move SFX off-main.** Documented limitation: **SFX is NOT stall-immune in MVP-1** (a 400 ms
main-thread freeze briefly hiccups SFX; acceptable because SFX is triggered by main-thread
input that the same stall already delays — the user's hard requirement is *music* through
jank, which path A delivers).

**Where SFX mixes into the bus (flag ON):**
- SFX sources still `AddSource` to `AudioDevice::mSources` (only the music
  `RB3StreamReceiverNative` skips registration, §3.4). So `mSources` now contains **only SFX**
  when the flag is ON.
- `PumpAudio` (off-main) runs a **second pass** for SFX: mix the SFX `mSources` via a trimmed
  `MixSources` into a small scratch buffer, resample (reuse the existing main-side
  resampler+`mLimiterEnv` — these stay alive precisely for SFX), and push to a **separate SFX
  output SAB** that the worklet **adds** to its mixed music bus before the limiter. OR (simpler
  v1) keep a dedicated low-latency SFX path: push SFX into the SAME single output ring the
  flag-OFF path uses, and have the worklet additively combine `(off-main music mix) + (SFX
  ring drain)` before the final limiter.
  - **Recommended v1:** the second approach — worklet final bus = `musicMix + sfxRingDrain`,
    then ONE limiter over the sum. This preserves the SFX top-up clamp behavior and keeps a
    single master limiter. The SFX ring keeps the **existing small adaptive floor / top-up
    clamp** (`AudioDevice_Web.cpp:769-774`) so one-shot SFX still fire promptly (open-Q #6 —
    the clamp that kept SFX from queueing behind a full ring MUST be preserved; it now governs
    only the shallow SFX ring, which is even better for latency).
- Because SFX is shallow + main-thread-fed, it inherits the old stall behavior — that is the
  documented, accepted v1 scope. Do not claim 0% under-run for SFX; claim it for **music**.

> Effort honesty (matches C1): the music path is the ~150-line port; SFX adds the second-pass
> plumbing above. Budget for it. If SFX second-pass plumbing risks the music gate, ship music
> off-main + SFX via the unchanged flag-OFF-style single ring first, then layer the combine.

---

## 5. Prime gate / song-start cushion under the fixed floor (C3, named must-do)

~85% of historical under-runs are the **song-start burst** before the ring is primed
(`audio-worklet.js:62-71`). With the output floor dropped to a fixed ≤80 ms, the cushion can
no longer come from a deep output ring — it must come from the **stem rings being pre-filled
before the worklet starts mixing.** Design:

1. **Worklet prime gate (ported, stem-aware).** The off-main worklet keeps a one-time prime
   gate: until **every active stem** has at least `primeFrames` of buffered audio
   (`available_s >= primeFrames` for all active slots), output silence and do NOT advance any
   stem readPos (do NOT count as under-run — same as today). `primeFrames` is computed from a
   **fixed cushion** (e.g. `round(ctxRate * 0.12)` ≈ 120 ms equivalent, expressed in
   mix-rate availability), independent of the output floor. Once primed, never re-arm
   (one-time start-up cushion; steady-state latency unaffected). This is the SAME gate shape
   as `spike-worklet.js:60-79` but keyed on the **min across stems**.
2. **Pre-fill before mixing.** The kInit→kPlaying handoff already leaves the song start
   resident in `mBuffer` ahead of `mRingWritePos` (`rb3_stream_receiver_native.cpp:190-215`
   PlayImpl + the kInit fast-accept prime). The pump publishes that primed PCM into each stem
   SAB on the first ticks after `Play()`, so by the time the worklet's prime gate clears, the
   stems already hold the song start. No song-start data is dropped (the prime gate holds the
   read cursor at the start).
3. **Fixed output floor (≤80 ms), tuned not guessed (open-Q #5).** The worklet's own output
   buffering is just the AudioContext's internal latency (≈ a few 128-frame quanta) plus the
   prime cushion — there is no large output ring. Set the **fixed floor to 80 ms initially**,
   then **tune DOWN toward 60 ms** using the worklet low-water instrumentation from the bench:
   run the sweep, read `minRing(ms)` at steady state, and pick the smallest floor that keeps
   `minRing` comfortably > 0 at the 0 ms-stall step (no jitter under-runs). The floor here is
   the **prime cushion + the control-SAB `targetDepthFrames`** the pump publishes; the pump
   does not need a big ring because stall tolerance now comes from the stem rings. Verify the
   latency log stays at the floor (does NOT ride to 500 ms) across the whole sweep — that is a
   pinned success criterion.

**Why this preserves the dominant failure mode's fix:** the start-up burst is absorbed by the
stem-ring pre-fill + the worklet prime gate, NOT by a deep output ring. Shrinking the output
floor cannot reintroduce the start glitch because the cushion source moved upstream.

---

## 6. Match-neutrality + files touched

All edits are **native/web-only** (NOT in the Wii `.o` set) → free to edit:
- `engine/src/platform/web/assets/audio-worklet.js` — add the `init-offmain` mixer mode.
- `engine/src/audio/AudioDevice_Web.cpp` — flag read; allocate stem+control SABs; demote
  `PumpAudio` to top-up pump under the flag; SFX second pass; publish target depth.
- `engine/src/audio/AudioDevice.h` — add stem-SAB pool members + `sOffMainMix` plumbing
  (web-only `#ifdef __EMSCRIPTEN__` block; keep `mResample*`/`mLimiterEnv` for SFX + flag-OFF).
- `rb3/native/src/rb3_stream_receiver_native.cpp` — `#ifdef HX_NATIVE`, flag-gated: skip
  `AddSource` when off-main; add `PublishToSAB`/SAB-readPos mirror hooks. **No change to the
  producer state machine** (byte-identical decode/back-pressure).
- `rb3/src/App.cpp` — **no change needed**; `PumpAudio()` is already called at `:569` and now
  does the top-up. (If a `?env=` allowlist entry is wanted, that is `main_web.cpp`, native-only.)

`rb3/src/system/synth/StreamReceiver.{h,cpp}` are shared Wii decomp — **do not edit** (the
`HX_NATIVE` ring-size block already exists; the SAB copy reads `mBuffer` from the native
bridge, no header change). No new build flags affect the Wii image.

Engine edits go in the **paired engine worktree**; commit to the worktree branch; report
branch+SHA; do NOT push or bump `MILO_ENGINE_PIN` (coordinator lands).

---

## 7. Success gate (objective, pinned) — what the build agent must measure

1. **Stall sweep (PRIMARY).** `scripts/web/audio-stall-bench.mjs --stalls 0,50,100,200,400,800
   --step-secs 12 --interval 250` on the full rb3-web build with `?env=RB3_WEB_OFFMAIN_MIX=1`:
   - **MUSIC under-run ~0% across the whole sweep.** PRIMARY: **0% at 400 ms AND 0% at 800 ms**
     (baseline breaks at 800 ms = 38.43%, per `STALL_BENCH_BASELINE.md`).
   - Achieved at a **fixed ≤80 ms output latency floor** — verify the latency log stays at the
     floor (does NOT ride to 500 ms) through the sweep.
   - Capture `curve.csv` before (flag OFF) and after (flag ON); compare directly to the
     baseline doc. Run the flag-OFF curve too to prove the shipping path is untouched.
2. **Correctness (PRIMARY).** `scripts/native/audio_verify.py` (or `web-audio-capture.mjs`)
   on a captured real song with the flag ON → **MATCH: chroma ≥ 0.65, ~1.0× speed, 0% clip**,
   converging to the native real-device reference (`aloop-native-realdevice-capture-verify.md`).
   This is the gate that proves the limiter+resampler-carry move (§2.2/§2.3) is correct — the
   wave-08 "static/clipping" class of bug shows up here. Treat it as a real go/no-go.
3. **Open-Q #1 verification.** Paste the grep results from §3.2 step 1–2 into the handoff
   (proof no main-side music-bus read of `mResamplePos`/`mResampleCarry*`/`mLimiterEnv`
   remains when the flag is ON, and no external TU reads them).

---

## 8. Ranked risk list (build agent: attack in this order)

1. **[HIGHEST] Resampler-carry + limiter state hand-off (C2 / open-Q #1).** The carry-all
   resampler is the exact code whose single-sample-drop caused the prior "static/clipping"
   regression (MEMORY: audio-verify wave-08). Porting it to JS must carry EVERY unconsumed
   bus frame (not one). The limiter env must persist on `this` and use the ctx-rate release
   coefficient. **Gate: `audio_verify.py` MATCH + 0% clip.** Test with a real song, not a sine
   (the bug is jitter-gated and invisible on a constant-cadence tone).
2. **[HIGH] Per-stem independent cursors (§1.4).** The spike's lockstep model is wrong; real
   stems have independent rings + back-pressure + pause/finish. A bug here = wrong mix / one
   stem starving all / desync. Mirror `RenderAudio`'s per-source availability math exactly.
3. **[HIGH] Producer back-pressure via mirrored SAB readPos (§3.4).** If the pump doesn't feed
   the worklet's consumed-frame count back into each source's `mAudioReadPos`/`mPlayedTotal`,
   `SendDoneImpl` never releases chunks → decode stalls after the prime (the exact
   chicken-and-egg deadlock the native bridge comment warns about). Verify decode keeps
   refilling through a full song.
4. **[MED] Prime gate under fixed floor (C3 / §5).** If the stem pre-fill + stem-aware prime
   gate isn't right, the floor-shrink reintroduces the song-start burst (~85% of historical
   under-runs). Gate: 0% under-run in the FIRST window of each song-start in the bench.
5. **[MED] SFX second-pass combine (C1 / §4).** Getting the music+SFX sum + single limiter +
   SFX top-up clamp right without regressing SFX latency. If risky, ship music-off-main first
   with SFX on the unchanged single-ring path, then layer.
6. **[MED] Slot lifecycle / generation (§1.3-1.4).** Song change reassigns slots; a stale
   `generation` or a not-cleared `activeMask` bit = a dead stem still mixed or a live stem
   dropped. Reset per-slot worklet phase/fade on generation bump.
7. **[LOW] Output-floor tuning (open-Q #5).** Start 80 ms, tune to 60 ms from observed
   low-water; purely a tuning pass once correctness is green.
8. **[LOW] Flag-OFF regression.** Confirm `RB3_WEB_OFFMAIN_MIX` unset leaves the shipping path
   byte-identical (DC3 unaffected: it only ever sends `init`, never `init-offmain`). Run the
   baseline curve with the flag OFF and confirm it matches `STALL_BENCH_BASELINE.md`.

---

## 9. Quick reference — load-bearing source cites (all verified this pass)

- Stem ring (9 s int16 mono): `src/system/synth/StreamReceiver.h:55-69` (`mBuffer[0xC0000]`),
  ctor sizing `src/system/synth/StreamReceiver.cpp:11-37` (`mRingSize = chunks*0xC000`).
- Producer write: `StreamReceiver::WriteData` `StreamReceiver.cpp:48-65` (byte ring,
  `mRingWritePos`/`mRingWrittenSpace`/`mRingFreeSpace`).
- Consumer drain (the thing the worklet ports): `RB3StreamReceiverNative::RenderAudio`
  `native/src/rb3_stream_receiver_native.cpp:283-414` (availability math `:314-322`;
  int16→float `:351,366-367`; vol/pan `:117-121`; SPSC cursor `:60-72,395`).
- Web mixer/limiter/resampler-carry: `engine/src/audio/AudioDevice_Web.cpp` — `MixSources`
  + limiter `:499-556`; `PumpAudio` + adaptive law `:560-767`; SFX top-up clamp `:769-774`;
  carry-all resampler `:808-872`; SAB ring write `:267-294`; Init/SAB alloc `:394-462`.
- Resampler/limiter state (to move): `AudioDevice.h:96,107-119`.
- Current worklet (template): `engine/src/platform/web/assets/audio-worklet.js` — prime gate
  `:62-87,107-134`; hold-last+ramp `:44-60,161-176`; underrun-stats post `:118-130,186-204`.
- Spike mixer template: `.claude/worktrees/worklet-mix/scripts/web/worklet-mix-spike/spike-worklet.js`.
- SFX source (stays main-thread): `native/src/rb3_sampleinst_native.cpp:85-313` (RenderAudio
  `:267-302`, vol/pan `:79-83`).
- Pump site (no change): `src/App.cpp:560-570` (Poll then PumpAudio, same rAF tick).
- Flag plumbing: `native/web/rb3_pre.js:130-157` (`?env=RB3_*` → `getenv`); env reads already
  work in web (`AudioDevice_Web.cpp` already calls `getenv` for `RB3_AUDIO_LATENCY_MS`).
- Memory model (forces copy-into-SAB): `native/CMakeLists.txt` — `-sJSPI` only, NO
  `-pthread`/`-sSHARED_MEMORY`/`-sAUDIO_WORKLET` → wasm heap is a plain ArrayBuffer.
- Bench reads `window._rb3Audio.underruns`: `scripts/web/audio-stall-bench.mjs:154-187`;
  baseline curve to beat: `docs/native/audio-thread-2026-06-20/STALL_BENCH_BASELINE.md`.
