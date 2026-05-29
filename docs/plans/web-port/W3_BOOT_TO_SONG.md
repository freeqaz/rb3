# W3 — App-driven boot to song in browser

**Parent plan:** [`PLAN.md`](PLAN.md). Read it for context.
**Depends on:**
- [`W2_RENDER_MILO.md`](W2_RENDER_MILO.md) — **DONE.** Browser renders RB3
  geometry (menu `main_hub` + gameplay `tracksystem`, byte-for-byte matching
  native) via the **static mesh-walk harness**.
- **Native v1 milestone** —
  [`rb3/docs/sessions/native/V1_ONE_SONG.md`](../../sessions/native/V1_ONE_SONG.md).
  **Hard blocker for W3c only.** As of 2026-05-29 native v1 is *in progress*
  (X6 NativeSynth/StreamReceiver bridge in flight; X7–X11 ahead). W3a and W3b
  can land with stubbed audio while native v1 finishes; **W3c (one-song
  integration) cannot start until native v1 demonstrably plays the chosen song
  end-to-end on Linux.**
**Blocks:** W4 polish.

## Goal

The real RB3 `App` boots in the browser and drives `App::RunOneFrame()` per
frame (mirroring DC3's `dc3-decomp/native/src/main_web.cpp`). That unlocks, in
dependency order: (1) interactive menu — `UIScreen`/`UIPanel` poll + draw, so
`RndText` glyph quads (menu labels) render and the menu responds to input;
(2) keyboard input → the game's input pipeline; (3) audio + one song
end-to-end (the v1 bar, same as native).

## Current starting state (read before coding)

**The web boot does NOT run the game.** It runs a static mesh-walk harness:

- `rb3/native/src/main_web.cpp` is a 7-state boot machine
  (`BOOT_INIT` → `BOOT_FETCHING` → `BOOT_ENGINE_INIT` → `BOOT_GPU_WAIT` →
  `BOOT_GPU_READY` → `BOOT_LOADING_MILO` → `BOOT_RUNNING_RENDER`). It calls
  `SystemPreInit`/`SystemInit` inline (`DoEngineInit`, `main_web.cpp:144-196`),
  registers a hand-picked subset of factories (`RegisterCommonFactories`,
  `main_web.cpp:77-100` — deliberately omits synth leaves), stands up
  `gBandRnd` directly (`StartGpuInit`/`InitGpuResources`), then per-frame calls
  `RenderFrame(sWalk)` from `rb3_render_mesh.cpp` — which walks `RndMesh`
  **only** (`RenderFrame`, `rb3_render_mesh.cpp:377-406`). It never constructs
  `App`, never polls `TheUI`, never draws `RndText`.
- `rb3/native/src/rb3_render_mesh.cpp` is the harness: `LoadMiloAndWalk` +
  `RenderFrame` (web) / `RenderToPng` + `RunRenderMesh` (native, `#ifndef
  __EMSCRIPTEN__`). Includes `LoadMiloDirYielding` (`:268-312`), the JSPI
  yielding load loop W3 reuses.
- `rb3/native/src/rb3_game_object_factories.cpp` — W2's
  `RB3RegisterGameObjectFactories()` (Dir-container factories so multi-chunk
  milos don't desync). Already in both native + web source sets.
- **Font-fetch groundwork (W2, landed):** every UI resource milo now loads on
  web. The fix was a one-line MEMFS relative-path anchor in
  `rb3/native/src/native_file.cpp`'s `__EMSCRIPTEN__` fetch fallback (see
  W2 doc, "WEB UI-HUD wall — RESOLVED"). This is exactly what an App-driven
  boot needs: the App's font/icon resources resolve the same way.

**The native reference is `RunGame` (`main_native.cpp:529-603`, gated by the
`RB3_GAME` env var — NOT a preprocessor symbol).** It:
1. Registers a BandRnd shutdown exit-callback, seeds `TheNativeSettings`.
2. Stands up the GpuDevice synchronously: `gBandRnd.InitGpu(W,H,headless)`
   (`main_native.cpp:564`) — the monolithic `InitGpu` = `StartGpuInit` + sync
   `IsReady()` poll + `InitGpuResources` (W2a engine split, `Rnd_Wgpu_RB3.h:75-84`).
3. `RB3RegisterLegacyRndAliases()` (`main_native.cpp:583`) — registers the
   legacy short class names `Tex`/`Text`/`Dir` so font/UI milos don't hit
   "Can't make Tex".
4. `chdir(RB3_DATA)`, `TheLoadMgr.mPlatform = kPlatformXBox`, `SetSystemArgs`.
5. `App app(argc, argv); app.Run();` — the App ctor (`App.cpp:136-384`) runs
   the full boot spine; `App::Run()` → `App::RunWithoutDebugging()`
   (`App.cpp:497-602`) enters the `HX_NATIVE` frame loop (`:535-558`):
   `SystemPoll` → `TheUI.Poll()` → `RB3GameInputPoll(frame)` → `TheTaskMgr.Poll()`
   → `TheSynth->Poll()` → `TheRnd->BeginDrawing()` → (sigsetjmp guard) `TheUI.Draw()`
   → `TheRnd->EndDrawing()`.

**`App.cpp` has NO `RunOneFrame()` and NO `__EMSCRIPTEN__`/`HX_WEB` arms today**
— only `HX_NATIVE` gating (33 sites). W3a adds both. The native frame loop body
(`App.cpp:535-558`) is the literal template for `RunOneFrame()`.

**The W1 audio filter is still in place.** `VorbisReader.cpp` and `Synth.cpp`
are excluded from the web build by name in
`RB3_WEB_NATIVE_FORK_EXCLUDE` (`CMakeLists.txt:635-645`). Root cause:
`src/system/oggvorbis/codec.h:31-33` unconditionally defines
`inline void *alloca(size_t size){ return __alloca(size); }`, which clashes with
clang's `__builtin_alloca` under emcc. **DC3 already solved this** — its
`codec.h:32-34` guards the equivalent with `#ifndef HX_NATIVE` (and the web
build defines `HX_NATIVE=1`). See W3c.

## Out of scope

- Online multiplayer (native v1 non-goal; `src/network/` stays stubbed).
- MIDI / gamepad input. Keyboard only.
- HD / 60fps target. Stable 30fps is fine for v1.
- Service worker / IndexedDB caching. W4.
- Safari (Chrome 137+ JSPI only, per PLAN.md non-goals).

## Sub-phases

W3a → W3b → W3c, **sequential** (W3a and W3c both touch `main_web.cpp` +
`App.cpp`; concurrent dispatch merge-conflicts). W3b (input) is mostly a
self-contained edit to `rb3_game_input.cpp` and can overlap W3c's audio
bring-up, but its acceptance depends on W3a's interactive menu, so dispatch it
after W3a lands.

**Independently shippable?** Yes — W3a (App boot → interactive menu with text)
is a real, demoable milestone on its own (audio still stubbed). Ship it, then
W3b, then gate W3c on native v1. The hard native-v1 dependency is W3c-only.

---

### W3a — App-driven boot + interactive menu [OPUS]

> **STATUS 2026-05-29 — DONE (stable menu renders; interactivity is W3b).** The real `App`
> boots in-browser via `RunOneFrame`: it advances `intro_movie_screen → splash_screen` and
> renders a stable frame — the "ROCK BAND 3" logo text + full main-hub venue (829 meshes /
> 223K tris), `rb3AppBooted==1`, frame count climbs indefinitely, no trap. Master commit
> (squash of `wt-web-w3a` 29a079f6/1b1ed9ef/69951976). Fixes (all `#ifdef HX_WEB`-gated, zero
> native/Wii-asm impact): (1) gate the Bink movie-blit in `MoviePanel::Draw`/`TexMovie` (stubbed
> decoder); (2) register `Fader::Init()`+`BinkClip::Init()` directly (synth is a no-op stub on web);
> (3) define the undefined object-VALUE `.s`-stub globals (`TheStoreMetadata`/`TheNet`/`TheServer`/
> `TheMC`/`TheWii*`/…) as zeroed weak `char[4096]` in `rb3_web_globals.cpp`; plus a yielding
> `LoadMgr::PollUntilLoaded`/`Poll` (`Loader.cpp`) + non-fatal `Debug::Fail` on web.
> **W3b is the gate to interactivity:** `splash_screen` is RB3's "Press START to Rock" screen —
> it waits for `button_down … kAction_Confirm` to fire `{ui goto_screen main_hub_screen}`. Wire
> keyboard input (W3b below) so a Confirm press advances splash → main_hub. No further
> blocking globals/movie/audio traps expected between splash and main_hub.

Replace the static mesh-walk harness with the real `App` boot. This is the
critical-path integration. Read
[`dc3-decomp/docs/plans/web-port/UNIFY_WITH_APP.md`](../../../../dc3-decomp/docs/plans/web-port/UNIFY_WITH_APP.md)
first — DC3's bug catalogue ports almost directly.

#### Step 1 — Extract `App::RunOneFrame()` (`rb3/src/App.cpp` + `rb3/src/App.h`) [OPUS]

Mirror DC3's `UNIFY_WITH_APP.md` Phase 1. Pull the **core poll + draw** out of
the `HX_NATIVE` frame-loop body (`App.cpp:535-558`) into a new method:

```cpp
// App.cpp — inside #ifdef HX_NATIVE (covers both native desktop AND web —
// the web build also defines HX_NATIVE=1; see CMakeLists.txt:371).
void App::RunOneFrame() {
    SystemPoll(false);
    TheUI.Poll();
    RB3GameInputPoll(...);     // native frame counter / web event drain
    TheTaskMgr.Poll();
    if (TheSynth) TheSynth->Poll();
#ifdef __EMSCRIPTEN__
    AudioDevice::GetInstance().PumpAudio();   // W3c wires the real impl
#endif
    if (TheRnd) TheRnd->BeginDrawing();
    // draw guard (sigsetjmp) stays native-desktop-only; see note below
    TheUI.Draw();
    if (TheRnd) TheRnd->EndDrawing();
}
```

- Declare `RunOneFrame()` in `App.h` inside `#ifdef HX_NATIVE` (the PPC build
  must not see it — it would regress the match).
- `RunWithoutDebugging`'s loop body calls `RunOneFrame()` per iteration on
  native; everything else (frame cap, `MILO_MAX_FRAMES`, HTTP server poll,
  `RB3HttpServerPollScreenshots`) stays in `RunWithoutDebugging`.
- **`sigsetjmp` draw guard:** `App.cpp:546-553` wraps `TheUI.Draw()` in a
  SIGSEGV longjmp guard. POSIX signals don't exist under emcc. Guard the
  sigsetjmp with `#if defined(HX_NATIVE) && !defined(__EMSCRIPTEN__)` and on web
  call `TheUI.Draw()` directly (the web boot's existing per-frame try/catch in
  `main_web.cpp` provides the analogous safety net).
- **The `AudioDevice::PumpAudio()` call goes INSIDE `RunOneFrame()` under
  `#ifdef __EMSCRIPTEN__`**, matching DC3 (`dc3-decomp/src/App.cpp` RunOneFrame,
  per `UNIFY_WITH_APP.md` Phase 1). Do NOT pump from `main_web.cpp` — that
  double-pumps. It is a no-op until W3c provides the real `AudioDevice_Web`
  source (the engine's `AudioDevice_Web.cpp` is already in the web source set,
  `CMakeLists.txt:578`).

**Validation:** native `RB3_GAME=1 rb3-native` still runs identically (same
frame-loop output, same PNGs). Web build still compiles.

#### Step 2 — Add `__EMSCRIPTEN__`/`HX_WEB` arms to the `App` ctor (`rb3/src/App.cpp`) [OPUS]

The App ctor already has the `RB3_GAME`-equivalent boot spine — but every
divergence is currently `HX_NATIVE`-only, and a few are `#ifndef HX_NATIVE`
(Wii-only) which the web build (defining `HX_NATIVE=1`) correctly skips. The
work is making the ctor's GPU + path-resolution + content-discovery steps
web-safe. Apply DC3's `UNIFY_WITH_APP.md` Phase 2 lessons:

- **GPU init is async on web.** The ctor calls `TheRnd->PreInit()`
  (`App.cpp:157`) then `TheRnd->Init()` (`App.cpp:188`). `TheRnd` is `gBandRnd`
  (`BandRnd : Rnd`). On native, `RunGame` calls `gBandRnd.InitGpu(W,H,headless)`
  (synchronous) *before* constructing App. On web the device is async, so the
  monolithic `InitGpu` returns false (the W2a problem). **Recommended seam:**
  in `main_web.cpp`, keep the existing two-phase bring-up
  (`StartGpuInit` in `BOOT_ENGINE_INIT` *before* the App ctor; poll `IsReady()`
  in `BOOT_GPU_WAIT`; `InitGpuResources()` in `BOOT_GPU_READY`) — then construct
  `App` in a NEW `BOOT_APP_CTOR` state *after* the GPU is ready. That way
  `BandRnd` is fully initialised before the App ctor's `TheRnd->Init()` runs, so
  give `BandRnd::Init()` (or a small `#ifdef __EMSCRIPTEN__` arm in the ctor) an
  early-return-if-`mGpuReady` so it doesn't re-init. This is cleaner than DC3's
  in-ctor JSPI yield because RB3's BandRnd two-phase split already exists.
  *Confirm the exact ordering by reading `Rnd_Wgpu_RB3.cpp`'s `Init`/`PreInit`
  bodies — this is a discover-as-you-go integration point.*
- **`RB3RegisterLegacyRndAliases()`** (the native `RunGame` calls it at
  `main_native.cpp:583`): the web boot must call it too, before any UI milo
  loads, or font/UI milos hit "Can't make Tex/Text". Either call it from
  `main_web.cpp` before the App ctor, or add an `#ifdef __EMSCRIPTEN__` call
  inside the ctor near `TheRnd->PreInit()`.
- **Native manager stubs:** the ctor already calls
  `RB3RegisterNativeManagerStubs()` under `#ifdef HX_NATIVE` (`App.cpp:332-338`)
  — web inherits this automatically (it defines `HX_NATIVE=1`). Good: it
  registers `saveload_mgr`/`net_cache_mgr` stubs + `StartRefresh()` (song
  discovery) the menu DTAs gate on.
- **`DirLoader` cache mode / path eval:** verify the web load path matches
  native. The W2 harness sets `TheLoadMgr.mPlatform = kPlatformXBox` and
  chdir's to `/data`; the App boot must do the same before the ctor (the boot
  machine already chdir's in `DoEngineInit`). DC3 adds
  `DirLoader::SetCacheMode(true)` under `#ifdef __EMSCRIPTEN__`
  (`UNIFY_WITH_APP.md` Phase 2 item 3) — check whether RB3 needs the equivalent
  (RB3's `ObjectDir::Init` cache-mode auto-set may already cover it).
- **`PollUntilLoaded` safety valve:** the App boot drives loads through
  `LoadMgr::PollUntilLoaded` (`rb3/src/system/utl/Loader.cpp`), which on web
  runs un-interruptibly (the W2 `LoadMiloDirYielding` worked around this only
  for the harness's single load). Port DC3's web cap: cap the loop iterations
  under `HX_WEB` and yield via `emscripten_sleep(0)` per poll — DC3's cap lives
  in `dc3-decomp/src/system/utl/Loader.cpp` `LoadMgr::PollUntilLoaded` (cf. W2's
  `LoadMiloDirYielding`). **This is the dominant unknown** — the App boot loads
  far more than one milo (splash → main_hub → song_select chains), so the load
  loop must yield or the tab freezes. *Read RB3's `Loader.cpp` PollUntilLoaded
  and decide whether the cap + yield goes there (matched-fork, `#ifdef HX_WEB`)
  or whether the boot machine drives it state-by-state.*
- **Non-fatal `Debug::Fail()` on web:** mirror DC3's
  `dc3-decomp/src/system/os/Debug.cpp` web early-return so a missing-asset
  `MILO_FAIL` (e.g. a dev-only DTA 404) doesn't `abort()` the tab. Port to
  `rb3/src/system/os/Debug.cpp` under `HX_WEB`.

#### Step 3 — Rewrite `main_web.cpp` to construct + drive `App` [OPUS]

Replace `BOOT_LOADING_MILO` + `BOOT_RUNNING_RENDER` with App-driven states.
Mirror DC3's `main_web.cpp` (`BOOT_ENGINE_INIT: sApp = new App(0, nullptr)` →
`BOOT_RUNNING: sApp->RunOneFrame()`), adapted to RB3's existing GPU two-phase
states:

```
BOOT_INIT        → WebAssetsInit + WebAssetsFetchBundle           (unchanged)
BOOT_FETCHING    → poll WebAssetsAllDone                          (unchanged)
BOOT_ENGINE_INIT → chdir(/data) + platform=XBox + StartGpuInit    (trim DoEngineInit:
                   NO SystemPreInit/SystemInit/RegisterCommonFactories — the App
                   ctor owns those now)
BOOT_GPU_WAIT    → Gpu().PollEvents() + IsReady()                 (unchanged)
BOOT_GPU_READY   → InitGpuResources()                             (unchanged)
BOOT_APP_CTOR    → RB3RegisterLegacyRndAliases() + sApp = new App(0, nullptr)  (NEW)
BOOT_RUNNING     → sApp->RunOneFrame(); window.rb3FrameCount = N  (NEW)
```

- **Delete** `RegisterCommonFactories` and `GetMiloPathFromUrl`/`?milo=` parsing
  + `LoadMiloAndWalk`/`RenderFrame` calls. The App ctor registers factories
  via `BandInit()`/`UIManager::Init()` etc.
- **Keep the W2 render harness behind a flag, don't delete it.** Gate the old
  mesh-walk path on a `?milo=<path>` URL param: if present, run the W2 harness
  (W2 regression guard stays green); if absent, run the App boot. This keeps the
  pixelmatch regression test alive and gives a fast geometry-only debug path.
- Wrap the App ctor + each `RunOneFrame` in the existing try/catch →
  `BOOT_ERROR` so a boot exception is a console line, not a tab kill.
- Emit a NEW `window.rb3AppBooted = 1` once `sApp` is constructed, and keep
  `window.rb3FrameCount`. Add `window.rb3CurrentScreen` (set from
  `TheUI.CurrentScreen()->Name()`) so the smoke can poll screen-flow progress.

**Acceptance W3a:**
- `window.rb3AppBooted === 1`, `window.rb3FrameCount >= 60`, no WASM trap.
- `window.rb3CurrentScreen` advances past `splash`/`startup` to a menu screen
  (the boot auto-advances; no input yet). Screenshot of the menu shows **text
  labels** (`RndText` glyph quads) — not just panel/background meshes (the W2
  harness drew only the 11 panel meshes; this proves `UIScreen`/`UIPanel` poll +
  draw their components).
- The `?milo=` regression path still passes the W2 pixelmatch smoke.

**Suggested subagent prompt (W3a):**
> Execute Phase W3a of the RB3 web port
> (`rb3/docs/plans/web-port/W3_BOOT_TO_SONG.md`). W2 landed: the browser renders
> RB3 geometry via a static mesh-walk harness (`main_web.cpp` +
> `rb3_render_mesh.cpp` `RenderFrame`). W3a retires that for the real `App` boot,
> mirroring DC3's `dc3-decomp/native/src/main_web.cpp` (`sApp = new App(0,
> nullptr)` then per-frame `sApp->RunOneFrame()`). Read
> `dc3-decomp/docs/plans/web-port/UNIFY_WITH_APP.md` first — its bug catalogue
> ports directly. Steps: (1) extract `App::RunOneFrame()` from the
> `HX_NATIVE` frame-loop body (`App.cpp:535-558`) into a method declared in
> `App.h` inside `#ifdef HX_NATIVE`; put `AudioDevice::PumpAudio()` inside it
> under `#ifdef __EMSCRIPTEN__`; guard the sigsetjmp draw guard with `#if
> defined(HX_NATIVE) && !defined(__EMSCRIPTEN__)`. (2) Make the App ctor
> web-safe: web GPU is async — keep the two-phase StartGpuInit/InitGpuResources
> bring-up in `main_web.cpp` and construct App AFTER the GPU is ready; ensure
> `BandRnd::Init` early-returns when `mGpuReady`; call
> `RB3RegisterLegacyRndAliases()` before any UI milo loads; port DC3's
> `PollUntilLoaded` web cap + yield to `rb3/src/system/utl/Loader.cpp`; port
> DC3's non-fatal `Debug::Fail()` web early-return to `rb3/src/system/os/Debug.cpp`.
> (3) Rewrite `main_web.cpp` to construct + drive App; keep the W2 mesh-walk
> harness behind a `?milo=` URL param (so the W2 pixelmatch regression stays
> green); emit `window.rb3AppBooted`, `window.rb3FrameCount`,
> `window.rb3CurrentScreen`. NOTE: `RB3_GAME` is a runtime env var in
> `main_native.cpp`, not a preprocessor symbol — there are no `#ifdef RB3_GAME`
> gates. Acceptance: the browser boots through splash to a menu screen with
> visible text labels, `rb3AppBooted=1`, `rb3FrameCount>=60`, no trap; the
> `?milo=` path still passes the W2 smoke. Do not break native `RB3_GAME=1` or
> the DC3 web build.

---

### W3b — Real keyboard input [SONNET]

> **STATUS 2026-05-29 — DONE.** Real browser keyboard input wired. `window._rb3Keys`
> bitmask maintained by JS keydown/keyup listeners; `RB3GameInputPoll` edge-detects new
> presses per frame and calls `ExecButton(action, button, screen)` — the same path the
> synthetic script and HTTP `/api/input` use. Key→action map: Enter→Confirm, Space→Start,
> Esc/Backspace→Cancel, Tab→Option, Arrow/WASD→d-pad, Q/E→PageUp/Dn. Smoke test confirms:
> two Enter presses advance `splash_screen → main_hub_screen` (frame 47 + frame 70). On
> `main_hub_screen` the next Enter fires on `mb_playnow.btn` (Play Now button). Canvas
> 56.7% painted with venue+menu geometry. W2 `?milo=` regression: 0.00% pixel diff (PASS).
> Native build: compiles unchanged (`#ifdef __EMSCRIPTEN__` gate confirmed). Commit: wt-web-w3b.
> **Next: deeper menu navigation (main_hub → song_select → part_difficulty) is the W3b
> fuller acceptance; the core input pipeline is working and splash→main_hub is confirmed
> interactive. W3c (audio) can now be dispatched.**

#### W3c-nav — full menu→gameplay-entry flow by keyboard [2026-05-29, DONE up to part_difficulty]

> **STATUS 2026-05-29 — keyboard drives the WHOLE menu spine to `part_difficulty_screen`
> with the 83-song library populated and the selected song resolved.** Branch
> `wt-web-w3cnav`. Deepest screen: **`part_difficulty_screen`** (the seldiff part/difficulty
> picker), reached purely by keyboard from a cold boot. Stops there: the part_difficulty →
> gameplay transition is **audio/native-v1-gated** (`Game.cpp:774`
> `MILO_ASSERT(GetSongStream())` — the gem-track `game_screen` hard-requires a decoded MOGG
> song stream, which is exactly W3c Step 3, gated on native v1 + audio recovery).
>
> **Full key sequence (cold boot → deepest screen):**
> | Screen | Key | Result |
> |---|---|---|
> | `splash_screen` | **Space** (Start) | fires splash start.btn → overshell add-user |
> | `splash_screen` | **Enter** (Confirm) | overshell continue-without-profile → `main_hub_screen` |
> | `main_hub_screen` | **Enter** | focus `mb_playnow.btn` → `set_state kMainHubState_PlayNow` (focus → `pn_quickplay.btn`) |
> | `main_hub_screen` | **Enter** | `pn_quickplay.btn` → `kMainHubState_Quickplay` (focus → `qp_quickplay.btn`) |
> | `main_hub_screen` | **Enter** | `qp_quickplay.btn` → set_override Waiting → `song_select_enter_screen` → **`song_select_screen`** (83 songs) |
> | `song_select_screen` | **↓ (ArrowDown)** | navigate `song.lst` |
> | `song_select_screen` | **Enter** | confirm song → **`part_difficulty_screen`** (song "1. 20th Century Boy — T. Rex", part picker shows GUITAR/BASS) |
> | `part_difficulty_screen` | (further) | **STOP — overshell part-select → `Game::LoadSong` is audio-gated** |
>
> **The minimum-win blocker that was fixed — empty song list on web.**
> `NativeContentMgr::StartRefresh()` (`native/src/rb3_platform_native.cpp`) keyed song
> discovery on `getenv("RB3_DATA")`, which is **unset on web** (the boot machine `chdir`s to
> `/data` MEMFS instead). So the web song_select list was empty and a song-confirm bounced to
> `no_valid_songs_screen`. Fix (one `#ifdef __EMSCRIPTEN__` block): default `dataRoot` to
> `/data` on web, where `WebAssets` unpacks `songs/songs.dta`. Result:
> `StartRefresh loaded /data/songs/songs.dta — TheSongMgr now has 83 ranked songs`. The
> populated list renders (MUSIC LIBRARY header + song rows + scores 0/10·0/5·0/30★ + album-art
> box) and a song-confirm now correctly enters `part_difficulty_screen`.
>
> **New JS signals** (`main_web.cpp`, all `#ifdef __EMSCRIPTEN__` / web-only): `window.rb3SongCount`
> (GetRankedSongs size — confirms DB populated), `window.rb3FocusButton` (current focused
> UIComponent name — verifies the focus chain mb_playnow→pn_quickplay→qp_quickplay).
>
> **Engine concurrency note:** the live engine tree had **uncommitted in-progress RTT
> (render-to-texture) code** in `Rnd_Wgpu_RB3.cpp` (`BeginDrawTarget`/`EndDrawTarget`/
> `mRtActiveTex`) that threw `beginRenderPass ... depthClearValue non-finite` at frame 2 and
> **froze the RAF loop** (W3b baseline FAILED against the live engine: painted=0%, screen stuck
> empty). This is NOT in the pin `4077997`. Built against an isolated pin worktree
> (`/tmp/milo-engine-pin-4077997`) per the concurrency protocol → no page error, the loop runs
> and `window.rb3CurrentScreen` publishes. **`MILO_ENGINE_PIN` should stay at `4077997` until
> the RTT work lands cleanly.**
>
> **Regressions GREEN:** W2 `?milo=ui/track/gen/gem_smasher_guitar_meshes.milo_xbox` renders
> (milosLoaded=1, 45 frames, 3.69% nonClear, center 200,200,200 — PASS). splash→main_hub
> (W3b) still works (Space+Enter). Native `rb3-native RB3_GAME=1` builds + runs unchanged
> (the song-discovery fix is `#ifdef __EMSCRIPTEN__`, zero native impact; `git diff` confirms).
>
> **Test:** `scripts/web/w3cnav-test.mjs` — boots App, drives the key sequence above, polls
> `rb3CurrentScreen`/`rb3SongCount`, screenshots each milestone →
> `scripts/web/results/web-w3cnav/{song_select,gameplay}/`.
>
> **Next blocker (independently fixable vs gated):** the only thing between here and the gem
> highway is (1) the overshell part-select keyboard nav on `part_difficulty_screen` (the part
> picker is shown but the synth user's slot doesn't consume the screen-level Confirm — focus is
> `(none)`; native crosses this with synthetic `track:guitar` + `msg:overshell:end_override_flow`
> verbs, not raw nav) — *independently fixable*, and (2) `Game::LoadSong`'s song-stream assert —
> **audio/native-v1-gated (W3c)**. Even with (1) solved, (2) blocks the actual gem_screen until
> W3c lands audio.

The App boot makes the menu interactive; W3b feeds real browser keypresses into
the same input pipeline the synthetic driver uses.

**RB3's input seam is `rb3/native/src/rb3_game_input.cpp`.** It exposes
`RB3GameInputPoll(int frame)` (`:691`, called per frame from the native loop /
`RunOneFrame`), and the real engine paths `ExecButton(action, button, screen)`
(`:461-470` → `ButtonDownMsg` → `TheUI.Handle`), `ExecSelect`, `ExecMsg`. It
already has an HTTP-injected verb queue (`gPendingInject`, drained in
`RB3GameInputPoll`) — the web keyboard path is the same shape: a bitmask/queue
filled by JS, drained on the main thread.

**DC3's web keyboard pipeline is `dc3-decomp/native/src/platform/Joypad_Native.cpp`**
(`InitWebInput`, ~`:87-150` — NOT `Keyboard_Native.cpp`, which is GLFW-only and
`#ifndef __EMSCRIPTEN__`-gated). It registers `document.addEventListener('keydown'/'keyup', …)`
via `EM_JS`/`EM_ASM`, maintains `window._dc3Keys` as a button bitmask
(arrows/WASD → d-pad, Enter → confirm/X, Esc/Backspace → cancel/Circle,
Space → Start, Tab → Select, Q/E → L1/R1, etc.), with `preventDefault` on the
nav keys.

**Files:**

- `rb3/native/src/rb3_game_input.cpp`: add an `#ifdef HX_WEB` (or
  `__EMSCRIPTEN__`) block:
  - `InitWebInput()` (once): `EM_ASM` install `keydown`/`keyup` listeners that
    set `window._rb3Keys` per the DC3 key→bit map, mapped to RB3's
    `JoypadButton`/`JoypadAction` (the `ActionFromName` table at
    `rb3_game_input.cpp:178-192` is the canonical action set: confirm/start/
    cancel/option/up/down/left/right). Use `preventDefault` on arrows/Space/
    Tab/Esc/Backspace.
  - In `RB3GameInputPoll`, read the bitmask via `EM_ASM_INT`, diff against the
    previous frame's mask, and for each newly-pressed bit call `ExecButton(...)`
    with the mapped action/button on `TheUI.CurrentScreen()`. (Edge-trigger:
    fire on press, not while held — the synthetic driver's `ExecButton` is a
    single `ButtonDownMsg`.)
  - Reuse `SynthUser()` (`:302-341`) for the `LocalUser` — the same user the
    synthetic/HTTP paths use, with pad 0 marked connected. This makes the
    overshell add-user / slot-join flow (which gates splash → main_hub) work.
  - Keep the `RB3_GAME_INPUT` scripted path AND the HTTP path as fallbacks
    (they coexist — all three feed `ExecButton`/the verb queue).
- `rb3/native/web/index.html`: document the key bindings in the status panel.
- **AudioContext autoplay:** the keypress handler must also resume the
  `AudioContext` on first interaction (browser autoplay policy) — DC3 wires this
  via the same keydown listener. Defer the actual `audioCtx.resume()` to W3c
  (no audio yet), but design the listener so W3c can hook it.

**Acceptance W3b:** a human navigates the menus in the browser via keyboard
alone — reaches song_select, scrolls the list, selects a song. The Playwright
smoke (`--boot-to-song`, see below) drives keys via
`page.keyboard.press(...)` and asserts `window.rb3CurrentScreen` advances
`main_hub → song_select → part_difficulty`.

**Suggested subagent prompt (W3b):**
> Execute Phase W3b of the RB3 web port
> (`rb3/docs/plans/web-port/W3_BOOT_TO_SONG.md`). W3a landed: the real `App`
> boots in the browser and the menu is drawn + interactive via synthetic input.
> W3b adds real browser keyboard input. Mirror DC3's
> `dc3-decomp/native/src/platform/Joypad_Native.cpp` `InitWebInput` (~lines
> 87-150; NOT `Keyboard_Native.cpp`, which is GLFW-only): `EM_ASM`-install
> `keydown`/`keyup` listeners maintaining a `window._rb3Keys` bitmask, mapped to
> RB3's `JoypadButton`/`JoypadAction` (arrows/WASD → d-pad, Enter → confirm,
> Esc → cancel, Space → Start, Tab → Select). Wire it into RB3's input seam
> `rb3/native/src/rb3_game_input.cpp` under `#ifdef HX_WEB`: read the bitmask in
> `RB3GameInputPoll`, edge-detect new presses, call the existing
> `ExecButton(action, button, TheUI.CurrentScreen())` path (reuse `SynthUser()`
> for the LocalUser). Keep the `RB3_GAME_INPUT` script + HTTP paths as
> fallbacks. Add the AudioContext-resume-on-keypress hook (no-op until W3c).
> Document keys in `rb3/native/web/index.html`. Acceptance: a human reaches
> song_select and selects a song by keyboard; the smoke drives keys and asserts
> `window.rb3CurrentScreen` advances main_hub → song_select → part_difficulty.
> Don't break the synthetic/HTTP input paths or native.

