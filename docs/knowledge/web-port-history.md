# Web Port & Audio/Load History

This document is migrated from agent-memory notes and consolidates the RB3
Emscripten/WASM web-port waves, audio-fidelity work, and load-performance
investigations into one version-controlled reference. Each section captures the
problem, root cause, fix (file/flag/default/commit), status, and durable
gotchas. Point-in-time claims (file:line, commit hashes, percentages) should be
verified against current code before being treated as live state. The native
build reproduces web bugs identically and rebuilds in ~3s — debug there first
(see CLAUDE.md "Web Build").

---

## Web port orchestration (W0–W10)

Model used: orchestrator/coordinator of subagents driving the web port
(`docs/plans/web-port/`, phases W0–W4+). Goal = RB3 booting + one song
end-to-end in a browser (WASM + WebGPU), mirroring DC3's completed web port.

- **Model tiering for dispatched agents:** Opus = planning + hard
  GPU/async/codegen; Sonnet = mechanical ports (CMake wiring, file adapt);
  Haiku = read-only questions + trivial refactors.
- **Workflow:** per-phase docs (`W0_…md`…`W4_…md`) are the handoff artifact.
  After each milestone, RE-PLAN with an Opus agent so docs reflect ACTUAL
  implementation state (docs written pre-implementation drift). Verify subagent
  claims against real artifacts (git log, build output, smoke PASS).
- **COMBINED-VERIFY is non-negotiable.** Build+verify the merged set of parallel
  fixes before landing — it repeatedly caught cross-cutting regressions no
  standalone run could see (e.g. the magenta-fix `prelit` gate stripping vertex
  colour off the now-drawing band chars → flat-white band).

Landed milestone highlights:
- **W9 (rb3 `3e9463ae`, engine `4ad3a5f`):** scene-wide dimness SOLVED — the
  WebGPU surface was non-sRGB (`RGBA8Unorm`) while shader math is linear (no
  gamma encode → ~2.5× too dark). Fix = sRGB OETF at fragment output in engine
  `src/gfx/standard_wgsl.inc`. Also: white sustain tails (per-fret `TexXfm`
  U-offset never written; derive fret colour from material name in
  `Rnd_Wgpu_RB3.cpp::DrawMesh`), console-spam lag (per-frame `console.log` was
  the killer — `BandRnd::EndFrame`/`DataArray::Execute`/`Debug::Fail` → 0/s,
  60fps), multiplier ×4 at song start (`StreakMeter::Reset` now fires
  `mHideMultiplierTrig`).
- **W10 (rb3 `b0b2ec95`, engine `070562a`):** characters T-pose fix (drive
  `TheBandDirector->Poll()` from `App::RunOneFrame`; `WorldDir::DrawShowing`
  also draws the 4 BandCharacters on wide shots), scene-wide magenta cast fix
  (baked per-vertex lighting × white dynamic light; `BeColor()` D3DCOLOR ARGB
  R/B swap) resolved via a 3-way `isSkinned` gate (prelit verbatim /
  non-prelit static suppress / non-prelit skinned desaturate,
  `kSkinnedVtxChroma=0.25`), and main_hub menu text overlap (`Font.h` — Xbox hub
  atlas is 512×1024, `CellDiff` carries a spurious atlasH/atlasW factor).

**Durable gotchas:** all rb3 `src/` edits are HX_NATIVE/HX_WEB/`#ifdef
__EMSCRIPTEN__`-gated so Wii MWCC match is preserved; engine edits are
free/unmatched. Web `getenv`/`ENV` debug toggles (the `Module.preRun`
`ENV.x='1'` trick) DO NOT reach the WASM — a probe that "never fired" may just
be disabled; make probes temporarily UNCONDITIONAL or use `EM_ASM` counters / a
DTA hook. Both master and engine-main routinely advance mid-wave (user commits
concurrently) — cherry-pick onto current HEAD, never ff from a stale base; when
the user has uncommitted work in a file you edit, ASK rather than clobber.
Known residual: character skin/cloth textures (`dummy_torso.tex`,
`poredetail_norm.tex`, `skin.pal`, `*_output.tex`) are MISSING from the
extracted asset set — chars rely on desaturated vertex colour.

## W1 deviations (clear-frame) that later waves inherit

W1 landed at rb3 `4183f495` (canvas clears to rgb(51,102,178)) but took
shortcuts W2+ inherit:
1. **`BandRnd::InitGpu` was sidestepped** — it aborts if `mGpu.IsReady()` is
   false (always false async on web). W1 drove `GpuDevice` directly via a
   file-static `sWebGpu` + `DrawClearFrame()`. Real `BandRnd` lives in
   `milo-native-engine/src/platform/Rnd_Wgpu_RB3.{cpp,h}` (NOT
   `rb3/native/src/rb3_band_rnd.cpp`, a 0-byte stub). Full backend needs
   `InitGpu` split across `BOOT_GPU_WAIT` / `BOOT_GPU_READY`.
2. **4 source files filtered from rb3-web** for libc++/emcc incompatibility:
   `Gen.cpp` + `MultiMeshProxy.cpp` (now-private libc++
   `list::iterator(__base_pointer)` ctor); `VorbisReader.cpp` + `Synth.cpp`
   (bundled `oggvorbis/codec.h` `inline void *alloca` clashes with clang
   `__builtin_alloca`). Audio needs VorbisReader recovered.
3. **x86 GAS link-stub `.s` files don't assemble under emcc** — replaced with
   `-Wl,--allow-undefined` + `native/web/rb3_pre.js` patching
   `Module.instantiateWasm` to swap abort-stub imports for a no-op returning 0.
4. **`Symbol time` renamed `_hmx_time_sym`** under emcc (libc++ `<chrono>`
   pulls `<time.h>` `::time()`).

Consumer CMake pattern (mirror DC3 `c800138e`): `set(MILO_BUILD_WEB ON … FORCE)`
+ `list(APPEND MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE <*_Web.cpp>)` BEFORE
`add_subdirectory(engine)`; then `milo_engine_apply_web_target_options`,
`milo_engine_set_web_canvas_selector(<tgt> "#rb3-canvas")`, `MILO_WEB_AUDIO_NS=rb3`.

## W2 findings — geometry rendering DONE on web

