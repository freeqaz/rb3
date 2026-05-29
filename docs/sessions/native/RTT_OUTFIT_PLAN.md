# RTT outfit-patch compositing — implementation plan (engine render-to-texture)

**Authored:** 2026-05-29 (read-only planning subagent). **Status:** plan only — no
build/run/edit done. **Companion to:** `CHAR_OUTFIT_DIAGNOSIS.md` (the root-cause
diagnosis this plan implements the fix for). **Layer:** engine (milo-native-engine),
`MILO_ENGINE_GPU_BACKEND=rb3` / `BandRnd`.

---

## 0. TL;DR

RB3 builds a character's final skin/outfit texture by **rendering layers into a
texture** (a `kRenderedNoZ` render-target `RndTex`): a base color, two-color
diffuse/interp/mask tint passes, then projected patch meshes (tattoos / decals /
face overlays / band logo). Natively this composite is a **no-op at three engine
layers**, so those materials bind an empty render-target bitmap → fall back to
`mWhiteView` (white/unpainted). Bodies/clothing/hair/skin already render via the
simple per-material texture path — this is the missing polish layer only.

**The good news: the DC3 backend (`WgpuRnd`, `Rnd_Wgpu.cpp` + `Tex_Wgpu.cpp` +
`gfx/DrawRect2D.cpp`) already contains a complete, working reference RTT
implementation.** This plan is largely "port the DC3 RTT pattern into BandRnd,
translating to RB3's 2010-era rndobj shapes and the existing single-pass BandRnd
frame model." That makes full RTT achievable, but it is still a meaty
self-contained renderer feature touching three call sites — honestly **effort L**.

A cheap **CPU-tint partial (effort S–M)** recovers the two-color *clothing* tints
(the most visible subset) without any render-pass work, by modulating the tint
textures on the CPU at bind time. It does NOT recover tattoos/decals/face/logo.

---

## 1. The consumer is already live (confirmed)

The RTT *caller* path compiles and runs natively today — only the engine side is
dead. Confirmed by reading:

- `OutfitConfig::RenderToTextures` driver (`OutfitConfig.cpp:959-988`): iterates
  `mMats`, calls `it->SwapResource()`, `mPatches[i].PreRender()`, then
  `it->Compose(mColors, mPatches, unk38)` for non-two-color then two-color mats,
  then `mPatches[i].PostRender()`, then binds `mBandLogo`. This is reached natively
  (V23 wired `LoadMainCharacters`/`SyncTransProxies`; `BandCharacter::SyncOutfitConfig`
  → `OutfitConfig::SetSkinTextures` at `BandCharacter.cpp:1074`).
- `MatSwap::Compose` RTT branch (`OutfitConfig.cpp:119-200`): selects the simple
  path when `diffTex` is null or not `kRenderedNoZ` (`:108-118`, works today);
  otherwise the RTT branch sets `sCam->SetTargetTex(diffTex)` + `sCam->Select()`
  (`:133,143`), draws base + two-color layers via `TheRnd->DrawRect(...)`
  (`:160,169,175,181`), renders patches (`patches[i].Render(diffTex, sMat)` `:186`),
  then `sCam->SetTargetTex(nullptr)` + `prevCam->Select()` (`:191,195`).
- `OutfitConfig.cpp` is NOT stubbed: `native/src/band3_link_stubs.s:317,840`
  explicitly note the OutfitConfig stubs were removed because the TU is compiled.
- `BandPatchMesh.cpp` is compiled + brought up (per the diagnosis); its
  `Render → patch->DrawShowing()` runs, but the pixels go nowhere without the RTT
  target being live.

So: **no matched-fork or glue changes are required.** Every change in this plan is
in the engine renderer.

---

## 2. RTT implementation design (full fix — the 3 layers)

The fix mirrors the DC3 backend's proven structure. The three dead layers from the
diagnosis map 1:1 to three DC3 mechanisms that already exist and just need an RB3
translation:

