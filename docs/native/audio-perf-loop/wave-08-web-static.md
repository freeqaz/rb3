# Wave 08 — WEB "static that's clipping" (user re-report, 2026-06-08)

> Orchestrator framing. Audio waves 02→05 landed an instant-attack limiter
> (gain-clip) + a web-only 44100→ctx-rate linear resampler (chipmunk), both
> committed in the engine (`513dcd5`, `b458b18`) and present in the deployed wasm
> (`RESAMPLING` string confirmed in `native/web/build/release/rb3-web.wasm`,
> built 2026-06-08 08:50). Native audio is verified-correct. Yet the user now
> reports the WEB build "sounds like static that's clipping."

## What we already know (orchestrator recon)
- Deployed build (06-08 08:50) is NEWER than engine audio source (06-06 21:25);
  `RESAMPLING` present ⇒ NOT a stale-build miss of the resampler.
- Native + web limiters are BOTH instant-attack (`if (desired<env) env=desired`,
  AudioDevice.cpp:418 / AudioDevice_Web.cpp:489) ⇒ limiter-clip should be gone.
- The real 48000 resampling path was only ever validated with a PURE SINE TONE
  (`web-audio-tone-verify.mjs`); never with real music captured end-to-end. The
  headless box returns `ctx.sampleRate=44100` so it runs the 1:1 FAST path by
  default — the resampler must be FORCED on (ctx→48000) to exercise the user path.
- The ring "no underrun" measurement (wave-05) ran on the 44100 fast path, not the
  48000 resampling path.

## Prime suspect (orchestrator hypothesis H-RESAMP)
`AudioDevice_Web.cpp` PumpAudio resample loop (lines 558-604): `MixSources()` is a
destructive stream pull advancing by `newMix` frames/chunk, but the carry preserves
only ONE sample (`sMixBuffer[consumed]`). When `consumed != newMix` (math says ~92%
of chunk boundaries at step=44100/48000=0.91875), stream frame `F+newMix-1` is
SKIPPED → a 1-sample discontinuity each chunk = periodic clicks/broadband static on
MUSIC (invisible to a slowly-varying sine). Needs proof/refute against real PCM.

## Wave plan — investigations (read/measure only; fixes in converge)
- A — real in-browser MUSIC capture @ forced ctx=48000 (the user path) + clip/
  static/divergence metrics vs the ground-truth mogg. THE ground truth.
- B — resampler correctness audit: re-derive carry/consumed; host DSP unit test on
  real decoded mogg PCM (44100→48000), measure chunk-boundary click/noise energy.
- C — ring underrun/drift on the 48000 path (worklet drain vs PumpAudio cadence).
- D — boot-crash status of the deployed build (metamaterials Rand→MainThread); is
  audio even reachable, or is "static" pre-crash garbage?
- E — build provenance: deployed wasm from engine HEAD (both fixes) vs the PIN
  (neither); confirm the limiter is actually compiled-in & active on web.

---
(agents append `### <label>` sections below)

### probe:dc3-compare  (2026-06-08)