W2 = render `.milo` in browser. Two "milo won't load" bugs were NOT what they
looked like:
1. The "multi-chunk ChunkStream load fault" was a **missing object-factory
   registration**, not endianness. An unregistered `*Dir` subclass can't be
   `ReadDead`-skipped → stream desyncs → giant alloc. Fix: register
   bandobj/track/world/ui Dir-container factories before `DirLoader` runs
   (`rb3/native/src/rb3_game_object_factories.cpp`). The `.milo_xbox` chunk
   HEADER is little-endian on disk; the body is big-endian. Don't chase
   endianness.
2. The real blocker for menu/HUD milos was a **one-line MEMFS-path bug** (rb3
   `4ef00c69`) in `native/src/native_file.cpp`: relative resource-milo paths
   (`FileRoot()` is `"."` on web) were `FS.writeFile`'d against MEMFS cwd
   without creating parents → `ErrnoError` → null `ResourceDir()` → `abort()` at
   `UILabel.cpp:522` (looked like a ~40s hang). Fix = anchor relative fopen to
   `/data/` before `WebAssetsFetchSync`.

**Strategic boundary:** the static mesh-walk harness (`LoadMiloAndWalk`) walks
`RndMesh` only. TEXT labels, UI interactivity, and song playback need the real
`App`-driven boot (`sApp = new App(...); sApp->RunOneFrame()`) — that App boot IS
W3. Port-shim convention for web-only changes in matched decomp files
(`DirLoader.cpp`, `UIComponent.cpp`): `#ifdef __EMSCRIPTEN__` (invisible to Wii
MWCC and native clang → zero match/native impact).

---

## Dual build (release + debug) + HTTP wasm caching

Landed 2026-06-06. Two builds switchable at runtime, with HTTP caching of the
release wasm so reloads skip re-download+recompile.

- **Layout:** `scripts/web/build.sh` deploys
  `native/web/build/{release,debug}/rb3-web.{js,wasm,.br,.gz}`. `index.html` +
  `audio-worklet.js` stay at ROOT (shared; worklet loaded via document-relative
  `addModule`). Two emcc dirs: `native/build-web` (debug, -O0 -g2) +
  `native/build-web-release` (release, -O0 -g0). Brotli q11 is release-only
  (debug = gzip; brotli on the 28M -g2 wasm is the multi-minute step).
- **Switch:** `http://host:8421/` = release (cached);
  `http://host:8421/?debug=true` = debug (no-store).
- **Caching (`server.py _cache_control_for_path`):** `/release/*` →
  `public, max-age=31536000, immutable`; `/debug/*` + `/api/version` →
  `no-store`; everything else → `no-cache`. Release URLs version-busted with
  `?v=<token>` (assets mtime + release-wasm mtime).

**GOTCHAS:**
- `_maybe_serve_precompressed` must strip the `?query` BEFORE the `.wasm/.js`
  endswith check, else `?v=` defeats brotli negotiation.
- **Editing `server.py` requires RESTARTING the running `python3
  native/web/server.py`** — a stale process serves stale cache logic.
- Verifying reload-cache reuse needs a PERSISTENT browser profile
  (`chromium.launchPersistentContext(tmpdir)`), not `browser.newContext()` —
  the ephemeral cache evicts the 2.4MB wasm under the boot's asset-fetch flood
  (cost ~1hr chasing header/streaming theories; it's eviction, real users cache
  fine).
- **DC3 got the same treatment** (dc3 `cf5a975a`): same 3 files
  (`native/web/{build.sh,server.py,index.html}`); DC3 has no `RB3_WEB_RELEASE`
  toggle so release = same build stripped via `wasm-opt --strip-debug`.

---

## Web polish 2026-06-02 (harness, load freeze, font/icon)

Master `a207f93f`, engine `f52f2f1`.

- **No-xvfb web-test harness:** `scripts/web/lib/core.mjs` is the shared module
  (launches Playwright's bundled Chromium HEADLESS,
  `--enable-unsafe-webgpu --use-angle=vulkan`; no xvfb, no system chromium).
  Exports parseArgs/waitForServer/launchBrowser/createCapture/engineState/
  navigateTo/screenshot/cleanup. `node scripts/web/smoke-test.mjs` = boot→main_hub
  gate. Canvas id `rb3-canvas`; state on `window.rb3CurrentScreen/rb3FocusButton/
  rb3OvershellView/rb3SongCount`. GOTCHA: keyboard nav PAST main_hub is
  timing-finicky — use the `/api/input` verb path, not raw keypresses.
- **Web load freeze root cause:** `LoadMgr::Poll()` HX_WEB arm drained the WHOLE
  `mLoading` queue in ONE `RunOneFrame` (repaint only on Poll return), freezing
  the tab. Fix = frame-bound to `RB3_LOADER_BUDGET_MS` (default 8) then return;
  `mPeriod>=1e29f` drainToEmpty sentinel preserves PollUntilEmpty's sync
  contract. ~329ms→8-16ms/frame. Loader is CPU-bound (DTA parse + ctor), NOT
  I/O — off-thread reader regresses. Diag: `RB3_FRAME_INSTRUMENT=1`,
  `scripts/web/loadperf-responsiveness.mjs`.
- **Overshell font/icon** (both Xbox 512×256 wide-atlas fonts): (1) squish =
  `RndFont::CellDiff` spurious atlasH/atlasW factor, fixed in `Font.h`
  (HX_NATIVE) via UV-cell recovery, guard widened `h>w`→`h!=w`. (2) white blob =
  engine `Rnd_Wgpu_RB3.cpp::BandRnd::DrawMesh` `useAlphaAsRGB` heuristic fired
  on colour-icon glyph font `instrument_icons_small*` → discarded RGB. Fix =
  exclude materials whose name contains `"icon"`. PATTERN for "white where a
  textured glyph should be": check `isTextMeshHeur`/`useAlphaAsRGB`
  misfiring on a colour-artwork font.

---

## USB guitar controllers on web (SHIPPED)

2026-07-12. All in the `__EMSCRIPTEN__` branch of
`native/src/rb3_joypad_native.cpp` (desktop/GLFW byte-identical). Docs:
`docs/native/web-guitar-input.md`.

