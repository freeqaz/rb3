# Wave 04 — deeper perf (splash venue over-draw + ENTER milo-parse)

> Orchestrator merges verdicts into STATE.md at converge. Each `### <label>` is one
> fan-out subagent's evidence.

---

### A3 fallback-splash

**Task.** Independently of the A1 frustum-cull fix (which is blocked on the native
world.cam frustum not matching the WebGPU render projection — proven in wave-03 P2:
re-enabling cull dropped the "ROCK BAND 3" logo + cityscape chunks,
`/tmp/venue_splash_{off,on}.png`), enumerate cheaper ways to cut the ~10 fps / p50
100 ms splash venue cost while KEEPING the splash looking right. Recommend the best
fallback if the frustum fix slips this wave.

**DESIGN/MEASURE only** — no engine/src edits. RAN the pre-built
`native/build-native/rb3-native` (Jun 6 03:35) for the measurements below.

#### Live re-confirmation of the cost (RAN the binary)

`RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=…/orig-assets/extracted RB3_HTTP=1 RENDER_DBG=60`,
sat on the splash (did NOT press Start):

```
currentScreen = splash_screen
[render f60]  cam=world.cam pos=(110.76,60.11,299.97) meshes=823 tris=252385 uploaded_tex=943
[render f120] cam=world.cam pos=(110.76,60.11,299.97) meshes=823 tris=252385 uploaded_tex=943
```

`/api/screenshot` = 2.17 MB, the retail splash: "ROCK BAND 3 / PRESS START" over a
fully-rendered night cityscape (`/tmp/a3_splash_baseline.png`). Confirms wave-03: the
venue draws under `world.cam` (110.76,60.11,299.97) at a steady **823 meshes / 252 385
tris** every frame, with frustum culling disabled wholesale (`Draw.cpp:167-172` /
`183-198`, `#ifdef HX_NATIVE`).

#### The draw path (the seams a fallback can hook)

- `App::RunOneFrame` (**`src/App.cpp:472-562`, DECOMP, HX_NATIVE-gated**) brackets the
  whole frame: `TheRnd->BeginDrawing()` → `TheUI.Draw()` → `TheRnd->EndDrawing()`.
  BOTH the native loop (`App::RunWithoutDebugging`) and the web loop
  (`native/src/main_web.cpp:553-555` `BOOT_RUNNING → sApp->RunOneFrame`) call this same
  function — so a throttle here covers native + web identically. **This is the cleanest
  fallback seam.**
- The venue itself is `sv8_panel` (a `BackdropPanel`, `ui/vignettes.dta:183-194`,
  `world/vignette/shell/sv8/a/cityscape`) drawn by `WorldDir::DrawShowing`
  (`src/system/world/Dir.cpp:393`) under `world.cam`.
- Per-drawable skip seam already exists: `MenuVoidDrawHook(RndDrawable*)`
  (`Draw.cpp:63`, HX_NATIVE) is called from both `Draw()` (L168) and `DrawBudget()`
  (L188). Any per-mesh cull (distance/LOD/frustum) bolts on here.
- `EndFrame`/present: `BandRnd::EndFrame` (`Rnd_Wgpu_RB3.cpp:1124`) Finishes+Submits the
  encoder; web `GpuDevice::PresentFrame` (`GpuDevice_Web.cpp:185`) = `mSurface.Present()`.
  Skipping a frame's draw leaves the **last presented backbuffer on screen** (web canvas
  retains its last image; native headless retains `mHeadlessTex`) — so a "don't redraw"
  throttle does NOT blank, it just freezes the (animated) vignette for that frame.

#### Vignette is ANIMATED (constrains throttle/reuse options)

Wave-03's per-frame trace saw tris vary **220 815 → 257 190** ⇒ the cityscape is an
animated looping vignette (`cityscape_clips.milo`: blinking windows / drifting clouds /
moon). (My coarse `RENDER_DBG=60` sampling only saw the steady 252 385; the per-frame
wave-03 capture is authoritative.) The animation is *subtle background ambience*, not
gameplay-critical — a 2×/3× redraw throttle freezes it for 1-2 frames at a time, which
is very likely imperceptible on a static "PRESS START" hold, but it is NOT a frozen
still, so any "render once and reuse forever" idea is wrong.

#### Distance-cull viability — MEASURED (the data that ranks option e)

`MENU_VOID_DBG2=2` mesh-position dump (235 keyed backdrop meshes), camera at
(110.76,60.11,299.97). Distance = ‖mesh-origin − cam‖. Full analysis in
`/tmp/a3_meshdump.log`:

| bucket (origin dist) | count |
|---|---|
| <500 | 52 |
| 500–1k | 36 |
| 1k–2k | 6 |
| 2k–3k | 30 |
| 3k–5k | 15 |
| 5k–8k | 22 |
| 8k–12k | 69 |
| >12k | 5 |

**CRITICAL SAFETY DATUM — the visible backdrop sits FAR from the camera:**

| backdrop mesh (MUST NOT cull) | origin dist |
|---|---|
| sky_dome / sky_dome02 / difference_clouds | 418 / 482 / 667 |
| **moon.mesh** | **5 112** |
| skyscrapers2 / skyscrapers_01 | 6 547 / 6 743 |
| **skynight.mesh** | **8 539** |

So the farthest *visible* backdrop mesh is **8 539** units away. A naive distance-cull
must therefore use a radius **> 8 539** to be safe — which only trims the ~67 keyed
meshes beyond 10k (named `warehouse_*`, `silos_01`, `prison_01`, `sidewalk_*`,
`railwaybridge_*`, `fireescape_*` — a far-back district likely off-frame). Distance-cull
is **lower-risk than frustum-cull** (a scalar radius test, no projection-matrix matching
— it cannot mis-cull from an aspect/fovScale mismatch) but it is NOT free of A/B risk:
"distant" ≠ "off-screen" for the skyline, so the cull radius must be tuned + screenshot-
verified, and at a safe radius it removes a *modest* fraction (≈ the 67/823 far district),
not the dominant cost. It is a partial win, not the big one.

---

#### The five options

**(a) Cap / throttle the splash render rate (render the venue at e.g. 20–30 fps; rAF
still 60 Hz).**
- *Mechanism:* in `App::RunOneFrame`, when `TheUI.CurrentScreen()->Name()=="splash_screen"`,
  only run `TheRnd->BeginDrawing()/TheUI.Draw()/EndDrawing()` every Nth call (env
  `RB3_SPLASH_DRAW_EVERY`, default 1 = off). On skipped frames do nothing — the last
  presented surface stays on screen. Keep polling input every frame (Start must stay
  responsive) — only the DRAW is throttled.
- *Feasibility:* HIGH. ~10 lines, one seam, no per-mesh logic, no frustum/projection
  concern. Covers native + web from the same edit. The animated vignette degrades from
  ~14 fps to a capped fps (e.g. N=2 → ~7 fps draw but the GPU is idle the other half →
  the rAF gap on present-only frames collapses to near-zero, so the *average* and *p50*
  inter-frame gap drop sharply). Input latency unaffected (poll every frame).
- *Visual risk:* LOW-MED. The splash already runs ~10 fps, so a further halving of the
  *draw* rate makes the background ambience choppier — but it is a static "PRESS START"
  hold, the user isn't interacting with the venue, and the 2D logo is part of the same
  world pass so it does NOT get a separate higher rate (acceptable: nothing on the logo
  moves). Risk it looks "laggy" if N too high; N=2 is the safe default.
- *native-only vs decomp:* edit is in **`src/App.cpp` (DECOMP, matched vs Wii)** but
  wholly inside the existing `#ifdef HX_NATIVE RunOneFrame` body → Wii byte-identical, 0
  match impact. RB3-only (App.cpp is the rb3 repo's own decomp TU; not shared engine; DC3
  has its own App.cpp). HX_NATIVE covers native + web.
- *A/B:* native `frame_profiler.py … --worst 20` (splash_screen p50: baseline ~68 ms →
  target ≪) + `/api/screenshot` (logo + skyline still present, just choppier ambience).
  Web: `scripts/web/web-stutter-probe.mjs` rAF-gap p50 on splash (baseline 100 ms → target
  ~50 ms at N=2). **Cheapest, most robust fallback.**

**(b) Draw the venue every Nth frame and REUSE (cache the venue framebuffer, composite
the 2D logo every frame).**
- *Mechanism:* render the world.cam venue pass into an offscreen color target every Nth
  frame; on the in-between frames blit the cached target + redraw only the cheap 2D
  splash_panel/meta overlay. Needs a backdrop-FB cache in the renderer.
- *Feasibility:* LOW-MED. Requires real renderer work in **`milo-native-engine/src/platform/
  Rnd_Wgpu_RB3.cpp` (native-only, no Wii concern)** — a cached RT, a composite path, and
  splitting the world pass from the 2D overlay pass (today they're one world pass; the
  logo composites *in* the world pass, proven by `RB3_SKIP_STATIC` blanking everything).
  That split is the hard part and is the same structural surgery the in-song postproc path
  needed. More code, more risk than (a).
