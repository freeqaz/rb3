# W3.1a — Lighting fills: fog + projLight (struct-neutral, zero DC3 blast)

**Lane:** Wave 5 Lane A, item 3 (tail). Runs **after** `W0.3d-fix → W2.1-flip` — same engine file
`src/platform/Rnd_Wgpu_RB3.cpp`, so serial per the standing same-file rule (WAVE5_REVIEW C2).
**Model:** planner Opus. **Behavior class:** CHANGE, **default-OFF** behind registered flags.
**Engine pin:** `609efb7` (do NOT bump). Engine HEAD at plan time: `dbf2758` (W2.1-flip landed).

## Objective

First real SYS-4 lighting work, de-risked to the **struct-neutral** half (WAVE5_REVIEW amendment C1):
fill the `SceneUniforms` **fog** and **projLight** fields that RB3 currently hard-zeros, reading them
faithfully from the current `RndEnviron`. **NO `SceneUniforms`/WGSL/`static_assert` change** — the
fields and their WGSL consumption already exist, so **zero DC3 blast radius** by construction
(DC3's own `WriteSceneUniforms` keeps writing its own values). The 4→8 light-array growth (the real
656-byte cross-backend contract change) is explicitly **deferred to Wave 6 (W3.1b)** and is NOT in
scope here.

## Verified ground truth (re-grepped at plan time — cited lines are LIVE, not the stale brief)

**The zeros to replace** (engine `src/platform/Rnd_Wgpu_RB3.cpp`, `BandRnd::WriteSceneUniforms`,
def at `:1160`):
- `:1287` `RndEnviron* venv = RndEnviron::sCurrent;` — already fetched, **in scope** at the zero-fills below.
- `:1429` `s.fogEnabled = 0;`  ← fog fill replaces this.
- `:1431` `s.numProjLights = 0;` ← projLight fill replaces this.
- `:1436-1441` scene bind group: `e[3].binding = 3; e[3].textureView = mWhiteView;` (`:1440`),
  `e[4] = mSampler`. **WGSL binding 3 = `projLightTex`, binding 4 = `projLightSampler`**
  (`standard_wgsl.inc:103-104`) — RB3 binds a white texture as the gobo today.

**The struct + WGSL already consume these (NO edit needed):**
- `src/gfx/UniformStructs.h:23-27` fog fields (`fogColor/fogStart/fogEnd/fogEnabled`),
  `:49-53` projLight fields (`projLightDir/Color/ProjRow0/ProjRow1/numProjLights`),
  `static_assert(sizeof(SceneUniforms)==656)` at `:56` — **unchanged**.
- `src/gfx/standard_wgsl.inc:872-875` fog consume: `if (isEnabled(scene.fogEnabled) &&
  isEnabled(material.materialFogEnabled)) { … mix(scene.fogColor, finalColor, fogFactor) }`.
- `standard_wgsl.inc:818-832` projLight consume (gobo sample + cone-UV gate), sampled from
  `@binding(3) projLightTex` / `@binding(4) projLightSampler`.

**Faithful RB3 `RndEnviron` API** (rb3 `src/system/rndobj/Env.h`, mirrored into the engine tree):
`bool FogEnable() const` (`:39`), `float GetFogStart() const` (`:55`), `float GetFogEnd() const`
(`:56`), `const Hmx::Color& FogColor() const` (`:52`), owner guard `mAmbientFogOwner` (`:105`;
the getters deref it — MUST null-check like the venue block at `:1288`).

**DC3 reference (the exact port target)** — `src/platform/Rnd_Wgpu.cpp`:
- Fog: `:1355-1364` (`if (env->FogEnable()) { scene.fogEnabled=1; scene.fogStart=env->FogStart();
  scene.fogEnd=env->FogEnd(); scene.fogColor[]=env->FogColor(); }`). Note DC3 uses
  `FogStart()/FogEnd()`; **RB3's accessors are `GetFogStart()/GetFogEnd()`**.
