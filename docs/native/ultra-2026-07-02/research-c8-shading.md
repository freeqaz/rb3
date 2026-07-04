# C8 research — over-bright/flat face shading (b) + RndTexBlender stub (c)
2026-07-02, rendering researcher (root-cause only; no fixes landed).

## TL;DR verdicts

1. **(b) Over-bright/flat faces = LIGHTING, not missing texture detail.**
   The venue-light path's **directional term** (chars.env `rim.lit` + the
   no-light-env grey-key fallback) frontal-lights band flesh at full authored
   intensity; killing only the dir term (`RB3_VENUE_DIR_EXPOSURE=0`) collapses
   the band to GT-like rim-lit darkness while the venue stays lit. A
   systematic display-gamma lift (sRGB texels treated as linear + `linearToSrgb`
   re-encode) multiplies whatever light arrives by ~2-3x in display space.
2. **(c) RndTexBlender port = NO-GO as the lever for the GT gap.** The Wii
   target's `DrawShowing__13RndTexBlenderFv` is **4 bytes (blr) at 100% match**
   — the empty stub is FAITHFUL; the Wii game never texblends, and the Dolphin
   GT faces are equally normal-map-flat. Porting DC3's impl cannot close a
   native-vs-Dolphin gap. It is a *separate, optional Xbox-fidelity upgrade*
   (native renders Xbox milos), and on its own it changes **nothing** because
   the RB3 native backend never binds a normal map anywhere (see §4).

Recommended order: fix (b) first (dir-light semantics + probe LightPreset),
treat (c) as later polish behind its own gate.

## 1. Ground truth vs native (evidence)

GT (`docs/native/c8-ground-truth-2026-07-01/dolphin-shots/`):
- `face_singer_rimlit.png` — gameplay singer face is a near-black silhouette,
  rim/back-lit; only teeth/mic catch warm light. Face luma ~0-10/255.
- `face_guitarist_ambient.png` — dark cool directional key, face mostly shadow.

Native (fresh captures this session, `band-closeup-capture.py --member vocals`,
dive-bar venue; crops copied next to this doc):
- `bassist_crop.png` — vocalist flesh (face/neck/chest V) reads as a blown-out
  pale-warm band vs her dark jacket; eyes/mouth glow (issue (a), separate).
- `voc_face_crop.png` — guitarist face flat warm-orange, bright brow highlights.

Flesh-material census (`RB3_HEADMAT_DBG=1`): all band flesh draws with
`prelit=0 useEnviron=1 color=(1,1,1)` sampling the composited
`*_skin_diffuse_output` RTs → they take the full LIT venue path. Not emissive,
not unlit — the brightness must come from the lighting terms.

## 2. A/B attribution (the key experiment)

`band-closeup-capture.py` runs with venue-light knobs (all default-ON venue
path, world.cam only). Crops `cmp_*.png` beside this doc (band region,
`*_coop_front_n00_0.png`):

| run | knob | band flesh appearance |
|---|---|---|
| baseline | — | blown-out pale faces, warm flood |
| `cmp_np.png` | `RB3_VENUE_POINT_EXPOSURE=0` | **still blown out** (warm bright faces persist) |
| `cmp_nd.png` | `RB3_VENUE_DIR_EXPOSURE=0` | **dark, GT-like**; only faithful red point spill remains |
| `cmp_nl.png` | both =0 | full silhouettes ≈ GT rim-lit look |
| `RB3_VENUE_POINT_FALLOFF_LEGACY=1` | falloff law | no meaningful change (face ~35 both) |

⇒ The over-brightening is carried by the **directional** contribution, not the
point lights and not the GX-falloff tail. Caveat: the band prefab rotates per
launch and some `n01` shots catch venue light flashes, so region-luma means are
noisy across runs — the verdict above is from matched `front_n00` frames and is
visually unambiguous.

