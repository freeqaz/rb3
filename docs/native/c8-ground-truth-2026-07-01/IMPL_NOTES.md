# Implementation-readiness notes — skin composite (2026-07-01)

The RTT machinery to paint the skin `_output` textures **already exists and works**
on the WebGPU backend. The gap is narrow: the skin texblend composite is never
drawn. Reuse the outfit-tint path.

## What already works (the template)
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`:
- `BandRnd::BeginDrawTarget(RndTex*)` (:2012) — lazily creates a per-RndTex
  RGBA8 `RenderAttachment|TextureBinding` texture in `sTexGpu` (`isRenderTarget`),
  suspends the main pass, opens a transparent-clear pass into the RT view. The
  same side-table the diffuse-bind path reads (`GetRB3TexView`/`UploadRndTexIfNeeded`
  :651-658) — so once painted, materials sample it.
- `BandRnd::EndDrawTarget()` (:2075) — resumes the main pass with LoadOp::Load.
- `RndTex::FinishDrawTarget()` (:6183) — END hook; `MakeDrawTarget()` (:6182) is an
  empty strong def because the shared `rndobj/Cam.cpp` never calls BEGIN — the RT
  begin is driven per-draw by a begin-hook (`if (tt && tt != mRtActiveTex)
  BeginDrawTarget(tt)` at :3359-3368 in the DrawRect path, mirror in DrawMesh).
- `Rnd::DrawPreClear()` (:1666, default-on, opt-out `RB3_NO_PRECLEAR=1`) — the
  render-textures pre-pass. Iterates registered pre-clear drawables: `TexRenderer`,
  `TexMovie`-to-tex, and **`OutfitConfig`**, whose `DrawPreClear → MatSwap::Compose`
  (`rb3/src/system/bandobj/OutfitConfig.cpp:103`) is the ONLY working caller of the
  DrawRect RTT-outfit-tint branch. **Character clothing tints composite correctly.**

## The gap
`RndTexBlender::DrawShowing()` is **empty** (`rb3/src/system/rndobj/TexBlender.cpp:64`)
— the Wii composite draw was GX-specific and never ported. Its members:
`mBaseMap`, `mNearMap`, `mFarMap` (source maps), `mControllerList`
(`RndTexBlendController` — distance-driven wrinkle/expression weights),
`mOutputTextures` (the `_output` RTs), `mControllerInfluence`. Exposes outputs via
`OnGetRenderTextures` → `GetRenderTexturesNoZ(Dir())`.

## OPEN (composite agent → t4-composite-mechanism.md)
- **Does `head_skin_diffuse_output.tex` (the DIFFUSE) come from a RndTexBlender, or
  from a MatSwap/skin-tone compose like the outfit tint?** If MatSwap: the working
  path exists — the fix is making it register/trigger for skin (small). If
  RndTexBlender: implement its composite draw (DrawRect the maps into the output,
  weighted by controllers) — moderate, but reuses BeginDrawTarget/DrawRect/EndDrawTarget.
- **Are the SOURCE skin textures in `orig-assets/extracted/`?** Determines
  runtime-composite vs pre-bake feasibility.

## Likely fix shape (pending t4)
Implement a native `RndTexBlender` pre-clear composite that, for each output RT:
`BeginDrawTarget(output)` → DrawRect(base map, full) → DrawRect(near/far/wrinkle
maps, controller-weighted, additive/lerp) → `EndDrawTarget()`. Register the head's
texblends as pre-clear drawables (or drive from the char head setup). Gate default-on
RB3-only, opt-out env, DC3-safe (DC3 compiles its own backend). Also covers eyes
(`eye_diffuse_output.tex` empty → the "googly bright eyes"): same empty-RT cause.
