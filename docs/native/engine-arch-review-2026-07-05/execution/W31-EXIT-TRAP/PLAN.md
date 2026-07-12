# W31-EXIT-TRAP — Lane C PLAN

**Lane:** C (Opus; hygiene). **KEY:** W31-EXIT-TRAP. **Base SHA (rb3):** fd119705. **Engine pin:** b36bcfc.

## Target (verbatim charter)
The exit-time teardown SIGSEGV (rc=-11/139, sometimes SIGABRT 134) on **bounded non-HTTP boots** that every gate currently tolerates (`drawlog-golden.py:183-190,234-237`, `song-end-test.py:269`, W30 process rule 10). Known class: **Dawn/GPU device teardown at static-dtor order** (W0.3.S1 origin; W1.1/W2.2/W1.4 STATUS refs).

**Do NOT conflate with:**
- (a) the web-release past-score-screen trap (`scripts/native/_exit-trap-test.py`) — NOT mine.
- (b) the W26 **splash→main_hub** panel-unload teardown (A10) — NOT in-song, NOT mine.

## Known state (pre-STEP-0 reading of the code)
- `gBandRnd` is a **global** (`Rnd_Wgpu_RB3.cpp:46 BandRnd gBandRnd;`) whose `GpuDevice mGpu` member holds `wgpu::Device/Queue/Surface/Adapter/Instance`.
- An elaborate teardown-ordering scheme ALREADY exists:
  - `RB3RegisterBandRndShutdown()` (main_native.cpp:637, registered FIRST in RunGame so it runs LAST in Debug::Exit) → `BandRndShutdownExitCallback` → `gBandRnd.Shutdown()` which drops every wgpu ref then `mGpu.Shutdown()`, guarded by `mGpuReady`.
  - `AudioDevice::Suspend()` at frame-loop exit (App.cpp:930) + `RB3AudioTerminateExitCallback` net.
- Bounded path: frame loop exits → `App::~App()` → `TheDebug.Exit(0,true)` → runs callbacks (incl. `gBandRnd.Shutdown()`, sets `mGpuReady=false`) → `exit(0)` → libc static-dtor phase → `~gBandRnd` → `~GpuDevice` → `GpuDevice::Shutdown()` (handles already null; second Shutdown should be a no-op).
- So on paper the trap should already be closed on the clean bounded path — yet rc=-11 persists. **The backtrace must name what actually faults.** Hypotheses to disambiguate with the bt:
  1. Debug::Exit is NOT reached on the bounded non-HTTP path (App is stack-local in RunGame — is its dtor actually run? does `exit(0)` inside Debug::Exit pre-empt it?), so gBandRnd tears down cold in static-dtor phase.
  2. A DIFFERENT static/global (not gBandRnd) holds a live wgpu/Dawn/GLFW/Vulkan handle and destructs after the ICD unmaps.
  3. GLFW `glfwTerminate()` double-call or a Dawn instance-proc-table teardown.

## Steps
- **STEP 0 (checkpointed, lint 10 — BLOCKING, no fix before this):** symbolized backtrace.
  - Own debug build dir `native/build-agent-W31-EXIT-TRAP` (clang, -O0 -g2).
  - Bounded 5-frame non-HTTP boot: `RB3_GAME=1 MILO_HEADLESS=1 RB3_FIXED_CLOCK=1 MILO_MAX_FRAMES=5` (no RB3_HTTP), under `gdb --batch -ex run -ex 'bt full' -ex 'thread apply all bt'`. Also confirm rc with a bare run loop (N/10 baseline).
  - ASan variant if gdb frames are thin.
  - Commit the symbolized transcript text into STATUS; raw gz into evidence/.
- **STEP 1:** From the named dtor chain, ONE fix in the engine gfx teardown path — explicit device/queue/surface teardown ordering preferred. A scoped `_exit`-class bypass is NOT mine to choose; if ordering is priced prohibitive → status=blocked + pricing memo (coordinator decides).
- **STEP 2 (acceptance):** bounded non-HTTP boot exits rc=0 ≥10/10 in MY tree; `rb3-tests` clean; `drawlog-golden.py` + lineup gates PASS unchanged. Do NOT edit `drawlog-golden.py`/`song-end-test.py` (A7 coordinator-owned). No new flags unless behavioral (then default-OFF + class.json append under lock).

## Owned surfaces
Engine gfx teardown TUs (`src/gfx/GpuDevice.cpp`, `src/platform/Rnd_Wgpu_RB3.cpp` Shutdown path, and whatever the bt names). Verify flavor-membership (lint 9): they compile into rb3-native. READ-ONLY: everything else.

## Rider (deferred / may hand to Sonnet)
Web-side hub-menu yellow-highlight capture (A9/A11) — verify deploy freshness first, then `menuhub-probe.mjs`/`keyboard-to-gameplay.mjs` grade vs `a_hubtop_00_focused.png`. Secondary to the exit-trap fix.