- *Visual risk:* MED. Same animation-freeze as (a) for the cached frames, PLUS a seam risk
  if the 2D-overlay/venue separation isn't pixel-perfect. Strictly dominated by (a) for the
  splash (the only thing (b) buys over (a) is redrawing the 2D logo at full rate — but the
  logo is static, so there's nothing to gain).
- *native-only vs decomp:* native-only (renderer). DC3 unaffected.
- *A/B:* same as (a) plus a careful screenshot diff at the cache-refresh boundary.
- *Verdict:* not worth it over (a) here — the 2D logo is static so the per-frame-redraw
  benefit (b)'s extra machinery buys is nil. **Defer.**

**(c) LOD / decimate the cityscape meshes for the shell vignette.**
- *Mechanism:* substitute lower-poly meshes (or skip sub-detail meshes) for the sv8
  vignette specifically. Either an offline asset LOD or a runtime "skip these N detail
  meshes on splash" list.
- *Feasibility:* LOW. The 252k tris are spread across 823 meshes; no LOD chain exists in
  the assets, so this is either (i) an offline asset-authoring pass (out of scope, changes
  ship assets) or (ii) a hand-maintained skip-list of detail meshes by name — brittle and
  the same "is this mesh visible?" judgement call as frustum-cull, just manual. The cost is
  also as much *per-draw overhead × 823 draws* as it is raw tris, so decimating tris
  without cutting draw count under-delivers.
- *Visual risk:* MED-HIGH (hand-skipping named meshes can drop a visible building; offline
  LOD changes the shipped look). 
- *native-only vs decomp:* a runtime skip-list would live in `Draw.cpp`'s `MenuVoidDrawHook`
  (DECOMP, HX_NATIVE-gated) keyed by mesh-name + screen; offline LOD is an asset change.
- *A/B:* screenshot per skip-candidate. High labor. **Low ROI; skip.**

**(d) Cheaper static backdrop image for the splash.**
- *Mechanism:* replace the live 3D vignette with a pre-rendered still (a captured splash
  PNG drawn as a fullscreen quad) behind the 2D logo, only on splash_screen.
- *Feasibility:* MED. Capture a reference still (we already have `/tmp/a3_splash_baseline.png`
  = the exact retail framing), ship it as a tex, and on splash draw the quad instead of
  the venue. The plumbing (a fullscreen textured quad + suppressing the sv8 venue draw on
  splash) is modest renderer/UI work, native-only.
- *Visual risk:* MED. Loses the animated ambience entirely (blinking windows / drifting
  moon → frozen). On a 2-second "PRESS START" hold most users won't notice, but it IS a
  fidelity regression vs retail, and it bakes in one resolution/framing (re-capture needed
  per aspect). Effectively the strongest perf win (venue cost → ~0) at the cost of the
  most visible-fidelity loss.
- *native-only vs decomp:* native-only (renderer + a shipped asset). DC3 unaffected.
- *A/B:* screenshot (should match the captured still) + stutter-probe (splash → flat
  ~16 ms). **Strong perf, but a real fidelity downgrade — the "if nothing else works"
  option, not the first choice.**

**(e) Distance-cull (cull meshes beyond a far radius from world.cam) — lower risk than
frustum.**
- *Mechanism:* in `MenuVoidDrawHook` (or a sibling), when `RndCam::sCurrent->Name()==
  "world.cam"` and `RB3_VENUE_DIST_CULL` is set, skip a static (non-boned, non-skybox)
  drawable whose world-sphere center is farther than R from the cam. Reuse the Wii
  primitives (`MakeWorldSphere`, the cam world pos); a zero-radius sphere ⇒ never cull
  (skyboxes safe, same as the frustum patch).
- *Feasibility:* HIGH (scalar test, no projection matrix). 
- *Visual risk:* MED — **measured above:** the visible skyline (`skynight` 8 539, `moon`
  5 112, `skyscrapers` 6.7k) is FAR, so R must be > 8 539 to be safe, and at that radius the
  cull only removes the ~67 far-district meshes (warehouse/silos/prison/sidewalk at 10k+).
  Lower risk than frustum-cull (cannot mis-cull from aspect/fovScale mismatch) but a
  *partial* win and still needs a screenshot A/B to confirm the far district is off-frame.
- *native-only vs decomp:* `Draw.cpp` (DECOMP, HX_NATIVE-gated). RB3-only. DC3 unaffected.
- *A/B:* `RENDER_DBG` (watch meshes fall from 823) + `/api/screenshot` (skyline/moon/sky
  intact) + stutter-probe. Sweep R ∈ {9000,10000,12000} for the largest safe cull.
- *Verdict:* a credible *complement* to (a) — but on its own it only trims the far district
  (~8% of meshes at the safe radius), so it does not reach sub-frame. Best used stacked
  on top of (a).

---

#### RECOMMENDATION — if the A1 frustum fix slips: **(a) splash draw-rate throttle.**

(a) is the highest-confidence, lowest-risk, fewest-LOC fallback and the only one that
needs no renderer surgery, no per-mesh visibility judgement, and no asset change. It
directly attacks the metric that matters (the sustained ~10 fps / p50 100 ms web rAF gap
on the splash hold) by halving/thirding the *draw* rate while the GPU idles the rest —
collapsing the average inter-frame gap — and it keeps input polling at 60 Hz so "PRESS
START" stays instant. The only cost is choppier background ambience on a static hold,
which is acceptable. One seam in `App::RunOneFrame` (DECOMP, HX_NATIVE-gated, Wii
byte-identical) covers native AND web from a single edit; default-off (`RB3_SPLASH_DRAW_EVERY=1`)
until A/B-verified, then default N=2.

**Stack (e) distance-cull on top** if (a) alone isn't enough — it's an independent,
also-low-risk lever (safe radius > 8 539 trims the ~67-mesh far district) that composes
cleanly with the throttle and shares the same A/B harness.

Reserve **(d) static backdrop** as the last resort (biggest perf win, but a real fidelity
downgrade — loses the animated vignette). Skip (b) (dominated by (a) here — the logo is
static) and (c) (low ROI, brittle).

**Evidence:** `/tmp/a3_splash_baseline.png` (2.17 MB, full splash), `/tmp/a3_meshdump.log`
(235-mesh origin dump), `/tmp/a3_splash_run.log` (RENDER_DBG 823/252385 world.cam).
Code read: `src/App.cpp:472-562`, `src/system/rndobj/Draw.cpp:63-208`,
`src/system/world/Dir.cpp:393-494`, `native/src/main_web.cpp:553-567`,
`milo-native-engine/src/gfx/GpuDevice.cpp:300-360,403-413`,
`milo-native-engine/src/platform/{GpuDevice_Web.cpp:153-185,Rnd_Wgpu_RB3.cpp:210-224,1124-1160}`.
A/B harnesses: `scripts/native/frame_profiler.py`, `scripts/web/web-stutter-probe.mjs`,
`/api/screenshot`, `RENDER_DBG`.

---

### A2 include-stagger

**Task.** Wave 03 proved panel-LEVEL staggering is a no-op: the song_select-ENTER cost is
the `song_select_panel`'s OWN 2.82 MB `song_select.milo` decompress+parse + ~17 nested
Includes, all pulled in one `CheckLoad`. Design a milo-INCLUDE-level fix to spread that
parse across frames / off the activate frame. HX_NATIVE-gated + env opt-in.

#### 1. How a panel's milo + its ~17 Includes actually load (traced, full chain)

```
UIManager::GotoScreenImpl (UI.cpp:665) / UIScreen::Exit (UIScreen.cpp:239)
  -> UIScreen::LoadPanels (UIScreen.cpp:288) -> PanelRef::CheckLoad (per panel)
    -> UIPanel::CheckLoad (UIPanel.cpp:37, on ++mLoadRefs==1)
      -> UIPanel::Load (UIPanel.cpp:72)
        -> mLoader = new DirLoader(fp, kLoadBack, ...)   <- BACKGROUND (budgeted) loader
```

Focus panel `song_select_panel` -> `ui/song_select/gen/song_select.milo_xbox` (**2.82 MB**,
confirmed on disk). Siblings: `song_select_shortcut.milo_xbox` (0.33 MB) +
`song_select_filter.milo_xbox` (3.01 MB). The ~17 "Includes" are NOT separate panels and NOT
separate files - they are **inline sub-dirs serialized INSIDE the 2.82 MB song_select.milo
stream** (leaderboards / widespinner / score_display / star_display / mini_leaderboard /
header_performance / icons_esrb / setlist / browser / details ...).

The parent panel DirLoader is **kLoadBack -> budgeted by LoadMgr::Poll** (SystemPoll ->
TheLoadMgr.Poll(), 8 ms/frame RB3_LOADER_BUDGET_MS). The top-level milo decompress IS budgeted
and spread (this is why the song_select_enter_screen frames show bgLoad 8-18 ms - the parent +
early inline parse being sliced).

**Where the budget is DEFEATED - the inline-Include cascade is one un-budgetable burst:**
the parent DirLoader's LoadDir state (DirLoader.cpp:627-652) calls mDir->PreLoad(*mStream) then
mDir->PostLoad(*mStream) (ObjectDir::PreLoad/PostLoad, Dir.cpp:194/390). Inside that:

- ObjectDir::PreLoad (Dir.cpp:363-375) creates one inline DirLoader per Include via
  `curIDir.dir.LoadInlinedFile(fpath, &bs)` (Dir.h:92-100). **Each inline DirLoader reads from
  the SAME parent BinStream &bs** - the inline dir's serialized tree sits at the current stream
  position, interleaved inline in the parent stream.
- ObjectDir::PostLoad (Dir.cpp:394-418) loops every mInlinedDirs[i] and calls
  `iDir.dir.PostLoad(mLoader)` -> ObjDirPtr::PostLoad (Dir.h:123-130) ->
  **TheLoadMgr.PollUntilLoaded(mLoader, loader)** - the UNBUDGETED synchronous drain
  (Loader.cpp:231, native arm sets unk1c = 1e30f, ignores LoadMgr::Poll's budget). It pumps
  that inline dir to completion before advancing to inline dir i+1.

So once the parent dir's PostLoad begins, **all ~17 inline Includes drain to completion inside
a single PollFrontLoader call** (the parent's LoadDir step). LoadMgr::Poll's budget break
(Loader.cpp:469-471) only fires *between* PollFrontLoader calls, never inside one - so the whole
inline tree runs on one frame regardless of the 8 ms budget.

#### 2. The ENTER spike has TWO components - confirmed by measurement

Re-ran `frame_profiler.py --scroll 2` on the current binary: ENTER spike = **f250 dt=52.43 ms,
scr=song_select_screen, tag `LOAD+2,pend=1`, `lp=0 lpu=0`**. Cluster: of 4719 ms long-frame
time, **0 ms in PollUntil* (SYNC drain), 244 ms in LoadMgr.Poll (bg), 4475 ms elsewhere
(draw/poll/gpu)**.

`lp=0 lpu=0` on the spike frame is the key tell and RESOLVES "where does it block". The 52 ms
native spike splits:

- **(A) milo parse + inline-Include drain** = the `lp` (budgeted Poll) cost, ALREADY spread on
  native across the preceding song_select_enter_screen bgLoad frames (8-18 ms each). By the
  activate frame the parse is mostly DONE - hence `lp=0`. (The nested inline PollUntilLoaded
  runs INSIDE the parent PollFrontLoader inside LoadMgr::Poll, so its time is attributed to
  `lp`/Poll, NOT `lpu` - gLoadPollUntilMsThisFrame is only bumped by *top-level* PollUntil
  calls, of which there are none here. THIS is why lpu=0 even though PollUntilLoaded IS the
  drain primitive.)
- **(B) first-draw GPU realization** = the dominant native activate-frame cost. GPU
  mesh/texture upload is **LAZY at first DrawMesh** (Rnd_Wgpu_RB3.cpp:1212-1226,3547 "lazy
  upload if the texture hasn't been SyncBitmap'd yet"). When song_select_screen first draws,
  every now-visible widget mesh+texture from the 17 Includes uploads in that one frame ->
  attributed to draw/gpu/elsewhere (the 4475 ms bucket) -> lp=0 lpu=0. Same class as the splash
  f22/24 ~190 ms one-time VBO-upload spike.

**Native (46-52 ms): dominated by (B), GPU first-draw upload.**
**Web (~150 ms one-shot, web-stutter-probe): dominated by (A).** wasm parses milo ~3x slower
AND every ChunkStream read under the web loader is a JSPI suspend/resume; the 2.82 MB + inline
tree parse is the bulk. The web PollUntilLoaded arm (Loader.cpp:168-223) slices +
emscripten_sleep(0)s, but the parent ObjectDir::PostLoad calls nested PollUntilLoaded per inline
dir and the OUTER PostLoad does not return until ALL 17 drain, so the whole tree still presents
as one ~150 ms rAF-gap hitch.

#### 3. Is the inline-Include parse cleanly splittable across frames? - NO (honest answer)

- **The inline dirs share the parent's ONE BinStream.** LoadInlinedFile(fpath, &bs)
  (Dir.h:92-100, Dir.cpp:368/380) hands the inline DirLoader the parent's &bs. Inline dir i's
  bytes are interleaved at a FIXED stream position; the parent PostLoad MUST fully consume
  inline dir i before reading the bytes that follow it (LoadRest/ReadString/proxy,
  Dir.cpp:455-481). Parse order is fixed and sequential - NOT 17 independent round-robinnable
  loaders.
- **ObjectDir::PostLoad is a single straight-line, NON-RE-ENTRANT function** (Dir.cpp:390+): it
  loops the inlined dirs then continues reading the parent stream. No resume-point state machine
  (unlike DirLoader's outer mState). You cannot return mid-loop and resume next frame without
  re-architecting it into a state machine that checkpoints (a) loop index i, (b) the parent
  stream position, (c) the gRev/PushRev/PopRev stack it threads. That is a deep,
  **match-breaking** change to a hot matched TU (Dir.cpp/Dir.h) referenced by every milo load.

**Conclusion:** the inline-Include parse is *fundamentally synchronous within
ObjectDir::PostLoad* (shared stream + straight-line design). "Spread the 17 inline parses across
frames in place" is NOT cleanly feasible without re-entrant-izing ObjectDir::PostLoad (deep +
match-breaking). The design pivots to two levers that DON'T touch that function's control flow.

#### 4. The viable HX_NATIVE-gated fix - PRE-WARM the focus panel during the PRIOR screen

Lowest-risk win: **start loading song_select's panels while the user still sits on
main_hub_screen**, so by ENTER the parse (A) is done and only first-draw GPU upload (B) remains
(and B can be warmed by an off-screen pre-draw). Two independently-shippable sub-levers:

**(4a) Prefetch the next screen's panel milos (kills component A on web - the big web win).**
HX_NATIVE, env opt-in (`RB3_PREWARM_SCREENS=1`): when the current screen is loaded + idle, issue
the next screen's panel DirLoaders as **kLoadBack (budgeted background)** ahead of time. The
loaded ObjectDirs sit in TheLoadMgr.mLoaders; DirLoader::Find(fp) returns them, so the later
UIPanel::Load finds an already-loaded loader and the parse is free.
- Seam: `UIScreen::Poll` (matched file; HX_NATIVE tail). Once current screen is CheckIsLoaded()
  + idle, call `TheLoadMgr.AddLoader(fp, kLoadBack)` per panel file of the configured next
  screen. Mapping from a `(prewarm_next "song_select")` array on the screen TypeDef OR env
  allowlist `RB3_PREWARM_NEXT=main_hub:song_select` (env keeps it out of .dta -> ships dark).
  Panel file resolved exactly like UIPanel::Load: TypeDef()->FindArray("file").
- Safe: kLoadBack loaders are budget-capped (8 ms), never block a frame; an unused prewarm is a
  cached loader the unload path reclaims; it does NOT change LoadPanels/CheckLoad/CheckIsLoaded
  semantics - it only seeds the loader cache. Degrades gracefully if the user ENTERs before
  prewarm completes (DirLoader::Find returns the in-flight loader, finished on the normal path).
- Decomp: src/system/ui/UIScreen.cpp is DECOMP -> whole hook #ifdef HX_NATIVE +
  getenv("RB3_PREWARM_SCREENS"); Wii #else byte-identical. RB3 native/web only (DC3 has its own
  UIScreen.cpp; shared engine has none).

**(4b) Off-frame GPU warm of the just-loaded panel (kills component B on native).**
After the focus panel milo is loaded but before the visible activate frame, force the lazy GPU
mesh/texture uploads on a non-presented frame: walk the newly-loaded dir's RndMesh/RndTex and
force the same SyncBitmap/buffer-create the first DrawMesh would. Native-only renderer behavior
-> hook in `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (native-only file, NO Wii
concern), `RB3_PREWARM_GPU`-gated, called from the (4a) prewarm hook. Lower priority than (4a)
because (B) is the *native* cost and native 46 ms is already 3x better than web 150 ms - but it
is the lever that takes the native activate frame from 52 ms toward steady ~6 ms.

**Fallback (only if 4a/4b unacceptable):** re-entrant-ize ObjectDir::PostLoad (checkpoint loop
index + stream position + rev-stack, return-and-resume across frames under a budget), #ifdef
HX_NATIVE. Feasible in principle but a substantial state-machine rewrite of a hot matched TU
referenced by every milo load - high regression risk, NOT the first move.

#### 5. Recommended exact patch (lever 4a - prewarm; cleanest + biggest web win)

DECOMP-gated, env opt-in, default OFF. No ObjectDir::PostLoad change. Add to the END of
`UIScreen::Poll()`, wholly under #ifdef HX_NATIVE:

```cpp
#ifdef HX_NATIVE
    // RB3_PREWARM_SCREENS (default off): once this screen is loaded + idle, seed the
    // NEXT screen's panel milo loaders as kLoadBack (budgeted) so the ENTER transition
    // finds them already-parsed. Pure loader-cache seeding - no LoadPanels/CheckLoad
    // semantics change. Wii build never compiles this; #else body byte-identical.
    {
        static int sPrewarm = -1;
        if (sPrewarm < 0) {
            const char *e = ::getenv("RB3_PREWARM_SCREENS");
            sPrewarm = (e && e[0] && e[0] != '0') ? 1 : 0;
        }
        // sPrewarmedSet: native-only std::set<UIScreen*> static in the .cpp (NOT a
        // member - adding a member shifts the matched UIScreen struct on Wii). Cleared
        // for `this` on Exit().
        if (sPrewarm && CheckIsLoaded() && sPrewarmedSet.find(this) == sPrewarmedSet.end()) {
            DoPrewarmNextScreen();   // native-only helper in the HX_NATIVE block:
                                     //   for each panel file fp of the next screen:
                                     //     if (!DirLoader::Find(fp))
                                     //         TheLoadMgr.AddLoader(fp, kLoadBack);
            sPrewarmedSet.insert(this);
        }
    }
#endif
```

- Store the per-activation "already prewarmed" flag in a .cpp-static std::set<UIScreen*>, NOT a
  new member (a member shifts the matched UIScreen struct - see BandCharacter::Filter header
  regression lesson). Clear `this` from the set in UIScreen::Exit (also HX_NATIVE-gated).
- DoPrewarmNextScreen() resolves the next-screen panel list (from `(prewarm_next ...)` TypeDef
  array OR env RB3_PREWARM_NEXT) and AddLoader(fp, kLoadBack) only where DirLoader::Find(fp)==0.

**Files touched:** src/system/ui/UIScreen.cpp (DECOMP, all HX_NATIVE-gated, Wii byte-identical;
RB3 native/web only - not shared milo-native-engine, not DC3). Optionally (4b) a native-only
prewarm-draw helper in milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp (no Wii concern).

#### 6. Predicted win + how to measure

- **Web (big target): ~150 ms ENTER one-shot -> expected sub-30 ms** if main_hub has ~1-2 s
  idle dwell to budget-load song_select.milo ahead (it does - the user reads the hub). Remaining
  web ENTER cost = first-draw GPU upload + screen swap. Faster-than-prewarm ENTER degrades
  gracefully to today (no regression).
  - Measure: `node scripts/web/web-stutter-probe.mjs` - compare the isolated
    main_hub->song_select transition gap (baseline ~150 ms) with RB3_PREWARM_SCREENS=1 (via the
    main_web.cpp rb3WebOpts/env bridge). Assert splash + steady-scroll gaps unchanged; ENTER gap
    drops.
- **Native: f250 activate 52 ms -> target < 16 ms** with (4a)+(4b). (4a) removes parse residue;
  (4b) removes first-draw GPU upload. (4a)-only on native is a modest win (native parse already
  fast); native win dominated by (4b).
  - Measure: `python3 scripts/native/frame_profiler.py --scroll 2 --worst 12` A/B
    RB3_PREWARM_SCREENS=0 vs =1 (+ RB3_PREWARM_GPU=1 for 4b). Compare the isolated first
    song_select_screen frame (LOAD+2,pend=1) + song_select per-screen max. Confirm song_select
    steady p95 (~6.5 ms) unchanged, lp/lpu stay 0 on the activate frame, main_hub_screen p95 not
    regressed (prewarm stays inside the 8 ms bg budget).

#### 7. Honest scoping / risks

- Panel-level stagger (Wave 03 P1) correctly a no-op - re-confirmed: the cost is ONE panel (the
  focus) whose 2.82 MB milo + inline tree is the burst; deferring shortcut/filter defers
  already-late work. Do not re-attempt that axis.
- "Spread the inline parse in place" NOT cleanly feasible (Sec 3): shared parent stream +
  non-re-entrant ObjectDir::PostLoad. Stated as required-deeper-change, not done.
- Prewarm risk: extra bg load on the prior screen - bounded by the existing 8 ms LoadMgr::Poll
  budget, cannot itself stutter main_hub. Memory: prewarmed dir holds ~3 MB until used/unloaded
  (loaded seconds later anyway). Wrong next-screen mapping = wasted prewarm (harmless); keep it
  env/.dta-driven, default-off until A/B-verified.
- Decomp safety: all RB3-tree DECOMP edits HX_NATIVE-gated, NO header/member additions to
  matched classes (use a .cpp-static set, not a new member), Wii build byte-identical. DC3
  unaffected (separate UIScreen.cpp; shared engine has none). Renderer prewarm (4b) is in the
  native-only Rnd_Wgpu_RB3.cpp - zero Wii/match concern.

Evidence: /tmp/a2-enter-trace.jsonl (f250 = 52.43 ms, LOAD+2, lp=0 lpu=0). Code read:
DirLoader.cpp:242-264/357-694, Loader.cpp:163-489, Dir.cpp:139-485, Dir.h:80-177,
UIPanel.cpp:37-102/292-300, UIScreen.cpp:288, UI.cpp:514-690, BandScreen.cpp:46-76,
Rnd_Wgpu_RB3.cpp:1212-1226/3547. Milo sizes: ls -laS orig-assets/extracted/ui/song_select/gen.

---

### A1 frustum-mismatch — DIAGNOSED (the splash venue cull is unsafe because per-mesh bounding spheres loaded from the Xbox milo are unreliable, NOT because the frustum FOV/aspect is wrong)

> RUN-only (no build). Probed the pre-built `native/build-native/rb3-native` (Jun 6 03:35)
> headless at the splash. Read every cited line in rndobj/Cam.{cpp,h}, math/Geo.cpp,
> math/Mtx.h, math/Sphere.h, rndobj/Mesh.cpp, rndobj/Draw.cpp, world/Dir.cpp, ui/PanelDir.cpp,
> and milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp (WriteSceneUniforms).

#### TL;DR
The P2 splash venue-cull (`RB3VenueFrustumCull` in Draw.cpp, reverted) dropped the "ROCK BAND 3"
logo + cityscape chunks NOT because `RndCam::mWorldFrustum` is geometrically wrong. I verified the
cull frustum's FOV / aspect / near / far / handedness / orientation ALL match the WebGPU render
projection at 16:9 (1280×720). The real defect is upstream of the frustum: **the per-drawable
bounding sphere the cull tests (`RndDrawable::mSphere`, loaded verbatim from the Xbox-platform milo
`sv8_a.milo_xbox`) does not reliably enclose the geometry the native backend actually renders**,
because on native these venue meshes are GPU-vertex-compressed and their CPU `mVerts` array is EMPTY
(`verts=0 (compressed=N)`), so the engine can never recompute/validate the sphere against the real
geometry the way the Wii build can. A correct frustum testing an untrustworthy sphere still drops
visible meshes. Fix = make the cull test a sphere derived from the geometry the renderer truly draws
(recompute per-mesh world-sphere after GPU upload), OR build a render-truthful cull frustum and pair
it with a conservative bounds source — world.cam-scoped, `RB3_VENUE_FRUSTUM_CULL` default-OFF,
screenshot-A/B-gated.

#### 1. The Wii cull frustum construction (rndobj/Cam.cpp + math/Geo.cpp::Frustum::Set)
`RndCam::UpdateLocal()` (Cam.cpp:155-182):
```
ratio = (mScreenRect.h / mScreenRect.w) * mUnknownFloat;        // 1.0 * 1.0 for world.cam
ratio *= TheRnd->YRatio();                                       // *0.5625 (kWidescreen)
mLocalFrustum.Set(mNearPlane, mFarPlane, mYFov, ratio);
```
`Frustum::Set(near,far,fovY,ratio)` (Geo.cpp:906-930) builds 6 planes in a **Y-forward, Z-up,
X-right camera-local** convention:
- `front = (0, 1, 0, -near)`  `back = (0,-1, 0, far)`     → depth axis is +Y
- `top = (0, sinHalfY, -cosHalfY, 0)`  `bottom = (0, sinHalfY, cosHalfY, 0)` → vertical uses Z(up)
- `sx = sinHalfY / ratio`; `left = (cosHalfY, sx, 0,0)`  `right = (-cosHalfY, sx, 0,0)` → horiz uses X
Then `RndCam::UpdatedWorldXfm()` (Cam.cpp:184-190) transforms it to world:
`Multiply(mLocalFrustum, world, mWorldFrustum)` — each plane via `Multiply(Plane,Transform)`
(Geo.cpp:104-120, normal by `FastInvert(t.m)`, offset by moving a point through `t`). This is
mathematically correct world-space plane transport. `CompareSphereToWorld(s)=s>mWorldFrustum`
(Cam.h:63) → `operator>(Sphere,Frustum)` (Geo.cpp:932-941) returns true (CULL) only when the sphere
center is beyond some plane by MORE than its radius, i.e. the sphere is FULLY outside — conservative.

`mWorldFrustum` IS kept live natively: `UpdatedWorldXfm()` is the `RndCam` override and is invoked by
`SetWorldXfm`/`SetWorldPos`/the lazy `WorldXfm()` recompute (Trans.cpp:110-144). Not stale.

#### 2. The actual WebGPU render projection (Rnd_Wgpu_RB3.cpp::WriteSceneUniforms, 838-913)
Built per-cam from the SAME camera state:
```
view  = world->camera-local from WorldXfm basis (right=m.x, fwd=m.y, up=m.z, eye=v);
        camera-local x'=right·(p-eye), y'=fwd·(p-eye)[depth], z'=up·(p-eye)   → Y-fwd, Z-up, X-right (SAME convention)
aspect  = WindowWidth()/WindowHeight() = 1280/720 = 1.7778
tanHalf = tan(yfov/2);  sy = fovScale/tanHalf (default fovScale=1);  sx = sy/aspect
P maps:  clip.x = sx*x ;  clip.y = sy*z ;  clip.z = f/(f-n)*(y-n) ;  clip.w = y   → WebGPU NDC z∈[0,1]
```
Horizontal half-FOV of the RENDER: `tan(halfX) = aspect·tan(halfY) = 1.7778·tan(halfY)`.
Vertical half-FOV: `tan(halfY_render) = tanHalf/fovScale = tan(yfov/2)`.

#### 3. The FOV/aspect/near/far ALL MATCH at 16:9 — the frustum is NOT the bug
- Horizontal CULL: `tan(halfX) = tan(halfY)/ratio = tan(halfY)/0.5625 = 1.7778·tan(halfY)`  ==  RENDER. ✓
  (`ratio=0.5625=YRatio(kWidescreen)`; the enum is `kSquare,kRegular,kWidescreen,kLetterbox` ⇒
   `kRatio[kWidescreen]=kRatio[2]=0.5625`, and `1/0.5625 = 1.7778 = 16:9` — exactly the render aspect.)
- Vertical CULL: `top/bottom` give `tan(halfY) = tan(yfov/2)` == RENDER (fovScale=1). ✓
- near/far: cull uses `mNearPlane/mFarPlane`; render uses the same `cam->NearPlane()/FarPlane()`.
  Probed world.cam: **near=20.0, far=20000.0** (RB3_LIGHT_PROBE, 782 world.cam frames). Same source. ✓
- Convention/handedness: both are Y-forward, Z-up, X-right camera-local. ✓
- `mUnknownFloat` (the SetFrustum 4th arg / `ratio` extra factor) is never written by `RndCam::Load`
  (Cam.cpp:75-110 reads near/far/yFov/screenRect/zRange/targetTex only) → stays its ctor default 1.0
  (Cam.cpp:130). `mScreenRect` for a full-screen cam = (0,0,1,1) ⇒ h/w=1. So `ratio` reduces to
  exactly `YRatio()=0.5625`. No hidden aspect skew.

CAVEAT (the one regime where they DIVERGE — not the splash): if the window is NOT 16:9, the two
horizontal terms part ways — the cull's horizontal extent is HARD-WIRED to 1/0.5625=1.7778 via
`YRatio()`, while the render's is the live `WindowWidth()/WindowHeight()`. At e.g. 4:3 or a square
headless window (`main_native.cpp:405` RunGpuSmoke default is 256×256!) the cull frustum would be the
WRONG width. The RB3_GAME path defaults 1280×720 (main_native.cpp:662) and the screenshot harness
inherits it, so the A/B ran at the matching 16:9 — which is exactly why the FOV is NOT the culprit
here. (If the production window is ever non-16:9, this becomes a SECOND, real bug — see fix note.)

#### 4. THE REAL ROOT CAUSE — the sphere the cull tests is untrustworthy for native compressed venue meshes
The cull calls `MakeWorldSphere(s,false)` (Draw.cpp:175/201). For a mesh that is
`RndMesh::MakeWorldSphere(s,false)` (Mesh.cpp:202-224): if `mSphere.GetRadius()!=0` it returns
`Multiply(mSphere, WorldXfm(), s)` (the milo-loaded LOCAL sphere transported to world); else returns
false ⇒ never culled.

Two facts make that sphere unreliable on native specifically:
- **`mSphere` is loaded verbatim from the milo** (`RndDrawable::Load`, Draw.cpp:299-300 `bs>>mSphere`,
  rev>0). The native build loads the **XBOX** milo `world/vignette/shell/gen/sv8_a.milo_xbox`
  (confirmed on disk; the only sv8 milo present). The bounding sphere baked by the Xbox tool is what
  is tested — not the Wii-baked sphere the `#else` Wii cull was written against.
- **The venue meshes are GPU-vertex-compressed and have NO CPU verts.** `MENU_VOID_DBG2=2` dump of 235
  backdrop meshes: every one reports `verts=0 (compressed=N)` (PostLoadVertices reads the Xbox blob
  into `mCompressedVerts`, leaves `mGeomOwner->mVerts` EMPTY — Mesh.cpp:474-507, HX_NATIVE arm). So the
  engine canNOT recompute/validate the sphere from geometry: `RndMesh::UpdateSphere()` (Mesh.cpp:170)
  and `MakeWorldSphere(s,true)` (202-218) both iterate `Verts()`, which is empty ⇒ they yield a
  DEGENERATE sphere. The native build is therefore wholly at the mercy of the (Xbox) milo's baked
  `mSphere` for the cull. The Wii build has the full uncompressed `mVerts` and a Wii-baked sphere.
- **`Multiply(Sphere,Transform)` carries NO scale** (Sphere.h:43-46: center transformed by full xfm,
  `radius` copied unchanged). If any venue mesh's `WorldXfm` has a scale>1 (vignette fill-scaling),
  the world radius is too small ⇒ premature cull. Same code on Wii, so scale≈1 is likely, but it is
  an additional reason the sphere can under-enclose.

Net: a CORRECT frustum testing an UNDER-SIZED / MIS-CENTERED sphere reports "fully outside" for a mesh
whose actual triangles are on-screen ⇒ the mesh is culled though visible. That is exactly the converge
A/B symptom (RENDER_DBG max meshes 983→677, ~31% dropped; some of those 306 were genuinely visible —
the logo + cityscape chunks).

#### 5. Why the "ROCK BAND 3" logo specifically vanished (resolves the apparent world.cam-scope paradox)
The logo is NOT a 2D UI-cam overlay — it is a **3D mesh inside the sv8 cityscape venue drawn under
world.cam**. The existing MENU_VOID comment in this very file states it: Draw.cpp:24 — "the BAND3/RB3
`logo`" is one of the sky-backdrop layers (`difference_clouds`, `skynight`, logo, `sky_dome*`) behind
the black `worldcenter` box, all in the world.cam venue pass. So a world.cam-scoped cull legitimately
reaches the logo; if the logo mesh's Xbox-baked `mSphere` is small/origin-centered, it culls. (The 2D
`splash_panel` press-start button DOES draw under `TheUI.GetCam()` via `PanelDir::DrawShowing`
Select→restore, PanelDir.cpp:133-158 — that part is correctly out of scope; the logo is the 3D one.)

#### 6. Evidence (RAN the binary; no build)
- RB3_LIGHT_PROBE: `WriteSceneUniforms cam='world.cam' near=20.0 far=20000.0` ×782 frames; the venue
  draws under world.cam every splash frame (also Cam.cam/meta.cam/[default cam] for other passes).
- MENU_VOID_DBG2=2 (235 meshes): all `verts=0 (compressed=N)`. World origins far off-axis vs cam
  (110.76,60.11,299.97): `skynight (-6770,5089,-206)`, `moon (-4458,2342,517)`, `building_12
  (-2035,1688,-10742)`, `bgbuildings_11/12` at X≈-2000…-6000 — yet the screenshot (1280×720, 2.16 MB)
  shows the full cityscape + moon ON screen ⇒ geometry is positioned by WorldXfm/local verts, and the
  per-mesh world ORIGIN is NOT where the visible triangles are ⇒ a sphere centered near the local
  origin lands far off-screen.
- Screenshot `/tmp/frustum_splash.png` (1280×720) = full intended cityscape (matches retail). Render
  default window = 1280×720 (main_native.cpp:662) ⇒ the A/B aspect was 16:9 ⇒ FOV matched ⇒ FOV is not
  the bug. RB3_LIGHT_PROBE log `/tmp/frustum_probe2.log`; mesh dump `/tmp/menu_dump.log`.
- `/api/dta/eval` could NOT read per-mesh `mSphere` (venue meshes aren't root-scope DTA symbols;
  `exists moon.mesh`→0 while `exists sv8_panel`→1) — so the exact baked radii weren't dumped without a
  build; the structural `verts=0`+Xbox-milo+`Multiply(Sphere)`-no-scale chain is the proof.

#### 7. THE FIX (so CompareSphereToWorld matches the REAL render frustum AND tests a trustworthy bound)
Two correct options; both world.cam-scoped, `RB3_VENUE_FRUSTUM_CULL` default-OFF, A/B-gated.

OPTION A (preferred, robust) — **cull against a per-mesh world sphere recomputed from the geometry the
renderer actually uses** (native-only, no Wii concern, no decomp edit):
- In the native mesh backend (`milo-native-engine/src/platform/Mesh_Wgpu.cpp` / `MeshGpuCache.cpp`),
  after the Xbox vertex blob is decompressed for GPU upload, compute the mesh's LOCAL bounding sphere
  from those decompressed positions ONCE and `RndDrawable::SetSphere()` it (Draw.h:72). Then the
  existing `MakeWorldSphere(s,false)` path produces a world sphere that truly encloses the drawn
  triangles, and `RB3VenueFrustumCull` (the reverted P2 seam) becomes SAFE — the frustum already
  matches at 16:9. Boned/skinned meshes keep the `s.Zero()` path (never culled), so chars are immune.
- This is native-only (the cull-decision helper in Draw.cpp stays HX_NATIVE/default-off); no Wii bytes
  change; DC3 unaffected (separate trees).

OPTION B (lighter, but keeps the bound-trust risk) — **build the cull frustum from the renderer's own
projection** and reuse it via a native-only setter, so there is ZERO chance of a future non-16:9
window de-syncing the horizontal extent (Section 3 caveat). In `WriteSceneUniforms` after `view`/`P`
are built, derive the 6 world-space frustum planes from `viewProj` (Gribb–Hartmann: rows of viewProj)
and store them in a native-only `BandRnd`/global the cull reads, instead of `RndCam::mWorldFrustum`.
This fixes the aspect-divergence regime but does NOT fix the under-sized/mis-centered sphere — so it
must still be paired with Option A's bound, or it will keep dropping the same meshes. ⇒ Option A is the
necessary fix; Option B is an optional hardening for non-16:9.

DO NOT just re-enable the P2 cull as-is against `mWorldFrustum` + the loaded `mSphere`: the frustum is
fine but the sphere is not, so it will drop the same visible meshes again.

#### 8. Exact reusable seam (the reverted P2 cull insertion point — Draw.cpp)
The cull DECISION still lands at the existing HX_NATIVE hook in BOTH `RndDrawable::Draw()` (after the
`MenuVoidDrawHook(this)` check at Draw.cpp:168, before `DrawShowing()`) and `DrawBudget()` (after
Draw.cpp:188, before `DrawShowingBudget(f)`), guarded by `RB3_VENUE_FRUSTUM_CULL` (default-off) AND
`RndCam::sCurrent->Name()=="world.cam"`. With Option A's recomputed sphere it is then:
```
// SITE: after MenuVoidDrawHook(this) in Draw() / DrawBudget(), HX_NATIVE only
if (RB3VenueFrustumCull(this)) return;        // Draw()  — or `return true;` in DrawBudget()
```
helper (native-only, default-off, world.cam-scoped; reuses the proven Wii primitives):
```
static bool RB3VenueFrustumCull(RndDrawable* d) {
    static int sOn = -1;
    if (sOn < 0) { const char* e = getenv("RB3_VENUE_FRUSTUM_CULL"); sOn = (e && e[0] && e[0]!='0') ? 1 : 0; }
    if (!sOn) return false;
    RndCam* cam = RndCam::sCurrent;
    if (!cam) return false;
    const char* nm = cam->Name();
    if (!nm || std::strcmp(nm, "world.cam") != 0) return false;   // venue cam ONLY
    Sphere s;
    if (!d->MakeWorldSphere(s, false)) return false;              // radius==0 ⇒ never cull (skybox-safe)
    return cam->CompareSphereToWorld(s);                          // true ⇒ fully outside ⇒ skip draw
}
```
This is INERT until `RB3_VENUE_FRUSTUM_CULL=1` AND a trustworthy `mSphere` exists (Option A). The
helper goes in the top HX_NATIVE block before the `#endif` at Draw.cpp:156; both call sites are inside
existing `#ifdef HX_NATIVE` arms; the `#else` Wii path is byte-identical ⇒ 0 match% impact.

#### 9. Decomp / DC3 safety
- `src/system/rndobj/Draw.cpp` + `Cam.cpp` are DECOMP (Wii-match) → any edit MUST be HX_NATIVE-gated.
  The cull helper + call sites are entirely inside existing `#ifdef HX_NATIVE` regions; Wii build sees
  none of it (byte-identical). The Option A sphere-recompute lives in the native-only engine
  (`milo-native-engine/src/platform/Mesh_Wgpu.cpp`) — no Wii/match concern at all.
- DC3: Draw.cpp/Cam.cpp are RB3-tree-local (rb3/native CMake globs `${REPO_ROOT}/src/system/rndobj/*.cpp`);
  DC3 has its own copies; the shared milo-native-engine compiles no Draw.cpp/Cam.cpp. Option A's
  Mesh_Wgpu.cpp IS shared — but it only ADDS a SetSphere() after upload (a no-op unless the mesh has a
  zero sphere AND the bound is recomputed); guard it behind the same env or a RB3-only path if DC3
  parity is a concern. DC3 cannot regress with the cull default-off.

#### 10. How converge verifies
1. Build Option A (sphere recompute) + the default-off `RB3VenueFrustumCull` seam.
2. `RB3_VENUE_FRUSTUM_CULL=1` + `/api/screenshot` A/B at **splash** (cityscape + moon + ROCK BAND 3
   logo + sky gradient MUST all survive — diff vs `/tmp/frustum_splash.png`), **main_hub**, and an
   **in-song venue** (world.cam also draws gameplay venue/crowd). Pixel-identical to flag-off minus
   genuinely off-screen geometry ⇒ pass.
3. `RENDER_DBG=1`: confirm `mDrawnMeshes` drops on splash (was 983→677 with the BROKEN sphere; with the
   recomputed sphere the drop should be the genuinely-off-screen fraction with NO visible loss).
4. Web re-probe `scripts/web/web-stutter-probe.mjs` splash p50 (baseline 100 ms) → target sub-frame.
5. Only flip the default ON after the screenshot A/B is clean on all three screens.
What could still break: (a) a non-16:9 production window re-introduces the horizontal aspect divergence
(Section 3) — add Option B if windows are resizable/non-16:9; (b) animated vignette props whose sphere
is stale frame-to-frame — Option A recomputes once at upload, so a prop that MOVES after upload could
under-cull; venue props are mostly static, verify in the in-song A/B. Default-off means shipped
behavior is unchanged regardless.

Evidence files: `/tmp/frustum_splash.png` (1280×720 full cityscape), `/tmp/frustum_probe2.log`
(world.cam near=20/far=20000 ×782), `/tmp/menu_dump.log` (235 meshes, all verts=0 compressed, far
origins). Code read: rndobj/Cam.cpp:14-190, Cam.h:50-63, math/Geo.cpp:104-120/906-941, math/Mtx.h:
330-341/585-592, math/Sphere.h:34-60, rndobj/Mesh.cpp:170-224/442-507/608-682, rndobj/Draw.cpp:
40-208/299-300, rndobj/Rnd.cpp:235-238, rndobj/Rnd.h:49-54, rndobj/Trans.cpp:110-144, ui/PanelDir.cpp:
125-159, world/Dir.cpp:393-462, milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:838-913.

---

### W4-VERIFY world.cam frustum-mismatch ROOT CAUSE + sphere FIX (adversarial — RUN-only, no build)

**Task:** try to REFUTE the claim that the native world.cam cull frustum is *geometrically
correct* (matches the WebGPU render projection) and that the splash logo+cityscape were dropped
by a BAD/EMPTY per-mesh bounding SPHERE (not a wrong frustum). Re-read all cited code from
scratch + ran the pre-built `native/build-native/rb3-native` (no build). **VERDICT: NOT REFUTED.
The root cause is correct on every checkable axis; the diagnosis (frustum OK, sphere bad) holds;
Option A (recompute the local sphere from decompressed verts) is necessary and sufficient; the
Draw.cpp seam/patch anchors are accurate.** One genuine residual caveat (non-16:9 + WorldXfm
scale) flagged below — it does NOT refute the fix, but Option B / a scale-aware radius are the
correct hardening, and the seam MUST stay default-off + screenshot-A/B-gated.

**(1) Frustum geometry — CONFIRMED MATCHING at 16:9 (the default native/web window).**
Independently derived both half-angles from source:
- *Cull frustum* (`Cam.cpp:155-161` UpdateLocal → `Geo.cpp:906-930` Frustum::Set). Full-screen
  `mScreenRect=(0,0,1,1)` ⇒ `ratio = (h/w)*mUnknownFloat*YRatio() = 1*1*YRatio()`. The left/right
  plane normal is `(cos halfY, sin halfY / ratio, 0)` ⇒ horizontal half-extent
  `tan(halfX_cull) = tan(halfY)/ratio = tanHalf / YRatio()`. Vertical = yFov by construction.
- *Render projection* (`Rnd_Wgpu_RB3.cpp:876-911`). `sy = fovScale/tanHalf`, `sx = sy/aspect`,
  `aspect = WindowWidth/WindowHeight`. On-screen X test `|sx*x| ≤ w=y` ⇒
  `tan(halfX_render) = aspect*tanHalf`. Vertical `|sy*z| ≤ y` ⇒ `tan(halfY_render)=tanHalf`. ✓
- Equality ⇔ `aspect = 1/YRatio()`. **VERIFIED LIVE:** `Rnd.cpp:235-238` `kRatio[]={1,.75,.5625,
  .5625}`, default `mAspect=kWidescreen` (=2, `Rnd.cpp:217`; enum `Rnd.h:49-54`) ⇒ **YRatio()=
  0.5625** ⇒ `1/YRatio()=1.7778=16/9`. Default window = 1280×720 (`main_native.cpp:662-663`) ⇒
  **aspect=1.7778**. The horizontal AND vertical FOV are byte-equal; near/far identical (both use
  `cam->NearPlane()/FarPlane()` — RAN `RB3_LIGHT_PROBE=1`: world.cam **near=20 far=20000**, render
  reads the same). Camera-basis convention identical (x=right `world.m.x`, y=forward `world.m.y`,
  z=up `world.m.z`; `Rnd_Wgpu_RB3.cpp:861-863`; Frustum::Set front/back on Y, left/right on X,
  top/bottom on Z). `fovScale` default 1.0 (`NativeSettings`, no-op). **The frustum is NOT the
  defect — the root cause's central premise survives.**

**(2) Empty CPU verts — CONFIRMED.** RAN with `MENU_VOID_DBG2=2`: of 234 dumped venue meshes,
**every single one logs `verts=0 (compressed=N)`** (0 uncompressed) — `Mesh.cpp:474-507` HX_NATIVE
arm reads the Xbox blob into `mCompressedVerts` and `mVerts` stays empty. ⇒ `RndMesh::MakeWorldSphere
(s,true)` and `UpdateSphere()` (`Mesh.cpp:170-224`) iterate the empty `Verts()` and CANNOT recompute
a geometry bound natively. Confirmed.

**(3) The cull's sphere source — CONFIRMED it's the loaded Xbox `mSphere`, and that a CORRECT one
could not have dropped the logo.** `RndDrawable::Load` DOES read `mSphere` from the milo
(`Draw.cpp:300 bs >> mSphere`), so the cull path `MakeWorldSphere(s,false)` → (`Mesh.cpp:219-221`)
`Multiply(mSphere, WorldXfm(), s)` uses the baked sphere, NOT zero. `Multiply(Sphere,Transform)`
(`Sphere.h:43-46`) transforms center but COPIES radius unscaled. **Decisive logic:** `logo.mesh`
renders DEAD-CENTRE on the splash (visible in `/tmp/venue_splash_off.png`) yet its `WorldXfm().v`
origin is **(-1515.8, 1347.0, 136.1)** — thousands of units off the cam axis (cam at
≈110.8,60.1,300.0). `moon.mesh` origin (-4458,2342,517) likewise renders top-left. A *correctly*
baked local sphere (center at the local geometry centroid, radius enclosing it) transformed by
WorldXfm would land the sphere CENTER where the visible triangles are (on-screen, in front) ⇒
`operator>(Sphere,Frustum)` (`Geo.cpp:932-941`, fully-outside-only) returns false ⇒ NOT culled.
**The only way the cull dropped the on-screen logo is a sphere whose center sits at the off-axis
pivot (local center≈0 → world center = WorldXfm.v = the off-axis origin) or whose radius is too
small.** This is exactly the root cause's claim, and the A/B image confirms the symptom: cull-ON
(`venue_splash_on.png`) the "ROCK BAND 3" logo is GONE and large skyline chunks are replaced by
flat planes, while the sky/moon (huge spheres encompassing the cam) survive. **Diagnosis holds.**

**(4) Option A is implementable AND is the load-bearing companion — CONFIRMED.** The decompressed
LOCAL positions exist exactly where Option A wants them: `MeshGpuCache.cpp:312-342`
`EnsureMeshUploaded` static path fills `GpuVertex* verts` via
`VertexFormats::UnpackCompressedVertices(geomOwner->CompressedVerts(), numCompressedVerts, verts,
vertCount)` BEFORE the GPU `WriteBuffer`. Right there one can fold a local min/max → center+radius
and call `mesh->SetSphere(local)` (`Draw.h:72`). Then `MakeWorldSphere(s,false)` → `Multiply(mSphere,
WorldXfm())` yields a world sphere centered on the drawn geometry; skinned meshes keep `s.Zero()`
(`Mesh.cpp:172` mBones branch) so chars are immune. **Without Option A the Draw.cpp seam culls
visible meshes again** (it reuses the same bad loaded sphere) — the PATCH's own warning is correct.

**(5) Patch anchors — VERIFIED EXACT.** `Draw.cpp:156` is the helper-block closing `#endif`
(Site-1 lands before it, beside `MenuVoidDrawHook`). `Draw.cpp:168` is `if (MenuVoidDrawHook(this))
return;` inside `Draw()`'s `#ifdef HX_NATIVE` (Site-2). `Draw.cpp:188` is `if (MenuVoidDrawHook
(this)) return true;` inside `DrawBudget()`'s `#ifdef HX_NATIVE` (Site-3). All three grep to exactly
one occurrence. `#else`/Wii arms (174-179, 200-206) untouched ⇒ Wii byte-identical, 0 match% impact.
DECOMP-file rule satisfied (all edits `#ifdef HX_NATIVE`). DC3 unaffected (RB3-tree Draw.cpp, not
the shared engine). Includes for `getenv`/`strcmp`/`Sphere`/`RndCam` already present.

**(6) Residual caveats (REAL, do NOT refute, but bound the safe-enable envelope):**
- *Non-16:9 windows.* The match is EXACT only at aspect=1/YRatio()=16/9. The cull `ratio` is
  hard-wired to YRatio() (0.5625) regardless of the live window, whereas the render uses live
  `WindowWidth/WindowHeight`. At a non-16:9 window (e.g. `RunGpuSmoke` 256×256, or a resized web
  canvas) the cull horizontal half-angle (1.7778·tanHalf) DIVERGES from the render's (1.0·tanHalf
  at 1:1) → a NARROWER cull than the render → it could clip on-screen meshes. Option B (extract the
  real viewProj planes via Gribb-Hartmann into a native-only cull frustum) is the correct fix for
  that regime; Option A alone does NOT cover it. The seam being world.cam-scoped + default-off +
  the project's standard 1280×720 keeps this latent, but a resized/non-16:9 deploy must NOT enable
  the flag until Option B lands. `fovScale≠1` is the same class (render-only knob; cull ignores it).
