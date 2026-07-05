# Lane 04 — Lighting & Materials: Path to Real Lighting

**Reviewer:** Opus (lighting-materials lane) · **Date:** 2026-07-05 · **Mode:** read-only
**Verdict:** **REFACTOR** — the uniform/shader scaffolding and the material-mode
taxonomy are sound and already partly faithful, but the *light-selection semantics
are inverted* relative to the engine's own real/approx classification, the
approx→box-ambient model is entirely absent, and the gameplay-highway "lighting" is
string-keyed material surgery rather than lighting. These are targeted redesigns of
the light path, not a ground-up rewrite.

---

## Executive summary

Milo (Wii GX) lights a draw in three fixed-function modes chosen per-material
(`rndwii/Mat.cpp:261-304`): **unlit** (register color only), **prelit** (vertex
color is the baked light), **lit** (REG material + REG ambient + up to 8 hardware
lights, N·L clamped). Lights are split into two classes with *fundamentally
different math*: `mLightsReal` (kPoint + kFakeSpot only) drive GX hardware
per-vertex Lambert; `mLightsApprox` (everything else, incl. directionals) are folded
into a **6-axis box ambient** on the CPU (`Env.cpp:308 UpdateApproxLighting` →
`BoxMapLighting`). Environ is a per-drawable global (`RndEnviron::sCurrent`) swapped
as groups draw.

Our engine reproduces the **three material modes faithfully** (unlit/prelit/lit map
1:1 to `standard_wgsl.inc:841`) and the **per-environ draw granularity faithfully**
(the DrawMesh per-environ SceneUniforms rewrite, `Rnd_Wgpu_RB3.cpp:3934`). It also
got the **point-light falloff law faithfully right** behind a flag
(`pointFalloffMode=1` = `1/(1+d/range)`, matching `GXInitLightAttn(...,1,1/range,0)`
at `rndwii/Lit.cpp:38`). But four things diverge structurally:

1. **The real/approx split is inverted.** The venue path reads `mLightsApprox` and
   promotes them to Lambert directionals; the Wii folds `mLightsApprox` into a box
   *ambient* and never Lamberts them. The char path reads `mLightsReal` — which by
   construction (`Env.cpp:172 IsValidRealLight`) holds *only* point+fakespot, so its
   directional loop is dead and it silently extracts point lights only.
2. **Box ambient is not modeled at all** — collapsed to a scalar `ambientColor` with
   clamp/floor/grey-key/char-cap tuning knobs.
3. **Fog, shadows, and spot/projected lights are hard-off** in the RB3 scene path
   (`Rnd_Wgpu_RB3.cpp:1566-1568`), despite the plumbing existing and the data being
   present in `RndEnviron`.
4. **The gameplay highway is not lit** — it's per-draw material-color surgery keyed
   on camera-name + material-name string compares (`surface.mat ×0.12`, rails
   force-prelit, gem glow ×2; `Rnd_Wgpu_RB3.cpp:6160-6205`).

The skin-RTT grey-on-web is **not a lighting bug**: it's an offscreen multi-layer
alpha-composite whose result depends on backend-specific RT format / colorspace /
blend-clear defaults (fault class: RTT state/colorspace contract), already shipped
around by a direct-bind bypass.

`Rnd_Wgpu_RB3.cpp` carries **71 distinct `RB3_*` env flags** (measured); the lighting
subset (`RB3_VENUE_*`, `RB3_CHAR_*`, `RB3_TRACK_LIGHT_*`, `RB3_HIGHWAY_BLOOM_*`) is a
large fraction and every one is a tuning lever over an approximation.

---

## 1. FAITHFUL MODEL — how the Wii lights a draw (MEASURED)

