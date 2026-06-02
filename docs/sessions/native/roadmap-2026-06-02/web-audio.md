# Web Audio Bring-Up — Hardening the W3c AudioWorklet Path

> ## IMPLEMENTATION STATUS — 2026-06-02
> Acting on "port the web audio from DC3 / it should be in the shared engine": the
> core web/song-audio path was **already shared in the engine** (`AudioDevice_Web.cpp`,
> namespaced `MILO_WEB_AUDIO_NS=rb3`, AudioWorklet+SAB, `PumpAudio` from `RunOneFrame`,
> COOP/COEP set) — confirmed, no port needed. Landed this session:
> - **Phase 0 DONE** — removed the duplicate `TheSynth` in `main_web.cpp` (was UB
>   masked by `--allow-multiple-definition`; `Synth.cpp:58` is the sole def) + fixed
>   the stale "web is audio-free" comment in `App.cpp` (comment-only, `#ifdef HX_WEB`).
> - **Phase 2 PARTIAL (PCM) DONE** — the genuine "port from DC3" piece. The engine's
>   DC3 `SampleInst_Native.cpp` is platform-excluded for rb3 (API shape), so added
>   RB3-shaped **`native/src/rb3_sampleinst_native.cpp`** (`SynthSample::NewInst` →
>   `RB3SampleInstNative : SampleInst, AudioSource`), wired into the native target +
>   `RB3_WEB_NATIVE_GLUE`, and **enabled SFX load** (`mEnableSFX=true` in
>   `rb3_platform_native.cpp`, env-gate `RB3_NO_SFX`). **Empirical finding:** the 360
>   `.milo_xbox` SFX banks are a MIX — **big-endian PCM (now plays, byteswapped)** and
>   **XMA (format 3, skipped)**. So menu/UI/MIDI PCM one-shots are now audible; XMA
>   one-shots remain silent. DC3's model is also PCM-only → this is exact DC3 parity.
> - **OPEN follow-ups:** (1) **XMA decode** for the remaining SFX via the engine's
>   `FFmpegAudioReader` (FFmpeg=1 is on) — the real way to make *all* SFX audible;
>   (2) **Phase 1 audibility proof** — still needs a real browser tab / captured WAV
>   (this headless box has no audio *output*, N10, so it can't self-verify audibility,
>   only that the source registers + renders).

**One-line verdict:** The web audio backend is *architecturally complete and
wired* (synth → StreamReceiver/AudioSource → main-thread `PumpAudio` → JS
SharedArrayBuffer ring → `milo-audio-processor` AudioWorklet → speakers), and W3c
reports "20th Century Boy" decoding end-to-end in the browser — but it has
**never been confirmed audible in a real browser tab**, carries one latent
duplicate-symbol footgun (`TheSynth` defined twice), and is **structurally silent
for all menu/UI one-shot SFX** (the `SampleInst` sample path is stubbed to null on
both native and web).

---

## 1. Current state — what works vs what's stubbed/missing

### What is wired and (per W3c) functional

The push-model web audio pipeline is fully built and reachable from the App boot
spine. Threading maps cleanly to single-threaded WASM:

- **Output device (engine, layer b):**
  `milo-native-engine/src/audio/AudioDevice_Web.cpp` is the web `AudioDevice`.
  `Init()` (line 286) allocates a JS-side `SharedArrayBuffer` ring
  (`RING_FRAMES=32768`, ~743 ms @ 44.1 kHz; line 40), spins up an `AudioContext`
  + `AudioWorkletNode` named `milo-audio-processor`, and wires a
  keydown/click/touchstart `AudioContext.resume()` gesture for the browser
  autoplay policy (`js_audio_init`, lines 67-131). **There is no headless skip on
  web** — unlike the native `AudioDevice.cpp:148` (`MILO_HEADLESS` → skip), the
  web `Init` always opens the worklet. Backend = AudioWorklet (not
  ScriptProcessorNode).