- *Scaled WorldXfm radius.* `Multiply(Sphere,Transform)` copies radius unscaled. If a venue mesh's
  WorldXfm carries non-unit scale, even an Option-A-correct LOCAL radius is wrong after transform.
  This is IDENTICAL to Wii behavior (the Wii cull also used unscaled `Multiply`), so it is not a
  *new* native regression and the conservative fully-outside `operator>` tolerates modest error —
  but it is why the in-song-venue A/B (animated/scaled props) must be checked before flipping the
  default, exactly as the patch's gate requires. Could not measure per-mesh WorldXfm scale (verts=0
  so the local-extent dump is unavailable), so this stays an A/B-gated unknown, not a proven bug.

**NET: NOT REFUTED.** Frustum-correct-at-16:9 = proven from source + live params. Empty-verts =
measured (234/234). Sphere-is-the-defect = proven by the dead-center logo vs off-axis origin +
the A/B image. Option A = implementable at MeshGpuCache.cpp:312-342 and necessary. Patch anchors
= exact, Wii byte-identical, DC3-isolated. The ONLY way this fails is (i) enabling it on a non-16:9
window without Option B, or (ii) a scaled-WorldXfm venue prop surviving with a stale radius — both
caught by the mandatory default-off + splash/main_hub/in-song screenshot A/B before flipping the
default. Do NOT re-enable the P2 seam against the *loaded* mSphere without Option A.

