# Wave 03 — perf fixes (panel-stagger + splash venue-cull)

> Orchestrator merges verdicts here into STATE.md at converge. Each `### <label>`
> is one fan-out subagent's evidence.

### P3 perf-baseline

**Purpose.** Establish a CLEAN per-screen frame-time baseline on the CURRENT binary
(`native/build-native/rb3-native`, built **Jun 6 02:54**, which ALREADY contains the audio
limiter — perf is unaffected by it) so `converge` can A/B the two attributed perf fixes
(panel-stagger at UIScreen.cpp:288, and the splash venue-cull render workstream).

**Method.** `scripts/native/frame_profiler.py` over a stable nav
`boot → intro_movie → splash → main_hub → song_select (scroll) → confirm song → part_difficulty → tv3 venue`.
RUN-ONLY (no build). Ran **3×** to gauge run-to-run variance. Raw traces:
`/tmp/rb3-frame-trace-run{1,2,3}.jsonl`. The tracer is `native/src/rb3_frame_trace.cpp`
(per-frame `dt`/`lp`/`lpu`/`ld`/`st`/`pend` + `scr`).

#### Exact reproduction command (what converge runs for an A/B)

```bash
# BASELINE (current binary). Run 2-3× and compare PER-SCREEN PERCENTILES + the isolated
# transition frame, NOT absolute frame indices (capture offset drifts run-to-run).
python3 scripts/native/frame_profiler.py --into-song --scroll 8 --run-secs 18 \
        --worst 20 --trace /tmp/rb3-frame-trace-baseline.jsonl --verbose

# A/B a fix: rebuild (orchestrator only) then re-run with the fix's env knob and a
# distinct --trace, then diff the per-screen table + the ENTER spike. e.g.
RB3_STAGGER_PANELS=1 python3 scripts/native/frame_profiler.py --into-song --scroll 8 \
        --run-secs 18 --trace /tmp/rb3-frame-trace-stagger.jsonl --verbose
# (splash venue-cull would A/B the splash_screen row; its knob is the render workstream's.)

# Re-parse any saved trace without re-launching:
python3 scripts/native/frame_profiler.py --parse-only /tmp/rb3-frame-trace-baseline.jsonl --worst 20
```

#### Per-screen frame ms — CURRENT binary (3 runs, p50/p95/p99/max)

| screen | run1 | run2 | run3 | notes |
|---|---|---|---|---|
| **splash_screen** (n=62) | 69.4 / 87.0 / 98.4 / **196.7** | 67.9 / 83.4 / 97.8 / **188.7** | 69.8 / 85.7 / 103.8 / **194.1** | full venue world.cam draw behind 2D splash; max = 1-time mesh VBO/IBO upload (f22/24). **TARGET of splash venue-cull fix.** |
| **main_hub_screen** (n=158) | 9.7 / 16.6 / 25.5 / 28.9 | 9.2 / 15.8 / 25.5 / 28.4 | 9.6 / 16.6 / 25.5 / 30.1 | hub panel milos; modest, steady |
| song_select_enter_screen (n≈8) | 11.3 / 17.9 / — / 17.9 | 10.9 / 20.0 / — / 20.0 | 10.7 / 20.2 / — / 20.2 | slide-in anim + bgLoad (lp 8-18 ms); precedes the ENTER spike |
| **song_select_screen** (n≈970) | 5.2 / 6.8 / 7.9 / **52.8** | 4.9 / 6.5 / 10.6 / **49.6** | 5.1 / 6.4 / 7.2 / **52.5** | steady scroll is SMOOTH (p95 ~6.5). max = the ENTER spike (isolated below). **TARGET of panel-stagger fix.** |
| **part_difficulty_screen** (n≈430) | 3.6 / 4.8 / 6.0 / 24.5 | 3.4 / 3.9 / 15.1 / 25.1 | 3.4 / 3.8 / 5.7 / 22.9 | smooth; max = the part_difficulty entry frame (LOAD+1 gameplay preload, ~23-25 ms) |
| intro_movie_screen (n=22) | 9.6 / 15.6 / 33.2 / 33.2 | 9.7 / 15.8 / 33.0 / 33.0 | 9.7 / 15.3 / 33.0 / 33.0 | boot movie panel; first frames warming up |
| tv3_*_screen (venue, n≈55) | 9.0 / 9.4 / — / 10.3 | 8.8 / 9.1 / — / 9.9 | 9.8 / 10.2 / — / 10.4 | early venue frames after part_difficulty; SMOOTH (see caveat re: not full gameplay) |

Overall (per run): p50 **5.08 / 4.81 / 4.90**, p95 **12.1 / 14.8 / 15.2**, p99 **79.8 / 77.8 / 79.4**,
max **196.7 / 188.7 / 194.1** ms. Long-frame tail (frames ≥33 ms): **0 ms synchronous drains
(lpu=0) everywhere** — the budgeted loader is NOT the stutter (confirms W2-C / the W3 loader fix
holds). Of ~4.6–4.8 s total long-frame time, ~250 ms is budgeted LoadMgr.Poll (bg), the rest
(~94.6%) is draw/GPU/poll — overwhelmingly the splash venue draw.

#### Isolated song_select-ENTER transition frame (the panel-load spike)

The spike is the **FIRST frame on `song_select_screen`** (the frame where `scr` flips to
`song_select_screen`, tagged **`LOAD+2 pend=1`, `lp=0 lpu=0`**). Frames immediately before it are
the `song_select_enter_screen` slide-in (10-20 ms, bgLoad); the very next frame collapses to ~8 ms,
then ~5 ms steady. So the cost is a single isolated frame, not a sustained region:

```
run1: f250 dt=1.94 (enter, lp=1)  →  f251 dt=52.75 song_select_screen LOAD+2 pend=1  →  f252 dt=8.39  →  f253 5.51 → steady ~5
run2: f249 dt=4.64 (enter, lp=3)  →  f250 dt=49.60 song_select_screen LOAD+2 pend=1  →  f251 dt=8.03  →  f252 5.43 → steady ~5
run3: (same shape)                →       dt=52.52 song_select_screen LOAD+2 pend=1  →  collapse to ~5
```

**ENTER spike = 52.75 / 49.60 / 52.52 ms** across the 3 runs (tight band ~**50-53 ms**). `lp=lpu=0`
on the spike frame ⇒ NOT the budgeted loader and NOT a sync drain — it is the milo decompress+parse+
object-tree build of `song_select.milo` + its ~17 sub-resource Include cascade, all on the single
activate frame (W2-C `UIScreen::LoadPanels` attribution; `src/system/ui/UIScreen.cpp:288`). This is
the A/B target for `RB3_STAGGER_PANELS`; expect it to drop to ~12-15 ms with the rest spread over the
next ~4 frames.

> Note: my measured ~50-53 ms is slightly higher than W2-C's reported 45-48 ms band, but same class
> and same single-frame `LOAD+2` signature. Treat ~50 ms as the current-binary baseline for the A/B.

#### Boot-splash worst frames

| | run1 | run2 | run3 |
|---|---|---|---|
| worst frame (one-time mesh VBO/IBO upload, f22/24) | **196.7** | **188.7** | **194.1** ms |
| 2nd worst | 98.4 | 97.8 | 103.8 ms |
| 3rd worst | 87.1 | 87.5 | 88.4 ms |
| splash p50 (per-frame venue draw) | 69.4 | 67.9 | 69.8 ms |

The splash worst frame is a **one-time spike** (frame ~22-24, the venue mesh buffer upload as world.cam
first traverses the scene), with `bgLoad≈13 ms,pend=4`; the remaining ~60 splash frames are the steady
**~68-90 ms per-frame draw of the full 823-mesh / ~250k-tri venue behind the 2D splash** (W2-C: this is
a render workstream — cull world.cam during splash; expect splash → sub-frame). The bgLoad component is
only 8-16 ms of each splash frame; the bulk is draw/GPU.

#### Variance + A/B guidance (determinism caveats)

- **Run-to-run variance is small and the shape is stable.** splash p50 within ±2 ms (67.9-69.8),
  splash max within ±8 ms (188.7-196.7), ENTER spike within ±3 ms (49.6-52.8), all steady screens
  within ±0.5 ms p50. Three runs is enough to call a >±5 ms per-screen-p50 / >±10% ENTER-spike change
  a real regression/improvement.