**Hypothesis (user's lead):** RB3's clipping is an RB3-specific pipeline/config defect
that the limiter MASKS, not something inherent to multi-stem summing — provable by
comparing against DC3 (same shared engine, "works fine" with no clipping hack).

**Verdict: SPLIT — the over-unity sum IS content-inherent to RB3's mix (15/11 stems vs
DC3's 2-stem pre-mix), NOT a pipeline bug; BUT the limiter already tames it cleanly to
0% hardclip / 0 flat-top runs. So the user's WEB "static that's clipping" is NOT this
multi-stem sum.** The decisive content/config divergences are real and below, but they
do not produce audible clipping after the limiter — they are why RB3 *needs* the limiter
and DC3 doesn't, not why web sounds like static.

**1. Compile wiring (confirmed by reading CMake + the actual files):**
- RB3-native: engine `src/audio/AudioDevice.cpp`, NOT excluded (rb3 native/CMakeLists.txt
  line 120-122, 158) → HAS the limiter (`mLimiterEnv`, `sPreGain`, instant-attack at :418).
- RB3-web: engine `src/audio/AudioDevice_Web.cpp` (rb3 native/CMakeLists.txt:651) → limiter
  (:472-495, instant-attack :489) + 44100→ctx linear resampler (:517-620).
- DC3-web: SAME engine `AudioDevice_Web.cpp` (dc3 native/CMakeLists.txt:1326) → identical
  limiter+resampler.
- DC3-native: its OWN LOCAL `dc3-decomp/native/src/audio/AudioDevice.cpp` (dc3
  CMakeLists.txt:1085/1142/1498) → NO limiter; `static float sMasterGain = 1.1f` (:35) +
  hard clamp to [-1,1] (:310-313); `mSampleRate = mDevice->sampleRate` (:181). This is the
  same naive structure RB3's OLD (pre-limiter) path had.

**2. CONTENT — decode (decrypt_mogg.py + ffprobe + ffmpeg f32le) + authored vols/pans
(songs.dta), constant-gain pan law mirroring rb3_stream_receiver_native.cpp ComputePanGains:**

| song | ch | rate | summed stereo peak (authored vols/pans, NO limiter/master_vol) | >1.0 frac |
|---|---|---|---|---|
| RB3 20thcenturyboy | **15** | 44100 | **1.64x** (+4.3 dBFS) | 0.030% |
| RB3 25or6to4 | **11** | 44100 | **1.80x** (+5.1 dBFS) | 0.038% |
| RB3 antibodies | **11** | 44100 | **1.80x** (+5.1 dBFS) | 0.018% |
| DC3 firework | **2** | 44100 | **0.66x** (-3.6 dBFS) | 0.000% |
| DC3 firework +master_vol -2 | 2 | 44100 | **0.52x** (-5.6 dBFS) | 0.000% |

  Channel-count histograms (pans entries in each songs.dta):
  - **DC3: 57/62 songs are 2-channel** (stereo PRE-MIX); only 5 are 8/10/12. ALL 9 extracted
    DC3 moggs I decoded are 2ch/44100.
  - **RB3: 138 songs, bulk 11-15 channels** (multi-stem); min 2, max 15. Histogram
    {2:1, 3:11, 4:35, 9:3, 10:7, 11:12, 12:18, 13:13, 14:23, 15:15}.
  ⇒ The over-unity sum is **CONTENT (channel count)**, not pipeline: RB3 ships multitrack
  stems that the port sums additively (15 sources), DC3 ships a 2-channel pre-mixed stereo
  master that needs no summation. RB3 sums to 1.6-1.8x, DC3 to 0.5-0.66x — a ~3x ratio.
  (Earlier wave-01 "~3.2x" was likely with the +1.1x master gain and/or a louder song; the
  raw authored-mix peak is 1.6-1.8x, which is still over the rail.)

**3. PIPELINE/config divergence (real, but DC3-safe-by-design):**
- DC3 `config/synth.dta`: `(master_vol -2)` + `(track_levels TRUE)`. **RB3
  `config/synth.dta` has NO master_vol and no compressor/limiter directive** (47 lines,
  grep master/compress/limit = nothing). So DC3 applies a global -2 dB; RB3 unity.
- DC3-native clips WITHOUT a limiter? On its 2ch content it CAN'T: even pre-master_vol the
  sum is 0.66x, and DC3-native's `sMasterGain=1.1f` brings the loudest 2ch master to ~0.72x
  — under the rail. DC3 "plays clean with no limiter" because its **content is a finished
  2-channel master**, not because of a clever pipeline. RB3 can't do that: it must sum stems.

**4. RATE:** REFUTED as a DC3-vs-RB3 discriminator. **Both DC3 and RB3 moggs are 44100 Hz**
(every one decoded). DC3-web is NOT avoiding the resampler by being 48000 — at the same ctx
rate it hits the SAME 44100→ctx resampler RB3 does. So if the resampler caused the static,
DC3-web would static too. (mogg ENCRYPTION versions differ — RB3=16, DC3=14 — but sample
rate is identical.)

**Limiter sufficiency (the clincher):** ran a sample-accurate port of the web limiter
(T=0.90, instant attack, rel 80 ms, SoftClip) on the RB3 25or6to4 loud 10s window:
- pre-limiter peak **1.57x**, hardclip 0.084% → **POST-limiter hardclip 0.0000%, flat-top
  runs≥3 = 0.** The limiter fully tames the over-unity multi-stem sum.
- DC3 firework: pre-limiter peak 0.42x (after master_vol -2) → limiter **never engages
  (transparent)**.

⇒ **The 15-stem sum is content-inherent AND already handled.** The user's web "static
that's clipping" is therefore NOT the multi-stem mix (that would show as sustained flat-top
runs, which the limiter eliminates → 0). It must be a separate WEB-path defect: prime
suspect remains the resampler chunk-boundary discontinuity (H-RESAMP, lane B) and/or
ring-drift (lane C), to be exercised at forced ctx=48000 with a REAL music capture (lane A)
— note DC3-web hitting the same resampler is a strong CROSS-CHECK: if lane-A music capture
shows static on RB3-web but a DC3-web 44100→48000 capture is clean, the static is
RB3-content-specific (its denser/transient spectrum exposes the 1-sample resampler skip),
which would still be a resampler bug, just content-revealed.