- **Gamepad API path** (rb3 `1ff2d174`, e2e 27/27): families `ps3wii_rb` (12ba,
  primary), `xinput_rb`, `gh_ps3`. Whammy via `window._rb3GpWhammy` → `mSticks`;
  tilt ORs bit 8. Runtime remap `window.rb3GuitarMap`; `window._rb3GpDebug=1`
  logs raw buttons/axes. Also `b94e6a0a` (guard glibc-only `backtrace()`/
  `<execinfo.h>` in BandCharacter.cpp + CharDriver.cpp with `#ifndef
  __EMSCRIPTEN__`) and `c2fc51ac` (e2e harness).
- **WebUSB X-plorer path** (rb3 `c07480eb`, e2e 17/17): GH X-plorer 1430:4748 is
  Xbox 360 XUSB (class 0xFF/0x5D), NOT HID → Gamepad API permanently blind on
  macOS (all-null getGamepads). Fix: page-level `native/web/guitar-webusb.js`
  claims it via WebUSB (works on macOS — no OS driver owns the interface),
  decodes XUSB reports, injects a synthetic `mapping:'standard'` pad into wrapped
  getGamepads → existing C++ `xinput_rb` family consumes unchanged (zero wasm
  rebuild). Linux: claimInterface fails (xpad owns it) = expected, Gamepad path
  works there.
- **/rb3 HTTPS serving** (rb3 `86646f94` + nginx): Gamepad/WebUSB need a secure
  context → https://home.freeqaz.com/rb3/ via nginx proxy to server.py:8421.
  Page-level URL-base shim in index.html patches fetch/XHR/sendBeacon/media.src.
  nginx: `location ^~ /rb3/` proxy_pass with trailing slash; telemetry →
  `return 204` (NOT 403 — rb3_pre.js retries forever on non-ok); proxy_buffering
  off.

**NOT verified:** real physical hardware (button indices are documented-HID
best-effort; remap is the fallback), audible whammy effect, actual overdrive
deploy, whammy rest polarity (assumed −32768=rest; flip in `decodePacket()` if
inverted).

---

## Web/native audio state

The core song/MOGG path is NOT a missing port — it's shared in the engine:
`milo-native-engine/src/audio/AudioDevice_Web.cpp` (AudioWorklet + SAB ring),
namespaced `MILO_WEB_AUDIO_NS=rb3` (processor name is the fixed engine-wide
`milo-audio-processor`), driven by `AudioDevice::PumpAudio()` from
`App::RunOneFrame`. Song decode = VorbisReader + rb3_keychain + tomcrypt;
StreamReceiver bridge = `native/src/rb3_stream_receiver_native.cpp` (shared
native+web). Chronological fix stack (newest first):

- **Off-main mix stem-anchor + pause-latency** (engine `20dba552`, pin rb3
  `747b45b1`): `RB3_WEB_OFFMAIN_MIX` (default-ON) had two web-only bugs. (1)
  Guitar stem ~10s behind — each stem's worklet cursor seeded independently with
  no shared song-start anchor; a late joiner starts ~one stem ring (~8.9s,
  `kStemRingFrames` `AudioDevice_Web.cpp:56`) behind. Fix
  (`PumpAudioOffMainStems`): defer seeding until the armed set is STABLE for one
  pump, then seed the whole batch in ONE tick (one t=0). Opt-out
  `RB3_NO_STEM_ANCHOR=1`. (2) Pause ramps latency to 500ms ceiling — the
  hard-underrun grow branch wasn't gated on "audio actually playing". Fix: gate
  on a playback heartbeat (`mOffMainFedFramesThisWindow`); `mSources` does NOT
  hold the music in off-main mode. Both DC3-safe. Doc:
  `docs/native/web-audio-fixes-2026-06-21/IMPL_AND_VERIFY.md`.
- **"Jitter / buffer stalls" = RENDER-FPS STARVATION, not GC/leak** (rb3 docs
  `609a7cc0`): REFUTED the memory-leak→GC hypothesis (heap flat; a v8.gc_stats
  tracing observer-effect faked 85ms MajorGCs, removed). REAL cause: web gameplay
  ran ~25fps at -O0 with per-draw vbuf/ibuf recreation; `PumpAudio` runs INSIDE
  `RunOneFrame` on the single JSPI main thread → a slow frame can't refill the
  SAB ring → ~22 underruns/s. Fix (engine pin 5ac9501 + rb3 -O2 `735fff11`):
  mesh-cache v2 + **-O2 release wasm** (was -O0) → 25→60fps, underruns 22/s→0/s.
  GOTCHA: underrun telemetry UNDERCOUNTS (worklet only flags ring <2.67ms) — use
  the low-water-mark (minDepth) series; "underruns=0" alone ≠ "no jitter". Tool:
  `scripts/web/audio-jitter-profile.mjs`.
- **SFX sound bank no longer skipped** (rb3 `db54b18f`, `e973434f`, `d8bdae49`):
  App.cpp gated the common-bank `LoadFile` behind `#ifndef __EMSCRIPTEN__`. Fix:
  narrowed the `#ifndef` to ONLY the `TheSynth->SetUnk40` line (null-guarded)
  so App::App preprocesses byte-identically on Wii (match unchanged). Also
  `SidecarDir()` now auto-discovers the derived sidecar tree. Route =
  XBOX-XMA (Wii banks are kNintendoADPCM DSP-ADPCM with no decoder). Multitrack
  ring-depth `StreamReceiver.{h,cpp}` 0x18000→0xC0000 + `RB3_STREAM_BUF_SECS`.
- **"Chipmunk" (too-fast pitch)** (engine `b458b18`): web-only — browser runs
  AudioContext at hardware rate (commonly 48000); engine mixed at 44100 and
  pushed straight to the SAB ring with no rate conversion → replayed at 1.0884×
  fast. Fix (`AudioDevice_Web.cpp`): `js_audio_init` returns actual
  ctx.sampleRate; `PumpAudio` resamples 44100→device rate (linear interp,
  click-free via persistent `mResamplePos`/`mResampleLast`). GOTCHA: headless
  box honors 44100 → chipmunk does NOT repro here (force ctx=48000 to test).
