# W3.1b — Lighting tail of Lane A: projLight fill + fog visual verification

**Lane:** Wave 6, Lane A **tail** (item after the flip-blocker stages). W3.1b edits the SAME engine
file `src/platform/Rnd_Wgpu_RB3.cpp` `WriteSceneUniforms` (`:1176`–`:1487`) as the flip-blocker
lane, so it is **serial after** them per the same-file single-writer rule (WAVE6_KICKOFF Lane A).
**Model:** planner Opus. **Behavior class:** CHANGE, **default-OFF** behind registered flags.
**Engine pin:** `8e7eddd` (do NOT bump — coordinator-only). Engine HEAD at plan time: `3d76285`
(W2.7 landed on top of W3.1a fog fill `a4bde9f`).

## Where W3.1a left this (verified, not trusted)

- **Fog fill LANDED default-OFF** (`RB3_ENV_FOG`, engine `a4bde9f`). Scene side at
  `Rnd_Wgpu_RB3.cpp:1450-1458`; material side `mu.materialFogEnabled` in `RB3MaterialBinder.cpp`.
  DC3 zero-blast confirmed (`UniformStructs.h` + `standard_wgsl.inc` diff EMPTY, `static_assert 656`
  untouched), flag-OFF byte-identical (drawlog 888/888, lineup PASS), `milo-engine-tests` 198/0/2.
- **Fog is visually UNVERIFIED — asset-blocked.** All 34 boot-reachable environs report
  `FogEnable()==false` (W3.1a S1 boot sweep). Confirmed at plan time: `fog_enable`/`fog_start`/
  `fog_end`/`fog_color` appear in the extracted assets **only as editor property schema**
  (`system/run/config/rnd_objects.dta:653`, `system/run/world/world_objects.dta:183` — descriptor
  templates, defaults disabled), not as an authored-ON value on any reachable venue. W3.1a proved
  the pipeline end-to-end **only** via a temporary force-override that was reverted before commit →
  no reproducible screenshot of visible fog exists.
- **projLight NOT landed.** `s.numProjLights = 0;` at `Rnd_Wgpu_RB3.cpp:1460` (explicit) and the
  `projLight*` scene fields are only implicitly zeroed by `SceneUniforms s{};` at `:1176`. Scene
  bind-group slot 3 is hard-wired to `mWhiteView` (`:1469`).

## Objective

Close the two W3.1a residuals, both default-OFF, zero DC3 blast:

1. **Land `projLight`** — faithful `RndEnviron` `kFakeSpot` projected-light (gobo) fill into the
   already-existing `SceneUniforms.projLight*` fields + slot-3 gobo bind, ported from DC3.
