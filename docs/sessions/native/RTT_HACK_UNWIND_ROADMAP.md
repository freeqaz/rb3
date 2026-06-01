# RB3 Native Port — RTT + song_select Hack-Removal Roadmap

**Authored:** 2026-06-01 (Opus, via a 6-agent scoping workflow). **Companion to:**
[`OFFSCREEN_RTT_INVESTIGATION.md`](OFFSCREEN_RTT_INVESTIGATION.md) (current RTT state
+ hack audit). **Engine pin:** `070562a`.

This sequences the 5 scoped features into a dependency-ordered, risk-minimized plan
whose end-state removes the `RB3SongSelectHideAlbumSmear` hack family and the
details-pane PropAnim hack. Per-feature implementation specs are in the appendix.

> **IMPLEMENTATION STATUS (2026-06-01).**
> - ✅ **capture-hygiene** — LANDED (`88cc9d52`). `RB3RenderFreshHeadlessFrame()` renders
>   a fresh frame before each `/api/screenshot`; same-batch consecutive captures proven
>   byte-identical. Native visual verification is now trustworthy.
> - ✅ **smear-removal Stage 1** — LANDED (`bb077020`). Native-only album cover-hide
>   deleted; depth-6 top-right now shows the cover correctly (was blanked grey). Etched
>   hide + `App.cpp` call kept (await postproc-rtt).
> - ⚠️ **propanim-snap** — STILL OPEN; details-pane hack is **LOAD-BEARING** (kept).
>   CORRECTION (2026-06-01, v2 details stage): the earlier "terminal keyframe already
>   applies / hack is inert" claim was **WRONG** — it came from a misleading DTA query
>   (bare `{song_select_details showing}` returns 0 because the symbol doesn't resolve at
>   root). The proper nested probe `{{song_select_panel find song_select_details} showing}`
>   shows that with the hack disabled, a TRIGGERED terminal `showing=FALSE` keyframe
>   (`details_hide.trg`) does **NOT** apply natively (pane stays `showing=1` after
>   `{details_hide.trg trigger}` AND `play_end_of_anims`). So the `RB3_NO_DETAILS_FIX`
>   glue hack is genuinely needed and was KEPT. (Note: the *continuously-driven* ObjectKeys
>   postproc interp DOES work natively — see the gameplay grade below — so the gap is
>   specific to one-shot *triggered* terminal keyframes, a real future engine fix.)
> - ✅ **drawrect** — LANDED (engine `5cfaf30`). Self-contained `BandRnd::DrawRect` +
>   shared 2D-quad pipeline; rect-composite layers now paint. Verified (fires + group-0
>   restore + `RB3_RTT_OFF` no-crash; live RTT-outfit path uses the planned fallback).
> - ✅ **postproc-rtt** — LANDED (engine `f7e0fd6`). `BandRnd` now honors
>   `RndPostProc::Current()` per-screen via an offscreen intermediate + grade composite
>   (self-contained — `PostProcPass`/`Bloom`/`Dof` are OFF for the RB3 backend,
>   `native/CMakeLists.txt:66,185`; grade WGSL ported verbatim). Noise v1-skipped (env
>   postprocs use a tiled noise *texture* at intensity 3.0; procedural fallback washes
>   gray — proper grain is v2). `RB3_PP_OFF`/`RB3_RTT_OFF` A/B gates. Canary clean
>   (neutral-postproc identity passthrough), clouds RTT intact, zero Dawn aborts.
> - ✅ **smear-removal Stage 3** — LANDED (rb3 `017c20ef`). Removed the `etched_art`
>   hide + the `App.cpp` per-frame call. Verified hide-OFF on the **web** swapchain with
>   a real cover (Marilyn Manson): cover present, no center smear, etched groups showing.
>   A controlled A/B (proved `getenv` reads `Module.ENV` via `RB3_SMEAR_DBG`) confirmed
>   the clean capture was genuinely hide-off. The prior center-left smear does NOT
>   reproduce on the merged engine.
>
> Both engine features merged to engine `main` `c70be2a`; rb3 pin bumped (`15f166cc`).
> **All song_select album hacks are now retired.**
>
> **V2 + Tier-2 LANDED (2026-06-01, engine `main` `dc95098`, rb3 pin `67b59ef7`).**
> - ✅ **PostProc noise grain** (engine `960f804`) — binds the real tiled noise texture +
>   `kNoiseGain` attenuation + midtone mask; subtle film grain, **no gray-wash**.
> - ✅ **Bloom** (engine `2c2ee04`) — `BloomPass` wired into the composite; localized glow,
>   no blowout.
> - ✅ **outfit-RTT verify** (`RB3_PRECLEAR` seam, engine `d8608e4` + rb3 `e8352ca`) —
>   `DrawRect` RTT-outfit tint proven correct (`mod==matColor`); dispatch gated (off by
>   default — the default `BeginFrame` doesn't call `DrawPreClear`).
> - ✅ **Tier-2 venue-only grade layering** (engine `132add5`, merged via `dc95098`) — the
>   grade applies to the VENUE only; the gem-highway + fret-buttons + HUD draw OVER the
>   composite **ungraded**. Merged with the concurrent note-highway depth fix (`6498fab`);
>   the Tier-2 `ClearDepthForOverlay` subsumes it (venue-grade flush + depth-clear, with a
>   plain depth-clear fallback). Verified: during an authored B&W camera shot the venue is
>   B&W while the gem highway/fret-buttons render in **color** on top (strike-plate sat 0.09
>   vs venue 0.01); steady-state gameplay fully colored; song_select/menus unchanged.
>
> **CORRECTION — there was NO gameplay color regression.** A mid-investigation alarm
> ("gameplay renders grayscale") was a **misdiagnosis**: captures were taken during the
> song's *authored* intro B&W camera shots. Tier-1 instrumentation proved the venue
> postproc saturation holds the correctly-authored value at all times (0 at steady-state =
> fully colored; −100 only during authored intro B&W). No value fix was needed. Tier-2 is
> still a genuine improvement (gameplay HUD/gems stay colored even under an authored B&W
> venue grade, matching retail layering).
>
> Remaining v2 follow-ups (non-blocking): texture-driven PostProc noise at full fidelity
> (grain is in but tuned conservatively); enabling the outfit `DrawPreClear` dispatch by
> default; the triggered-terminal-PropAnim engine fix (would retire the details-pane hack).

Feature slugs:
- **drawrect** — `BandRnd::DrawRect` (engine, M)
- **postproc-rtt** — PostProc-driven offscreen intermediate + composite in BandRnd (engine, M)
- **propanim-snap** — Native PropAnim terminal-keyframe apply (engine, M)
- **smear-removal** — song_select album-smear workaround removal (rb3-glue, M)
- **capture-hygiene** — Headless `/api/screenshot` frame hygiene (native glue, M)

> **Correction to the investigation doc §3.** `etched_art` is NOT a render-to-texture
> effect — the `etched_art.grp` meshes draw **directly** to the framebuffer (plain
> `Group`s, no `TexRenderer`). The "etched glass" look on Wii comes from a **full-frame
> `RndPostProc` color-grade** (`B+W_film02.pp`, Selected each Poll by
> `MetaPanel::UpdatePostProc`). The native gap is that **BandRnd never honors
> `RndPostProc::Current()` at all** — so the frame is never graded. The fix
> (**postproc-rtt**) is to render the main frame into an offscreen intermediate and run
> the engine's existing `PostProcPass` (exactly what DC3's `WgpuRnd` already does), NOT
> to RTT the etched meshes.