- **"Clipped noise"** (engine `AudioDevice.{h,cpp}` + `AudioDevice_Web.cpp`):
  a song sums 11–15 stems as additive AudioSources; even at ~−4 dB the SUM peaks
  ~3.2× full scale (channel COUNT, not per-stem level), and the mix applied 1.1×
  master gain then HARD-CLAMPED → 1124 flat-top runs. Fix (in
  `AudioDevice::MixSources`): replaced 1.1× + hard clamp with a one-pole
  stereo-linked peak limiter (T=0.90, atk 3ms, rel 80ms, `mLimiterEnv`) + a
  soft-knee saturator backstop (knee 0.95); `sMasterGain`→`sPreGain` default 1.0
  (DC3_AUDIO_GAIN-overridable). Content-adaptive ⇒ DC3-safe. Limiter alone left
  1.2% railing; the soft-knee backstop took it to 0 flat-top runs. Decisive
  metric = alignment-free flat-top-RUN count (waveform/spectrogram corr is
  unreliable on cross-run captures).
- **Song preview + song audio play** (`src/App.cpp` + `VorbisReader.cpp`,
  HX_NATIVE): (1) preview never FIRED — call `TheMusicLibrary->Poll()` in
  RunOneFrame (opt-out `RB3_NO_LIBRARY_POLL`). (2) **Vorbis CTR-seek
  double-swap** — previews seek mid-song; `VorbisReader::DoRawSeek` re-key
  `*(uint*)mNonce = EndianSwap(byte/16)` is correct on big-endian Xbox/Wii but
  DOUBLE-swaps on little-endian native/web → wrong AES-CTR counter → garbage
  decrypt → phantom EOF → ring drains negative → silence. Fix = store `byte/16`
  directly (no EndianSwap) under `#ifdef HX_NATIVE`.
- **Song-audio silence (deadlock)** (rb3 `10af87ab`, shared native+web in
  `rb3_stream_receiver_native.cpp`): producer/consumer chicken-and-egg —
  `RenderAudio` only rendered when `mSendActive`, set only by a producer send,
  which during kPlaying only fires when the play cursor advances, which only
  advances when `RenderAudio` renders → 0.0 RMS forever. Plus a slip gate. Fix:
  `RenderAudio` plays the buffered ring continuously (available =
  `mRingWrittenSpace` − play-consumed, NOT writePos−readPos); force
  `mSlipEnabled=false`; non-blocking. DIAGNOSIS LESSON: probe-first — the
  confident "Play() never fires" was wrong.
- **SFX / SampleInst** (rb3 `50177cce`, `1c5c187c`): engine DC3
  `SampleInst_Native.cpp` is API-excluded for rb3, so `SynthSample::NewInst` was
  a weak no-op → all one-shots silent. Added
  `native/src/rb3_sampleinst_native.cpp` (`RB3SampleInstNative`). **XMA
  resolved** via SIDECAR (offline conversion, no WASM ffmpeg):
  `native/tools/xma_repack/rb3-xma-convert` decodes kXMA → PCM sidecar keyed by
  FNV-1a content-hash; runtime glue in `rb3_xma_sidecar.h`. Regenerate:
  `scripts/assets/convert_xma_banks.sh` → `orig-assets/derived/sfx_pcm/`
  (gitignored, 180MB). Runtime `RB3_SFX_PCM_DIR` or `<RB3_DATA>/sfx/gen/xma_pcm/`.
- **Native ALSA `poll` crash** (rb3 `23a7a486`): a global `Symbol poll("poll")`
  (Symbols4.cpp:861, unmangled ELF symbol `poll`) made the static linker bind
  miniaudio's libc `poll()` to that BSS data object → SIGSEGV at `__bss_start`
  the instant `ma_device_start` fired (ALSA-only, deterministic; null backend
  masked it). Fix = `#ifndef HX_NATIVE` guard (joins
  `time`/`select`/`random`/`pause`/`on_exit`). WATCH: unguarded libc-name Symbol
  collisions still in the table (`read`/`write`/`open`/`close`/`index`/`system`).

**Headless audio testing:** the engine null-backend WAV path —
`MILO_AUDIO=1 MILO_AUDIO_BACKEND=null DC3_DUMP_AUDIO=/tmp/x.wav
DC3_DUMP_SECONDS=N` + `RB3_HTTP=1` (unbounded frame loop). GOTCHA: a
`timeout`/SIGTERM kill skips `AudioDevice::Terminate` → WAV header unfinalized →
reads empty; analyze raw (skip 44-byte header). SAB-ring non-zero (real
AudioWorklet output) is the reliable proof, NOT the capture buffer. Docs:
`docs/native/audio-perf-loop/` (STATE.md + wave-NN.md). cf audio-verify skill.

---

## Song-start content skip ("audio ~9s early vs notes")

FIXED 2026-07-02, rb3 `5c5eb8a8`, native+web. **Root cause:** pre-Play
fast-accept prime refunded ring free space → the Vorbis decoder raced a 2nd lap
over the 16-chunk (~9.1s) stream ring during load/count-in, overwriting the song
start; Play began at the oldest resident ≈ song[+9.1s]. **Fix** (HX_NATIVE,
Wii-neutral): (1) `StreamReceiver::BytesWriteable` caps TOTAL pre-Play decode at
one ring lap (`mTotalWrittenEver`, opt-out `RB3_STREAM_PREPLAY_CAP_OFF=1`); (2)
`RB3StreamReceiverNative::PlayImpl` resurrects the freed-but-resident lap
(deterministic guard `totalWritten==ringSize && writtenSpace==0 && !mEndData`).
Doc: `docs/native/songstart-2sec-2026-06-21/CONTENT_SKIP_DIAGNOSIS.md`.

**Measurement gotchas:** strongest proof = byte-level `RB3_STREAM_AUDIO_DBG=1`
RINGHEAD vs `decode_reference.py`. `decode_reference` WAVs are FLOAT with peak>1
— a "peak>2 ⇒ /32768" loader heuristic reads them as SILENCE (cost hours). Branch
on dtype. `audio_verify --section full` envelope alignment fails on quiet
self-similar song INTROS — use band-limited 200–2000Hz sweeps, or A/B two game
captures at lag 0.

---

## Songlib web crash on song_select (char-preview UAF)

2026-06-09. "Song library won't load / hangs on black" entering song_select =
a CPU **use-after-free**: `PropSync<RndTex>` `dynamic_cast` on a DANGLING freed
`Hmx::Object*`, driven by the default-on char-preview menu material on
`MusicLibrary::OnEnter`. Only crashes with BOTH `RB3_GUEST_PROFILE` +
`RB3_CHAR_PREVIEW` on. Trigger commit `9318bb73`.