Evidence: `/tmp/venue_sphere_probe_60879.log` (234 meshes verts=0 compressed; moon/logo/building
origins), `/tmp/scale_probe_*.log` (world.cam near=20 far=20000 live), `/tmp/venue_splash_off.png`
(logo+skyline present), `/tmp/venue_splash_on.png` (logo GONE, chunks dropped). Code read:
Cam.cpp:14-190, Geo.cpp:906-941, Sphere.h:1-62, Mesh.cpp:170-224/442-507, Draw.cpp:40-208/259-308,
Rnd.cpp:217/235-238, Rnd.h:49-54, Rnd_Wgpu_RB3.cpp:838-921, MeshGpuCache.cpp:223-342,
main_native.cpp:662-663.

---

### W4-VERIFY world.cam frustum-mismatch ROOT CAUSE + FIX (adversarial REFUTE; RUN-only, no build)

**Task:** try to REFUTE the claim that (1) the native world.cam frustum is NOT the defect — it
matches the WebGPU render projection at 16:9 — and (2) the real defect is an empty/zero per-mesh
`mSphere` (compressed venue meshes have empty CPU `mVerts`), so OPTION A (compute the sphere from
the decompressed Xbox blob + `SetSphere`) is the load-bearing companion to the Draw.cpp seam.
Re-read all cited code independently; RAN the pre-built `native/build-native/rb3-native`
(built Jun 6 03:35). Did NOT build.