- **DO NOT compare absolute frame indices** — the boot/capture offset drifts run-to-run (song_select
  is reached at f276 / f? / f279; the ENTER spike lands at f250 vs f251). **Compare PER-SCREEN
  PERCENTILES** (the table above) and **the isolated ENTER spike** (first `song_select_screen` frame,
  `LOAD+2`), which are offset-independent.
- **What to compare for each fix:**
  - *panel-stagger* (`RB3_STAGGER_PANELS`): the **`song_select_screen` `max`** (= ENTER spike) and the
    first `song_select_screen` frame's `dt`. Baseline ~50-53 ms → target <16 ms. Watch `song_select`
    p95 stays ~6.5 ms (steady scroll must not regress) and `lpu` stays 0.
  - *splash venue-cull* (render workstream knob): the **`splash_screen` p50/p95** (baseline ~68/86 ms
    → target sub-frame ~5-8 ms). The one-time `splash_screen max` (~190 ms mesh upload) may persist or
    move to first venue-load, not vanish — call that separately.
- **The `tv3` venue screen name is non-deterministic** (`tv3_a` / `tv3_b` / `tv3_c` — a random venue
  per run). Aggregate by the `tv3_` prefix when comparing.
- **CAVEAT — sustained in-song gameplay (`game` screen) was NOT reached.** The harness's into-song verb
  sequence lands on `part_difficulty_screen` (holds the whole dwell, 416-446 frames) and only reaches
  the `tv3_*` venue screen at the very end (~50 frames, smooth 9-10 ms); `songMs` stays -1 (song never
  starts playing headless). So there is **no `game`-screen / note-highway frame baseline** in this
  capture — the deepest screen measured is part_difficulty + early venue. If converge needs a true
  gameplay-frame baseline it must fix the into-song nav (the autohit/end_override_flow verbs don't
  launch the song in this headless path) or use a dedicated gameplay harness
  (`scripts/native/song-end-test.py`-style). The two attributed perf fixes (splash, song_select-ENTER)
  are BOTH fully covered by this capture regardless.

**Reproduction summary (for StructuredOutput / converge):**
`python3 scripts/native/frame_profiler.py --into-song --scroll 8 --run-secs 18 --worst 20 --trace /tmp/T.jsonl --verbose`
on `native/build-native/rb3-native`. Compare per-screen percentiles + the isolated first-`song_select_screen`
frame (`LOAD+2`). Determinism: small variance, but absolute frame indices drift — use percentiles.

---

### P1 panel-stagger

**Goal.** Kill the song_select-ENTER 48 ms spike (W2-C) by staggering `UIScreen::LoadPanels()`
so only the focus panel loads on the activate frame and the rest drain one-per-frame.
DESIGN/MEASURE wave: patch is ready-to-apply (below); orchestrator builds.

#### Baseline (re-measured on the built binary, no rebuild)
`python3 scripts/native/frame_profiler.py --scroll 2 --worst 12`:
```
SCREEN-TRANSITION COST (first frame after each screen change):
  frame 250   48.75 ms   song_select_screen   LOAD+2,pend=1   <-- THE SPIKE
PER-SCREEN: song_select  n=3804  p50 5.9  p95 7.0  p99 7.6  max 48.8 ms
```
Reproduces W2-C exactly: one 48.75 ms frame on the song_select ENTER, tagged `LOAD+2`,
`lpu=0` (NOT a sync drain), bg-loader portion small (the spike is milo decompress+parse
of the burst of ~18 nested `.milo` Includes that the 5 panels' `CheckLoad()`s pull in on
one frame). song_select steady scroll is already smooth (p99 7.6 ms).

#### Load-state machine (where the fix hooks)
- `UIManager::GotoScreenImpl` (UI.cpp:665) → `kTransitionTo`, then `mCurrentScreen->Exit(scr)`
  (→ `scr->LoadPanels()` at UIScreen.cpp:239) **or** `scr->LoadPanels()` directly (UI.cpp:689).
  **`LoadPanels()` is the activate-frame call that issues ALL 5 `CheckLoad()`s.**
- Per frame during `kTransitionTo`, `UIManager::Poll()` (UI.cpp:569 & 585) calls
  `mTransitionScreen->CheckIsLoaded()` as the **readiness gate**. While it returns false the
  transition stays in `kTransitionTo` (faster loader period 26.67 ms). When it returns true,
  `mCurrentScreen->Enter(mTransitionScreen)` fires **once** (UI.cpp:583) and advances to
  `kTransitionFrom` (the fade). ⇒ **`CheckIsLoaded()` is the natural per-frame drain hook.**
- `CheckLoad()` (UIPanel.cpp:37) → `Load()` (UIPanel.cpp:72) → `new DirLoader(fp, kLoadBack…)`.
  The DirLoader is budgeted (kLoadBack; `LoadMgr::Poll` 8 ms/frame), but issuing 5 at once on
  one frame is what cascades the parse burst onto the activate frame.
- **Readiness invariant:** `PanelRef::Active() == mActive && mLoaded`. EVERY per-frame loop
  (Entering/Exiting/Poll/Enter/Draw/CheckIsLoaded/IsLoaded/AllPanelsDown) gates on `Active()`.
  So `mLoaded` is the master "this panel participates" flag — a panel with `mLoaded=false` is
  invisible to all of them. That is exactly what lets us DEFER a panel safely.

#### Fix mechanism (deadlock-free, no header change)
1. **Staggered `LoadPanels()`** (HX_NATIVE, `RB3_STAGGER_PANELS`): for each panel that the
   Wii path would load (`mAlwaysLoad || IsReferenced()`), `CheckLoad()`+`mLoaded=true` ONLY if
   it is the focus panel, already referenced (shared/already-loaded, e.g. `meta`), or already
   non-`kUnloaded`. Every other should-load panel is **deferred**: left `mLoaded=false` (so it
   is NOT yet `Active()`).
2. **`CheckIsLoaded()` drain** (HX_NATIVE, same env): before the normal readiness loop, scan
   for a deferred should-load panel (`!mLoaded && (mAlwaysLoad||IsReferenced())`); promote at
   most ONE per RENDERED frame (`DbgGetFrameID()` guard — `CheckIsLoaded` is called 1-2x/frame)
   by calling its `CheckLoad()`+`mLoaded=true`. **Return false while ANY deferred panel is still
   un-promoted-or-unfinished**, so the transition stays in `kTransitionTo` and keeps polling us.
3. When the queue is empty AND every promoted panel's own `CheckIsLoaded()` (→`FinishLoad()`,
   state→`kDown`) is true, the drain falls through to the original readiness loop → returns true
   → `Enter()` runs once, and by then **all 5 panels are `mLoaded=true`** so `Enter()` enters
   every `kDown` panel. No deadlock, all panels present.

#### Correctness reasoning (verified by reading every call site)
- **No early-ready:** the explicit `if (anyPending) return false;` blocks the transition while
  any deferred panel is pending — the screen cannot fade in before all panels finish. The fade
  (`kTransitionFrom`) runs only after `CheckIsLoaded()`==true, identical to today.
- **No deadlock:** every deferred should-load panel is promoted within ≤ (N-1) frames (one per
  frame); once promoted it loads on the budgeted path; `CheckIsLoaded()` only returns true after
  all finish. The `DbgGetFrameID()==0` case (counter not yet live pre-Rnd-init) falls back to
  one-promotion-per-CALL (`frameGuardOk = frame==0 || frame!=last`) — still drains, never stalls.
- **No synchronous-load assumer:** both `LoadPanels()` callers (UIScreen.cpp:239 `Exit`,
  UI.cpp:689 `GotoScreenImpl`) are immediately followed by the `CheckIsLoaded()`-gated state
  machine; `AllPanelsDown()` (UI.cpp:577) runs only inside the `CheckIsLoaded()==true` branch.
  `BandScreen::LoadPanels` → `LoadInterstitials()` loads its OWN `mExtraPanels` synchronously
  (separate list, not `mPanelList`) — untouched. `BandScreen::CheckIsLoaded` forwards to
  `UIScreen::CheckIsLoaded` (BandScreen.cpp:61), so song_select (a BandScreen) gets the drain.
- **Fade-in tolerates multi-frame load:** yes — load completes BEFORE the fade starts (gate is
  upstream of `kTransitionFrom`). User sees the OLD screen ~3 extra frames, then a smooth fade.
- **Other screens:** a 1-panel screen (panel==focus) loads it on the activate frame → identical
  to today (no deferral). Heavy screens (hub, etc.) get the same latency smoothing for free.