- **Fix:** `3d00d1dd` (BandPatchMesh LP64 `offsetof`) + `65f7f0e6`
  (GetPictureTex null on native) → char preview re-enabled default-ON. Also
  `cc047050` (ExtendTwin fix, closes ~1/12 char-composite boot abort).
- On web it surfaces as a GPU-process SIGSEGV; native shows the direct
  in-process SIGSEGV with the readable PropSync stack. **LESSON: the GPU symptom
  layer ≠ the cause — get the native symbolized backtrace before trusting a
  web-only GPU signal.** A confidently-wrong theory (unbounded per-frame GPU
  mesh-buffer leak in `BandRnd::DrawMesh`) was REFUTED by experiment: a mesh-cache
  build (97k→624 buffers) STILL crashes; baseline leaks MORE yet doesn't.
- **BandPatchMesh OOB heap corruption** (`3d00d1dd`): LP64 struct-layout bug —
  `MeshVert` starts with a pointer, so host field offsets shift +4 vs the
  hardcoded Wii `0x32`/`0x27`/`0x38`; the face-index write scribbled the twin
  cursor → OOB `mMeshVerts[]`. Fix = HX_NATIVE `offsetof`-derived constants;
  `#else` keeps literals → Wii byte-identical. **LESSON: hardcoded Wii struct
  offset literals are LP64 landmines when a struct has a leading pointer.**
- **Separate wins:** JS-heap leak fix `b79cbafa` (`rb3_pre.js __rb3CachePut`
  kept a 2nd copy of every asset, ~187MB). **Mesh cache v2** (engine `b5309b3`,
  rb3 pin `91468cd5`): per-mesh VB/IB cache (97k→~624 buffers, ~2768MB→22MB) +
  per-(mesh, occurrence-within-frame) uniform SLOTS. `a0f98ad` alone FAILED —
  one persistent uniform buffer per mesh renders every instance with the LAST
  instance's uniforms (WebGPU WriteBuffers all run before submit). `RB3_NO_MESH_
  CACHE=1` = true revert. Tool: `scripts/analysis/visual_diff.py`; doc
  `docs/native/songlib-web-gpu-device-lost-2026-06-09.md`.

---

## Web data symbols resolve to address 0 (wasm-ld)

**Undefined DATA symbols silently land at address 0 in the rb3-web link.** The
x86-GAS `.s` stub files can't assemble under emcc; `missing_stubs.js` +
`-sERROR_ON_UNDEFINED_SYMBOLS=0` only stub undefined FUNCTIONS. So all
`.zero`-blob globals (`gInitComplete`, `TheSplasher`, Wii-only RTTI/vtables, ~61
syms) aliased address 0 → `App::App`'s `gInitComplete = false` was a 1-byte NULL
write; on ASan builds, reads of aliased pointers returned shadow-gap poison →
layout-sensitive follow-on abort in musl `pop_arg`. **wasm-ld gives NO warning
for undefined data (unlike functions)** — invisible until ASan.

**Fixed 2026-07-05** (`ad348c94`): `native/src/web_data_stubs.cpp` = weak
16-byte-aligned zero blobs, exact symbol names via `extern "C"`. **When adding a
data stub to a `.s` file for a native link error, mirror it there.** Companion
`9f2115d9` (App::App argv stack-use-after-scope, web argc==0 path). ASan
workflow: dedicated `native/build-web-asan/` dir + `-DRB3_WEB_ASAN=ON` — NEVER
set RB3_WEB_ASAN in build-web-release (sticks in CMakeCache, ships a ~3× slower
ASan release). Release ASan uses `-fno-inline` so frame #0 is the real function.

---

## Web-release heap-corruption triage (-O2 miscompiles)

Three RB3 web crashes reported 2026-07-05. Two classes: (A) emcc `-O2` per-TU
miscompiles (release-only), (B) plain NULL/logic bugs (repro on native too).

- **SONG-END FREEZE — FIXED (`77304bba`):** emcc -O2 miscompile of
  `MetaPerformer`'s ctor — member-construction writes never land, so
  `mPendingData.stats.mSoloStats` reads recycled-heap garbage; song-end copies
  in, the vector dtor walks a null-begin/garbage-end range → freeze. Fix = pin
  `MetaPerformer.cpp` -O0 (3rd such pin next to OutfitConfig.cpp — see below —
  via `set_source_files_properties(... COMPILE_OPTIONS "-O0")`). The repeating
  "vertex/texture" pattern in the corrupt block was the FREED previous occupant,
  NOT an active stomp (mis-attribution caught by verify-first).
- **SPACEBAR/"FILTER" CRASH — FIXED (`cb15ac8a`):** plain NULL deref
  REPRODUCIBLE ON NATIVE (not -O2). `UIList::SetSelected` (UIList.cpp:278)
  derefs `mListState.Provider()` unconditionally; the details pane's
  `{instruments.lst set_selected $instrument}` hits it with a null provider after
  details→part_difficulty→back→reopen. Fix = HX_NATIVE null-provider guard.
  GOTCHA: on web the SORT/FILTER panel is UNREACHABLE (`native/dta/config/
  joypad.dta` maps kPad_Select→kAction_Option, not kAction_ViewModify).
- **EXIT-TRAP — OPEN (`77304bba` known remaining):** continuing PAST the score
  screen (coop_endgame → meta_loading_continue → song_select) traps `unreachable`,
  web-release only. Same -O2 class. Fix hatch READY:
  `cmake -DRB3_WEB_O0_GLOB="<repo>/src/band3/game/*.cpp"` then narrow to the
  culprit + permanent pin. Candidates ranked: Game.cpp #1.

**WEB TEST ENV (shared box):** GPUs VRAM-full from vLLM → use SOFTWARE WebGPU
(`--enable-unsafe-swiftshader --use-angle=swiftshader --disable-gpu`). Heavy CPU
contention (concurrent clang++/wibo, load 25-51) starves async screen-load →
boot "wedges" (NOT a build bug). Private iso server needs `boot-assets.manifest`
(+ `screen-*.manifest`) next to server.py or `/api/bundle/boot` returns 0 files.
Tools: `scripts/web/_swrender-boot.mjs`, `_songend-sw.mjs`, `_fullboot.mjs`,
`scripts/native/_filter-crash-test.py`, `_exit-trap-test.py`.

**The three -O0 per-TU pins (emcc -O2 miscompiles):** `OutfitConfig.cpp`
(corrupt outfit-mesh GPU buffer — see O2 rollout below), `MetaPerformer.cpp`
(song-end), and the pending exit-trap culprit.

---

## Load-perf findings (boot/transition stutters)

Full writeup: `docs/native/web-loadperf-findings-2026-06-03.md`;
network-conditioned suite `docs/native/web-netperf-findings-2026-06-08.md`.

**Root cause (measured):** every on-demand asset miss = a synchronous
`WebAssetsFetchSync` (`xhr.open(...,false)`) that FREEZES the wasm main thread
for the whole transfer; freeze = bytes÷throughput (invisible on loopback, brutal
over a real network). RB3 uses its OWN `native/src/native_file.cpp`
`NativeStdioFile` (EXCLUDES shared-engine File_Web.cpp — DC3-shaped);
`ReadAsync`=`fread` from MEMFS, no JSPI suspend per read. So the App ctor's
~12.1s was 70.9% IDLE — 63.9MB of boot assets fetched as ~98 individual
synchronous XHRs, since `/api/bundle` only carried 7.8MB of `.dta/.dtb`
(`BUNDLE_EXTS`). Named offenders: `20thcenturyboy.mogg` 37MB→6.76s freeze,
`colorpalettes.milo_xbox` 21MB→3.7s, `small_club_01` 19MB→3.5s. Worst
transition = part_difficulty→game (11.4s blocked / 6.7s single dead frame @
50Mbit). **"Bandwidth is fine; synchrony is the bug."** GPU-independent
(SwiftShader and RTX 3090 both = 12.3s App ctor); NOT shader compile / wasm
size (that only affects the pre-fetch cold-compile of the 28MB -O0 -g2 dev
wasm). Native cross-check: rb3-native (sync I/O) boots in ~5.2s.

Tooling (use, don't guess): `scripts/web/loadperf-profile.mjs` (CDP CPU profile +
Long Tasks + RAF gaps + boot-phase milestones), `analyze-cpuprofile.mjs`,
`scripts/web/netperf-suite.mjs` (CDP `Network.emulateNetworkConditions`
throttling that DOES bite localhost; low=50Mbit/normal=200Mbit/local=unbounded),
`_netmatrix.mjs`. main_web.cpp `BootMark()` → `performance.mark('rb3:boot:*)` +
`window.rb3BootPhaseLog`; `ApplyUrlLoaderEnv()` plumbs `?loaderYieldMs=`/
`?loaderBudgetMs=`/`?frameInstrument=` → setenv.

**Wave 0 (2026-06-09):** R5 (server.py on-demand brotli/gzip + atomic disk cache
for /api/file, engine `076e8146` / rb3 `6e627201`) = PURE WIN (bytes −27..32%).
R3 (boot-critical `.milo_xbox` as one async `/api/bundle/boot` before `new
App()`, rb3 `150a33f8`) = BANDWIDTH TRADEOFF (helps 50Mbit, regresses localhost).
KEY LESSON: **independent re-verification ESSENTIAL** — the impl agent's claimed
numbers didn't survive a fresh rebuild+netperf (bundle shipped 60MB RAW,
bypassing R5 compression; fixed → 13.75MB br). R3↔R5 must integrate (bundle must
be compressed too).