---

## 1. Recommended sequence

The only hard edge in the dependency graph is **smear-removal Stage 3 depends on
postproc-rtt**. Everything else is independent. capture-hygiene gates *trustworthy
native verification* for all the rest, so it leads.

| # | Feature | Layer | Effort | Why this position |
|---|---|---|---|---|
| 1 | **capture-hygiene** | native glue (no engine pin) | M | Makes every later native `/api/screenshot` trustworthy (kills the "transient lingers 1-2 frames" alias). Fix the instrument before measuring. |
| 2 | **smear-removal (Stage 1)** | rb3-glue | S | Pure deletion of the `#ifndef __EMSCRIPTEN__` cover-hide (`rb3_game_input.cpp:1097-1127`). No engine change. §2 already proved it blanks correct art on `070562a`. Highest unblock-per-risk today. |
| 3 | **drawrect** | engine | M | First engine change; net-additive (DrawRect is empty `{}` today), zero default-path regression. Safe first pin bump. Independent. |
| 4 | **propanim-snap** | engine | M | Touches `AnimTask::Poll` (drives *every* animation) → higher blast radius; do after the proven-safe additive engine change. Confidence medium — confirm root cause empirically first. Unwinds `RB3_NO_DETAILS_FIX`. |
| 5 | **postproc-rtt** | engine | M | Heaviest (intermediate tex + composite pass + possible CMake source-list change). Sole blocker for smear-removal Stage 3. |
| 6 | **smear-removal (Stage 3)** | rb3-glue | S | Strictly after #5 is verified **on web**. Deletes the etched-group hide + the `App.cpp:528-532` per-frame call → full teardown. |