What the dir term is (from `RB3_VENUE_PROBE=1`, gameplay venue):
- The char-scoped environ `chars.env` (`ambRaw=(0,0,0)`, 6 lights) carries
  `rim.lit` **type=1 directional (0.49,0.51,0.49)** + `rim_underneath.lit`
  (black) + 4 white per-member `*_silhouette.lit` points (range 40, at each
  member's station) + crowd light.
- A degenerate no-name env `''` (`ambRaw=(1,1,1)→clamped 0.09`, 0 lights) also
  scopes band outfit meshes → gets the **grey-key fallback directional**
  (0.22 x 0.8 = 0.176 white, fixed screen-ish direction) from
  `WriteSceneUniforms` (Rnd_Wgpu_RB3.cpp ~:1470).

Why GT is dark where native is bright, same authored lights — three candidate
mechanisms (in likely order of contribution):
1. **Backlight semantics lost.** The venue names these lights *rim* /
   *silhouette*: on Wii they read as back/edge light (GT faces are literally
   rim-lit black). Native applies `rim.lit` as a plain lambert directional from
   `light->WorldXfm().m.y` — if the light's transform/direction convention (or
   the Xbox-milo orientation load) is off, a BACKlight becomes a FRONT key and
   paints the whole face. **Next probe:** one-shot dump of `rim.lit`'s
   `WorldXfm().m.y` vs the vocalist's forward vector; expected: light dir
   should oppose the char's facing (backlight). If it doesn't, the fix is the
   direction handling, not exposure tuning.
2. **No runtime light animation/masking.** `LightPreset` (99.9% decompiled,
   compiled into native) animates venue/char light colors per keyframe; on Wii
   the dark verse moments come from preset-dimmed lights. Native reads
   `L->GetColor()` per frame — if presets never Poll/ApplyState in native
   gameplay, chars are lit by static authored maxima. **Next probe:** log
   `LightPreset::ApplyState` calls + `rim.lit` GetColor over a song.
3. **Display-gamma midtone lift (systemic, scene-wide).** Diffuse textures are
   uploaded `RGBA8Unorm` (sRGB bytes, never decoded), lit "in linear", then
   `linearToSrgb`-encoded at fragment output (standard_wgsl.inc:639, W9). Net:
   display = encode(T_srgb x L) vs Wii's display = T_srgb x clamp(L) — for
   L=0.2 that is 0.42 vs 0.13, a ~2-3x lift on dim-lit surfaces. The venue
   floors/exposures were tuned post-encode so the backdrop is calibrated, but
   any un-tuned term (the char dir key) reads much hotter than on Wii. Fixing
   this properly (decode-at-sample → true linear pipeline) is a scene-wide
   recalibration — do NOT flip it casually; note it as the amplifier.

Also noted: `nl`/`nd` runs confirm the flesh-skin composite itself is fine
(dark-lit faces still show correct skin/texture hue — the landed 372baf7b fix
is not implicated).

### "Flat" component
GT's face shading contrast comes from a single oblique/back key. Native sums a
face-on grey key + warm flood → lambert is near-uniform across the face →
flat. Same root cause as the brightness; NOT missing normal maps (GT has none
— see §3). Skinned meshes keep baked vertex-AO luma (kSkinnedVtxChroma mixes
chroma only), so AO is not the gap either.

## 3. TexBlender: what the stub really is

- Wii target: `DrawShowing__13RndTexBlenderFv` size=4 (blr), 100% match; the
  whole `TexBlender`/`TexBlendController` units are 100% matched. **The empty
  DrawShowing is the faithful Wii behavior** (GX has no per-pixel normal
  mapping; HMX compiled the feature out). `RndTexBlendController` on Wii has
  only distance bookkeeping — **no `GetBlendState`** in the Wii binary; DC3
  (360 NG renderer) has the full impl at
  `dc3-decomp/src/system/rndobj/TexBlender.cpp:117-265` + `DrawBlendList`,
  using `TheNgRnd.DrawRect` + `ShaderMgr` + `mesh->DrawFacesInRange`.