### 1a. Environ is a per-drawable global, swapped mid-frame
`RndEnviron::sCurrent` is a static (`src/system/rndobj/Env.cpp:12`), assigned by
`RndEnviron::Select()` (`Env.cpp:18`) as the scene graph walks groups. Every material
apply reads it: `RndEnviron *env = RndEnviron::sCurrent;` (`rndwii/Mat.cpp:143,201`).
So different mesh groups in one camera pass are lit by different environs. **Our
per-environ rewrite mirrors this correctly** (`Rnd_Wgpu_RB3.cpp:3934`, gated on
`RndEnviron::sCurrent != mLastSceneEnv` under world.cam).

### 1b. Two light classes with different math (the crux)
`RndEnviron::AddLight` (`Env.cpp:205`) routes by `IsValidRealLight` (`Env.cpp:172`):

```
IsValidRealLight(l) == (type == kPoint || type == kFakeSpot)   // Env.cpp:172-178
  true  -> mLightsReal
  false -> mLightsApprox   // directional (type 1), everything else
```

- **`mLightsReal`** (point + fakespot only): loaded into GX hardware light registers
  0-7 (`WiiEnviron::SetLight`→`WiiLight::Update`→`GXLoadLightObjImm`,
  `rndwii/Lit.cpp:47`) and shaded by GX per-vertex hardware Lambert with
  `GX_DF_CLAMP` (`rndwii/Mat.cpp:291,299`). Point attenuation:
  `GXInitLightAttn(Intensity, 0,0, 1, 1/mRange, 0)` → distance term
  `1/(1 + d/range)` (`rndwii/Lit.cpp:38`). Fakespot uses `GXInitLightDistAttn` +
  `GXInitLightSpot` (`rndwii/Lit.cpp:40-41`).
- **`mLightsApprox`** (directionals + fills): NOT hardware lights. Folded into a
  **6-axis box ambient** by `RndEnviron::UpdateApproxLighting` (`Env.cpp:308`):
  `BoxMapLighting::QueueLight` per approx light → `ApplyQueuedLights(boxResults[6],
  pos)` → packed to `GXColor[6]` → `ApplyApproxLighting`. This is a directional
  *ambient cube* evaluated at the object position, not a Lambert term.

**Directional environ keys (`SetDirLight`, `rndwii/Env.cpp:74`)** are a separate
explicit path that can load a directional into a GX slot with
`GXInitLightAttn(1,0,0,1,0,0)` (no attenuation), but the *object-level* classification
still puts authored `type==kDirectional` lights in `mLightsApprox` (box ambient).

### 1c. Three material lighting modes (`rndwii/Mat.cpp:261-304`)
```
!mUseEnviron && !mPreLit  -> UNLIT   : GX_SRC_REG color, GX_LIGHT_NULL (register only)
 mPreLit                  -> PRELIT  : COLOR0=GX_SRC_VTX (vertex color = baked light), ambient zeroed
 else                     -> LIT     : REG mat + REG ambient + lights(lightIds), GX_DF_CLAMP,
                                        allDirectional ? GX_AF_SPOT : GX_AF_NONE
```
These map **1:1** to our shader's three branches (`standard_wgsl.inc:841`,
`material.unlit`/`material.prelit`/else). This taxonomy is faithful and correctly
wired on the RB3 side (`Rnd_Wgpu_RB3.cpp:5932` sets `unlit = !mUseEnviron && !mPreLit`).

### 1d. Material color / ambient modulation
`diffuseCol = mColor * env->AmbientColor()` when `mUseEnviron`
(`rndwii/Mat.cpp:149-155`), used as GX **material** color (`GXSetChanMatColor`,
`:249-250`); `env->AmbientColor()` is also the GX **ambient** color
(`GXSetChanAmbColor`, `:251-252`). So on Wii the base color is *pre-multiplied* by
ambient before lighting.

### 1e. Per-vertex, GX-clamped
GX lighting is per-vertex hardware T&L; the rasterized channel color is clamped to
[0,1] **before** the TEV texture multiply (a light can only *tint* a surface, never
brighten it past texture). Our shader does per-pixel Lambert (a fidelity upgrade) and
approximates the pre-TEV clamp with `softClipLighting` (`standard_wgsl.inc:613`).

