# W3.2 — BoxMapLighting (SYS-4 faithful per-environ ambient) — PLAN

**Stage:** B.S1 (checkpoint id `B-S1`). **Planner:** Opus. **Date:** 2026-07-06.
**Lane:** B (engine). **Mode this wave:** **PROTOTYPE-ONLY** — no edits to the engine mainline
working tree. Deliverable is this PLAN + a validated prototype build harness + gate design;
implementation lands in **Wave 7** after Lane A settles (WAVE6_REVIEW A6: the overlap with Lane A is
near-certain, so Lane B is pre-declared stop-at-prototype, not conditional).

**Why prototype-only (A6, verified):** per-environ light state is written *exclusively* in
`BandRnd::WriteSceneUniforms` (`Rnd_Wgpu_RB3.cpp:1175`), and the cross-backend shading contract lives
in `gfx/UniformStructs.h` (656-byte `static_assert`) + `gfx/standard_wgsl.inc`. A faithful box-ambient
model cannot avoid all three, and Lane A (W2.1-flip tail + W3.1b projLight/fog) is editing exactly
those files this same wave. A worktree prototype lets S1/S2 do the lighting-path design + a
throwaway build without colliding with Lane A's single-writer ownership.

---

## 0. Scope & exit criteria

**In scope (this wave, prototype):**
1. File:line inventory of where per-environ / BoxMap lighting is approximated or sign-flipped today.
2. Faithful-replacement design grounded in the Wii GX BoxMap source (ground truth: `BoxMap.cpp`,
   `Env.cpp:UpdateApproxLighting`, `Mesh.cpp:UpdateApproxLighting`).
3. A **validated** prototype build strategy (engine worktree + scratch rb3 build dir).
4. Visual gate definition (before/after venue captures + band-closeup lineup gate).

**Exit criteria for THIS stage (B.S1):** PLAN.md committed (rb3 flock); worktree
`/tmp/wave6-boxmap-wt` (branch `wave6-boxmap-proto`) created and HEAD-verified; checkpoint written.
No engine mainline edits. No flag flips. No pin bump.

**Deferred to W3.2.S2 (Wave 7):** actual prototype implementation in the worktree behind a default-OFF
flag `RB3_BOX_AMBIENT`, before/after venue capture set, lineup-gate run, patch series.

---

## 1. INVENTORY — where lighting is approximated / sign-flipped today

All engine line numbers vs `milo-native-engine` HEAD `8e7eddd` (the pinned engine).

### 1a. The approx→ambient collapse (G1 — box ambient absent)
`BandRnd::WriteSceneUniforms` (`src/platform/Rnd_Wgpu_RB3.cpp`):

| Concern | Lines | What it does today | Faithful model |
|---|---|---|---|
| Scalar ambient | `:1312-1325` | `s.ambientColor` = `venv->AmbientColor()` with a hard `>0.85→×0.09` clamp (`sVenueAmbientClamp`) + `0.008` floor (`sVenueAmbientFloor`). **A single RGB scalar for all normals.** | 6-axis box-ambient cube evaluated per object position; ambient is direction-dependent (`BoxMap.cpp`). |
| Approx-as-Lambert (**inverted**) | `:1360-1397` | `litList = mLightsApprox` (default path); iterates and promotes `type==1` directionals into `s.lightDirs[]` **Lambert** slots (`:1377-1385`). | Wii routes `mLightsApprox` (directional + fill) into the **box ambient**, never Lambert (`Env.cpp:172` `IsValidRealLight`, `Env.cpp:318-322`). |
| Char-env demotion knob | `:1398-1421` | When `useReal`, averages `mLightsApprox` colors into a capped ambient scalar (`sCharApproxAmbient 0.11`, `sCharAmbientMax`). A hand-tuned stand-in for the box cube. | The box cube *is* the faithful demotion — no averaging, no `char`-name string test. |
| No-light grey key | `:1422-1434` | Synthetic `(-0.4,-0.5,-0.75)` dir + `sVenueGreyKey × sVenueDirExposure` grey. | An env with only approx lights still produces a real (dim, directional) box ambient — no synthetic key needed. |

### 1b. The real/approx routing inversion (G2 — sign of the selection)
- Default venue path shades from **`mLightsApprox`** as Lambert (`:1360`, `litList = ... : venv->mLightsApprox`).
- The char path (`sCharRealLight()` + env name contains `"char"`, `:1351-1359`) shades from
  **`mLightsReal`** — which by construction holds *only* `kPoint`+`kFakeSpot` (`Env.cpp:172-178`), so
  its directional branch (`:1377`) is **dead code** and it silently extracts point lights only.