---

### W3c — Audio recovery + one song end-to-end [OPUS]

> **STATUS 2026-05-29 — Part A DONE; Part B reaches Game::LoadSong (the gem-track
> `game_screen` handoff) by keyboard.** Branch `wt-web-w3c`.
>
> **Part A — audio source recovery (A1 + A2 DONE):**
> - `codec.h:31-33`'s unguarded `inline void *alloca` is now wrapped in
>   `#ifndef HX_NATIVE` (web defines `HX_NATIVE=1`, so `__builtin_alloca` is used;
>   the PPC MWCC build still takes the explicit forwarder — matched-fork-safe).
> - `VorbisReader.cpp` + `Synth.cpp` removed from `RB3_WEB_NATIVE_FORK_EXCLUDE`;
>   they now compile into `rb3-web.wasm` with no codec.h/alloca error.
> - Audio glue re-added to `RB3_WEB_NATIVE_GLUE`: `rb3_synth_native.cpp`,
>   `rb3_stream_receiver_native.cpp`, `rb3_keychain_native.cpp`,
>   `rb3_vorbis_poll_shim.cpp` + tomcrypt `aes.c`/`crypt.c`/`ctr.c` (LANGUAGE C,
>   `-fno-ms-*`). All are gated `#ifdef HX_NATIVE` (web defines it), so they
>   compile under emcc unchanged. **A1: rb3-web links (28MB wasm).**
> - **The stream-receiver maps onto the web AudioSource with ZERO adaptation.**
>   The engine `AudioDevice`/`AudioSource` interface (`AddSource`/`RemoveSource`/
>   `RenderAudio`/`IsFinished`) is shared between native (miniaudio) and web
>   (`AudioDevice_Web.cpp`, AudioWorklet + SAB ring). On web, BOTH the producer
>   (synth `Poll` writes the ring) and the consumer (`RenderAudio` via
>   `AudioDevice::PumpAudio()` from `App::RunOneFrame`, `#ifdef __EMSCRIPTEN__`)
>   run on the main thread; the `std::atomic` back-pressure in
>   `rb3_stream_receiver_native.cpp` is single-threaded but correct. No
>   `#ifdef HX_WEB` divergence was needed in the receiver.
> - **A2:** `SynthPreInit` reads `(use_null_synth FALSE)` from the real config
>   and selects `CreateNativeSynth() → NativeSynth::Init() → AudioDevice::Init(44100)`.
>   Console confirms `AudioDevice: initialized (web) -- 44100 Hz, ring 32768 frames`
>   + `AudioDevice: AudioWorklet connected (44100 Hz)` on every boot — the web
>   audio backend is live + the worklet streams from the SAB ring.
>
> **Part B — reach the gameplay screen (B1 DONE; B2 at the LoadSong handoff):**
> - **Part-select advance (`rb3_game_input.cpp`, `#ifdef __EMSCRIPTEN__`):** a
>   Confirm on `part_difficulty_screen` ARMS a per-frame, readiness-gated verb
>   sequence replicating native v1's part-select crossing:
>   `track:guitar → msg:overshell:end_override_flow:1:0 → nofail → autohit`
>   (`WebDrivePartSelect`). Console shows `armed → FIRE track → FIRE msg → FIRE
>   nofail → FIRE autohit`, the screen crosses `part_difficulty_screen →
>   tv3_b_screen`, and `BandDirector::OnLoadSong` + `Game::LoadSong` run to
>   `SongData::Load`. **The `Game.cpp:774 MILO_ASSERT(GetSongStream())` is PASSED**
>   — LoadSong proceeds past it into the MIDI/MOGG load (audio recovery satisfied
>   the assert).
> - **Song-select confirm:** a Confirm on `song_select_screen` now fires
>   `{music_library select_highlighted_node}` (the native verb), not a raw
>   ButtonDownMsg — deterministic song-confirm to `part_difficulty_screen`.
> - **`window.rb3HighlightedSong` / `rb3HighlightedType`** added to `main_web.cpp`
>   (a `TheMusicLibrary->GetHighlightedNode()` probe) so the gameplay smoke can
>   observe the library highlight.
> - **`NodeSort::GetNode` web hardening (`SongSort.cpp`, `#ifdef HX_WEB`):** the
>   out-of-bounds branch `MILO_FAIL(...)` is non-fatal on web (returns), so the
>   original `return mList[idx]` then indexed an empty vector and HARD-TRAPPED
>   (wasm `unreachable`). This fired intermittently at boot when a song-sort
>   preview queried the highlighted node before the sort was populated (a
>   load-order race the bigger audio-laden web build exposes). The web arm returns
>   `nullptr` instead so the caller bails gracefully. **The PPC matched build is
>   byte-identical** (verified — the `#ifdef HX_WEB` block is invisible to MWCC).
> - **Test:** `scripts/web/w3c-gameplay-test.mjs` boots → menu → song_select
>   (navigates to 20th Century Boy) → part_difficulty → arms the crossing →
>   game-load. Screenshots → `scripts/web/results/web-w3c/gameplay/`.
>
> **The one remaining gap (B2 finale, precise follow-up blocker):** with the
> target pinned to 20th Century Boy, the crossing fires cleanly and the song's
> `20thcenturyboy.milo_xbox` + `20thcenturyboy.mid` are fetched (HTTP 200) and the
> MIDI chart parses (note events log). `Game::LoadSong` then runs its heavy
> SYNCHRONOUS MIDI-driven gem-track / BeatMaster setup on the WASM main thread and
> **HARD-TRAPS (wasm `unreachable`) before the `.mogg` is ever opened** (the
> server never sees a `20thcenturyboy.mogg` GET). So the crash is NOT the 35MB MOGG
> decode (that path is never reached) — it is in the chart/gem/beatmatch
> construction. The signature matches the SongSort class of bug: a `MILO_FAIL`
> that is non-fatal on web (returns) followed by an out-of-bounds / null deref in
> the same function. The fix is to find that specific call site in the
> gem/beatmatch setup chain (`Game::LoadSong → BeatMaster/GemManager/TrackWatcher`
> setup) and add the same `#ifdef HX_WEB` graceful-return guard SongSort got.
> Deepest screen reached by keyboard: `tv3_b_screen`/`tv3_a_screen` (the gameplay
> venue transition) + `BandDirector::OnLoadSong` — one trap short of the gem
> highway painting. **NOTE the boot-time SongSort FAILs are now SURVIVED** (the
> `#ifdef HX_WEB` GetNode guard turned the former intermittent splash trap into a
> non-fatal log), which is what let the run reach the song load at all.
>
> Separately, the 35MB MOGG over the on-demand SINGLE sync-XHR fetch on the main
> thread is W4 streaming territory: once the LoadSong trap is fixed, expect a
> multi-second main-thread stall during the MOGG fetch+decode (the same stall
> native v1 absorbs off-thread via miniaudio). A worker-backed streaming fetch is
> the W4 follow-up.
>
> **Regressions GREEN:** rb3-native builds + runs `RB3_GAME=1` to gameplay
> (`Game::LoadSong() ENTERED — song='20thcenturyboy'`, gem track + MIDI load,
> clean exit) — the codec.h guard + glue are `#ifndef HX_NATIVE`/`#ifdef
> __EMSCRIPTEN__`-gated, zero native/Wii-asm impact. W2 `?milo=` + W3b menu-nav
> still pass.