### 1f. Fog is real
`SetFog(doFog, env, cam)` (`rndwii/Mat.cpp:313`), gated by blend mode
(`:310`). Fog color/start/end live in `RndEnviron` (`Env.h:52-56`).

---

## 2. CURRENT MODEL — what our engine computes (MEASURED)

**Container:** `SceneUniforms` (656 B, `gfx/UniformStructs.h:18`), one ring buffer.
Fields: viewProj/view/cameraPos, fog(4), 4 directional (dir+color), ambient(4),
4 point (pos+color+range+mode), shadow block, 1 projected light.

**Writer:** `BandRnd::WriteSceneUniforms(cam)` (`Rnd_Wgpu_RB3.cpp:1297`), called
per-camera (`BeginFrame`/`SetCam`) and re-invoked on environ change under world.cam
(`:3934`). Two regimes:

- **Non-venue cams (game.cam, all menus)** — `Rnd_Wgpu_RB3.cpp:1560-1563`: hardcoded
  **1 white directional `(-0.4,-0.5,-0.75)` + 0.45 grey ambient**. No environ is
  read. This is fully synthetic and **load-bearing** — every menu and the gameplay
  highway depend on it because those cams never resolve an environ.
- **Venue (world.cam) path** — `:1425-1558`: reads `RndEnviron::sCurrent`, packs up
  to 4 directional + 4 point, with a stack of tuning:
  - ambient: clamp `>0.85 → ×0.09` (`sVenueAmbientClamp`), floor `0.008`
    (`sVenueAmbientFloor`) — `:1439-1447`.
  - exposures: point `×0.70`, dir `×0.80` (`sVenuePointExposure/Dir`), clamps 1.8/1.5
    — `:1502-1516`.
  - point falloff: `pointFalloffMode = 1` (GX law) — `:1429` — **faithful**.
  - char environs (name contains "char") with a usable real key: read `mLightsReal`,
    demote approx to a capped ambient average (`0.11/0.14`) — `:1472-1543`.
  - no-light fallback: soft grey key `0.22 × dirExposure` — `:1549-1554`.

**Light packing loop** (`:1486-1519`): handles `type==1` (directional) and
`type==0` (point) only. **`type==2` fakespot and `type==3` floorspot are dropped.**

**Always-off in the RB3 path** (`:1566-1568`): `fogEnabled=0`, `shadowEnabled=0`,
`numProjLights=0`. Fog, shadows, and projected/gobo lights are dead despite the
plumbing existing (SceneUniforms carries all three; ShadowPass.cpp exists but is
DC3-driven — `mShadowView` is bound as a white fallback, `:1574`).

**Gameplay highway "lighting"** — NOT in the scene uniforms. It is per-draw material
surgery in DrawMesh, keyed on `cam->Name()=="game.cam"` + `mat->Name()` string
compares (`Rnd_Wgpu_RB3.cpp:6160-6205`): `surface.mat` base `×0.12`, `rails.tex`
force-prelit, gem-smasher glow `×2`. This is the single largest hack cluster in the
lighting path and is **entirely load-bearing** for the shipped highway look.

**Shader combine** (`standard_wgsl.inc:835-889`): `unlit||prelit → baseColor` else
`baseColor * softClip(ambient + Σdiffuse·shadow) + Σspecular·shadow`, then additive
emissive/rim/environ, then fog (dead), highlight compression, and — because the web
canvas is a non-sRGB format — a `linearToSrgb` output encode (`:639,886`).

---

## 3. GAP ANALYSIS — what faithful lighting needs that this can't express