**VERDICT: NOT REFUTED. The root cause is correct and the fix is sound — with two implementation
caveats (below) that the orchestrator MUST honor or the seam will either no-op or drop a visible
mesh.** Every load-bearing assertion verified by code-read AND/OR a live run.

#### (1) Frustum IS geometrically correct at 16:9 — CONFIRMED (math, not assertion)
Compared `Frustum::Set(near,far,fovY,ratio)` (Geo.cpp:906-930) against the WebGPU clip matrix
(Rnd_Wgpu_RB3.cpp:876-913) term by term:
- **Convention match:** cull `front.Set(0,1,0,-near)`/`back.Set(0,-1,0,far)` ⇒ axis 1 (Y) = depth;
  top/bottom use Y(depth)+Z(up); left/right use X(right)+Y(depth). The render builds clip from
  `right=world.m.x, fwd=world.m.y, up=world.m.z` with `clip.w=y(depth)`, `clip.y=sy*z(up)`,
  `clip.x=sx*x(right)`. **Same X-right / Y-forward(depth) / Z-up camera-local convention.**
- **Vertical FOV match:** cull top-plane boundary `sy*y - cy*z = 0` ⇒ `z/y = tan(halfY)`. Render
  edge `sy_r*z = w = y` with `sy_r = fovScale/tanHalf` ⇒ `z/y = tanHalf` (fovScale=1). **Identical.**
