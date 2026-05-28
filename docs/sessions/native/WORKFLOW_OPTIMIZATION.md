# rb3-native debug/tweak workflow optimization

**Date:** 2026-05-28
**Scope:** Rendering + gameplay-visual iteration (the current phase). The loop today is
`edit source → rebuild → re-run ~6000 frames → capture screenshots → manual review`.
This doc measures that loop, inventories DC3-decomp's dev tooling and assesses portability,
and ranks concrete speedups.

> READ-ONLY investigation. No source/build files were modified beyond `touch`ing a few
> TUs (mtime only, no content change) to time incremental rebuilds. No commits.

---

## 1. Current-loop cost breakdown (measured)

All numbers measured on this machine (32 cores, 93 GiB RAM, clang++ Debug build, `ninja`).

| Stage | Measured cost | Notes |
|-------|---------------|-------|
| **Incremental rebuild** (1 rendering TU recompile + relink) | **2.5 – 2.8 s** | `touch src/system/rndobj/Mat.cpp` → 2.80s total (1.85s compile + ~1s link). `touch native/src/rvl_shims.cpp` → 2.53s. The single static `rb3-native` exe relinks every time (default `ld`, 106 MB binary). |
| **No-op rebuild** | **0.03 s** | `ninja: no work to do`. |
| **Full run** (6000 frames, `MILO_HEADLESS=1 MILO_AUDIO=1`, V12 gem-flow reproducer) | **27.8 s wall** (21.45s user, 99% CPU) | ≈ **216 fps**. Pure CPU-bound `for` loop — **NOT wall-clock / vsync / audio gated** (App.cpp:504-525, no sleep). |
| **Boot → gameplay lead-in** | **frames 0–456** (`game_screen` reached at frame 456) | ≈ **2.1 s of the 27.8s run** is boot+menu nav before any gameplay renders. Screen flow: `intro_movie`(f0) → `splash`(f2) → … → `game_screen`(f456). |
| **Gameplay render window** | frames 456–6000 (≈ 5544 frames ≈ **25.7 s**) | This is the bulk. The 4 screenshots (V12 targets f3200/4000/4800/5600) live deep in this tail — you pay the full ≈26s to reach them. |
| **Screenshot capture** | negligible per shot (PNG readback) | BUT the `MILO_SCREENSHOT_DIR` mechanism does **not** `mkdir` the dir — if the dir is missing, `WritePNG` silently fails (observed: `WritePNG failed -> .../01_f3200.png`). Pre-create the dir. |
| **Review** | **manual, multi-minute** | Eyeball 4 PNGs + grep `*_DBG` stderr lines out of a **42,462-line** log. This is the human-latency step and (with the run) the real bottleneck. |

### Dominant cost
The **build is NOT the bottleneck** (2.8s). The bottleneck is **run + review**:
- **~26s wall per run**, of which **~24s is "fast-forwarding" through gameplay frames you don't care about** to reach the screenshot frames.
- **manual review** of PNGs + a 42k-line stderr log.

Implication: the highest-leverage wins are (a) **not re-running boot+menu every iteration**, (b) **reaching the target song time without rendering every intermediate frame**, and (c) **structured/queryable state instead of grepping a 42k-line log**.

