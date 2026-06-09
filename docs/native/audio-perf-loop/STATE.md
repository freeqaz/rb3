# audio-perf-loop — STATE (canonical living brief)

## WAVE 09 (2026-06-09) — web static PERSISTS after wave-08 carry-all fix; RE-DIAGNOSED.
### User: web song audio = "static mixed with chipmunks, wayyy too high frequency" + SFX latency ~few-hundred-ms.
### DECISIVE NEW MEASUREMENTS (orchestrator inline; user-provided REAL browser capture `fucked-audio1.m4a` = "Beast and the Harlot", 11.3s):
### - UNDERRUNS = 0 over 30s (user console log; frame p50~20ms, one 85ms blip) → NOT a buffering/ring-starvation problem.
###   The dedicated-audio-thread redesign is SHELVED (it would not touch this symptom).
### - audio_verify(capture vs beastandtheharlot mogg): RIGHT song (chroma 0.69, fp BER 0.34), RIGHT pitch/speed
###   (speed 1.003x — NOT a chipmunk), NOT clipped (clip 0.00%, crest 19dB) — BUT **HF/THD 6.08x** (thr<2.0) +
###   **flatness 0.62** = BROADBAND HIGH-FREQUENCY STATIC layered on otherwise-correct music. The "chipmunk"
###   PERCEPTION is the HF harshness, not an actual pitch/rate shift.
### - Noise characterization of the capture: **0 clicks / 0 discontinuities** (wave-08 seam-CLICK theory is DEAD),
###   steady-ish HF hiss (HF>8kHz frac mean .035, cv .75), **strong spectral peak at exactly 11025 Hz = 44100/4**.
### - carry-all resampler fix IS deployed (engine pin bd695fb=HEAD; release build 21:09 == AudioDevice_Web.cpp 21:09)
###   → it is running and is NOT the bug. Wave-08 over-claimed "fixed."
### - Resampler (AudioDevice_Web.cpp:560-666) is clean continuous LINEAR interp 44100→ctx (step=44100/ctx; ctx=48000
###   per earlier rb3_audio_stats ⇒ UPSAMPLING). Linear upsample is a mild LOW-pass — it should NOT manufacture 6x HF.
###   ⇒ static is either UPSTREAM of the resampler (mix/limiter/decode; "native clean" was only ever shown on
###   20thcenturyboy, a DIFFERENT song), OR the linear-interp imaging is worse than expected, OR ctx forces a
###   downsample path without anti-aliasing. THIS is what wave-09 fans out to localize.
### WAVE-09 RESULT (converged, adversarially verified + orchestrator cross-check):
### - RESAMPLER EXONERATED (probe B host-test): the exact linear-interp+carry, jittered, = 0.919x HF vs scipy resample_poly
###   (BELOW a good resampler) on real music. Cannot manufacture 6x HF. Also: artifact line is at 11025=44100/4, NOT
###   12000=48000/4 ⇒ born in the 44100 domain, not the 48000 resample. (Don't replace the resampler.)
### - LIMITER EXONERATED (orchestrator native A/B, RB3_LIM_BYPASS/RB3_LIM_ATTACK_MS knobs added to AudioDevice.cpp):
###   bypassing the limiter ENTIRELY leaves fs/4 HF unchanged (band-E: instant 0.26% / BYPASS 0.43% / 3ms 0.39%; peak
###   16-19x floor in all three). The artifact is in the RAW ADDITIVE STEM SUM, before the limiter.
### - SHARED ENGINE, NOT WEB-PATH-ONLY (probe A + orchestrator cross-check): the elevated upper-mid HF is present in EVERY
###   engine capture, native AND web (native gameplay peak 16-57x floor / band-E 0.26-1.2%), but ABSENT in the clean
###   ffmpeg decode of the same mogg (2.4x / 0.10%). So it is engine-injected at the 44100 mix rate, in the per-stem
###   DECODE/RENDER (VorbisReader / StandardStream ConsumeData->WriteData / RB3StreamReceiverNative). RenderAudio itself
###   is a clean 1:1 int16->float copy (inspected) — suspect is the DECODE filling mBuffer.
### - CONFOUNDS (be honest): native HF measured vs a FILTERED reference downmix; the user's m4a went through tab-capture
###   ->48000->44100->AAC (lossy, may add HF); peak freq TRACKS the song (beast 11047 / default 11326) = more like content
###   HF than a fixed injected tone. ⇒ Unresolved: is native a real bug or faithful-music-HF-vs-filtered-ref, and is the
###   web 0.4%->1.2% step real engine or capture-chain? RESOLVING via USER EAR TEST (native render A/B) before committing
###   a wave direction.
### - SFX LATENCY (probe D, confirmed 0 refutes): greedy SAB ring fill — PumpAudio queries free once then while(free>0)
###   fills ALL of it ⇒ ring sits at ~RING_FRAMES-1=32767 frames = ~673ms @48000 output-latency floor; delays EVERY sound
###   incl. SFX. No prefill/target cap anywhere. FIX READY (web-only, underruns=0 so safe): cap fill to a target depth
###   ~ctx/10=4800 frames=100ms ⇒ keydown->audio ~685ms -> ~63ms (~10.8x). Bundle into the next web build.
### NEXT: (1) USER ear-test native render to localize shared-decode vs web-output. (2) Then either native decode-bisect
###   (single-stem vs ffmpeg sample-diff) OR web-output wave (force ctx=48000 headless to reproduce 6x; test ctx=44100
###   true-context to let the browser's good SRC do output instead of our path). (3) Land SFX ring-cap in that web build.

