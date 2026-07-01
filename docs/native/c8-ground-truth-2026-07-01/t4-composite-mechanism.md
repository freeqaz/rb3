# T4 — Character skin-texture composite: end-to-end mechanism + fix recommendation

**Authored:** 2026-07-01 (read-only analysis subagent; no source edited).
**Scope:** map what paints `head_skin_diffuse_output.tex` (and the sibling skin/eye/
norm/wrinkle `_output` textures) end-to-end, inventory the source textures, and
recommend the cheapest correct native fix.

---

## TL;DR (6 lines)

- **What paints the skin DIFFUSE** (`head_skin_diffuse_output.tex`, torso, legs, eye):
  `OutfitConfig::MatSwap::Compose` (`src/system/bandobj/OutfitConfig.cpp:103-201`) — an
  RTT **two-color palette recolor** (base skin-tone rect → `_diff` detail → `_interp_gw`
  → mask → tattoo patches), driven by `skin.cfg`/`eyes.cfg` (both are `OutfitConfig`s).
  **NOT** a `RndTexBlender` — those only make the **normal/wrinkle** outputs.
- **Sources ARE present** in `head.milo_xbox`: `male_head_diff.tex`,
  `male_head_interp_gw.tex`, `skin.pal` (+ torso/legs/eye equivalents). A native
  composite is fully possible from shipped assets — and already runs.
- **The premise that the outputs "stay black" is STALE.** The native WebGPU RTT +
  `DrawRect` + pre-clear composite **already ships and is default-ON** (engine
  `9f635b7`/`5cfaf30`/`1a1f84e`). The skin DIFFUSE composite path is done.
- **What is still missing → the real remaining "flat face" cause:**
  `RndTexBlender::DrawShowing()` is an **empty stub** in RB3
  (`src/system/rndobj/TexBlender.cpp:64`), so `norm_output.tex` + `head_wrinkle_output.tex`
  are never painted → the head samples the engine's **flat-normal fallback** → no
  skin surface-normal detail = flat shading.