#### Default = ON (opt-out `RB3_STAGGER_PANELS=0`)
Justified: pure latency win, no visual change once loaded (deferred panels are off-screen
scaffolding on frame 1 per W2-C), and it is the perf fix this wave ships. A/B with
`RB3_STAGGER_PANELS=0` to recover the old all-at-once behavior.

#### DC3 impact = NONE
`rb3-native` compiles **RB3's own** `src/system/ui/UIScreen.cpp` (native/CMakeLists.txt:261
`file(GLOB ENGINE_UI ${REPO_ROOT}/src/system/ui/*.cpp)`; REPO_ROOT = rb3 repo). The shared
`milo-native-engine` has NO UIScreen.cpp — DC3 compiles its OWN independent copy
(`dc3-decomp/src/system/ui/UIScreen.cpp`, 17896 B). So this edit reaches neither DC3 native
nor the shared engine. The only cross-target file is the Wii DECOMP build, kept byte-identical
by the `#ifdef HX_NATIVE` gate (the original `LoadPanels`/`CheckIsLoaded` bodies are preserved
verbatim as the fall-through; the staggered arms compile only under HX_NATIVE).

#### Prediction + how to measure
- **Activate frame (250 today): 48.75 ms → < 16 ms** (focus-panel-only cost ≈ first ~12-15 ms
  of the burst, per W2-C). The remaining ~33 ms of milo parse spreads over the next ~3-4 frames,
  each bounded by the 8 ms `LoadMgr::Poll` background budget → **no frame > 16 ms on the ENTER**.
- song_select steady-scroll p95 (~7 ms) must NOT regress; `lpu` must stay 0 (W3 loader fix holds).
- **Measure:** `python3 scripts/native/frame_profiler.py --scroll 2 --worst 12` before/after.
  Compare the isolated first `song_select_screen` transition frame (the `LOAD+2,pend=1` line in
  "SCREEN-TRANSITION COST") and `song_select` per-screen `max`. A/B the env:
  `RB3_STAGGER_PANELS=0` (old 48 ms) vs unset/`=1` (staggered, <16 ms). Confirm the screen still
  reaches `song_select_screen` (no deadlock) and steady scroll p95 unchanged.

#### Files touched
- `src/system/ui/UIScreen.cpp` — **DECOMP file** (matched vs Wii). Whole change is `#ifdef
  HX_NATIVE` + `RB3_STAGGER_PANELS`; Wii build byte-identical. (RB3-native-only; not the shared
  engine, not DC3.)

---

### P2 splash-venue-cull

**TL;DR — the W2-C premise is FACTUALLY WRONG, and the gate it asked for is UNSAFE.**
splash_screen does NOT declare "only a 2D splash_panel + MoviePanel / no 3D world." It declares
`(panels meta sv8_panel splash_panel)`, and `sv8_panel` is a **`BackdropPanel` that loads the sv8
cityscape shell-vignette venue** (`world/vignette/shell/sv8/a/cityscape`). The cityscape IS the
intended visible backdrop behind the "ROCK BAND 3 / PRESS START" logo — verified by screenshot.
A blanket "skip world.cam venue draw while screen == splash_screen" would BLANK the backdrop
(proven below: the splash goes fully black). The correct, safe win is NOT a cull-by-screen; it is
**restoring per-drawable frustum culling for the venue draw under world.cam** (disabled wholesale
on native), env-gated DEFAULT-OFF until A/B-verified.

#### 1. The exact draw path (traced)

`UIManager::Draw()` (`src/system/ui/UI.cpp:634`) → `UIScreen::Draw()` (`UIScreen.cpp:165`) →
each active panel's `UIPanel::Draw()` → `PanelDir::DrawShowing()` (`src/system/ui/PanelDir.cpp:133`)
draws back/front panels + `RndDir::DrawShowing()`. The venue is the `sv8_panel` `BackdropPanel`,
whose world (`world/vignette/shell/sv8/a/cityscape`) is traversed by **`WorldDir::DrawShowing()`**
(`src/system/world/Dir.cpp:393`): it `Select()`s the world camera (`world.cam`, pos 110,60,300),
selects the venue environ, then `RndDir::DrawShowing()` enumerates the venue draw graph — the 823
cityscape meshes. (The in-song venue path `BandDirector::DrawShowing → mCurWorld->DrawShowing()`,
`BandDirector.cpp:316/427`, is the SAME `WorldDir::DrawShowing` for gameplay venues; on splash it
is the sv8 vignette WorldDir reached via the `sv8_panel` PanelDir, not BandDirector.)

The RENDER_DBG counter W2-C used (`Rnd_Wgpu_RB3.cpp:1172`, `mDrawnMeshes`/`mDrawnTris`) sits at the
bottom of `BandRnd::EndFrame()` — it only TALLIES what `DrawMesh` was handed; it does not issue the
draw. The issuer is `WorldDir::DrawShowing` feeding each cityscape `RndMesh` through
`RndDrawable::Draw()` → `DrawShowing()` → `BandRnd::DrawMesh`.

#### 2. Repro / evidence (RAN the pre-built `native/build-native/rb3-native`, Jun 6 02:54)

Headless (`RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=…/orig-assets/extracted RB3_HTTP=1 RENDER_DBG=1
RB3_FRAME_TRACE=…`). Frame-trace `scr` joined to RENDER_DBG cam tally:

| frames | scr | cam | meshes | tris | dt |
|---|---|---|---|---|---|
| 0–21 | intro_movie_screen | [default cam] | 0 | 0 | 8–12 ms |
| 22 | **splash_screen** | [default]→f23 world.cam | 0→983 | 0→257k | **185.0 ms** (one-time mesh VBO upload) |
| 23–141 | **splash_screen** | **world.cam** (110,60,300) | **823** | **220k–257k** | **62–83 ms steady (~14 fps)** |

Tri count VARIES per frame (220 815 → 257 190) ⇒ the cityscape is an **animated looping vignette**
(`cityscape_clips.milo`), not a frozen still. Cam tally over the splash window: 207 `world.cam`
frames vs 23 `[default cam]` (intro). Reproduces W2-C's "823 meshes / ~250k tris / world.cam every
frame" exactly.

**Screenshot (`/api/screenshot`) on splash_screen:** the "ROCK BAND 3 / PRESS START" logo over a
fully-rendered 3D night cityscape skyline — the retail RB3 splash. The venue is CORRECT, INTENDED,
VISIBLE. (saved `/tmp/p2_splash.png`, 2.16 MB.)

**A/B SAFETY PROOF — `RB3_SKIP_STATIC=1` (skips static venue meshes):** splash goes **completely
black** (screenshot 38 KB vs 2.16 MB; even the 2D logo disappears — it is composited in the same
world pass). ⇒ the cityscape venue draw is LOAD-BEARING for the splash image. **Any gate that skips
the world.cam venue draw on splash_screen blanks the screen. The W2-C-specified gate is UNSAFE and
must NOT be implemented.** (saved `/tmp/p2_splash_skipstatic.png`, black.)

#### 3. splash.dta ground truth (refutes the W2-C "no 3D world" premise)

`ui/splash/splash.dta:171` — `{new BandScreen splash_screen (panels meta sv8_panel splash_panel)
(focus splash_panel)}`. `sv8_panel` is created by `new_shell_vignette "sv8"`
(`ui/vignettes.dta:183-194`) as a **`BackdropPanel`** whose `(file …)` resolves to
`../world/vignette/shell/sv8_a.milo` (the cityscape — the same milo the main-hub "black void" fix
at `Draw.cpp:18` names). So the venue on splash is DECLARED content, not a native pre-warm artifact.
(`meta` = `meta_panel.milo`, 2D overlay; `splash_panel` = `splash.milo`, press-start button + logo.)

#### 4. ROOT CAUSE of the cost (the real, safe fix target)