- Consequence: `norm_output.tex`/`head_wrinkle_output.tex` never painting is
  Wii-faithful. RESOLUTION.md follow-up #3's framing ("stub → no surface
  detail") is true vs **Xbox**, not vs the Dolphin GT.

## 4. The real blocker for ANY normal-map detail on RB3 native

Porting TexBlender alone would change zero pixels, because the normal-map
chain is severed in two more places:

1. **RndMat discards the normal map at load.** RB3 `Mat.cpp` Load
   (src/system/rndobj/Mat.cpp:159-163 region) reads the Xbox stream's
   `specularRGB, normalMap, emissiveMap, specularMap` (field order confirmed
   against dc3 Mat.cpp:587-588) but throws normalMap + specularMap (+ spec
   color) into temp ObjPtrs. Only `mEmissiveMap`/`mRefractNormalMap` are kept.
   (`mShaderVariation` incl. kShaderVariationSkin IS loaded — usable.)
2. **BandRnd never binds a normal map.** All three material-bind-group
   builders in engine `Rnd_Wgpu_RB3.cpp` (MakeMaterialBindGroup{,Cached,Raw},
   ~:1744/1794/1836) hard-wire binding 3 = `mFlatNormalView` and binding 10 =
   flat detail; `mu.hasNormalMap`/`shaderVariation`/spec uniforms are never
   set (zero-init). The shared shader (`standard_wgsl.inc`) fully supports
   normal maps/TBN/skin+hair variations — DC3's `MaterialSetup.cpp:142` uses
   it today, so the shader side is proven.

## 5. Go/no-go + plan for (c)

**NO-GO now** as a face-fix: wrong lever for the GT gap, and 3 layers deep.
**GO later** as an Xbox-fidelity polish track (house rule: prefer Xbox assets/
fidelity), sliced so each step is independently visible + gated:

- **Slice 1 — static normal maps (no TexBlender needed).** HX_NATIVE-gated
  `mNormalMap`/`mSpecularMap` members appended to RndMat (Wii build untouched
  — members + load-keeps under `#ifdef HX_NATIVE`), bind in the three bind
  group builders + set `hasNormalMap`/`deNormal=0`/`shaderVariation`. Guard:
  unpainted `kRenderedNoZ` RT normal maps (the face `*_output` RTs) must fall
  back to `mFlatNormalView` (reuse the DrawRect unpainted-RT census). Visible
  on venue/instruments/outfits immediately. ~0.5-1 day. Gate:
  `RB3_NORMALMAP_OFF=1`.
- **Slice 2 — TexBlender base-map composite (faces get their static norm).**
  HX_NATIVE body in `RndTexBlender::DrawShowing()` (Wii stays blr): if
  `mOutputTextures` is a NoZ RT and `mBaseMap` set → `BandRnd::DrawRect`
  base→output once (mirror `MatSwap::Compose` / DrawPreClear pattern,
  including the RTT begin-hook + scene-bind-group restore at :3359/:3543).
  ~0.5 day. This is the "minimal viable slice" for faces.
- **Slice 3 — near/far/wrinkle controller blending.** Port DC3
  `GetBlendState` + DrawBlendList; needs mesh-into-RT drawing with a custom
  override material on the RB3 backend (new capability — the current DrawRect
  only does fullscreen quads). 1-2 days, lowest ROI (wrinkles show only during
  strong expressions). Defer until slices 1-2 prove out.

**Evidence gates BEFORE porting (cheap, do first):**
1. Temporarily log the discarded normal-map texPtr names in Mat::Load for
   `head_naked.mat`/`torso_naked.mat`/venue mats — confirm heads reference
   `*_norm_output` RTs and venues reference static `*_norm.tex` (drives the
   slice split).
2. Screenshot DC3-native faces (same engine/shader, normal maps on) as the
   proof-of-shader baseline; no Xenia needed.