- **Net:** the two classes are swapped vs `Env.cpp:172`. Faithful = `kPoint`/`kFakeSpot` →
  hardware-light (Lambert) slots; `kDirectional`/fill → box ambient.

### 1c. Per-*environ* granularity vs per-*object* (G1 architectural)
- Native re-writes `SceneUniforms` on `RndEnviron::sCurrent` change under world.cam
  (`Rnd_Wgpu_RB3.cpp:3934`-region; `mLastSceneEnv` compare). That is **per-environ**, one uniform set
  per environ group.
- The Wii box ambient is **per-object**: `RndMesh::UpdateApproxLighting` (`Mesh.cpp:1463-1480`) calls
  `curEnv->UpdateApproxLighting(&objectPos, mBoxLightColorsCached)` with the *mesh's own* world sphere
  center (`Mesh.cpp:1471-1476`), caching 6 colors **per mesh** (`mBoxLightColorsCached[6]`,
  double-buffered via `mUseCachedBoxLightColors`). Two objects under one environ at different
  positions get different box ambient (point/spot falloff is position-dependent — `BoxMap.cpp:133-158`
  point, `:160-199` spot).
- **Consequence for design:** a faithful port must evaluate the cube **per draw** (object position),
  not once per environ. The point/spot *contribution* is per-object; the directional contribution is
  position-independent (`BoxMap.cpp:122-131`) and can be precomputed per environ.

### 1d. Dropped light types & always-off blocks (G4/G5 — extension, Lane-A-owned)
- `:1376-1396` handles `type==1` (dir) and `type==0` (point) only — **`type==2` fakespot / floorspot
  dropped**. Wii folds fakespot into the box ambient directional queue (`BoxMap.cpp:26-27`
  `kFakeSpot` → `LightParams_Directional`) and real fakespots into `mLightsReal`.
- `:1459-1460` `s.shadowEnabled = 0; s.numProjLights = 0` — hard-off (projLight is **W3.1b**'s
  Lane-A item, not W3.2).
- Fog (`:1450-1458`) is **W3.1a/W3.1b** (Lane A), not W3.2.

### 1e. Shader side (`gfx/standard_wgsl.inc`) — the consumer
- Ambient today: `ambientLight = scene.ambientColor.rgb` (`:835`), a scalar added to the light sum
  (`:835-889`, `softClipLighting` at `:613`).
- No box-ambient cube uniform exists. `SceneUniforms` (`:49-95`) has no `boxAmbient[6]` field; the
  656-byte `static_assert` (`UniformStructs.h:56`, WGSL `:49`) is the cross-backend contract that
  gates any struct growth (this is the DC3 zero-blast surface).

### 1f. Wii ground-truth cross-reference (what `WriteSceneUniforms` should mirror)
- `RndEnviron::UpdateApproxLighting` (`src/system/rndobj/Env.cpp:308-368`): clears a `BoxMapLighting`,
  `QueueLight`s each `mLightsApprox` entry, `ApplyQueuedLights(boxResults[6], &objPos)`, packs to
  `GXColor[6]`, then `ApplyApproxLighting` (a no-op in base `RndEnviron`; `WiiEnviron` uploads the 6
  colors into GX ambient registers). **On native the `ASM_BLOCK` GX-packing degenerates to nothing**
  (`decomp.h:76`), so native can't reuse the packing path — but `BoxMapLighting` itself is pure scalar
  and compiles/links into `rb3-native` today (`BoxMap.cpp.o` present in `build-native`).
- `BoxMapLighting::ApplyLight(Directional)` (`BoxMap.cpp:122-131`) is the cube accumulation:
  `for i in 0..5: d = max(0, dot(sAxisDir[i], lightDir)); d*=d; cube[i] += d * color`. Point
  (`:133-158`) and spot (`:160-199`) reduce to a per-object directional then accumulate the same way.

---

## 2. FAITHFUL REPLACEMENT — design