- **The worklet** (`milo-native-engine/src/platform/web/assets/audio-worklet.js`,
  deployed to `native/web/build/audio-worklet.js`): a 128-frame-quantum
  `AudioWorkletProcessor` that drains the SAB ring via `Atomics.load/store`
  cursors and pads silence on underrun. Processor name matches
  `AudioDevice_Web.cpp:300` (`milo-audio-processor`). It is copied next to the
  wasm by `milo_engine_apply_web_target_options` POST_BUILD
  (`milo-native-engine/CMakeLists.txt:603-607`).
- **Threading model = single thread + standalone JS SAB.** Native uses a separate
  miniaudio audio thread (`AudioDevice.cpp` `MaDataCallback` → `MixSources`) with
  `std::atomic` producer/consumer. On web, **both** producer and consumer run on
  the WASM main thread: `App::RunOneFrame` calls
  `AudioDevice::GetInstance().PumpAudio()` every frame under `#ifdef
  __EMSCRIPTEN__` (`src/App.cpp:515-520`); `PumpAudio` (`AudioDevice_Web.cpp:419`)
  calls `MixSources` → each source's `RenderAudio` → copies the mixed PCM into the
  JS SAB ring via `js_audio_ring_write` (`HEAPF32.subarray` copy, line 159). The
  only true cross-thread boundary is the SAB itself, read by the worklet thread —
  and that is plain `Atomics`, **no wasm pthreads / `-pthread` /
  `-sSHARED_MEMORY` needed** (the SAB is allocated in JS by `new
  SharedArrayBuffer`, not in the wasm heap). The `std::atomic` back-pressure in
  the RB3 receiver is single-threaded-but-correct on web.
- **COOP/COEP for SharedArrayBuffer:** `native/web/server.py:44-47` already sends
  `Cross-Origin-Opener-Policy: same-origin` + `Cross-Origin-Embedder-Policy:
  require-corp` + `Cross-Origin-Resource-Policy: cross-origin`, so the JS `new
  SharedArrayBuffer(...)` constructor is permitted (cross-origin isolation
  satisfied). `index.html` needs no audio changes (the canvas-click in the boot
  flow already serves as the resume gesture).
- **Synth + stream (layer c + a):** `SynthPreInit` (`src/system/synth/Synth.cpp:265`)
  reads `(use_null_synth FALSE)` and selects `CreateNativeSynth()` under
  `HX_NATIVE` (web defines it) → `NativeSynth::Init`
  (`native/src/rb3_synth_native.cpp:29`) → `AudioDevice::GetInstance().Init(44100)`
  + binds `StreamReceiver::sFactory = &RB3CreateNativeStreamReceiver`. The
  RB3-shaped receiver (`native/src/rb3_stream_receiver_native.cpp`) is
  `: public StreamReceiver, public AudioSource` and registers itself with
  `AudioDevice::AddSource` on `PlayImpl` — **shared verbatim between native and
  web, zero `#ifdef HX_WEB` divergence.** The MOGG decode path is real:
  `Synth::NewStreamDecoder` → `VorbisReader` (`Synth.cpp:580`), keys from
  `rb3_keychain_native.cpp`, AES/CTR from tomcrypt, HMX incremental poll from
  `rb3_vorbis_poll_shim.cpp`. All in `RB3_WEB_NATIVE_GLUE`
  (`native/CMakeLists.txt:639-648`); `Synth.cpp`/`VorbisReader.cpp` compile into
  the wasm (codec.h alloca clash fixed by `#ifndef HX_NATIVE`).
- **W3c claim of record** (`docs/plans/web-port/W3_BOOT_TO_SONG.md:456-526`,
  commit `f21547d9`): boot console shows `AudioDevice: initialized (web) -- 44100
  Hz, ring 32768 frames` + `AudioWorklet connected`; song decodes (`MOGG_DBG …
  OggS`, `STREAM_DBG: kBuffering -> kReady`), mixer runs ~30 fps.

### What is broken, latent, or missing