## NEW GOAL (2026-06-06, user): (1) song audio 100% correct for GAMEPLAY + PREVIEW;
## (2) character animations render correctly in the venue (crowd + on-stage).
## Role = COORDINATOR; subagents implement; hand off via docs.
### Audio status now: limiter fixed the gain-clipping, BUT user reports preview is
###   "music but TOO FAST (chipmunks) + clipping a lot." => a SEPARATE sample-RATE bug
###   (H3, reopened). mogg = 44100 Hz / 15 ch (ffprobe of decrypted ogg). Native likely
###   OK (miniaudio config.sampleRate=44100 → internal resample to device). Suspected
###   WEB-specific: AudioContext requested 44100 may be CLAMPED to 48000 by the browser
###   with NO resampler in the SAB→worklet path → 48000/44100 = 1.088x fast + ring drift
###   (the "clipping" may be drift glitches, OR the deployed web build still has the
###   3ms-attack limiter not the user's instant-attack — undeployed). MEASURE the exact
###   ratio (read back ctx.sampleRate + capture real browser output) before fixing.
### Char status: deep prior investigation in docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md
###   — SHIPPED fling-clamp mitigation; FAITHFUL fix BLOCKED at name-resolution: all band
###   outfit meshes bind to ONE shared char/main/skeleton.milo at the MALE bind (female
###   trackjacket → skinPos (19.8,3.8,0.4) FLUNG). ~15 char files + BoneSetup.cpp +
###   Rnd_Wgpu_RB3.cpp are DORMANT uncommitted WIP (no live agent) — build on them.
### Wave 05 DONE: AUDIO FIXED+VERIFIED+COMMITTED (b458b18: web resampler 44100→ctx rate;
###   440Hz tone @ forced ctx=48000 → 440.00Hz; chipmunk gone; 0 flat-top runs). Native
###   correct by construction. AUDIO GOAL MET (pending user ear-confirm on fresh web build).
### Wave 08 DONE (2026-06-08, user re-report "WEB sounds like static that's clipping"):
###   ROOT-CAUSED to the wave-05 resampler ITSELF (that "fix" was buggy). 4-lane ultracode
###   wave + adversarial verify: (1) MIX/limiter EXONERATED — dc3-compare + rb3-mix-vols both
###   REFUTED: RB3 faithfully applies songs.dta vols/pans; the 3.9x multi-stem sum is content-
###   inherent (15 stems vs DC3's 2ch pre-mix) and the instant-attack limiter ALREADY tames it
###   to 0% clip / 0 flat-top. NOT masking a bug. (2) resampler-static CONFIRMED (survived 3/3
###   refute, 0 refuted): the web 44100→ctx LINEAR resampler carried only ONE sample between
###   PumpAudio chunks while MixSources DESTRUCTIVELY pulls `newMix` frames → on ~8.1% of
###   *jittered* chunks it silently DROPS one input frame = a 1-sample click. ~4.7 ticks/s @
###   60fps. Constant cadence → 0 skips (why wave-05's steady sine + headless capture MISSED it;
###   real browsers jitter under render load → audible crackle). (3) lane-A headless music
###   capture of the deployed build came back CLEAN (0 clip, no chipmunk 0.992x, no boot crash,
###   right song chroma 0.94) — but that's the steady-cadence artifact (didn't jitter → ran the
###   bug-free path), consistent with the bug being jitter-gated. FIX LANDED (engine, UNCOMMITTED
###   pending user ear-confirm): AudioDevice_Web.cpp PumpAudio now carries ALL unconsumed frames
###   (mResampleCarry[]/mResampleCarryN replace the single mResampleLast*). Host unit test:
###   jittered chunks seam-anomalies 139(old)→0(new), maxDelta 1.92→0.9188(=step). SHARED engine
###   → DC3-web benefits too. Web rebuilt+redeployed (also defeats stale immutable-cache).
###   OPEN: lane C (ring underrun on real-hardware worklet scheduling) NOT exercised end-to-end —
###   if static persists after the user hard-reloads the fresh build, that's the next wave.
### CHAR: both quick fixes (SyncObjects rebind / offset rebake) REFUTED 2/2 on EMPIRICAL
###   grounds — there is NO live female-posed per-member skeleton to bind to. Runtime fact
###   (doc probes): per-member skeletons load (8-9 distinct) but are UNUSED; the char
###   pipeline poses ONE SHARED, MALE-BIND skeleton (CharBoneDir::FindResource → shared
###   sResources). So a rebind gives no-op (own==shared) OR a dormant static male-bind
###   skeleton (still flung + un-animated). REAL FIX (doc + refutation agree): each band
###   member needs its OWN skeleton, posed by its OWN pipeline to ITS gender bind (the
###   female's female-bind) — i.e. surgical band-only loader UN-SHARE + per-member deform
###   pose. Refutation's bar: a BUILD+PROBE proving own!=shared AND own is female-posed-LIVE
###   (skinPos 19.8→0 WITH clip+IK, no re-fling, animated not frozen). Wave 06 = design the
###   surgical un-share → implement+probe (owns build) → adversarially verify the MEASURED
###   result. Shipped fling-clamp stays as backstop. Diagnostics live: XBONE, SHARD_CATCH,
###   BONE_PROBE, RB3_NO_{CLIP,IK,DEFORM,POSEMESHES,FACE}, char-burst-capture.py.