- **Horizontal FOV match:** cull left-plane `cy*x + sx*y = 0` with `sx = sin(halfY)/ratio` ⇒
  `|x|/y = tan(halfY)/ratio`. Render `sx_r = sy_r/aspect` ⇒ edge `|x|/y = aspect*tanHalf`. At 16:9,
  `ratio = (h/w=1)*(mUnknownFloat=1)*(YRatio=0.5625)` so cull `|x|/y = tanHalf/0.5625 = 1.7778*tanHalf`
  and render `|x|/y = (aspect=1.7778)*tanHalf`. **Identical.** near/far/yFov are read from the SAME
  RndCam. ⇒ FOV/aspect/NDC are demonstrably NOT the defect at the default 1280x720 (16:9).
- `mWorldFrustum` stays LIVE: `RndCam::UpdatedWorldXfm` (virtual override, Cam.h:30 / Cam.cpp:184)
  recomputes `Multiply(mLocalFrustum, world, mWorldFrustum)`; it is invoked from
  `RndTransformable::SetWorldXfm`/`WorldXfm_Force` (Trans.cpp). So a moving world.cam re-derives the
  cull frustum each pose. CONFIRMED.

#### (2) The real defect — empty mSphere — CONFIRMED by a live run
RAN headless to splash_screen (`RB3_INTRO_SECS=0 RENDER_DBG=30 MENU_VOID_DBG2=2`,
/tmp/wave04_probe.log):
- `[render f30/f60] cam=world.cam pos=(110.76,60.11,299.97) meshes=823 tris=~240k-252k` — venue
  draws every frame under world.cam (reproduces W2-C/Wave-03 exactly).