**Intro-movie SFX prefetch (`46a59614`, 2026-07-02):** "sound effects take 10+s
after enter" = 321 individual xma_pcm .ogg sidecar XHRs after splash ENTER. Fix:
`native/web/screen-shell_sfx.manifest` (321 oggs as ONE `/api/bundle/screen/
shell_sfx`) fired at intro_movie_screen entry. @50Mbit: SFX XHRs 330→0,
splash→hub 3.9→2.1s (−47%). OPEN: SFX bank milos (guitar_fx/kit03/tambourine)
EXCLUDED — one run with them pre-resident hung splash→main_hub in a suspended
PollUntilLoaded (flaky loader-ordering race when instant-MEMFS opens interleave
with slow network loads).

**Roadmap CLOSED / SUPERSEDED (2026-07-01):** the whole web-perf-roadmap R1-R6 is
SHIPPED — not by this thread's Wave 0/1 plan but by the parallel incremental-load
effort (below). **DO NOT re-open R1-R6.** Re-baseline @50Mbit: boot→hub
15500→35ms, hub→select 5900ms→0ms, part_diff→game 11400ms→0ms/67ms hitch. Roadmap
docs `docs/native/web-perf-roadmap/` marked SUPERSEDED.

**Frame-stall attribution (2026-06-20):** #1 stall was NOT engine —
`native/web/index.html` `log()` appended a `<div>` + read scrollHeight per
console line → forced full-doc relayout (engine spams 100s NOTIFY/frame,
#console grew to 17,817 nodes; 3.72s Layout = 37% of main-thread). Fix
(rAF-batch + 200-line cap, page-chrome only): 3.72s→0.17s Layout, longtask stall
1176ms→173ms. Doc: `docs/native/frame-stall-2026-06-20/`.

**DC3 web-perf parity port (dc3 `8af3c71f`, 2026-07-02):** ported rb3's approach
in ONE commit → @50Mbit 178.6→~52MB (−70%), ~49→~27s (−45%). Also DC3 SFX ogg
sidecars + boot bundle (`9f6f777a`). Tool `dc3-decomp/scripts/web/
netperf-lite.mjs`.

---

## Web-perf handoffs (Opus-verified implementation specs)

2026-06-09: `docs/native/WEB_PERF_ROADMAP.md` (measured baseline + P0–P4) and
`docs/native/web-perf-handoffs/` (8 per-item specs, each deep-audited by Explore
then adversarially re-verified by Opus subagents with file:line quote-checks).
User rule: don't trust Explore-only findings for planning.

IMPLEMENTED 2026-06-09 (ultracode): **App-ctor 12.45s→5.36s** (rb3 `55c2d8e0` —
real lever was `PollUntilLoaded`'s UN-throttled per-slice `emscripten_sleep`,
~2600 yields ≈11s; throttled via `RB3_LOADER_MIN_YIELD_MS=16`); audio
low-water+soft-pressure law (engine `53fb203`+`5ac9501`); sTexGpu leak fixed
(engine `d8c1ec6`+rb3 `aea5a22a`); venue cull (engine `f8544a5`+rb3 `1f09fce2`,
default-OFF `RB3_VENUE_FRUSTUM_CULL`, −37% world.cam meshes); prewarm (rb3
`ad3b7e61`+UAF fix `585ad0f8`).

**-O>0 boot crash ROOT-CAUSED:** inline-in-.cpp cross-TU defs discarded at -O>0 →
silent env-import stubs (`ERROR_ON_UNDEFINED_SYMBOLS=0`) → SystemConfig NULL; fix
`-fno-inline` SHIPPED (rb3 `64b8a238`). **O2 SHIPPED (rb3 `735fff11`,
2026-06-10):** default O0→O2. 2nd blocker (render GPU-process crash) isolated by
~15-build per-TU binary search to ONE TU: `src/system/bandobj/OutfitConfig.cpp`
(miscompiles outfit-mesh deform/AO data → corrupt GPU buffer); fix = pin that TU
-O0, rest O2 (harness `RB3_WEB_O0_GLOB` cache var + `/tmp/fast-deploy.sh`).
**Sizes: raw 16.7M→6.1M (−62%), brotli 2.39M→1.51M (−37%).** Mesh-cache v2 ≈ 2.3×
native p50 — likely THE web-25fps fix.

Verified status deltas that superseded the roadmap: mesh-cache v2 DONE (engine
`b5309b3`, rb3 `91468cd5`); prewarm spec flaw (`UIPanel::Load` always `new
DirLoader`, never `DirLoader::Find` — needs an HX_NATIVE adoption hook);
`CompareSphereToWorld` returns true ⇒ FULLY OUTSIDE ⇒ cull.

---

## Incremental-load perf (transitions, preview hover, frame drops)

2026-06-10, rb3 `bb8b1871`: 10-agent ultracode investigation. Docs:
`docs/native/incremental-load-perf/PLAN.md` + `research/01..14`.

**Diagnosis (measured):** Milo's loader is ALREADY async; the native port
collapsed it at ONE seam — `NativeStdioFile` ctor does a blocking sync XHR on
MEMFS miss (`native_file.cpp:258` → `WebAssets.cpp:339` `xhr.open(...,false)`) and
`ReadAsync` reads inline ⇒ the ReadDone/TempEof machinery is dormant. Native
`lpu=0.0ms` everywhere ⇒ the 17 PollUntilLoaded sites are free with resident
bytes — **fix the File seam, NOT per-site conversion.** Preview hover =
`SongPreview::PrepareSong` → `TheSynth->NewStream` → `NewFile` sync-XHR of the
FULL 32-37MB mogg; ~13-15s @20Mbps.

**Waves 0-5 SHIPPED (2026-06-10/11):** WebPendingFile async open + `/api/manifest`
size/404 oracle (rb3 `79cb7a54`, engine `fa89954`) + Range moggs 206 + N2 LRU
chunk cache; per-screen bundles `/api/bundle/screen/<name>` (rb3 `bc651674`,
hub→select fileReqs 20→1); Q10 prewarm default-ON; A5 pipeline pre-warm (engine
`a0848b1`, venue-build frame 165→82ms); N1 loader read-ahead
(`RB3_LOADER_READAHEAD=6`); SFX vorbis sidecars (rb3 `e5728fc0`, 59MB PCM→8.5MB
ogg, 10× lever); A4 texture-downscale. Flags all default-on, A/B'd:
`RB3_ASYNC_OPEN_OFF`, `RB3_MOGG_RANGE_OFF`, `RB3_BC_TEX_OFF`,
`RB3_PREVIEW_PREFETCH_OFF`, `RB3_LOADER_READAHEAD`, `RB3_MOGG_CACHE_MB=24`. E3
throttled 20Mbps: cold preview hover frozen 0ms (was 7.5-12s whole-mogg XHR).

**Network matrix lesson:** the canvas NEVER freezes at any bandwidth — remote
"hangs" = WALL-CLOCK serial-fetch stalls (~85MB milos download 100% serially).
20Mbps/40ms gate is provably blind to this class — **gate perf at 8Mbps/80ms +
4Mbps/150ms.** At 4-8Mbps the pipe is SATURATED (wall ≈ totalBytes/bandwidth);
parallelism kills RTT gaps but can't beat throughput → the only remaining lever
is BYTES (A4).

**A4 texture-downscale SHIPPED default-ON (2026-06-23, `RB3_WEB_DOWNSCALE`):**
1.5Mbps/300ms DNF→REACHES game_screen @574.5s. Tool `scripts/milo/mip_strip.py`
(Python byte-surgery — engine has NO ObjectDir save path,
`DirLoader::SaveObjects`=`MILO_ASSERT(0)`). Strip = drop top mip. Visual gate =
PASS-WITH-EXCLUSIONS (SSIM 0.875): EXCLUDE BC5/DXN normals, BC3-alpha, textures
≤256². Wii byte-identical. **First-frame FIXED** (framestall venue-prewarm,
`234e3c57`/`86312dcf`/`e5c854e5`: W5 warm was a no-op because
`BandDirector::EnterVenue` IGNORES `mVenue` and force-loads `small_club_01`); web
reveal 410→96ms. **L1 vertex-unpack cache CONFIRMED** (native p50 10.96→5.49ms).
sticky-q11 = phantom (server `_encoded_cache_valid` keys size+mtime only, never
clobbers a valid q11).

**Progressive sharpen SHIPPED default-ON (2026-07-02, `RB3_PROGRESSIVE_SHARPEN`):**
A4-stripped venue loads fast → in-session background sidecar fetch → live
per-texture recreate at full res. `mip_strip.py sharpen` emits SHRP-v1 sidecar
(verbatim removed top-mip BC bytes, keyed by engine-replicated TexFingerprint of
the STRIPPED base); engine `RB3TexSharpen.cpp` matches loaded RndTex by recomputed
fingerprint, swaps bitmap, re-uploads. Byte-EXACT restore. Flags
`RB3_SHARPEN_PER_FRAME=4`, `RB3_SHARPEN_DBG`. Follow-up (research/14): strict
yield-to-mogg chunking MEASURED WORSE → shipped OPT-IN
(`RB3_SHARPEN_CHUNK_KB` default=0). Bugs caught en route: `ReadWholeFile`
FILE_OPEN_NOARK lacked READ bit → NativeStdioFile opened "wb" and TRUNCATED the
sidecar; `rb3_texsharpen_native.cpp` missing from `RB3_WEB_NATIVE_GLUE` = silent
web no-op (`ERROR_ON_UNDEFINED_SYMBOLS=0` hides it — check BOTH source lists).

**GOTCHA — "stuck on song load (web)" report (2026-06-11) = STALE DEPLOYED
BUILD, no code bug.** `scripts/web/build.sh` rebuild fixed it. TRIAGE GOTCHA: the
song intro cinematic runs up to ~25s at songMs=0 inside game_screen with
venue/crowd close-up cams; crowd/extras skin-fling shards are VISIBLE there on
both native and web (pre-existing). Don't screenshot at +4s and call gameplay
broken — wait for songMs>0/track slide-in. cf web-perf-handoffs.

---

## Intro cinematic on web (video overlay shim)

Plays on web as of rb3 `bc897ab8`. RB3 has no native Bink decoder, so the matched
`Movie` (`src/system/movie/Movie.cpp`) routes Begin/Poll/Ready/End/IsOpen/
SetPaused to `native/src/rb3_movie_native.cpp` under `#ifdef HX_NATIVE`
(byte-identical `#else` keeps Wii unaffected).

- **Web** (`__EMSCRIPTEN__`): a hardware-decoded `<video>` element appended to
  `#canvas-container` as a fullscreen overlay (z-index max) over the WebGPU
  canvas. The `.bik` name is rewritten to a pre-transcoded `.webm` (VP9+Opus)
  streamed from `/api/file/videos/...webm` (server.py already serves `.webm` +
  HTTP range → zero server change). No GPU upload — the browser composites.
- **Native desktop:** skips by default; `RB3_INTRO_SECS=<n>` virtual-plays n
  seconds for headless flow tests.
- Gated to fullscreen cinematics by basename (rb3_intro_cinematic /
  rb3_end_credits). Transcode (gitignored):
  `scripts/web/transcode_videos.py`. Skip via Start/Confirm.
- Pre-existing noise (NOT from this): `SongSort.cpp:51 "access item 0 in list of
  0"` spams while intro holds (songs.dta still loading) — non-fatal.

---

## DC3 song-start audio / UI hack cleanup (DC3-specific)

DC3 `0bd29e7b` + root-cause `9512b895`, 2026-07-02. User report: missing .ogg
during song load + glitchy audio + menu footsteps under the song.

1. **Root cause — menu ambience under the song:** Xbox silences menu audio at
   song start via `Game::Restart` → `StopAllSfx` + `StopAllSounds` (faithful).
   The native-only "synthetic panel entry" block in `PanelDir::Enter` (from the
   2026-03 UI unwind, `c6f4f7f8`) blanket-activates startMode==0 flows via a
   recursive `ObjDirItr<Flow>` walk — and every panel lists the shared sound
   banks as subdirs, so each panel enter re-activated the whole sound-flow
   universe (`titlescreen_amb.flow` re-fired AFTER the faithful stop). Fix
   `9512b895`: skip flows under `sfx/` in the blanket walk. `d5b46da3` then
   defaulted the whole blanket flow activation OFF
   (`MILO_NATIVE_FLOW_FILTER=none`; curated/all/menu_only = escape hatch).
2. **"sample mismatch"** — native `VorbisReader::Poll` passed
   `startSamp = granulepos - pcmAvail`; granulepos resets on every loop-wrap.
   **Xbox reader passes -1** — matched it.
3. **289 .ogg 404s** — native run wrote sidecars to cwd-relative
   `sfx/gen/xma_pcm` while server serves `orig-assets/extracted`; durable fix =
   `SidecarDir()` anchors at `NativeGetDataDir()`.
4. `7c0c170b` removed BOTH MetaMusic hacks.

**RB3 follow-up leads — BOTH CLOSED, no changes needed:** (1) rb3
`VorbisReader::Poll` granulepos is NOT a bug — Wii-faithful AND structurally
sound; DC3 needed -1 because its Xbox-era StandardStream contract differs — **do
NOT port -1 to rb3.** (2) rb3-native has NO blanket flow-activation analog (no
Flow system in RB3 — a DC3-era subsystem). Tooling: `DC3_AUDIO_TRACE=1`
attribution probes (reach in-browser via `?env=DC3_AUDIO_TRACE=1`).

---

## DC3 render bridge study (semi-active)

2026-07-12 (ultracode DAG, committed `11d1ad1b`). Hub:
`docs/native/dc3-render-bridge-2026-07-12/` (CHARTER/PLAN/SPEC/README + recon/ +
review/).

**Verdict: NO BRIDGE — the premise dissolves.** RB3-native already renders
through the SAME shared milo-native-engine WebGPU core as DC3; the delta is only
the per-game backend *flavor* (BandRnd vs WgpuRnd + 6 rndobj-coupled gfx pass
TUs that read DC3's RndMat/RndCam/RndMesh::Vert shapes, per
`native/CMakeLists.txt:65-70`). The defect-ledger audit found ZERO open defect
families needing the DC3 backend. Option A (scene adapter) NO-GO on
one-binary/one-rndobj-fork grounds; Option B (flavor flip) NO-GO on
RndMesh::Vert wire-format + shipped-fix regression grounds.

**Adopted: Option C (as amended)** — a small `SceneView` accessor seam makes the
DC3-flavor pass TUs rndobj-shape-neutral so ONE implementation compiles against
either fork; RB3 adopts converged TUs behind default-OFF flags only on chartered
need; retires `RB3*` twin TUs (~2k LOC). Framing is explicitly
**code-health/capability-headroom, NOT a defect campaign** — visual-defect effort
stays on class-(c) families (perf-clip, HUD-glyphs, patch-mesh).

**Next action (not yet chartered):** SPIKE-TQ + C2b, one bounded wave (PLAN.md
§3/§4): C1 minimal SceneView slice + C2 port `TransparentQueue.cpp` into the rb3
flavor behind `RB3_TRANSPARENT_SORT` default-OFF (matched-frame A/B; MANDATORY
web smoke — the rb3 flavor has never compiled a Tier-2 pass TU under Emscripten)
+ C2b parallel paper-audit. C0 three-repo pin reconciliation is a LANDING gate
only. Spike failure → "keep the twins, document why", sunk cost one wave.
