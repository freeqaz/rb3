# W3 — Interactive boot-to-song in browser

**Parent plan:** [`PLAN.md`](PLAN.md). Read it for context.
**Depends on:**
- [`W2_RENDER_MILO.md`](W2_RENDER_MILO.md) — browser renders RB3 geometry.
- **Native v1 milestone** — see
  [`rb3/docs/sessions/native/V1_ONE_SONG.md`](../../sessions/native/V1_ONE_SONG.md).
  **Hard blocker for W3e.** W3a–d *can* land with stubbed audio if native
  v1 is still in flight, but W3e (one-song integration) cannot start
  until native v1 demonstrably plays the chosen song end-to-end on Linux.
**Blocks:** W4 polish.

## Goal

One song plays end-to-end in the browser. Same v1 bar as native: audio +
gem track + scoring + visible venue + the boot-to-song screen flow
(splash → main_hub → song_select → select → loading → game). Driven by
keyboard input.

## Out of scope

- Online multiplayer (native v1 non-goal).
- MIDI / gamepad input. Keyboard only.
- HD / 60fps target. Stable 30fps is fine for v1.
- Service worker / IndexedDB caching. W4.

## Sub-phases

Each sub-phase has a discrete deliverable. **Sub-phases are mostly
sequential, not parallel** — W3a, W3b, and W3c all touch `main_web.cpp`
and `App.cpp`, so concurrent dispatch will produce merge conflicts. Run
W3a → W3b → W3c → W3d → W3e. W3d (resampling) is the only one cleanly
parallelisable with W3b, and the W0 work already made it mostly a no-op
(see W3d below).

### W3a — App boot flow under HX_WEB

Currently `main_web.cpp` boots the `RB3_BOOT=1`-equivalent minimal state
(inline SystemPreInit/SystemInit). W3a swaps that for the
`RB3_GAME=1`-equivalent — the full `App` flow the native build runs under
boot-to-song. Note: `RB3_BOOT` and `RB3_GAME` are **runtime env vars** in
`main_native.cpp`, not preprocessor symbols.

**Files:**

- `rb3/native/src/main_web.cpp`: replace the simplified init in
  `BOOT_ENGINE_INIT` with the same `App` constructor call the native
  `RB3_GAME=1` path makes (`main_native.cpp:502-585`). Per-frame: call
  `App::RunOneFrame()`.
- `rb3/src/App.cpp`: add `__EMSCRIPTEN__` / `HX_WEB` arms alongside the
  existing `HX_NATIVE` arms (~33 sites). The divergences to apply are
  documented in
  [`dc3-decomp/docs/plans/web-port/UNIFY_WITH_APP.md`](../../../../dc3-decomp/docs/plans/web-port/UNIFY_WITH_APP.md):
  - `TheRnd.PreInit()` before `SystemInit`/`MagnuInit` (not after).
  - Add `MidiParser::Init()`.
  - Add `ContextCheckerInit()`.
  - Add `DirLoader::SetPathEvalCallback()`.
  - Add sound bank load.
  - Add player provider wiring.
  - Add `ContentMgr::RefreshSynchronously()`.

  Most of these already exist in the native `RB3_GAME=1` path. The change
  is adding `#ifdef __EMSCRIPTEN__` / `#ifdef HX_WEB` siblings next to
  the existing `HX_NATIVE` gates so the web build also picks them up.

- `rb3/native/src/rb3_game_input.cpp`: today this is a synthetic-headless
  input driver. Under `HX_WEB`, the same driver can still inject events;
  it just becomes the W3b real-keyboard impl's fallback.

**Acceptance W3a:** the browser boots through splash → main_hub → song_select
(no input yet, screen flow auto-advances per `RB3_GAME_INPUT` env or its
web equivalent). Screenshot of song_select shows the song list.

### W3b — Real keyboard input

DC3's web keyboard pipeline lives in
**`dc3-decomp/native/src/platform/Joypad_Native.cpp:91-145`** (NOT
`Keyboard_Native.cpp` — that file is GLFW-only, `#ifndef __EMSCRIPTEN__`
gated). It registers a `document.addEventListener('keydown', …)` via
`EM_JS`, sets a bitmask, and the existing native joypad poll picks it
up.

**Files:**