1. **`TheSynth` is defined twice (latent footgun).**
   `src/system/synth/Synth.cpp:58` defines `Synth *TheSynth;` (the real engine
   global) and `native/src/main_web.cpp:98` *still* defines `Synth *TheSynth =
   nullptr;`. The link only survives because of `-Wl,--allow-multiple-definition`
   (`native/CMakeLists.txt:405`). The `main_web.cpp` block (lines 86-98, with a
   stale comment that literally says *"W3c links the real Synth and removes
   this"*) was written for W3a's audio-free build (commit `52e95b60`, 2026-05-29
   07:59) and was **never removed** when W3c re-added the real `Synth.cpp` 2 hours
   later (commit `bca377ac`, 10:15). Today both TUs ship the symbol; wasm-ld
   collapses them to one address (first-definition-wins), so it functionally works
   *only by accident* and is one link-order change away from binding reads/writes
   of `TheSynth` to a stale `nullptr` static-init.

2. **Stale, contradictory comments in the App boot spine.**
   `src/App.cpp:250-265` (`#ifdef HX_WEB`) still asserts *"Web is audio-free
   (Synth.cpp is excluded… SynthInit() resolves to a no-op stub)"* — false since
   W3c. `SynthInit()` (line 249) now runs the real synth. The `Fader::Init()` /
   `BinkClip::Init()` factory registrations inside that block are still needed
   (harmless), but the comment will mislead the next implementer into thinking web
   has no audio.