### Key mechanical facts discovered
- **Loop is uncapped CPU-bound** (`App.cpp:485-527`, `HX_NATIVE` branch). No throttle. `MILO_AUDIO=1` does **not** gate it to real-time — 6000 frames in 28s, not the ~100s a real-time song would take. So "turn off audio for speed" buys little; the loop is already free-running.
- **Gem/song clock = `MasterAudio::GetTime()` → `mSongStream->GetTime()`** (MasterAudio.cpp:505-513) — the audio-stream playback position, advanced by `TheSynth->Poll()` each frame. **There is no seek/jump API** in the native harness (confirmed: `rb3_waitinguser_gate_native.cpp:18` notes "1500 gameplay frames run to songMs ~1027" — i.e. songMs is reached by *simulating* frames, not seeking).
- **UI/anim clock = wall-clock** (`TaskMgr::Poll` → `mTime.Split()`, Task.cpp:369-376) via `Timer::CyclesToMs`. So UI animation timing is real-cycle-derived, but gem timing rides the synth/audio stream.
- **All tuning today is env-var + rebuild-free-only-if-the-knob-exists.** Existing knobs: `CAM_ROTX`, `CAM_DBG`/`GEM_DBG`/`RENDER_DBG`/`CLOCK_DBG`/`PART_DBG`/`RB3_SCREEN_DBG`/`RB3_INPUT_DEBUG`, `MILO_SCREENSHOT_*`, `RB3_GAME_INPUT`, `RB3_USE_SCENE_CAM`, `RB3_CAM_DIR`, `RB3_ONLY_SHOWING`. These are ad-hoc, read once at startup, and adding a new one is a rebuild.
- **No ccache** (`ccache not found`), **Debug build** (`-g`, no `-O`), **default `ld`** linker (but `lld`/`ld.gold` ARE installed). 977 TUs.
- **rb3-native has none of DC3's interactive tooling** — no `HttpServer`, `DebugPanel`, `NativeSettings`, or `GameplayTelemetry`. Those are all DC3-only today.

---

## 2. DC3 tooling inventory + portability to rb3-native

DC3 built three relevant tooling layers. Source paths are under `/home/free/code/milohax/dc3-decomp/`.

### 2a. HTTP debug server — **the big one**  ✅ portable, high value
- **Files:** `native/src/platform/HttpServer.{h,cpp}` (44 KB cpp), `native/src/platform/DebugPanel.{h,cpp}`, vendored `native/include/httplib.h` (cpp-httplib, single-header, MIT, 683 KB, zero external deps).
- **Design doc:** `docs/sessions/2026-03-25-http-debug-server-design.md` (very thorough; 6 phases, phases 1–4 marked implemented+tested).
- **What it does:** background `std::thread` runs cpp-httplib; HTTP handlers push commands onto a queue; the **main thread drains the queue each frame** (App.cpp:1095 `ProcessCommands()`, screenshots after `EndDrawing` at :1137). 12 live endpoints:
  - `/api/health` (frame, uptime), `/api/screenshot` (PNG of current frame, on demand), `/api/settings` (GET/PUT `NativeSettings` — **live FOV/camera/offset tuning, applied next frame, no rebuild**),
  - `/api/dta/eval` (**execute arbitrary DTA against the live engine** — the killer feature: query/mutate any object via `{$obj find ... set_showing ...}`), `/api/dta/funcs`,
  - `/api/objects`, `/api/scene/tree` (browse the live object graph), `/api/screen`, `/api/frame`,
  - `/api/input/press`, `/api/input/sequence` (inject button presses — same path as `MILO_INPUT_SCRIPT`).
- **Enabled by:** `-DDC3_HTTP_SERVER=1` compile define + `DC3_HTTP=1` env at runtime (off by default), `DC3_HTTP_PORT` (default 9090). Desktop-only (guarded OFF for Emscripten).
- **Portability to rb3-native — HIGH.** Everything it depends on exists in rb3:
  - `DataReadString(const char*)` (DTA eval primitive) is present at `rb3/src/system/obj/DataFile.cpp:615`, with the same `gDataReadCrit` thread-safety.
  - Clean main-loop hook: rb3 `App.cpp:504-525` `HX_NATIVE` loop already calls `RB3GameInputPoll(frame)` (line 507) and `TheRnd->EndDrawing()` (line 523) — drop `ProcessCommands()` next to the input poll and `ProcessScreenshots()` after `EndDrawing`.
  - Screenshot-to-memory already exists (`BandRnd`/`GpuDevice` readback used by `MILO_SCREENSHOT_*`).
  - Input injection already exists and is richer than DC3's (`rb3_game_input.cpp` has `select:`/`msg:`/`track:` directives) — the HTTP layer would just call the same `RB3GameInput*` helpers.
  - rb3 has **no `NativeSettings` struct** yet, so `/api/settings` needs a small rb3 settings struct first (or skip it and use `/api/dta/eval` to mutate objects directly).