---


> Orchestrator owns this file; rewritten at the end of every converge phase.
> Per-wave detail in `wave-NN-*.md` (wave-01 = `docs/native/audio-perf-investigation/phase1-*.md`).

## Goal
1. **Audio correctness** — game "sounds like clipped noise." Measure reference-vs-output
   divergence, fix, re-measure. Native + web.
2. **Asset-load stutter** — attribute each long frame, drive the tail down. Native + web.

## Current phase/wave
- **Wave 01 (diagnose) DONE. Wave 02 (fix audio) DONE — LANDED + VALIDATED NATIVE + WEB.**
  ✅ AUDIO COMPLETE: in-browser gameplay 15 sources / SAB nonZero 65460/65536 / **0
  flat-top runs** (crest 13.0 dB); preview 18 sources / peak 9206 (below rail) / **0
  flat-top runs**. "Clipped noise" eliminated. Web rebuilt+deployed 02:58.
- **Wave 03 (perf) DONE — MEASURED + ATTRIBUTED; both quick-fixes VALIDATED-REJECTED.**
  Both perf patches were applied, A/B-measured, found non-viable, and REVERTED (clean tree):
  - **P2 splash venue-cull → REJECTED (breaks visuals).** Screenshot A/B with
    RB3_VENUE_FRUSTUM_CULL=1 proved the native world.cam frustum is WRONG: re-enabling
    cull dropped VISIBLE meshes (the "ROCK BAND 3" logo + cityscape chunks gone — see
    /tmp/venue_splash_{off,on}.png). Same class as the SMASHER_DRAW_FIX. ALSO the 68 ms
    splash cost is largely a HEADLESS artifact (per-frame Submit+WaitAny, GpuDevice.cpp:
    403-413) absent under web/windowed vsync — likely not a real user stutter. REAL FIX =
    correct the native world.cam frustum to match the WebGPU projection (render workstream).
  - **P1 panel-stagger → REJECTED (no-op).** A/B: ENTER spike 46.4 ms (off) vs 44.4 ms
    (on) = within ±3 ms noise. The 46 ms is NOT spread across the 5 panels — it's
    concentrated in the song_select_panel's OWN milo + ~17 nested Includes (one CheckLoad).
    Panel-level staggering can't split one panel's milo tree. REAL FIX = milo-Include-level
    / DirLoader staggering or async parse (loader workstream) — deeper than a UIScreen edit.