| # | Faithful requirement | Current state | Fix class |
|---|---|---|---|
| G1 | **Approx lights = 6-axis box ambient**, per object position (`Env.cpp:308`) | Collapsed to scalar `ambientColor` + clamp/floor/grey/char-cap knobs; approx *promoted to Lambert directionals* in venue path | **Redesign** — add ambient cube (6×vec3) to SceneUniforms; port `BoxMapLighting`; eval per object. Per-*object* (not per-environ) granularity → current per-environ rewrite can't capture two objects at different positions under one environ |
| G2 | **Real/approx routing** = point+fakespot→HW lights, directional/fill→box ambient (`Env.cpp:172`) | **Inverted.** Venue reads `mLightsApprox` as directionals; char reads `mLightsReal` (point+fakespot only → its directional loop is dead, extracts points only) | **Redesign** of the selection logic — not tuning |
| G3 | **8 GX hardware lights** | 4 dir + 4 point caps | Extension (bump arrays) — cheap |
| G4 | **Fakespot / floorspot / projected gobo** lights (`Lit.cpp:40`, `SceneUniforms.projLight*`) | Dropped; `numProjLights=0` always | Extension — plumbing exists, wire from environ |
| G5 | **Fog** from environ (`Env.h:52`, `rndwii/Mat.cpp:313`) | `fogEnabled=0` hard-off | Extension — cheap, data present |
| G6 | **Highway lit by a track environ** | String-keyed material color surgery under game.cam | **Redesign** of that look |
| G7 | **Exposure/tonemap** = GX pre-TEV [0,1] clamp + `RndEnviron` mExposure/mWhitePoint/mUseToneMapping (loaded, `Env.cpp:122`, unused) | `softClipLighting` knee + per-venue exposure knobs approximate it; environ's own exposure fields ignored | Refactor — replace knobs with faithful clamp + environ exposure |

**Is it extension or redesign?** The SceneUniforms *container* is extensible for
G3/G4/G5 (add fields, fill from environ). But G1 (box ambient, per-object), G2
(inverted classification) and G6 (highway) are architectural: the per-environ /
per-scene granularity cannot express per-object box ambient, and the light-list
semantics are wrong versus the engine's own `IsValidRealLight`. These are
*targeted redesigns of the light-selection + ambient model* — the shader scaffolding
and uniform ring stay.

---

## 4. Skin-RTT grey composite — architectural read

**Mechanism (MEASURED, from source + `c8-ground-truth-2026-07-01/RESOLUTION.md`):**
The outfit skin texture is built into an **offscreen render target at frame start**:
`Rnd::DrawPreClear()` (`Rnd_Wgpu_RB3.cpp:1726-1742`) iterates registered pre-clear
drawables → `OutfitConfig::MatSwap::Compose` → `BandRnd::DrawRect` layers into a
`kRenderedNoZ` RT. The two-color composite shader (`Rnd_Wgpu_RB3.cpp:3390-3398`):
`src = diff.rgb * color2, src.a = w`, SrcAlpha-**over** a `diff*color1` base →
`diff·lerp(color1,color2,w)`. The skin mesh then samples that RT.

**Why web ≠ native — fault class, not lighting:** the correctness of this path
depends on state that is *backend-defined*, not on the lighting math:
- **Colorspace contract (most likely).** The main shader applies `linearToSrgb`
  *only* because the web surface is a non-sRGB format (`standard_wgsl.inc:630-643`).
  If the composite RT's format or its sample path disagrees on sRGB-ness between
  Dawn-native and the browser (ANGLE/WebGPU), the baked `diff×tone` lands at the
  wrong luminance/chroma → the grey/desaturated skin. (HYPOTHESIS — cheapest trail;
  not confirmed from source alone.)
- **Blend/clear defaults.** SrcAlpha-over *requires* the `diff*color1` base already
  resident in the RT; if the web backend's RT load-op clears to a different value or
  the base DrawRect layer doesn't land, the composite "collapses toward grey." This
  is the same blend-equation fragility that already produced the REPLACE (white-flat)
  and DEST-MULTIPLY (black) collapses documented in
  `render-regress-2026-07-02/FINDINGS.md §2`.
- **Timing** is **DISPROVEN** as the cause (per memory: inputs were correct +
  resident, not an async upload gap).