3. After Slice 1, `RB3_ISOLATE_MESH` A/B on one normal-mapped venue mesh.

## 6. Recommended next actions for (b) — the actual face fix

1. **Probe light directions:** one-shot dump of every type=1 light's
   `WorldXfm().m.y` in the char-scoped envs + each band member's forward;
   verify backlights are behind. If flipped/identity → fix direction load or
   convention (this single bug would explain frontal blowout AND flatness).
2. **Probe LightPreset:** does `LightPreset::ApplyState`/Poll fire in native
   gameplay? Do `rim.lit`/silhouette colors change over the song? If static →
   wire preset polling (code is 99.9% decompiled already).
3. Consider a char-environ-scoped dir exposure (like sVenueDirExposure but for
   envs whose lights are named rim/silhouette) only as a stopgap; the faithful
   fix is 1+2.
4. Keep hands off the gamma pipeline in this pass; document-only (§2.3).

## Repro commands
```
cmake --build ~/code/milohax/rb3/native/build-native --target rb3-native
python3 scripts/native/band-closeup-capture.py --member vocals --out /tmp/x --tag t
# knobs: RB3_VENUE_DIR_EXPOSURE=0 | RB3_VENUE_POINT_EXPOSURE=0 |
#        RB3_VENUE_PROBE=1 | RB3_HEADMAT_DBG=1  (env is inherited by the game)
# logs: /tmp/rb3-bandcloseup-<tag>-<pid>.log  (grep -a — log has binary bytes)
```
Evidence files beside this doc: `bassist_crop.png`, `voc_face_crop.png`,
`cmp_nd.png`, `cmp_np.png`, `cmp_nl.png`.

---

# ADDENDUM — 2nd research pass (2026-07-02 evening): root cause pinned to the mLightsReal/mLightsApprox split

This pass executed §6's "next probes" 1-2 and found the deeper mechanism. It
**supersedes §2's candidate-mechanism ranking**; everything in §3-§5 (TexBlender
NO-GO + slices) stands and is further confirmed.

## A1. §6 probe #1 ANSWERED — direction handling is NOT the bug
Temp probe (reverted) printed `WorldXfm().m.y` for every venue light.
`chars.env` `rim.lit` fwd=(-0.05,-0.87,-0.48) — a genuine back/top light
(stations run +y back: vocals y=18 … drums y=206; rim sits at y=166, z=165).
With the shader's `-lightDirs` convention it CANNOT light a face frontally.
The frontal flood instead comes from `rim_underneath.lit` fwd=(0,0.43,0.90)
(= light FROM front-below) once LightPreset recolors it from black, plus the
wholesale approx→Lambert promotion below. Mechanism §2.1 (flipped backlight)
is **refuted**.

## A2. §6 probe #2 ANSWERED — LightPreset is (at least partly) alive
A per-(env, dirCount, pointCount) signature probe showed `chars.env`
shifting dl=1↔2, pl=4↔0 during play, i.e. light Showing()/colors DO change
with cues. Mechanism §2.2 ("no runtime light animation") is **not the main
gap** (preset completeness still worth an audit, but lights are not static).

## A3. NEW ROOT CAUSE — native reads the wrong half of the environ's lights
Wii semantics (`src/system/rndobj/Env.cpp`):
- `mLightsApprox` are literally the "fake" lights (`IsFake()` tests that list).
  `UpdateApproxLighting()` feeds them through `BoxMapLighting` into a 6-face
  color box evaluated at the OBJECT's position and applies it as GX **ambient**
  — approx lights NEVER produce Lambert directional shading on Wii.
- `mLightsReal` are the GX hardware lights = the actual directional shading.

Native `WriteSceneUniforms` iterates **mLightsApprox only** and promotes each
approx light to a full Lambert directional/point at 0.7-0.8 exposure, and never
reads mLightsReal. Probe dump (club venue):

