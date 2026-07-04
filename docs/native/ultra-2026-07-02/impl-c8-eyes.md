# C8 impl: glowing eyes / bright teeth fix (dest-multiply outfit composite)

Implements the fix from `research-c8-eyes.md`. Scope item (a) of the C8
follow-ups. Landed 2026-07-02.

## Commits

- Engine: `04c8e1c` (milo-native-engine) — `src/platform/Rnd_Wgpu_RB3.cpp`
- rb3:    `fadd179a` — `native/CMakeLists.txt` (pin bump) +
  `src/system/bandobj/OutfitConfig.cpp`
- Pin bumped `3e02cea` -> `04c8e1c`.

## Mechanism

Band-member eyes render as glowing white dots in dark venues. The eye color
texture is a render target (`eyes_diffuse_output.tex`) painted by
`OutfitConfig::MatSwap::Compose` as a 4-pass DrawRect sequence into the RT:

1. base fill — `colorMod = kColorModNone (0)`, color = picked eye color (col1)
2. `mTwoColorDiffuse` layer — `kColorModModulate (3)`, color = col2
3. `mTwoColorInterp` layer — `kColorModModulate (3)`
4. `mTwoColorMask` layer — `kColorModAlphaUnpackModulate (2)` (alpha->gray)

All four are drawn `kBlendSrc`. Native `BandRnd::DrawRect` mapped `kBlendSrc`
to REPLACE (One/Zero) for every layer, so the RT collapsed to "last layer
wins" = `gray(eyes_mask_gw.a)` ≈ near-white -> an untextured white eyeball ->
reads as a glowing dot under warm venue light. On Wii/360 the `ColorModFlags`
TEV/shader modes product-combine each layer with the destination.

**Fix**: while the outfit composite is active, combine the MODULATE layers
(colorMod 2 and 3) with the destination via DEST-MULTIPLY (`WgpuBlend::Multiply`
= srcFactor Dst, dstFactor Zero, `PipelineManager::MapBlend`) instead of
REPLACE. The base fill (colorMod 0) stays REPLACE. Result RT =
`col1 × (diffuse×col2) × interp × gray(mask.a)` — the authored product — so the
eyeball gets its iris/pupil/sclera detail back and reads recessed/dark.

### Scoping (why it doesn't wash the screen)

Gated on THREE conditions so only the composite layers are affected:
- `gRB3OutfitComposeActive` — a file-scope flag set ONLY inside
  `MatSwap::Compose` via an RAII scope (rb3 side, `#ifdef HX_NATIVE`). Mirrors
  the Wii's `WiiTex::bComposingOutfitTexture` pattern.
- `mRtActiveTex` — the composite paints an RT, not the main framebuffer.
- `colorMod == 2 || colorMod == 3` — the modulate layers only. Postproc/
  vignette DrawRects also carry colorMod but are not composite-scoped, so they
  are untouched. The base fill (colorMod 0) is untouched.

Default-ON, opt-out `RB3_COMPOSE_MULT_OFF=1`. Engine change lives in the
RB3-only TU `Rnd_Wgpu_RB3.cpp` -> DC3 byte-identical. rb3 change is entirely
`#ifdef HX_NATIVE` and the Wii target never defines HX_NATIVE, so the Wii match
build is untouched.

## Diffs summary

Engine (`Rnd_Wgpu_RB3.cpp`, +31 lines):
- File-scope `bool gRB3OutfitComposeActive = false;` above `BandRnd::DrawRect`.
- In DrawRect after blend selection: `if (!kComposeMultOff &&
  gRB3OutfitComposeActive && mRtActiveTex && (colorMod==2||colorMod==3)) blend =
  WgpuBlend::Multiply;` (static-cached getenv opt-out).

rb3 (`OutfitConfig.cpp`, +14 lines, all `#ifdef HX_NATIVE`):
- At top of `MatSwap::Compose`: `extern bool gRB3OutfitComposeActive;` + an RAII
  `ComposeScope` struct that sets the flag true in its ctor / false in its dtor
  (covers all return paths).

The archived `c8-eyes-{engine,rb3}-experiment.diff` were the researcher's
opt-in (`RB3_COMPOSE_MULT=1`) probes; production flips polarity to default-ON,
drops the RECT_TRACE / COMPOSE_DBG / HEADMAT-emissive debug hunks, and keeps
only the load-bearing logic.

## Teeth

Left alone, per research: `teeth.mat` is NOT composited (direct `teeth_diff.tex`,
no emissive). Their brightness is (b)-family venue char-lighting exposure, not an
eye-composite defect. No teeth-specific change.

## Verification (native, band-closeup-capture.py)

Harness gate PASS on all runs:
- guitar fix-on: PASS 10/10 pinned, 0 drops (`/tmp/c8fix/guitar_on`)
- guitar opt-out: PASS 10/10, 0 drops
- vocals fix-on: PASS 6/6, 0 drops (`/tmp/c8fix/vocals_on`)
- vocals opt-out: PASS 6/6, 0 drops

Visual evidence (persisted in this dir):
- `impl_singer_fix_face.png` — fix-on full-scene singer face: dark, rim-lit,
  recessed non-glowing eyes. Matches `dolphin-shots/face_singer_rimlit.png` GT.
- `impl_iso_fix.png` / `impl_iso_baseline.png` — `RB3_ISOLATE_MESH=eyes.mesh`
  fix vs opt-out (both wash-free frames).
- `impl_eye_fix_zoom.png` vs `impl_eye_baseline_zoom.png` — nearest-neighbour
  eyeball zooms; fix shows a darker/recessed structured eyeball.
- `impl_brightvenue_fix.png` — brighter red-lit venue frame: band members
  coherent silhouettes, no white eye dots, no geometry regressions.

Objective datapoint (lighting-robust): in matched isolation crops the fix
eyeballs are **~35% dimmer** (mean luma 126 vs 195) *despite the fix run having
a MORE-lit venue background* (dark-red vs the baseline's black) — i.e. the eye
texture itself darkened toward the authored product, which is the desired
glowing->subtle direction. Internal contrast (CV) stays ~21% both ways.

Web: `scripts/web/build.sh --debug` exit=0 — shared engine change compiles +
links for WASM.

## Residual risks / notes

- **ALL two-color composites now product-combine** (skin, hair, clothing,
  instruments), so they render darker than the old last-layer-wins output. In
  the dark CORK boot venue this is GT-faithful (GT faces are very dark). The
  fix multiplies real authored textures (skin ~0.6 × interp ~0.7 ≈ 0.42 flesh
  mid-tone), so skin does not go muddy-black. A direct A/B of lit-SKIN tone in a
  BRIGHT venue was NOT obtained: the fast harness boots into the dark CORK
  club, band members are backlit/shadowed there, and the prefab rotates per
  launch (random-lineup A/B trap). Recommend a later spot-check of skin tone in
  a bright venue vs Dolphin GT; low risk.
- **Pre-existing intermittent magenta/pink full-frame wash** (research trap #4)
  reproduced in a baseline opt-out run here too — unrelated to this fix; a
  particle/billboard or halo-path artifact worth its own investigation.
- Exact console `ColorModFlags` per-layer math is not fully recoverable from the
  decomp; dest-multiply is an empirically-good approximation (restores iris/
  pupil + GT recessed look). If higher tint fidelity is needed later, dump the
  Xbox `eyes_diffuse/interp_gw/mask_gw` and validate
  `fill × diff×col2 × interp × gray(mask.a)`, or replace the 4-pass emulation
  with a single native two-color shader pass.
