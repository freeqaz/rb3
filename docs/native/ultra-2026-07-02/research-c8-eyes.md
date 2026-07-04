# C8 research: glowing eyes / over-bright teeth in dark venues (2026-07-02)

Researcher handoff. Scope item (a) of the C8 follow-ups (RESOLUTION.md). All
evidence gathered on rb3-native via `band-closeup-capture.py` (pinned shots,
default boot song → "CORK" blues-club venue + one rooftop launch). No fixes
landed; all probes reverted (working trees clean; build re-verified green).

## TL;DR — root cause

**The eyes glow because the eye-color composite RT contains no eye art.**
`OutfitConfig::MatSwap::Compose` paints each two-color layer with a DrawRect
whose material blend is `kBlendSrc`, and the native `BandRnd::DrawRect`
(engine `Rnd_Wgpu_RB3.cpp`) maps that to REPLACE (One/Zero) for **every**
layer. The layered composite therefore collapses to "last layer wins": for
eyes the last layer is `eyes_mask_gw.tex` via `kColorModAlphaUnpackModulate`
(alpha unpacked to gray ≈ white) → `eyes_diffuse_output.tex` ends up a
near-white flat texture → the eyeball renders as an untextured white ball →
any warm venue light on it reads as a glowing dot through the eyelid gap.

The Wii/360 make the same 4-pass sequence work because their `ColorModFlags`
shader/TEV modes combine each layer with what is already in the target;
native's DrawRect only approximates colorMod as a source swizzle and keeps
replace blending, discarding all earlier layers.

**Teeth are a different, smaller story**: `teeth.mat` is NOT composited
(direct `teeth_diff.tex`, near-white albedo, color (1,1,0.94), prelit=0,
useEnviron=1, no emissive). Their brightness is purely the (b)-family venue
char lighting being hotter than the Wii's — same family as over-bright faces,
not an eye-composite defect. Fixing (b)'s exposure will dim teeth; nothing
teeth-specific to fix.

## Evidence chain (all reproducible)

1. **Census** (`RB3_HEADMAT_DBG=1`, engine 77eb428): band-member (dir
   `outfit`) `eyes.mesh` draws with `eyes.mat`/`eyesmale.mat`, diffuse =
   `eyes_diffuse_output.tex` / `eye_diffuse_output.tex`, `isRT=1 hasTex=1`,
   `blend=1 prelit=0 useEnviron=1 alphaCut=0`, color white. A one-shot probe
   extension confirmed **no emissive map** (`emMap=<null>` → the engine
   forces `mu.emissiveMultiplier=0`), `nextPass='eyelense.mat'`.
2. **Self-illumination refuted**: RB3 DrawMesh path never sets
   `specularPower` (zero-init `mu{}`); emissive needs `mEmissiveMap` (null);
   bloom halo needs an emissive map (`IsHaloSourceMat` line ~2230) — eyes and
   teeth are excluded; `mNextPass` (`eyelense.mat`, the Wii lens-shine pass)
   is never drawn by the native engine (no reference besides my probe).
3. **Isolation** (`RB3_ISOLATE_MESH=eyes.mesh`): eyeballs render as uniform
   bright balls with NO iris/pupil/sclera detail (`iso_eye_n03.png` — saved
   in this dir). Dot color (239,235,126) ≈ white texel × the char env point
   light (`char_rooftop.env` light `foregroundred_char01` color
   (2.0,1.91,0.89) × 0.70 point exposure × GX falloff) — i.e. lit white
   diffuse, no additive term.
4. **Compose runs with all inputs present** (temporary `RB3_COMPOSE_DBG`
   probe in `MatSwap::Compose`, diff saved here): for eyes —
   `rtBranch=1 twoColor=1 tcDiff=eyes_diffuse.tex tcInterp=eyes_interp_gw.tex
   tcMask=eyes_mask_gw.tex pal1=eyes.pal pal2=eyes_secondary.pal`, base fill
   color = the picked eye color (e.g. (0.05,0.39,0.58)). So the composite is
   fully wired — the failure is in how the layers COMBINE, not missing data.
   (The earlier `eyes.cube` NOTIFY belongs to the never-drawn `eyelense.mat`
   and is a red herring.)