- `rb3/native/src/rb3_game_input.cpp`: add an `HX_WEB` mode mirroring
  `Joypad_Native.cpp:91-145`. Register `document.addEventListener` via
  `EM_JS` for `keydown` / `keyup`, set a bitmask, and feed events into
  the same input pipeline the synthetic driver uses.
  - Map arrow keys + WASD + Enter + Esc to the RB3 menu actions
    (mirror the key→bitmask mapping in DC3's `Joypad_Native.cpp`).
- `rb3/native/web/index.html`: document the key bindings in the
  status panel.

**Acceptance W3b:** a human can navigate the menus in the browser via
keyboard alone, reach song_select, scroll the list, select a song.

### W3c — Audio (StreamReceiver + AudioDevice_Web + Worklet)

The engine's `AudioDevice_Web.cpp` is lifted in W0. RB3's
`rb3_synth_native.cpp` and `rb3_stream_receiver_native.cpp` need to bind
into it under HX_WEB.

**Files:**

- `rb3/native/src/rb3_synth_native.cpp`: under `HX_WEB`,
  `NativeSynth::Init` keeps its existing `AudioDevice::GetInstance().Init(44100)`
  call (verified at `rb3_synth_native.cpp:40`). The engine's AudioDevice
  constructor picks the web impl when `__EMSCRIPTEN__` is defined; no
  RB3-side change needed beyond ensuring the file is in the `rb3-web`
  source set.
- `rb3/native/src/rb3_stream_receiver_native.cpp`: under `HX_WEB`,
  same `AudioDevice::AddSource` registration as native.
- **`AudioDevice::PumpAudio()` goes inside `App::RunOneFrame()`** — DC3
  calls it between Poll and Draw (`dc3-decomp/src/App.cpp:790-792`),
  ratified in `UNIFY_WITH_APP.md` Phase 1. Add the same `#ifdef
  __EMSCRIPTEN__` arm to `rb3/src/App.cpp::App::RunOneFrame()`. Do
  **not** call it from `main_web.cpp` — that would double-pump.
- `rb3/native/CMakeLists.txt`: add `rb3_synth_native.cpp` /
  `rb3_stream_receiver_native.cpp` to the `rb3-web` source set (already
  there if W1 left them in).
- The audio worklet (`audio-worklet.js`) is copied via the engine's
  post-build step (W0). Verify it lands next to `rb3-web.js`.

**Acceptance W3c:**
- AudioContext shows "running" in DevTools.
- A test `.mogg` plays through the speakers when loaded via a dev-mode
  trigger. (The `?test-mogg=<path>` trigger isn't pre-built — define it
  in `main_web.cpp` as a query-param shortcut that bypasses the menu
  flow and feeds the path directly to `StreamReceiver`. Specify the API
  during W3c, don't invent it ad-hoc.)
- Native still plays the same `.mogg` identically (no regression).

### W3d — Sample-rate resampling (44.1 → 48kHz)

Browser AudioContext typically runs at 48kHz; RB3 source audio is 44.1kHz.
DC3 deferred this (Phase 7d). For RB3 W3, decide:

**Option A:** resample in the AudioWorklet (CPU-cheap linear interpolation
or higher quality if needed).

**Option B:** create the AudioContext at 44.1kHz (`new AudioContext({
sampleRate: 44100 })`) and let the browser resample at output.

**Status: Option B is already wired in the engine.** Engine
`AudioDevice_Web.cpp` defaults to 44100 Hz and already passes `sampleRate`
to JS `new AudioContext({ sampleRate: sampleRate })` (lines 42-55,
248-252). The work that's actually left:

- **Safari fallback.** Chromium and Firefox honour the requested
  sampleRate; Safari historically clamps to device rate (48 kHz) and
  returns an AudioContext that ignored the request. Detect this in JS
  (`audioCtx.sampleRate !== 44100`) and either log a warning + accept
  drift, or fall back to Option A (linear-interp resample in the
  worklet). Pick one when Safari support actually surfaces — RB3 v1 is
  Chrome 137+ only per PLAN.md non-goals, so this is a follow-up.
- Verify the engine's runtime sampleRate plumbing matches RB3's expected
  44100 Hz (no RB3-side hardcode of 48000 anywhere).

**Acceptance W3d:** A 44100Hz reference tone played through
AudioDevice_Web matches the same tone played natively (no pitch drift
or aliasing) on Chromium.

### W3e — One song end-to-end

This is the v1 milestone for web. Once W3a-d land:

**Steps:**

1. Procure the same `.mogg`+`.mid` pair the native v1 uses (task V1 in
   `V1_ONE_SONG.md`).
2. Bundle them into the web asset server.
3. Boot the browser, navigate via keyboard to the song, select, play.

**Acceptance:**
- Audio plays in sync with the gem track.
- Hit detection registers when keys are pressed at the right times.
- Score increments visibly.
- The song reaches the end without WASM trap or audio dropout.
- Native v1 still passes (no regression).

## Known gotchas

- **DC3's `UNIFY_WITH_APP.md` is the catalogue of bugs we'll hit.** Read
  it once before W3a — most of its lessons port directly to RB3.
- **AudioContext autoplay policy:** browsers require user interaction
  before audio plays. DC3's `keydown/click/touchstart` listener
  auto-resumes the context. Reuse the pattern.
- **PollUntilLoaded safety valve:** DC3 caps the loop at 10000
  iterations on web to prevent infinite blocking. The cap lives in
  `dc3-decomp/src/system/utl/Loader.cpp:415-427`
  (`LoadMgr::PollUntilLoaded`), not in `UIPanel.cpp` (UIPanel only calls
  it). Port the same fix to `rb3/src/system/utl/Loader.cpp` under
  `HX_WEB`.
- **HelpBarPanel null guards:** DC3's actual guards in
  `dc3-decomp/src/system/ui/HelpBarPanel.cpp:53-94` protect `mAll` and
  `TheSaveLoadMgr` under `HX_NATIVE` (not `HX_WEB`, not
  `mLeftHandNavList`). Port the same `HX_NATIVE` guards to the RB3
  equivalent.
- **`Debug::Fail()` non-fatal on web:** DC3's
  `dc3-decomp/src/system/os/Debug.cpp:175-194` returns early under
  `HX_WEB` (matches Xbox "Continue" dialog). Port to RB3.
- **WebGPU input lag:** the engine adds a 1-2 frame latency vs native.
  Acceptable for v1.
- **MOGG decryption performance: unmeasured.** tomcrypt AES/CTR runs on
  the WASM main thread. A 3-minute song is ~30MB ciphertext; the
  decrypt cost on WASM is plausibly sub-second but no benchmark exists.
  Measure on native first; if it's >500ms there, expect a frame-pump
  stall during song-load and consider moving decrypt to a worker (W4e
  territory).

## Acceptance test (full W3)

```sh
cd rb3/
bash scripts/web/build.sh
python3 native/web/server.py --port 8421 &
node scripts/web/smoke-test.mjs \
    --boot-to-song \
    --song <id> \
    --frames-to-completion 12000
```

(`--boot-to-song` is a new test mode that drives the keyboard via
Playwright through the menu flow and confirms audio + score events fire.)

Regression checks:
- DC3 web (`dc3-decomp/scripts/web/smoke-test.mjs`) passes.
- RB3 native v1 still passes (same song end-to-end on Linux).

## Suggested subagent prompt

> Execute Phase W3 of the RB3 web port plan
> (`rb3/docs/plans/web-port/W3_BOOT_TO_SONG.md`). W2 has landed: browser
> renders RB3 milos. Native v1 has landed: one song plays end-to-end on
> Linux (hard prerequisite for W3e specifically). Walk through the five
> sub-phases **sequentially** — W3a/b/c all touch `main_web.cpp` +
> `App.cpp`; parallelising will produce merge conflicts. Key
> corrections from the original draft to honour:
> - W3a uses `__EMSCRIPTEN__`/`HX_WEB` arms alongside existing
>   `HX_NATIVE` arms in `App.cpp` (there are no `#ifdef RB3_GAME` gates —
>   that's a runtime env var).
> - W3b mirrors `dc3-decomp/native/src/platform/Joypad_Native.cpp:91-145`
>   (NOT `Keyboard_Native.cpp`, which is GLFW-only) for the web keyboard
>   pipeline.
> - W3c puts `AudioDevice::PumpAudio()` **inside `App::RunOneFrame()`**
>   (matching `dc3-decomp/src/App.cpp:790-792`), not in `main_web.cpp`.
> - W3d is mostly already wired in the engine (Option B); the only work
>   left is verifying Safari behaviour (defer to follow-up if RB3 v1
>   stays Chrome-only).
> - PollUntilLoaded cap lives in `Loader.cpp`, not `UIPanel.cpp`.
> - HelpBarPanel guards are `mAll` + `TheSaveLoadMgr` under `HX_NATIVE`.
> Read `dc3-decomp/docs/plans/web-port/UNIFY_WITH_APP.md` first; most of
> its bug list applies. Acceptance: a human plays one song end-to-end in
> the browser via keyboard, audio + gems + score all functional. Do not
> break the DC3 web build (run
> `node dc3-decomp/scripts/web/smoke-test.mjs`) or RB3 native v1.