`RndDrawable::Draw()` and `RndDrawable::DrawBudget()` (`src/system/rndobj/Draw.cpp:165,183`)
**disable frustum culling entirely on the native build** under `#ifdef HX_NATIVE` ("the WebGPU
renderer's camera/frustum setup does not yet match RndCam::sCurrent's, so culling here would
wrongly drop visible drawables. Over-draw is harmless."). On the Wii each of the 823 cityscape
meshes is `CompareSphereToWorld`-culled per drawable (`s > mWorldFrustum`, `Cam.h:63`); natively
**every cityscape mesh is drawn whether or not it is in the world.cam frustum.** The cityscape is a
wide world and world.cam frames a subset → a meaningful fraction of the 823 draws are off-screen.
The cost is "823 draws / 250k tris with NO culling," NOT "a venue that shouldn't be there."
(`mWorldFrustum` IS maintained natively: `Cam.cpp:187` `Multiply(mLocalFrustum, world,
mWorldFrustum)` runs in the cam world-xfm update; the disable comment is a stale early-port concern
about WebGPU projection handedness, not about `mWorldFrustum` being unpopulated.)

Headless amplifier (NOT the production cost): `GpuDevice.cpp:403-413` does a per-frame `Submit` +
`OnSubmittedWorkDone WaitAny` blocking sync for the screenshot-readback path, so headless CPU-blocks
on the full GPU workload. A windowed/web build uses `PresentFrame()` (Fifo vsync, `GpuDevice.cpp:308`)
and does not CPU-block the same way — part of the 62–83 ms steady is a headless measurement
artifact, but the 823-draw / 250k-tri GPU work is real either way.

#### 5. THE FIX — venue frustum-cull at the existing `MenuVoidDrawHook` seam

`MenuVoidDrawHook(RndDrawable*)` (`Draw.cpp:63`) is already an HX_NATIVE per-drawable "should I skip
this draw?" gate called from BOTH `Draw()` (line 168) and `DrawBudget()` (line 188). Add a sibling
skip-test: when `RB3_VENUE_FRUSTUM_CULL` is enabled AND the current cam is `world.cam` (the venue
cam — NOT the 2D UI cam, NOT game.cam highway) AND the drawable's world sphere is outside the
world.cam frustum, skip the draw. Scoped to world.cam so it can NEVER touch the 2D splash logo /
meta overlay (UI cam), the gameplay highway (game.cam), or any HUD. DEFAULT-OFF so the shipped
build is behavior-identical until the orchestrator A/B-verifies the splash screenshot is unchanged
(cityscape fully present) with the flag on, then flips the default.

- **File:** `src/system/rndobj/Draw.cpp` — **DECOMP file** (matched vs Wii). The whole change lives
  inside the existing `#ifdef HX_NATIVE` block; the `#else` (Wii) path is byte-identical → no
  decomp/match impact.
- **DC3 impact:** none. The change is in RB3's decomp tree (`rb3/src/system/`), not the shared
  `milo-native-engine`. DC3 has its own `Draw.cpp`. The shared-engine renderer is untouched (no
  `Rnd_Wgpu*` edit). DC3 cannot be affected by construction.
- **Why default-OFF, not default-ON:** the disable comment warns culling "wrongly drops visible
  drawables." That risk is real and unverifiable design-only (no build). Land dark + A/B
  (screenshot diff splash + main_hub + an in-song venue, flag on vs off); flip the default only
  after with-flag == without-flag minus genuinely off-screen meshes.

**Predicted win (flag ON):** splash steady 62–83 ms → the frustum-visible fraction of the 823
meshes (cityscape is wide; a large share is off-screen). Even a 40–60% off-screen cull lands GPU
work well under the draw-bound steady cost; with the headless-sync caveat the windowed/web splash
should reach sub-16 ms. The one-time frame-22/24 185 ms mesh-VBO-upload spike RELOCATES, not
vanishes — it is the first-traverse buffer upload for whatever venue is drawn; with culling it
uploads only frustum-visible meshes on first traverse, and a later-revealed mesh uploads when it
first enters frame. Measure: `RENDER_DBG=1` (watch `mDrawnMeshes` fall on splash) +
`frame_profiler.py` (splash p50 62→target) + `/api/screenshot` A/B (must lose nothing visible).

**Fallback if frustum-cull drops visible meshes in A/B:** the only other SAFE lever is a
venue-backdrop redraw decimation (render the static sv8 vignette every Nth frame into a cached
view; composite the 2D logo every frame). That needs a backdrop-framebuffer cache (engine
`Rnd_Wgpu_RB3.cpp`, native-only) — higher-risk/more-code; defer unless frustum-cull fails.

**DO NOT** implement "skip world.cam while screen == splash_screen" — proven to blank the backdrop.

#### 6. Exact patch — `src/system/rndobj/Draw.cpp`

Add helper after `MenuVoidDrawHook` (before the closing `#endif` at line 156), then one call line
in each of `Draw()` and `DrawBudget()` (after the existing `MenuVoidDrawHook` check). Exact
old/new strings in StructuredOutput `exactPatch`.

---

### W3-VERIFY splash-venue-cull (adversarial — REFUTE that the perf patch is SAFE+EFFECTIVE)

Independent re-read of every cited line + RAN the pre-built `native/build-native/rb3-native`
(no build). Verdict per gate below. **Overall: NOT REFUTED on safety/decomp/DC3; PARTIALLY
REFUTED on the "will move the ms" effectiveness claim (unverifiable without a build — but the
patch is DEFAULT-OFF, so shipping it changes nothing until the orchestrator A/B-flips it).**

**(a) Wii decomp byte-identical — CONFIRMED.** `src/system/rndobj/Draw.cpp` is RB3's OWN decomp
copy (15385 B, already carries MenuVoidDrawHook + SMASHER_DRAW_FIX under HX_NATIVE). All three
patch sites land strictly inside existing `#ifdef HX_NATIVE … #endif` blocks: Site-1 helper before
the first `#endif` (line 156, the top include/helper block); Site-2/3 inside the `#ifdef HX_NATIVE`
arms of `Draw()`/`DrawBudget()`. Bare `#endif`s at 156/179/206/225/252/332; the `#else`/Wii arms
are untouched. Wii compile sees none of it → byte-identical → 0 match% impact. Includes for the new
code (`<cstdlib>` getenv, `<cstring>` std::strcmp, `math/Geo.h` Sphere, `rndobj/Cam.h`
RndCam/CompareSphereToWorld/sCurrent) are ALL already present (Draw.cpp:4,11,12 + the unconditional
Cam.h at :2). Anchors verified unique: Site-2 `…return;` only at L168, Site-3 `…return true;` only
at L188. Compiles in scope.

**(b) Can it blank a screen / break load-state — NO (the cull is the ORIGINAL Wii cull, reused).**
The patch does NOT invent a frustum test; it calls the SAME `MakeWorldSphere(s,false)` +
`RndCam::CompareSphereToWorld(s)` pair the Wii `#else` arm uses (Draw.cpp:175-176/201-202). Read the
primitives: `MakeWorldSphere(s,false)` returns false when `mSphere.GetRadius()==0` → patch treats as
"never cull" (Mesh.cpp). `RndMesh::UpdateSphere()` zeroes the sphere for **boned** meshes
(`!mBones.empty()→s.Zero()`) so all skinned/char meshes are NEVER culled (safe). For static meshes,
`Multiply(mSphere, WorldXfm(), s)` puts the world sphere at the TRUE rendered location, not the mesh
origin. `operator>(Sphere,Frustum)` (Geo.cpp:932) keeps anything whose center is within `radius` of
every plane → huge backdrop/sky spheres are conservatively KEPT. It touches NO load-state machine
(pure draw-time skip; `mShowing`/loaders/CheckLoad untouched). world.cam scoping (verified, below) means
it can never reach the 2D splash logo (UI cam), highway (game.cam), or HUD.
- **RESIDUAL RISK (real, why default-off is load-bearing):** the cull frustum is `RndCam::mWorldFrustum`,
  built from `cam->WorldXfm()` + `mLocalFrustum.Set(near,far,yFov,ratio)` where
  `ratio=(rect.h/rect.w)*YRatio()` (Cam.cpp:155-161, YRatio()∈{1,.75,.5625}). The native GPU viewProj
  (Rnd_Wgpu_RB3.cpp:838-913) is built from the SAME eye/fwd/up/yFov/near/far BUT a DIFFERENT horizontal
  term: `aspect = WindowWidth()/WindowHeight()` AND a runtime `NativeSettings::fovScale` knob
  (default 1.0). If the native window aspect ≠ the frustum `ratio`'s implied aspect, OR fovScale≠1, the
  cull frustum is narrower/wider than what is actually drawn → a narrower cull would clip meshes that
  ARE on screen. I FOUND a concrete tripwire: `moon.mesh` renders visibly top-left of the splash
  (confirmed in /api/screenshot) yet its `WorldXfm().v` origin is at x=-4458,y=+2342 (camera at
  x=110.76); whether the moon survives depends entirely on its `mSphere` radius vs a CORRECT frustum.
  The original native disable comment ("WebGPU frustum doesn't match RndCam::sCurrent's") is exactly
  this concern; I could NOT rebuild to confirm the frustum math now matches → cannot prove zero
  visible-mesh loss. The patch's mitigation (DEFAULT-OFF env gate + mandatory A/B of splash/main_hub/
  in-song screenshots before flipping) is the correct and ONLY safe disposition. Gate logic verified
  correct by inspection: no env / env=="0" → sOn=0 → returns false → never culls → shipped build
  behavior-identical.