Graph: `drawrect`, `propanim-snap`, `postproc-rtt` are mutually independent engine
changes; `smear-removal` splits — Stage 1 has no dependency (do early), Stage 3
depends on `postproc-rtt`.

---

## 2. Start here (first PR) — capture-hygiene

The only feature with **no engine pin bump** (native-only `BeginDrawing/UI.Draw/
EndDrawing` trio; leave `GpuDevice.cpp` unchanged). It de-risks every subsequent
native screenshot — the investigation repeatedly caveats that headless `/api/screenshot`
retains stale pixels (§2, §5-item3), which is what makes "is the smear gone? / did the
tint paint?" unreliable. Low risk: helper early-returns unless `IsHeadless()`.

Steps: (1) add `void RB3RenderFreshHeadlessFrame()` in `native/src/main_native.cpp`
(early-return unless `gBandRnd.mGpuReady && gBandRnd.Gpu().IsHeadless()`; run the draw
trio under the `sigsetjmp` guard from `App::RunOneFrame:543`; **no** `PresentFrame`; use
the trio not full `RunOneFrame` to avoid double-polling the game tick). (2) call it in
`RB3HttpServer::HandleScreenshot` (`rb3_http_handlers.cpp:66-87`) right before
`ReadbackHeadlessFrame` (`:76`). (3) confirm `HandleScreenshot` runs main-thread
post-`EndDrawing` (the `QueueAndWait` blocks the HTTP worker). (4) canary: two
consecutive captures of a static scene byte-identical; gameplay diff empty.

---

## 3. Quick wins available NOW (no engine change, current pin `070562a`)

**A. capture-hygiene** (the first PR) — native glue only.

**B. smear-removal Stage 1** — delete the native-only cover-hide. §2 *empirically
proved* `album.mesh` renders correctly in the top-right on `070562a` and this block is
*blanking correct art* (depth 6: "ROCKBAND cover → empty grey").
- Delete the `#ifndef __EMSCRIPTEN__` block at `rb3_game_input.cpp:1097-1127`.
- **Keep** the etched-group hide (`:1086-1095`) + function shell (`:1069-1080`) — those
  wait on postproc-rtt.
- Narrow the comment block (`:997-1068`): delete the false "skinning is a native
  no-op" paragraphs; point forward to postproc-rtt.
- `#ifndef`-guarded, so web is untouched by Stage 1 (verify the web smear stays absent —
  the etched hide still runs there; open question whether the `Symbol==` etched hide
  even fires on web).

**C. smear-removal Stage 2 data enabler** — symlink/copy `<song>_keep.png_xbox` from
`orig-assets/extracted-xbox-full/songs/<name>/gen/` into
`orig-assets/extracted/songs/<name>/gen/` for ≥1 `(album_art TRUE)` song (e.g.
`20thcenturyboy`). Pure data; needed to verify real per-song covers load on native.
**Recommend folding permanently into the asset-extraction step** (same class as the
`SONGSELECT_FIX.md` `ui/image/` populate).

> Everything below requires editing `../milo-native-engine` + a `MILO_ENGINE_PIN` bump
> — coordinate per §5.

---

## 4. Unified verification harness

Two surfaces, one philosophy: **web is authoritative for the smear** (real Marilyn
Manson cover loads, double-buffered, no readback retention — `album-art-check.mjs`
hardcodes "The Beautiful People"); **native is the fast loop (~3s rebuild)** for
tint/RTT/PropAnim but only *trustworthy* once capture-hygiene lands.

