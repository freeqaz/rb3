# Character outfit / skin-texture diagnosis (read-only)

**Authored:** 2026-05-28 (Opus read-only diagnosis subagent). **Scope:** does the
in-song band + crowd + extras render with CORRECT outfits + skin/face textures, or
default/untextured/wrong? No build, no run, no source edits — code + existing
screenshots + existing session docs only.

---

## 1. VERDICT — a PARTIAL, REAL, but NARROW gap (do not over-prioritize)

**The characters are NOT untextured grey mannequins.** Their bodies, clothing, heads,
hair and skin DO render with real textures via the simple per-material texture path,
and they look recognizably like RB3 characters. **But there is a genuine, visible
fidelity gap for the subset of character materials whose skin/outfit is built by RB3's
render-to-texture (RTT) "patch" compositor** — tattoos, decals, two-color
diffuse/interp/mask tinting, face overlays (eyebrows / goatee / tongue) and the band
logo. That RTT compositing is a **no-op at every layer of the native engine**, so those
specific materials render white / untinted / unpainted rather than wrong-colored-grey.

This matches what every prior session doc (V19→V36) flagged in passing and what the N4
agent meant by "the same deferred class as the documented `BandPatchMesh` no-op." It is
**real**, but it is a **secondary cosmetic fidelity item**, not a "characters are
broken" problem.

### Screenshot evidence (frames I viewed first-hand)

- **`v20-characters/01_f0505.png`, `04_f0535.png`** and **`v21-band-players/01_f0570.png`,
  `02_f0650.png`** — the small_club venue with a dense crowd + band on stage. Characters
  wear **distinct colored clothing** (blue/purple/lighter tops, not flat grey); they are
  normal-proportioned bodies, not mannequins. Lighting is dim club pink/purple so fine
  texture detail is hard to judge, but the figures are clearly clothed and skinned, not
  default. The crowd silhouettes change pose frame-to-frame (animating).
- **`v36-camera-cuts/on_run1/05_f4200.png`** — gameplay over the venue: a richly textured
  club (SMOOTH poster, dartboard, wooden chairs), and at the right edge a character with
  **magenta/purple hair and a green textured arm** — hair + skin texturing visibly works.
- **`v36-camera-cuts/final/03_f2400.png`** — the guitar closeup: a solid **teal guitar**
  with the gem-highway HUD. Plausibly a correct instrument color; nothing reads as a
  texture failure here.

So: clothing/skin/hair = textured and plausible. The white/unpainted RTT materials
(eyebrows/tongue/tattoo decals/two-color tints/band-logo) are small, dark, and easy to
miss in the dim venue lighting — which is why they have never been the headline gap.

This verdict is consistent with the CHAR_DBG instrumentation already in the tree
(`Rnd_Wgpu_RB3.cpp:1530-1544`), which the V20/V21 docs report showed character base
materials as `type=0x1 hasTexView=1` (textured) while a few accessory mats showed
`diffuse=(null) type=0xffffffff` (the RTT-target subset that never paints).

---

## 2. ROOT CAUSE — RTT render-to-texture is a no-op at every layer

RB3 builds a character's final skin/outfit texture by **rendering INTO a texture** (a
`kRenderedNoZ` render-target `RndTex`) and compositing layers onto it: a base color, the
two-color diffuse/interp/mask tint passes, then the patch meshes (tattoos / decals /
face features) projected onto the body and drawn into that same target. Once composited,
that render-target texture is the material's diffuse and the body samples it normally.

The compositor is `OutfitConfig::MatSwap::Compose`
(`rb3/src/system/bandobj/OutfitConfig.cpp:103-201`), driven by
`OutfitConfig::RenderToTextures`/`Recompose` (the `Compose` calls at lines 969 / 975).
The exact branch:

```
OutfitConfig.cpp:108  RndTex *diffTex = mMat->GetDiffuseTex();
OutfitConfig.cpp:109  if (!diffTex || (diffTex->GetType() & kRenderedNoZ) != kRenderedNoZ) {
OutfitConfig.cpp:113      int idx = colors[mColor1Option] % mTextures.size();
OutfitConfig.cpp:114      mMat->SetDiffuseTex(mTextures[idx]);   // <-- SIMPLE PATH: works natively
      ...
OutfitConfig.cpp:119  } else {                                    // <-- RTT PATH: dead natively
OutfitConfig.cpp:133      sCam->SetTargetTex(diffTex);            // redirect render into diffTex
OutfitConfig.cpp:143      sCam->Select();
OutfitConfig.cpp:160      TheRnd->DrawRect(rect, baseColor, sMat, ...);   // base + two-color layers
OutfitConfig.cpp:169/175/181  TheRnd->DrawRect(...);              // mTwoColorDiffuse/Interp/Mask
OutfitConfig.cpp:185      for (...) patches[i].Render(diffTex, sMat);     // tattoos/decals/face
OutfitConfig.cpp:191      sCam->SetTargetTex(nullptr);
OutfitConfig.cpp:195      prevCam->Select();
  }
```