- **Wave 03b — WEB stutter verification (real GPU, `scripts/web/web-stutter-probe.mjs`):
  BOTH stutters are REAL on web (refutes the "splash = headless artifact" caveat).**
  rAF inter-frame gaps, ANGLE-Vulkan (real GPU, not SwiftShader):
  - **splash: p50 100 ms (~10 fps!), max 117 ms — SUSTAINED** while the user sits on
    "PRESS START." The venue over-draw is a genuine, visible web stutter, NOT just the
    headless Submit+WaitAny artifact. THE BIGGER WIN.
  - main_hub→song_select transition: a **~150 ms one-shot hitch** (the song_select.milo
    parse; ~3× the native 46 ms because wasm parses milo slower). Real hitch.
  - steady-state (song_select, scroll): smooth 60 fps (16.7 ms). Gameplay not re-probed
    but expected smooth (native steady-state was).
- **USER DIRECTION: #1 commit (DONE) → #2 verify-web (DONE) → #3 deeper perf (NEXT).**
  Re-prioritized by the web data: splash venue over-draw first (sustained), then the
  ENTER milo-parse hitch.

## Wave 04 (deeper perf — #3) DONE — root-caused; implementation BLOCKED/PARTIAL, reverted
- **Splash frustum: ROOT CAUSE FOUND (verify 0/3 refuted).** The native world.cam frustum
  is NOT wrong — it matches the WebGPU projection at 16:9. The real defect is the **baked
  Xbox bounding SPHERES**: native venue meshes ship GPU-vertex-compressed with EMPTY CPU
  Verts, so `MakeWorldSphere` can't recompute the bound and the baked Xbox `mSphere` is
  wrong-centered/undersized → a correct frustum culls VISIBLE meshes. FIX (A1, high-conf):
  recompute a tight local sphere from the unpacked verts + `SetSphere`, then re-enable the
  world.cam-scoped cull (Draw.cpp seam). **BLOCKED on implementation:** the recompute must
  go in RB3's vertex-upload path = `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
  (`MeshGpuCache.cpp`/`Mesh_Wgpu.cpp` are DC3-only, EXCLUDED for RB3 by the Gpu/Wgpu CMake
  filter — A1 named the wrong file). Rnd_Wgpu_RB3.cpp is **concurrent-modified by the
  char-skinning agent** (BandRnd::DrawMesh 2812-3293) → not safe to edit now. AND per A3
  the win is only **~30%** (visible skyline is genuinely far, 8539u; little is off-screen).
  Tried Option A in MeshGpuCache.cpp + the Draw.cpp seam → MeshGpuCache.cpp not compiled for
  RB3 → no effect → REVERTED clean. READY SPEC, defer until the concurrent work settles.
  (Throttling draw-rate is NOT a fix: it inserts idle frames; the heavy frame still renders
  the venue slowly, so perceived smoothness is unchanged.)
- **ENTER hitch (A2, feasible, deferred):** in-place Include staggering is NOT feasible
  (inline dirs share one BinStream; ObjectDir::PostLoad is non-re-entrant). The fix is
  **PREWARM song_select.milo during main_hub's idle dwell** (RB3_PREWARM_SCREENS) +
  optional GPU prewarm — targets the web ~150 ms → sub-30 ms. More involved (prewarm
  mechanism + next-screen mapping); ready spec, not yet implemented.

## Status: audio DONE+committed+validated(native+web)+user-improved(instant-attack, uncommitted).
## Perf: fully measured + web-verified + root-caused; both fixes have exact specs but
## implementation is blocked (concurrent file) / partial (~30% splash) / involved (prewarm).

## Confirmed facts
- **AUDIO ROOT CAUSE (H1, confirmed):** 11–15 song stems each a separate additive
  AudioSource; even at authored ~−4 dB vols the sum peaks ~3.2× full scale (channel
  COUNT, not per-stem level); engine applied 1.1× master gain then hard-clamped → 1124
  flat-top runs/gameplay-region = "clipped noise." vols/pans ARE applied correctly (not a
  bypass/format/rate bug). `AudioDevice.cpp:364-381`, web `AudioDevice_Web.cpp:399-413`.