- **234 of 234 keyed venue backdrop meshes report `verts=0 (compressed=N)`** (e.g. compressed=1632,
  1384, 202…). **ZERO meshes have uncompressed CPU verts.** This is the HX_NATIVE PostLoadVertices
  arm (Mesh.cpp:474-507): the Xbox blob is read into `mCompressedVerts`, `mGeomOwner->mVerts` stays
  empty, early-return. CONFIRMED at runtime.
- `mSphere` is NEVER populated for these meshes: it is loaded from the stream ONLY for `gRev < 0xF`
  (Mesh.cpp:680) — RB3 mesh rev is far higher (MESH_REV_SEP_COLOR=0x25; PostLoad tests gRev>0x25) —
  and `RndMesh::UpdateSphere` (the only other writer, via `SetSphere`) is **never called on native**:
  it is invoked solely by the `update_sphere` DataArray handler (Draw.cpp:399) and is NOT in PostLoad;
  the native `RndMesh::OnSync` (MeshGpuCache.cpp:94-99) just flags `uploaded=false`. The Wii
  `OnSync`/patch-gen path that would have run is `#ifndef HX_NATIVE`-gated out (Mesh.cpp:1065-1144).
  ⇒ `mSphere.radius == 0` (RndDrawable ctor zeroes it, Draw.cpp:163). VERIFIED by code-read.

#### (3) Why the prior P2 dropped the logo + cityscape — FULLY EXPLAINED (consistent with this RC)
With empty `mVerts`, a cull via `MakeWorldSphere(s,true)` (recompute-from-geometry) runs
`CalcBox(this,box)` (Utl.cpp:202) which iterates `Verts()` — **empty ⇒ Box never GrowToContain'd ⇒
default/degenerate box**; `CalcBoxCenter` yields a bogus center; the FOREACH over empty Verts leaves
`radius=0`. A radius-0 sphere at a bogus, off-axis point is culled by `operator>(Sphere,Frustum)`
(Geo.cpp:932) whenever that single point is outside any plane. The drawn TRIANGLES are elsewhere:
`logo.mesh` origin=(-1515.8,1347.0,136.1) and `moon.mesh` origin=(-4458.2,2342.7,517.1) (from the live
MENU_VOID_DBG2 dump) are thousands of units off the cam axis (cam at 110.8,60.1,300.0) yet both render
on screen (prior-wave screenshots; W3-VERIFY confirms moon visible top-left). ⇒ origin-off-screen ≠
triangles-off-screen ⇒ the prior cull dropped the logo. **The frustum was innocent; the empty sphere
was the killer. The new RC's reframing is correct.**

#### (4) The fix — SOUND, with TWO mandatory caveats
- **Draw.cpp seam (SITES 1-3): byte-identical-safe + correctly placed.** Verified live line numbers:
  `#endif` at 156 (SITE 1 helper lands before it, in the existing HX_NATIVE helper block with
  MenuVoidDrawHook); SITE 2 after `if (MenuVoidDrawHook(this)) return;` at Draw.cpp:168 (wraps the
  `DrawShowing();` at 172); SITE 3 after `if (MenuVoidDrawHook(this)) return true;` at Draw.cpp:188
  (wraps `return DrawShowingBudget(f);` at 198). All strictly inside `#ifdef HX_NATIVE` ⇒ Wii build
  byte-identical, 0 match% impact. Required includes (`<cstdlib>`,`<cstring>`,`rndobj/Cam.h`,
  `math/Geo.h`/Sphere) all already present. world.cam strcmp-scoped ⇒ cannot touch the 2D splash logo
  (UI cam), game.cam highway, or HUD. Default-off (`RB3_VENUE_FRUSTUM_CULL`) ⇒ shipped build inert.
