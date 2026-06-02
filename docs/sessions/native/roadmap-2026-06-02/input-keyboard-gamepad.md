# Keyboard / Gamepad Gameplay Input for the RB3 Native + Web Port

**One-line verdict:** Live controller input is dead on RB3-native — `JoypadPoll()`/`JoypadInit()` are weak **no-op stubs** (`native/src/dta_link_stubs.s:90-97`) because the engine's working `Joypad_Native.cpp` is excluded from the build, so today the only input is synthetic `ButtonDownMsg` injection into `TheUI` (menu-only) plus a hand-built part-select macro in `native/src/rb3_game_input.cpp`; gameplay frets/strum/whammy never fire because nothing populates `JoypadData::mButtons` or calls `SendButtonMessages`. The fix is to provide a real RB3 `JoypadPoll()` (glue layer) that drives the **already-compiled** common `Joypad.cpp` message path, which both the menu and the gameplay `GuitarController` consume unchanged.

---

## 1. Current state — what works vs what's stubbed/missing

### The engine's gameplay input architecture (all compiled into RB3-native today)
The platform-agnostic input spine is fully present and clang-clean:

- **`src/system/os/Joypad.cpp`** — the common joypad core. Defines `JoypadData gJoypadData[4]`, `JoypadGetPadData`, `JoypadInitCommon`, `JoypadSubscribe`/`JoypadUnsubscribe`, `JoypadPushThroughMsg`, `ButtonToAction`, `ButtonToVelocityBucket`, and — critically — **`SendButtonMessages(int pad, unsigned int btns)`** (`Joypad.cpp:321`). `SendButtonMessages` diffs `btns` against `mButtons`, fills `mNewPressed`/`mNewReleased`, runs the drum-cymbal-mask + velocity-bucket logic, and broadcasts `ButtonDownMsg`/`ButtonUpMsg` through `gJoypadMsgSource` (`JoypadSendMsg`, `Joypad.cpp:315`) to every subscribed sink. **This function is the single chokepoint that feeds both menu and gameplay.** It is non-static (callable from glue).
- **Menu input path:** `gJoypadMsgSource` → focused `UIScreen`/`UIPanel`/`UIButton` → `UIComponentSelectMsg`. (Today's synthetic injection mimics the tail of this via `TheUI.Handle(ButtonDownMsg)` in `rb3_game_input.cpp:633-642 ExecButton`.)
- **Gameplay input path:** `src/system/beatmatch/BeatMatchController.cpp:21 NewController` builds a `GuitarController` / `JoypadController` / `KeyboardController` / etc. per the song part; each calls `JoypadSubscribe(this)` in its ctor and implements `OnMsg(ButtonDownMsg)` / `OnMsg(ButtonUpMsg)`:
  - **`GuitarController`** (`GuitarController.cpp`) — 5-lane plastic guitar. Frets = slot buttons → `FretButtonDown/Up` + `NonStrumSwing`; strum = `mStrumBarButtons` (default `kPad_DUp`/`kPad_DDown`) → `Swing(...)`; star power = `mMercuryButton`; **whammy = analog**, polled in `GuitarController::GetWhammyBar()` (`GuitarController.cpp:88`) from `JoypadData::GetLY()`/`GetRX()` per the `ly_whammy`/`negative_rx_whammy_val` controller cfg. `GuitarController::Poll()` (`GuitarController.cpp:111`) is driven every frame by `BeatMatcher::Poll` (`BeatMatcher.cpp:97 mController->Poll()`).
  - **`JoypadController`** — gamepad-as-instrument (drums/joypad guitar); reads `GetJoypadData()` for whammy/velocity/cymbal.
- **`GemPlayer`** (`src/band3/game/GemPlayer.cpp`) consumes the controller's sink callbacks — it never reads `JoypadData` directly. `FretButtonDown` (`GemPlayer.cpp:744`), `FilteredWhammyBar` (`GemPlayer.cpp:828`), strum `Swing`. So **wiring the controller is sufficient**; no GemPlayer changes needed.
- **`JoypadData`** layout (`src/system/os/Joypad.h:213`): `mButtons` (0x0), `mNewPressed`, `mNewReleased`, `mSticks[2][2]` (whammy reads `GetLY`/`GetRX`), `mTriggers[2]`, `mConnected` (0x64), `mType` (`JoypadType`), `mControllerType` (Symbol). `SetButtons()` is inline (`Joypad.h:259`). **Note RB3's `JoypadData` has NO `mNumAnalogSticks` member** (the engine `Joypad_Native.cpp` sets that — one reason it doesn't compile for RB3).