- **FAITHFULNESS (W2-B):** real RB3 master fader is UNITY (synth.dta has no master_vol);
  the Wii AX/DSP **hardware mixer** handled summation+saturation, RB3 left AX's soft
  compressor off. **DC3 already applies −2 dB master_vol** (dc3 synth.dta) → a flat shared
  `sMasterGain` would double-attenuate DC3. ⇒ a **content-adaptive limiter is DC3-safe by
  construction**; the 1.1× boost is a port artifact, correct to delete for both games.
- **THE FIX (LANDED, native-validated):** replaced the 1.1× master gain + hard clamp with
  a **one-pole stereo-linked peak limiter (T=0.90, atk 3 ms, rel 80 ms) + soft-knee
  saturator (knee 0.95) backstop**, on BOTH `AudioDevice.cpp` (native) and
  `AudioDevice_Web.cpp` (web). `sMasterGain`→`sPreGain` (default 1.0, still DC3_AUDIO_GAIN-
  overridable). New member `mLimiterEnv` (AudioDevice.h), reset in Resume(). Content-
  adaptive (DC3-safe), HX_NATIVE/HX_WEB-only (no Wii decomp impact).
- **PERF (W2-C, attributed):** the budgeted loader is NOT the stutter (lpu=0). (1)
  song_select-ENTER 48 ms = `UIScreen::LoadPanels` (`src/system/ui/UIScreen.cpp:288`)
  parsing all 5 panels' .milo Includes in ONE frame → fix = stagger panels one-per-frame
  (HX_NATIVE, env RB3_STAGGER_PANELS). (2) boot-splash 185/96/84 ms = the **full 3D venue
  (823 meshes/250k tris/943 tex) drawn behind the 2D splash via world.cam** — a render
  workstream (cull world.cam during splash); NOT a loader issue. one-time 184 ms frame-24
  spike = venue mesh VBO/IBO upload.

## Ruled out
- H2 (format/scale), H4 (downmix/stride/pan-law), per-channel vol bypass, "silence/noise."

## Open hypotheses (ranked)
1. **Web parity** — does the limiter behave identically in-browser (single-threaded
   PumpAudio chunks vs miniaudio callback)? Confirm SAB nonZero + clip metrics post-build.
2. **H3 rate/pitch (secondary)** — only if a warble remains post-fix; NEEDS-MEASUREMENT.
3. **Limiter tuning** — crest lands 13.8 dB (ref 17.5); ~3.7 dB of compression. Acceptable
   for a rhythm game; revisit only if it sounds over-compressed in listening.

## Measurement baselines (gameplay LOUD region, mono)
| build | RMS | peak | hardClip(±32767) | flat-top runs≥3 | crest dB |
|---|---|---|---|---|---|
| BROKEN 1.1× (before) | 0.278 | 1.000 | 2.597% | **1124** | 11.1 |
| limiter only | 0.210 | 1.000 | 1.199% | 680 | 13.6 |
| **limiter+soft-knee (LANDED)** | 0.203 | ~1.0 | **0.036%** | **0** | 13.8 |
| gain030 (trivial fix ref pt) | 0.068 | 0.808 | 0.000% | 0 | 21.5 |
- Decisive: audible clipping signature (sustained flat-top runs) 1124 → **0**; residual
  0.036% are isolated single/double samples (longest run 2), inaudible. +9.5 dB louder than
  the trivial gain fix. Captures: /tmp/rb3_native_gameplay{,_limiter,_limiter2,_gain030}.wav.
- audio_correlate.py waveform/spectrogram path is unreliable for cross-run captures (40s
  from process start includes menu+count-in + nondeterministic offset → low corr). The
  alignment-free clip-ratio / flat-top-run / crest metrics are the decisive ones.
- **Perf:** p50 4.33 / p95 11.73 / p99 77.35 / max 184 ms; 94.6% of long-frame time =
  draw/GPU/poll, loader 249 ms, sync drains 0. splash p50 67 ms (venue-behind-splash).

## Next wave plan (Wave 03)
- **Web audio confirm:** after build, run `scripts/web/web-song-preview-audio.mjs`
  (--phase song + preview), assert SAB nonZero + capture clip metrics (flat-top runs ~0).