| Feature | Native (`song-select-capture.py`) | Web (`album-art-check.mjs`) | Regression canary |
|---|---|---|---|
| capture-hygiene | 2 consecutive captures of a static scene byte-identical; hide a mesh then capture — absent in a *single* shot | n/a (headless-only helper) | Gameplay + song-select diff empty; mesh count unregressed |
| smear-removal St.1 | `--depths 0,6,12` on a Stage-2 real-cover song: cover correctly top-right, not full-extent; art-less song shows blank placeholder | baseline then post-St.1: web `#ifndef`-guarded → smear stays **absent** | Non-song-select gameplay pixel-unchanged |
| drawrect | char outfit composes with tint colors not flat; `RB3_RENDER_DBG=1` → `BeginDrawTarget` before patch `DrawMesh`; `RB3_RTT_OFF=1` paints main pass no crash | spot-check once | Gameplay frame with NO outfit compose pixel-for-pixel vs pre-change |
| propanim-snap | `RB3_NO_DETAILS_FIX=1`: `/api/dta/eval {song_select_details showing}`→`0`, `details_mode`→`0`; + unit canary (one `showing=FALSE` BoolKeys @ frame10, `Animate(9.99,10,kTaskUISeconds,0,0)`, one zero-delta poll, assert `Showing()==false`) | inherits via shared `src/` | Gameplay (BandDirector `SetFrame`) unchanged; `main_hub→song_select` still completes; 24000-frame sweep exit 0 |
| postproc-rtt | `RB3_NO_ETCHED_ART_FIX=1` depths 0/6/12: etched top-right B+W-graded not raw; log fires only on song_select | **authoritative**: cover PRESENT, no center smear | Gameplay + main_hub diff empty (intermediate inactive when `Current()==null`) |
| smear-removal St.3 | etched top-right correctly graded, not blanked | **authoritative gate**: smear absent *with etched hide removed* before merge | Gameplay unchanged |

**Universal canary (every engine change):** main scene pixel-unchanged when no
TargetTex/PostProc/DrawRect/snap path active; `BandRnd: frame drawn — N meshes`
unregressed. Cheap (~3s rebuild), catches default-path drift.

---

## 5. Coordination & risks

**Pin bump** (`MILO_ENGINE_PIN` in `native/CMakeLists.txt`, now `070562a`): engine is
soft-pinned + shared; concurrent `web-chars` worktrees edit `Rnd_Wgpu_RB3.cpp`.
Coordinate before editing — especially `Rnd_Wgpu_RB3.cpp` (touched by **drawrect** AND
**postproc-rtt**). Land in engine → commit → bump pin → re-pin. No
`stash`/`revert`/`checkout`/`restore` in either repo with concurrent agents; use
`tools/setup-worktree.sh`. Re-check matched-fn match% after big landings.

**Pin-churn sequencing:** drawrect + postproc-rtt both edit `Rnd_Wgpu_RB3.cpp/.h` — if
one agent owns both, land drawrect first (proves the loop), then postproc-rtt on top.
propanim-snap touches `Anim.cpp` (different file) → can proceed in parallel.

**Cross-feature risks:**
- **propanim-snap = highest blast radius** — `AnimTask::Poll` drives gem track, venue
  cam, char clips, all UI. Gate the terminal-snap strictly on the *existing* self-delete
  predicate; only ADD a final clamped-terminal `SetFrame` in the branch about to
  `delete this`; never change *when* deletion happens; never touch loop/blend. Land
  under `#ifdef HX_NATIVE` in the matched fork (`rb3 src/system/rndobj/Anim.cpp`, the
  copy `App::RunOneFrame` compiles). **Confirm root cause empirically first** (medium
  confidence; alt root cause = UISeconds reset, different fix).
- **drawrect** — leaving group-0 bound to the 2D layout corrupts the next `DrawMesh`;
  restore `mSceneBindGroup` after `Draw` (DC3 `Rnd_Wgpu.cpp:1859-1860`). Pipeline
  format/depth must match the active pass (RT `mRtFmt` no-depth vs main `mTargetFmt`).
- **postproc-rtt** — main build risk: `PostProcPass.cpp`/`BloomPass.cpp`/`DofPass.cpp`
  may be gated to the dc3 backend; if so the rb3 link breaks until added (resolve that
  open question first). Mid-frame RTT resume must target the *intermediate* via
  `MainColorTarget()`; intermediate format = `mGpu.SurfaceFormat()` (not RTT `RGBA8`).
- **smear-removal St.1 vs St.3 ordering is load-bearing** — keep the etched hide +
  `App.cpp` call until postproc-rtt is verified **on web** (the regression-sensitive
  surface).
- **Headless readback retention is a *measurement* risk** — why capture-hygiene is #1.

---

## 6. Hack → feature traceability