2. **Visually verify `RB3_ENV_FOG`** — make "fog visibly renders" a *reproducible screenshot*, since
   no in-repo asset authors fog. Cheapest reproducible path = a registered default-OFF **probe**
   flag that synthesizes authored fog on the current environ, driving a real `/api/screenshot` A/B
   through the whole pipeline (the honest, permanent replacement for W3.1a's reverted temp probe).

## Verified ground truth (re-grepped at plan time — cited lines are LIVE on `3d76285`)

**Scene struct + WGSL already consume projLight — NO struct/WGSL edit (zero DC3 blast by
construction):**
- `src/gfx/UniformStructs.h:49-53` — `projLightDir[4]`, `projLightColor[4]`, `projLightProjRow0[4]`,
  `projLightProjRow1[4]`, `numProjLights`. `static_assert(sizeof(SceneUniforms)==656)` unchanged.
- `src/gfx/standard_wgsl.inc:90-94` mirror fields; `:103-104`
  `@group(0) @binding(3) var projLightTex`, `@binding(4) var projLightSampler`;
  `:818-830` consume (`if (scene.numProjLights > 0.5) { … textureSample(projLightTex, …) }`).
  → the entire projLight consumption path exists; RB3 only needs to write the fields + bind the gobo.

**The zeros to replace (engine `Rnd_Wgpu_RB3.cpp`, `BandRnd::WriteSceneUniforms`):**
- `:1302` `RndEnviron* venv = RndEnviron::sCurrent;` — function-scope local, **in scope** at both
  the fog fill (`:1450`) and the projLight zero (`:1460`).
- `:1460` `s.numProjLights = 0;` ← projLight scene fill replaces this.
- `:1469` `e[3].binding = 3; e[3].textureView = mWhiteView;` ← slot-3 gobo bind swaps this to the
  gobo view when a projected light is filled.

**RB3 API available (all PUBLIC — no decomp-source edit needed):**
- `RndEnviron::mLightsReal` (`src/system/rndobj/Env.h:94`, public `ObjPtrList<RndLight>`).
- `RndLight` (`src/system/rndobj/Lit.h`): `GetType()` (`:37`), `Showing()` (`:47`),
  `GetColor()` (`:36`), `Range()` (`:34`), enum `kFakeSpot=2` (`:12`); public members `mTexture`
  (`ObjPtr<RndTex>`, `:69`), `mTextureXfm` (`Transform`, `:72`), `mTopRadius` (`:73`),
  `mBotRadius` (`:74`), `mRange` (`:62`). **No `GetTexture()` accessor** (DC3 has it) → use
  `light->mTexture.Ptr()`. **No `Projection()` method** (DC3-only) → port locally (below).
- `GetRB3TexView(RndTex*)` (`Rnd_Wgpu_RB3.cpp:783`) → `wgpu::TextureView`, falls back internally,
  caller uses `mWhiteView` on null.

**DC3 port targets (exact):**
- projLight fill loop: `dc3-decomp/native/src/platform/Rnd_Wgpu.cpp:1597-1636` (first `kFakeSpot`
  in `LightsReal()` with a texture → dir `= -lxfm.m.y`, color `= GetColor()`, rows from
  `light->Projection()`, `numProjLights = 1`, `mProjLightTexView = GetGpuTexView(GetTexture())`,
  `break`). Slot-3 bind: `:1694` `entries[3].textureView = mProjLightTexView ? … : mWhiteTexView`.
- `RndLight::Projection()`: `dc3-decomp/src/system/rndobj/Lit.cpp:93-139` (needs `WorldXfm()`,
  `mTopRadius`/`mBotRadius`/`mRange`, `mTextureXfm`, `Multiply(Transform,Transform,Transform)`, and
  a `MakeShadowBias()` helper).
- `MakeShadowBias()`: `dc3-decomp/src/system/rndobj/Lit.cpp:84-91` (9-line static; bias `m.x=(.5,0,0)`
  `m.y=(0,.5,0)` `m.z=(.5,.5,1)` `v=0`). **DC3-only — port as a file-local static** in
  `Rnd_Wgpu_RB3.cpp`; do NOT add it to rb3 `Lit.cpp` (keeps this engine-only, zero decomp blast).

**Key design decision — no new BandRnd member.** DC3 caches the gobo in `mProjLightTexView` because
its bind group is built in a later function. RB3's `WriteSceneUniforms` fills the scene struct AND
builds the bind group in the **same** function (fill `:1460`, bind `:1465-1474`), so a **function-local**
`wgpu::TextureView projView = mWhiteView;` set inside the fill loop and read at `e[3].textureView`
suffices. No `Rnd_Wgpu_RB3.h` change, no member add.

## Flags (registered, default-OFF, presence-truthy opt-in — pattern of `RB3_ENV_FOG` / `sVenueLightEnabled`)

| Flag | Class | Gates | Default |
|---|---|---|---|
| `RB3_ENV_PROJLIGHT` | feature | projLight scene fill + slot-3 gobo bind | off |
| `RB3_ENV_FOG_FORCE` | probe | synthesize authored fog on the current environ at the fill site (requires `RB3_ENV_FOG` also set); screenshot-A/B instrumentation only, never ships | off |

`RB3_ENV_FOG` is **already registered** (`classification.json:196`) — do NOT re-add. A projLight
force/probe flag is **NOT planned up front**: S1 must first sweep boot-reachable environs for a real
`kFakeSpot`-with-gobo light (venues commonly author spotlights) — if one exists and is reachable via
the harness, that is a byte-real screenshot with no probe flag. Only if the sweep finds none does S1
add a `RB3_ENV_PROJLIGHT_FORCE` probe (same shape as `RB3_ENV_FOG_FORCE`), registered append-only.
Decide from the sweep; record the outcome in STATUS.

**classification.json (APPEND-ONLY, `flock /tmp/milo-engine-classjson.lock`, NO `gen.inc` regen —
coordinator regens ONCE at wave end):**
```
"RB3_ENV_PROJLIGHT":  { "class": "feature", "owner": "render/lighting", "faithfulStatus": "not-live: faithful RndEnviron kFakeSpot projected-light fill (mLightsReal gobo -> projLight* SceneUniforms fields + local RndLight::Projection port + slot-3 gobo bind), default-OFF (W3.1b)", "default": "off" },
"RB3_ENV_FOG_FORCE":  { "class": "probe", "owner": "render/lighting", "faithfulStatus": "probe: synthesizes authored fog params onto the current environ at the fill site so RB3_ENV_FOG renders on a real venue for screenshot A/B (no in-repo asset authors fog); requires RB3_ENV_FOG, default-OFF (W3.1b)", "default": "off" },
```

## Subtasks

### W3.1b.S1 — projLight fill + slot-3 gobo bind, flag `RB3_ENV_PROJLIGHT` — **model: opus** — PRIMARY
**Goal:** faithful venue projected (gobo) light renders flag-ON; byte-identical flag-OFF.
**Files:**
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (flag accessor + `MakeShadowBias` static +
  local `RndLightProjection(const RndLight*)` port + fill loop replacing `:1460` + slot-3 bind at
  `:1469`)
- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` (append `RB3_ENV_PROJLIGHT`
  row, flock; + `RB3_ENV_PROJLIGHT_FORCE` only if the sweep found no real gobo light)

**Steps:**
1. Flag accessor mirroring `RB3EnvFogEnabled()` (`:1169`), presence-truthy opt-in default-OFF:
   `bool RB3EnvProjLightEnabled()` — file-local static latch. (Free function only needed inside this
   TU, so a `static` at file scope is fine; NO header change.)
2. Add file-local static `MakeShadowBias()` (port dc3 `Lit.cpp:84-91`) and file-local
   `static Transform RndLightProjection(const RndLight* light)` (port dc3 `Lit.cpp:93-139` verbatim,
   reading the public `light->mRange/mTopRadius/mBotRadius/mTextureXfm` + `light->WorldXfm()`). Keep
   the DC3 float-temp ordering identical — it is deliberately shaped for FP-bit fidelity; do not
   "simplify".
3. Declare `wgpu::TextureView projView = mWhiteView;` before the fill (default = today's behavior).
4. Replace `s.numProjLights = 0;` (`:1460`) with the gated fill (guard `venv` + `mLightsReal`), port
   of dc3 `:1597-1636`:
   ```
   if (RB3EnvProjLightEnabled() && venv) {
       for (ObjPtrList<RndLight>::iterator it = venv->mLightsReal.begin();
            it != venv->mLightsReal.end(); ++it) {
           RndLight* light = *it;
           if (!light || !light->Showing()) continue;
           if (light->GetType() != RndLight::kFakeSpot) continue;
           RndTex* gobo = light->mTexture.Ptr();
           if (!gobo) continue;
           const Transform& lxfm = light->WorldXfm();
           s.projLightDir[0] = -lxfm.m.y.x; s.projLightDir[1] = -lxfm.m.y.y;
           s.projLightDir[2] = -lxfm.m.y.z; s.projLightDir[3] = 0.0f;
           const Hmx::Color& lc = light->GetColor();
           s.projLightColor[0]=lc.red; s.projLightColor[1]=lc.green;
           s.projLightColor[2]=lc.blue; s.projLightColor[3]=1.0f;
           Transform proj = RndLightProjection(light);
           s.projLightProjRow0[0]=proj.m.x.x; s.projLightProjRow0[1]=proj.m.y.x;
           s.projLightProjRow0[2]=proj.m.z.x; s.projLightProjRow0[3]=proj.v.x;
           s.projLightProjRow1[0]=proj.m.x.y; s.projLightProjRow1[1]=proj.m.y.y;
           s.projLightProjRow1[2]=proj.m.z.y; s.projLightProjRow1[3]=proj.v.y;
           s.numProjLights = 1.0f;
           wgpu::TextureView gv = GetRB3TexView(gobo);
           if (gv) projView = gv;
           break;  // one projected light supported (matches DC3)
       }
   }
   ```
   (Match dc3 field-mapping byte-for-byte: row0 reads `proj.m.*.x`, row1 reads `proj.m.*.y`.)
5. At `:1469` change `e[3].textureView = mWhiteView;` → `e[3].textureView = projView;`. flag-OFF:
   `projView` stays `mWhiteView` → byte-identical bind.
6. **Boot sweep for a real gobo light** (before deciding on a force flag): with `RB3_ENV_PROJLIGHT=1`
   plus a throwaway (reverted-before-commit) probe logging `venv->mLightsReal` `kFakeSpot`+texture
   hits per environ, boot through hub→song-select→gameplay and record which environs (if any) author
   a projected light. If ≥1 → S3 captures its A/B byte-real (no probe flag). If 0 → add
   `RB3_ENV_PROJLIGHT_FORCE` (probe, registered) that synthesizes one `kFakeSpot`+gobo light for the
   A/B, same disclosure discipline as `RB3_ENV_FOG_FORCE`.
7. Register `RB3_ENV_PROJLIGHT` (+ force if needed) in classification.json (flock, append-only, no
   regen). Commit (engine repo, `flock /tmp/milo-engine-git.lock`, own files only).

**Gates (hard, all flag-OFF unless noted):** drawlog-golden `--canonical-order --fixed-clock` PASS
(888/888, unchanged); lineup-gate PASS all layers; `milo-engine-tests` 198/0/2; **DC3 zero-blast**
`git diff <pre>..HEAD -- src/gfx/UniformStructs.h src/gfx/standard_wgsl.inc` EMPTY; `static_assert 656`
untouched. **flag-ON:** boots without crash; the WGSL projLight path compiles (already validated by
`test_wgsl_validation` since the shader is unchanged).

### W3.1b.S2 — fog visual verification via `RB3_ENV_FOG_FORCE` probe — **model: sonnet** — the W3.1a exit-#2 closer
**Goal:** a reproducible screenshot showing visible fog, produced through the real shipping fog path
(scene ∧ material AND-gate), replacing W3.1a's reverted temp probe.
**Files:**
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (add the force hook inside the existing fog
  block)
- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` (append `RB3_ENV_FOG_FORCE`)

**Steps:**
1. Add `bool RB3EnvFogForce()` (file-local static latch, presence-truthy, default-OFF).
2. In the fog block (`:1450`), when `RB3EnvFogEnabled() && RB3EnvFogForce() && venv` (independent of
   `venv->FogEnable()`), synthesize authored fog: `s.fogEnabled=1.0f;` with fixed test params
   (`s.fogStart` = near plane-ish, `s.fogEnd` = a mid venue depth, `s.fogColor` a distinct tint e.g.
   grey-blue) so distant geometry visibly fades. Do NOT alter the non-force branch — the real
   `FogEnable()` path stays exactly as W3.1a shipped it. The material-side `materialFogEnabled` is
   already gated on `RB3_ENV_FOG` alone, so it lights up for the force path too.
3. **Screenshot A/B** via the headless harness (`RB3_HTTP=1 RB3_FIXED_CLOCK=1`, camera-pinned like
   `scripts/native/placement-gate-capture.py`): capture a gameplay/venue frame with
   (a) `RB3_ENV_FOG=1 RB3_ENV_FOG_FORCE=1` and (b) neither set. Numeric detector: mean per-pixel
   diff should show a depth-graded tint on distant geometry (W3.1a measured ~75.7 on its override).
4. **Fail-red (the AND-gate is the live path):** re-run with the scene-side forced ON but the
   material gate held OFF (a throwaway probe that skips `mu.materialFogEnabled`, reverted after) →
   diff collapses to ~baseline (W3.1a measured ~9.3), proving WGSL `scene.fogEnabled &&
   material.materialFogEnabled` is what renders the tint, not a coincidental scene-wide wash.
5. Register `RB3_ENV_FOG_FORCE` (probe) in classification.json (flock). Commit (engine repo).

**Optional (best-effort, only if cheap) numeric rb3-tests gate:** `EngineTestFixture` boots a real
headless Dawn device (`gBandRnd.InitGpu(headless=true)`, cf. `test_wgsl_validation.cpp:74`) — a
`test_env_fog.cpp` could construct a synthetic `RndEnviron` (fog ON) + `RndMat` (`mFog=1`), render a
depth-varying quad through the real pipeline, read back center pixels, and assert a fog tint vs a
no-fog control (fail-red = flag-OFF → no tint). Driving a full `DrawMesh` in-test is known-hard
(`test_draw_log_golden.cpp:24` notes it needs "full camera + material"), so this is **not required**
for the exit — the `RB3_ENV_FOG_FORCE` screenshot A/B is the mandatory proof. Add the gtest only if
the render-and-readback wiring is small; otherwise document the decision in STATUS and skip.

**Gates (hard):** flag-OFF (neither env set) drawlog 888/888 + lineup PASS + `milo-engine-tests`
198/0/2 + DC3 zero-blast diff EMPTY. flag-ON produces the fog screenshot (exit #2).

### W3.1b.S3 — venue A/B package + gate consolidation + STATUS — **model: sonnet** — sign-off
**Goal:** assemble the coordinator sign-off package and re-verify every gate independently.
**Steps:**
1. Create `execution/W3.1b/venue-ab/` with: the fog A/B pair + numeric diff (S2), the projLight A/B
   pair (byte-real venue if S1's sweep found one, else the `RB3_ENV_PROJLIGHT_FORCE` A/B), each with
   the exact env/flags/camera/songMs recorded, and a `README.md` reviewer checklist.
2. Re-run all hard gates from a **fresh** agent build dir (`build-agent-W3.1b-verify`) — do NOT trust
   S1/S2 STATUS: drawlog-golden flag-OFF (888/888), lineup all layers, `milo-engine-tests` 198/0/2,
   DC3 zero-blast (`UniformStructs.h`+`standard_wgsl.inc` diff EMPTY, `static_assert 656`).
3. Confirm `classification.json` has valid JSON rows for `RB3_ENV_PROJLIGHT`, `RB3_ENV_FOG_FORCE`
   (+ `RB3_ENV_PROJLIGHT_FORCE` if added), `gen.inc` untouched (coordinator regens once at wave end).
4. Append the final STATUS section; commit docs (`flock /tmp/rb3-git.lock`, own files only).

## Exit criteria (W3.1b item)

1. **projLight landed** default-OFF (`RB3_ENV_PROJLIGHT`), scene fill + local `Projection` port +
   slot-3 gobo bind; flag-OFF byte-identical (drawlog 888/888, lineup PASS, bind slot-3 == `mWhiteView`).
2. **Fog visibly renders** in a reproducible screenshot (`RB3_ENV_FOG` + `RB3_ENV_FOG_FORCE`), with
   the scene∧material AND-gate fail-red demonstrated — closes W3.1a exit #2.
3. **DC3 zero-blast (hard):** `git diff <pre>..HEAD -- src/gfx/UniformStructs.h src/gfx/standard_wgsl.inc`
   EMPTY; `static_assert(sizeof(SceneUniforms)==656)` untouched; `milo-engine-tests` 198/0/2.
4. Both features **default-OFF**, all new flags registered append-only (no lane `gen.inc` regen).
5. Coordinator sign-off package in `venue-ab/`.

## Explicitly OUT of scope (deferred, per WAVE6_KICKOFF)

- **4→8 light-array growth** — the real 656→~1024 cross-backend `SceneUniforms` contract change
  (DC3 blast). Stays Wave 7+; W3.1b is struct-neutral only.
- **W3.2 BoxMapLighting** — Lane B, prototype-in-worktree-only this wave.
- Any edit to `UniformStructs.h`, `standard_wgsl.inc`, or rb3 `src/system/rndobj/Lit.cpp`/`Env.h`
  (decomp source). W3.1b is engine-`platform/`-only.

## Sequencing / collision notes

- **Single-writer:** W3.1b and the flip-blocker lane both edit `Rnd_Wgpu_RB3.cpp`
  `WriteSceneUniforms`. This item runs **after** the flip-blocker stages settle (Lane A tail). Re-grep
  the `:1450`/`:1460`/`:1469` anchors on the current engine HEAD before editing — the flip/re-golden
  may have shifted line numbers.
- Use an isolated build dir (`native/build-agent-W3.1b*`); never touch `build-native`/`build-web*`.
- Commit-per-review-cycle: S1 and S2 each commit as soon as their own gates pass (engine repo under
  `flock /tmp/milo-engine-git.lock`, own files only); do not batch to wave end.