```
[VENUE_REAL] env=chars.env light 'main.lit' type=0 show=1
             color=(0.93,0.69,0.99) range=755.0 pos=(-14.8,-173.2,215.8)
```

`main.lit` = the front-of-house key (audience side, high, huge range, dim
warm-lavender) — **the light Wii actually shades band faces with** — is
entirely ignored. Meanwhile the approx rim/silhouette set (box-ambient on Wii)
is applied as full directionals. Net: faces are uniformly frontal-lit by
promoted underlight/rim dirs (bright + flat + cue-static shading), instead of a
dim, attenuated, directional front key with a real shadow side (GT).
`char.env` (real: `Light.lit`, `Light02.lit` warm points) and the other
`*_char.env`s show the same pattern.

## A4. Corrected A/B evidence (time-anchored — the §2 table has a cue confound)
Un-anchored captures land at arbitrary songMs and lighting cues flip the look
(~20.7s has a pink-flash/silhouette cue that fakes results — it produced both a
false "grey key did it" and polluted the earlier dir0 read). Re-run matrix with
`--anchor-ms 25000` (matched cue, `/tmp/c8shade/m-*`):
- baseline: warm flat readable faces;
- `RB3_VENUE_GREY_KEY=0`: **unchanged** → grey key exonerated for gameplay
  chars (chars.env always has ≥1 usable light so the fallback never engages;
  the §2 note that env='' scopes band outfits is TRUE ONLY IN THE MENU HUB —
  keyed probes show gameplay flesh under chars.env the entire window);
- `RB3_VENUE_DIR_EXPOSURE=0`: chars collapse to dark GT-like silhouettes →
  today's face light is entirely the promoted approx directionals.
**Always pass `--anchor-ms` when A/B-ing character lighting.**

## A5. Recommended fix (b) — contained in the venue path, DC3-safe
In `BandRnd::WriteSceneUniforms` venue branch only:
1. Fill dir/point slots from `venv->mLightsReal` (same Showing/color guards;
   `main.lit` is a point → existing GX-falloff machinery applies).
2. Demote `mLightsApprox` to an ambient fold (first cut: uniform ambient +=
   Σ color × falloff × ~1/6; later: pack a 6-face box in spare SceneUniforms
   pad slots, default 0 = legacy — the converge-venue-lighting gating pattern).
3. Keep the grey key only when BOTH lists are empty.
Gate default-ON, opt-out `RB3_CHAR_REAL_LIGHT_OFF=1` (or fold into
`RB3_VENUE_LIGHT_OFF`). `mLightsReal` is the same ObjPtrList already iterated
(the WASM-hang warning was about ObjDirItr, not this list). Expect knock-on to
venue geometry groups (geom.env etc. also have real lists) → re-run the
converge-2026-06-20 visual gates and retune `RB3_VENUE_*_EXPOSURE`; the §2.3
gamma-lift amplifier still applies on top and stays documentation-only.
Verify: anchored closeups A/B + Dolphin same-venue face oracle
(t2-dolphin-oracle.md). Side benefit: likely tames issue (a) glowing eyes —
eyes.mat has no emissive map (`emisMul` gated on map presence), the "glow" is
the same promoted frontal flood on white sclera.

## A6. TexBlender (c) — additional confirmation of NO-GO
This pass independently confirmed §4: `Rnd_Wgpu_RB3.cpp` has zero NormalMap
references (binding 3 hard-wired to `mFlatNormalView`), Wii-era RndMat has no
normal-map member, and the shader decodes an unpainted/black normal RT to
(0,0,1) flat — so the stub cannot explain any native-vs-Dolphin delta. §5's
slice plan + evidence gates remain the recommendation.

Artifacts this pass: `/tmp/c8shade/` (m-base/m-nokey/m-dir0 anchored matrix,
voc*/probe logs `/tmp/rb3-bandcloseup-*.log`, grep -a). All engine probe edits
reverted; engine repo left with only the pre-existing FxSendNative.cpp edit.