**Proposed fix (if the team wants RB3 to need the limiter LESS, independent of the static
bug):** add `(master_vol -X)` to RB3's synth.dta is NOT faithful (Wii AX hardware handled
this; RB3 left it unity) and would double-attenuate nothing in DC3 (separate file) — but a
cleaner port-level option is a small RB3-only `sPreGain < 1.0` (e.g. 0.6, already
DC3_AUDIO_GAIN-overridable) to drop the 1.8x sum to ~1.1x so the limiter barely works.
However this does NOT address the web static (limiter already yields 0 clip). The static
fix lives in the resampler/ring (lanes A/B/C), not here.

**Files/commands:** decrypt = `scripts/native/decrypt_mogg.py <mogg> <ogg>`; decode =
`ffmpeg -i ogg -f f32le -ac N -ar 44100`; mix matrices from `orig-assets/extracted/songs/
songs.dta` (RB3) and `dc3-decomp/orig-assets/extracted/songs/songs.dta` (DC3); master_vol
from each `config/synth.dta`. Scratch scripts: /tmp/w08_summix.py, w08_more.py,
w08_limiter.py.

---
### probe:resampler-static  (2026-06-08)

**Hypothesis (orchestrator H-RESAMP):** the web 44100→48000 LINEAR resampler in
`AudioDevice_Web.cpp` PumpAudio (517-619) drops a sample at chunk seams because
`MixSources()` is a DESTRUCTIVE stream pull (advances by `newMix` frames/chunk) but the
carry preserves only ONE sample. When `consumed != newMix`, mix frame `F+newMix-1` is
skipped → a 1-sample discontinuity per chunk = clicks/static on MUSIC (invisible to the
slowly-varying sine wave-05 tested).

**VERDICT: CONFIRMED — the resampler injects real per-seam CLICKS on music, but the
orchestrator's frequency framing was INVERTED. It is intermittent crackle (~4.7
clicks/sec @ 60fps), NOT continuous broadband "static," and is cadence-dependent.**

#### 1. Carry/consumed/newMix math re-derived (step = 44100/48000 = 0.91875)
- `newMix = floor(sEnd)+1` uses `(outChunk-1)*step`; `consumed = floor(sStart+outChunk*step)`
  uses `outChunk*step` (one extra `step`). Since `step<1`, `floor(x+step)-floor(x) ∈ {0,1}`,
  so **`consumed ∈ {newMix-1, newMix}` ALWAYS** — never `>newMix`. The line-601 guard
  `if(consumed>newMix)` is provably DEAD (verified 2M random chunks: range [-1, 0]).
- `consumed==newMix` ⇒ carry = `sMixBuffer[newMix]` = stream's last fresh frame ⇒ next
  chunk continues contiguously. CLEAN.
- `consumed==newMix-1` ⇒ carry = `sMixBuffer[newMix-1]`, but `MixSources` already pulled &
  committed frame `newMix` (cursor advanced by `newMix`). That frame is NEVER carried and
  the stream is past it ⇒ **1 mix frame DROPPED at the seam = a 1-sample read-position jump
  of `step+1`** = a slope discontinuity = a click.
- The orchestrator said "consumed!=newMix at ~92% of boundaries." It's the OPPOSITE:
  `consumed==newMix` ~91.9% (clean), `consumed==newMix-1` ~8.1% (the bug). So ~8% of chunks
  drop a frame, not 92%.
- **Why wave-05 missed it: with CONSTANT chunk sizes the phase is degenerate** — `1024*step`
  and `800*step` land such that `consumed==newMix` for 100% of chunks (0 skips). The bug only
  fires when `freeFrames` JITTERS (the real PumpAudio cadence, where the worklet drain varies).

