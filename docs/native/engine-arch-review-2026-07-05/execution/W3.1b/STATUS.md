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

## W3.1b.S1 — pending
## W3.1b.S2 — pending
## W3.1b.S3 — pending