The simple `mTextures[idx]` branch (108-118) is exactly the path the docs call "the base
materials texture correctly (`type=0x1 hasTexView=1`)" — a plain texture into the diffuse
slot that the engine uploads via `UploadRndTexIfNeeded`. **That works.** The `else`
branch (the RTT composite) is dead natively for THREE independent reasons, every one of
which alone is sufficient to break it:

1. **The engine ignores `cam->TargetTex()`.** The RTT mechanism redirects rendering into
   a texture by `RndCam::SetTargetTex(diffTex)` + `cam->Select()`
   (`OutfitConfig.cpp:133,143`; also the general `RndTexRenderer::DrawToTexture` at
   `rndobj/TexRenderer.cpp:251-252`). The engine `BandRnd` **never reads
   `RndCam::sCurrent->TargetTex()`** — `BandRnd::BeginFrame`/`WriteSceneUniforms`
   (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:838-876, 739-836`) always render to
   `mFrameView` (the main swapchain / headless readback frame). There is no off-screen
   render pass, no per-target framebuffer, no `TargetTex`→GPU-texture mapping anywhere in
   the file. So composited pixels never reach `diffTex`'s GPU surface.

2. **`RndTex::MakeDrawTarget` / `FinishDrawTarget` are explicit no-ops.**
   `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:1642-1643`:
   ```cpp
   void RndTex::MakeDrawTarget() {}
   void RndTex::FinishDrawTarget() {}
   ```
   with the comment "not needed for the mesh render path (no render-to-texture)." These
   are the entry points (`RndCam::Select`/`SetTargetTex` call `FinishDrawTarget`,
   `rndobj/Cam.cpp:54-55,66-67`) that would allocate/resolve the render-target. They do
   nothing, so `diffTex` is never backed by a GPU render target.

3. **`TheRnd->DrawRect(...)` is a no-op natively.** The two-color tint layers
   (`OutfitConfig.cpp:160,169,175,181`) draw via `TheRnd->DrawRect`. In the RB3 fork
   `Rnd::DrawRect` is an inline empty `{ }` body (`rb3/src/system/rndobj/Rnd.h:80`), and
   `BandRnd` does NOT override it (verified: the only `::DrawRect` strong def in the tree
   is `WgpuRnd::DrawRect` in the DC3 backend `Rnd_Wgpu.cpp:1851`, which is OFF for RB3 via
   `MILO_ENGINE_BUILD_GPU_BACKENDS=OFF`). So even if (1)+(2) were fixed, the base/tint
   rect passes paint nothing.

Net effect: a material whose authored diffuse is a `kRenderedNoZ` render-target keeps an
unpainted/empty render-target bitmap. When the engine binds it
(`BandRnd::MakeMaterialBindGroup`, `Rnd_Wgpu_RB3.cpp:933-963`), `UploadRndTexIfNeeded`
reads an empty/garbage `mBitmap` and returns an empty view → falls back to `mWhiteView`
(white). That is the tattoo/decal/two-color/face-overlay/band-logo subset rendering as
white/untinted.

### What is NOT the cause (ruled out)

- **`BandPatchMesh` is compiled and works.** It was un-excluded + brought up clang-LP64
  in V20 (`native/CMakeLists.txt:~334`; `BandPatchMesh.cpp` HX_NATIVE `stlpmtx_std`
  gates). Its `Render`/`ProjectPatches`/`Construct` logic is intact. The N4 phrase "the
  same deferred class as the documented `BandPatchMesh` no-op" is slightly stale: the TU
  is no longer stubbed — but `BandPatchMesh::Render` (`BandPatchMesh.cpp:1170-1203`) ends
  in `patch->DrawShowing()`, which only produces a composited result if the surrounding
  RTT target (reasons 1-3) is live. So the *patch math* runs; the *patch pixels* go
  nowhere. The no-op is in the RTT plumbing, not in BandPatchMesh itself.
- **Outfit→material binding DOES run.** V23 wired `LoadMainCharacters` /
  `SyncTransProxies` so `OutfitConfig` composes (the simple path assigns `mTextures[idx]`
  to bodies/heads/hair). The merge completes (V23d `ReplaceRefs` LP64 fix). So the bodies
  are correctly textured — the binding is not the gap.
- **The skinned-mesh diffuse bind path works** (V14a/V21): `MakeMaterialBindGroup` resolves
  `mat->GetDiffuseTex()` and uploads it. For the simple-path materials that is a real
  texture; only the RTT-target materials have nothing to upload.

---

## 3. PROPOSED FIX + layer + effort

**Right layer: engine (layer b) — implement real render-to-texture in `BandRnd`.** This
is squarely an engine-renderer capability gap, not a matched-fork or glue problem. The
matched-fork RTT *callers* (OutfitConfig, RndTexRenderer, PatchRenderer, BandPatchMesh)
are already correct and compiled; they just need the engine to honor `cam->TargetTex()`.

Concrete work in `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`:

1. **Give each `kRenderedNoZ` `RndTex` a GPU render-target.** In
   `RndTex::MakeDrawTarget` (currently `:1642`), create/look-up a `wgpu::Texture` with
   `RenderAttachment | TextureBinding` usage at the tex's `Width()/Height()`, sized to the
   `RB3TexEntry` side-table (`sTexGpu`, `:324`). `FinishDrawTarget` resolves/keeps it.
2. **Route draws to the active target.** Track `RndCam::sCurrent->TargetTex()` at
   `BeginFrame`/`DrawMesh` time. When non-null, begin (or switch to) a render pass whose
   color attachment is that target's view instead of `mFrameView`, with its own
   viewport/projection (the `Compose` cam is an ortho-ish unit quad cam,
   `OutfitConfig.cpp:136-142`). On `SetTargetTex(nullptr)` / `prevCam->Select()`, end that
   pass and resume the main pass. The engine already re-writes scene uniforms per cam
   change (V2/V13, `DrawMesh:1081-1095`) — extend that to also switch the color target.
3. **Implement `BandRnd::DrawRect`** (override the empty `Rnd::DrawRect`): a textured/tinted
   full-target quad honoring `RndMat` color/blend/colormod (kColorModModulate /
   kColorModAlphaUnpackModulate, per `Compose:162/172/178`). This is what paints the base
   + two-color tint layers into the target.
4. After the target is painted, the existing `UploadRndTexIfNeeded` /
   `MakeMaterialBindGroup` path binds it as a normal diffuse — no further change needed,
   except possibly skipping the CPU `mBitmap` upload for textures that are GPU
   render-targets (read directly from the rendered GPU texture instead).

**Effort: M-to-L (lean L).** Implementing a correct off-screen render-target pass +
target-switching + a real `DrawRect` in the WebGPU backend is a meaty, self-contained
renderer feature (estimate ~1-2 focused sessions). It carries low regression risk to the
main scene (it only activates when `cam->TargetTex()` is non-null, which today never
happens, so the default path is untouched). Order of operations and the
`Compose`-cam projection are the fiddly bits. There is **no smaller matched-fork or glue
shortcut** that achieves real composited skins — the pixels genuinely have to be rendered
into a texture.

(A cheap *partial* win, if full RTT is deferred: for the `mTwoColor`/two-color-tint
materials, approximate the composite on the CPU when binding — modulate `mTwoColorDiffuse`
by the palette color into an RGBA8 buffer and upload that as the diffuse. This skips the
patch/tattoo layers but recovers the two-color clothing tints. Effort S-M. It does NOT
recover tattoos/decals/face-overlays/band-logo, which need real geometry projection.)

---

## 4. How a follow-up agent should VERIFY

1. **Quantify the gap first (no build needed to plan):** run the V21/V20 reproducer with
   `CHAR_DBG=1` and count skinned meshes reporting `diffuse=(null) type=0xffffffff` vs
   `type=0x1 hasTexView=1`. The `0xffffffff`/null set is the RTT-target subset this fix
   targets. (The CHAR_DBG instrument is already at `Rnd_Wgpu_RB3.cpp:1530-1544`.)
2. After implementing engine RTT: capture a tight band-player closeup where outfit detail
   is visible (the V23 `coop_g_cg` guitar closeup framing, or force a stage-facing cam).
   Compare a character with authored tattoos / two-color clothing before/after — the
   white/untinted regions should become colored/patterned.
3. **Opus visual review required** (per repo memory `visual-reviews-opus-only`): judge
   whether skin tone, two-color clothing tint, and any tattoo/decal now read as RB3 outfit
   detail rather than flat white. Sonnet may do the mechanical capture but not the
   "does this look right" call.
4. **Regression canary:** the main scene (highway/gems/venue/crowd bodies) must be
   pixel-unchanged when no `TargetTex` is active — diff against a V36 baseline frame.
   Confirm `BandRnd: frame drawn — N meshes` count and gem-highway draw under `game.cam`
   (`CAM_DBG`) are unregressed.
5. Beat the menu→gameplay reach flake noted across V26/V32 (`gdb -batch -ex run`, re-run
   until mesh count > 0) — the in-song crowd-cinematic view is where RTT materials are
   most visible.

---

## 5. Confidence + most-likely alternative

**Confidence: HIGH (~90%)** that the RTT-composite no-op is the root cause of the
white/unpainted-outfit-subset gap, and HIGH that the *bodies/skin/hair* themselves are
correctly textured (multiple first-hand screenshots + the existing CHAR_DBG findings
agree). The three independent dead layers (TargetTex ignored, MakeDrawTarget no-op,
DrawRect no-op) are confirmed by direct code reading, not inference.

**Most-likely alternative / caveat:** the *severity* could be lower than even "secondary"
if the specific characters loaded for the test song (`20thcenturyboy`, default
`small_club` band) happen to use mostly simple-path materials with few tattoos / minimal
two-color tinting — in which case the visible delta after a full RTT implementation might
be modest (subtle tint + a band logo + eyebrows), not a dramatic transformation. A second
possibility is that a few "wrong-looking" features attributed to RTT are actually the
separate documented servo/IK skeleton residuals (V24/V26 face-servo eyebrows/goatee
slivers) rather than texture problems — those are a *geometry* bug, not a texturing bug,
and would not be fixed by RTT. Disambiguate with the CHAR_DBG `type=0xffffffff` count
(texture) vs SHARD_RATIO (geometry) before committing effort.

---

## 6. Bottom line for prioritization

This is a **real but narrow cosmetic fidelity gap**, not a "characters render as default
mannequins" emergency. Bodies, clothing, skin and hair are textured and look like RB3.
The missing piece is RTT-composited skin detail (tattoos / two-color tints / face overlays
/ band logo), which is dead because the engine has no render-to-texture support. It is
worth doing for outfit fidelity, but it is an **M-L engine feature** that should be
sequenced *after* the higher-visibility items already tracked (camera void-cut polish,
the V12/V31 apply-handler convergence) unless outfit fidelity is explicitly the goal.
Every prior session doc correctly classified it the same way ("low-priority / secondary
fidelity item, not why the band is invisible") — this diagnosis confirms that
classification with the full three-layer root cause.

### Key file:line references

- Engine RTT no-ops: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:1642-1643`
  (`MakeDrawTarget`/`FinishDrawTarget`); no `TargetTex` handling in
  `BeginFrame`/`WriteSceneUniforms` (`:838-876`, `:739-836`); diffuse bind
  `MakeMaterialBindGroup` (`:933-963`); CHAR_DBG instrument (`:1530-1544`).
- Engine `DrawRect` missing: `Rnd::DrawRect` empty inline body
  `rb3/src/system/rndobj/Rnd.h:80`; no `BandRnd::DrawRect` override.
- Compositor: `rb3/src/system/bandobj/OutfitConfig.cpp:103-201` (`MatSwap::Compose`),
  simple path `:108-118`, RTT path `:119-200`, drivers `:969/975`.
- RTT cam plumbing: `rb3/src/system/rndobj/TexRenderer.cpp:251-279` (`DrawToTexture`);
  `rb3/src/system/rndobj/Cam.cpp:51-71` (`Select`/`SetTargetTex` → `FinishDrawTarget`).
- Patch compositor: `rb3/src/system/bandobj/BandPatchMesh.cpp:1170-1203` (`Render` →
  `patch->DrawShowing()`), un-excluded `native/CMakeLists.txt:~334`.
- `PatchRenderer` (RTT subclass): `rb3/src/system/bandobj/PatchRenderer.cpp` (a
  `RndTexRenderer`; `DrawBefore`/`DrawAfter` use `TheRnd->DrawRect` + `mOutputTexture`).