#### 2. Host C++ unit test on REAL decoded mogg PCM (`/tmp/resamp_test.cpp`, verbatim port of
the 517-619 loop; stream = 30s of 20thcenturyboy decoded 15ch→stereo @44100 via
`decrypt_mogg.py` + ffmpeg, peak 0.256 / rms 0.034):
| chunk cadence | chunks | seam skips (consumed==newMix-1) | seam clicks/sec | length drift |
|---|---|---|---|---|
| const-800 (degenerate, ≈wave-05) | 5000 | **0** | **0** | 0 |
| const-1024 (degenerate) | 5000 | **0** | **0** | 0 |
| jitter N(800,120) (real 60fps) | 1593 | **138** | **4.6** | +148 frames |
- **Per-seam read-position gap = exactly +1.0 mix frame** (137 gap anomalies, maxGap 1.0000),
  i.e. each seam drops precisely one input frame.
- **Click energy (deviation from a smooth 4-tap predictor, at the 138 seam indices vs 3000
  control indices):** seam median **+20.9 dB** above control floor (11.1×); worst seam click
  reaches **−0.8 dB vs program RMS** (near full level → reads as a "pop"/"clip"). Median seam
  click ≈ −11 dB vs program. Continuous linear-interp imaging error vs soxr = only −20.6 dB
  (mild HF, NOT the dominant artifact).
- **Cadence scaling** (seam rate ∝ chunks/sec): 60fps(~800)=4.7/s, 30fps(~1600)=2.5/s,
  10fps(~4800)=0.8/s, 3fps(~8192)=0.5/s. So on a smooth web build it's worse (steady ~4.7
  ticks/s); on a stuttering build it's sparser.

#### 3. MixSources destructiveness CONFIRMED (not a phantom): `rb3_stream_receiver_native.cpp`
`RenderAudio()` (243-342) reads int16 from the ring at `mAudioReadPos`, advances `readPos`
per frame (329), and **commits `mAudioReadPos.store(readPos, release)` (341)** — the cursor
is published, driving the producer's send/back-pressure loop. So a frame pulled by
`MixSources(sMixBuffer+2, newMix)` is consumed-for-good; the `consumed==newMix-1` drop is a
genuine lost input frame, not re-readable.

#### Proposed fix (verified): carry ALL unconsumed fresh frames, not one sample.
Implemented in `/tmp/resamp_fix.cpp`: keep a small `carry[]` of the `filled-consumed` leftover
fresh frames (already pulled, abs-contiguous), prepend them to next chunk, MixSources only the
*additional* frames needed. Result: **gap anomalies 137→0, maxGap 1.0→0.0, drift unchanged
(+148, which is the legit 44100→48000 ratio), seam click residual −8.2 dB median / now within
the control p99 (clicks ELIMINATED)**. (Equivalent simpler fixes: mix exactly `consumed`
fresh frames per chunk so nothing is over-pulled; or replace the whole linear interp with a
proper polyphase/windowed-sinc resampler — also kills the −20 dB imaging floor.)

**Severity / honest scope:** this is a REAL bug that produces audible seam clicks ONLY on the
48000 resampling path (browsers that clamp ctx→48000; the headless test box returns 44100 and
runs the bug-free FAST path, which is why end-to-end web capture here can't see it without
forcing ctx→48000). At ~4.7 intermittent ticks/sec with occasional near-full-level pops it
reads as crackle and can read as "clipping," and is a plausible CONTRIBUTOR to the user's
"static that's clipping." But it is NOT continuous broadband static, so it may not be the sole
cause — pair with lane A (real in-browser music capture @ forced ctx=48000) and lanes C/D
(ring underrun / boot-crash) to confirm whether a louder, continuous component also exists.

**Files/scripts:** loop source = `milo-native-engine/src/audio/AudioDevice_Web.cpp:517-619`;
stream = `rb3/native/src/rb3_stream_receiver_native.cpp:243-342`. Decrypt
`scripts/native/decrypt_mogg.py <mogg> /tmp/x.ogg`; decode 15ch→stereo with an equal-gain
pan matrix via ffmpeg. Scratch: /tmp/resamp_test.cpp (buggy, verbatim), /tmp/resamp_fix.cpp
(fix), /tmp/derive{,2,3,4}.py (math).

---
### probe:web-music-capture  (lane A, 2026-06-08)

**Hypothesis (lane A / ground truth):** capture the REAL in-browser web audio on MUSIC
(not a tone) through the user's actual path — AudioContext FORCED to 48000 so the
44100→ctx resampler runs (the headless box defaults to 44100 = the bug-free FAST path) —
and measure whether it actually sounds like "static that's clipping," to adjudicate
resampler-static (lane B) vs mix-overflow-clipping vs crash-garbage.