- **Recommendation = option (a), ~80% already implemented.** VERIFY the diffuse
  composite fires for band heads (cheap A/B), then complete it by implementing
  `RndTexBlender::DrawShowing` (port DC3's, reuse the *existing* RTT/DrawMesh hook).
- Over-bright eyes = a **separate** eye-material issue (emissive/spec), not this composite.

---

## 1. What produces `head_skin_diffuse_output.tex` (the face-skin DIFFUSE)

It is produced by **`OutfitConfig::MatSwap::Compose`**, a render-to-texture "two-color"
palette recolor. This is a *different* system from the `RndTexBlender`s: those only
produce the NORMAL and WRINKLE outputs (see §5).

### The asset graph (from `orig-assets/extracted/char/main/head/male/gen/head.milo_xbox`)

The head milo contains exactly one `skin.cfg` + one `eyes.cfg` (both class
`OutfitConfig`), two `TexBlender`s (`norm.texblend`, `wrinkle.texblend`), 17
`TexBlendController`s, and these render-target outputs:

| Output tex (`kRendered`, empty) | Produced by | Source inputs |
|---|---|---|
| `head_skin_diffuse_output.tex` | `skin.cfg` OutfitConfig `MatSwap` (`head_naked.mat`) | `male_head_diff.tex`, `male_head_interp_gw.tex`, `skin.pal` |
| `torso_skin_diffuse_output.tex` | `skin.cfg` `MatSwap` (`torso_naked.mat`) | `male_torso_diff.tex`, `male_torso_interp_gw.tex`, `skin.pal` |
| `legs_skin_diffuse_output.tex` | `skin.cfg` `MatSwap` (`legs_skin.mat`) | `male_legs_diff.tex`, `male_legs_interp_gw.tex`, `skin.pal` |
| `eyes_diffuse_output.tex` | `eyes.cfg` OutfitConfig `MatSwap` (`eyes.mat`) | `eyes_diffuse.tex`, `eyes_interp_gw.tex`, `eyes.pal`/`eyes_secondary.pal` |
| `norm_output.tex` | **`norm.texblend` (RndTexBlender)** | 5 `male_norm_*.mesh` + `male_head*_norm*.tex` via `norm_*.texblendctl` |
| `head_wrinkle_output.tex` | **`wrinkle.texblend` (RndTexBlender)** | `male_wrinkle_*.mesh` + `male_head_wrinkles_{near,far}.tex` via `wrinkle_*.texblendctl` |

### The wiring: `OutfitConfig::SetSkinTextures` (`OutfitConfig.cpp:428-482`)

For each skin material (`torso_naked.mat`, `legs_skin.mat`, `feet_skin.mat`,
`feet_socks_skin.mat`, `head_naked.mat`) it:
- finds the SOURCE diffuse `MakeString("%s_%s_diff.tex", gender, part)` → e.g.
  `male_head_diff.tex` (`:439-441`),
- stashes it + the interp map into the `skin.cfg` MatSwap:
  `curswap.mTwoColorDiffuse = curtex` (`:449`), `mTwoColorInterp = interptex`
  (`:450-454`),
- finds `MakeString("%s_skin_diffuse_output.tex", part)` → `head_skin_diffuse_output.tex`
  and binds it as the material's diffuse: `curmat->SetDiffuseTex(difftex)` (`:456-460`).

So `head_naked.mat`'s diffuse becomes the empty RT; the source detail + interp + palette
live in the `MatSwap` awaiting the composite.

### The composite: `OutfitConfig::MatSwap::Compose` (`OutfitConfig.cpp:103-201`)

The `else` branch (`:119`, taken because the material's diffuse is a `kRenderedNoZ` RT):
```
:133  sCam->SetTargetTex(diffTex);          // redirect rendering into head_skin_diffuse_output.tex
:143  sCam->Select();
:160  TheRnd->DrawRect(rect, baseColor, sMat,...)   // layer 0: solid skin-tone (mColor1Palette->GetColor)
:161-170  if (mTwoColorDiffuse) DrawRect(...)        // layer 1: male_head_diff.tex, kColorModModulate, color2 tint
:171-176  if (mTwoColorInterp)  DrawRect(...)        // layer 2: male_head_interp_gw.tex, modulate
:177-182  if (mTwoColorMask)    DrawRect(...)        // layer 3: mask, kColorModAlphaUnpackModulate
:185-187  for(...) patches[i].Render(diffTex, sMat)  // layer 4: tattoos / decals / band-logo (BandPatchMesh)
:191  sCam->SetTargetTex(nullptr);          // finish → FinishDrawTarget()
:195  prevCam->Select();
```
Only the Wii-GX hardware bits are gated out (`#ifndef HX_NATIVE`: `WiiTex::bComposingOutfitTexture`,
`GXPixModeSync` — `:188-199`). Everything else is platform-agnostic C++.

`skin.pal`/`eyes.pal` are `ColorPalette` objects; the chosen skin tone is
`mColor1Palette->GetColor(colors[mColor1Option])` (`:152-155`). So the composite = a
palette-tinted base + detail/interp modulate. There is **no** `CharDriverImage` /
`skin`-material-pass class; `skin.cfg`/`eyes.cfg` are `OutfitConfig`s and the whole
thing is `OutfitConfig` + `MatSwap` + `ColorPalette`.

---

## 2. Source-texture inventory — ARE the inputs shipped?

**Yes.** All diffuse/interp/palette sources for the default (procedural-skin) composite
are present inside the head milo. From `strings orig-assets/extracted/char/main/head/male/gen/head.milo_xbox`:

**Skin diffuse sources (the composite inputs):**
- `male_head_diff.tex` — face-skin detail diffuse (brows/lips/pores baked in)
- `male_head_interp_gw.tex` — grayscale "interp" weight map (recolor gradient)
- `male_torso_diff.tex`, `male_torso_interp_gw.tex`
- `male_legs_diff.tex`, `male_legs_interp_gw.tex`
- `eyes_diffuse.tex` (`eyes_diffuse.bmp`), `eyes_interp_gw.tex`, `eyes_mask_gw.tex`, `eyes_spec.tex`
- `skin.pal`, `skin.cfg`; `eyes.pal`, `eyes_secondary.pal`, `eyes.cfg`

**Normal/wrinkle sources (for the RndTexBlender composite, §5):**
- `male_head00_norm.tex`, `male_head_norm01.tex`, `male_head_spec.tex`
- `male_head_wrinkles_near.tex`, `male_head_wrinkles_far.tex`
- deform-region meshes: `male_norm_{chin,eye,mouth,nose,shape}.mesh`,
  `male_wrinkle_*.mesh` (cheek/chin/eyes/forehead/mouth/nosewings, L/R)
- `head.deform`, controllers `norm_*.texblendctl`, `wrinkle_*.texblendctl`

**Teeth/tongue/eyelense:** `teeth_{diff,norm,spec}.tex`, `teeth.mat`, `tongue.mat`, `eyelense.mat`.

There is **no** pre-composited flesh `.tex` in `orig-assets` — the composite is
genuinely runtime. The `_diff.tex` are neutral detail maps (not tone-tinted), so binding
one raw would look wrong-toned; the palette tint is essential.

(`skin.cfg`/`skin.pal` are not standalone files — they are milo objects inside
`head.milo_xbox`, serialized in the OutfitConfig block near byte offset 494k.)

---

## 3. Who triggers the paint, and the RTT mechanism

### Trigger (platform-agnostic, `src/`)

The composite is a **pre-clear drawable**. `OutfitConfig::DrawPreClear()`
(`OutfitConfig.cpp:945-1048`) runs every frame the config is dirty (`unk38 != 0`, set by
`Recompose()` — `:418`, cleared `:1025`):
- `mTexBlender->DrawShowing()` (`:946-947`) → `norm.texblend` (normal composite)
- `SetSkinTextures()` (`:962`) + the two `Compose` loops (`:1003-1014`, non-two-color then
  two-color MatSwaps) → the skin/eye DIFFUSE composite
- `mWrinkleBlender->DrawShowing()` (`:1045-1047`) → `wrinkle.texblend` (wrinkle composite)

Dirty is (re)armed from `BandCharacter` on (re)load/skin-color change
(`BandCharacter.cpp:2246-2252`, `SetSkinColor` → `Recompose`).

### RTT mechanism (how a draw lands in a texture)

`RndCam::SetTargetTex(tex)` + `RndCam::Select()` (`src/system/rndobj/Cam.cpp:51-71`)
redirect subsequent draws into `tex`; the shared Cam only fires the **END** hook
`RndTex::FinishDrawTarget()` (`:55`,`:67`). A `kRendered` `RndTex` has **no CPU bitmap**
by design (`RndTex::SetBitmap` `Tex.cpp:113-118` — the `kRendered` branch allocates
nothing), so it must be painted on the GPU. `RndTexRenderer::DrawToTexture`
(`TexRenderer.cpp:79-289`) is the *general* render-to-texture pre-pass and its Wii-GX
guts (`GXSetPixelFmt`, `WiiMat`) are `#ifndef HX_NATIVE` (`:253-278`) — i.e. skipped by
the port; the native equivalent lives in the engine.

### Native engine state — the RTT is IMPLEMENTED and default-ON

`../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (RB3 backend `BandRnd`):
- `BandRnd::StartFrame` dispatches `Rnd::DrawPreClear()` **by default**
  (`:1663-1667`; opt-out `RB3_NO_PRECLEAR=1`) → runs `OutfitConfig::DrawPreClear`.
- `BeginDrawTarget(RndTex*)` (`:2012`) lazily creates an RGBA8
  `RenderAttachment|TextureBinding` target keyed in the `sTexGpu` side-table the
  diffuse-bind path reads, suspends the main pass, opens a transparent-clear RT pass.
  `EndDrawTarget()` (`:2075`) resumes the main pass with `LoadOp::Load`.
- The begin is hooked lazily from **both** `DrawMesh` (for mesh RTT) and
  `BandRnd::DrawRect` (`:3354`, hook at `:3364-3366`) — the latter is exactly what the
  `MatSwap::Compose` rect layers need.
- `RB3RttDisabled()` (`:2006`) defaults to **enabled** (off only with `RB3_RTT_OFF=1`).
- Painted RT view is bound as the material diffuse via `GetRB3TexView` (`:920`, `:3428`).
- `RndTex::MakeDrawTarget/FinishDrawTarget` are real (`src/platform/Tex_Wgpu.cpp:236-247`;
  RB3 backend routes through `BeginDrawTarget/EndDrawTarget`).

This is corroborated by the session docs:
`docs/sessions/native/SESSION_2026_06_01_RTT_WRAP.md` ("Outfit `DrawPreClear` default-on…
outfit RTT tints", engine `1a1f84e`) and `OFFSCREEN_RTT_INVESTIGATION.md` (which marks
`CHAR_OUTFIT_DIAGNOSIS.md §2` — the "no-op at every layer" claim that this campaign's
`ROOT_CAUSE.md` step 5 echoes — as **stale**).

> **Correction to `ROOT_CAUSE.md`:** step 5 ("MakeDrawTarget/FinishDrawTarget are weak
> no-ops… the TexBlend composite never runs → the outputs stay black") describes the
> **pre-2026-06-01** engine. It is no longer accurate for the skin DIFFUSE path.

---

## 4. What is STILL missing — the normal/wrinkle `RndTexBlender` composite

This is the concrete remaining cause of **flat/untextured** faces.

- `RndTexBlender::DrawShowing()` in RB3 is an **empty stub**:
  `src/system/rndobj/TexBlender.cpp:64` → `void RndTexBlender::DrawShowing() {}`.
  There is **no** native override (grep-confirmed in `milo-native-engine/src`).
- So `norm.texblend`→`norm_output.tex` and `wrinkle.texblend`→`head_wrinkle_output.tex`
  are **dispatched but paint nothing** (`OutfitConfig::DrawPreClear` calls them at
  `:947`/`:1046`).
- Consequence: the head material's **normal map = `norm_output.tex`** never gets pixels,
  so the engine binds its **flat-normal fallback** (`mFlatNormalView`, bind slot 3 —
  `Rnd_Wgpu_RB3.cpp:1744`, `:1794`). Flat normals → no pore/wrinkle/brow surface shading
  → the "flat, detail-less" face the FINDINGS capture shows.
- **DC3 has the full implementation:** `dc3-decomp/src/system/rndobj/TexBlender.cpp:117+`
  — bind `mOutputTextures` (a `kRenderedNoZ` RT) via `cam->SetTargetTex`, draw `mBaseMap`
  as a rect, then per-controller draw the deform-region **meshes**
  (`blendCtrlr->Mesh()->DrawFacesInRange`) with the near/far maps at distance-weighted
  alpha. It uses DC3's `TheShaderMgr`/`TheNgRnd.DrawRect`; the RB3 port would use
  `RndCam::SetTargetTex` + `TheRnd->DrawMesh`/`DrawRect` — which is precisely the
  begin-hook path the RB3 engine **already** has (mesh-RTT is the sky-dome `clouds_rnd.tex`
  path, proven working). So this reuses existing plumbing rather than adding new.

Over-bright eyes are a **separate** track: `eyes_diffuse_output.tex` is composited by
`eyes.cfg` (same `OutfitConfig::Compose` mechanism, so it should paint); the glow is an
eye-**material** issue (emissive/spec, cf. `CharEyes::Highlight` `CharEyes.cpp:160`), not
a missing composite.

---

## 5. Recommendation — option (a), and it is ~80% already done

Mapping to the three options in the brief:

- **(b) bind the source diffuse directly** — reject. The composite already works, and
  `male_head_diff.tex` is a neutral detail map with no skin-tone tint, so raw-binding
  would look wrong-toned and still lose tattoos/normals. Strictly worse than (a).
- **(c) pre-bake from emulator** — reject as the primary fix. Unnecessary now that the
  runtime composite ships; it also wouldn't cover custom/random skin tones and adds an
  asset-pipeline burden. Keep only as a fallback IF verification shows the runtime
  composite is unreliable for band heads.
- **(a) full runtime composite on the WebGPU backend — RECOMMENDED.** The RTT plumbing
  (`BeginDrawTarget`/`EndDrawTarget`/`DrawRect`, pre-clear dispatch, RT-view-as-diffuse
  bind) already exists and is default-on, so the "big, general" cost is already paid.

### Cheapest correct next steps

1. **VERIFY the skin-diffuse composite actually fires for band heads (≈30 min, no code).**
   Run the band-closeup capture (`scripts/native/band-closeup-capture.py`) with
   `RB3_RENDER_DBG=1` and confirm a line like
   `[dbg] RTT created 512x512 for tex 'head_skin_diffuse_output.tex'`, then A/B
   `RB3_RTT_OFF=1` and `RB3_NO_PRECLEAR=1`. If the diffuse changes between on/off, the
   composite fires (expected) → go to step 2. If it does **not**, the gap is trigger
   wiring (OutfitConfig pre-clear registration / `unk38` dirty for the merged band char
   dir) — debug that first (still cheap).

2. **Implement `RndTexBlender::DrawShowing()`** (the actual remaining flat-face cause).
   Port `dc3-decomp/src/system/rndobj/TexBlender.cpp:117+`, adapted to RB3's
   `RndCam::SetTargetTex` + `TheRnd->DrawMesh`/`DrawRect` + `RndTexBlendController`
   (RB3's controller API differs from DC3's `GetBlendState` — use RB3's
   `GetCurrentDistance`/min/max, `TexBlendController.cpp:18-53`). This reuses the
   engine's existing mesh-RTT begin-hook (no new engine capability). **Effort: S-M**
   (~1 focused session). Gate it `HX_NATIVE`-neutral so the Wii match is untouched, with
   an opt-out env (e.g. `RB3_NO_NORM_BLEND`). This lights up `norm_output.tex` +
   `head_wrinkle_output.tex` → real skin surface-normal detail.

3. **(separate)** Triage over-bright eyes as an eye-material emissive/spec issue.

**Net:** the "faces without skin diffuse" problem is architecturally solved (option (a)
shipped); the residual "flat faces" is a small, well-scoped completion —
`RndTexBlender::DrawShowing` — that reuses the RTT machinery already in the backend.
Verify first so effort targets the real residual rather than re-solving a solved layer.

---

## Key file:line index

- Skin DIFFUSE composite: `src/system/bandobj/OutfitConfig.cpp:103-201` (`MatSwap::Compose`),
  wiring `:428-482` (`SetSkinTextures`), trigger `:945-1048` (`DrawPreClear`),
  dirty `:418`/`:956`/`:1025`.
- `kRendered` RT has no CPU bitmap: `src/system/rndobj/Tex.cpp:98-138` (`:113-118`).
- Cam RTT hooks: `src/system/rndobj/Cam.cpp:51-71`.
- General RTT pre-pass (Wii-GX gated out on native): `src/system/rndobj/TexRenderer.cpp:79-289`.
- **RndTexBlender stub (the gap):** `src/system/rndobj/TexBlender.cpp:64`; members
  `src/system/rndobj/TexBlender.h:30-37`; controller `src/system/rndobj/TexBlendController.cpp`.
- DC3 reference (full composite): `../dc3-decomp/src/system/rndobj/TexBlender.cpp:117+`.
- Native RTT engine (implemented, default-on): `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
  — pre-clear dispatch `:1663-1667`, `BeginDrawTarget:2012`, `EndDrawTarget:2075`,
  `DrawRect:3354` (RTT hook `:3364-3366`), `RB3RttDisabled:2006`, flat-normal fallback
  `:1744`/`:1794`; `../milo-native-engine/src/platform/Tex_Wgpu.cpp:236-247`.
- Asset graph: `orig-assets/extracted/char/main/head/{male,female}/gen/head.milo_xbox`.
- Prior docs: `docs/sessions/native/OFFSCREEN_RTT_INVESTIGATION.md` (current-state; marks
  CHAR_OUTFIT §2 stale), `docs/sessions/native/SESSION_2026_06_01_RTT_WRAP.md`,
  `docs/sessions/native/CHAR_OUTFIT_DIAGNOSIS.md` (§2/§5 stale re: no-op).
