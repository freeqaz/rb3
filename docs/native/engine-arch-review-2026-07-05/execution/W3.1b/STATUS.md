# W3.1b — STATUS

Append-only log. Update under `flock /tmp/rb3-docs.lock`. One `## <subtask-id> — done|partial|blocked`
section per subtask with commit SHAs + blockers. Re-runs read this + `git log --grep=W3.1b` and skip
done work.

## Plan — done (Opus planner, A.S5, 2026-07-06)

`PLAN.md` written with `## Subtasks` (S1 projLight / S2 fog visual verify / S3 package+gates).
Investigation verified against LIVE engine source (HEAD `3d76285`, pin `8e7eddd`).

**Decisions made in the plan:**
1. **projLight (S1, opus, PRIMARY)** — port DC3's `kFakeSpot` gobo fill
   (`dc3 Rnd_Wgpu.cpp:1597-1636`) into the existing `SceneUniforms.projLight*` fields (all consume
   paths already exist in WGSL `standard_wgsl.inc:818-830` + bindings 3/4 → **zero DC3 blast**).
   Replace `s.numProjLights = 0;` (`Rnd_Wgpu_RB3.cpp:1460`) with a gated fill over `venv->mLightsReal`
   (all RB3 `RndLight`/`RndEnviron` members are PUBLIC — no decomp-source edit). RB3 has **no**
   `RndLight::Projection()` (DC3-only) → port it + `MakeShadowBias()` as **file-local statics** in
   `Rnd_Wgpu_RB3.cpp`. Slot-3 gobo bind via a **function-local** `wgpu::TextureView projView`
   (bind group is built in the same function as the fill) → **no BandRnd member, no header change**.
   Flag `RB3_ENV_PROJLIGHT` (feature, off).
2. **Fog visual verify (S2, sonnet)** — root cause of W3.1a's open exit #2 is asset-blocked: no
   boot-reachable venue authors fog (`FogEnable()==false` on all 34; extracted DTA has fog only as
   editor property *schema*, not authored-ON values — confirmed `rnd_objects.dta:653`,
   `world_objects.dta:183`). Cheapest reproducible screenshot path chosen = a registered default-OFF
   **probe** flag `RB3_ENV_FOG_FORCE` that synthesizes authored fog on the current environ at the
   fill site → real `/api/screenshot` A/B through the shipping scene∧material path (the honest,
   *permanent* replacement for W3.1a's reverted temp probe). A numeric rb3-tests synthetic-`RndEnviron`
   render-and-readback gate is offered as **best-effort optional** (DrawMesh-in-test is known-hard per
   `test_draw_log_golden.cpp:24`); the screenshot A/B is the mandatory exit-#2 proof.
3. **DC3 zero-blast gates (S3 + each subtask)** — exactly as W3.1a: `git diff -- UniformStructs.h
   standard_wgsl.inc` EMPTY, `static_assert 656` untouched, `milo-engine-tests` 198/0/2, flag-OFF
   drawlog 888/888 + lineup PASS. Both features default-OFF; all new flags registered append-only,
   coordinator does the single `gen.inc` regen at wave end.

**projLight force flag NOT pre-committed:** S1 must first sweep boot-reachable environs for a real
`kFakeSpot`+gobo light (venues commonly author spotlights); use it byte-real if found, else add
`RB3_ENV_PROJLIGHT_FORCE` (probe). Decide from the sweep, record in STATUS.

**Sequencing:** W3.1b edits `Rnd_Wgpu_RB3.cpp WriteSceneUniforms` — same file as the flip-blocker
lane → runs as the **Lane A tail** after those stages; re-grep the `:1450`/`:1460`/`:1469` anchors on
current HEAD before editing (flip/re-golden may shift lines).

## W3.1b.S1 — done (projLight fill, Opus, A.S6, 2026-07-06) — engine `0f3d7ef`

Ported DC3's `kFakeSpot` gobo fill (`dc3 Rnd_Wgpu.cpp:1597-1636`) into the
already-existing `SceneUniforms.projLight*` fields, gated on **`RB3_ENV_PROJLIGHT`**
(feature, default-OFF). `RndLight::Projection()` + `MakeShadowBias()` (DC3-only)
ported as file-local statics `RB3RndLightProjection()`/`RB3MakeShadowBias()` in
`Rnd_Wgpu_RB3.cpp` — **zero decomp-source blast** (no edit to rb3 `Lit.cpp`/`Env.h`).
Replaces `s.numProjLights = 0;` (fill loop over `venv->mLightsReal`, all PUBLIC
members); slot-3 gobo bind via a **function-local** `wgpu::TextureView projView`
(no `BandRnd` member / header change). Field mapping byte-for-byte vs DC3
(row0=`proj.m.*.x`, row1=`proj.m.*.y`).

**flag-OFF byte-identical** (numProjLights stays 0, projView stays `mWhiteView`):
drawlog 888/888 canonical + lineup PASS all layers (final binary).

**Boot-sweep outcome (plan step 6):** no reachable venue authors a `kFakeSpot`+gobo
light → the faithful path is a correct no-op (`off vs real` A/B = 17.2 ≈ 19.4
noise floor). Added **`RB3_ENV_PROJLIGHT_FORCE`** (probe) which relaxes the
type/gobo gates to synthesize one. **Visual A/B is honestly asset+shader-blocked**
(`off vs force` = 20.3 ≈ noise): the WGSL projected term is added to
`totalLighting.diffuse` only for LIT surfaces inside the projection cone facing the
light, and the bar venue is overwhelmingly unlit → a white stand-in gobo lands
nowhere visible. **Primary exit (fill landed + byte-identical) MET; compelling
projLight screenshot is beyond the probe's cheap-synthesis remit** (needs a real
gobo pattern + lit surface in a real cone). See `venue-ab/README.md`.