**(c) DC3 regression — IMPOSSIBLE.** Draw.cpp is RB3-tree-local. DC3 has its OWN
`../dc3-decomp/src/system/rndobj/Draw.cpp` (6771 B, no MenuVoidDrawHook); the shared
`milo-native-engine` has NO Draw.cpp. The patch cannot reach DC3's TU. (Contrast the audio fix, which
IS in the shared engine — this one is not.)

**(d) Will it move the target frame ms — PLAUSIBLE but UNVERIFIED (the one weak gate).**
RAN the binary, RENDER_DBG=1: the W2-C premise reproduces EXACTLY — splash is `cam=world.cam`
(163/186 frames) pos (110.76,60.11,299.97), steady **823 meshes / ~223k tris** every frame (frame 23
peak 983/257k = the mesh-upload frame). Screenshot is 2.15 MB (full cityscape: rooftops + moon + sky),
NOT the 38 KB black of RB3_SKIP_STATIC → venue draw is load-bearing, a screen-name cull WOULD blank it
(patch's reframe is correct; it does the RIGHT, geometry-level thing instead). MENU_VOID_DBG2=2 mesh
dump: of 235 keyed backdrop meshes, world-X spans **-6771…+409** (median -1449, camera at 110.76);
**122/235 are >1500 units off-axis**. So a large fraction of backdrop geometry has far-off origins —
IF those are genuinely outside a correct frustum, the cull is a big win. BUT `moon.mesh` (origin
x=-4458) is VISIBLE on screen → far-origin ≠ off-screen for this venue; the true cull rate is a
function of per-mesh `mSphere` radius + frustum correctness, neither measurable without a build. Net:
effectiveness is **directionally credible** (lots of wide off-axis geometry that is currently
force-drawn) but the specific "823 → big drop, splash sub-frame" magnitude is UNPROVEN. The `b=false`
cull is also a no-op for any static mesh whose `mSphere` was never computed (radius 0 → never cull) —
so the real-world cull rate could be lower than the off-axis-origin histogram suggests.

**Side concern (minor, not a refutation):** `RB3VenueFrustumCull` calls `RndCam::sCurrent` per drawable
and `std::strcmp(name,"world.cam")` BEFORE any sphere work → on non-venue cams (UI/game/HUD) it
short-circuits at the strcmp, negligible. On world.cam it adds one virtual `MakeWorldSphere` +
~6 plane dots per drawable — far cheaper than the draw it elides. No per-frame string alloc. Fine.

**VERDICT: NOT REFUTED as a SAFE-TO-LAND change** (decomp byte-identical ✓, DC3-isolated ✓, default-off
so shipped behavior unchanged ✓, reuses the proven Wii cull primitives ✓, world.cam-scoped so UI/HUD/
highway untouchable ✓). **REFUTED only on the unconditional "it WILL move the ms / splash goes
sub-frame" claim** — that is unverifiable without a build, and there is a concrete frustum-mismatch
tripwire (native GPU aspect/fovScale vs RndCam mWorldFrustum; moon.mesh is the canary) that COULD drop
visible backdrop meshes if the frustum math still diverges. The patch correctly hedges this with
default-off + mandatory A/B. **Disposition: SAFE to APPLY (it's inert until enabled); the orchestrator
MUST, with the flag ON, A/B /api/screenshot at splash + main_hub + in-song venue (assert the moon, sky
gradient, skyline, and all visible backdrop survive AND meshes/tris drop in RENDER_DBG) BEFORE flipping
the default. Do NOT flip the default on the strength of this spec alone.**

---

### W3-VERIFY panel-stagger (adversarial — REFUTE that the patch is SAFE+EFFECTIVE)

Independent read of all cited code + a RUN of the (unpatched) `rb3-native` (built Jun 6 02:54)
under `frame_profiler.py` to attribute the 50 ms frame. Verdict on each gate below.

**(a) Wii decomp byte-identical — CONFIRMED SAFE.** Every line is inside `#ifdef HX_NATIVE`
(helper block, the staggered LoadPanels arm with `return;`, the CheckIsLoaded drain). The
non-HX_NATIVE fall-through bodies in both functions are character-for-character the current
`for` loops (verified against UIScreen.cpp:288-299 and 309-329). `getenv` already compiles in
this exact TU under HX_NATIVE (existing UISCREEN_DBG blocks at lines 106, 313). The local
`int DbgGetFrameID();` fwd-decl is the ONLY declaration in this TU (os/Debug.h declares only
`gpDbgFrameID`, NOT the function) and matches the real def in os/Debug.cpp:34 — legal, no ODR
conflict. `gpDbgFrameID` IS live during song_select (Rnd ctor sets it, Rnd.cpp:230; rendering
works), so the frame-guard path (one-per-rendered-frame) is the active path, not the
one-per-call fallback. NO Wii-build risk.

**(b) Screen never ready / blank screen — NOT A NEW RISK, but two real edge cases.**
- The happy path is sound: `CheckIsLoaded()` returns false while any deferred should-load panel
  has `mLoaded==false`, promoting ≤1/frame; once drained it falls into the original loop which
  drives each promoted panel's `UIPanel::CheckIsLoaded()`→`PollForLoading`→`FinishLoad`→`kDown`.
  The `kTransitionTo` gate (UI.cpp:569) only fires `Enter()` (UI.cpp:583) when CheckIsLoaded()
  is true, by which point every should-load panel has `mLoaded=true`; `Enter()` (UIScreen.cpp:152)
  enters every `Active()&&kDown` panel. No half-loaded fade. The deadlock claim is correctly
  ruled out (bounded ≤N-1 frames; DbgGetFrameID()==0 fallback drains one-per-call).
- EDGE 1 (pre-existing, NOT worsened): `UIPanel::Load()` MILO_FAILs if `mState!=kUnloaded`
  (UIPanel.cpp:74). The drain calls `CheckLoad()` which only calls `Load()` on the
  `++mLoadRefs==1` edge — so a re-promotion of an already-ref'd panel does NOT re-Load. The
  staggered LoadPanels `loadNow` predicate intentionally force-loads any panel with
  `GetState()!=kUnloaded || IsReferenced()` on the activate frame (so it's never deferred into
  a CheckLoad-with-refs==2 path). This is the SAME unconditional-CheckLoad behavior the original
  LoadPanels already has every call. No new FAIL surface.
- EDGE 2 (real, UNADDRESSED by the patch, but LOW severity): `UIManager::CancelTransition()`
  (UI.cpp:644) during a kTransitionTo stagger abandons the transition screen — it Enters the
  OLD `mCurrentScreen`, not the half-staggered one (the screen-swap only happens in the success
  branch). The half-promoted deferred panels of the abandoned screen keep their `mLoadRefs`
  (a transient ref leak), but the screen is discarded; re-navigating re-runs LoadPanels (whose
  predicate now sees IsReferenced()==true → loads them). NO crash, NO stuck screen — identical
  in kind to the original (the original also leaves loaders mid-flight on cancel). Acceptable.

**(c) DC3 regression — CONFIRMED SAFE.** The entire change is HX_NATIVE-gated AND lives in the
RB3 tree's `src/system/ui/UIScreen.cpp` (a decomp file compiled into RB3, NOT the shared
`../milo-native-engine`). DC3 has its OWN UIScreen.cpp. The shared engine is untouched. No DC3
surface at all. (Contrast the audio fix, which IS in the shared engine — this perf patch is not.)

**(d) Will it move the target frame ms — REFUTED (the patch defers the WRONG panels).**
This is the decisive finding. Reproduced the spike on the live binary: song_select activate
frame (f251) = **dt 56.8 ms, lp=0.0, lpu=0.0, ld=2, pend=1**; frames f252-262 immediately
return to ~5-6 ms with lp≈0. So:
- The 56 ms is **NOT in the budgeted loader** (LoadMgr.Poll lp=0) and **NOT a sync drain**
  (lpu=0). It is synchronous milo Include parse during the screen-activate (`Enter`/`LoadPanels`
  path), exactly as W2-C said — BUT it does NOT "drain over the next ~4 frames": f252+ are flat
  ~5 ms with lp≈0. Nothing meaningful spreads. So W2-C's "remaining ~33 ms spreads over the next
  ~4 frames at ~8 ms/frame" prediction is itself wrong against this run — the cost is a single
  synchronous burst, not budgeted tail.
- Panel→milo mapping (orig-assets/.../ui/song_select/gen, sizes verified): FOCUS panel
  `song_select_panel` → `song_select.milo_xbox` (**2.82 MB**) which INTERNALLY Includes
  `song_select_details.milo` (0.34 MB) + leaderboards/widespinner/score_display/star_display/
  mini_leaderboard/header_performance/icons_esrb/setlist/browser (all confirmed via `strings`
  INSIDE song_select.milo_xbox and its details include). Those are precisely the W2-C burst
  items at +0..47 ms. DEFERRED panels = `song_select_shortcut.milo_xbox` (0.33 MB) +
  `song_select_filter.milo_xbox` (3.01 MB) — the +53..85 ms tail W2-C itself says drains LATER.
- The patch loads the FOCUS panel (and `meta`=meta_panel.milo, and any IsReferenced/loaded panel)
  on the activate frame and defers shortcut+filter. But the **focus tree (3.16 MB: song_select
  2.82 + details 0.34) is ~half the bytes and is the +0..47 ms bulk of the 56 ms burst** — and
  it is NOT deferred. shortcut+filter (3.34 MB) is the part that, per both W2-C and this run, is
  already off the critical 56 ms frame (the frame ends before they parse; f252+ show lp≈0, so
  they aren't even budget-loading much). Deferring already-late work cannot move the 56 ms frame.
- Therefore the claimed "48 ms → ~12-15 ms" is NOT supported. Best case the patch trims only the
  fraction of shortcut(+early filter) that happens to land inside the 56 ms window; the dominant
  focus-panel synchronous parse (song_select.milo 2.82 MB + details + its leaderboard/widespinner
  Includes) remains on the activate frame. Expected real effect: modest (a few ms at most),
  NOT the ~36 ms the doc predicts. **EFFECTIVENESS REFUTED.**

**Root-cause mismatch.** The 56 ms is a *synchronous milo-parse of the focus panel's own 2.82 MB
.milo + inline Includes*, not "5 panels loaded in one frame." A panel-count stagger targets the
wrong axis. The cost-bearing unit is ONE panel (the focus), whose milo is the heavy one. To move
this frame you must either (i) split song_select.milo's inline Includes (leaderboards / score /
star / mini_leaderboard) into separately-deferrable sub-loads (the W2-C "alternative: split the
milo resource list" path), or (ii) make the focus panel's milo Include parse budgeted/async
rather than synchronous — neither of which this patch does.

**Net verdict: REFUTED.** Safe (a/b/c all hold; HX_NATIVE-gated, RB3-only, no DC3, no Wii impact,
no deadlock, no blank screen — only a benign pre-existing cancel-path ref-leak). But INEFFECTIVE
on the target: it defers shortcut+filter (the already-late tail) while the dominant 56 ms is the
focus panel's synchronous 2.82 MB milo parse that the patch loads on the same activate frame.
Measured baseline f251=56.8 ms with lp=lpu=0 and a FLAT ~5 ms tail proves the cost is one
synchronous burst dominated by the non-deferred focus panel. The frame will not drop to ~12-15 ms.
Evidence: /tmp/rb3-frame-trace.jsonl (f248-262), milo sizes above. To actually move it, target the
focus panel's milo Include tree, not the panel list.

### W3-VERIFY panel-stagger #2 (independent adversarial — game_screen safety dimension)

Second independent verifier. Read all cited code from scratch + RAN the unpatched `rb3-native`
(built Jun 6 02:54) through `frame_profiler.py` to song_select AND `--into-song` (game_screen).
Confirms the prior W3-VERIFY effectiveness refutation and adds a NEW safety finding it missed.

**File classification (confirmed).** All edits in `src/system/ui/UIScreen.cpp` — a DECOMP file in
the RB3 tree (matched vs Wii), NOT shared `../milo-native-engine`. Both edits are fully
`#ifdef HX_NATIVE`-gated; the non-HX_NATIVE fall-through is char-for-char the originals (verified
vs UIScreen.cpp:288-299 and 309-329). `getenv` already compiles here under HX_NATIVE (lines 106,
313). `int DbgGetFrameID();` fwd-decl is NOT in os/Debug.h (only `extern int *gpDbgFrameID`) →
needed, legal, matches os/Debug.cpp:34 (same pattern as rndwii/Mat.cpp:83). **(a) Wii byte-identical
= SAFE.** **(c) DC3 = SAFE** (RB3-only decomp TU; DC3 has its own UIScreen.cpp; shared engine
untouched). MILO_DEBUG IS defined in the native build (native/CMakeLists.txt:371), so
`gpDbgFrameID = &mFrameID` (Rnd.cpp:230, `#ifdef MILO_DEBUG`) IS live → the one-per-RENDERED-frame
guard is the active path, not the one-per-call fallback. mFrameID++ once/frame (Rnd.cpp:773);
frames advance during transitions (W2-C: venue draws every frame) → guard advances, no stall.

**NEW SAFETY FINDING — the patch is DEFAULT-ON for EVERY screen, including `game_screen`, which
routes through the fragile SyncGameStartPanel handshake.** The prior verifier only analyzed
song_select. But `RB3_STAGGER_PANELS` defaults ON, so LoadPanels+CheckIsLoaded are restructured
for ALL screens. The dangerous one:
- `game_screen` (game.dta:266-271) panels = `(game world_panel coop_track_panel sync_audio_net_panel)`
  with **NO `(focus ...)` array** → SetTypeDef (UIScreen.cpp:59-61) defaults focus to `mPanelList[0]`
  = the `game` panel. So under the stagger: `game` loads on the activate frame; `world_panel` (the
  VENUE), `coop_track_panel` (the highway), and **`sync_audio_net_panel` (=SyncGameStartPanel, the
  net-start handshake)** are all DEFERRED, promoted one-per-rendered-frame, with `sync_audio_net_panel`
  promoted LAST (it's the 4th/last panel).
- Why this matters: native already documents game_screen entry as a delicate
  `CheckIsLoaded()`-gated stall point — `native/src/rb3_netsession_native.cpp:119-130`: "game_screen
  kTransitionTo stalls forever on game_screen->CheckIsLoaded() (the sync_audio_net_panel never
  reaches mState==5)". The handshake: `SyncGameStartPanel::IsLoaded()` returns
  `UIPanel::IsLoaded() ? mState==5 : false` (SyncGameStartPanel.cpp:26-28); `Load()`→`StartSync`
  sets mState=0 (`:19-22,30-33`); `PollIsSynced` walks 0→1→2→3 (`:40-76`); `OnMsg(SyncStartGameMsg)`
  asserts `mState==kStartingSession(4)`/`kWaitingForSessionStart(3)` then sets mState=5 (`:80-87`);
  the msg is fired by the native shim `NetSession::EnterInGameState` (rb3_netsession_native.cpp:139-143).
- VERDICT on game_screen: **functionally still safe but NOT helped, and ADDS latency to a fragile
  path.** The handshake is SELF-driven once the panel is promoted (the panel's own poll walks its
  states; the msg is downstream of its own machine, so the `mState==3||4` assert is not raced by
  deferral). A deferred panel is `Active()==false` → skipped by Enter/Draw/Poll/CheckIsLoaded
  (PanelRef::Active()==mActive&&mLoaded, UIScreen.h:21) until promoted; CheckIsLoaded returns false
  (anyPending) until the queue drains → Enter fires once with all panels loaded → no blank/no
  half-load. BUT the venue+highway+handshake now each wait their one-per-frame promotion slot, so
  game_screen entry takes ~3 extra rendered frames before the handshake panel even STARTS its
  StartSync→...→mState=5 walk. On a path the project already had to hand-shim against stalling, this
  is needless added fragility for ZERO benefit (game_screen's cost is not LoadPanels — see below).

**Empirical: game_screen cost is a 292 ms SYNC-DRAIN, not LoadPanels.** `--into-song` run, worst
frame = **f3975 dt=359.36 ms on game_screen, tagged `syncDrain=292ms,pend=1`** (PollUntil* sync
drain, lpu≠0). That is `BandScreen::LoadInterstitials` (BandScreen.cpp:64-72:
`cur->CheckLoad(); cur->CheckIsLoaded();` synchronous on mExtraPanels) + venue mesh VBO upload —
NONE of which is `UIScreen::LoadPanels`/mPanelList. The stagger patch touches neither. So for the
heaviest real in-game transition the patch does nothing but add ~3 frames of promotion latency.

**Refcount-balance (CheckLoad↔CheckUnload) — verified BALANCED.** `mLoaded` (PanelRef) is the single
marker tying `CheckLoad()` (mLoadRefs++, UIPanel.cpp:37-40) to `UnloadPanels`'s `CheckUnload()`
(mLoadRefs--, gated on `it->mLoaded`, UIScreen.cpp:301-306). The patch sets `mLoaded=true` ONLY
immediately after each `CheckLoad()` (both in staggered LoadPanels and the drain), and never
double-CheckLoads (the drain excludes `mLoaded==true`). Deferred panels (`mLoaded=false`) get no
CheckLoad and are skipped by UnloadPanels → balanced. Cancel-path (CancelTransition kTransitionTo,
UI.cpp:653-656) calls `mCurrentScreen->Enter(oldScreen)` → `Enter(from)` →
`from->UnloadPanels()` (UIScreen.cpp:146-150), so even the abandoned transition screen's loaded
panels DO get CheckUnload'd → no ref leak (correcting the prior verifier's "benign ref-leak" note:
the kTransitionTo cancel path DOES unload via the `from` arg; the leak would only occur if
CheckIsLoaded never returned and the user navigated away with mCurrentScreen==null, edge of edges).

**(d) Effectiveness — REFUTED, confirming prior verifier independently.** Re-measured: song_select
activate **f251 = 60.31 ms (first run) / 56.81 ms (into-song run), lp=0 lpu=0 ld=2 pend=1**, f252+
flat ~5-6 ms. The 56-60 ms is a synchronous milo parse on the activate frame, NOT the budgeted
loader (native LoadMgr::Poll is budget-capped at RB3_LOADER_BUDGET_MS=8 ms/frame via budgetTimer
break, Loader.cpp:431-475 — so a 60 ms frame CANNOT be the background drain). DirLoader::PollLoading
is one-state-step-per-poll on native (DirLoader.cpp:255-258). The dominant cost is the FOCUS panel
`song_select_panel`→`song_select.milo` (the prior verifier measured 2.82 MB) + its inline
widespinner/leaderboard/score Includes (which live in ui/resource/list/gen/*.milo and are Includes
of song_select.milo, NOT separate panels) — and the patch loads the focus panel on the activate
frame. Deferring only shortcut/filter/sv4 cannot move the focus-panel-dominated 56 ms. Predicted
"48 ms → 12-15 ms" is unsupported.

**NET: REFUTED.** Safe in the narrow sense (HX_NATIVE-gated, RB3-only, no DC3, no Wii impact, no
deadlock, no blank screen, refcount balanced) — but (1) INEFFECTIVE on the stated target
(focus-panel synchronous milo parse is not deferred; prior verifier's measurement reproduced), AND
(2) being DEFAULT-ON for ALL screens it needlessly adds ~3 frames of one-per-frame promotion
latency to `game_screen` — a transition the project ALREADY had to hand-shim against
CheckIsLoaded-stall (rb3_netsession_native.cpp) — for zero benefit there (game_screen's spike is a
292 ms interstitial/mesh sync-drain, untouched by this patch). Recommendation if shipped anyway:
default it OFF (opt-IN via RB3_STAGGER_PANELS=1) and scope it to song_select, OR — better — target
the focus panel's milo Include tree (split song_select.milo's leaderboard/widespinner/score
Includes into deferrable sub-loads, or make that parse budgeted/async), which is the actual
cost-bearing unit. Evidence: /tmp/rb3-frame-trace.jsonl; song_select f251=60.3 ms; game_screen
f3975=359 ms syncDrain=292; game.dta:266-271; SyncGameStartPanel.cpp:19-87;
rb3_netsession_native.cpp:119-143; Loader.cpp:431-475; DirLoader.cpp:255-258.

---

### W3 VERIFY — splash-venue-cull patch (adversarial; RUN-only, no build)

**Task:** try to REFUTE that the `splash-venue-cull` perf patch (re-enable per-drawable
frustum culling for the venue only, via `RB3VenueFrustumCull(d)` after `MenuVoidDrawHook`
in `RndDrawable::Draw()`/`DrawBudget()`, gated on env `RB3_VENUE_FRUSTUM_CULL`, scoped to
`RndCam::sCurrent->Name()=="world.cam"`) is SAFE and EFFECTIVE. Read all cited code; ran the
pre-built `native/build-native/rb3-native` (Jun 6 02:54). Did NOT build.

**VERDICT: NOT REFUTED. The patch is safe (all four axes pass) and effective (real, large
cull headroom empirically confirmed). Ship it default-OFF as specced; flip default only after
the orchestrator's A/B screenshot gate.**

#### (a) Wii decomp byte-identical — CONFIRMED
`src/system/rndobj/Draw.cpp` IS a decomp file matched vs the Wii binary, but all three edits
land strictly inside existing `#ifdef HX_NATIVE` regions:
- SITE 1 (helper `RB3VenueFrustumCull`) goes in the top HX_NATIVE helper block (the one that
  already holds `MenuVoidDrawHook` + `MenuVoidIsWorldcenterOccluder`), before its closing
  `#endif` at Draw.cpp:156.
- SITE 2/3 sit inside the `#ifdef HX_NATIVE` branches of `Draw()` (167-179) / `DrawBudget()`
  (187-198). The `#else` (Wii) branches (174-178, 200-205) are untouched.
MWCC compiles only the `#else` path → zero bytes change → byte-identical → 0 match% impact.
This file is ALREADY a divergent native file (carries MenuVoidDrawHook + SMASHER_DRAW_FIX
under the same guards, all match-neutral). The three OLD_STRINGs each grep to **exactly 1**
occurrence in the live file (verified) → the Edits apply cleanly and uniquely.

#### (b) Cannot break the screen load-state machine or blank a venue-needing screen — CONFIRMED
- The cull is in the per-drawable DRAW submission path, NOT the screen/loader state machine —
  it cannot affect screen readiness. It only elides a `DrawShowing()`/`DrawShowingBudget()`
  call for drawables whose world bounding-sphere is *fully* outside the world.cam frustum.
- `CompareSphereToWorld` = `s > mWorldFrustum` (Cam.h:63), and `operator>(Sphere,Frustum)`
  (Geo.cpp:932-941) returns true ONLY when the sphere center is beyond a frustum plane by more
  than its radius — i.e. the sphere is *entirely* outside. Conservative: a partially-visible
  mesh is never culled. So a correct frustum cannot blank on-screen geometry.
- Safety clause `if (!d->MakeWorldSphere(sphere,false)) return false;` (no-cull) means any
  unbounded/zero-sphere drawable (skybox-class, e.g. `skynight`/`sky_dome`/`moon` — their
  spheres are huge and encompass the cam, so even if bounded they test inside) is NEVER culled.
- The DANGEROUS alternative the patch explicitly rejected (screen-NAME cull on splash) was
  re-confirmed unsafe: `RB3_SKIP_STATIC=1` → splash goes black (per patch). This patch does
  NOT do that — it is geometry-frustum, not screen-name.
- DEFAULT-OFF (`getenv("RB3_VENUE_FRUSTUM_CULL")`, cached in `static int sOn`) → shipped build
  is byte-behavior-identical until explicitly enabled.

#### (c) DC3 regression — IMPOSSIBLE (stronger than the patch claims)
The patch's `decompSafe` worried that Draw.cpp is "shared via add_subdirectory". **It is NOT.**
- `rb3/native/CMakeLists.txt:250` compiles `${REPO_ROOT}/src/system/rndobj/*.cpp` (REPO_ROOT =
  the rb3 repo) into the rb3-native/rb3-web binary.
- `milo-native-engine` compiles its OWN gfx core + `Rnd_Wgpu_RB3.cpp`; it *includes* the
  consumer's rndobj HEADERS but does NOT compile any `rndobj/Draw.cpp` (verified: no Draw.cpp in
  its CMakeLists).
- DC3 has its OWN separate `dc3-decomp/src/system/rndobj/Draw.cpp` (distinct 6.7KB file).
⇒ This edit compiles ONLY into the RB3 native/web binary. DC3 never sees it. DC3 regression is
structurally impossible (not merely default-off-mitigated).

#### (d) Will it actually move the target frame ms? — YES (large headroom, empirically)
Ran `rb3-native` headless (`RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=…/orig-assets/extracted`):
- **`RENDER_DBG=30` on the boot splash:** confirmed the venue draws under **`cam=world.cam`
  pos=(110.76,60.11,299.97)**, **meshes=823 tris=252385** EVERY frame (matches W2-C exactly).
  → the strcmp gate string `"world.cam"` is the *actual runtime cam name*. The gate WILL fire.
- **`MENU_VOID_DBG2=2` mesh-position dump (235 dumped venue meshes):** world-origin spread is
  **X[-6770..+409] Y[-118..+9696] Z[-20132..+517]** with the cam at (110.8,60.1,300.0). Many
  meshes sit thousands of units off-axis: `building_12` Z=-10742, `moon` X=-4458, dozens of
  `building_*`/`bgbuildings_*`/`fireescape_*`/`sign_*` at X≈-2000..-5000. The /api/screenshot
  shows a TIGHT cityscape crop (2.16 MB, venue visible) → a large fraction of the 823 small
  distant props are fully off-screen with tight local extents ⇒ their world spheres are fully
  outside the world.cam frustum ⇒ culled. Skybox meshes (skynight/sky_dome/moon) are NOT culled
  (encompass cam) — small count, correctly retained.
- `mWorldFrustum` IS maintained natively (`Cam.cpp:187 Multiply(mLocalFrustum, world,
  mWorldFrustum)` in `UpdatedWorldXfm`) → the disable comment is a stale early-port concern.
The exact culled fraction needs a build to count (no `/api` hook arms FrameCapture), but the
6,770×9,800×20,650-unit origin spread vs a tight on-screen crop proves the cull removes a
**substantial** share of the 823-draw / 252k-tri load. Effective.

#### Correctness / compile notes (all verified against the live file)
- `RndCam::sCurrent` (Cam.h:70), `CompareSphereToWorld` (Cam.h:63), `Name()` (Object.h:294,
  the OBJECT-instance name — returns "world.cam", NOT the class name), `MakeWorldSphere` (virtual,
  Draw.h:48), `Sphere` (Geo.h/Sphere.h, already used in the `#else` paths), `getenv`/`std::strcmp`
  (`<cstdlib>`/`<cstring>` in the HX_NATIVE block, Draw.cpp:11-12; bare strcmp/strstr already used)
  — ALL in scope. Compiles under native clang/gcc (this code is never seen by MWCC).
- `RndGroup::MakeWorldSphere(s,false)` returns the group's `mSphere` which `UpdateSphere`/
  `MakeWorldSphere(s,true)` grows to contain ALL children (Group.cpp:331-345) → culling a group
  only when its whole sphere is outside is conservatively safe for its children.
- **Band characters are immune:** they draw via direct `bandChar->DrawShowing()` (world/Dir.cpp:
  459), bypassing `Draw()`/`DrawBudget()` entirely → the cull cannot drop band members.
- **Within-frame cam scoping is sound:** `RndCam::sCurrent` switches per sub-pass (world.cam for
  venue, game.cam for highway, UI cam for HUD/splash logo). The gate reads sCurrent at each
  drawable's draw-time, so it only engages during the world.cam venue sub-pass. The codebase
  already relies on this exact `cam->Name()=="game.cam"` idiom for the landed track-lighting/glow
  fixes (f5ee015, 7e2fe9a9) → the pattern is proven.

#### The ONE residual risk (already correctly mitigated by the patch)
The original disable comment ("WebGPU frustum doesn't match RndCam::sCurrent's") is the real
hazard: if `mWorldFrustum` math is subtly wrong natively, a re-enabled cull could drop a VISIBLE
mesh. I confirmed mWorldFrustum is populated (Cam.cpp:187) and the screenshot frustum looks
sane, but I could NOT build to prove the frustum math drops zero visible meshes. The patch
handles this correctly: (1) DEFAULT-OFF; (2) world.cam-scoped (never the splash logo/highway/
HUD); (3) MakeWorldSphere==0 ⇒ never cull (skyboxes safe); (4) mandatory A/B of splash /
main_hub / in-song-venue screenshots flag-on vs -off before flipping the default. With those
four guards the worst case is "a venue mesh blinks off in one shot," caught by the A/B gate, and
recoverable instantly (unset the env). No path to a black screen, broken load state, Wii-match
regression, or DC3 regression.

**Also note for the in-song venue:** gameplay ALSO draws the venue/crowd/band under world.cam
(per the established two-camera architecture; highway+HUD under game.cam). So with the flag ON
the cull fires in gameplay too — correct for static backdrop geometry, but animated venue props
whose `mSphere` is stale are the only theoretical false-cull source. This is exactly what the
orchestrator's in-song-venue A/B screenshot must check before flipping the default. (Default-off
means shipped behavior is unchanged regardless.)

Evidence logs: /tmp/splash_rd2.log (world.cam/823 mesh), /tmp/menuvoid_dump.log (235-mesh
origin spread), /tmp/splash_shot.png (2.16 MB venue screenshot). Code read: Draw.cpp:40-208,
Cam.h:63/70, Cam.cpp:184-190, Geo.cpp:932-941, Draw.h:48, Mesh.cpp:202-224, Group.cpp:331-345,
world/Dir.cpp:393-462, rb3/native/CMakeLists.txt:250, milo-native-engine/CMakeLists.txt.

---

## ORCHESTRATOR SYNTHESIS — Wave 03 converge (2026-06-06)

Goal: drive down the two attributed stutters. Both designed patches were applied,
A/B-measured against the P3 baseline, found non-viable, and REVERTED (clean tree).
The measure-after-fix discipline rejected both before any could ship.

### P2 splash venue-cull — REJECTED (breaks visuals)
- Applied (Draw.cpp, default-off RB3_VENUE_FRUSTUM_CULL, world.cam-scoped), built.
- Screenshot A/B at splash_screen: flag-off = full cityscape + "ROCK BAND 3" logo;
  flag-on = logo GONE + large cityscape chunks missing/replaced by empty planes
  (/tmp/venue_splash_off.png vs _on.png). RENDER_DBG max meshes 983→677 (cull active).
- Root: the native world.cam frustum (RndCam::mWorldFrustum) does NOT match the WebGPU
  render projection, so CompareSphereToWorld culls VISIBLE meshes — exactly the
  pre-existing bug the SMASHER_DRAW_FIX disabled culling for (it was game.cam there;
  this proves world.cam is wrong too). Default-off gate prevented any shipped harm.
- Also: the 68 ms splash frames are largely a HEADLESS Submit+WaitAny artifact
  (GpuDevice.cpp:403-413), absent under web/windowed vsync — questionable as a real
  user stutter. REAL FIX = correct the native frustum (render workstream), not a cull toggle.

### P1 panel-stagger — REJECTED (no-op)
- Applied (UIScreen.cpp LoadPanels + CheckIsLoaded drain, default-off RB3_STAGGER_PANELS,
  Wii byte-identical), built.
- A/B (frame_profiler --into-song --scroll 8): song_select-ENTER spike 46.4 ms (off) vs
  44.4 ms (on) — within the ±3 ms run-to-run noise. No win.
- Root: the 46 ms is NOT distributed across the 5 panels; it is the song_select_panel's
  OWN milo decompress+parse + ~17 nested Includes in a single CheckLoad. Panel-level
  staggering cannot split one panel's milo tree. REAL FIX = stagger milo Include loading
  / async parse at the DirLoader level (loader workstream).

### Net Wave-03 deliverable
The user's explicit perf ask — "measure/profile to identify the stutters" — is COMPLETE
and attributed precisely (loader is healthy/budgeted; the two real long frames are the
splash venue over-draw and the song_select-ENTER milo-parse). "Drive them down" needs
deeper engine work in two named areas, each a real next workstream. No broken or no-op
code shipped. Only the audio limiter (Wave 02) remains in the tree.