**HARD-GATED on native v1** (`V1_ONE_SONG.md`): do not start until native plays
the chosen song end-to-end on Linux. The audio source recovery (Step 1) can be
done earlier since it's a compile-only change, but the song integration (Step 3)
needs native v1's `LoadSong` byte-correctness first.

#### Step 1 — Recover `VorbisReader.cpp` + `Synth.cpp` under emcc [SONNET]

The clash is `src/system/oggvorbis/codec.h:31-33`:
```c
inline void *alloca(size_t size){ return __alloca(size); }
```
clang under emcc treats `alloca` as `__builtin_alloca` and rejects the
redefinition. **DC3 already proved the fix** — its `codec.h:32-34`:
```c
#ifndef HX_NATIVE
static void *alloca(size_t size) { return (void *)_alloca(size); }
#endif
```
The web build defines `HX_NATIVE=1` (`CMakeLists.txt:371`), so `#ifndef
HX_NATIVE` correctly omits the bogus definition on both native and web (the
system `<stdlib.h>` / `__builtin_alloca` provides the real one). **Recommended
fix:** guard RB3's `codec.h:31-33` with `#ifndef HX_NATIVE` exactly as DC3 does.
This is a matched-fork header edit, but it's inside a guard that the PPC build
(which does not define `HX_NATIVE`) still compiles unchanged — verify the PPC
`.o` for the two affected TUs is byte-identical after the edit (it should be:
PPC takes the `#else`-equivalent, i.e. the unguarded path). Then:
- Remove `VorbisReader` and `Synth` from `RB3_WEB_NATIVE_FORK_EXCLUDE`
  (`CMakeLists.txt:643-644`).