**VERDICT: REFUTED that the deployed web build produces audible static/clipping on the
48000 resampler path. The real end-to-end capture is COHERENT music — 0% clip, 0 flat-top
runs, 0 overflow wraps, crest 17.3 dB — and is statistically CLEANER than the 44100 fast
path, not dirtier. No boot crash; sustained 15-stem song audio reached cleanly across 3
runs. The orchestrator's H-RESAMP "static" framing is NOT visible in the user's actual
output; lane B's seam-click bug is real in isolation but is buried below the dense RB3
music transient floor and does NOT dominate as broadband static.**

#### What I ran (tool: `scripts/web/web-music-force-capture.mjs`, NEW — forces ctx + captures
real preview/song through rb3CaptureAudio→rb3DownloadAudio = POST-resample device-rate output)
- Deployed build `native/web/build/release/rb3-web.{js,wasm}` (06-08 08:50), server :8421.
- Console CONFIRMS resampler active on the forced path:
  `AudioDevice: initialized (web) -- mix 44100 Hz -> ctx 48000 Hz (RESAMPLING 0.9187x)`,
  `AudioWorklet connected (ctx 48000 Hz [requested 44100, resampling])`, `ctxRate=48000`.
- SONG (game_screen, 15 active sources / 11-ch mogg, the worst case): forced 48000 AND
  control 44100. PREVIEW (song_select, 18 sources): forced 48000.

#### Raw numbers — `scripts/native/audio_coherence.py` (LOUD region)
| capture | sr | peak | crest | clip_ratio | flat_top | fs_pin | wraps | flatness | zcr | verdict |
|---|---|---|---|---|---|---|---|---|---|---|
| SONG forced **48000** (resampler ON) | 48000 | 29490 | **17.3 dB** | **0.0000** | **0** | 0.000% | 0/ch | 0.361 | 0.057 | **COHERENT** |
| SONG **44100** (fast path, control) | 44100 | 29490 | 15.8 dB | 0.0000 | 0 | 0.000% | 0/ch | 0.283 | 0.068 | **COHERENT** |
| PREVIEW forced **48000** | 48000 | 20693 | **20.3 dB** | 0.0000 | 0 | 0.000% | 0/ch | 0.443 | 0.118 | **COHERENT** |

- peak=29490 = exactly 0.9·32767 = the limiter target T=0.90; SAB max pinned at 0.900.
  But longest rail-RUN = **1 sample** in both, **0 runs ≥3** → NOT audible clipping (a
  hard-clip signature is sustained flat-top runs; there are none). Preview never even
  reaches the rail (SAB max 0.64, peak 21133).

#### A/B — does forcing 48000 (resampler ON) INTRODUCE static the 44100 path lacks? NO.
The 48000 path is CLEANER on every static-relevant axis (it's a mild low-pass):
- adjacent-sample `|diff|>4000`: **48000 = 8.85%  vs  44100 = 11.04%**
- adjacent-sample `|diff|>8000`: **48000 = 3.49%  vs  44100 = 5.90%**
- HF energy >16 kHz (where resampler imaging / broadband static would live):
  **48000 = 5.86%  vs  44100 = 11.78%** (the linear interp ATTENUATES the top octave;
  it does not add HF static)
- rail-pinning fraction: **48000 = 0.0008%  vs  44100 = 0.22%**
⇒ Linear resampling slightly SMOOTHS the signal; it does not inject broadband static.

#### Lane-B reconciliation (seam clicks). I re-ran the carry/consumed math on a ramp stream
with a JITTERED chunk schedule [200,137,512,89,333,410,256,700,128,480]: output
ideal-stream-index advanced by exactly step=0.91875 with **maxdev=0.0000 at every chunk
boundary** in MY trace — but lane B's verbatim-port host test (jitter N(800,120), real mogg
PCM) found ~8% of chunks drop one frame (`consumed==newMix-1`) at ~4.6 seam clicks/sec. Both
can be true: the drop is a 1-input-frame skip = a ~+20 dB-above-SMOOTH-floor click *in
isolation*. The decisive ground-truth fact: on REAL dense RB3 music captured end-to-end, the
2nd-difference "click" floor is **7000–9000 events/sec on BOTH paths** (distorted-guitar +
drum transients), and the 48000 path's aggregate crest/flatness/large-jump metrics are
BETTER than 44100's. So if lane-B seam clicks are present they are **buried below the music's
own transient floor and do not aggregate into the user's "static that's clipping"** — they
are not the dominant audible defect in the deployed build's output.