| Hack (file:line) | Class | Unwound by | Order |
|---|---|---|---|
| native-only cover hide + `SetHookTex(false)` — `rb3_game_input.cpp:1097-1127` | HACK (active regression) | smear-removal | **2 (St.1)** |
| etched-group hide — `rb3_game_input.cpp:1086-1095` | HACK | postproc-rtt → smear-removal | **5→6 (St.3)** |
| `App::RunOneFrame` per-frame call — `src/App.cpp:528-532` | HACK | postproc-rtt → smear-removal | **6 (St.3)** (maybe St.1 if etched never re-shows) |
| details-pane hide (`RB3_NO_DETAILS_FIX`) — `rb3_game_input.cpp:~1235-1261` | HACK — **LOAD-BEARING** (kept) | triggered-terminal-PropAnim engine fix (NOT done) | open — a TRIGGERED terminal `showing=FALSE` keyframe is not applied natively (verified via the proper nested DTA probe); the hack stays until that engine fix lands |
| etched_art `01` transform/swap PropAnim | n/a | (n/a — etched hide already removed in St.3 via postproc-rtt) | — |
| CPU-composite fallback (`CHAR_OUTFIT_DIAGNOSIS.md §3`) | pre-empted | drawrect | **3** |
| headless readback-retention quirk (§2/§5-item3) | measurement | capture-hygiene | **1** |
| `ui/image/` + `<song>_keep.png_xbox` populate | GLUE | smear-removal St.2 (promote into extraction) | **2 (companion)** |

**Kept (not unwound, per §4 audit):** select-highlighted-on-confirm (PERMANENT),
stale-row-label clear `MusicLibrary.cpp:~1048` (GLUE), no-op `InvalidateGpuMesh`
(PERMANENT).

---

## Appendix — per-feature implementation specs

Produced by the per-feature scoping agents. `file:line` are as of pin `070562a`.

### A. drawrect — `BandRnd::DrawRect` (engine, M, confidence HIGH)

**Summary.** Implement a `BandRnd::DrawRect` override drawing a single textured,
color-modulated 2D quad into the *currently-active* render pass. Today `Rnd::DrawRect`
is empty `{}` (`rb3/src/system/rndobj/Rnd.h:80`), no override → every rect-composite
layer paints nothing. Unblocks `OutfitConfig::MatSwap::Compose`'s base + two-color
diffuse/interp/mask tint layers (`OutfitConfig.cpp:160/169/175/181`). Mirror DC3's
`WgpuRnd::DrawRect2D` (`engine/src/gfx/DrawRect2D.cpp`) but self-contained in the rb3
backend (DrawRect2D.cpp is DC3-only; hard-depends on `gWgpuRnd`,
`WgpuRnd::CurrentTargetFormat/HasDepth/SampleCount`, `GetGpuTexView`).

**Files.**
- `engine/src/platform/Rnd_Wgpu_RB3.h`: add `void DrawRect(const Hmx::Rect&, const
  Hmx::Color&, RndMat*, const Hmx::Color*, const Hmx::Color*) override;` (near :128-133);
  private `m2dShader/m2dBindGroupLayout/m2dPipelineLayout/m2dVertexBuffer/m2dPipelineReady`
  + `void EnsureRectPipeline();`; null in Shutdown().
- `engine/src/platform/Rnd_Wgpu_RB3.cpp`: implement. **(1)** guard `!mGpuReady||!mInPass`.
  **(2) CRITICAL RTT begin-hook** — replicate DrawMesh's (`:1188-1191`) BEFORE drawing,
  because Compose calls DrawRect *before* any DrawMesh: `if (!RB3RttDisabled() &&
  RndCam::sCurrent) { RndTex* tt = RndCam::sCurrent->TargetTex(); if (tt && tt !=
  mRtActiveTex) BeginDrawTarget(tt); }`. **(3)** lazy 2D pipeline (own WGSL: vec2 NDC +
  vec2 uv + vec4 color; `fs`=textureSample*vertexColor + a `notex` entry). **(4)** map
  rect (absolute Rnd-pixel space) to NDC via `TheRnd->Width()/Height()` NOT framebuffer
  size. **(5)** modulation = `param color * mat->GetColor()` (Compose passes white param
  + sets real tint via `sMat->SetColor`). **(6)** diffuse `GetRB3TexView(mat->
  GetDiffuseTex())` → `mWhiteView` fallback; sampler `mSampler`. **(7)** blend
  `mPipelines.MapBlend((WgpuBlend)mat->GetBlend())`; format `mRtActiveTex?mRtFmt:
  mTargetFmt`; no depth in RT pass, else depth-disabled Depth24PlusStencil8. **(8)** write
  6 verts, draw. **(9)** restore `mSceneBindGroup` at slot 0 after Draw.