### 2a. What the Wii computes (MEASURED ground truth)
Per object at world position `P`, for the current environ:
```
cube[6] = {0}                                  // one Hmx::Color per ±X,±Y,±Z axis
for each L in mLightsApprox (Showing):
    switch L.type:
      kDirectional | kFakeSpot: dir = -L.worldXfm.m.y                       // BoxMap.cpp:31
                                cube += ApplyLight_Dir(dir, L.color)         // :122-131
      kPoint:  cube += ApplyLight_Point(L, P)   // dir=(L.pos-P)/|.|, atten,  :133-158
      (spot):  cube += ApplyLight_Spot(...)      // cone atten,               :160-199
// ApplyLight_Dir accumulation, per axis i:
//   d = max(0, dot(axisDir[i], lightDir)); cube[i] += (d*d) * lightColor
```
`sAxisDir[6] = {+X,-X,+Y,-Y,+Z,-Z}` (`BoxMap.cpp:7-9`). The result is a directional ambient cube:
6 colors that a per-vertex normal samples on hardware.

**Per-vertex evaluation (GX ambient cube):** the standard sample the fixed-function pipeline performs
is `ambient(N) = Σ_i max(0, dot(N, axisDir[i])) * cube[i]`. For a unit normal exactly three axes have
positive dot (one per component sign), so `ambient(N) = |N.x|·cube[±x] + |N.y|·cube[±y] + |N.z|·cube[±z]`
— the canonical ambient-cube / 6-face hemisphere sample. **(To be confirmed against `WiiEnviron`'s GX
register setup + Dolphin oracle in S2 — the accumulation weighting (`d*d`) is source-proven; the
per-vertex sample weighting must be validated, not assumed. This is the single largest correctness
risk and is called out as gate G-A below.)**

### 2b. Native structure (prototype)
Three pieces, all in the worktree, all default-OFF behind `RB3_BOX_AMBIENT`:

1. **CPU (per draw), engine `Rnd_Wgpu_RB3.cpp`:** replace the scalar-ambient + approx-as-Lambert
   block (`:1312-1434`) with:
   - Route `mLightsReal` (point+fakespot) → the existing Lambert `s.lightDirs`/`s.pointLight*` slots
     (fixes the G2 inversion — this is the *hardware* light path).
   - Build the box cube from `mLightsApprox` by **calling `BoxMapLighting` directly** (it links
     natively; `#include "rndobj/BoxMap.h"`): `Clear()`, `QueueLight(L, 1)` per approx light,
     `ApplyQueuedLights(cube, &objPos)`. Because the cube is **per object**, this must run at draw
     time with the object's world position — see 2c granularity note.
   - Upload the 6 `cube` colors into a new uniform field.
   Behind `RB3_BOX_AMBIENT`, flag-OFF keeps the exact current block byte-for-byte.