3. **All menu / UI one-shot SFX are silent — on native AND web.**
   `Sfx.cpp` (`SfxInst` ctor, `src/system/synth/Sfx.cpp:23-24`) plays samples via
   `smp->NewInst(false, 0, -1)` (`SynthSample::NewInst(bool,int,int)`). That
   virtual is **declared** in `SynthSample.h:33` but **has no definition in
   `SynthSample.cpp`** (grep: 0 hits) — its real impl is the platform sample
   player (Wii `SampleInstWii`, or the engine's `SampleInst_Native.cpp`). The
   engine TU is **platform-EXCLUDED for RB3** (`native/CMakeLists.txt:154`,
   because RB3's `SampleData` lacks `HasData/NumChannels/DataPtr/GetNumSamples`),
   with **no RB3 replacement**. On native it resolves to a weak no-op stub
   (`native/src/rndobj_synth_link_stubs.s:71` → returns null); on web it resolves
   via `missing_stubs.js` to 0. So `inst` is always null → menu nav blips, confirm
   sounds, gameplay one-shots from `Sfx`/`MidiInstrument` (`MidiInstrument.cpp:22`
   uses the same `NewInst`) **never play.** Only the song MOGG stream is audible.

4. **`RB3StreamReceiverNative` does not override the `HX_WEB` `DebugDescribe`
   hook.** `AudioSource::DebugDescribe` (`AudioDevice.h:27`, `#ifdef HX_WEB`) is
   the per-source diagnostic the engine's `DebugDumpSources()` (`window.rb3AudioStats()`)
   prints. RB3's receiver doesn't override it (grep: 0 hits), so audio-debug dumps
   show empty descriptions — DC3's `StreamReceiverNative::DebugDescribe`
   (`milo-native-engine/src/platform/StreamReceiver_Native.cpp:235`) is the model.
   Cosmetic, but it blunts the only browser-side audio observability tool.

5. **Audibility never actually proven in-browser.** Every W3c verification was
   headless ("headless = no speakers, same as native v1",
   `W3_BOOT_TO_SONG.md:496`) — i.e. they confirmed the *decode + mix loop runs*,
   not that PCM reached the worklet and a human (or a captured WAV) heard it. The
   engine ships the exact tools to close this (`window.rb3CaptureAudio()` /
   `rb3DownloadAudio()` / `rb3DumpSAB()` / `rb3AudioStats()`,
   `AudioDevice_Web.cpp:312-336, 477-512`) but no test exercises them.

6. **MOGG fetch is a single main-thread sync-XHR (W4 follow-up).** The ~35 MB
   `.mogg` is fetched on-demand on the main thread; fine on localhost (~0.5 s),
   but stalls multi-seconds over a real network (`W3_BOOT_TO_SONG.md:557-561`,
   `W4_POLISH.md:167-168`). Not a "no sound" bug, but a UX cliff before the first
   note.

---

## 2. Goal — desired behavior

In a WebGPU-capable browser tab served by `server.py`, the RB3 web build should:
1. Open the AudioWorklet on boot (already does).
2. Play **menu/UI one-shot SFX** (button blips, confirms) when navigating — to
   parity with native (which is *also* currently silent for these).
3. Play the **song MOGG multitrack** (≈6 channels mixed) audibly during gameplay,
   confirmed by a downloadable non-silent capture WAV.
4. Carry no latent duplicate-symbol / stale-comment hazards in the audio path.

---

## 3. Proposed approach — phased

### Phase 0 — De-risk the existing path (no behavior change, prevents regression)

- **(c) Remove the duplicate `TheSynth`.** Delete
  `native/src/main_web.cpp:86-98` (the `class Synth; Synth *TheSynth = nullptr;`
  block + its stale comment). `Synth.cpp:58` is now the sole definition and is in
  the web source set. Verify the link no longer relies on
  `--allow-multiple-definition` for this symbol (run `wasm-ld` map / `nm` on the
  two objects). This is the single highest-value cleanup — it removes UB.
- **(a) Fix the stale App.cpp comment.** Rewrite the `src/App.cpp:250-262`
  `#ifdef HX_WEB` comment to state that web audio is live (W3c) and that
  `Fader::Init()`/`BinkClip::Init()` remain only as inert-factory registration.
  **No code change** — keep the two `Init()` calls; this is a comment-only edit so
  the `#else` (MWCC PPC) path is byte-identical. (If even the comment edit is
  considered risk to the matched fork, move the explanation to a glue-side doc and
  leave a one-line pointer.)

### Phase 1 — Prove audibility (the actual deliverable)

- **(test) Extend `scripts/web/w3c-gameplay-test.mjs`** (or add
  `scripts/web/web-audio-capture.mjs`): after reaching `game_screen`, call
  `page.evaluate(() => window.rb3CaptureAudio())`, wait 3 s (CAPTURE_SECONDS),
  then `window.rb3DownloadAudio()` (or read the SAB directly via
  `window.rb3DumpSAB(64)` / `window.rb3AudioStats()`), and assert the capture is
  **non-silent** (max abs sample > threshold, nonZero count > 0). The canvas
  `.click({force:true})` already in the test (line 159) satisfies the
  AudioContext-resume gesture. This converts "decode loop runs" into "PCM reaches
  the worklet."
- This phase needs **no source changes** — it consumes the engine's existing
  `AudioDevice_Web` debug exports. It is the cheapest way to confirm the W3c claim
  is real and to catch the duplicate-`TheSynth` regression if Phase 0 is skipped.

### Phase 2 — Menu/UI SFX (the genuine missing feature, native + web)

- **(c) Provide an RB3-shaped `SampleInst` / `SynthSample::NewInst`.** Mirror
  `rb3_stream_receiver_native.cpp`'s pattern: a new
  `native/src/rb3_sampleinst_native.cpp` (gated `#ifdef HX_NATIVE`, so it serves
  native and web) defining `SampleInst *SynthSample::NewInst(bool,int,int)` →
  returns an `RB3SampleInstNative : public SampleInst, public AudioSource` that
  decodes RB3's `mSampleData` (PCM/ADPCM per `SampleData::GetFormat`) and renders
  one-shot into the mixer via `AudioDevice::AddSource`. Strong def wins over the
  weak `rndobj_synth_link_stubs.s` stub (same mechanism as `rb3_keychain_native.cpp`).
  Add it to both the native target and `RB3_WEB_NATIVE_GLUE`
  (`native/CMakeLists.txt`). The hard part is RB3's older `SampleData` API surface
  (the very mismatch that excluded the engine's `SampleInst_Native.cpp`); inspect
  `src/system/synth/SampleData.h` for the real accessors and adapt. **Do native
  first** (`rb3-native` rebuilds in ~3 s, headless), then confirm on web.
- **(c) Add the `HX_WEB DebugDescribe` overrides** to both
  `RB3StreamReceiverNative` and the new sample-inst class so
  `window.rb3AudioStats()` lists meaningful per-source state. Trivial, copy DC3's
  shape.

### Phase 3 — Streaming MOGG fetch (W4, defer)

- **(b/c) Worker-backed range-streaming MOGG fetch** so the first-note stall over
  a real network is hidden behind the existing loading screen. `server.py` already
  supports HTTP `Range` (`_serve_range`, line 317). This is the documented W4 task,
  not a "no sound" blocker — defer.

---

## 4. Key files

- `milo-native-engine/src/audio/AudioDevice_Web.cpp` *(b)* — web AudioDevice:
  SAB ring, AudioWorklet, `PumpAudio`, autoplay-resume gesture, capture/dump
  debug exports. No headless skip.
- `milo-native-engine/src/audio/AudioDevice.h` *(b)* — shared `AudioDevice` /
  `AudioSource` interface; `PumpAudio` + `HX_WEB DebugDescribe` declarations.
- `milo-native-engine/src/platform/web/assets/audio-worklet.js` — the
  `milo-audio-processor` AudioWorkletProcessor (SAB drain). Deployed POST_BUILD.
- `native/src/rb3_synth_native.cpp` *(c)* — `NativeSynth`: `AudioDevice::Init`,
  binds `StreamReceiver::sFactory`. Shared native+web.
- `native/src/rb3_stream_receiver_native.cpp` *(c)* — RB3 `StreamReceiver` +
  `AudioSource` bridge (the audible song-MOGG path). Missing `DebugDescribe`.
- `native/src/main_web.cpp` *(c)* — web boot SM; **lines 86-98 hold the stale
  duplicate `TheSynth`** (delete).
- `src/App.cpp` *(a)* — `RunOneFrame` calls `PumpAudio()` (line 519) + the
  `SynthInit()` boot (line 249); **lines 250-262 stale "audio-free" comment.**
- `src/system/synth/Synth.cpp` *(a)* — `SynthPreInit`/`SynthInit`, `TheSynth`
  (line 58, the true global), `NewStream*`/`NewStreamDecoder` HX_NATIVE MOGG path.
- `src/system/synth/Sfx.cpp` *(a, read-only)* — `SfxInst` ctor calls
  `SynthSample::NewInst` → currently null on native+web (silent SFX).
- `src/system/synth/SynthSample.h` / `.cpp` *(a, read-only)* — `NewInst(bool,int,int)`
  declared (`.h:33`) but **never defined** in `.cpp`; the gap Phase 2 fills.
- `native/CMakeLists.txt` — `RB3_WEB_NATIVE_GLUE` (audio TUs, 639-648),
  `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE` (SampleInst excluded, 154),
  `--allow-multiple-definition` (405).
- `native/web/server.py` — COOP/COEP headers (44-47, satisfy SharedArrayBuffer);
  Range support (317).
- `scripts/web/w3c-gameplay-test.mjs` — the menu→gameplay Playwright driver to
  extend with a capture assertion.

---

## 5. Quick wins (< 1 day) vs larger work

**Quick wins:**
- Delete the duplicate `TheSynth` in `main_web.cpp:86-98` (Phase 0a) — minutes,
  removes real UB.
- Fix the stale App.cpp audio-free comment (Phase 0b) — minutes (comment-only,
  matched-path byte-identical).
- Add a capture-assert step to the web gameplay test (Phase 1) — a few hours;
  **this is what actually answers "is there sound on web?"** using existing
  engine debug hooks, no source changes.
- Add the `HX_WEB DebugDescribe` override to the stream receiver — minutes.

**Larger work:**
- Phase 2 (RB3 `SampleInst`/`NewInst` for menu/UI + MIDI sample SFX) — ~1.5-3
  person-days; gated on understanding RB3's older `SampleData` decode API. Benefits
  native and web equally.
- Phase 3 (worker-streamed MOGG fetch) — separate W4 effort, multi-day.

---

## 6. Dependencies & risks

- **`SampleData` API shape (Phase 2):** RB3's 2010 `SampleData` is the exact
  reason the engine's `SampleInst_Native.cpp` was excluded. Decoding it correctly
  (format, channel count, loop points) is the only real unknown; budget time to
  read `SampleData.h`/`.cpp` and possibly the Wii `SampleInstWii` for the data
  layout. Risk: medium.
- **Matched-fork constraint:** `Synth.cpp`, `Sfx.cpp`, `App.cpp`, `SynthSample.*`
  are layer (a). The only (a) touch proposed is the **comment-only** App.cpp edit;
  everything functional lands in (c) glue (`main_web.cpp` delete + new
  `rb3_sampleinst_native.cpp`). Keep the `NewInst` definition in glue (strong-def-
  over-weak-stub), never edit `SynthSample.cpp`.
- **Concurrency note:** `src/system/char/CharBones.cpp` and
  `src/system/world/LightPreset.cpp` are another session's uncommitted work — none
  of this plan touches them.
- **Autoplay policy:** already handled (the boot test clicks the canvas, and
  `js_audio_init` registers keydown/click/touchstart resume). A *real human* on a
  fresh tab must still interact once before sound starts — document in the UI.
- **Risk of Phase 0 alone:** removing the duplicate `TheSynth` could, in theory,
  expose an ordering bug if the engine global's static init runs late — verify with
  Phase 1's capture test immediately after.

---

## 7. Effort & priority

- **Phase 0 (de-risk dup symbol + comment):** P0, ~0.25 person-day. Do first.
- **Phase 1 (prove audibility):** P0, ~0.5 person-day. This is the literal
  acceptance criterion for "web has sound."
- **Phase 2 (menu/UI SFX via RB3 SampleInst):** P1, ~2-3 person-days. Real
  feature gap, improves native too.
- **Phase 3 (streamed MOGG fetch):** P2, multi-day, W4 scope. Defer.

Overall area priority: **P1** (the core song-audio path is already wired and
claimed working; the open items are confirmation + the SFX feature gap + a UB
cleanup — none block the headline "song plays" milestone, but Phase 0/1 are cheap
and should ship immediately).

---

## 8. Verification plan

All steps are read-only against the *already-built* artifacts unless rebuilding is
explicitly required (do NOT rebuild in a shared tree without coordination).

1. **Confirm current pipeline wiring (no rebuild):**
   `python3 native/web/server.py --port 8421` (auto-detects
   `orig-assets/extracted` — must serve the dir with text DTAs **and** the
   song mogg/mid, per W3c Trap 1). Open `http://localhost:8421` in
   Chromium-with-WebGPU. Console must show `AudioDevice: initialized (web) -- 44100
   Hz, ring 32768 frames` and `AudioDevice: AudioWorklet connected (44100 Hz)`.
   Also confirm `Audio debug commands: rb3CaptureAudio(), rb3DownloadAudio(),
   rb3DumpSAB(n), rb3AudioStats()` printed.
2. **Drive to gameplay + capture (audibility proof):** run
   `node scripts/web/w3c-gameplay-test.mjs` to reach `game_screen`; in the same
   tab (or via the test) run `rb3CaptureAudio()`, wait 3 s, then
   `rb3AudioStats()` (expect `pumpCount` rising, `active source count > 0`) and
   `rb3DumpSAB(32)` (expect `nonZero > 0`, non-trivial min/max). `rb3DownloadAudio()`
   writes `rb3_web_capture.wav` — play it off-box; it must contain the song, not
   silence. **This is the pass/fail gate for "web has song audio."**
3. **After Phase 0 (dup-symbol removal) — rebuild + relink check:**
   `scripts/web/build.sh`; confirm the link has exactly one `TheSynth` (grep the
   `.js`/wasm symbol table or the wasm-ld map) and re-run step 2 (no regression —
   `pumpCount` still rises, capture still non-silent).
4. **After Phase 2 (SampleInst) — native first:**
   `cmake --build native/build-native --target rb3-native`, then headless
   `MILO_AUDIO=1 MILO_AUDIO_BACKEND=null DC3_DUMP_AUDIO=/tmp/sfx.wav` while driving
   the menu via the HTTP API (`RB3_HTTP=1`, `/api/input` nav verbs) — the dump WAV
   must now contain menu blips (it is silent today). Then rebuild web and repeat
   step 2 capturing during *menu navigation* (not just gameplay): the capture must
   show nonZero samples on each confirm/nav.
5. **Regression guard:** native `RB3_GAME=1` 900-frame headless run still exits 0
   (`AudioDevice: skipped` on this no-output box is expected and correct — N10);
   `?milo=` harness + W3b/W3c-nav menu flow still pass.
