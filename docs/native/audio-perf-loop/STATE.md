# audio-perf-loop — STATE (canonical living brief)

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
- Fix files: `../milo-native-engine/src/audio/AudioDevice.{h,cpp}` + `AudioDevice_Web.cpp`.
- `scripts/native/audio_correlate.py` (clip/crest/spectral; trust the clip-run metrics),
  `decode_reference.py`+`decrypt_mogg.py`, `capture_gameplay_audio.py OUT --secs N [--gain G]`,
  `frame_profiler.py`, `_capture_preview.sh`, web `web-song-preview-audio.mjs`/`loadperf-*`.
- Capture: `MILO_AUDIO=1 MILO_AUDIO_BACKEND=null DC3_DUMP_AUDIO=<wav> DC3_DUMP_SECONDS=N`
  (+`RB3_HTTP=1 RB3_HTTP_PORT=P`). Build: `cmake --build native/build-native -j$(nproc)`.
- **Build contention:** ONE native build dir — orchestrator builds; never edit source
  during the web build. Fan-out agents may RUN the binary, not BUILD.