2. **Uniform contract (`gfx/UniformStructs.h` + WGSL struct):** add `float boxAmbient[6][4]` (6×vec4,
   96 B) → `SceneUniforms` grows 656 → **752 B**. **This changes the DC3 cross-backend `static_assert`
   and is the reason Lane B is prototype-only** — the struct-growth decision is a Wave-7 coordinated
   change with DC3 (mirror the W3.1a zero-blast discipline: DC3's `UniformStructs.h` + WGSL must move
   in lockstep or DC3's `static_assert` breaks). *Prototype avoids touching DC3 by building rb3-only
   in the worktree; the DC3 mirror is a Wave-7 gate, not this stage.*

3. **Shader (`gfx/standard_wgsl.inc`):** replace `ambientLight = scene.ambientColor.rgb` (`:835`)
   with the cube sample `ambientLight = boxAmbientSample(worldNormal)` where
   `boxAmbientSample(N) = Σ_i max(0, dot(N, axisDir[i])) * scene.boxAmbient[i].rgb`. Keep
   `ambientColor` as a fallback for the non-venue / flag-OFF path.

### 2c. Granularity decision (the architectural crux)
- **Directional + fakespot cube contribution is position-independent** (`BoxMap.cpp:122-131` uses only
  `lightDir`), so it can be computed **once per environ** in `WriteSceneUniforms`.
- **Point + spot contribution is position-dependent** (`:133-199` use `viewPos`), so it is
  **per-object**.
- **Prototype path (cheapest faithful-enough):** compute the *directional* cube per environ in
  `WriteSceneUniforms` (fits the existing per-environ rewrite seam). Evaluate point/spot approx lights
  as the existing Lambert `pointLight*` path (already per-pixel, already position-correct) rather than
  folding them into the cube. This is a **documented deviation** from the Wii (Wii folds point→box
  ambient) but is *closer* to faithful than today's approx-as-Lambert-directional inversion, and it
  avoids a per-draw CPU box eval. S2 measures both against Dolphin; if the per-environ directional
  cube is insufficient, S2 escalates to a per-draw cube (object position available in `DrawMesh`,
  `obj.world` post-W1.6 `RB3DrawContext`).
- **Deletes on success:** `sVenueAmbientClamp/Floor`, `sCharApproxAmbient/Max`, `sVenueGreyKey`,
  `RB3_CHAR_REAL_LIGHT` char-name string test, the approx-as-Lambert promotion (`:1377-1385`).

### 2d. What stays out of W3.2 (owned elsewhere)
- Fog (W3.1a/b), projLight/fakespot-as-projector (W3.1b), 4→8 light arrays (deferred Wave 7+),
  highway-as-real-lighting (Stage 2, a separate item), exposure/tonemap (Stage 3).

---

## 3. PROTOTYPE BUILD STRATEGY (validated)

**Engine worktree (created + verified this stage):**
```bash
git -C /home/free/code/milohax/milo-native-engine worktree add \
    /tmp/wave6-boxmap-wt -b wave6-boxmap-proto      # DONE — HEAD 8e7eddd == pin
```
This creates a *new branch* and *new dir* and does **not** touch the engine mainline working tree
(HARD RULE compliant). All prototype engine edits happen in `/tmp/wave6-boxmap-wt`.

**Point a scratch rb3 build dir at the worktree (mechanism confirmed from
`rb3/native/CMakeLists.txt`):**
- `MILO_ENGINE_PATH` is a `CACHE PATH` (`:72-73`) consumed by `add_subdirectory(${MILO_ENGINE_PATH} …)`
  (`:224`). Overridable at configure time.
- The pin check (`:76-86`) is **`message(WARNING …)` only — non-fatal**. Pointing at the worktree HEAD
  emits a warning and builds. (No `-DMILO_ENGINE_PIN` override needed; if the warning is noise,
  pass `-DMILO_ENGINE_PIN=<worktree HEAD>` to silence it.)
```bash
flock /tmp/rb3-native-build.lock cmake -S /home/free/code/milohax/rb3/native \
    -B /tmp/wave6-boxmap-build \
    -DMILO_ENGINE_PATH=/tmp/wave6-boxmap-wt \
    -DCMAKE_BUILD_TYPE=Debug
flock /tmp/rb3-native-build.lock cmake --build /tmp/wave6-boxmap-build --target rb3-native -j
```
- Scratch build dir is `/tmp/wave6-boxmap-build` (NOT `native/build-native` — HARD RULE 5).
- The build still contends on `flock /tmp/rb3-native-build.lock` per campaign protocol.
- **De-risk done this stage:** worktree HEAD == pin (no rebuild-the-world from a divergent base);
  `BoxMapLighting` already links into `rb3-native` (`BoxMap.cpp.o` present), so `#include
  "rndobj/BoxMap.h"` + direct calls need no new CMake wiring. Only remaining S2 build risk is Dawn/glfw
  first-configure time in a fresh `/tmp` dir (can be shortened by seeding `CPM`/Dawn cache from an
  existing build dir if S2 needs speed).

**New flag registration (S2, Wave 7):** `RB3_BOX_AMBIENT` registered in
`NativeCompatFlags.classification.json` under `flock /tmp/milo-engine-classjson.lock`, append-only,
**no gen.inc regen** (coordinator regens once at wave end).

---

## 4. VISUAL GATES

The band-closeup drop/ratio gate is **BLIND to lighting** (it scores geometry placement/shards, not
luminance — README SYS-7). So W3.2 needs *lighting-specific* before/after evidence in addition to the
standard no-regression lineup gate.

### G-A (correctness, blocking) — Dolphin matched-frame venue A/B
- Force a known environ + camera and compare native (flag-ON) vs **Dolphin ground truth**
  (`c8-ground-truth-2026-07-01/t2-dolphin-oracle.md`). This is the ONLY gate that can validate the
  per-vertex cube-sample weighting (§2a risk). Ground each environ in `RB3_VENUE_PROBE` light dumps
  (`Rnd_Wgpu_RB3.cpp:1326-1341` already emits type/color/range/pos per light).
- Pass = flag-ON venue faces/backdrop match Dolphin luminance/chroma better than flag-OFF on the same
  matched frame (channel-histogram closeness, `RB3_PP_OFF=1` to compare the raw lit path).

### G-B (before/after venue captures) — songMs-pinned, both tails
- Harness: adapt `scripts/native/band-closeup-capture.py` + `song-select-capture.py` (RB3_HTTP=1 +
  RB3_FIXED_CLOCK=1). **Pin captures to a fixed songMs window** (per WAVE6_REVIEW A2 — wall-clock
  settle varies songMs and authored venue lighting legitimately moves luma).
- Score continuously: mean luma + %pixels>0.95 (blow-out) + %pixels<0.05 (crush) + pink-hue fraction
  (missing-texture/broken-env class vs exposure/bloom class). Capture ON and OFF at the same songMs.
- Venues: at least one moody theater/arena env (coloured stage spots → exercises point/spot path) and
  one ambient-only env (sky/back — exercises the directional cube + no-light fallback removal).

### G-C (no-regression) — lineup gate + drawlog
- `scripts/native/lineup-gate.py` (patch-bearing band lineup) must stay GREEN flag-ON (proves the
  ambient change did not perturb geometry/placement).
- flag-**OFF** must be **byte-identical** to pre-prototype: `drawlog-golden.py --canonical-order
  --fixed-clock` count unchanged + `milo-engine-tests` 198/0/2. Because the prototype is worktree-only
  and default-OFF, flag-OFF == mainline by construction; this is the S2 regression assertion.

### G-D (fail-red) — the anti-blindness proof
- Disable the cube sample (force `boxAmbientSample → 0` or perturb one axis 10×) and show G-A/G-B move
  (faces go black / one side blows out) — proving the gate actually observes the lighting, not just
  passing on a no-op. Mirrors the campaign's fail-red discipline.

---

## 5. RISKS / OPEN QUESTIONS (for S2 / coordinator)

- **R1 (per-vertex sample weighting, §2a):** the `d*d` *accumulation* is source-proven; the per-vertex
  *sample* (`Σ max(0,N·axis)·cube[i]`) is the standard ambient-cube form but must be confirmed against
  `WiiEnviron`'s GX ambient-register setup + Dolphin. If GX uses a different blend (e.g. squared, or
  normalized), the shader eval changes. **Highest correctness risk.**