**Key edge cases.** Compose calls DrawRect before DrawMesh (hence the begin-hook
replication); tint comes from `mat->GetColor()` not the param (DC3 ignores matColor →
would yield no tint here); base layer has null diffuse (white fallback);
`kColorModAlphaUnpackModulate` mask layer = alpha-as-grayscale (v1 may approximate as
plain modulate); all 4 layers `kBlendSrc` (overwrite, no cross-layer compositing); must
restore group-0 or next DrawMesh aborts in Dawn.

**Deps:** none. **Risk:** low (additive; DrawRect is a no-op today). **Verify:** compose
a char outfit, confirm tints paint; `RB3_RENDER_DBG=1` shows `BeginDrawTarget` before
patch DrawMesh; gameplay-highway pixel-diff empty. **Unwinds:** CHAR_OUTFIT §3
CPU-composite fallback; the rect half of the outfit RTT gap.

### B. postproc-rtt — PostProc intermediate + composite in BandRnd (engine, M, HIGH)

**Summary.** BandRnd never honors `RndPostProc::Current()`. Wire it to render the main
frame into an offscreen intermediate when a PostProc is selected, then composite via the
engine's existing `PostProcPass` — exactly what DC3's `WgpuRnd` does. For song_select,
`MetaPanel::UpdatePostProc()` Selects `B+W_film02.pp` each Poll; natively that grade
never applies, so the `etched_art` relief meshes read as raw geometry that the smear
hack blanks. Unblocks the grade for every PostProc screen (venues, transitions, film).

**Files.**
- `engine/.../Rnd_Wgpu_RB3.h`: add `PostProcPass mPostProcPass;` (+ `#include
  "gfx/PostProcPass.h"`), `mIntermediateTex/View/Width/Height`, `mPostProcFlushed`;
  helpers `wgpu::TextureView& MainColorTarget()` (intermediate when `Current()` else
  `mFrameView`) + `void EnsureIntermediate(int,int)`. (`mBlackView` already exists `:572`.)
- `engine/.../Rnd_Wgpu_RB3.cpp`: **BeginFrame (~873)** before BeginRenderPass: `hasPostProc
  = RndPostProc::Current()!=nullptr`; if so `EnsureIntermediate(W,H)` + `colorAtt.view =
  mIntermediateView`; reset `mPostProcFlushed=false`. **EndFrame (~913)** after
  `mPass.End()` before `mEncoder.Finish()`: if intermediate && Current() && !flushed →
  `mPostProcPass.Run(mEncoder, mIntermediateView, mIntermediateTex, W, H, mDepthView,
  mFrameView, mBlackView, mGpu)`; flushed=true. **EndDrawTarget (~1149)**: change
  `colorAtt.view = mFrameView` → `MainColorTarget()`. **InitGpuResources**:
  `mPostProcPass.Init(mGpu)`. **Shutdown**: Terminate + null. (Mirror DC3
  `Rnd_Wgpu.cpp:617-700/1072-1081/342`; no MSAA — rb3 is 1x.)
- `rb3/native/CMakeLists.txt`: verify `PostProcPass.cpp`+`BloomPass.cpp`+`DofPass.cpp`
  are in the rb3 link (engine gfx CORE; may be dc3-gated — **main build risk**).
- `rb3/native/src/rb3_game_input.cpp`: (Stage-3, after web+native verify) remove the
  etched-group hide + native cover hide.

**Key edge cases.** Mid-frame RTT (`BeginDrawTarget`) must resume into the intermediate
(→ `MainColorTarget()`), not `mFrameView`; intermediate re-create on resize;
`Current()` toggles per frame (panel enter/exit; `Reset()` in MetaPanel::Unload);
**etched_art meshes are plain Groups, NOT TexRenderer — they draw direct, do NOT need
SetTargetTex**; `DoPost()` is CPU-only (the GPU composite is `PostProcPass::Run` from
EndFrame); intermediate format = `mGpu.SurfaceFormat()` NOT RTT `RGBA8`; B+W_film02.pp
likely has no refract/bloom (pure grade → PostProcPass handles it).