- **Port effort:** **~1 day** for phases 1–2 (health + screenshot + dta/eval + input), reusing the DC3 cpp + the vendored header verbatim. Phase 3 (scene tree) another ~half day.
- **Value:** **eliminates the rebuild AND the re-run for most tweaks** — change camera/object state and re-screenshot a *running* instance over HTTP. This is the single highest-ROI item.

### 2b. GPU profiling / frame capture — **portable as external tooling, medium value**
- **What DC3 has:** there is **no in-engine GPU profiler** (the HttpServer's "Phase 6 perf endpoints" `/api/perf/*` were **designed but never implemented** — only `/api/health` returns frame/uptime). GPU debugging in DC3 is **external Vulkan-layer tooling**, documented in `docs/native/TOOLS.md`:
  - **GFXReconstruct** — captures/replays Vulkan API calls at the layer level; **works headless** (no swapchain needed), exactly our case. Capture → JSON-Lines (`gfxrecon-convert`) → grep `vkCmdDraw` counts, extract SPIR-V shaders, dump render targets at a specific draw call, replay with screenshots, hot-swap shaders.
  - **RenderDoc** + `rdc-cli` + a **RenderDoc MCP server** (`../gpu/renderdoc-skill`) exposing 13 tools to Claude Code. RenderDoc needs a swapchain → only useful in **windowed** mode.
  - Helper scripts: `dc3-decomp/scripts/gpu/{capture,inspect,rdc_capture,screenshot}.sh`, `query_trace.py`.
- **Note:** the `../gpu/` checkout (gfxreconstruct/renderdoc built from source) referenced by TOOLS.md does **not** exist in this tree right now (only the scripts do) — it would need building (`cmake … -j$(nproc)`, tens of minutes one-time).
- **Portability:** these tools are **app-agnostic Vulkan layers** — they attach to `rb3-native` (Dawn→Vulkan) with **zero engine changes**. Just point `capture.sh`/the `VK_LAYER_PATH` env at the rb3 binary. The DC3 scripts are copyable nearly verbatim.
- **Value for *current* work:** **medium, situational.** Our current bugs are "is the camera framing right / are gems in-window / are colors right" — those are answered faster by an on-demand screenshot (HTTP) than by a Vulkan trace. GFXReconstruct earns its keep for the harder "why is this mesh invisible / why are colors wrong / what draws happen" class (shader/pipeline/descriptor bugs). Worth wiring the scripts now, building the layer lazily when a draw-level bug appears.

### 2c. `NativeSettings` + ImGui `DebugPanel` — **partially portable, windowed-only for the panel**
- **`NativeSettings`** (`native/src/platform/NativeSettings.h`): a struct of camera knobs (FOV scale, near/far, aspect, forward/height/lateral offset, blend) each overridable by `MILO_CAM_*` env vars at startup, **and** live-mutable via `/api/settings` PUT. This is the **systematic version of rb3's ad-hoc `CAM_ROTX`/`CAM_DBG` env vars.**
- **`DebugPanel`** (`native/src/platform/DebugPanel.cpp`): an ImGui window (toggled with `~`) with sliders for the same knobs — **only useful windowed**; rb3 iteration is headless, so the panel itself is low value, but the **`NativeSettings`-struct pattern** (one struct, env-var init + live setter) is exactly what rb3 should adopt to consolidate its scattered env knobs.
- **rb3 status:** `HX_IMGUI=1` is defined on the engine when GFX is on (so rb3 *could* build the panel), but headless runs never show it. **Adopt the struct, skip the panel** (until/unless we do windowed sessions).

---

## 3. Live-tuning assessment (specific to current work)

- **Consolidate the ad-hoc env knobs into one rb3 `NativeSettings` struct** (mirror DC3). `CAM_ROTX`, the `*_DBG` flags, camera offsets → fields with `RB3_*` env init. Quick win on its own; becomes *live-tunable* the moment the HTTP server lands (`PUT /api/settings`).
- **DTA-over-HTTP is the live console.** rb3 already has `DataReadString` + a full `Handle()`/`DataVariable` object model. `POST /api/dta/eval {"expr":"{<obj> find game.cam ...}"}` lets you nudge camera pose, toggle `set_showing`, query gem state on a **running** instance — no rebuild, no re-run. DC3 verified this exact pattern.
- **DTA hot-reload** (re-read a `.dta` mid-run) was *designed* in DC3 (`/api/dta/upload`) but **never implemented** there (needs overlay-dir write + DataArray cache invalidation). Lower priority — `/api/dta/eval` covers most of the need.
- **Long-running process accepting commands:** this is precisely what the HTTP server provides (it also supersedes the awkward batch `MILO_INPUT_SCRIPT` for interactive nav, per DC3's own findings). Set `MILO_MAX_FRAMES` high, leave one instance running, drive it from `curl`/Python: wait-for-screen, inject input to reach gameplay once, then iterate camera/object tweaks + on-demand screenshots against the *same* booted process.

---

## 4. Faster-run assessment

- **Skip-to-gameplay (boot+menu bypass): worth doing.** Boot→`game_screen` costs frames 0–456 (~2.1s) *plus* the menu-nav `RB3_GAME_INPUT` choreography every run. A fast-boot mode that scripts straight into a loaded song (or keeps a warm instance via the HTTP server) removes this lead-in from every iteration. Cheapest realization: **the HTTP server's keep-alive instance** — pay the 456-frame boot *once*, then iterate forever against the live instance. (A true "construct game state directly" shortcut is more invasive; not recommended first.)
- **Audio gating: NOT a bottleneck.** The loop is already free-running CPU-bound at ~216 fps with `MILO_AUDIO=1`; it is not pinned to real time. Dropping audio won't materially speed the run. (Leaving audio on is fine and keeps the gem/song clock representative.)
- **Frame-skip / clock-seek to a target songMs: high value but higher effort.** The gem clock is `mSongStream->GetTime()` (audio-stream position) with **no seek API**. To jump to songMs=X without simulating every frame you'd need to add a `SetTimeOffset`/stream-seek path (MasterAudio already has `SetTimeOffset`, MasterAudio.cpp:515) and ensure beatmatch/gem state catches up. Feasible but engine-surgery; defer until the cheaper wins are in.
- **Single-frame capture mode: the ideal end state.** boot → load song → seek to songMs=X → render ONE frame → screenshot → exit would be a sub-3s loop. It depends on the clock-seek above. Until then, the HTTP keep-alive instance is the practical equivalent (one boot, many cheap captures).
- **Don't re-render frames you won't screenshot:** today you run to frame 6000 to grab f5600. If you only need f3200, set `MILO_MAX_FRAMES=3260` — saves ~13s. Trivial, do it now.

### Build-speed wins (low priority — build is only 2.8s, but cheap to grab)
- **Switch the linker to `lld`** (`-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld`): the relink is ~1s of the 2.8s; `lld` typically halves link time on a 106 MB binary. 5-minute change.
- **Add ccache** (`-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`): helps full/from-scratch rebuilds and branch switches, not the 1-TU incremental. 5-minute change.
- These are nice-to-haves; the build is not the loop's bottleneck.

---

## 5. Ranked recommendations

Ranked by **(time-saved × frequency) / effort**.

| # | Recommendation | Effort | Per-iteration saving | How to start |
|---|----------------|--------|----------------------|--------------|
| **1** | **Port DC3's HTTP debug server (phases 1–2: health, screenshot, dta/eval, input).** Reuse `HttpServer.{h,cpp}` + vendored `httplib.h` verbatim; hook `ProcessCommands()` at rb3 `App.cpp:507` and `ProcessScreenshots()` after `:523`; gate behind `RB3_HTTP_SERVER` define + `RB3_HTTP=1` env. | **~1 day** | **Removes rebuild (2.8s) AND most re-runs (~26s) for any state tweak** — iterate camera/object/screenshot against a live, already-booted instance. ~25–28s → seconds, many times/session. | Copy the two files + header from `dc3-decomp/native/`; adapt includes; add the two hook calls; verify `curl localhost:9090/api/health` then `/api/screenshot`. |
| **2** | **Consolidate env knobs into an rb3 `NativeSettings` struct** (mirror DC3's). Fold `CAM_ROTX`, camera offsets, the `*_DBG` flags into one struct with `RB3_*` env init + live setters. | **~2–3 hrs** | Modest alone, but it's the **substrate for `PUT /api/settings`** (rec #1) → makes every camera/render knob live-tunable with no rebuild. | Create `native/src/rb3_native_settings.{h,cpp}` from `NativeSettings.h`; route existing getenv sites through it. |
| **3** | **Wire DC3's GPU-capture scripts for rb3-native** (GFXReconstruct, headless). Copy `dc3-decomp/scripts/gpu/*` and point them at `rb3-native`; build the layer lazily when a draw-level bug appears. | **~1 hr** (scripts) + tens of min one-time layer build when needed | Big saving **only for the draw/shader/pipeline class of bugs** ("mesh invisible", "colors wrong") — turns guesswork into `grep vkCmdDraw` + render-target dumps. Zero engine changes. | Copy the scripts; `MILO_*=… scripts/gpu/capture.sh -s <submit-range> rb3-native`; `gfxrecon-convert … | jq`. |

---

## 6. Quick wins (<1 hr) vs investments (>1 day)

**Quick wins (do today):**
- **Pre-`mkdir` the `MILO_SCREENSHOT_DIR`** in the run wrapper (or add a `mkdir -p` to `InitScreenshots`) — silent `WritePNG failed` is purely a missing-dir bug.
- **Cap `MILO_MAX_FRAMES` just past the last screenshot frame** (e.g. `=3260` if you only need f3200) — saves ~0.5s/100 frames skipped; ~13s if you currently over-run to 6000.
- **`-fuse-ld=lld`** in the build config — ~0.5s off every relink (`lld` is already installed).
- **`ccache` launcher** — helps branch-switch / clean rebuilds.
- **Copy the GPU-capture scripts** (rec #3) — usable the moment you build the layer.

**Investments (>1 day):**
- **HTTP debug server port** (rec #1) — the loop-changer.
- **Clock-seek / single-frame capture mode** — add `MasterAudio` seek + beatmatch catch-up so you can jump to songMs=X and render one frame. Highest run-time win, but real engine surgery; sequence it after the HTTP server proves out the interactive workflow.

---

## 7. Next 3 things to build (for the coordinator to dispatch)

1. **Port the HTTP debug server (phases 1–2).** Reuse DC3's `HttpServer.{h,cpp}` + `httplib.h` verbatim; hook the rb3 `HX_NATIVE` frame loop (`App.cpp:507` / after `:523`); gate behind `RB3_HTTP_SERVER`/`RB3_HTTP=1`. Acceptance: against a running `MILO_MAX_FRAMES=999999` instance, `curl /api/screenshot` returns the live frame and `POST /api/dta/eval {"expr":"{<cam-obj> ...}"}` mutates camera state visible in the next screenshot — **no rebuild, no re-run.** *(Coordinate with V13: this only adds new glue files + two call sites in `App.cpp`; it does not touch track/bandobj/`Rnd_Wgpu_RB3.cpp`.)*

2. **Introduce an rb3 `NativeSettings` struct** (mirror DC3) and route the existing `CAM_ROTX`/camera-offset/`*_DBG` getenv sites through it. Acceptance: all current env knobs still work via `RB3_*`, and the struct is PUT-able once the HTTP server lands.

3. **Land the quick wins as a small workflow patch:** `mkdir -p` the screenshot dir in `InitScreenshots`, switch to `-fuse-ld=lld`, add the `ccache` launcher, and check in the copied `scripts/gpu/*` capture helpers pointed at `rb3-native`. Acceptance: screenshots never silently fail; relink time drops; GFXReconstruct capture of `rb3-native` produces a `.gfxr` you can `gfxrecon-convert | jq`.

**Deliberately deferred:** clock-seek / true single-frame mode (engine surgery — do after #1 proves the interactive loop), the windowed ImGui `DebugPanel` (headless workflow doesn't need it), and DTA hot-reload/`/api/dta/upload` (DC3 never finished it; `/api/dta/eval` covers the need).