#### Boot-crash status (lane D cross-check): NO crash. Reached `game_screen` with 15 live
stream sources, `pumpCount=1653`, `capture ready pos=132300/132300`, downloaded a full 3 s WAV
— 3 independent runs (song 48000, song 44100, preview 48000). The memory-flagged
metamaterials Rand→MainThread ~1.25 s trap did NOT occur on this deployed build. The "static"
is therefore NOT pre-crash garbage; sustained song audio is fully reachable.

#### Song identity (best-effort; capture is only 2.76 s). `audio_verify --rank` returned
SILENT/ERROR for all 3 — a **TOOL ARTIFACT**, not an audio defect: the gate is
`r["loud_s"] < 1.0` (audio_verify.py:648); a 2.76 s capture segments to only 0.9 s "loud" so
the gate trips, yet the printed reason misleadingly cites `RMS 4088 < 60.0` (4088 ≫ 60). Real
sub-metrics from the run: **pitch_ratio 0.992× (≈1.0 → NOT chipmunk)**, clip 0%, flat-top 0,
crest 16.9 dB. Direct chroma vs the 20thcenturyboy reference = 0.937 (same song; modest lift
0.036 because the dense additive mix raises the chroma null floor and 2.76 s is short).
Native audio_verify already CONFIRMED 20thcenturyboy plays correctly; web uses the identical
MixSources upstream. ⇒ right song, right pitch, no chipmunk.

#### Attribution of "static that's clipping"
- **resampler-static (lane B):** real bug in isolation, but NOT the dominant audible defect in
  the captured output (48000 path is cleaner than 44100 in aggregate). Worth fixing for
  correctness (lane B has a verified fix), but lane-A ground truth says it is not producing
  broadband static the user would hear as "static."
- **mix-overflow-clipping:** REFUTED in the output — 0 flat-top runs, longest rail-run = 1
  sample, crest 17 dB. The limiter tames the 1.6–1.8× multi-stem sum cleanly (matches
  dc3-compare). NOT the cause.
- **crash-garbage:** REFUTED — no crash; sustained clean song audio.

**Honest residual:** the deployed build I captured sounds CLEAN by every metric. If the user
still hears "static that's clipping," the most likely remaining explanations are (a) a
DIFFERENT/older deployed artifact than the 06-08 08:50 build I tested, (b) a real-hardware
ctx rate other than 48000 (e.g. 44100-clamp variance) or a worklet ring underrun under real
browser scheduling that the headless capture's steady cadence doesn't reproduce (lane C, not
yet exercised end-to-end), or (c) the lane-B seam clicks being more audible on the user's
specific content/volume than the aggregate metrics suggest. Recommend: land lane-B's verified
carry fix (cheap, correct), and have the user re-confirm by ear on a fresh build; if static
persists, exercise lane C (ring drift under real worklet scheduling) and verify the exact
deployed wasm hash the user is loading.

**Files/scripts:** capture tool `scripts/web/web-music-force-capture.mjs` (force ctx +
real music); metrics `scripts/native/audio_coherence.py`, `audio_verify.py` (note the
`loud_s<1.0` short-capture gate bug at :648). Captures:
`/tmp/rb3_web_song_48000.wav`, `/tmp/rb3_web_song_44100.wav`, `/tmp/rb3_web_preview_48000.wav`;
coherence JSON `/tmp/coh_48000.json`, `/tmp/coh_44100.json`. Reference decode:
`scripts/native/decode_reference.py 20thcenturyboy --section gameplay`.

---
### probe:rb3-mix-vols  (2026-06-08)