5. **Native combine is replace**: `BandRnd::DrawRect` takes
   `blend = mat->GetBlend()` (= `kBlendSrc` for all Compose layers; set once
   in `Compose`), `PipelineManager::MapBlend(Src)` = One/Zero. The quad
   shader (`kRB3QuadShaderSource`, `fs_rect`) only implements colorMod==2 as
   an alpha→gray swizzle. So final RT = last layer = gray(mask.a) ≈ white.
6. **Decisive experiment** (diffs in this dir, reverted): forcing
   `WgpuBlend::Multiply` (src×dst) for Compose-scoped DrawRects with
   colorMod∈{2,3} (scope flag set by `MatSwap::Compose`, mirroring the Wii's
   `WiiTex::bComposingOutfitTexture` pattern) **restores a real eye**: iris
   ring + dark pupil visible on the isolated eyeball (`mult_eye_zoom.png`),
   and in the full scene the guitarist's eyes become recessed/dark like the
   Dolphin GT (`m4_guitar.png` vs `guitar_eye_zoom.png` baseline glowing
   dot / `dolphin-shots/face_singer_rimlit.png`).
7. Same defect hits every two-color composite: skin
   (`male_head_diff` → `interp_gw` last), hair, clothing, instruments
   (`d2010_woodmaple`, `beta57_mic` — see RECT trace in the saved diff run).
   The "flat/untextured face" in follow-up (b) is partly THIS (final skin RT
   ≈ interp_gw layer instead of tone×detail product).

## What the correct combine actually is (open detail)

The console semantics of `ColorModFlags` for these passes are not fully
recoverable from the decomp: Wii `WiiMat::SetStageState` (m2c of
`SetStageState__6WiiMatFRiRiRibi`) shows colorMod 1/2 only swap TEV alpha
inputs (no blend change), and xenon folds colorMod into a compiled-shader
permutation (`Shader.cpp:668`) whose HLSL we don't have; neither side ever
overrides the D3D/GX framebuffer blend away from replace. So the exact
per-layer math is still an approximation. Empirically, dest-multiply for
modes 2/3 already produces an iris/pupil and GT-like recessed eyes; the
residual risk is tint accuracy (e.g. whether the sclera should escape the
col1 fill tint via the mask). If higher fidelity is needed later, dump
`eyes_diffuse/interp_gw/mask_gw` from the Xbox milos and check whether
`fill × diff×col2 × interp × gray(mask.a)` matches the retail texture, or
replace the 4-pass emulation with a single native two-color shader pass
(`lerp(col1,col2,interp)` tint, mask-gated) — all inputs are available on
the `MatSwap` at Compose time (probe proves they resolve).

## Recommended fix plan (house style)

Engine side (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, RB3-only TU
→ DC3-safe by construction):
1. Define `bool gRB3OutfitComposeActive` (file scope, default false).
2. In `BandRnd::DrawRect`, after the existing blend selection: when
   `gRB3OutfitComposeActive && mRtActiveTex && (colorMod==2 || colorMod==3)`
   use `WgpuBlend::Multiply`. Default-ON, opt-out `RB3_COMPOSE_MULT_OFF=1`.
   (The saved diff used opt-in `RB3_COMPOSE_MULT=1`; flip the polarity.)