- Emscripten supplies ogg/vorbis via `-sUSE_OGG=1 -sUSE_VORBIS=1` (already wired
  in `milo_engine_apply_web_target_options`; see `CMakeLists.txt:388-390`). DC3
  does the same — it removes only `bitwise.c`/`framing.c` from the web source
  set and lets the emscripten port satisfy the `ogg_*`/`vorbis_*` symbols. Check
  whether RB3 needs the equivalent removal (RB3 globs `oggvorbis/*.c`? — it does
  NOT appear to compile them into the web target; verify the symbol resolution
  matches DC3's `-sUSE_*` port and add the `rb3_vorbis_poll_shim.cpp` if the
  HMX `vorbis_synthesis_poll` entry-point goes undefined — it's native-only at
  `CMakeLists.txt:460` and excluded from web at `:591`).

#### Step 2 — Bind the audio path into `AudioDevice_Web` [SONNET]

The engine's `AudioDevice_Web.cpp` (lifted in W0) is already compiled into the
web target (`CMakeLists.txt:578`). DC3's `AUDIO.md` (Option 2: custom
AudioWorklet + SharedArrayBuffer ring buffer) is the proven model — the COOP/COEP
headers, the worklet JS, and the SharedArrayBuffer push from `MixSources()` are
all engine-layer and shared. RB3's binding:
- `rb3/native/src/rb3_synth_native.cpp` (`NativeSynth::Init`, `:29-51`) calls
  `AudioDevice::GetInstance().Init(44100)` and binds
  `StreamReceiver::sFactory = &RB3CreateNativeStreamReceiver`. The engine's
  `AudioDevice` ctor selects the web impl when `__EMSCRIPTEN__` is defined — so
  **no RB3-side change beyond adding `rb3_synth_native.cpp` +
  `rb3_stream_receiver_native.cpp` to the web source set**
  (`RB3_WEB_NATIVE_GLUE`, `CMakeLists.txt:598-613` — currently EXCLUDED at
  `:589-590` because they pulled miniaudio; under emcc the engine `AudioDevice`
  is the web impl, not miniaudio, so re-add them and confirm they compile).
  *Verify `rb3_stream_receiver_native.cpp` compiles under emcc — it was authored
  for native miniaudio; the engine's `AudioSource`/web ring-buffer interface may
  differ. This is a discover-as-you-go point.*