- **CAVEAT A (the seam ALONE is a NO-OP):** because `mSphere.radius==0` for every venue mesh,
  `MakeWorldSphere(s,false)` returns false (Mesh.cpp:219-223) ⇒ the helper's `if(!MakeWorldSphere)
  return false` ⇒ it NEVER culls. The Draw.cpp patch without OPTION A changes nothing (safe but
  useless). **OPTION A is not optional — it is load-bearing, exactly as the spec states.**
- **CAVEAT B (OPTION A must measure the radius in WORLD space, not naive local):** `Multiply(Sphere,
  Transform,out)` copies the radius UNSCALED (Sphere.h:43-46) and the renderer draws each mesh as
  `local_pos * mesh->WorldXfm()` (Rnd_Wgpu_RB3.cpp:3013 `MiloXfmToColMajor(mesh->WorldXfm(), obj.world)`).
  If OPTION A computes a naive local-space radius from the decompressed positions and any venue mesh's
  WorldXfm has scale ≠ 1, the world sphere UNDER-COVERS ⇒ could drop a visible (scaled) mesh — the very
  failure this whole exercise is trying to avoid. The Wii avoids this because `RndMesh::UpdateSphere`
  (Mesh.cpp:170-179) measures the radius in WORLD space (`MakeWorldSphere(s,true)` transforms verts by
  WorldXfm first) then inverse-transforms the CENTER back to local while COPYING the world radius — so
  the stored local `mSphere` carries a world-scale radius, and the draw-time `Multiply` re-applies the
  same world radius. **OPTION A must replicate that** (decompress → transform positions by WorldXfm →
  measure world-space center+radius → inverse-transform center, keep radius; i.e. literally feed the
  decompressed verts into the existing UpdateSphere logic), OR it must conservatively grow the radius
  by the max WorldXfm row-length. A naive `min/max of local positions` sphere is the one way OPTION A
  could still drop a visible mesh. (This is the surgical-decompose spot: MeshGpuCache.cpp:280-282 /
  315-317, where `verts[]` already holds the decompressed local positions right after
  `UnpackCompressed*Vertices`.)
- OPTION B (Gribb-Hartmann planes from the live viewProj) correctly characterized as optional
  hardening ONLY for the non-16:9 regime (e.g. the 256x256 RunGpuSmoke where the cull is hard-wired to
  16:9 via YRatio while the render uses live WindowWidth/Height + fovScale). B alone does NOT fix the
  empty sphere, so A remains required. Agreed.

#### Residual concern (where it could still go wrong)
1. **Scaled venue meshes (CAVEAT B).** If OPTION A is implemented as naive-local-min/max and any
   backdrop mesh has a non-unit WorldXfm scale, that mesh can be wrongly culled. Mitigated entirely
   by computing the radius in world space (Wii-parity) — but the spec's prose ("compute the mesh
   local bounding sphere") is ambiguous on this and could be mis-implemented. MUST be world-radius.
2. **Animated/looping vignette meshes.** The splash cityscape is animated (tri count varies
   240k→252k frame-to-frame ⇒ `cityscape_clips.milo`). A sphere computed once from the bind/first
   frame could under-cover a mesh that later animates outward. Low risk for backdrop props but a
   real in-song-venue concern (the same world.cam path serves gameplay venues) — exactly what the
   orchestrator's in-song-venue A/B screenshot must check before flipping default. Default-off makes
   shipped behavior unchanged regardless.
3. **Group spheres.** Groups cull via `RndGroup::MakeWorldSphere` (a separate sphere grown to contain
   children). If a GROUP also has an empty/zero sphere on native, OPTION A must also cover the group
   level, or the group-level seam call is a no-op (children still drawn — safe, just not the full win).
   Worth confirming the cull rate accounts for the group vs leaf split.

**Disposition:** the world.cam frustum-mismatch RC is REFUTED-as-the-cause (the frustum is correct);
the empty-mSphere RC is CONFIRMED. The Draw.cpp seam is safe-to-apply (inert without A; world.cam +
default-off). The fix WILL work and will NOT drop visible meshes **iff** OPTION A computes the world
sphere with a WORLD-space radius (Wii-parity), and the orchestrator A/Bs splash + main_hub +
in-song-venue screenshots (assert logo + moon + skyline + every visible backdrop survive AND
meshes/tris drop in RENDER_DBG) before flipping the default. Do NOT ship the seam without OPTION A
(no-op) and do NOT implement OPTION A as a naive local-radius (CAVEAT B). Stays Wii byte-identical
(seam HX_NATIVE-gated) and DC3-isolated (Draw.cpp is RB3-tree-local; OPTION A is in milo-native-engine
but only the mesh-upload path, no DC3 venue concern; both are native-only).

Evidence: /tmp/wave04_probe.log (world.cam 823 meshes; 234/234 venue meshes verts=0 compressed; logo
& moon origins off-axis). Code read: Geo.cpp:906-941 (Frustum::Set + operator>), Rnd_Wgpu_RB3.cpp:
838-913 (WebGPU projection) + 3013 (per-mesh model = WorldXfm), Cam.cpp:155-190 (UpdateLocal/
UpdatedWorldXfm), Cam.h:30, Mesh.cpp:170-224 (UpdateSphere/MakeWorldSphere) + 442-507 (HX_NATIVE
compressed-vert PostLoad) + 680 (mSphere load gRev<0xF) + 1063-1144 (native OnSync vs Wii), Sphere.h:
43-46 (Multiply copies radius), Utl.cpp:202 (CalcBox over empty verts), Draw.cpp:156/163/168/188/
202/399 + Draw.h:46/72, MeshGpuCache.cpp:94-99 (OnSync) + 235-345 (decompress path, OPTION A site),
Trans.cpp:110-144.

### W4-VERIFY world.cam frustum-mismatch root cause + Option A fix (adversarial — try to REFUTE)

Independent re-read of every cited line + RAN the prebuilt `native/build-native/rb3-native`
(built Jun 6 03:35, no rebuild). Goal: refute the claim that the P2 cull dropped the logo +
right side because of a BAD SPHERE (not a bad frustum), and that Option A (geometry-derived
sphere via SetSphere) makes culling safe. **VERDICT: NOT REFUTED. Root cause is sound;
Option A is the load-bearing companion and is required.**

**(1) "Frustum is geometrically correct at 16:9" — CONFIRMED (numerically proven).**
`RndCam::UpdateLocal` (Cam.cpp:155-161): `ratio = (mScreenRect.h/mScreenRect.w) * mUnknownFloat
* TheRnd->YRatio()`. Defaults: screenRect (0,0,1,1) → h/w=1; mUnknownFloat=1 (RndCam ctor,
Cam.cpp:130); `mAspect=kWidescreen` (Rnd.cpp:217) so `YRatio()=kRatio[2]=0.5625` (Rnd.cpp:236).
⇒ ratio = 0.5625. `Frustum::Set` (Geo.cpp:906-930) builds left/right planes with
`sx = sin(halfY)/ratio` ⇒ horizontal half-FOV `= atan(tan(halfY)/0.5625)`. The renderer
(Rnd_Wgpu_RB3.cpp:880,893-895) uses `aspect = WindowWidth/WindowHeight`, `sx_render = (1/tanHalf)
/aspect` ⇒ render horizontal half-FOV `= atan(tan(halfY)*aspect)`. Default + web window are BOTH
1280x720 (main_native.cpp:662-663, main_web.cpp:225-226, Rnd_Wgpu.cpp:284-285) ⇒ aspect=1.7778.
And `1/0.5625 = 1.77778 = 16/9`. So `tan(halfY)/0.5625 == tan(halfY)*1.7778` EXACTLY — verified
in Python for yfov∈{0.60,0.70,0.90}, horizontal match=True to 1e-6. Vertical FOV, near (20), far
(20000) are shared by construction. **At the resolution where the ~10fps web splash stutter was
measured, the cull frustum == the render frustum. FOV/aspect/NDC-z is NOT the defect.**

**(2) world.cam splash params — CONFIRMED on the live binary.** `RB3_LIGHT_PROBE=1`:
`WriteSceneUniforms cam='world.cam' near=20.0 far=20000.0`, eye ≈ (110.76, 60.11, 299.97).
Full cityscape renders (screenshot /tmp/cull_probe_splash.png, 2.16 MB): the **moon is clearly
visible top-left**, the 3D "ROCK BAND 3" logo center, skyline fills the frame incl. the right side.

**(3) "Every venue mesh is GPU-compressed with empty CPU mVerts" — CONFIRMED on the live binary.**
`MENU_VOID_DBG2=2` dumped 235 keyed backdrop meshes: **234/235 report `verts=0 (compressed=N)`,
0 have uncompressed CPU verts.** moon.mesh pos=(-4458.2,2342.7,517.1); buildings span world-X
−6771…+409 (camera at x=110.76); many >1500 units off-axis YET on screen. This is the
PostLoadVertices HX_NATIVE arm (Mesh.cpp:474-507): for compressed (Xbox sv8_a) meshes it fills
`mCompressedVerts`/`mNumCompressedVerts` and `mGeomOwner->mVerts` stays EMPTY, then `return`s.

**(4) "Baked Xbox mSphere survives load and is NOT recomputed from geometry" — CONFIRMED by code
path.** `RndMesh::Load`→`PreLoad`→`LOAD_SUPERCLASS(RndDrawable)`→`RndDrawable::Load` reads
`bs >> mSphere` (Draw.cpp:300, rev>0, DRAW_REV=3). `RndMesh::Load` has NO `UpdateSphere()` call
(grep: UpdateSphere appears only at Mesh.cpp:170 def + the `update_sphere` HANDLE_ACTION). And if
`UpdateSphere()` DID run, `MakeWorldSphere(s,true)` iterates `FOREACH(it,Verts())` — EMPTY on
native → radius stays 0. `Multiply(Sphere,Transform)` (Sphere.h:43-46) copies radius UNSCALED.
⇒ The cull's `MakeWorldSphere(s,false)` (Mesh.cpp:219-223) uses the baked Xbox sphere verbatim.

**(5) DECISIVE deductive argument (the bad-sphere claim is the ONLY explanation consistent with
the observed P2 drop + the proven-correct frustum).** Three exhaustive sphere cases vs the
WAVE-03 ground truth (P2=RB3_VENUE_FRUSTUM_CULL dropped the logo + right side):
  - (i) baked radius==0 → `MakeWorldSphere(s,false)` returns false → patch returns "don't cull"
    → NO mesh dropped. CONTRADICTS the observed P2 drop. ⇒ baked spheres are nonzero.
  - (ii) radius>0 AND correctly encloses the rendered triangles → correct frustum KEEPS it → NO
    on-screen mesh dropped. CONTRADICTS the observed P2 drop.
  - (iii) radius>0 but undersized/mis-centered (sphere centered on the mesh PIVOT, e.g. moon
    pivot x=-4458, but the rendered quad's triangles are elsewhere in NDC) → false-outside →
    DROPS a visible mesh. CONSISTENT.
  Since the frustum is proven correct (1) AND P2 demonstrably dropped on-screen meshes, only
  (iii) survives. This is a deduction, not a guess. The moon is the canary: pivot 4569 units
  off-axis in X, yet rendered top-left → pivot != rendered-triangle-centroid for these vignette
  meshes, exactly the (iii) signature.
  CAVEAT (could not be directly measured w/o a build): I could NOT read the per-mesh baked
  `mSphere` radius at runtime. `/api/objects/<name>` (HttpServer.cpp:567-620 exposes
  `draw->GetSphere()`) returns 404 for moon.mesh/building_*.mesh — the sv8 vignette meshes live
  in a BackdropPanel sub-objectdir NOT reachable by `ObjectDir::Main()->FindObject(...,false,true)`.
  So scenario (iii) rests on the WAVE-03 P2 A/B observation (drop happened) + (1)+(4), not on a
  fresh radius dump. The binary has no `RB3_VENUE_FRUSTUM_CULL`/`CULL_PROBE` symbol (P2 reverted),
  so I could not re-run the exact P2 A/B without building (forbidden).

**(6) Option A is geometrically VALID and is REQUIRED.** `UnpackCompressedVertices`
(VertexFormats.cpp:255-288) writes `gv.pos[]` = raw stored mesh-LOCAL positions — the SAME local
frame the baked sphere lives in and that `MakeWorldSphere(s,false)` transforms via `WorldXfm()`.
The decompressed `verts` array is already materialized in `MeshGpuCache.cpp` (static path
~L314-344, skinned ~L279-311) right after unpack, before the GPU `WriteBuffer` + `delete[] verts`.
That is the exact one-time hook to compute min/max → center/radius and call
`RndDrawable::SetSphere` (Draw.h:72). Because `Multiply(Sphere,Transform)` copies radius unscaled,
a LOCAL geometry sphere there reproduces EXACTLY the Wii sphere semantics (Wii also stores a local
sphere via UpdateSphere→FastInvert and copies radius unscaled at draw). So Option A makes
`MakeWorldSphere(s,false)` yield a world bound that ENCLOSES the drawn triangles ⇒ the
already-matching frustum culls safely. **Without Option A the P2 seam re-drops visible meshes** —
the seam alone is necessary-but-insufficient; the spec is correct that A is load-bearing.

**(7) CORRECTION to the spec's char-immunity rationale (does NOT refute, but must be implemented
explicitly).** The spec says "boned meshes keep s.Zero so chars are immune." That is the Wii
`UpdateSphere` behavior (`mBones.empty()? compute : s.Zero()`, Mesh.cpp:172-178) — but
`UpdateSphere` is NEVER called on native (4), so boned meshes do NOT auto-get `s.Zero()`; they
keep their baked Xbox sphere. Therefore Option A MUST explicitly gate on `mesh->IsSkinned()`
(`!mBones.empty()`, Mesh.h:251 — already computed as `bool skinned` at MeshGpuCache.cpp:238) and
NOT SetSphere for skinned meshes (or SetSphere(Zero) for them). On the CULL side this is moot for
the splash: chars draw under game.cam, the seam is `world.cam`-scoped, and splash has no chars —
but the rule must hold for the in-song venue where both cams exist in a frame.

**(8) PATCH SITES re-verified against the clean tree (P2 reverted; `grep RB3VenueFrustumCull`
=0).** Draw.cpp is RB3's OWN decomp copy; the `#else`/Wii cull arms (L173-179 Draw, L199-206
DrawBudget) are intact and byte-identical (the ORIGINAL `MakeWorldSphere`+`CompareSphereToWorld`
pair). All HX_NATIVE patch anchors present and unique: Site-1 helper before `#endif`@156 (top
HX_NATIVE block); Site-2 `if (MenuVoidDrawHook(this)) return;`@168 then `DrawShowing();`@172;
Site-3 `if (MenuVoidDrawHook(this)) return true;`@188 then `return DrawShowingBudget(f);`@198.
Cam.h already `#include`d (Draw.cpp:2): `CompareSphereToWorld`@63, `sCurrent`@70 available;
`RndCam::Name()` inherited from Hmx::Object (used at Cam.cpp:142); `<cstdlib>`/`<cstring>`
included@11-12. Gate `RB3_VENUE_FRUSTUM_CULL` default-off + `strcmp(cam->Name(),"world.cam")`
⇒ inert until enabled, world.cam-scoped (never the 2D logo/UI cam, never game.cam highway/smasher),
Wii byte-identical. DC3 untouched (its own Draw.cpp; MeshGpuCache.cpp is shared-engine native-only,
no Wii concern).

**(9) Residual risk (Option B, real but NOT the splash defect).** The cull frustum is hardwired to
`YRatio()=0.5625` (16:9) while the renderer uses live `WindowWidth/WindowHeight`. At a NON-16:9
window (e.g. the 256x256 RunGpuSmoke default, or a user-resized window, or fovScale≠1) the
horizontal extents DIVERGE and the cull could clip on-screen meshes again. Option B (Gribb-
Hartmann planes extracted from the renderer viewProj into a native-only global) hardens this, but
B alone does NOT fix the bad sphere — A is still required. For the web/native default 1280x720
splash (where the ~10fps stutter was measured) the frustum already matches, so A is sufficient
THERE; B is forward-looking hardening for non-16:9.

**NET: NOT REFUTED.** (1) frustum-correct-at-16:9 is numerically proven; (3)(4) bad-sphere
mechanism confirmed by code path + live mesh dump; (5) the bad-sphere explanation is the ONLY
case consistent with the observed P2 drop + correct frustum (deduction); (6) Option A is
geometrically valid and required; patch sites (8) verified. Concerns: I could not directly dump
the per-mesh baked radius (sub-objectdir 404) so case (iii) leans on the WAVE-03 P2 A/B
observation, and the spec's char-immunity wording needs the explicit `IsSkinned` gate in Option A
(7). Neither overturns the root cause or the fix.