The shipped fix (memory `266ffb1b`) *bypasses the RTT composite* on web with a
direct-bind `diff×palette-tone`, which by construction confirms the fault is in the
**RTT composite step**, not the inputs or the lighting. **Characterization:** an
offscreen multi-layer alpha composite whose result is contingent on backend RT
format + colorspace + blend/clear defaults — an *RTT state/colorspace-contract fault*.
Root cause is cheaply *narrowable* (dump the composite RT native vs web via
FrameCapture and diff; check RT `TextureFormat` sRGB-ness on both) but the source
alone doesn't pin it, and it is orthogonal to the lighting redesign.

---

## 5. RECOMMENDATIONS — staged path to real lighting

**Stage 0 — cheap faithful fills (extension, low risk):**
- Wire **fog** from `RndEnviron` (`fogEnabled/Start/End/Color` ← `FogEnable()`,
  `GetFogStart/End`, `FogColor()`). No hack deleted; fills a real gap.
- Bump directional + point light arrays **4 → 8** (GX cap). Uniform + shader loop
  extension.
- Populate `projLight` from environ **fakespot(s)** — plumbing already exists
  (`SceneUniforms.projLight*`, `standard_wgsl.inc:818`).

**Stage 1 — light-selection redesign (the core fix):**
- Replace the inverted heuristic with the faithful split (`Env.cpp:172`): route
  `kPoint`/`kFakeSpot` to hardware-light slots, route `kDirectional`/fills to the
  ambient model. Stop promoting `mLightsApprox` → Lambert directionals.
- **Port `BoxMapLighting`** (`Env.cpp:308`, `rndobj/BoxMap`) to compute the 6-axis
  ambient cube per environ (per-object if affordable); upload 6×vec3; shader
  evaluates by `worldNormal`. This replaces scalar ambient + the char-ambient average
  + the grey-key fallback all at once.
- *Deletes:* `RB3_CHAR_REAL_LIGHT` path + `sCharApproxAmbient/Max`, `sVenueGreyKey`,
  `sVenueAmbientClamp/Floor`.

**Stage 2 — highway as real lighting:**
- Give **game.cam a track environ** so the highway is environ-lit; delete the
  `surface.mat ×0.12` / rails force-prelit / gem `×2` string-keyed surgery
  (`Rnd_Wgpu_RB3.cpp:6160-6205`). Largest hack cluster removed.

**Stage 3 — faithful exposure/tonemap:**
- Implement the GX pre-TEV `[0,1]` channel clamp and drive exposure from the
  environ's own `mExposure`/`mWhitePoint`/`mUseToneMapping` (already loaded,
  `Env.cpp:122`, currently ignored). *Deletes:* `sVenuePointExposure/DirExposure`
  knobs and the `softClipLighting` knee tuning.

**Verification (per stage):**
- **Matched-frame A/B on venue** via the Dolphin oracle
  (`c8-ground-truth-2026-07-01/t2-dolphin-oracle.md`): force a known environ + camera,
  compare native vs Dolphin. Ground each environ in `RB3_VENUE_PROBE` light dumps
  (already emits type/color/range/pos per light, `:1493`).
- **Per-environ exposure:** capture the raw lit path (`RB3_PP_OFF=1`), compare channel
  histograms to Dolphin — the exposure knobs exist *because* the raw sum runs hot;
  the faithful clamp should remove the need.
- **Skin composite (separate track):** FrameCapture-dump the composite RT native vs
  web, diff, and audit the RT `TextureFormat` sRGB-ness on both backends.

**Verdict: REFACTOR.** The bones are right — the unlit/prelit/lit taxonomy, the
per-environ draw granularity, and the GX point-falloff law are already faithful, and
the uniform ring + shader are a clean, extensible contract. But three load-bearing
inversions/absences (approx→box-ambient missing, real/approx routing inverted,
highway = material surgery) mean the current model cannot express faithful lighting
without redesigning the light-selection + ambient path. That is a bounded refactor of
one subsystem, not an overhaul of the renderer.