- MOGG decryption (`tomcrypt aes.c/crypt.c/ctr.c` + `KeyChain` +
  `ByteGrinder`) is already wasm-clean (PLAN.md anchor). Add the three `.c`
  files + `rb3_keychain_native.cpp` to the web source set
  (`CMakeLists.txt:500-502`/`:455` — native-only today, `:594-595` notes they're
  excluded for clear-frame).
- `AudioDevice::PumpAudio()` is already called from `App::RunOneFrame()` under
  `#ifdef __EMSCRIPTEN__` (W3a Step 1). Verify the worklet JS
  (`audio-worklet.js`) lands next to `rb3-web.js` (engine post-build copy, W0).

**W3c Step-1+2 acceptance:**
- `VorbisReader`/`Synth` compile into `rb3-web.wasm` (no codec.h error).
- AudioContext shows "running" in DevTools after a keypress.
- A test `.mogg` plays through the speakers via a dev trigger. Define the
  trigger as a `?test-mogg=<path>` query param in `main_web.cpp` (a debug
  shortcut that feeds the path straight to a `MoggClip`/`StreamReceiver`,
  bypassing the menu) — specify the exact API in W3c, don't invent it ad-hoc.
- Native still plays the same `.mogg` identically (no regression).
- Sample rate: the engine's `AudioDevice_Web.cpp` already creates the
  AudioContext at 44100 Hz (Option B, PLAN.md / DC3 AUDIO.md 7d). No RB3-side
  resampling needed on Chromium. Safari clamp is a follow-up (out of scope).