**Hypothesis (user's lead, LIVE-instrumented):** the limiter MASKS an RB3-specific
mix-wiring defect — vols/pans NOT applied (raw/near-unity stems), wrong channel→source
mapping, center stems double-counted, or missing master attenuation — making the ~3x sum a
BUG, not faithful multitrack summing. Tested by instrumenting the LIVE engine (not an offline
decode estimate) to read the ACTUAL per-source mVolume/mPan at mix time + the raw pre-limiter
summed peak.

**Verdict: REFUTED — the mix wiring is CORRECT.** Vols ARE applied per-channel (not unity),
pans match songs.dta exactly, 15 sources == 15 mogg channels. The ~3.9x raw sum is FAITHFUL
15-stem additive summing, NOT a wiring bug. The limiter is a correct backstop, not masking a
defect — and native output is provably clean (0 clip). So the web "static" is NOT a mix bug;
it is the web-only resampler (probe:resampler-static), which native never runs.

**Method (real, in-engine):** two gated debug probes added to the binary (SINCE REVERTED —
both files `git diff` clean + tree rebuilds green): (1) `RB3_STREAM_VOL_DUMP=1` in
`rb3_stream_receiver_native.cpp` RenderAudio — one-shot dump of LIVE mVolume/mPan/volL/volR +
input peak the first time each channel renders non-silent; (2) `RB3_RAW_PEAK=1` in engine
`AudioDevice.cpp` MixSources — running max of the additive sum BEFORE sPreGain/limiter +
fraction of samples over full-scale. Ran `scripts/native/capture_gameplay_audio.py` (real
gameplay, null backend) on default song **20thcenturyboy** (15ch/44100; songs.dta pans
`-1 1 -1 1 -1 1 0 -1 1 -1 1 -1 1 -1 1`, vols `-4.5…-1.5 dB`).

**1. Source count + LIVE applied vol/pan (grep RB3VOLDUMP):** exactly **15 AudioSources == 15
mogg channels** (`srcs=15` every RAWPEAK line). Applied vs songs.dta authored `DbToRatio(vol)`:
| ch | track | authored | LIVE applied | applied/authored | mPan live==dta |
|---|---|---|---|---|---|
| 0 | drumL | 0.5957 | **0.5012** | 0.841 (−1.5 dB) | −1.0==−1.0 |
| 2 | drum2 | 0.5821 | 0.4898 | 0.841 | −1.0==−1.0 |
| 6 | bass  | 0.8414 | **0.7080** | 0.841 | 0.0==0.0 (center) |
| 7 | gtrL  | 0.5957 | 0.5957 | 1.000* | −1.0==−1.0 |
| 9 | voxL  | 0.6095 | 0.5129 | 0.841 | −1.0==−1.0 |
| 11| keyL  | 0.5957 | 0.5012 | 0.841 | −1.0==−1.0 |
(*ch7,8,13,14 dumped at peakIn≈0.001 = silent intro, BEFORE a uniform **−1.5 dB parent fader**
propagated; the 11 channels with real signal all show −1.5 dB.) ⇒ **Vols ARE applied** —
applied tracks authored (NOT unity-clamped / NOT bypassed), plus a uniform extra **−1.5 dB**
runtime band fader (`DbToRatio(-1.5)=0.8414` exactly). **Pans match songs.dta channel-for-
channel.** No wrong mapping, no dropped vols; the −1.5 dB is EXTRA attenuation (helps).

**2. Center double-count:** ch6 (bass, pan=0) → 0.708 into BOTH L and R via the pan law
(`left=vol*(pan<=0?1:1-pan); right=vol*(pan>=0?1:1+pan)`). This is the **authored** mono-center
behavior (songs.dta pan=0.0), one stem only — NOT a doubling bug.

**3. Raw pre-limiter summed peak (grep RB3RAWPEAK):**
- intro/early (22s cap): **rawMax = 4.369×** FS, 3.7% samples >1.0.
- loud chorus (45s cap, 24.9s loud, post-limiter peak 29490 = 90% FS): **rawMax = 3.904×** FS,
  ~2.1% samples >1.0.
- This LIVE 3.9–4.4× is HIGHER than probe:dc3-compare's offline estimate (1.64×): that estimate
  is a steady-state downmix peak; the live running-max catches correlated transients where
  stems align (worst-case correlated sum of applied gains = **4.405×**, which 4.369 nearly
  hits; quadrature/uncorrelated = 1.57× ≈ the offline estimate = the sustained level). Both
  consistent; the live 3.9× is the true peak the limiter tames.

**4. Native output CLEAN (clincher — not a masked bug):** `audio_coherence.py` on the loud
chorus: peak 90% FS, **clip_ratio 0.0000, flat_top_run 0, fs_pin 0.000%, crest 17.3 dB →
COHERENT, music not degraded.** The limiter pulls 3.9× → clean 90% FS with zero clipping. So it
is a legitimate backstop for faithful summing, NOT a band-aid over a mix bug.

**Root cause (this probe):** NO mix-wiring defect. RB3 faithfully applies songs.dta vols/pans
and sums 15 stems additively (6 drum sub-channels + L/R instrument pairs), inherently
overflowing because RB3 ships multitrack stems authored for the Wii AX hardware sum-then-
saturate, replaced by the limiter. **The web "static that's clipping" cannot originate here** —
the web build shares this exact (correct, clean) mix via the same RB3StreamReceiverNative +
engine MixSources at 44100; it diverges from native ONLY in the post-mix 44100→ctx resampler.
⇒ converges with probe:resampler-static: the static is the resampler seam-drop, not the sum.

**Proposed fix (overflow-at-source, OPTIONAL — does NOT fix the static):** to make the limiter
a near-no-op backstop, drop the sum below ~1.0 at source with an RB3-only `sPreGain ≈ 0.25`
(≈−12 dB; 3.9×·0.25≈0.98), already `DC3_AUDIO_GAIN`-overridable + DC3-safe (separate native
AudioDevice). Port-level master trim, not a faithfulness change. But the ACTUAL user-facing
static fix lives in the web resampler carry (probe:resampler-static §"Proposed fix"); this mix
probe confirms the mix/limiter is SOUND and must NOT be touched to chase the static.

**Files/commands:** instrumentation (reverted) in `native/src/rb3_stream_receiver_native.cpp`
RenderAudio + `milo-native-engine/src/audio/AudioDevice.cpp` MixSources; harness
`scripts/native/capture_gameplay_audio.py OUT.wav --secs N`; metrics
`scripts/native/audio_coherence.py`; authored mix `orig-assets/extracted/songs/songs.dta`
(20thcenturyboy); `DbToRatio` = `src/system/math/Decibels.cpp`; fader path
`src/system/synth/StandardStream.cpp:707-725` (UpdateVolumes: DbToRatio→ClampEq[0,1]→SetVolume)
+ `:212` (SetPan). Captures: /tmp/rb3_gp_audit.wav (intro), /tmp/rb3_gp_loud.wav (chorus).

---
### SYNTHESIS + FIX LANDED (orchestrator, 2026-06-08)

**Diagnosis.** The user's DC3-comparison instinct was the right lever — it EXONERATED the
mix (the limiter is a correct backstop over a content-inherent 3.9x multi-stem sum, not a
band-aid over a wiring bug: `dc3-compare` + `rb3-mix-vols` both REFUTED) and redirected the
search to the WEB-only post-mix stage. There the wave-05 **resampler itself was buggy**
(`resampler-static` CONFIRMED, survived 3/3 adversarial refute): its single-sample carry
drops one input frame on ~8.1% of *jittered* PumpAudio chunks (constant cadence → 0 skips,
which is exactly why wave-05's steady sine tone + the lane-A headless capture both saw it
clean — neither jitters; a real browser does, under render load → audible crackle/"static").

**Fix (LANDED, engine, uncommitted pending user ear-confirm).**
`milo-native-engine/src/audio/AudioDevice_Web.cpp` PumpAudio + `AudioDevice.h`:
- Replaced the single-sample carry (`mResampleLastL/R`, `mResampleHavePrev`) with a small
  **carry-all** buffer (`mResampleCarry[kResampleCarryMax*2]`, `mResampleCarryN`). Each chunk
  now carries EVERY pulled-but-unconsumed frame (1-2 in practice), stream-contiguous, into
  the next chunk; MixSources fills only the genuinely-additional fresh frames.
- Shared engine ⇒ **DC3-web gets the fix too**.
- Verified (host unit test `/tmp/resamp_verify.cpp`, jittered N(800,120) chunks, abs-frame
  bookkeeping): seam-anomalies **139(old) → 0(new)**, maxDelta **1.92 → 0.9188 (= step)** —
  read position now advances by exactly `step` every output sample, no skips, no OOB.
- Web rebuilt + redeployed 09:31/09:32 (release+debug) — also **defeats any stale
  immutable-cache** the user may have been loading.
- Regression smoke (fresh build, headless `web-song-preview-audio.mjs --phase song`): boots,
  15 sources, peak 29490 (limiter rail), RMS steady ~4944, nonZero 83.9% → **clean, no
  regression**. (Headless is steady-cadence so it can't exercise the jitter path either way;
  the unit test is the decisive proof.)

**Remaining / next wave.** Lane C (worklet **ring underrun** under real-hardware scheduling)
was NOT exercised end-to-end — it could add a louder, more continuous "static" component the
seam-crackle alone doesn't explain. If the user still hears static after hard-reloading the
fresh build, exercise lane C (add a worklet underrun counter; measure SAB fill under real
60fps render load). Also viable port-level polish (NOT the bug): an RB3-only `sPreGain` to
make the limiter a near-no-op backstop — optional, changes loudness, deferred.