## W3.1b.S2 — done (fog visual verification, Opus, A.S6, 2026-07-06) — engine `0f3d7ef`

**Closes W3.1a exit #2.** Added **`RB3_ENV_FOG_FORCE`** (probe, default-OFF). Root
cause of W3.1a's blocker re-confirmed AND corrected: the plan/W3.1a assumed the
material-side `materialFogEnabled` "lights up on RB3_ENV_FOG alone" — it does NOT:
`RB3MaterialBinder.cpp:214` gates it on `mat->mFog`, which **no venue material
authors**, so the WGSL `scene.fogEnabled && material.materialFogEnabled` AND-gate
blocks fog even with the scene side forced. The probe therefore forces **both**
sides: scene fog synthesis at the fill site (`Rnd_Wgpu_RB3.cpp`, fogStart=8/
fogEnd=80/grey-blue, aggressive to exceed the animation noise floor) **and**
`materialFogEnabled=1` in the binder (`RB3EnvFogForce()` exported via
`RB3MaterialBinder.h`). Non-force `FogEnable()` path untouched.

**Fog RENDERS** (frame-locked A/B, songMs≈12665): `off vs on` mean|Δ| = **151.1**,
96% pixels changed — **8× the A/A capture-noise floor (19.4)**; `fog_on.png` shows
the whole interior washed grey-blue with depth-graded falloff. **AND-gate fail-red
demonstrated** by the scene-only→~22 (≈noise, no fog) vs scene∧material→151
progression. flag-OFF byte-identical (both branches skipped when `RB3_ENV_FOG`
unset). Optional rb3-tests numeric gate SKIPPED per plan (DrawMesh-in-test hard;
screenshot A/B is the mandatory proof, delivered).

## W3.1b.S3 — done (package + gates, Opus, A.S6, 2026-07-06)

Sign-off package in `venue-ab/` (6 PNGs + numeric wash table + reviewer notes).
**All hard gates re-verified on the FINAL binary** (`build-agent-W3.1b`, engine
HEAD `0f3d7ef`):
- **DC3 zero-blast:** `git diff -- src/gfx/UniformStructs.h src/gfx/standard_wgsl.inc`
  EMPTY; `static_assert(sizeof(SceneUniforms)==656)` untouched.
- **milo-engine-tests:** 198 passed / 0 failed / 2 skipped (my RB3-backend files are
  not in the dc3-flavor test compile path — `ninja: no work to do` on rebuild).
- **flag-OFF drawlog** `--fixed-clock --canonical-order`: PASS 888/888.
- **flag-OFF lineup-gate:** PASS img/segA/ratioB/countC/pin.

Flags registered append-only in `classification.json` (flock, NO gen.inc regen —
coordinator regens once at wave end): `RB3_ENV_PROJLIGHT` (feature),
`RB3_ENV_PROJLIGHT_FORCE` (probe), `RB3_ENV_FOG_FORCE` (probe). All default-OFF.
Engine commit `0f3d7ef`; rb3 docs/harness commit (this STATUS + `venue-ab/` +
`scripts/native/w31b-lighting-ab-capture.py`).