#### Step 3 — One song end-to-end [OPUS]

The v1 web milestone. Once Step 1+2 land AND native v1 plays the song:
1. Procure the same `.mogg`+`.mid` pair native v1 uses (`V1_ONE_SONG.md` X1
   extract: `orig-assets/extracted-xbox-full/`).
2. Bundle them into the web asset server (or serve on-demand via the existing
   `/api/file/` fetch hook — the heavier path, ~30MB ciphertext; see Risk 3).
3. Boot the browser, navigate via keyboard to the song, select, play.

**Acceptance W3c (= v1 web):**
- Audio plays in sync with the gem track.
- Hit detection registers on keypress at the right times; score increments
  visibly (the `RndText` score HUD updates).
- The song reaches the end without WASM trap or audio dropout.
- Native v1 still passes (same song end-to-end on Linux).

**Suggested subagent prompt (W3c):**
> Execute Phase W3c of the RB3 web port
> (`rb3/docs/plans/web-port/W3_BOOT_TO_SONG.md`). W3a+W3b landed (App boots,
> menu interactive via keyboard). **Confirm native v1 plays the chosen song
> end-to-end on Linux before starting Step 3** (`V1_ONE_SONG.md`). Step 1
> (compile-only, can start now): recover `VorbisReader.cpp` + `Synth.cpp` under
> emcc by guarding RB3's `src/system/oggvorbis/codec.h:31-33`
> `inline void *alloca(...)` with `#ifndef HX_NATIVE` (exactly as DC3's
> `codec.h:32-34` does — the web build defines HX_NATIVE=1, so this omits the
> bogus def on both native+web; verify the PPC `.o` is byte-identical), then
> remove `VorbisReader`/`Synth` from `RB3_WEB_NATIVE_FORK_EXCLUDE`
> (`CMakeLists.txt:643-644`). Step 2: re-add `rb3_synth_native.cpp`,
> `rb3_stream_receiver_native.cpp`, `rb3_keychain_native.cpp`, and tomcrypt
> aes/crypt/ctr `.c` to the web source set (currently native-only / excluded);
> the engine's `AudioDevice_Web.cpp` (already in the web set) is the output
> device — no miniaudio on web. Add a `?test-mogg=<path>` debug trigger in
> `main_web.cpp`. Step 3: bundle native v1's `.mogg`+`.mid`, navigate by keyboard,
> play one song. Acceptance: audio in sync with gems, score increments, song
> completes, no trap; native v1 unaffected; DC3 web smoke still passes.