**Deps:** none (but gates smear-removal St.3). **Risk:** contained — intermediate path
activates only when `Current()!=null`; default path byte-identical. Build-wiring + the
MainColorTarget + format traps are the real hazards. **Verify:** native depths 0/6/12
with `RB3_NO_ETCHED_ART_FIX=1` → B+W-graded top-right; **web authoritative** (real
cover, no smear); gameplay diff empty. **Open Q:** is PostProcPass in the rb3 link? does
B+W_film02 set a refract map? **Unwinds:** the entire `RB3SongSelectHideAlbumSmear`
(with smear-removal St.3) + `App.cpp:528-532`.

### C. propanim-snap — native PropAnim terminal-keyframe apply (engine, M, MEDIUM)

**Summary.** UI "snap to end of anim" (`UITrigger::PlayEndOfAnims`, used by
`details_hide.trg`) schedules a tiny AnimTask `EndFrame-0.01 → EndFrame` on
`kTaskUISeconds`, relying on the timeline advancing to reach EndFrame. Natively the task
is scheduled inside `TheUI.Poll()` and polled by `TheTaskMgr.Poll()` in the SAME
`RunOneFrame` with **zero UISeconds delta** between → `AnimTask::Poll` runs once at
`time=0 → frame=mMin=EndFrame-0.01` (still `showing=TRUE`), then self-deletes
(`time<mMin`), never hitting EndFrame. So the terminal `showing=FALSE` keyframe never
applies — hence the details-pane stays visible.

**Files.**
- `engine` + `rb3/src/system/rndobj/Anim.cpp` `AnimTask::Poll` (`:353-390`, matched-fork
  mirror under `#ifdef HX_NATIVE` — already diverged at `:378-384`): when the task is
  about to self-delete (range exhausted), recompute `frame = (mScale>=0) ? mMax : mMin`
  and `SetFrame(frame, blend)` ONE LAST time before `delete this`. Behavior-neutral for
  normal anims (their last natural poll already lands at `mMax`/`mMin`); only changes the
  pathological single-poll-at-start case.
- `rb3/native/src/rb3_game_input.cpp:~1235-1261`: delete the `RB3_NO_DETAILS_FIX`
  force-hide once verified.
- `rb3/native/CMakeLists.txt`: bump pin after the engine commit.

**Key edge cases.** Reverse anims (`mScale<0` → terminal is `mMin`); zero-length range
(`EndFrame==0`) may already work; leave loop/blend paths untouched; step bool keys apply
on `mLastKeyFrameIndex` change (snap transitions it); `kDirEvent` ObjectKeys must not
double-fire (reuses SetFrame path → `mLastFrame` guard holds); affects ALL UI snaps
engine-wide (scope at AnimTask level).

**Deps:** none. **Risk:** MEDIUM — `AnimTask::Poll` drives every animation. Gate strictly
on the existing self-delete predicate; only ADD a terminal SetFrame in the
about-to-delete branch; never change when deletion happens; `#ifdef HX_NATIVE` so Wii
match unchanged. **Verify:** `RB3_NO_DETAILS_FIX=1` →
`/api/dta/eval {song_select_details showing}` becomes `0`; unit canary (one BoolKeys
`showing=FALSE` @ frame10, `Animate(9.99,10,kTaskUISeconds,0,0)`, one zero-delta poll,
assert `Showing()==false`); gameplay + 24000-frame sweep unregressed. **Open Q:** confirm
the single-poll-then-delete root cause empirically before coding (alt: UISeconds reset →
different fix); decide locus (A: AnimTask::Poll snap, broad payoff vs B: PlayEndOfAnims
calls SetFrame(EndFrame) directly, more surgical) — lean A under HX_NATIVE. **Unwinds:**
details-pane hide; the transform/swap half of the etched `01` hack.

### D. smear-removal — staged workaround teardown (rb3-glue, M, HIGH)

**Summary.** Verify skinning + mesh-RTT cover the album case, then stage the safe removal
of `RB3SongSelectHideAlbumSmear` (`rb3_game_input.cpp:1069-1128` + `App.cpp:528-532`).
§2 proved the native cover-hide (`#ifndef __EMSCRIPTEN__`, `:1097-1126`) is now an active
regression. The etched-group hide (`:1086-1095`) stays until postproc-rtt.

**Stages.** **St.1 (now, no dep):** delete `:1097-1127`; narrow the comment. **St.2
(data):** symlink `<song>_keep.png_xbox` into `extracted/songs/<name>/gen` for a real
cover (fold into extraction). **St.3 (gated on postproc-rtt, after web verify):** delete
the etched hide (`:1086-1095`) + the whole function + `App.cpp:528-532`.