rb3 side (`src/system/bandobj/OutfitConfig.cpp`, `HX_NATIVE`-guarded so the
Wii match build is untouched):
3. In `MatSwap::Compose`, set/clear `gRB3OutfitComposeActive` with an RAII
   scope for the duration of the function (exactly the Wii
   `bComposingOutfitTexture` pattern already `#ifndef HX_NATIVE`'d there).

Both diffs are saved verbatim in this dir
(`c8-eyes-engine-experiment.diff`, `c8-eyes-rb3-experiment.diff`).

Verification gate: `band-closeup-capture.py` (guitar + vocals), (i) isolated
`RB3_ISOLATE_MESH=eyes.mesh` shows iris+pupil, (ii) full-scene closeups show
no white eye dots, (iii) no band drops (harness PASS), (iv) skin/hair/
clothing/instrument composites look sane (they all change!), compare against
`dolphin-shots/` — faces stay dark/rim-lit, eyes recessed. A/B with the
opt-out. Also rebuild web once at the end (shared engine).

## Traps for the implementer

- **The intermittent magenta/pink wash is PRE-EXISTING** (FINDINGS.md §2
  noted it on baseline 2026-07-01). It appeared in 2 of my 3 experiment runs
  and 0 of 3 baselines — looked causal but a later experiment run (m4) was
  wash-free in the same venue. Do NOT attribute it to this fix; use several
  runs / matched frames when judging. (Attribution of the wash itself: a big
  soft blob renders even with ALL meshes isolated away and zero main-pass
  DrawRects in the trace → it's the particle/billboard or halo path, worth
  its own investigation.)
- Do not scope the multiply by `mRtActiveTex` alone (other RT DrawRect users
  — postproc grade family — have colorMod set; my first attempt washed the
  screen), and never globally (vignette fades use colorMod on main).
- The base fill (colorMod==0) must stay REPLACE — only modes 2/3 multiply.
- `RB3_NO_DEFORM=1` crashes at boot; don't use it as an A/B.
- Compose runs at load for eyes/clothing (dirty-from-load) and at runtime
  for skin (via the 372baf7b re-dirty). Both paths hit the same DrawRect.
- All composited RTs get DARKER with the fix (product ≤ each layer). The
  372baf7b skin fix's visual sign-off ("flesh-toned arms") was calibrated on
  last-layer-wins output; re-judge skin tone against Dolphin GT, not against
  the previous native look.
- Teeth: leave alone; they are (b)-family lighting exposure.

## Independent verification pass (same day, second researcher session)

Re-verified from scratch before handoff; everything above stands:

- **Census reproduced** on a fresh `band-closeup-capture.py --member vocals`
  run (log `/tmp/rb3-bandcloseup-voc-45619.log`, captures `/tmp/c8eyes/voc/`):
  band-member `eyes.mesh` (dir='outfit') draws `eyes.mat` with
  `diffuse='eyes_diffuse_output.tex' isRT=1 blend=1 prelit=0 useEnviron=1
  alphaCut=0 color=white`; band teeth = `teeth.mat` direct `teeth_diff.tex`
  `isRT=0 color=(1,1,0.94)`. NOTE the name trap: crowd-extras eyes/teeth
  (dirs `male_extras01`…) sample plain `eyes_diffuse.tex`/`teeth_diff.tex`
  (isRT=0) — only the four band members use the composited `*_output` RTs,
  so only band eyes glow white; extras get the authored texture.
- **Evidence images inspected** and match their captions: baseline glowing
  dot (`guitar_eye_zoom.png`), isolated featureless bright balls
  (`iso_eye_n03.png`), iris+pupil restored under the multiply experiment
  (`mult_eye_zoom.png`), vs Dolphin GT recessed/no-glow eyes.
- **Working trees clean**: `Rnd_Wgpu_RB3.cpp` (engine) and
  `OutfitConfig.cpp`/`rndobj` (rb3) have no uncommitted experiment hunks;
  both experiment diffs are archived in this dir and apply contextually.
- **Magenta wash pre-existence CONFIRMED**: the fresh verification run —
  with ZERO experiment code applied — produced a full-frame magenta wash on
  `voc_coop_v_n01_0.png` while its sibling shots (`voc_coop_front_n00_*`)
  were clean. The wash is definitively baseline-intermittent, unrelated to
  the compose fix. Fresh baseline face crops:
  `/tmp/c8eyes/fresh_{guitar,singer}_face_zoom.png` (bright eye-whites under
  warm venue light, matching the glowing-dot symptom).

## Files/tools referenced

- Engine: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — DrawRect
  ~:3354 (blend selection ~:3466), quad shader ~:3191, MapBlend
  (`src/gfx/PipelineManager.cpp:307`, Multiply = Dst/Zero), census probe
  ~:5560, IsHaloSourceMat ~:2228.
- rb3: `src/system/bandobj/OutfitConfig.cpp` — `MatSwap::Compose` :107
  (RT branch :123), skin fix block :487+.
- Baselines/captures: /tmp/c8eyes/{voc,g2,g3,iso,mult-iso,mult2,mult3,mult4,base2},
  engine logs /tmp/rb3-bandcloseup-*.log (binary — byte-read + replace-decode).
- Evidence images in this dir: `guitar_eye_zoom.png` (baseline glowing dot),
  `iso_eye_n03.png` (isolated white balls), `mult_eye_zoom.png` (iris+pupil
  restored), `m4_guitar.png` (full-scene recessed eyes with experiment).