---

## Known gotchas

- **DC3's `UNIFY_WITH_APP.md` is the bug catalogue we'll hit.** Read it before
  W3a — NgEnviron/factory-order, MidiParser::Init, ContextCheckerInit,
  SetCacheMode, GLFW-symbol-resolution, ContentMgr behaviour all port.
- **`RB3_GAME` / `RB3_BOOT` are runtime env vars** in `main_native.cpp`, NOT
  preprocessor symbols. There are no `#ifdef RB3_GAME` gates. The web App boot
  mirrors the `RB3_GAME=1` *code path* (`RunGame`, `main_native.cpp:529-603`).
- **`PollUntilLoaded` un-interruptible on web** (W3a Step 2): the App boot loads
  many milos; the load loop must yield (`emscripten_sleep(0)` under JSPI) +
  cap iterations, or the tab freezes. W2's `LoadMiloDirYielding`
  (`rb3_render_mesh.cpp:268-312`) is the working pattern; W3a generalises it
  into `Loader.cpp` under `HX_WEB`.
- **AudioContext autoplay policy:** browsers require a user gesture before audio.
  The W3b keydown listener resumes the context (W3c hooks it).
- **The sigsetjmp draw guard** (`App.cpp:546-553`) is POSIX-signal-based and must
  be `#if defined(HX_NATIVE) && !defined(__EMSCRIPTEN__)`-gated; web relies on the
  `main_web.cpp` per-frame try/catch instead.
