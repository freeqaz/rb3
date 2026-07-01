# CONFIRMED ROOT CAUSE — "characters without faces" (2026-07-01)

**It is not a pose/skinning/normal-basis bug.** It is a **missing runtime texture
composite**: RB3 character skin (face/torso/legs) + eye + wrinkle textures are
empty render-targets that the real game paints at runtime via a TexBlend
composite. Native never paints them → faces sample blank textures → the shader's
"skin texture absent" desaturated-vertex-color fallback renders flat/blank/dark
faces = "without their faces."

## Proof chain
1. **skinDet=1.0, orthonormal skinRot** (`BONE_PROBE`) — the skinning basis is
   correct; normals are fine. Refutes the prior C8 "rotation-basis → dark normals"
   theory.
2. Band faces **do render** in native (vocalist capture `/tmp/bc-voc/*`), but
   **flat/featureless** with over-bright eyes — a texture problem, not geometry.
3. `char/main/head/{male,female}/gen/head.milo_xbox` contains the face-skin texture
   as **`head_skin_diffuse_output.tex`** — plus `torso_skin_diffuse_output.tex`,
   `legs_skin_diffuse_output.tex`, `head_wrinkle_output.tex`, `norm_output.tex`,
   `eye_diffuse_output.tex` — all **`_output`** textures, driven by `norm.texblend`
   + `wrinkle.texblend` (`RndTexBlender`) and a set of `*.texblendctl`
   (`RndTexBlendController`), with `skin.cfg` + `head.deform`.
4. `RndTex::SetBitmap` (Tex.cpp:113) for a **`kRendered`** texture allocates **NO
   bitmap data** (`mBitmap.Reset()`, no `Create`) — the `_output.tex` are empty RTs
   by design, painted at runtime.
5. Native stubs the paint: `RndTex::MakeDrawTarget`/`FinishDrawTarget` are weak
   no-ops (`native/src/rndobj_synth_link_stubs.s`; `Tex.h:52-53` HX_NATIVE `{}`),
   AND the TexBlend composite never runs. So the outputs stay black.
6. Shader (`milo-native-engine/src/gfx/standard_wgsl.inc:32,41-46,712-750`) already
   documents this: "RB3 skin/cloth textures **absent** from the extracted asset
   set" → keep desaturated vertex tint (`kSkinnedVtxChroma=0.25`) instead of white
   to avoid "the small_club **pink wash** blowout." The pink/magenta wash I saw is
   the same absent-output symptom.

## Fix surface (native/web engine)
The composite must run so the skin `_output` textures get real pixels. Parts:
- **(a) RndTex render-to-texture** on the WebGPU backend: implement
  MakeDrawTarget/FinishDrawTarget to bind the tex's GPU resource as a render pass
  target. Backend already does RTT (bloom, screenmask) so plumbing exists.
- **(b) The skin composite draw**: render the SOURCE skin textures (skin base +
  tone/features + wrinkles) into the `_output` RT with the right blend. Need to map
  the RndTexBlender/RndTexBlendController graph → source textures + blend ops, and
  find the TRIGGER (BandHeadShaper / CharDriverImage / skin.cfg apply).
- **(c) Trigger timing**: composite once per (re)load of a character head, and on
  expression/wrinkle change (distance-driven controllers) — or, minimally, once.

## Cheaper alternatives (evaluate)
- **Pre-bake / extract**: for the deterministic default band members, dump the
  composited skin textures from the REAL game (Dolphin/Xenia — the ground-truth
  agents) or composite offline, and load as static textures (skip runtime blend).
  Matches the user's "use Xbox/Dolphin assets for the ground-truth fix." Less
  general (custom chars won't composite) but immediate correct faces for presets.
- **Bind skin base source directly**: if a single dominant skin-base source texture
  exists, bind it (skip blend) — approximate but far better than blank.

## Why prior campaigns missed it
render-polish-2026-06-11 chased geometry/normals (pose basis) and got "band stands
dressed" — but the faces were always a missing-texture problem, invisible to a
geometry lens. `skinDet=1.0` is the tell.

## Status of measurements
- `/tmp/bc-voc/voc_coop_front_n0*_0.png` — native vocalist, flat face.
- `/tmp/voc-face-n00.png` — face crop.
- head milo strings confirm the `_output` texblend graph.