- projLight: `:1556-1595` (first showing `kFakeSpot` in `mLightsReal` with a texture →
  `projLightDir = -lxfm.m.y`, `projLightColor = GetColor()`, rows extracted from
  `light->Projection()`, `numProjLights=1`, `mProjLightTexView = GetGpuTexView(light->GetTexture())`);
  bind at `:1653` (`entries[3].textureView = mProjLightTexView ? mProjLightTexView : mWhiteTexView`).

**NEW findings from this investigation (not in the brief — both load-bearing):**

1. **Fog needs a SECOND edit — the material side.** WGSL ANDs `scene.fogEnabled &&
   material.materialFogEnabled` (`standard_wgsl.inc:872`). RB3's `RB3BuildMaterialUniforms`
   (`src/platform/RB3MaterialBinder.cpp:37`) **never sets `mu.materialFogEnabled`** (grep: 0 hits),
   so it stays 0 (value-init) and fog would render **invisibly** with scene-only fog. RB3's `RndMat`
   has the per-material flag `bool mFog : 1;` (rb3 `src/system/rndobj/Mat.h:329`). So the fog fill
   MUST also set `mu.materialFogEnabled = mat->mFog ? 1 : 0`, **flag-gated**, in the material binder.
   This is what makes the "fog visibly renders" exit achievable.
2. **projLight is engine-only-feasible but ~30 lines heavier than fog.** RB3's `RndLight`
   (`src/system/rndobj/Lit.h`) exposes `mTexture` (`:69`) and `mTextureXfm` (`:72`) as **public**
   members, `enum Type{kPoint,kDirectional,kFakeSpot=2,…}` + `GetType()` (`:37`), and the engine has
   `GetRB3TexView(RndTex*)` (`Rnd_Wgpu_RB3.cpp:783`) — so **no decomp-source edit is needed**. BUT
   RB3's `RndLight` has **no `Projection()` method** (DC3-only). The projection rows must be computed
   from a **local port of DC3's `RndLight::Projection()`** (dc3 `src/system/rndobj/Lit.cpp:93`, uses
   `WorldXfm()`, `mTopRadius`/`mBotRadius`/`mRange` — all public on RB3's RndLight) plus a scene
   **bind-group slot-3 swap** (`mWhiteView` → gobo view). This is why projLight is the **cut-first
   secondary** subtask, not co-equal with fog.

**Flag-OFF byte-identity is trivially provable for both fills** (the hard gate): fog fill guarded by
`RB3EnvFogEnabled()` → OFF leaves `s.fogEnabled=0` and the binder skips `materialFogEnabled` (stays
0); projLight guarded by `RB3EnvProjLightEnabled()` → OFF leaves `s.numProjLights=0` and
`mProjLightTexView` stays null → slot 3 stays `mWhiteView`. No struct/WGSL/layout change, so the
`WgslValidation`/engine-tests cross-backend gate cannot regress.

## Flags (registered, default-OFF, presence-truthy opt-in — pattern of `RB3_PLACEMENT_CONTRACT`)

| Flag | Class | Gates | Default |
|---|---|---|---|
| `RB3_ENV_FOG` | feature | fog fill (scene + material sides) | off |
| `RB3_ENV_PROJLIGHT` | feature | projLight fill + slot-3 gobo bind | off |

Two flags (not one) so fog — the headline, provably byte-identical with a single-file scene edit +
one material line — ships independent of projLight's heavier bind-group/matrix change, and each is
A/B-able alone. Coordinator may collapse to one umbrella flag at reconciliation if preferred; the
plan is written so fog stands alone if projLight is cut.