- **MOGG decrypt cost unmeasured on WASM main thread.** A 3-min song is ~30MB
  ciphertext; tomcrypt AES/CTR runs on the main thread. Measure on native; if
  >500ms, expect a frame stall during song-load — consider a worker (W4).

## Acceptance test (full W3)

```sh
cd rb3/
bash scripts/web/build.sh
python3 native/web/server.py --port 8421 &
node scripts/web/smoke-test.mjs --boot-to-song --song <id> \
    --frames-to-completion 12000
```

`--boot-to-song` is a NEW smoke mode (extends `scripts/web/smoke-test.mjs`,
which today supports `--milo`/`--diff-against` for the W2 path): it polls
`window.rb3AppBooted`, drives `page.keyboard.press` through the menu flow, polls
`window.rb3CurrentScreen` to confirm screen progression, and asserts audio +
score events fire. [SONNET to build this once W3a defines the
`window.rb3*` signals.]

Regression checks (must stay green):
- W2 `?milo=` pixelmatch smoke (`--milo … --diff-against …`).
- DC3 web smoke (`dc3-decomp/scripts/web/smoke-test.mjs`).
- RB3 native `RB3_GAME=1` boot + the `V1_ONE_SONG.md` guards.

## Critical path summary

```
W3a Step1 (RunOneFrame extract) → W3a Step2 (App ctor web arms)
        → W3a Step3 (main_web.cpp App boot)        [interactive menu w/ text]
              ↓
        W3b (keyboard input)                       [human plays menus]
              ↓                          W3c Step1 (codec.h fix, can start early)
        W3c Step2 (audio bind) ────────────────────┘
              ↓  (gate: native v1 plays the song)
        W3c Step3 (one song end-to-end)            [v1 web milestone]
```

**First implementation step:** W3a Step 1 — extract `App::RunOneFrame()` from
the `HX_NATIVE` frame-loop body (`App.cpp:535-558`) into a method declared in
`App.h` under `#ifdef HX_NATIVE`. It's a pure native-side refactor (no web
behaviour change yet), validates against the existing native run, and everything
downstream depends on it.