### What is actually stubbed / broken
- **`JoypadPoll()`, `JoypadReset()`, `JoypadTerminate()` are weak no-op stubs** — `native/src/dta_link_stubs.s:90-97,122-123` alias them to `__hmx_native_noop_stub`. `JoypadInit()` is likewise stubbed (`dta_link_stubs.s:88-89`). So `SystemPoll(false)` → `JoypadPoll()` (`System.cpp:673`, called from `App::RunOneFrame` `App.cpp:476`) does **nothing**: `mButtons` never changes, `SendButtonMessages` is never called, no `ButtonDownMsg`/`ButtonUpMsg` ever reach controllers.
- **The engine's good input file is excluded.** `/home/free/code/milohax/milo-native-engine/src/platform/Joypad_Native.cpp` is a complete keyboard + GLFW-gamepad + web-keydown + HTTP poll implementation (it sets `JoypadData.mButtons`, axes, and broadcasts `ButtonDownMsg`/`ButtonUpMsg` via `JoypadPushThroughMsg`). It is listed in `native/CMakeLists.txt:159 MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE` with the reason at `CMakeLists.txt:126-128`: *"RB3 TheUI is a value (not pointer), JoypadData lacks mNumAnalogSticks, no JoypadTerminateCommon / kAction_ShellOption."* **DC3 by contrast compiles it directly** (`dc3-decomp/native/CMakeLists.txt:1076`) — that is the proven working pattern we are missing.
- **Today's only input is synthetic and menu-shaped.** `native/src/rb3_game_input.cpp` injects `ButtonDownMsg` straight into `TheUI` (`ExecButton`, line 633), drives DTA messages, and for gameplay it cheats: a web "Confirm on part_difficulty" arms a macro (`track:guitar` → `end_override_flow` → `nofail` → `autohit`, `WebDrivePartSelect`, lines 704-750) that uses **`Player::SetAutoplay`** so the engine auto-hits gems. **There is no human gameplay input at all** — you cannot strum a note.
- **Web keyboard** exists for the menu only: `rb3_game_input.cpp:121 InitWebInput` installs JS keydown/keyup into `window._rb3Keys`, drained in `RB3GameInputPoll` (`rb3_game_input.cpp:1022-1090`) → `ExecButton` (menu) — it never touches `JoypadData` or gameplay controllers.
- **Desktop keyboard:** none. There is no `glfwGetKey` loop in any RB3 path. The GLFW window exists (`GpuDevice::Window()`, `milo-native-engine/src/gfx/GpuDevice.h:61`; created at `GpuDevice.cpp:193`) and `glfwPollEvents()` runs (`GpuDevice.cpp:422`), but nobody reads keys from it. `gNativeWindow` (the symbol `Joypad_Native.cpp` keys off) is only defined by the DC3 `Rnd_Wgpu.cpp` backend, which is OFF for RB3 (`MILO_ENGINE_GPU_BACKEND=rb3`).
- **Gamepad:** GLFW is linked (`CMakeLists.txt:190-191`), so `glfwGetGamepadState` / `glfwJoystickIsGamepad` are available, but unused by RB3.

### Why the synth user already half-works
`SynthUser()` (`rb3_game_input.cpp:444`) already does the user/pad plumbing the real path needs: binds the first `LocalBandUser` to pad 0 (`AssociateUserAndPad`), calls `JoypadInitCommon(SystemConfig("joypad"))` (builds `gControllersCfg`/`gButtonMeanings`), sets `pad0->mConnected = true`, and `DebugSetControllerTypeOverride(kControllerGuitar)`. This is exactly the state a real `JoypadPoll` needs; the new poll can reuse it.

---

## 2. Goal — desired/retail behavior

A player with **no plastic instrument** plays guitar/bass/keys (5-lane) using the **computer keyboard**, and optionally a **USB gamepad**, on both desktop (GLFW) and web (browser). Concretely:

- **Menu nav** continues to work (already does, via synthetic injection — must keep working through the new path).
- **Gameplay:** 5 fret keys, a strum key (up + down), a whammy control, a star-power/overdrive activate, pause. Pressing a fret + strumming on a gem hits it; whammy raises overdrive while sustaining; star power deploys.
- Frame-accurate enough to actually play on Easy/Medium (the engine's strike window does the timing; we just need clean down/up edges and a stable held-fret state).
- The existing headless/scripted/HTTP injection paths (used by the test harness, `RB3_GAME_INPUT`, `/api/input`) keep working unchanged.

Default keymap (proposed, GuitarController 5-lane):

| Action | Desktop key | Web key | JoypadButton | Notes |
|---|---|---|---|---|
| Fret Green (slot 0) | `1` / `A` | same | per `slots` cfg (face buttons) | |
| Fret Red (slot 1) | `2` / `S` | same | | |
| Fret Yellow (slot 2) | `3` / `D` | same | | |
| Fret Blue (slot 3) | `4` / `F` | same | | |
| Fret Orange (slot 4) | `5` / `G` | same | | |
| Strum Up | `Up` / `J` | `ArrowUp` | `kPad_DUp` | default `mStrumBarButtons[0]` |
| Strum Down | `Down` / `K` | `ArrowDown` | `kPad_DDown` | default `mStrumBarButtons[1]` |
| Whammy | hold `Space` | hold `Space` | analog `mSticks` LY | set `GetLY()` while held |
| Star Power / Overdrive | `Enter` (in-song) | `Enter` | `mMercuryButton` (`kPad_L2`/tilt) | |
| Pause | `Esc` | `Escape` | `kPad_Start` | reuse menu Start |
| Menu nav | arrows + Enter/Esc/Tab | same | unchanged | keep current mapping in menus |

(Fret keys must map to the JoypadButtons the controller's `slots` cfg uses for `guitar`. Step 3 includes a one-time dump of that cfg so the keymap targets the exact buttons; the menu's `kPad_X/Circle/Start/DUp/...` mapping stays for shell screens.)

---

## 3. Proposed approach — phased, concrete, layer-tagged

The whole win is: **provide a real `JoypadPoll()` for RB3-native** that sets `JoypadData` and calls the already-compiled `SendButtonMessages`. Prefer the per-decomp glue layer (c). No matched-fork (a) edits. One small engine (b) edit is optional and avoidable.

### Phase 1 — Desktop keyboard drives the real joypad path (glue, layer c) — the core fix
Create **`native/src/rb3_joypad_native.cpp`** (new TU; add to `native/CMakeLists.txt` RB3 native sources, NOT the engine list). It supplies the strong defs that today are weak no-op stubs (a strong symbol overrides the `.weak` in `dta_link_stubs.s`, same mechanism already used for `TheSynth`, `JoypadInit` etc.):

- `void JoypadInit()` — call `JoypadInitCommon(SystemConfig("joypad"))` and `JoypadReset()` (mirror `Joypad_Native.cpp:355`). (May be a no-op if `SynthUser()` already ran `JoypadInitCommon`; guard on `gJoypadLibInitialized` indirectly by only init-ing once.)
- `void JoypadReset()` — `ResetAllUsersPads()`, then mark pad 0 `mConnected=true`. (Do **not** hardcode `mType`/`mControllerType` here — let `SynthUser()`/the overshell join flow set the guitar type; see Phase 3.)
- `void JoypadTerminate()` — no-op (RB3 has no `JoypadTerminateCommon`).
- **`void JoypadPoll()`** — the heart:
  1. `JoypadData *d = JoypadGetPadData(0); if (!d->mConnected) return;`
  2. Read keyboard into a `unsigned int btns` bitmask. Desktop: `GLFWwindow *w = gBandRnd.Gpu().Window();` then `glfwGetKey(w, GLFW_KEY_*)` per the keymap (gate on `w != nullptr` for headless). Include `platform/Rnd_Wgpu_RB3.h` for `gBandRnd`. (`glfwPollEvents` already runs in the GPU frame, so key state is fresh.)
  3. Set whammy: `d->mSticks[0][1] = whammyHeld ? -1.0f : 0.0f;` (LY; `GetWhammyBar` does `min(0, GetLY())`).
  4. Call **`SendButtonMessages(0, btns)`** — this is the entire engine broadcast (frets/strum/menu all flow from here). Do **not** also inject into `TheUI` for buttons that `SendButtonMessages` already routes, or you double-fire.
- Call site: `JoypadPoll()` is already invoked by `SystemPoll(false)` (`System.cpp:673`) inside `App::RunOneFrame` — **no App edit needed**; defining the strong symbol is enough.

This single phase makes **menu nav AND gameplay frets/strum/whammy** work on desktop with the keyboard, because the gameplay `GuitarController` is already subscribed to the same `gJoypadMsgSource` that `SendButtonMessages` broadcasts to.

### Phase 2 — Web browser keyboard drives the real joypad path (glue, layer c)
In the same `rb3_joypad_native.cpp`, `#ifdef __EMSCRIPTEN__` branch of `JoypadPoll()`: read `window._rb3Keys` (the bitmask `rb3_game_input.cpp:InitWebInput` already maintains) plus `navigator.getGamepads()` (mirror engine `Joypad_Native.cpp:59 GetWebGamepadButtons`), and feed `SendButtonMessages(0, btns)` + whammy. **Then retire the web-only menu injection in `rb3_game_input.cpp`** (the `kWebKeyMap`/`ExecButton` loop, lines 1022-1090) so input flows through ONE path — but keep the part-select crossing macro and `nofail`/`autohit` test verbs (they are test scaffolding, not player input). Reuse the existing `InitWebInput` JS or move it into the joypad TU.

### Phase 3 — Present a real guitar controller to the game (glue, layer c; verify config)
For the gameplay controller to be a 5-lane `GuitarController` (not a `joypad` gamepad-as-drums), the song-load path must see the synth user's controller type as guitar and the joypad's `mControllerType`/`mType` consistent with a guitar breed:
- `SynthUser()` already does `DebugSetControllerTypeOverride(kControllerGuitar)` and `bu->SetTrackType(guitar)` via the `track:guitar` verb. Confirm `JoypadControllerTypePadNum(0)` resolves to a guitar breed so `NewController` picks `GuitarController` (`BeatMatchController.cpp:33`, gated on `beatmatch_controller_mapping`: breed `guitar`/`ro_guitar_xbox`/... → `"guitar"`, `beatmatch.dta:25`). If `JoypadControllerTypePadNum(0)` returns `none` (no real breed set), set `d->mControllerType = Symbol("ro_guitar_xbox")` (or the breed the player config expects) in `JoypadReset`, and `d->mType = kJoypadXboxRoGuitar`. **One-time investigation:** dump the controller `cfg` `slots`/`strum_buttons`/`ly_whammy`/`controller_style` that `NewController` receives for a guitar part (add a `MILO_LOG` in a debug build, or read the player controller config DTA) so the Phase-1 fret keymap targets the exact `slots` JoypadButtons. The whammy axis (`ly_whammy` vs `negative_rx_whammy_val`) determines whether to drive `mSticks[0][1]` (LY) or `mSticks[1][0]` (RX) — `GetWhammyBar` (`GuitarController.cpp:96-105`) shows both.

### Phase 4 — USB gamepad (glue, layer c)
Add a real-instrument-style mapping in the desktop `JoypadPoll()` using `glfwJoystickIsGamepad(0)` + `glfwGetGamepadState` (mirror engine `Joypad_Native.cpp:413-452`): face buttons → frets, bumpers → strum or fret-shift, right stick / triggers → whammy, Start → pause, Back/Guide → star power. Web gamepad via `navigator.getGamepads()` (Phase 2). A standard Xbox/PS pad then plays "joypad guitar" (the `joypad_guitar` breed) — already supported by `JoypadGuitarController`. Optional: detect a real RB plastic guitar over USB later (out of scope for v1).

### Optional cleanup (layer c)
Once Phase 1-2 land, the synthetic `ExecButton`→`TheUI` injection in `rb3_game_input.cpp` becomes redundant for live play; keep it only for the HTTP `/api/input` test harness and the `RB3_GAME_INPUT` boot script (which feed verbs, not raw keys). No need to delete it — it routes the same messages — but document that live keys now go through `JoypadPoll`/`SendButtonMessages`.

**No matched-fork (a) edits.** **No engine (b) edits required** (we deliberately do NOT un-exclude `Joypad_Native.cpp`; we write an RB3-shaped twin in glue, because the engine file uses DC3-only API like `mNumAnalogSticks`/`JoypadTerminateCommon`/`kAction_ShellOption`/`TheUI->`). If a future refactor wants to share code, the engine file could be `#ifdef`'d for RB3 shape — but that touches shared (b) and is not worth it for v1.

---

## 4. Key files

- `native/src/rb3_joypad_native.cpp` — **NEW (layer c)**. Strong defs for `JoypadPoll`/`JoypadInit`/`JoypadReset`/`JoypadTerminate`; reads keyboard/gamepad → `SendButtonMessages(0, btns)` + whammy axis. The whole fix lives here.
- `native/src/dta_link_stubs.s:88-123` — current weak no-op stubs for `JoypadInit/Poll/Reset/Terminate`; strong defs above override them.
- `native/CMakeLists.txt:153-179` (`MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`, line 159 excludes `Joypad_Native.cpp`); add `rb3_joypad_native.cpp` to the RB3 native source list (near other `native/src/*.cpp`).
- `src/system/os/Joypad.cpp:321 SendButtonMessages` — the engine broadcast chokepoint we call. Also `JoypadInitCommon` (249), `JoypadGetPadData`, `JoypadSubscribe`, `ButtonToAction` (660). Already compiled, DO NOT edit (layer a).
- `src/system/os/Joypad.h:213 JoypadData` — fields to set (`mButtons` via `SetButtons`, `mSticks`, `mConnected`, `mControllerType`, `mType`). No `mNumAnalogSticks`.
- `src/system/beatmatch/BeatMatchController.cpp:21 NewController` — chooses the gameplay controller per breed (`beatmatch.dta` mapping).
- `src/system/beatmatch/GuitarController.cpp` — 5-lane guitar: `OnMsg(ButtonDownMsg)` (frets/strum), `GetWhammyBar` (88), `Poll` (111, mercury). The consumer of our messages.
- `native/src/rb3_game_input.cpp` — current synthetic injection + web keymap (`InitWebInput` 121, `ExecButton` 633, `kWebKeyMap` 101, `WebDrivePartSelect` 704). Keep HTTP/script verbs; web menu-key loop (1022-1090) gets superseded by Phase 2.
- `src/App.cpp:476 SystemPoll(false)` / `System.cpp:673 JoypadPoll()` — the existing per-frame call site (no edit needed).
- `milo-native-engine/src/platform/Joypad_Native.cpp` — the **reference** (DC3 working impl) to copy structure from; NOT compiled for RB3.
- `milo-native-engine/src/gfx/GpuDevice.h:61 Window()` — GLFW handle for desktop key polling, via `gBandRnd.Gpu().Window()`.
- `orig-assets/extracted/config/joypad.dta`, `.../config/beatmatch.dta:24-58` — breed → instrument mapping; controller cfg.

---

## 5. Quick wins (< 1 day) vs larger work

**Quick wins (ship in < 1 day):**
- **Phase 1 desktop keyboard** — one new glue TU + CMake line. Defining a strong `JoypadPoll` that calls `SendButtonMessages(0, glfwGetKey-bitmask)` immediately gives menu nav AND gameplay frets/strum on desktop, because every consumer is already subscribed. Highest value for least code.
- **Whammy as a held key** — one line (`mSticks[0][1] = held ? -1 : 0`).
- **Pause/star-power keys** — reuse `kPad_Start` / `mMercuryButton` in the same bitmask.

**Larger work:**
- **Phase 3 controller-config verification** — confirming `NewController` picks `GuitarController` and dumping the exact `slots`/`strum_buttons`/whammy-axis cfg so the keymap is correct. May need a small breed/`mType` set in `JoypadReset`. ~0.5-1 day incl. live testing.
- **Phase 2 web** — JS gamepad + funneling web keys through `SendButtonMessages` (vs the current `TheUI` injection) and a web rebuild cycle (slow). ~1 day.
- **Phase 4 gamepad** — GLFW/web gamepad mapping + per-controller-type tuning. ~1 day.
- **Configurable keybinds / rebinding UI** — out of scope for v1.

---

## 6. Dependencies & risks

- **Double-fire risk:** if both the new `JoypadPoll`→`SendButtonMessages` AND the old `rb3_game_input.cpp` `ExecButton`→`TheUI.Handle` run for the same key, menus get two events. Mitigation: route live keys ONLY through `JoypadPoll`; leave `rb3_game_input.cpp` for HTTP/script verbs only.
- **`JoypadInitCommon` ordering:** `SendButtonMessages` needs `gButtonMeanings`/`gControllersCfg` (built by `JoypadInitCommon`) and `gJoypadMsgSource` (also built there). `SynthUser()` already calls it lazily; ensure `JoypadInit` (or first `JoypadPoll`) calls it before the first broadcast. `gJoypadLibInitialized` guards re-init.
- **Controller type / breed:** if `mControllerType` is `none` or a non-guitar breed, `NewController` may build a `joypad`/drum controller and the fret/strum semantics differ (drum lanes, no strum). Phase 3 mitigates by pinning a guitar breed.
- **Headless:** `gBandRnd.Gpu().Window()` is null when `MILO_HEADLESS=1`; the desktop key branch must early-out so the HTTP/script harness still works. Web has no GLFW window (uses JS bitmask) — separate `#ifdef __EMSCRIPTEN__` branch.
- **Whammy axis polarity:** `GetWhammyBar` clamps to `min(0, ...)`; LY whammy wants negative-when-engaged. Verify sign against the chosen breed's cfg (`ly_whammy` vs `negative_rx_whammy_val` vs `traditional_whammy_val`, `GuitarController.cpp:98-105`).
- **Engine (b) untouched** — by writing an RB3-shaped twin in glue we avoid perturbing the shared engine (DC3/xenon). No shared-build risk.
- **No matched-fork (a) risk** — zero asm-matched edits; `JoypadPoll` etc. are not in the Wii binary's native frame path (`HX_NATIVE`-gated call site already exists).

---

## 7. Effort & priority

- **Priority: P1.** "Play with a keyboard, no plastic instrument" is the headline interactivity feature of the port; without it the demo is autoplay-only. Not P0 only because the port currently boots/renders/auto-plays a song end-to-end (the harness proves the gameplay engine works), so this is the next experience milestone rather than a blocker.
- **Effort: ~3-4 person-days total.**
  - Phase 1 (desktop keyboard, the core): ~0.5-1 day.
  - Phase 3 (guitar controller verification + breed pin): ~0.5-1 day.
  - Phase 2 (web): ~1 day (incl. slow web rebuild + browser verify).
  - Phase 4 (gamepad): ~1 day.
- **Quickest shippable slice:** Phase 1 alone, ~0.5 day, desktop keyboard guitar playable.

---

## 8. Verification plan

**Phase 1 (desktop keyboard) — windowed:**
1. Build (an implementation agent, NOT this read-only session): `cmake --build native/build-native --target rb3-native`.
2. Run windowed (needs `$DISPLAY`): `RB3_GAME=1 RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted native/build-native/rb3-native`. Drive a song to gameplay (or `RB3_GAME_INPUT="...track:guitar,...end_override_flow..."` to reach `game_screen`), then press fret keys + strum; confirm gems hit (score ticks, hit FX).
3. Headless regression: `RB3_GAME=1 RB3_HTTP=1 RB3_HTTP_PORT=9100 MILO_HEADLESS=1 RB3_DATA=... native/build-native/rb3-native` — confirm `JoypadPoll` early-outs on null window and the existing `/api/input` + `RB3_GAME_INPUT` scripts still drive the menu (no double-fire, no crash). Use the `scripts/native/song-select-capture.py` / `song-end-test.py` harness pattern.

**Message-path proof (headless, no window needed):**
- Add a temporary `MILO_LOG` in `JoypadPoll` printing `btns`, and confirm in `scripts/native/...` logs that pressing a mapped key produces `ButtonDownMsg` (the `RB3 input: injecting action ...` style log already exists for the synthetic path; the live path should show `GuitarController::OnMsg` effects — e.g. via `RB3_GAME_INPUT` autohit being replaced by real strums hitting gems).
- `/api/dta/eval` to read live gameplay state (e.g. score, combo) before/after a strum to prove a human hit registered, mirroring `scripts/native/song-end-test.py`.

**Phase 2 (web):**
- `scripts/web/build.sh`, `python3 native/web/server.py`, open `http://localhost:8421`, drive to a song, press fret/strum keys in the browser, confirm gems hit. Confirm menu nav still works through the unified path (no regression vs the current `kWebKeyMap` behavior). Use the existing W3c capture scripts as templates.

**Phase 3 (controller type):**
- One-time debug-log dump of the `cfg` `NewController` receives for a guitar part; confirm breed → `GuitarController` and the `slots`/`strum_buttons` JoypadButtons match the keymap. Verify whammy moves overdrive (hold Space during a sustain, watch the OD meter).

**Phase 4 (gamepad):**
- Plug an Xbox/PS pad; desktop `glfwJoystickIsGamepad(0)` true; map face→fret, dpad/bumper→strum; confirm gameplay. Web: `navigator.getGamepads()` populated; same checks.