**classification.json (APPEND-ONLY, under `flock /tmp/milo-engine-classjson.lock`, do NOT run
`gen.inc` regen — coordinator regens ONCE at wave end, D2):** append to
`src/platform/NativeCompatFlags.classification.json`:
```
"RB3_ENV_FOG":       { "class": "feature", "owner": "render/lighting", "faithfulStatus": "not-live: faithful RndEnviron fog fill (FogEnable/GetFogStart/GetFogEnd/FogColor -> SceneUniforms fog + RndMat::mFog -> materialFogEnabled), default-OFF (W3.1a)", "default": "off" },
"RB3_ENV_PROJLIGHT": { "class": "feature", "owner": "render/lighting", "faithfulStatus": "not-live: faithful RndEnviron kFakeSpot projected-light fill (mLightsReal gobo -> projLight* + slot-3 bind), default-OFF (W3.1a)", "default": "off" },
```

## Subtasks

### W3.1a.S1 — Fog fill (scene + material), flag `RB3_ENV_FOG`  — **model: sonnet** — MANDATORY
**Goal:** faithful venue fog renders flag-ON; byte-identical flag-OFF. This is the wave-item's hard exit.
**Files:**
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (flag accessor + fog fill at `:1429`)
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.h` (declare the exported flag accessor)
- `milo-native-engine/src/platform/RB3MaterialBinder.cpp` (material-side `materialFogEnabled`)
- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` (append `RB3_ENV_FOG` row, flock)

**Steps:**
1. Add a flag accessor mirroring `sVenueLightEnabled()` (`:1057`) but **presence-truthy opt-in**
   (default OFF): `bool RB3EnvFogEnabled()` — `static int v=-1; if(v<0){const char* e=getenv("RB3_ENV_FOG"); v=(e&&e[0]&&e[0]!='0')?1:0;} return v!=0;`. Declare it **non-static** (free
   function) so the material binder can call it; add a prototype to `Rnd_Wgpu_RB3.h` (or the shared
   internal header the binder already includes — check `RB3MaterialBinder.cpp`'s includes and pick
   the one that avoids a new include edge).
2. Replace `s.fogEnabled = 0;` (`:1429`) with the gated fill, reading the already-in-scope `venv`
   (`:1287`), guarding `mAmbientFogOwner` (deref-safety) exactly like the venue block:
   ```
   if (RB3EnvFogEnabled() && venv && venv->mAmbientFogOwner && venv->FogEnable()) {
       s.fogEnabled = 1.0f;
       s.fogStart  = venv->GetFogStart();
       s.fogEnd    = venv->GetFogEnd();
       const Hmx::Color& fc = venv->FogColor();
       s.fogColor[0] = fc.red; s.fogColor[1] = fc.green; s.fogColor[2] = fc.blue;
   } else {
       s.fogEnabled = 0;   // unchanged default
   }
   ```
   Do NOT scope this to `world.cam` — fog is a per-environ property; the `venv` fetch already tracks
   `RndEnviron::sCurrent`, which DrawMesh re-writes on environ change. (Faithful: Wii applies GX fog
   whenever the current environ has it enabled.)
3. In `RB3BuildMaterialUniforms` (`RB3MaterialBinder.cpp:37`), near the existing `mu.prelit`/`mu.unlit`
   writes (`:196`/`:206`), add the gated material-side enable:
   `if (RB3EnvFogEnabled()) mu.materialFogEnabled = mat->mFog ? 1.0f : 0.0f;`
   (flag-OFF path leaves it at its current 0 → byte-identical). `mat` is the `RndMat*` already in
   scope; confirm the member is `mFog` and reachable (it is public: `Mat.h:329`).
4. Append the `RB3_ENV_FOG` classification row under `flock /tmp/milo-engine-classjson.lock`. **Do
   NOT run the census `gen`.**