| Dead layer (RB3 today) | DC3 reference that already works | RB3 port target |
|---|---|---|
| (1) `BandRnd` ignores `cam->TargetTex()`; every draw goes to `mFrameView` | `WgpuRnd::BeginTexturePass` / `SelectRenderTarget` / `FinishRenderTarget` (`Rnd_Wgpu.cpp:724-832`) | `BandRnd` target-switching (new) |
| (2) `RndTex::MakeDrawTarget`/`FinishDrawTarget` are empty (`Rnd_Wgpu_RB3.cpp:1642-1643`) | `Tex_Wgpu.cpp:236-245` (`EnsureRenderTargetData` + `SelectRenderTarget`/`FinishRenderTarget`) | real bodies in `Rnd_Wgpu_RB3.cpp` |
| (3) `Rnd::DrawRect` empty inline (`Rnd.h:80`), no BandRnd override | `WgpuRnd::DrawRect` (`Rnd_Wgpu.cpp:1851`) + `DrawRect2D` helper (`gfx/DrawRect2D.cpp`) | `BandRnd::DrawRect` override (new) |

### Layer A — per-`RndTex` GPU render target + `MakeDrawTarget`/`FinishDrawTarget`

Extend the existing `sTexGpu` side-table (`RB3TexEntry`, `Rnd_Wgpu_RB3.cpp:317-324`)
with render-target fields (mirror DC3's `GpuTexData`):

```cpp
struct RB3TexEntry {
    wgpu::Texture     tex;          // existing (sampled diffuse)
    wgpu::TextureView view;         // existing
    // --- new RTT fields ---
    bool              renderTarget = false;  // tex was created RenderAttachment|TextureBinding
    // (kRenderedNoZ → no depth needed; type 0x22 has no Z bit, so no depth attachment)
    ...
};
```

Add a `RB3EnsureRenderTarget(RndTex* tex)` helper (port of `EnsureRenderTargetData`,
`Tex_Wgpu.cpp:54-102`):
- early-out unless `tex->IsRenderTarget()` (`mType & kRendered`, `Tex.h:125`);
  `kRenderedNoZ == 0x22` has the `kRendered` (0x2) bit set, so it qualifies.
- size = `tex->Width()/Height()`, defaulting to 256×256 if zero (DC3 does the same;
  RB3 render-target sizes come from the milo and should be non-zero, but guard).
- create a `wgpu::Texture` with usage `RenderAttachment | TextureBinding | CopyDst`,
  format = the existing `mTargetFmt` (RGBA8Unorm headless / surface fmt windowed —
  must match the BandRnd pipeline target so the off-screen pass pipeline is valid).
- create its `view`; set `renderTarget = true`. **No depth attachment** for
  `kRenderedNoZ` (the "NoZ" is the whole point — flat 2D composite, depth-disabled).
- clear once to opaque black on creation (DC3 `Tex_Wgpu.cpp:79-88`) so an
  un-composed target reads black not garbage.

Then the no-ops become:
```cpp
void RndTex::MakeDrawTarget()   { gBandRnd.RB3SelectRenderTarget(this); }
void RndTex::FinishDrawTarget() { gBandRnd.RB3FinishRenderTarget(this); }
```
(`Rnd_Wgpu_RB3.cpp:1642-1643`). Note the RB3 call sites are `RndCam::Select`/
`SetTargetTex` (`Cam.cpp:54-55,66-67`) calling `prevCam->TargetTex()->FinishDrawTarget()`
— RB3 does NOT call `MakeDrawTarget` from `SetTargetTex`. So the engine must begin the
texture pass when it *observes* a non-null `TargetTex` at draw time (Layer B), not rely
on a `MakeDrawTarget` call. `FinishDrawTarget` reliably fires on the next `Select()`,
which is the natural place to end the off-screen pass and resume the main pass.

### Layer B — route draws to the active `cam->TargetTex()`

This is the heart of the change and the part where the BandRnd single-pass model
differs most from DC3's multi-pass model. BandRnd today opens exactly one render
pass in `BeginFrame` (`Rnd_Wgpu_RB3.cpp:852-875`) against `mFrameView` and ends it
in `EndFrame`. RTT requires the ability to suspend the main pass, run an off-screen
pass into the target texture, then resume.

Approach (mirrors `WgpuRnd::SelectRenderTarget`/`BeginTexturePass`/`FinishRenderTarget`):

1. Add state to BandRnd: `RndTex* mActiveTargetTex = nullptr;` and track whether the
   current `mPass` is the main pass or a texture pass.
2. Refactor the pass-open code in `BeginFrame` into a private `BeginMainPass()` so it
   can be re-entered, and add `BeginTexturePass(RndTex*)` and `EndActivePass()`
   helpers. (The single `mEncoder` is reused across both passes within the frame —
   `mEncoder.Finish()` stays in `EndFrame`; multiple `BeginRenderPass`/`pass.End()`
   on one encoder is valid WebGPU.)
3. `RB3SelectRenderTarget(tex)`: `EnsureRenderTarget(tex)`; `EndActivePass()`;
   `BeginTexturePass(tex)` (color attachment = the target's view, `LoadOp::Clear`,
   **no depth attachment**, viewport = full tex W×H). Set `mActiveTargetTex = tex`.
4. `RB3FinishRenderTarget(tex)`: if `mActiveTargetTex == tex`, `EndActivePass()`;
   `mActiveTargetTex = nullptr`. (The subsequent `prevCam->Select()` in `Compose`
   triggers `FinishDrawTarget` on the cam's old target via `Cam.cpp:54-55` — that is
   the resume trigger. After it, the next `DrawMesh`/`DrawRect` re-opens the main
   pass on demand if none is active, OR the engine resumes the main pass explicitly.)
5. **Critical pass-state plumbing:** the simplest reliable model is "there is always
   exactly one open pass; switching targets ends the current and begins another."
   When `mActiveTargetTex` clears, immediately `BeginMainPass(LoadOp::Load)` (preserve
   what was drawn) so subsequent body meshes continue into the main framebuffer. The
   main pass on resume must use `LoadOp::Load` (not Clear) for both color AND depth so
   the already-drawn scene + depth survive (DC3 has the identical Load-depth caveat at
   `Rnd_Wgpu.cpp:488-489`).
6. `WriteSceneUniforms` / the per-pass viewport: the Compose cam (`sCam`) is set to a
   unit ortho-ish quad cam (`SetFrustum(0.01, 5.0, 0.0, 1.0)`, world xfm at
   `OutfitConfig.cpp:136-142`). But `DrawRect` (Layer C) draws in 2D screen space
   directly (NDC from `rect / Width()×Height()`), so the **scene viewProj is mostly
   irrelevant for the rect passes** — what matters is the viewport covering the full
   target and the target's color attachment. For the *patch* meshes
   (`patches[i].Render`, which go through `DrawMesh`), the Compose cam's projection
   does matter; BandRnd's existing per-cam `WriteSceneUniforms` re-write
   (`DrawMesh:1081-1095`, triggered when `RndCam::sCurrent` changes) already handles
   selecting the Compose cam's view when `sCam->Select()` runs. Verify the BandRnd
   ortho/perspective build (`WriteSceneUniforms:770-797`) produces a usable
   projection for the patch-projection cam; if not, the patches layer may need a
   dedicated ortho path (see Risks).

### Layer C — `BandRnd::DrawRect` (the tint/layer painter)

Override the empty `Rnd::DrawRect` (`Rnd.h:80`). **Signature note:** RB3's
`Rnd::DrawRect` is `(const Hmx::Rect&, const Hmx::Color&, RndMat*, const Hmx::Color*,
const Hmx::Color*)` — color BEFORE mat, and NO `ShaderType` arg. This differs from
DC3's `WgpuRnd::DrawRect(const Hmx::Rect&, RndMat*, ShaderType, const Hmx::Color&,
const Hmx::Color*, const Hmx::Color*)`. So the BandRnd override cannot be a copy-paste
of the DC3 declaration — match RB3's order.

Two implementation options:

- **C1 (preferred, reuse): use the engine's `DrawRect2D` helper** (`gfx/DrawRect2D.{h,cpp}`).
  It already builds a screen-space quad (NDC from `rect / TheRnd.Width()×Height()`,
  `DrawRect2D.cpp:107-114`), samples the mat's diffuse via `GetGpuTexView`, maps
  `mat->GetBlend()`, and draws into the *current* pass at the *current* target format.
  BandRnd would: add a `DrawRect2D mDrawRect2D;` member, `Init` it in
  `InitGpuResources`, and in `BandRnd::DrawRect` call
  `mDrawRect2D.Draw(mPass, rect, mat, color, topRight, botLeft, mGpu, mPipelines,
  mWhiteView, mSampler)` then restore `mPass.SetBindGroup(0, mSceneBindGroup,...)`
  (exactly `WgpuRnd::DrawRect`, `Rnd_Wgpu.cpp:1851-1861`).
  **Caveat:** `DrawRect2D::Draw` references the DC3-only globals `TheRnd` (a global
  `WgpuRnd&`, not RB3's `Rnd* TheRnd`), `gWgpuRnd`, `GetGpuTexView`, and
  `rnd->CurrentTargetFormat()` (`DrawRect2D.cpp:107,149,158,162`). Those resolve to
  the DC3 backend symbols, which are OFF for RB3 (`MILO_ENGINE_BUILD_GPU_BACKENDS=OFF`).
  So `DrawRect2D.cpp` as-written will NOT link in the RB3 build. To reuse it, either
  (a) parameterize those three couplings (pass width/height, target format, and a
  tex-view lookup as args/callbacks) so the helper is backend-agnostic, or (b) accept
  a small amount of duplication and write a self-contained BandRnd quad blit (C2).
  Option (a) is the cleaner long-term move and matches the header's "future-modularity"
  note (`Rnd_Wgpu_RB3.h:16-21`).

- **C2 (self-contained): write a minimal quad blit inside `Rnd_Wgpu_RB3.cpp`.**
  Build 6 verts in NDC from `rect / TheRnd->Width()×Height()`, set UVs 0..1, write to a
  small reusable vertex buffer, bind the mat's diffuse view (via `GetRB3TexView` /
  `UploadRndTexIfNeeded`) or `mWhiteView`, pick a pipeline by `mat->GetBlend()` +
  `targetFormat = mTargetFmt` (the existing `PipelineManager::GetPipeline(key)` can
  serve a simple textured-quad pipeline, or add a tiny 2D shader like DC3's). Honor the
  ColorMod semantics below. This is ~80-120 lines, no cross-backend refactor, lowest
  coupling risk. Given the rest of `Rnd_Wgpu_RB3.cpp` is already self-contained
  (it re-implements DXT decode, vertex unpack, etc. precisely to avoid DC3 coupling),
  **C2 is the most consistent with this file's established pattern** and is the
  recommended choice unless the cross-backend `DrawRect2D` refactor is independently
  wanted.

**ColorMod / blend semantics** (`Compose` uses three modes, `OutfitConfig.cpp:145-181`,
enum at `Mat.h:122-125`):
- base pass: `kColorModNone` (0), `kBlendSrc`, no diffuse → flat base color fill.
- `mTwoColorDiffuse`/`mTwoColorInterp`: `kColorModModulate` (3) → output =
  texel × material color (the palette tint). This is a plain modulate (mat color in
  the uniform, multiplied by the sampled texel in the fragment shader — BandRnd's
  existing material shader already multiplies `mu.color × diffuse`, so a textured quad
  with the mat color set reproduces it).
- `mTwoColorMask`: `kColorModAlphaUnpackModulate` (2) → uses the texture's alpha as a
  mask to modulate. Needs the shader to treat sampled alpha as the blend weight. This
  is the one mode that may need a dedicated shader path; the base+two-color-diffuse
  modulate cases are straightforward.
- The `Compose` blend is `kBlendSrc` for the base and modulate for the layers — the
  layers must composite *onto* the target's existing pixels, so the texture pass uses
  `LoadOp::Load` after the base (the base clears, the rest load). Practically: open the
  texture pass with `LoadOp::Clear` (the base color fill is the first `DrawRect`), then
  each subsequent `DrawRect` in the same pass naturally composites with its blend mode.

After the target is composited and `FinishDrawTarget` ends the pass, the target's GPU
texture is ALREADY a valid sampled view (`renderTarget` entry in `sTexGpu`). The
existing `MakeMaterialBindGroup` (`Rnd_Wgpu_RB3.cpp:933-963`) → `GetRB3TexView` must
return the render-target view for these textures. **Add to `GetRB3TexView`/upload
path:** if `tex->IsRenderTarget()` and a render-target entry exists, return its view
directly and SKIP the CPU `mBitmap` upload (the rendered pixels live only on the GPU,
the `mBitmap` is empty). Mirror DC3 `GetGpuTexView` (`Tex_Wgpu.cpp:105-118`).

---

## 3. Cheap CPU-tint PARTIAL (alternative / first step) — effort S–M

If full RTT is deferred, recover the highest-visibility subset (two-color *clothing*
tints) with NO render passes:

- In `MakeMaterialBindGroup` / the diffuse-resolve path, detect a material whose
  diffuse is a `kRenderedNoZ` render target (so the simple path didn't fire). For such
  a material, read `mTwoColorDiffuse`'s CPU bitmap (it's a normal sampled texture with
  CPU pixels), CPU-modulate it by the palette color (`mColor2Palette->GetColor(...)`,
  the same color `Compose` would use) into an RGBA8 buffer, upload that as the diffuse,
  and bind it.
- This reuses the existing `UploadRndTexIfNeeded` decode + upload machinery; the only
  new code is the modulate loop + reaching the `MatSwap` palette color from the bind
  site (may need a small accessor or a precomputed map populated during
  `OutfitConfig::Compose`'s simple branch).
- **Recovers:** two-color clothing tints (the most visible outfit-color delta).
  **Does NOT recover:** tattoos, decals, face overlays (eyebrows/goatee/tongue), or the
  band logo — those are real *geometry projection* (`BandPatchMesh::Render` →
  `DrawShowing`) that genuinely must be rasterized into the target. No CPU shortcut
  reproduces them.
- Good as a **stepping stone**: ship the CPU-tint first for a quick visible win, then
  add the full RTT passes for patches/logo later. The two are not mutually exclusive —
  once full RTT lands, the CPU-tint code is removed (the RTT path supersedes it).

---

## 4. Files to edit (ENGINE work — full paths + layer)

All changes are in milo-native-engine (engine runtime, "layer b"). **No matched-fork
(`rb3/src/system/**`) or glue (`rb3/native/src/**`) edits are required** — the
consumer path is already compiled and correct.

| # | File (full path) | Change | Layer |
|---|---|---|---|
| 1 | `/home/free/code/milohax/milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` | **PRIMARY.** Extend `RB3TexEntry` + add `RB3EnsureRenderTarget`; replace the empty `RndTex::MakeDrawTarget`/`FinishDrawTarget` (`:1642-1643`); add `RB3SelectRenderTarget`/`RB3FinishRenderTarget`/`BeginTexturePass`/`EndActivePass`/`BeginMainPass` + `mActiveTargetTex` handling; add `BandRnd::DrawRect`; make `GetRB3TexView` return the render-target view + skip CPU upload for render targets. | engine |
| 2 | `/home/free/code/milohax/milo-native-engine/src/platform/Rnd_Wgpu_RB3.h` | Declare `BandRnd::DrawRect(const Hmx::Rect&, const Hmx::Color&, RndMat*, const Hmx::Color*, const Hmx::Color*) override;`, the new RTT helpers, `RndTex* mActiveTargetTex`, and (if C1) a `DrawRect2D mDrawRect2D;` member. | engine |
| 3 | `/home/free/code/milohax/rb3/src/system/rndobj/Rnd.h` | **Only if needed for vtable match:** the `DrawRect` virtual is already declared (`:79-81`); BandRnd just overrides it. No edit expected unless the override needs the matched-fork decl annotated `HX_NATIVE`. Most likely **no change** — flag as "verify the override binds" rather than "edit." | matched fork (verify only) |
| 4 (C1 only) | `/home/free/code/milohax/milo-native-engine/src/gfx/DrawRect2D.{h,cpp}` | Only if reusing `DrawRect2D`: parameterize the DC3-coupled globals (`TheRnd`/`gWgpuRnd`/`GetGpuTexView`/`CurrentTargetFormat`) so it links in the RB3 build. Skip entirely if using C2 (self-contained blit). | engine |

> **SERIALIZATION NOTE: This task edits `Rnd_Wgpu_RB3.cpp` (#1, the bulk of the
> change). If the crowd-guard refinement task (N5b) or any other concurrent task also
> edits `Rnd_Wgpu_RB3.cpp`, the two MUST be serialized** — they will touch the same
> file (the crowd guard lives in `DrawMesh` at `:1405-1493`; this RTT work adds pass
> helpers + `DrawRect` + touches `DrawMesh`'s pass/target awareness). Recommend RTT and
> crowd-guard run sequentially in the same worktree, RTT second (it is the larger,
> more invasive change), or rebase one onto the other.

Build wiring: `Rnd_Wgpu_RB3.cpp` is already compiled for the rb3 backend. If C1 pulls
in `DrawRect2D.cpp`, confirm it's in the rb3 backend's source list (it is currently a
DC3/`MILO_ENGINE_BUILD_GPU_BACKENDS` file — adding it to rb3 needs a CMake change in
`milo-native-engine/CMakeLists.txt`). C2 avoids this entirely.

---

## 5. Effort, regression risk, verification

### Effort: **L** (honest)
Full RTT is a self-contained but non-trivial renderer feature: per-target GPU texture
lifecycle, mid-frame pass suspend/resume on a single encoder, a real `DrawRect` with
three ColorMod modes, render-target-view binding. The DC3 reference removes most design
risk (the pattern is proven), but the *translation* to BandRnd's single-pass model +
RB3's rndobj shapes + the `kColorModAlphaUnpackModulate` mask shader is the work.
Estimate ~1–2 focused sessions. The CPU-tint partial alone is **S–M** and a good first
landing.

### Regression risk: **LOW**
Every new path activates ONLY when `cam->TargetTex()` is non-null / a material's diffuse
is a `kRenderedNoZ` render target — which **never happens in the main scene today** (the
diagnosis confirms the default path is untouched). The main-pass refactor (extracting
`BeginMainPass`) is the one place that could regress the normal scene; keep the
main-pass attachment/clear/viewport byte-identical to the current `BeginFrame` body
(`:852-875`) and the regression surface is just "did I preserve the main pass exactly."
The pass suspend/resume `LoadOp::Load` on resume is the subtle correctness point
(getting it wrong = the scene drawn before a mid-frame RTT composite gets cleared).

### Verification
1. **Quantify the gap first** (the disambiguation step, see §6): run the V20/V21
   reproducer with `CHAR_DBG=1` and count skinned meshes reporting
   `diffuse=(null) type=0xffffffff` vs `type=0x1 hasTexView=1`
   (instrument at `Rnd_Wgpu_RB3.cpp:1530-1544`). Also note `type=0x22`
   (`kRenderedNoZ`) materials — those are the exact RTT targets this fix paints.
   If that count is ~0 for the test song's loaded characters, the visible delta will
   be small (set expectations accordingly).
2. After implementing: capture a tight band-player closeup where outfit detail is
   visible (the V23 `coop_g_cg` guitar-closeup framing, or a forced stage-facing cam).
   Compare a character with authored tattoos / two-color clothing before/after — the
   white/untinted regions should become colored/patterned.
3. **Opus visual review required** (repo memory `visual-reviews-opus-only`): judge
   whether skin tone, two-color clothing tint, and any tattoo/decal/band-logo now read
   as RB3 outfit detail rather than flat white. Sonnet may do the mechanical capture
   but not the "does this look right" call.
4. **Regression canary:** the main scene (highway/gems/venue/crowd bodies) must be
   pixel-unchanged when no `TargetTex` is active — diff against a V36 baseline frame.
   Confirm `BandRnd: frame drawn — N meshes` count and gem-highway draws under
   `game.cam` (`CAM_DBG`) are unregressed.
5. Beat the menu→gameplay reach flake (V26/V32): `gdb -batch -ex run`, re-run until
   mesh count > 0. Need `MILO_MAX_FRAMES=9000` + `track:guitar` per the gameplay-loop
   memory to reach the in-song window where RTT materials are most visible.

---

## 6. The V24/V26-servo vs RTT disambiguation (do not chase the wrong bug)

The diagnosis explicitly warns (CHAR_OUTFIT_DIAGNOSIS.md §5) that some "wrong-looking"
character features may be the **separate V24/V26 servo/IK skeleton residuals** (the
face-servo eyebrow/goatee/finger slivers), which are a **geometry bug, not a texturing
bug**, and would NOT be fixed by RTT. Before/while implementing, distinguish them so the
RTT work isn't blamed for (or expected to fix) the geometry residual:

- **RTT (texture) symptom:** a material renders flat **white / untinted / unpainted** —
  the surface is *there* and correctly posed, just the wrong color/no pattern. Identified
  by `CHAR_DBG`: `type=0xffffffff` (null diffuse) or `type=0x22` (`kRenderedNoZ`) with
  `hasTexView=0`. **This is what RTT fixes.**
- **Servo (geometry) symptom:** thin teal/green/yellow **triangular slivers / fans**
  shooting away from a character — a vertex flung by a bad bone pose. Identified by the
  V24/V26 **`SHARD_RATIO`** metric (blended-world-AABB ÷ bind-pose-AABB; the guard in
  `DrawMesh:1422-1493`): legit poses sit ~1.0–1.9, shards jump to ~2.0–12×. **RTT does
  NOT touch this** — it's the `CharServo` skeleton-math root cause (the MakeRotQuat
  sqrt(2) lead noted in V26).
- **Decision rule:** if `CHAR_DBG` shows few/no `type=0x22`/`0xffffffff` materials but
  `SHARD_RATIO_DBG` shows residual >2.0 meshes, the visible "wrong" characters are a
  geometry problem → do the servo fix, NOT RTT. If `CHAR_DBG` shows a meaningful count
  of `kRenderedNoZ`/null-diffuse materials, RTT is the right tool. Run both instruments
  on the same gameplay window before committing the L effort.

---

## 7. Bottom line

Full RTT is **effort L, low regression risk**, and the DC3 backend hands you a working
reference for every piece. The cheap CPU-tint partial (**S–M**) is a legitimate
first-step that recovers the most-visible subset (two-color clothing tints) and can ship
ahead of the full feature. All work is in the engine — primarily
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (which **must serialize with any
concurrent task that edits that same file, e.g. the crowd-guard refinement**). Quantify
the `kRenderedNoZ`/null-diffuse material count with `CHAR_DBG` first to confirm the
visible payoff and to avoid conflating it with the separate V24/V26 servo-geometry
residual.