**Path facts.** Mounted owned song → `BandSongMgr::GetAlbumArtPath` (`BandSongMgr.cpp:341`)
→ `SongFilePath(s,"_keep.png",true)` (`:307`) → `songs/<name>/gen/<name>_keep.png`
(+`_xbox` suffix). `HasAlbumArt` ← `(album_art TRUE)` in songs.dta. `refresh_top`
(`song_select.dta:1432-1541`): mounted song → `{album_art.pic set tex_file {$item
album_art_path}}` (fall-through `:1534`); unmounted/no-art → `blank_album_art_keep.png`.

**Key edge cases.** Art-less songs must still show the blank placeholder via the working
skinned `album.mesh` (not a full-extent quad); async re-show (`UIPicture::FinishLoading→
HookupMesh→SetShowing(true)`, `UIPicture.cpp:158`) is now CORRECT with skinning; headless
readback retention — don't treat a faint retained native smear as live (cross-check
`RB3_SMEAR_DBG`, prefer web); web `Symbol==` etched-hide may silently miss (verify it
even fires on web).

**Deps:** postproc-rtt (St.3 only). **Risk:** low St.1 (pure deletion of proven-regression
code, web `#ifndef`-guarded); medium St.3 (depends on postproc-rtt actually compositing).
**Verify:** **web authoritative** (`album-art-check.mjs`, Marilyn Manson) cover present +
no smear; native real-cover run after St.2; gameplay diff empty + art-less song shows
placeholder. **Open Q:** does etched group re-show via `refresh_top` (→ is the App.cpp
call droppable in St.1)? does the etched hide fire on web at all?

### E. capture-hygiene — fresh headless frame per `/api/screenshot` (native glue, M, HIGH)

**Summary.** `GpuDevice::mHeadlessTex` is created once and reused (`GpuDevice.cpp:314`),
single-buffered. The main pass already clears it every frame (`BeginFrame:891
loadOp=Clear`), so the "transient lingers 1-2 frames" quirk is NOT a missing LoadOp — it
is capture-vs-draw timing: `/api/screenshot` is serviced async and reads back whatever
the single persistent texture held at the last submitted EndFrame, which can be one frame
behind the poll-side hide. Fix: render one fresh full frame into the headless target
immediately before each readback (zero change to the windowed swapchain).

**Files.**
- `rb3/native/src/rb3_http_handlers.cpp` `HandleScreenshot` (`:66-87`): call
  `RB3RenderFreshHeadlessFrame()` before `ReadbackHeadlessFrame` (`:76`); headless-only.
- `rb3/native/src/main_native.cpp` (or `rb3_render_mesh.cpp:408-422`): add
  `RB3RenderFreshHeadlessFrame()` — early-return unless `mGpuReady && IsHeadless()`; run
  `TheRnd->BeginDrawing(); TheUI.Draw(); TheRnd->EndDrawing()` under the `sigsetjmp` guard
  from `App::RunOneFrame:543`; **no** PresentFrame; use the trio not full `RunOneFrame`
  (avoids double-polling the game tick).
- `engine/src/gfx/GpuDevice.cpp`: **leave unchanged** (do NOT add a manual clear — would
  double-clear / blank RTT-resume LoadOp::Load). Optional double-buffer only if needed.

**Key edge cases.** EndFrame nulls `mFrameView`/`mRtActiveTex` so back-to-back frames are
clean; `ReadbackHeadlessFrame` already syncs (WaitAny `:355`); the `RB3_GAME`
auto-screenshot path reads inside EndDrawing (already post-submit, doesn't need this); web
`ReadbackHeadlessFrame` returns false → helper must be headless-only; prefer the Draw trio
over RunOneFrame to keep capture side-effect-free.

**Deps:** none. **Risk:** low (helper early-returns unless headless; one extra rendered
frame per capture). **Verify:** two consecutive captures of a static scene byte-identical;
hide a mesh then capture → absent in a single shot; gameplay diff empty. **Open Q:** does
HandleScreenshot run main-thread post-EndDrawing (QueueAndWait blocks the worker)? **Note:**
`mIntermediateTex` does NOT exist at pin `070562a` (only `mHeadlessTex/View`) — the
investigation §5-item3 mention was a stale assumption. **Unwinds:** the readback-retention
caveat (§2/§5-item3); makes native verification reliable.