5. Commit (engine repo, under `flock /tmp/milo-engine-git.lock`, stage only S1's files): message
   `W3.1a: fog fill from RndEnviron into SceneUniforms (default-OFF RB3_ENV_FOG)`.

**Verify:**
- Build own dir: `cmake -B native/build-agent-W3.1a -S native -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ && cmake --build native/build-agent-W3.1a --target rb3-native -j`.
- **flag-OFF byte-identity (hard):** `python3 scripts/native/drawlog-golden.py --canonical-order --fixed-clock` against the CURRENT committed `native/tests/goldens/drawlog/splash_screen.json` (888) → PASS; `python3 scripts/native/lineup-gate.py` all layers PASS. (Gate against the **current 888** golden — the placement re-golden is a coordinator post-wave step; do not depend on it.)
- **flag-ON renders (hard):** boot headless with fog, drive to a venue that authors fog, screenshot:
  `RB3_ENV_FOG=1 RB3_HTTP=1 <build>/rb3-native` → `/api/screenshot` on a gameplay venue; confirm the
  fog tint/depth-fade is visibly present vs a flag-OFF capture of the same camera-pinned frame (use
  the `rb3_director_disable`+`rb3_force_shot` recipe for matched frames — the A2/A3/A4 camera-desync
  lesson). Use `RB3_VENUE_PROBE=1` / `RB3_LIGHT_PROBE=1` to confirm `FogEnable()` is true for the
  chosen environ.
**Fail-red:** with `RB3_ENV_FOG=1`, force `materialFogEnabled` OFF (or scene `fogEnabled` OFF) → fog
disappears (proves the AND-gate is the live path, not a coincidental tint).
**Staging:** flag-OFF is the shipped default; the fill is inert until opt-in. No golden re-capture
required (flag-OFF unchanged).

### W3.1a.S2 — projLight fill + slot-3 gobo bind, flag `RB3_ENV_PROJLIGHT` — **model: opus** — SECONDARY (cut-first)
**Goal:** faithful projected (kFakeSpot gobo) light renders flag-ON; byte-identical flag-OFF.
**Files:**
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (flag accessor + `Projection()` local port +
  projLight fill at `:1431` + bind-group slot-3 swap at `:1440`)
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.h` (add `wgpu::TextureView mProjLightTexView;` member near `mWhiteView`, `:335`)
- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` (append `RB3_ENV_PROJLIGHT`, flock)

**Steps:**
1. Flag accessor `RB3EnvProjLightEnabled()` (presence-truthy opt-in, default OFF), same pattern as S1.
2. Add member `wgpu::TextureView mProjLightTexView;` to `Rnd_Wgpu_RB3.h` near `mWhiteView` (`:335`);
   null-init it wherever `mWhiteView` is nulled (`Rnd_Wgpu_RB3.cpp:981`).
3. Port DC3's `RndLight::Projection()` (dc3 `Lit.cpp:93`) as a **local static helper** in the RB3
   backend computing the projection `Transform` from `L->WorldXfm()`, `L->mTopRadius`, `L->mBotRadius`,
   `L->mRange` (all public on RB3's `RndLight`). Verify the ported matrix numerically against a probe
   (dump `proj.m`/`proj.v` for a known fakespot vs the DC3 output shape).
4. Replace `s.numProjLights = 0;` (`:1431`) with the gated fill mirroring DC3 `:1556-1595`, iterating
   `venv->mLightsReal` for the first `Showing()` `kFakeSpot` with `L->mTexture`:
   set `projLightDir = -WorldXfm().m.y`, `projLightColor = GetColor()`, rows from the ported
   `Projection()`, `numProjLights=1`, `mProjLightTexView = GetRB3TexView(L->mTexture)`; else leave 0
   and `mProjLightTexView = nullptr`. Gate the whole block on `RB3EnvProjLightEnabled() && venv &&
   venv->mAmbientFogOwner`.
5. Swap the scene bind-group slot 3 (`:1440`) to
   `e[3].textureView = mProjLightTexView ? mProjLightTexView : mWhiteView;` — flag-OFF / no-fakespot
   → null → `mWhiteView` (byte-identical). **Set `mProjLightTexView` BEFORE the bind group is built**
   (the fill at step 4 runs before `:1436`, so this holds — confirm ordering).
6. Append `RB3_ENV_PROJLIGHT` classification row (flock, append-only, no regen). Commit (own files):
   `W3.1a: projLight fill from RndEnviron fakespots (default-OFF RB3_ENV_PROJLIGHT)`.

**Verify:** build; **flag-OFF** drawlog 888 + lineup PASS (proves the slot-3 swap is inert when off);
**flag-ON** on a venue with an authored kFakeSpot gobo → the projected texture cone renders (matched-
frame A/B). **Fail-red:** perturb the projection rows / force `numProjLights=0` → cone vanishes.
**Staging:** default-OFF; inert until opt-in.
**CUT RULE:** if the `Projection()` port does not converge cleanly (numeric mismatch) OR no in-repo
venue authors a usable kFakeSpot gobo to demonstrate flag-ON, **STOP, land nothing for S2, and file
projLight into Wave 6 (W3.1b) alongside the 4→8 lights work** — record the finding in STATUS.md. Fog
(S1) is the wave exit; projLight is explicitly sheddable (WAVE5_REVIEW R-D: Lane C sheds first, and
within W3.1a fog outranks projLight).

### W3.1a.S3 — Gate consolidation + DC3-safety proof + venue A/B package — **model: opus**
**Goal:** prove the wave-item gates green and DC3 is untouched; hand the coordinator a matched-frame
venue A/B for sign-off.
**Files:** none (source READ-ONLY); writes only `W3.1a/STATUS.md` + the A/B artifact dir
`W3.1a/venue-ab/` (PNGs + a short note).
**Steps / Verify:**
1. **milo-engine-tests 198/0/2** (the cross-backend hard gate incl. WGSL validation): from the engine
   tests build, `DC3_DATA=/home/free/code/milohax/dc3-decomp/orig-assets MILO_LIB=/home/free/code/milohax/dc3-decomp/orig-assets/extracted ctest -j1`. Must be 198 pass / 0 fail / 2 skip. (Proves no DC3
   breakage even though the struct is unchanged — the belt-and-suspenders cross-backend check.)
2. **DC3-safety proof (struct/WGSL byte-unchanged):** `git -C milo-native-engine diff <pin>..HEAD --
   src/gfx/UniformStructs.h src/gfx/standard_wgsl.inc` → **empty** (0 lines). This is the primary
   zero-DC3-blast proof; the engine-tests are the confirmation. (If a full DC3 flavor build is
   reachable, boot it once and assert no visual regression; else the byte-unchanged proof stands per
   the brief.)
3. **flag-OFF byte-identity re-confirmed** on the composed S1(+S2) tree: drawlog 888 canonical PASS +
   lineup PASS.
4. **Venue A/B package:** camera-pinned (`rb3_director_disable`+`rb3_force_shot`) matched frames,
   ≥2 per flag state, `RB3_ENV_FOG=1` (and `RB3_ENV_PROJLIGHT=1` if S2 landed) vs default, on a
   fog-authoring gameplay venue; compare against the Dolphin oracle
   `docs/native/c8-ground-truth-2026-07-01/dolphin-shots/gp_*.png` + `images/retail-screenshots/`.
   A/A pairs mandatory (the pre-existing A/A-variable pink-bloom wash confounds single-frame judgment
   — WAVE5_REVIEW E1). Agent exit = "package produced + fog visibly present + all numeric gates
   green"; the **coordinator's human-eyes sign-off** is the acceptance, not the agent's.
5. Append the final STATUS.md sections (per-subtask done/partial/blocked + commit SHAs).

## Exit criteria (measurable)

1. **[hard] flag-OFF byte-identical:** `drawlog-golden.py --canonical-order --fixed-clock` PASS
   against the current committed 888-draw `splash_screen.json` **and** `lineup-gate.py` all layers
   PASS, with both `RB3_ENV_FOG`/`RB3_ENV_PROJLIGHT` unset. (No re-golden owed — flag-OFF unchanged.)
2. **[hard] fog visibly renders flag-ON:** matched-frame screenshot on a fog-authoring venue shows
   the fog depth-fade/tint with `RB3_ENV_FOG=1`, absent at default; fail-red (break the scene∧material
   AND-gate → fog vanishes) demonstrated once.
3. **[hard] milo-engine-tests 198 / 0 / 2** (incl. the WGSL/cross-backend validation) STILL GREEN.
4. **[hard] DC3 zero-blast proof:** `UniformStructs.h` + `standard_wgsl.inc` diff vs pin is **empty**
   (no struct/WGSL/`static_assert` change).
5. **[soft] projLight** either landed with its own flag-ON cone render + flag-OFF byte-identity, **or**
   cleanly cut and filed to Wave 6 (W3.1b) with the reason recorded — S1 fog is sufficient for the
   wave item.
6. Flags appended to `classification.json` (append-only, flock); **no lane-run `gen` regen** — left
   for the coordinator's single end-of-wave regen. Venue A/B package delivered for coordinator sign-off.

## Files touched (coordinator cross-diff list)

- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — flag accessors, fog fill (`:1429`), projLight
  fill (`:1431`) + slot-3 bind (`:1440`), local `Projection()` port. (**Same file as W2.1-flip →
  serial; W3.1a starts only after the flip commit lands.**)
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.h` — flag-accessor prototype, `mProjLightTexView` member.
- `milo-native-engine/src/platform/RB3MaterialBinder.cpp` — gated `mu.materialFogEnabled`.
- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` — append 2 rows (flock, append-only, NO regen).
- `docs/native/engine-arch-review-2026-07-05/execution/W3.1a/STATUS.md` + `venue-ab/` (docs/artifacts only).

**Explicitly NOT touched:** `src/gfx/UniformStructs.h`, `src/gfx/standard_wgsl.inc` (the DC3 contract),
`NativeCompatFlags.gen.inc` (coordinator regens), `src/App.cpp`, sibling `FxSendNative.cpp`, any
`src/system/rndobj/*` decomp source (projLight uses public RndLight members — no accessor add).

## Risks / conflicts

- **Lane A serial (C2):** W3.1a edits `Rnd_Wgpu_RB3.cpp` at `:1160-1458`; the W2.1-flip edits the same
  file at `:2874`. Functions disjoint, but one shared engine working tree → **W3.1a starts only after
  the flip commit is in** (`W0.3d-fix → W2.1-flip → W3.1a`). Re-grep line anchors at start (the flip +
  re-golden may shift line numbers).
- **classification.json multi-writer (D2):** the flip, W0.6b, and W3.1a all append to the same JSON +
  `gen.inc`. **Append-only rows under `flock /tmp/milo-engine-classjson.lock`; NO lane `gen` regen** —
  coordinator does ONE regen + reconciliation at wave end. (This is W2.6's live-clobber failure mode.)
- **Fog needs the material side** (new finding): if S1 lands only the scene fill and forgets
  `materialFogEnabled`, flag-ON fog is invisible and exit #2 fails — both edits are in S1 for this reason.
- **projLight `Projection()` port** is the riskiest sub-piece (matrix math + bind-group change) →
  gated as SECONDARY with an explicit CUT RULE to Wave 6; never let it jeopardize the fog exit or
  flag-OFF byte-identity.
- **DC3 blast radius = zero by construction** (no struct/WGSL edit), re-proven in S3 by an empty diff
  of the two contract files. Do NOT add light-array slots (4→8) — that is Wave 6 (W3.1b), a real
  656→~1024-byte cross-backend change out of scope here.
- **Do NOT bump `MILO_ENGINE_PIN`** (coordinator, once per wave). Never `git reset/rebase/checkout--`
  on the shared engine tree (hard rule 7).