- **Perf panel-stagger:** implement the HX_NATIVE RB3_STAGGER_PANELS fix at
  UIScreen.cpp:288 (CheckLoad focus panel on activate, queue rest one-per-frame),
  rebuild native, re-run frame_profiler.py → kill the 48 ms ENTER spike (target <16 ms),
  confirm lpu stays 0. (Boot-splash venue-cull = separate render workstream, flag only.)

## Tooling index
- **`scripts/native/audio_verify.py` (2026-06-08, NEW — reference-vs-output verifier;
  `/audio-verify` skill).** Proves the captured game audio is the SAME song + played
  correctly: mix-robust **chroma** xcorr + **Chromaprint fpcalc** BER (identity),
  onset **resample-search** speed ratio (chipmunk/rate), reference-free clip/flat-top/
  wrap (distortion), flatness+zcr (noise), silence. Verdicts MATCH/DEGRADED/WRONG-SIGNAL/
  SILENT. `--rank s1,s2,s3` = robust relative identity (right song wins). `--selftest`
  = 6/6 synthetic proof. RESULT (native, default song): GAMEPLAY+PREVIEW = 20thcenturyboy
  CONFIDENT (chroma 0.54/0.55, fp 0.33/0.31, margin +0.13/+0.22), **not clipped, not
  chipmunk** (flat speed curve). Doc: AUDIO_VERIFY_2026-06-08.md. Only 3 songs have moggs
  (20thcenturyboy/25or6to4/antibodies). Moderate chroma (~0.55) = our no-clamp downmix ≠
  game mix, NOT a bug.
- Fix files: `../milo-native-engine/src/audio/AudioDevice.{h,cpp}` + `AudioDevice_Web.cpp`.
- `scripts/native/audio_correlate.py` (clip/crest/spectral; trust the clip-run metrics),
  `decode_reference.py`+`decrypt_mogg.py`, `capture_gameplay_audio.py OUT --secs N [--gain G]`,
  `frame_profiler.py`, `_capture_preview.sh`, web `web-song-preview-audio.mjs`/`loadperf-*`.
- Capture: `MILO_AUDIO=1 MILO_AUDIO_BACKEND=null DC3_DUMP_AUDIO=<wav> DC3_DUMP_SECONDS=N`
  (+`RB3_HTTP=1 RB3_HTTP_PORT=P`). Build: `cmake --build native/build-native -j$(nproc)`.
- **Build contention:** ONE native build dir — orchestrator builds; never edit source
  during the web build. Fan-out agents may RUN the binary, not BUILD.

---
## WAVE 06 RESULT + WAVE 07 PLAN (2026-06-06, char)
- (1) SHARDS/FLING **FIXED+MEASURED+2/3-verified** (uncommitted, entangled w/ dormant char WIP):
  un-share is inert/crash-prone (prior sessions) so the implementer did a CONSTANT OFFSET
  REBAKE — Rnd_Wgpu_RB3.cpp BandRnd::DrawMesh SKEL_REBAKE pre-pass (default-on, opt-out
  RB3_NO_SKEL_REBAKE), band-only (skeleton_unshared.milo dir scope), static arm/twist bones
  only, dynamic hair/face stay on the clamp; + Mesh.h mNativeBonesRebound member (HX_NATIVE,
  byte-identical, match% 62.01/77.31 unchanged). MEASURED: trackjacket bone_R-upperArm skinPos
  (19.80,3.84,0.36)→(0,0,0); 3 males stay 0; clamp 0× on her; crowd/males/venue unchanged.
  => On-stage chars now RENDER coherently (no shards).
- (2) BAND STATIC — the deeper "animations" gap. NEW FACT: bone_R-upperArm worldPos BYTE-
  IDENTICAL across 1424 draws / whole song WITH clip+IK, MALES TOO (males look right only
  because invBind==static male bind). NOT a default-on gate. Drive path exists
  (CharClipDriver/PlayMainClip/FirstPlayingClip) but never moves the skeleton. Crowd DOES
  animate (own skeletons).
- WAVE 07 = DIAGNOSE why the on-stage band skeleton is static + SCOPE (tractable drive/clip-
  assignment gap vs large unported BandDirector choreography) + fix path. Read-heavy + probe.