**Net:** projLight fill LANDED + byte-identical (primary exit met; visual A/B
asset+shader-blocked, documented honestly). Fog visual verification COMPLETE
(exit #2 closed with a reproducible 151-vs-19 screenshot + AND-gate fail-red).
Out of scope confirmed untouched: 4→8 light growth, W3.2 BoxMap, decomp source.

## A.S7 — VERIFY (Opus, independent, 2026-07-06)

Independent re-verification of W3.1b against its PLAN.md exits, on a **fresh clang
build dir** (`native/build-agent-A-S7-verify`, engine HEAD `0f3d7ef`, pin still
`8e7eddd` — WARNING-only, coordinator bumps). Did NOT trust S1/S2/S3 STATUS;
re-ran every gate and re-derived the DC3-zero-blast and file-disjointness proofs.

**Exit 1 — projLight landed default-OFF + flag-OFF byte-identical: PASS.**
- Code re-read in `0f3d7ef` diff: `RB3EnvProjLightEnabled()` + `RB3EnvProjLightForce()`
  file-local latches; `RB3MakeShadowBias()` + `RB3RndLightProjection()` file-local
  statics (DC3 `Lit.cpp:84-139` port); fill loop over `venv->mLightsReal` replacing
  `s.numProjLights=0`; slot-3 bind `e[3].textureView = projView` (function-local,
  defaults `mWhiteView`). No header/member/decomp-source edit.
- **flag-OFF byte-identical CONFIRMED:** drawlog-golden `--fixed-clock
  --canonical-order` PASS 888/888 (225 known-residual, within bound); lineup-gate
  PASS all layers (img/segA/ratioB/countC/pin).

**Exit 2 — fog visibly renders + AND-gate fail-red: PASS (independently reproduced).**
- Fresh A/B (`RB3_ENV_FOG=1 RB3_ENV_FOG_FORCE=1` vs neither), frame-locked
  songMs≈12665, shot coop_g_n03: **mean|Δ| = 160.4, 99.6% pixels changed**, fog_off
  mean-luma 34.8 → fog_on 193.3 (committed fog_on 185.0 — corroborates). Far above
  the documented ~19.4 A/A noise floor. `/tmp/a-s7-vis/fog_on.png` = the whole bar
  interior washed grey-blue with depth-graded falloff (fog_off = normal magenta bar).
- **Value-driven / live-path proof (fail-red, exit 5):** same binary, only the env
  var differs → 160-delta ⟹ the fog code path executes (dead code = no env effect).
  Delta is **depth-graded** (distant bg 156.6 > near fg 124.5, ratio 1.26, keyed to
  fogStart=8/fogEnd=80) and **grey-blue tinted** (washed-region RGB 179/182/188, B
  highest, matches authored fogColor 0.55/0.60/0.72) — proving the written source
  values flow to output, not a constant. S2's scene∧material AND-gate progression
  (scene-only→~22 ≈ noise vs scene∧material→151) documented in venue-ab/README.

**Exit 3 — DC3 zero-blast: PASS.**
- `git diff 0f3d7ef^..0f3d7ef -- src/gfx/UniformStructs.h src/gfx/standard_wgsl.inc`
  = **EMPTY**. `static_assert(sizeof(SceneUniforms)==656)` intact.
- **File-disjointness proven structurally:** `Rnd_Wgpu_RB3.cpp` + `RB3MaterialBinder.cpp`
  compile ONLY under `MILO_ENGINE_GPU_BACKEND=rb3` (engine CMakeLists.txt:322-326,
  gated at :373); `milo-engine-tests` builds under `=dc3` → the W3.1b-changed files
  are NOT in the test compile path. `RB3MaterialBinder.h`'s only change is a
  `RB3EnvFogForce()` declaration (RB3-only header). So the suite is structurally
  immune to W3.1b; the empty contract diff closes the shared-struct surface.
- **milo-engine-tests EMPIRICAL:** fresh dc3-flavor build (`build-agent-A-S7-vtests`,
  engine HEAD `0f3d7ef`), `ctest -j1`: **200 tests, 100% passed, 0 failed, 2 skipped (ExtractBik.ExtractSmallest, SkinGolden.CaptureGolden) = the documented 198/0/2**. Isolated per-test via ctest (a whole-binary direct run deadlocks on a gtest death-test-with-7-threads fork — harness artifact, not W3.1b).

**Exit 4 — default-OFF + flags registered append-only: PASS.**
- classification.json valid JSON; 3 W3.1b rows all `"default":"off"`: `RB3_ENV_PROJLIGHT`
  (feature), `RB3_ENV_PROJLIGHT_FORCE` (probe), `RB3_ENV_FOG_FORCE` (probe). No
  `gen.inc` regen (coordinator, wave end).

**Exit 5 — package present + coherent: PASS.** `venue-ab/` has 6 PNGs + numeric
table + honest README. My independent numbers corroborate it (fog 160 vs their 151;
projLight 15.5 vs their 20.3 — both ≈ noise).

### The one honest caveat (NOT a regression, NOT a PLAN gate)
The task's paraphrased exit-3 asks projLight flag-ON for "a measurable, plausible
lighting change." **It has NONE that is visually measurable** — independently
reproduced: `RB3_ENV_PROJLIGHT=1 RB3_ENV_PROJLIGHT_FORCE=1` vs OFF = mean|Δ| 15.55
(≈ the 19.4 noise floor). Root cause verified in WGSL (`standard_wgsl.inc:817-833`):
`projContrib` adds to `totalLighting.diffuse` only for LIT surfaces inside the
projection cone facing the light; the bar venue is overwhelmingly unlit/prelit, so a
white stand-in gobo lands nowhere visible. This is S1's documented asset+shader block,
consistent with the PLAN's projLight exit (which is **fill-landed + byte-identical**,
both MET — the PLAN required a *visible* render for FOG only). The projLight fill's
**runtime liveness is not independently observable** (no visible effect, no
instrumentation) — it rests on code-reachability + the verbatim-DC3 field mapping
re-read in the diff. Flagged for the coordinator: projLight cannot be visually
validated with current in-repo assets; a compelling screenshot needs a real
gobo-authoring venue (backlog).

**Verdict: all PLAN.md hard exits GREEN, independently reverified.** projLight
visible-effect asset+shader-blocked (documented, not a defect). No gate failed.