- **R2 (struct growth 656→752):** crosses the DC3 cross-backend contract. Prototype dodges it
  (rb3-only worktree); the Wave-7 land needs DC3's `UniformStructs.h`+WGSL to mirror or DC3's
  `static_assert 656` breaks. Coordinator-sequenced with DC3 owner. Alternative: pack the 6 colors as
  6×`vec3` (72 B → 728) or reuse existing unused padding — S2 measures the cheapest struct delta.
- **R3 (granularity, §2c):** per-environ directional cube is the cheap path; if Dolphin A/B shows it's
  insufficient (point/spot ambient visibly wrong), S2 escalates to a per-draw cube using the W1.6
  `RB3DrawContext` object position. Flagged, not assumed.
- **R4 (Lane A overlap):** confirmed — `WriteSceneUniforms` + `UniformStructs.h` + `standard_wgsl.inc`
  are Lane A's this wave. Prototype-only avoids the collision; Wave-7 land sequences **after** Lane A's
  W3.1b (projLight/fog) settles in those files, then rebases the prototype onto the post-Lane-A engine
  HEAD.
- **R5 (fakespot ambiguity):** `kFakeSpot` is routed to `mLightsReal` by `IsValidRealLight`
  (`Env.cpp:172`) **but** to the *directional* box queue by `QueueLight` (`BoxMap.cpp:26-27`). The two
  Wii paths treat fakespot differently by context. S2 must not double-count: a fakespot in
  `mLightsReal` drives a hardware light; the box queue's fakespot handling applies only to approx-set
  fakespots. Verify which list real venue fakespots land in via `RB3_VENUE_PROBE`.

---

## 6. Subtasks

- **S1 (Plan, Opus) — THIS STAGE:** inventory + design + prototype harness + gates. Deliverable:
  this PLAN.md + validated worktree + checkpoint. **Exit: PLAN committed, worktree HEAD==pin.**
- **S2 (Prototype, Opus — Wave 7):** implement §2b behind `RB3_BOX_AMBIENT` in `/tmp/wave6-boxmap-wt`,
  build `/tmp/wave6-boxmap-build`, run G-A..G-D, produce a before/after venue capture set + patch
  series. **No mainline engine edit; no pin bump.** Rebase onto post-Lane-A HEAD before landing.
