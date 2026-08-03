# Refactor Execution — Wave Protocol

Coordinator: Fable. Implementation: ultracode workflows, one wave at a time. Parent plan:
`../REFACTOR_PLAN.md` (binding; Phase-0-first rule applies).

## Per-item artifact protocol (resume contract)

Each work item `<KEY>` (e.g. `W0.1`) owns `execution/<KEY>/`:

- **`PLAN.md`** — written by the item's Opus planner BEFORE implementation. Subtask breakdown
  (each tagged `model: opus|sonnet`), files touched, build/verify commands, exit criteria.
  If it already exists with a `## Subtasks` section, planners return it unchanged (resume).
- **`STATUS.md`** — append-only log updated by implementers/verifiers under
  `flock /tmp/rb3-docs.lock`. One `## <subtask-id> — done|partial|blocked` section per subtask
  with commit SHAs and blockers. Re-runs read this + `git log --grep=<KEY>` and skip done work.

## Hard rules (all agents)

1. A commit either **MOVES** code (behavior-preserving) or **CHANGES** behavior — never both.
2. Commit-early, commit-often; commit message starts with `<KEY>:`. Stage only your own files;
   never `git add -A`/`-a`. Engine commits go to `../milo-native-engine` (its own repo).
3. **Never bump `MILO_ENGINE_PIN`** — the coordinator bumps it once per wave.
4. Serialize git ops per repo: `flock /tmp/milo-engine-git.lock` / `flock /tmp/rb3-git.lock`
   around add+commit.
5. Use your own CMake build dir (`native/build-agent-<KEY>`); never touch `native/build-native`
   or `native/build-web*`; never run `scripts/web/build.sh`.
6. Do not run decomp (ninja) builds; if ever needed, only via `tools/ninja-locked`.
7. **NEVER run `git reset`, `git rebase`, `git checkout --`, or `git restore` on the shared
   working trees** (rb3 or engine). Wave 1 lost commits to concurrent `git reset` events
   (recovered by the W0.6 verifier — see `W0.6/STATUS.md`). If your index/tree looks wrong,
   STOP, record it in STATUS.md, and leave recovery to the coordinator.
8. Never revert or "fix up" a line that belongs to a sibling lane's commit — flag it in
   STATUS.md instead.

## Waves

- **Wave 1 (2026-07-05):** W0.1 skin golden · W0.2 loud stubs · W0.3 draw-log golden ·
  W0.4 bone live-pose · W0.5 non-blind lineup gate · W0.6 flag-registry skeleton ·
  W1.1 WGSL externalization. Lane chaining: W1.1 → W0.3 (both edit `Rnd_Wgpu_RB3.cpp`).
- Wave results are appended below by the coordinator after each workflow completes.

## Pre-dispatch review gate (standing rule, from Wave 3 on)

Before dispatching each wave's workflow, the coordinator writes a `WAVE<N>_KICKOFF.md` (lane
structure, item briefs, exit gates, explicit risk questions) and **kicks off a Fable subagent to
review it** (`WAVE<N>_REVIEW.md`) — an independent adversarial pass on sequencing, gate validity,
concurrent-edit collisions, and mis-parallelization, grounded in file:line. The coordinator adopts
the amendments (recording acceptance on the kickoff) and only then dispatches. Rationale: the Wave-3
review caught a mechanically-wrong gate premise (skin/effector goldens run in the DC3-context suite
and never exercise the rb3 band-rebind path), a file collision (W3.1 ⟂ W1.6 at `WriteSceneUniforms`),
and a refuted fallback (`setarch -R`) — before a ~5-hour fleet ran on the wrong plan.

## Wave 1 results (2026-07-05, run `wf_386f9206-c04`, 35 agents)

**6/7 complete with gates green + fail-red proven; W0.3 partial.** Engine advanced
`a8089c3` → `9561a19` (pin bumped by coordinator).

| Item | Status | Highlights |
|---|---|---|
| W0.1 skin golden | ✅ complete | `tests/test_skin_golden.cpp` (engine `d8d2127`+`09c9fab`): LP64 `RefSkinVertex` ≡ compiled `RndMesh::SkinVertex` (<1e-4); 10-vert golden incl. 6 hand/finger verts; fail-red via `MILO_SKIN_GOLDEN_BREAK=1` (25.5u hand fling → exit 1). |
| W0.2 loud stubs | ✅ complete | `__hmx_stub_hit` once-latch shim + `band3_stub_registry.tsv` classification + census gtest, extended to dta/rndobj_synth (`1f8057f9`…`417d1b62`). Fail-red: demoting an ok-noop row → `StubCensus.NoAssertUnreachableHits` RED. Gap (backlog): per-row completeness gtest is band3-only. |
| W0.3 draw-log golden | ⚠️ partial | Ring + JSON dump landed (engine `9561a19`), comparator gtest (rb3 `1242531c`), `/api/drawlog` + capture script (`1dc8d95d`). Both fail-red demos proven (co-location + bind-group collapse). **Blocker:** integration golden not stable — splash boot is wall-clock-driven (draw count 885–888 across boots). Needs a trace-free frozen-sim-clock seam (extend `RB3_REPLAY_FIXED_CLOCK` beyond `RB3ReplayActive()`), then re-golden. → **W0.3b in Wave 2.** |
| W0.4 bone live-pose | ✅ complete | `ClipPoseFixture.EffectorWorldPositionsMatchGolden` (engine `669ebc3`+`7a490f2`): crouching_great_01 @ fracs {0,.5,.9}, hand+toe+prop effector world positions; fail-red via `MILO_TEST_POSE_PERTURB=1.0`. |
| W0.5 lineup gate | ✅ complete | `patch-lineup-capture.py` + `lineup-gate.py` + `lineup_bbox_metrics.py` + golden (9 rb3 commits): img/segA/ratioB/countC/pin layers. Fail-red proven: broken-skin run (`RB3_NO_SKEL_REBIND=1 SHARD_GUARD_OFF=1 …`) → new gate FAIL on ratioB while OLD gate + image layer PASS — the exact blindness fixed. |
| W0.6 flag registry | ✅ complete | Engine `NativeCompatFlags` registry (`21eae3d`) + classification sidecar + `scripts/analysis/native_compat_census.py` + generated `NATIVE_COMPAT_LEDGER.md` burn-down doc + 5 demo rewires. Verifier also **recovered commits lost to concurrent `git reset`** (rb3 `6c8a3bbf`) → hard rule 7 added. |
| W1.1 WGSL externalize | ✅ complete | 5 shaders → `src/gfx/Shaders/*.wgsl.inc`, one MOVE commit each (engine `6602bc0`…`bd7c2f5`), zero-line diffs re-derived by verifier; wgpu validation gtest (real Dawn) + bad-shader fail-red. `grep R"WGSL(` on `Rnd_Wgpu_RB3.cpp` → 0. |

**Known debris / backlog from Wave 1:**
- 29 pre-existing `milo-engine-tests` failures from dc3-decomp drift (4 buckets: XMA sidecar
  namespace bug, `CharBones::ScaleAdd` retarget assert, compressed-skin numeric drift, Dir-merge
  bug) — characterized in `W0.1/STATUS.md`; none touch Wave-1 files. → **W2-TESTFIX lane.**
- rb3 `1242531c` carries one W0.2 line (`rb3-dta` linking `rb3_stub_census.cpp`) — harmless,
  documented, not unwound (rule 8).
- Engine working tree has an unrelated concurrent agent's uncommitted `FxSendNative.cpp` audio
  edit — left untouched.

## Wave 2 results (2026-07-06, run `wf_ffcf5a20-bb5`, 41 agents)

**6/7 complete with gates green; W0.3b partial; W1.6 auto-deferred (precondition unmet).** Engine
advanced `9561a19` → `41b9e3a` (pin bumped after coordinator build + lineup-gate PASS against the
fully-decomposed engine). **`Rnd_Wgpu_RB3.cpp`: 7,017 → 4,747 lines** with six extracted TUs.

| Item | Status | Highlights |
|---|---|---|
| W1.2 mesh cache | ✅ complete | `RB3MeshCache.{h,cpp}` (engine `daa0286`/`daf0ed1`/`6f9d340`): entry cache + Xbox-cvert unpack extracted, MOVE byte-identical (source-textual). **S4 convergence analysis: NOT provably identical to `gfx/MeshGpuCache`, kept verbatim** — correct conservative call. |
| W1.3 material binder | ✅ complete | `RB3MaterialBinder.{h,cpp}` (engine `c43b6fd`/`b206d44`): `RB3BuildMaterialUniforms` extracted; asset-name branches moved along unchanged for W1.7 to relocate. |
| W1.4 postproc/halo/quad | ✅ complete | `RB3HaloPass`+`RB3PostProc`+`RB3Quad` TUs (engine `3c59dc4`…`8d6d895`): gem-bloom capture/replay semantics preserved; diff-hunk equivalence re-derived by verifier. |
| W1.5 ring dedupe | ✅ complete | Shared `gfx/UniformRingBuffer` adopted by both backends (engine `0cd227f`/`648dc40`). One CHANGE (overflow wrap→grow) proven inert: **zero-Grow probe** shows the divergent path never executes at real scenes + path-identity for the rest. |
| W1.7 GameRenderHook | ✅ complete | Seam wired + **all 13 asset-name behavior branches (B1–B13) + Bucket-A probes relocated** to `rb3_render_hook.cpp`, one MOVE per commit behind existing flags (engine+rb3, `9083833`…`41b9e3a`). Engine `strcmp` survivors are only Bucket-C cam/environ selectors (Phase-3-owned, coordinator-signed-off) + generic env-driven. **SYS-2 structurally addressed.** |
| W2-TESTFIX drift | ✅ complete | `milo-engine-tests` **169/29-failed → 198/0-failed**. 4 buckets fixed at their owning repo (dc3 `034e8d12`/`a14f7e01`, engine `49c3f38`/`0dab386`). Two test-oracle corrections audited legit (float-cast serializer bug proven; Dir-merge oracle corrected against faithful `Utl.cpp:247`, one assertion *strengthened*). Suite is now a wave gate: **198 pass / 0 fail / 2 by-design skip** (requires `DC3_DATA`+`MILO_LIB`, `ctest -j1`). |
| W0.3b clock seam | ⚠️ **partial** | Frozen-clock seam landed (`RB3_FIXED_CLOCK`, rb3 `352d19ef`/`0026bee0`); **draw COUNT determinism exact (888×) — count-jitter root cause = wall-clock boot RNG seed, fixed.** But the lane verifier's **15-run sweep found ~33% flake** on the world-transform gate: run-to-run **draw-submission ORDER nondeterminism** (mesh-identity swaps, up to 354-draw divergence; byte-identical binaries disagree run-to-run). NOT a residual-sidecar candidate. → **new engine item W0.3c in Wave 3.** `--fixed-clock` gate is diagnostic-only until fixed, NOT a hard CI blocker. |
| W1.6 DrawContext | ⏸️ **deferred** | Workflow barrier correctly skipped it: precondition (W0.3b green) unmet. The riskiest item must not land on a probabilistic gate. → Wave 3 Lane A, after W0.3c. |

**Coordinator actions:** clean rb3-native build + lineup-gate PASS against decomposed engine HEAD
`41b9e3a`; pin bumped; W1.7 Bucket-C scoping signed off in `W1.7/STATUS.md`.

**Key finding for the campaign:** the W0.3b draw-order nondeterminism is not just a test-gate
annoyance — it is *evidence of the SYS-3 order-dependent-state fault the review predicted*, surfacing
under the microscope of a deterministic harness. Fixing it (W0.3c) both unblocks W1.6 and removes a
real source of render instability. The lane verifier's refusal to paper it into the residual sidecar
(it's mesh-identity swaps, a different and real bug class) is exactly the discipline the safety nets
were built to enforce.

## Wave 3 (2026-07-06, run `<pending>`)

- **Lane A (DrawMesh chain, sequential):** W0.3c (root-cause + fix draw-submission-order
  nondeterminism → W0.3b golden green) → W1.6 (DrawContext state-leak fix, now gated by the green
  golden). Both engine-backend, both edit `Rnd_Wgpu_RB3.cpp`.
- **Lane B (skinning bind — the hands/fingers headline fix, load path, parallel):** W2.2 (rebind
  outfit+appendage meshes to the per-member animated skeleton **and rebake `invBind` against the
  per-member bind pose**). Gated by the *deterministic* W0.1 skin golden + W0.4 effector golden —
  does NOT depend on the flaky draw-log. Lives in the char/skeleton load path, so no DrawMesh
  collision with Lane A. **Ground-truth requirement:** must establish independent correctness
  (Dolphin/retail) — the skin golden proves faithful-to-reference, not that a changed bind is *right*.
- **Lane C (cheap independent wins, parallel):** W3.1 (lighting fills: fog from `RndEnviron` +
  directional/point arrays 4→8, additive) + W2.5 (band-waypoint resolution assert, game-side).

Deferred to Wave 4: W2.1 placement contract + W2.3 GeomOwner aliasing (need W1.6's DrawContext),
W3.2 BoxMapLighting, W2.4 BandPatchMesh decision, Phase 4 UI.

## Wave 3 results (2026-07-06, run `wf_e0e4d32e-e39`, 22 agents)

**W1.6 + W2.2 + W2.5 complete; W0.3c partial (Exit-B comparator delivered, "15/15 green" bar blocked
by pre-existing eye-jitter eps, not the order axis).** Engine `41b9e3a` → `6221a56` (pin bumped
after fresh from-scratch build + lineup-gate PASS + canonical fail-red confirmed).

| Item | Status | Highlights |
|---|---|---|
| W0.3c determinism | ⚠️ **partial [Exit B]** | S1 diagnosis reshaped the item: the planned transparent-sort fix (Exit A) is **structurally absent from the rb3 binary** (`TransparentQueue.cpp` is DC3-only; `BandRnd::DrawMesh` submits in traversal order) → Exit A NO-GO, S2 correctly skipped. **Root cause = async-loader/worker COMPLETION-order feeding object-list insertion order** (16-run sweep: single invariant draw *multiset*, order-only divergence, 282/282 meshes keep stable heap addresses → not allocation nondeterminism). Exit-B **canonical-order (multiset) comparator** landed in `drawlog-golden.py` (rb3 `5d254e00`) — all 4 fail-red classes RED + permutation GREEN. **But** the "15/15 fresh-boot green" bar is blocked by the pre-existing **W0.3b CharEyes/CharLookAt eps=3.0 residual** occasionally grazing (max 3.99): the verifier's 24-run sweep was 1 PASS/23 FAIL, all on residual-name world-xfm, none on count/bind-group/mesh-identity. → **new item W0.3d (Wave 4).** |
| W1.6 DrawContext | ✅ **complete** | **The SYS-3 state-leak fix.** `WriteSceneUniforms` now returns an immutable `RB3SceneBinding`; `mSceneBindGroup`/`mSceneOffset` mutable mirrors collapsed into `mActiveScene`; `RB3DrawContext` + `SubmitDraw` thread the scene binding explicitly per draw (engine `9df8349`/`01c2642`/`6221a56`). Verifier built a **fresh pre-W1.6 baseline worktree** and did a 15-run A/B: **888/888 draws every run, scene bind-group-token partition identical (20 distinct), count + bind-group-collapse + multiset-key all identical.** The only A/B divergences are field=world on eye-jitter meshes — which W1.6 *mechanically cannot alter* (proven by A/A controls) = the W0.3d residual, not W1.6. Byte-identical modulo pre-existing nondeterminism. |
| W2.2 hands/fingers | ✅ **complete** | **The hands/fingers bind fix, staged so a blind revert is impossible.** The head/hands rest-capture rebind (`RebindHeadHandsAtRest`, `BandCharacter.cpp:522`) is **already default-ON** (opt-out `RB3_NO_HEAD_REBIND`) — a proven net win (rebake-OFF head guard-DROPs at 9.59×). New `rb3-tests` bind-pose identity oracle (`test_hands_bind_oracle.cpp`): at bind pose `offset′·perMemberBoneBindWorld ≈ identity` + skinned≈authored verts, **fail-red proven** (perturb 0.15 → 28.87u fingertip smear ≈ 200·sin0.15). An additional experimental branch behind `RB3_HANDS_BIND_FIX` (default-OFF, `BandCharacter.cpp:1385`) **measured no benefit → correctly NOT flipped** (S3 + verifier both recommend no-flip). Numeric: **FLING(>120u)=0, max SKINPOS 68.2u (< 92u tripwire), no 200-460u smear**; the ~69u head graze is **adjudicated STRUCTURAL** (identical flag-OFF/ON; non-rebound crowd bodies show the same 63-64u extent). **NOTE (corrected 2026-07-06 after Wave-4 Fable review):** `RB3_SKEL_REBIND_FULL` is NOT W2.2's flag — it is the *known-broken* full-body rebind used as W0.1's fail-red control. The real open residual is the **foot/shoe lower-body path** (`saddleshoe_skin.2` 4.73× guard-DROP, via `RebindOutfitBonesToOwnSkeleton`, not the hands path) → **W2.6 (Wave 4).** Coordinator handoff outstanding: register `RB3_HANDS_BIND_FIX` in `classification.json` (the census does not scan `rb3/src/system/`, so it went undetected — a coverage gap). |
| W2.5 waypoint assert | ✅ complete | `HX_NATIVE`-guarded diagnostic in `BandConfiguration::SyncPlayMode` (rb3 `082f933d`): `MILO_WARN` on any unresolved non-empty waypoint `targName`. Fail-red proven (injected bogus targName → warns); Wii compile provably untouched (`HX_NATIVE` undefined in the MWCC build). Surfaces "only some members placed." |

**Coordinator actions:** fresh from-scratch rb3-native build against post-W1.6 engine HEAD `6221a56`
(23 `RB3DrawContext`/`RB3SceneBinding` refs confirm W1.6 linked) + lineup-gate PASS all layers +
canonical comparator fail-red confirmed; pin bumped.

**Two headline structural wins this wave:** (1) **SYS-3 is fixed** — the mutable mid-frame scene
bind group that made rendering order-dependent global state is gone; draws now carry their scene
binding explicitly. (2) **The hands/fingers fix exists, is numerically gated, and is one
coordinator-signed flag-flip away from shipping** — with the exact bind-pose-identity oracle whose
absence caused the two BandPatchMesh reverts. Neither could have landed safely without Waves 0–2.

## Wave 4 results (2026-07-06, run `wf_c7f69c01-0ea`, 19 agents)

**W2.1 placement contract LANDED default-OFF (the crowd/drum fix); W2.3 refuted (no-op); W0.3d gate
cleaned; W2.6 PART 1 diagnosed, PART 2 flag-registry incomplete (API overload).** Engine `6221a56`
→ `609efb7` (pin bumped after lineup PASS + independent oracle A/B). One W2.6 subtask died on API
overload.

| Item | Status | Highlights |
|---|---|---|
| W2.1 placement contract | ✅ **complete [default-OFF]** | **The crowd/drum-kit-at-one-point fix — SYS-1 placement half.** Landed behind `RB3_PLACEMENT_CONTRACT` (engine `6852caa`) as a **provably vertex-invariant reorganization**: `obj.world = mesh->WorldXfm()` AND bind-relative palette (`skin·inverse(meshWorld)`), so `obj.world·(skin·meshWorld⁻¹)·v = skin·v` byte-for-byte (worst element diff **0.0000**) while `obj.world` now records real placement. **Gate-first (S1):** placement oracle asserting each crowd instance's drawn `obj.world` == the faithful `spXfm` it's posed with (`Crowd.cpp:403`) — **fail-red free** (RED on default build: all 231 skinned draws at origin). flag-OFF byte-identical (canonical 888, lineup PASS, 198/0/2, census 0). flag-ON: oracle GREEN (29 distinct crowd worlds spanning 316.6 ≈ posed 319). **Coordinator independently reproduced both:** flag-OFF oracle RED, flag-ON oracle GREEN. B2 A/A discipline correctly applied (A/A = 7 eye-flake meshes; flag-ON adds exactly 1 new skinned = crowd `0xc57f` spreading). **NOT flipped** (correct): the name-scoped placement hacks (`RB3_NO_HUB_BAR_PLACEMENT_FIX` etc.) are **not yet proven no-ops** under the contract, so flipping would double-run them → **W2.1-flip (Wave 5)** resolves subsumption + drum oracle + Dolphin A/B. |
| W2.3 GeomOwner aliasing | ✅ **complete [honest-refutation]** | The hypothesis was **refuted by measurement**: crowd meshes are ALREADY self-owned (owner==mesh, own-extent==owner-extent, all ≫12u), so "read the drawn mesh's own bones" is a **no-op**. `RebindCrowdCharBonesToOwnSkeleton` is **retained** (A1/R5) and proven load-bearing (~24× shard-drop when disabled — fail-red confirmed). No behavior change, no flag. Crowd placement is fully handled by W2.1's contract + the existing rebind. Negative controls green (imposter path byte-identical, SKIN_CLAMP unchanged). |
| W0.3d gate cleanup | ✅ **complete** | Froze CharEyes/CharLookAt RNG under `RB3_FIXED_CLOCK` (rb3 `c6b961da`, additions-only, HX_NATIVE-guarded). Exit met via the plan's **sanctioned per-name-eps fallback** (not the empty-sidecar ideal — a disclosed ~20u non-RNG residual survives on the same 7 eye/face meshes after the RNG freeze; per-name eps recalibrated from an **N=36** boot sweep, no global widening). fail-red intact. **Part (b)** async-loader/worker completion-order = **diagnosis-only, staged not landed** (F1 — its fix touches Lane-A files) → **W0.3d-fix (coordinator-sequenced).** |
| W2.6 foot/shoe + registry | ⚠️ **partial** | **PART 1 (foot/shoe):** diagnosed — `saddleshoe_skin.2` guard-DROP is **lineup-dependent (0.8×–3.99×)**, max SKINPOS 69.5u, 0 fling, nothing in the 200-460u tripwire band; **no source fix landed** (characterization shows it's within the structural envelope, not a clean rest-capture target). **PART 2 (flag-registry) PARTIAL** — S4 died on API overload, but the **scanner extension DID land** as a follow-up fix `a537c2a3` ("W2.6: land the S4 census scan-root extension"), so `native_compat_census.py` now **does** scan `rb3/src/system/` and `census check` exits 0 at 318 flags legitimately. What's left: the ~89 surfaced game-code flags (incl. `RB3_HANDS_BIND_FIX`/`RB3_SKEL_REBIND_FULL`) are registered but sit as `FlagClass::Unknown` in `gen.inc` — they need **classifying**, not registering. → **W0.6b (Wave 5) = classification, not scanner work** (corrected 2026-07-06 after Wave-5 Fable review). |

**Coordinator actions:** lineup PASS + independent placement-oracle A/B (RED off / GREEN on) against
engine HEAD `609efb7`; pin bumped; committed the accumulated planner `PLAN.md` + `STATUS.md`
artifacts that had never been git-tracked across all waves (docs-durability hygiene).

**Headline:** the **crowd-at-one-point / drum-kit-at-one-point root cause (SYS-1 placement) is
fixed** — as a vertex-invariant change proven correct by an oracle, sitting one deliberate flag-flip
from shipping. Combined with W2.2 (hands/fingers) and W1.6 (SYS-3), the three worst mesh bug families
from the original review now have landed fixes behind flags.

## Wave 5 results (2026-07-06, run `wt2tbkfja`, 20 agents)

**W0.3d-fix + W0.6b complete; W2.1-flip ready-for-flip; W3.1a partial. Flip HELD by coordinator on
visual sign-off (see below).** Engine `609efb7` → `a4bde9f` (pin bumped after lineup PASS +
default-OFF confirmed + single classification regen). One W3.1a subtask died on API overload.

| Item | Status | Highlights |
|---|---|---|
| W0.3d-fix determinism | ✅ **complete** | Deterministic material-name tie-break in `SortDraws` (rb3 `Utl.cpp`, `76f51077`), gated `RB3FixedClockActive() && !RB3DrawSortDeterministicOff()`, additions-only under HX_NATIVE (Wii byte-identical). **Fail-red proven under max-entropy (async+ASLR on): default = 12/12 identical submission order, opt-out = 12/12 distinct** (pre-fix pointer flake restored). Draw order is now deterministic — the re-golden prerequisite. Minor caveat: not 100% airtight under literal 32-core zero-idle contention. |
| W2.1-flip readiness | ✅ **complete [ready-for-flip]** | Flip staged as a one-line `kPlacementContractDefaultOn 0→1` (`Rnd_Wgpu_RB3.cpp:2909`) with opt-out-first `RB3_PLACEMENT_CONTRACT_OFF` (`dbf2758`); drum oracle implemented (`kind=="drum"`, fail-red); UI A/B (main_hub + song_select); **Dolphin A/B package produced** (`W2.1-flip/dolphin-ab/`, A/A pairs). Default still OFF; oracle GREEN companion (crowd + drum). **→ coordinator sign-off gate (E1).** |
| W3.1a fog fill | ⚠️ **partial** | Fog fill from `RndEnviron` into the existing `SceneUniforms` fog fields (engine `a4bde9f`, default-OFF `RB3_ENV_FOG`). **DC3 zero-blast confirmed** (`UniformStructs.h`+`standard_wgsl.inc` diff EMPTY, `static_assert 656` untouched), flag-OFF byte-identical, `milo-engine-tests` 198/0/2. **But Exit-#2 unverifiable — asset-blocked:** all 34 boot-reachable venue environs have `FogEnable()==false`, so fog can't be shown rendering. `projLight` (S2) not landed. → **W3.1b (Wave 6):** projLight + a fog-authoring venue (or synthetic env) to verify render. |
| W0.6b flag classify | ✅ **complete** | **91 game-root flags classified** (probe/workaround/feature/perf, engine `1fd2bfc`), append-only, verified zero game-root Unknown rows remain. Coordinator ran the **single deferred regen** (census now clean at 321 flags). Coverage gap closed. |

### ⚠️ COORDINATOR FLIP DECISION: HELD (2026-07-06)

**I reviewed the Dolphin A/B package and did NOT flip `RB3_PLACEMENT_CONTRACT`.** The placement fix
is *numerically* proven correct (crowd + drum oracle GREEN) and safe (flag-OFF byte-identical), but
the **visual E1 gate did not cleanly pass**:

- In both comparison montages (`layout_crowd_vs_dolphin.png`, `layout_drum_vs_retail.png`), the
  flag-ON A/A pair is **inconsistent**: `cap_ON_1` blows out nearly **fully white** while `cap_ON_2`,
  `cap_OFF_1`, `cap_OFF_2` all render normally (band members + venue visible). In this sample the
  severe blow-out appears **only in flag-ON** (1/2), never in flag-OFF (0/2).
- The package's **own reviewer checklist (item 3)** says an exposure wash appearing in only one flag
  state is "a new finding, not expected" → do not sign off.
- W2.1.S2's own STATUS notes the exact failure mode: *"clamped crowd bones fly to meshWorld·v …
  whose emissive geometry feeds the bloom pass into a full-screen wash."* The asymmetric blow-out is
  consistent with a **residual of that crowd-emissive→bloom interaction under flag-ON** — i.e. the
  flip may not be exposure-neutral even though it is vertex-invariant (vertex positions don't change,
  but *where* emissive crowd geometry draws does, which can feed the bloom pass differently).

Holding the flip is the disciplined call — this campaign's safety rests on the visual gate catching
what "looks fine" misses. **The flip is deferred until the flag-ON bloom blow-out is characterized
and resolved (→ W2.1-flip-blocker, Wave 6).** The fix stays landed + proven behind the flag; nothing
is lost.

**Coordinator actions:** single classification regen (census clean, 321); build + lineup PASS +
default-OFF oracle-RED confirmed against `a4bde9f`; pin bumped; flip HELD.

### New backlog items filed from Wave 5

- **W2.1-flip-blocker (Wave 6, gates the flip):** characterize the flag-ON bloom blow-out — capture
  N≥8 samples per flag state to establish whether it is A/A-variable (pre-existing, flip-independent)
  or flag-ON-specific (crowd-emissive→bloom, per W2.1.S2). If flag-ON-specific, fix the
  emissive-feeds-bloom path (likely the same `IsHaloSourceMat`/bloom-capture path from the
  A2/A3/A4 glow work). Then re-capture a clean Dolphin A/B → coordinator flip + re-golden (888→792).
- **W3.1b (Wave 6):** land `projLight` from environ fakespots; add a fog-authoring venue (or synthetic
  RndEnviron) to visually verify `RB3_ENV_FOG` renders. Then the 4→8 light-array change (the Wave-6
  DC3-blast-radius lighting item) + W3.2 BoxMapLighting.

### New backlog items filed from Wave 4

- **W2.1-flip (Wave 5):** prove the name-scoped placement hacks (`RB3_NO_HUB_BAR_PLACEMENT_FIX`,
  `RB3_SCROLLBAR_THUMB_FIX_OFF`, `RB3_NO_CROWD_REBIND`) are subsumed by the contract (or reconcile
  them), add the drum-specific bone/waypoint oracle assertion, capture reviewer-judged Dolphin
  gameplay A/B, then flip `RB3_PLACEMENT_CONTRACT` default-ON in a one-line commit. **This ships the
  crowd/drum fix.**
- **W0.6b (Wave 5):** the scanner extension already landed (`a537c2a3`); the remaining work is to
  **classify** the ~89 game-code flags that now sit as `FlagClass::Unknown` in `gen.inc` (incl.
  `RB3_HANDS_BIND_FIX`/`RB3_SKEL_REBIND_FULL`) into `probe|workaround|feature|perf`. Exit: zero
  game-root Unknown rows.
- **W0.3d-fix (coordinator-sequenced):** the staged async-loader/worker completion-order determinism
  patch from W0.3d part (b) — apply after Lane-A DrawMesh work settles (it touches the same path).

### New backlog items filed from Wave 3

- **W0.3d (Wave 4):** make the draw-log gate a clean non-probabilistic pass. Two parts: (a)
  recalibrate the CharEyes/CharLookAt residual eps from a large sample OR root-cause the eye-jitter
  (freeze/zero look-at state under `RB3_FIXED_CLOCK`) so the residual sidecar shrinks toward empty;
  (b) root-cause the mechanism-2 async-loader/worker completion-order nondeterminism (the actual
  draw-order flake). Until then the canonical comparator is usable **for changes that cannot alter a
  mesh world-xfm** (as W1.6's verifier correctly applied it) but is not yet a universal hard gate.
- **W2.6 (Wave 4), replaces the mistaken "W2.2-flip":** the Wave-4 Fable review established there is
  **no beneficial flip to ship** — `RebindHeadHandsAtRest` is already default-ON and the head graze is
  structural. The real residual is **foot/shoe rest-capture coverage** (`saddleshoe_skin.2` 4.73×
  guard-DROP through `RebindOutfitBonesToOwnSkeleton`). W2.6 extends the load-time rest capture to the
  lower-body/outfit meshes, default-OFF, inheriting W2.2's four-layer anti-revert gates. Plus a
  flag-registry cleanup: register `RB3_HANDS_BIND_FIX`/`RB3_SKEL_REBIND_FULL` and extend the census to
  scan `rb3/src/system/` (game-code flags are currently uncovered).

## Wave 6 results (2026-07-06, run `wf_9bfc818f-c39`, 16 agents + 3 side agents)

**THE CROWD/DRUM PLACEMENT FIX SHIPPED: `RB3_PLACEMENT_CONTRACT` flipped default-ON (engine
`fced18b`) after the Wave-5 wash hold was refuted by measurement.** Also: black singer head fixed
default-ON, main-hub grey quad hidden behind a flag, W3.1b lighting tail complete, BoxMap prototype
partially refuted its own design, grayscale-venue + menu-text + finger-shard all root-caused.
Engine `8e7eddd` → (pin bump pending re-golden). Kickoff+review: `caf28a2b` (8 amendments adopted;
key catch: the bloom-halo suspect was mechanically impossible — game.cam-gated vs venue-cam A/B).

| Item | Status | Highlights |
|---|---|---|
| W2.1-flip-blocker | ✅ **complete → FLIP EXECUTED** | S1 built `wash_score.py` (luma both-tails + pink-hue, selftest green, validated exactly against Wave-5 luma); batch-0 already found a PINK wash flag-OFF. S2 songMs-pinned (21000±250) interleaved measurement, n=7/state: **VERDICT A/A-variable** — wash flag-OFF 2/7 vs flag-ON 4/7 (Fisher p≈0.59), luma Mann-Whitney U=24.0 **p=1.0**, wash = full-frame magenta env cast a crowd-only transform cannot produce, class flips PINK↔NEARBLACK across boots at equal songMs. **The Wave-5 "wash only flag-ON" premise is refuted under time control.** S3 filed the wash as its own backlog item (ranked prior: async asset residency > RB3PostProc grade > P4 venue-light; needs N≥6/config matrix) + A5 pre-flip checks: re-measured post-flip count **792** (3/3), OFF-arm inversion sweep (3 harnesses), classification row drafts. S4 fresh sign-off package on detector-selected wash-free frames, oracle GREEN companion. **Coordinator E1: SIGNED OFF + flipped `kPlacementContractDefaultOn` 0→1 (`fced18b`); default build oracle GREEN / opt-out RED reproduced; harness OFF-arms inverted to `RB3_PLACEMENT_CONTRACT_OFF=1`.** Caveat recorded: the Wave-6 montage's small-venue shot is weakly informative for crowd-spread; sign-off leans on oracle + Wave-5 layout judgment + the wash refutation. |
| W3.1b projLight + fog verify | ✅ **complete** | projLight kFakeSpot gobo fill landed default-OFF `RB3_ENV_PROJLIGHT` (+`_FORCE` probe), byte-identical flag-OFF; **fog visual verification CLOSED** — `RB3_ENV_FOG_FORCE` probe (no in-repo asset authors fog) renders visible grey-blue depth-graded fog (scene+material AND-gate 151 vs 19 noise). projLight *visible-effect* honestly asset+shader-blocked (no venue authors kFakeSpot+gobo; term only lights lit surfaces in cone) — documented, not a defect. A.S7 independent verify: all PLAN exits GREEN on fresh build (engine `0f3d7ef`), DC3 zero-blast, milo-engine-tests 198/0/2. |
| W3.2 BoxMap prototype | ✅ **complete [design partially REFUTED]** | `RB3_BOX_AMBIENT` 6-axis box-ambient cube built + gated in worktree branch `wave6-boxmap-proto` (flag-OFF byte-identical, lineup PASS both states). **Refutation on real data: boot-reachable venues have 27 point + 1 fakespot + ZERO directional approx lights** → the per-environ directional-cube is a near-no-op; the genuine SYS-4 fidelity gap is **point-lights** (Wii per-object cube vs our per-pixel Lambert). → Wave-7 **escalate-or-drop** decision. |
| W4.1 UI parity | ✅ **complete** | (a) main_hub grey quad = REAL, root-caused (`playnow.lsw` LabelShrinkWrapper in 360-ARK `main_hub.milo`, opaque-grey on native / hidden on Wii — SYS-5); fix landed default-OFF `RB3_HUB_MENU_QUAD_HIDE` (`b537d275`). (b) song_select overlap = **largely already fixed**; residual red sliver adjudicated a legitimate scrollbar thumb. (c) part_difficulty = **NOT A BUG** — settle-frame recapture (frames 366-734) shows widgets render correctly; the coordinator's frame-390 screenshot was a mid `part:guitar` camera zoom; black poster quads = venue backdrop → backlog handoff. Lane verify PASS, fence respected. |
| W2.7 black head (Lane D) | ✅ **complete [default-ON]** | Singer flat-black head ROOT-CAUSED + FIXED in game code (`837808e1`): `<gender>_head_diff.tex` + `head_naked.mat` live in a nested head subdir unreachable by the non-recursive `dir1->Find` (Wii milo flat-merges); head has no `dummy_*` fallback (unlike torso/feet) → null diffuse → flat lit color (black in dim venues / pink in warm). Fix = recursive head-detail-texture bind, head-only, null-only, HX_NATIVE, Wii byte-identical, **default-ON** opt-out `RB3_BLACK_HEAD_FIX_OFF`. |
| W3.3 grayscale venue (Lane D) | ✅ **complete [diagnosis; fix staged Wave 7]** | Native-only **postproc-composite over-exposure of the authored song-start stage-light reveal** (songMs ~2000-6000, self-corrects ~9000): composite runs the reveal hotter than Wii GX; the per-channel Reinhard ceiling guard desaturates hot pink → grey wash. NOT authored B&W, NOT P4 grey-fallback, NOT uninit tonemap (3-way flag matrix at ms3000: default=grey, RB3_PP_OFF=color, RB3_VENUE_LIGHT_OFF=color). Root cause in Lane-A-owned engine files → **staged luminance-preserving-ceiling patch** + Wave-7 backlog (`e5166976`). Likely same mechanism-space as the wash item. |

**Side agents (user-reported, characterization-only):**
- **W4.2 menu text/selection (`537c4f7b`):** focused menu items render pale-on-pale because of an
  unconditional UI-text color floor `max(0.6, color)` at `RB3MaterialBinder.cpp:145-149` clobbering
  `UILabel` per-focus-state colors (focused = authored ~black on gold). LONG-STANDING (engine
  `08b3932`, 2026-05-30), not a wave regression. → Wave-7 engine fix; gate = the ticker/FRIEND
  RANKINGS/CHOOSE INSTRUMENT labels the floor originally rescued stay readable.
- **W2.8 missing hands + transparent torsos (`7ada449b`):** hands DO draw, zero guard-DROPs —
  fingers **shard into thin sheets by R·sin(θ)** (rotation-basis mismatch: animated bone basis ≠
  static magnet the invBind was baked against; the documented C8 residual). Whole-mesh ratio +
  origin skinpos read CLEAN = **W2.2's oracle is BLIND to this** (needs a far-vertex rotation
  metric). "Transparent torsos" = the same shard sheets over the silhouette; all torso base meshes
  draw opaque (blend=1) — no material bug found. → Wave-7: **BL-A2 far-vertex oracle first, then
  BL-A1 rotation-aware invBind rebake** (`BandCharacter.cpp`, collision-free).

**Coordinator actions:** E1 sign-off + flip (`fced18b`) + oracle A/B reproduced (default GREEN /
opt-out RED); single classification regen (`1b045d9`, 325 flags, check clean); harness OFF-arm
inversions; re-golden 888→792 + N≥30 sidecar + 15/15 sweep + lineup gate delegated (in flight);
pin bump follows.

### New/updated backlog (Wave 6 → Wave 7)

- **WASH (flip-independent, from W2.1-flip-blocker/STATUS.md):** stochastic full-frame venue-cam
  wash (PINK/NEARBLACK/WHITE), boot-nondeterministic. Ranked prior: async asset residency (W0.3d
  part-b patch still unlanded) > RB3PostProc grade > P4 venue-light. Method: 4-flag matrix, N≥6
  boots/config, scored by `wash_score.py`. Cross-check W3.3 (same mechanism space).
- **W3.3-fix:** land the staged luminance-preserving composite ceiling patch (Lane-A engine files
  now free).
- **W4.2-fix:** relax/gate the UI-text color floor (`RB3MaterialBinder.cpp:145-149`), A/B'd against
  the three labels it originally rescued.
- **W2.8:** BL-A2 far-vertex rotation oracle (rb3-tests; no engine hook) → BL-A1 rotation-aware
  outfit invBind rebake in `BandCharacter.cpp`. BL-B1 (low): named-member torso recheck.
- **W3.2 escalate-or-drop:** point-light fidelity (per-object cube vs per-pixel Lambert) is the real
  SYS-4 gap; decide whether to land the box-ambient prototype anyway (near-no-op on current venues)
  or redesign around point lights.
- **RB3_HUB_MENU_QUAD_HIDE flip decision:** default-OFF; coordinator visual A/B then flip.
- **Venue black poster quads** (part_difficulty backdrop, out of W4.1 fence) — SYS-5 family.
- Carried: 4→8 light arrays (DC3 gates), W2.6 foot/shoe lower body, W2.4 BandPatchMesh decision.

## Wave 7 results (2026-07-06, run `wf_5527e4f1-ad7`, 11 agents)

**TWO MORE FLIPS SHIPPED (UI-text color floor + hub grey-quad hide, both coordinator-signed); the
grayscale fix was REFUTED by its own verifier; the hands fix was proven to need a per-frame
correction (two static approaches empirically killed); BoxMap = DROP.** Engine → `a94762f`
(pin bumped after 792 golden + lineup PASS on the flipped defaults). Kickoff+review `7fddb7da`.

| Item | Status | Highlights |
|---|---|---|
| W3.3-fix grayscale | ❌ **refuted [kept default-OFF]** | A.S1 landed the luminance-preserving highlight ceiling (`RB3_PP_LUMA_CEILING`, engine `7943bfa`) with all its gates green — and A.S2's independent per-tonal-band verify **refuted the premise**: the grey wash is a **sub-knee MID/LOW-tone desaturation** (default mid-tone sat 0.026 vs pp_off control 0.389) and both ceiling branches are identity below the knee (L<0.82) — the fix architecturally cannot touch it; its single-crop positive read did not survive a determinism-controlled analysis. Flag-OFF byte-identical + flag-ON regression-free (beyond-sweep hot frames checked), so it stays landed as a documented no-op-on-target. **W3.3 reopened → Wave 8: the composite exposure/tonemap/grade stage (sub-knee desat), quantified by the pp_off control.** |
| WASH matrix | ⚠️ **partial [run in flight]** | Driver landed (`wash_matrix.py`, rb3 `a67d0d2c`): 5 configs (W3.3-ON baseline / highway_bloom_off / bloom_off / venue_light_off — each with W3.3 ON — / W3.3-OFF control), contract-default pinned in all arms, songMs-pinned round-robin, N=8/config. The full ~40-boot run executes detached; verdict appended on completion. Round-1 directional signal (unconfirmed): PINK survives highway_bloom_off + venue_light_off; `bloom_off` arm NEARBLACK. |
| W2.8 hands (BL-A2→BL-A1) | ✅ **oracle landed / fix path REDEFINED** | BL-A2 far-vertex oracle: RED on today's build (hand far-verts shard **79-107u** vs 20u threshold) — the gate that W2.2's oracle provably lacked. Step-0 (WAVE7_REVIEW A5) killed the dormant flag: `RB3_HANDS_BIND_FIX` is **INERT on the real band path** (its `clipPlaying` trigger fires 0×). B.S3 then empirically killed the rigid-anchor pattern too: `RB3_HANDS_POSEAWARE` (default-OFF, landed) helps uniformly-authored gloves (70-84→65u) but **DISTORTS per-bone-authored hands (106→205u)** — a static rigid collapse scrambles per-bone-authored verts. B.S4 confirms DO NOT FLIP. **Net: the finger shard requires a true per-frame pose-aware basis correction (Wave 8); two whole classes of static fix are now proven dead ends with numbers.** |
| W4.2-fix text floor | ✅ **complete → FLIPPED default-ON** | Relaxed floor landed: authored colors pass through unchanged; only true-invisible (<0.06 all channels) text lifts to 0.25 (the three labels the 0.6 floor originally rescued). C.S2 independent verify: focused-item contrast toward retail on **every reachable screen**, flag-OFF provably inert. **Coordinator flip (engine `a94762f`): default-ON, opt-out `RB3_UI_TEXT_FLOOR_STRICT`; verified live (QUICKPLAY dark-on-gold, PLAY NOW dimmed).** |
| W4.1 hub-quad | ✅ **package → FLIPPED default-ON** | C.S3 package: quad gone in both flag-ON boots, QUICKPLAY/ROAD CHALLENGE label ROIs byte-unchanged across all 4 captures. **Coordinator flip (rb3 `cda3b326`): default-ON, opt-out `RB3_HUB_MENU_QUAD_OFF`; verified live on the default build.** |
| W3.2b point-light | ✅ **complete [DROP]** | Recommendation accepted: keep per-pixel Lambert (strictly higher fidelity than a per-object ambient cube); do NOT land box-ambient (near-no-op: venues have 0 directional approx lights), do NOT grow `SceneUniforms` 656→752 (DC3 blast stays zero). Prototype + flag shelved on `wave6-boxmap-proto`. Reopen only on a human-captured point-spot-dominant venue that measurably beats Lambert. |

**Coordinator actions:** flip decisions per table (2 flips, 2 no-flips); classification regen
(330 flags, check clean); drawlog 792 green + lineup PASS on the flipped defaults; pin bumped
`1b045d9` → `a94762f`.

### Wave 8 menu (from Wave 7)

- **W3.3b (reopened):** the sub-knee mid/low-tone desaturation in the composite
  exposure/tonemap/grade stage — the pp_off control quantifies the target (mid sat 0.026→0.389).
- **W2.8c:** per-frame pose-aware basis correction for per-bone-authored appendage meshes (the only
  remaining fix class; BL-A2 oracle is the gate, 79-107u RED baseline).
- **WASH verdict** (when the matrix lands) → mechanism item or fix.
- Carried: venue black poster quads (SYS-5), 4→8 lights (DC3 gates), W2.6 foot/shoe, W2.4
  BandPatchMesh, song_select minor residuals.

## Wave 8 results (2026-07-07, run `wf_daec8375-122`, 8 agents)

**THE VENUE WASH + GRAYSCALE ARE FIXED AND SHIPPED (chroma-preserve composite, default-ON); the
matrix's own mechanism theory was overturned by instrumentation; the hands conjugation was refuted
honestly at smoke; the black poster quads are NOT a bug.** Engine → `a320f9d` (pin bumped after
792 + lineup PASS on the flipped default). Kickoff+review `2eb2652b`.

| Item | Status | Highlights |
|---|---|---|
| WASH-fix S1 instrumentation | ✅ **complete [H1 REFUTED]** | `RB3_WASH_PROBE` (engine `71469af`): **0/8 default boots miss engagement — the env-state machine is exonerated**; PINK boots have byte-identical lighting inputs to clean boots. The pink is a downstream screen-space magenta flood (composite grade), NOT a revealed pink base — reframing the Wave-7 matrix verdict (venue_light_off washes because the flat-default flood replaces dark engaged lighting, not because it unmasks). H2 confirmed-refined: composite desaturates hot venue input; the grey is **director-shot-dependent**, not ms-pinned. |
| WASH-fix S2/S3 | ✅ **complete → FIX-H2 FLIPPED default-ON** | Two fixes, separately flagged: **`RB3_PP_CHROMA_PRESERVE`** (venue-scoped chroma preservation in the Stage-2 composite — graded luminance × ungraded chroma, menu B+W look untouched via a `venueGrade` uniform, struct stays 176B) + **`RB3_VENUE_FALLBACK_FIX`** (dim exposure-safe broken-env fallback, world.cam-scoped). Gates: vlo deterministic fail-red 6/6 wash → **0/5 NEUTRAL** (both fixes); **grey venue → restored colored stage lighting** (mid_sat 0.067→0.208, PP_OFF-look with engagement verified); per-flag flag-OFF byte-identical; lineup PASS; DC3 zero-blast. A.S3 independent verify: reproduce all + **RECOMMEND FLIP for FIX-H2**. **Coordinator flip (engine `a320f9d`): FIX-H2 default-ON (opt-out `RB3_PP_CHROMA_PRESERVE_OFF`, legacy `=0` disable kept); FIX-H1 stays opt-in** (inert on the default path — documented safety net). Disclosed residual → Wave 9: engaged-venue WHITE over-exposure (raw sub-knee, in the space `RB3_PP_LUMA_CEILING` was aimed at). |
| W2.8c per-frame hands | ❌ **refuted [honest negative]** | The per-bone conjugation `offset_b(t)=inv(A_b)·inv(L_b(t0))·L_b(t)·A_b·inv(L_b(t))` **AMPLIFIED the twist under animation (80u → 500-2600u)** — caught at the planned S2 smoke tripwire, stopped rather than tuned blindly. B.S3 confirms DO NOT FLIP. "Rotation basis wrong, translation correct." **Three fix classes are now dead with numbers** (static rebake, rigid anchor, game-side conjugation). → Wave 9: bone-level attribution FIRST (dump per-bone offset/boneWorld/weight vs the CPU reference per sharding vertex; name the wrong factor empirically before any 4th fix attempt). |
| W5.1 black poster quads | ✅ **complete [NOT A BUG]** | `RB3_HEADMAT_DBG` census (zero new code): the reference cork-board black poster is the **tv3_a transition vignette**, and `showtonight_poster.mesh` binds + samples its texture (`hasTex=1`, faint embossed text) — an **authored-dark concert poster**, not a null-diffuse. The W2.7-family hypothesis was correctly killed by the probe before any code. No fix needed. |

**Coordinator actions:** FIX-H2 flip + regen (`a320f9d`, 336 flags clean); 792 + lineup PASS on
flipped default; pin bumped `a94762f` → `a320f9d`.

### Wave 9 menu (from Wave 8)

- **W2.8d hands attribution:** per-bone factor diff (GPU palette vs CPU reference vs authored data)
  on a sharding finger vertex — name the wrong factor with numbers; only then design fix #4.
- **WHITE over-exposure (engaged venue):** the residual A.S2 disclosed — raw sub-knee over-exposure
  on hot engaged venues; composite family, same files (now unowned).
- **Current-state re-baseline:** fresh screenshot sweep vs `/tmp/wave6-current-state/` — five
  defaults flipped since; find what the user sees now + surface new items.
- Carried: 4→8 lights (DC3 gates), W2.4 BandPatchMesh, song_select residuals.

## Wave 9 results (2026-07-07, run `wf_0a159b21-2f4`, 5 agents)

**Diagnosis wave: the hands factor is NAMED (offset rotation basis, constant 42-87° off the
per-member rest basis) but the world-space fix attempt was refuted (4th dead class) — exactly ONE
unrefuted path remains; WHITE is proven SCENE-SIDE with a staged patch; the rebaseline confirms 4
fixes on-screen + surfaces 1 new anomaly.** No flips this wave. Engine → `10a9ca6` (probes + regen).

| Item | Status | Highlights |
|---|---|---|
| W2.8d attribution | ✅ **named** / ❌ **fix refuted** | S1 (dualskin probe at the palette-compose point, RealPathFixture golden populated SKIP→RED 32.8u): **candidate (b) — the skin offset's rotation basis is conjugated ~42-87° off the per-member bone rest basis, constant/pose-independent**; candidate (a) refuted (live bone worlds are faithful rigid transforms to 178° curl). S2's minimal fix (world-space rest capture, `RB3_APPENDAGE_REST_ROT`) **REFUTED**: wext regresses 73→80u — it reproduces the 2026-06-11 world-space lever-arm dead-end, and the probe's ΔR is partly a placement-yaw artifact (the review's A1 risk realized). **Wave 10 = the only unrefuted path: asset-level rebake against `skeleton_unshared.milo`'s AUTHORED bind rotation (char-space, asset-derived, static)** + fix the ΔR metric to like-space comparison. Do NOT flip `RB3_APPENDAGE_REST_ROT` (documented regression). |
| WHITE-fix | ✅ **complete [SCENE-SIDE, patch staged]** | Force-reproduced 3 ways (deterministic flood 5/5 over-exposed; natural engaged 1/5 matching the disclosed ~1/6). Discriminator: **the RAW scene whites at ≥ the composite peak and the composite RAISES mid-band sat (0.045→0.283)** — chroma-preserve restores color, it is not the gain; the near-white is baked by `softClipLighting`+`compressHighlights` on pale surfaces × hot lighting before the UNORM clamp. Fix designed + **staged** (luminance-preserving venue highlight compression via a repurposed `SceneUniforms.venueHighlightLumaMode`, default-OFF `RB3_VENUE_WHITE_GUARD`) — touches Lane-A + DC3-shared files → lands Wave 10 coordinator-sequenced. |
| REBASELINE | ✅ complete | **Confirmed fixed on-screen vs the archived Wave-6 baseline:** hub grey quad GONE, menu text crisp, black head absent across 6 frames/3 boots, part_difficulty "missing widgets" independently re-refuted (settle sequence). **NEW anomaly: disconnected floating forearm+hand** (2 contexts: partdiff wipe transition +060/+120, and a gameplay ceiling-hung disembodied forearm with no wipe active) — H1 shard-family full detachment vs H2 occlusion illusion, deliberately not diagnosed in-lane → Wave-10 triage attached to the hands lane. |

**Coordinator actions:** no flips (nothing earned one); regen (`10a9ca6`, 339 clean); 792 + lineup
PASS; pin bumped `a320f9d` → `10a9ca6`; montages reviewed (hub + band confirmed by eye).

### Wave 10 menu

- **W2.8e:** asset-derived bind-rotation rebake (the only unrefuted hands path) + like-space ΔR
  metric fix + floating-forearm triage (H1 vs H2) as a sub-check.
- **WHITE-fix landing:** the staged `RB3_VENUE_WHITE_GUARD` patch (single-writer sequencing now free).
- Carried: 4→8 lights (DC3 gates), W2.4 BandPatchMesh, song_select residuals.

## Wave 10 results (2026-07-07, run `wf_79fca7a3-5a2`, 6 agents)

**No flips — and that is the system working: the 5th hands fix class was refuted (it freezes the
hands) with the S1 instrument itself unmasked as confounded; the WHITE guard was HELD on a
sign-flipping primary metric + a null control that quantifies a per-boot lighting-noise floor; the
floating forearm is confirmed H1 (real shard-family vertex smear).** Engine → `6834744`.

| Item | Status | Highlights |
|---|---|---|
| STEP-0 WHITE land | ✅ complete | Staged scene-side patch landed default-OFF `RB3_VENUE_WHITE_GUARD` (engine `2998e78`): `_padPL`→`venueHighlightLumaMode` (static_assert 656 intact), gated `compressHighlightsLuma`, world.cam-engaged write. Inertness proven (792, Dawn WGSL gtest, milo-engine-tests 200/200 incl. DC3). Single-writer handoff honored (audited: exactly one Lane-B engine commit). |
| W2.8e S1 | ✅ **MATCH** (with hindsight caveat) | Like-space fixture re-derivation; placement-yaw confound REFUTED by measurement (bone-chain roots at identity; placement lives in obj.world); fixture re-proven RED 37.4u; provenance-by-invariance: `inv(off)` identical 106.0° across members with distinct 38/40-bone skeletons = offset baked against the shared magnet while the palette animates per-member bones. All 6 pre-registered tolerances PASS → verdict MATCH. |
| W2.8e S2 | ❌ **REFUTED [5th class dead + instrument unmasked]** | The asset rebake collapses worstSep to **0.0u by FREEZING the appendages** (worldExt pinned to 2 discrete values; same worst vert every frame; characterize MARGINAL→HARD-SHARD). `RB3_APD_DIAG` found the real story: the DEFAULT rebind repoints mesh `bound`(static 129° bind copy)→`own`(live animating 106°), and **the dual-skin probe's reference was captured pre-repoint** — the 37.4u "shard" compares the drawn vertex against a bone the draw does not use; the metric is unsatisfiable without freezing. **All five bind-side bake classes are dead; any 6th attempt is forbidden — the next step is a NEW instrument (coherent-vs-drawn-bone, post-repoint) to establish whether a real residual exists at all.** `RB3_APPENDAGE_ASSET_REBAKE`+`RB3_APD_DIAG` kept default-OFF documented. |
| W2.8e S3 forearm triage | ✅ **H1 confirmed** | Both sightings = the known hand-shard family: `hands_naked.mesh` IS drawn (0 guard-DROPs, band cap 110u), far-from-wrist verts smear by ~R·sin(θ) to 61→106u worldExt while the wrist bone stays attached; body renders coherently at ~50u. H2 (occlusion) + H3 (guard-DROP leaving a legit forearm) refuted by draw-log census. **The visible symptom is real** — in tension with S2's "characterize=MARGINAL": reconciling the instruments is the Wave-11 job. |
| WHITE-fix B.S1/B.S2 | ✅ **complete → HOLD (no flip)** | Two blind runs: the primary `d_hi_frac` **sign-flips** (+17.96 vs −6.89); the **null control** (flood arm where the guard is source-provably inert) swings ±5-18 hi_frac — the eng_hot delta is the same noise; G1b (chroma up) fails BOTH runs because washing regions are **zero-chroma** (nothing for chroma-preservation to save — the design premise doesn't hold where it matters). Guard stays landed default-OFF, safe, documented. **The real levers: (a) venue exposure on hot engaged moments, (b) FIRST tame the per-boot lighting nondeterminism (mid_sat 0.067–0.362 at identical params/shot/fixed-clock) that makes every arm-mean visual gate non-resolving.** |

**Coordinator actions:** no flips (correctly none earned); regen (`6834744`, 343 clean); 792 +
lineup PASS; pin bumped `10a9ca6` → `6834744`.

### Wave 11 menu

- **BOOTRNG (new, unblocks everything):** root-cause the per-boot lighting/grade nondeterminism
  (which postproc/venue-event state varies per boot at a pinned shot under RB3_FIXED_CLOCK) — it is
  the visual-gate noise floor AND the last stochastic user-visible phenomenon.
- **W2.8f:** the coherent-vs-drawn-bone instrument (post-repoint reference); reconcile
  characterize-MARGINAL vs the confirmed H1 visible smear; NO fix attempt until the instruments agree.
- Carried: WHITE real-lever design (after BOOTRNG), 4→8 lights (DC3 gates), W2.4, song_select
  residuals.

## Wave 11 results (2026-07-07, run `wf_8a8111a5-b24`, diagnosis-only, 2 lanes)

**Both diagnosis exits hit: BOOTRNG's boot-varying state is NAMED (global `gRand` stream position —
upstream owner = W0.3d part-b) with the render/postproc stack exonerated as deterministic-given-
inputs; the hands defect axis is NAMED (authored-vertex-to-offset SHELL composition) with the
palette/skeleton axis EXONERATED by a trustworthy rest-capture-free instrument on the exact
visible-smear frames.** No flips (none expected — diagnosis wave). Engine → `146fd19`.

| Item | Status | Highlights |
|---|---|---|
| BOOTRNG A.S1/A.S2 | ✅ **named [upstream owner: W0.3d part-b]** | N≥10 instrumented boots at the pinned shot: preset PICK is deterministic (10/10 same preset — the A1 prime suspect refuted as the variance source), per-light VALUE digest + resolved postproc ColorXfm tuple **identical across boots** (the A3 color-blindness fixed and the Wave-8 exoneration now holds at value level). The boot-varying state = **global `gRand` stream POSITION** (~11k draw-count spread across boots): seed is pinned (0x5EED) but consumption ORDER varies per boot via async loader completion order — the exact mechanism W0.3d part-b (staged since Wave 4) exists to fix. Free harness lever found: capture `--tol` 2000→100-150ms (songMs Pearson 0.77 = the dominant measurable confound). Residual wash after both = FX/swept-light PHASE axis (co-sampling instrument = Wave-12 item). S2 correctly did NOT patch in-lane (mechanism is upstream of the render). |
| W2.8f B.S1/B.S2 | ✅ **GREEN_BRANCH_SHELL_AXIS** | The corrected instrument (Tier-2 parent/child **joint-attachment** on the UPLOADED palette, rest-capture-free per A5; fail-red proven by perturb) reads **GREEN ≤0.33u over 2,214 samples on the exact frames showing the visible 95-106u smear** (A7 co-variation FAILS) → **the palette/skeleton axis is EXONERATED** — retro-explaining why all five bind-side fix classes died: they patched a coherent palette. `wext` reproduces the smear in a **pure CPU 4-bone blend** of the same authored verts × weights × uploaded palette → GPU exonerated too (A6 readback = confirmatory-only, predicted GREEN). Named axis: **authored-vertex-to-offset composition (the mesh SHELL)** — the shell rotates about each joint by ΔR while joints stay attached (R·sin(θ): 48.5·sin(87.3°)=48.4u/bone → 95-106u finger chain), which is structurally invisible to every skeleton-side metric we built. Wave-12 fix gate = joint-attachment stays GREEN + wext drops to ~50u body-coherent range. |

**Coordinator actions:** no flips; regen (`146fd19`, 346 clean); 792 + lineup PASS; pin bumped
`6834744` → `146fd19`.

### Wave 12 menu

- **W0.3d part-b (coordinator-sequenced):** boot-stable gRand stream position under fixed clock —
  the single named blocker for boot-stable visual gates, three consumers (BOOTRNG, WHITE gate
  resolution, all future arm-mean gates). _CORRECTION (Wave-12 review A1): no staged patch exists —
  the only staged part-b artifact was the SortDraws tie-break, landed Wave 5; this is NEW design
  work, attribution-first (completion-frame TIMING is the prime suspect, not insertion order)._
  Plus the free harness lever: wash capture `--tol` 2000→100-150ms.
- **W2.8g hands SHELL axis:** with palette+GPU exonerated, diagnose the authored-vert-to-offset
  composition (vert basis vs offset basis at compose time; V24/weight interpretation on the shell
  verts) using the now-trustworthy joint-attachment + wext instruments as the fix gate.
- **FX/swept-light phase co-sampling probe:** the residual wash axis after BOOTRNG (per Lane A
  followups) — instrument first.
- Carried: WHITE real-lever reframe (per-preset/FX phase fidelity, unblocked once W0.3d-b lands),
  4→8 lights (DC3 gates), W2.4 BandPatchMesh, song_select residuals.

## Wave 12 results (2026-07-07, run `wf_c561d974-479`, 7 agents; C34 stalled → side-agent re-run)

**Two honest measured failures that each NAME the real fix, one major exoneration, and one
architecture-level UI diagnosis.** No flips. Engine → `44716f4` (regen 353 clean).

| Item | Status | Highlights |
|---|---|---|
| W0.3d-b A.S1 | ✅ **H-TIMING REFUTED / H-ORDER named** | All 511 loader completions byte-identical across boots and landed by frame 2, yet gdraw diverges from frame 4 with ZERO completions on diverging frames — completion-frame timing is OUT. Mechanism class = **consumer-ORDER × variable-count rejection samplers** (unsorted `mAnims` walk `Dir.cpp:53` + `Rand::Gaussian` do-while / `CameraShot.cpp:265-267` conditional draws / `Crowd.cpp:1234` Fisher-Yates — order permutation DOES change the count, reconciling A2), order variance sourced from the main↔ThreadCall-worker glibc-arena allocation race (survives setarch -R). `--tol` 150ms lever landed (`653ba4a4`); `RB3_LOADDET_PROBE` attribution instrument landed. |
| W0.3d-b A.S2 | ⚠️ **PRIMARY-FAIL / PARTIAL landed** | H-RESEED + worker-serialize + mAnims-sort behind opt-in `RB3_LOAD_DETERMINISM` (fixed-clock-scoped): best variant reduces the post-anchor gdraw spread ~62% but every ON arm stays distinct → PRIMARY (10/10 identical stream position) FAILS — reseed fixes VALUES not ORDER; the Wind/CameraShot/Crowd rejection-sampler sites remain order-varying. Kept landed as a documented partial reducer + gate scaffolding (`loaddet_gate.py`, jitter fail-red flag); flag-OFF 792 byte-identical BOTH arms. **Sufficient fix (staged design, in STATUS.md with the landed mAnims-sort as template): determinize order at every rejection-sampler-feeding site or per-consumer isolated Rand streams.** DO NOT default-flip. |
| W2.8g B.S1 | ✅ **SPACE_AXIS (decode refuted)** | Instrument B + rest-free discriminator landed (`:4872-4966`, `RB3_HANDS_INSTR_B`): sub-shells transported ISOMETRICALLY (isoDistort~0.0000, orthoResid~0.0002) with shellMax 20-227u vs clean-body 1-4u; oracle truth-table PASS (SPACE conjugation iso=0 + shellErr≈R·2sin(θ/2); DECODE iso=0.231). Verts sharing a bone move as ONE rigid rotation → per-vertex weight/index/decode REFUTED. Design-doc literal ‖s−ŝ‖ found CONFOUNDED for the per-frame A7 gate (hands smear from frame 3 — no clean-rest frame exists); trustworthy fix gate = rest-free invariants stay ~0 + wext collapse. |
| W2.8g B.S2 | ❌ **BLOCKED — 6th cell measured dead; RE-LANED** | The last untried single-live-bone cell (`RB3_HANDS_SHELL_FIX` = own-live + bound-rest 129°) predicted-and-confirmed shard-at-REST: wext min 34.8→51.0u UP, mean 68.9→82.4u UP, Tier-2 0.33→0.81u worse, screenshot = flesh-spike starburst. **The 87° basis gap is IRREDUCIBLE with any single live bone.** Root cause re-confirmed: the ANIMATING `Find(name)` instance is the SHARED MAGNET (invOff identical 106° across members with distinct 38/40-bone skeletons) while the per-member authored 129° rest lives on a STATIC bone. **The real fix = skeleton instancing / loader merge: make `Find(name)` resolve the per-member ANIMATING bone carrying `skeleton_unshared.milo`'s authored rest — out of the renderer/no-bake charter → Wave-13 SKEL lane.** Flag documented default-OFF regression. |
| W4.3-C1 | ✅ **DIAGNOSED_ESCALATE (+1 faithful sub-fix landed)** | Both A7 candidates DISPROVEN with probes: UILabel focus color IS applied (mb_playnow focused sets dark 0.118,0.122,0.035) and it REACHES the shader unchanged. **Root cause = compositing: the postproc grade lifts dark glyphs + the focus bar composites through semi-transparent AA text** — PP_OFF passes the contrast gate (2.20 vs default 1.95<2.0; retail calibration 4.17). Sub-finding: `highlight_main.mat` alpha animates to 3.56 (>1) natively vs Wii's [0,1] clamp → clamp landed default-OFF `RB3_HUB_TEXT_CONTRAST` (faithful, doesn't pass the gate alone). **Gate-passing fix = draw UI after grade OR opaque UI text — needs a coordinator declared-range grant (Rnd_Wgpu_RB3.cpp/RB3PostProc) → Wave-13.** |
| W4.3-C2 | ✅ **C2c NOT A BUG / C2a refuted / C2b diagnosed** | **C2c: the all-devil ratings are CORRECT AND FAITHFUL** — `RankTier` returns found=1/ntiers=7 for all 856 calls, tiers monotonic, even histogram; both A8 probes refuted; devil glyph renders correctly (skulls intact). Tiering is equal-count bucketing: on the 83-song stock library the hardest ~1/7 land tier6=Impossible=devil; the retail ref showed a 587-song DLC library — apples-to-oranges. No fix. C2a: leaderboard-hide refuted; the panel backing lives in the sibling `song_select_details.milo` sub-panel which is not compositing behind the grid natively (next: walk the subdir — never-submitted vs dropped). C2b: album-art quad overlapping the header = SYS-5 Y-anchor/panel-origin offset family (same as C4), diagnosis only. |
| W4.3-C34 | ✅ **C3 NOT_A_BUG / C4 narrowed** (side-agent re-run `c7f101f7`) | Prior agent stalled 6× (suspected `pkill -f rb3-native` casualty → pgid-only cleanup rule). Re-run verdicts: **C3 = NOT A BUG** — the "flipped" hold-labels are `InlineHelp::SetLabelRotationPcts` (`InlineHelp.cpp:549-554`), a correctly-bounded faithful flip-card reveal animation with no HX_NATIVE guard; ~93k telemetry samples show it never sticks; the capture caught mid-transition; text renders upright at rest. **C4 = game-side text SCALE/WRAP gap on `message.lbl`**: retail renders the message body at a smaller font wrapped to 2 lines; native renders label-sized/unwrapped (world-xfm ~6u from sibling vs 20.8u confirmed-stacked pair — spacing may be correct FOR a wrapped font) → NOT the engine mesh path, NOT necessarily the C2b anchor family (coordinator note passed to the Wave-13 C2b4 lane). Follow-up: find the unapplied font-scale/wrap-width property (Text/UILabel/AppLabel/BandLabel). |

**Coordinator actions:** no flips (correctly none earned); regen (`44716f4`, 353 clean); 792 +
lineup PASS on default arms; pin bumped `146fd19` → `44716f4`; C34 re-dispatched.

### Wave 13 menu

- **SKEL (new lane, from B.S2's re-lane):** per-member skeleton instancing — make the animating
  `Find(name)` resolution return the member's own bone (authored 129° rest) instead of the shared
  magnet; the named root fix for the hands/finger shard family, loader/merge-side, renderer
  untouched. Gates already built: rest-free Instrument-B invariants + wext + Tier-2 + lineup.
- **C1-grant:** UI-after-grade compositing (coordinator declared-range grant into
  Rnd_Wgpu_RB3.cpp/RB3PostProc): draw UI/overlay pass after the postproc grade (or opaque text
  path) — the gate-passing fix for focused-text contrast; highest user-visible win.
- **C2a/C2b/C4 (+C3 per side-agent):** song_select_details panel-bg submission, SYS-5 Y-anchor
  layout offsets (art-over-header + ticker), flipped hold-labels per the C34 re-run verdict.
- Carried: loader-determinism sufficient fix (rejection-sampler-site order determinization —
  staged design in W0.3d-b/STATUS.md), WHITE real-lever, 4→8 lights (DC3 gates), W2.4.

## Wave 13 results (2026-07-07, run `wf_6092bda5-74c`, 6 agents + 2 side agents)

**One flip shipped (hub ticker), one flip earned-but-held on my E1 (UI post-grade — visible
song_select depth-clear artifact), the hands root cause finally NAMED at the asset level after a
premise inversion, and two "bugs" honestly closed as not-bugs.** Engine → `3b5af48` (regen 354).

| Item | Status | Highlights |
|---|---|---|
| SKEL S.S1 | ✅ **PREMISE INVERTED** | Runtime pointers overturn the Wave-9→12 framing: `own=Find(name)` is PER-MEMBER, ANIMATES (4 distinct ptrs across members, 116 distinct own vs 42 shared bound, moves 186-276u/Poll) and IS gender-posed (index01 male 109.5° vs female 120.1°); `bound=BoneTransAt` is the SHARED static authored bind. The old "shared magnet animates" reading = the dual-skin probe sampling PRE-rebind state with own/bound labelled backwards. Offset writer = RebindHeadHandsAtRest distinct-resolve bake (`:1656-1752`); crowd risk EVAPORATES (no share-layer change needed). Residual = mesh verts encode `bound`'s inter-bone geometry vs own's per-member gender rest (~17°/bone female, ~6° male). |
| SKEL S.S2 | ❌ **BLOCKED (feasibility-gated pre-edit)** | Seam A (un-share+gender-pose the embedded bind) proven DEGENERATE by dataflow: the palette never reads `bound` after the SetBone repoint — copying ownRest onto it is a no-op; the only bound-reading bake IS the dead 6th cell. No "gender-posed bound rest" exists (gender bind is a runtime CharClip, no static skeleton asset). Seam B (per-vertex re-pose) TEARS multi-bone knuckle blends (per-bone own-vs-bound gaps MIXED SIGN up to ~35°). **ROOT CAUSE NAMED: native-port bind-basis split — hands verts skinned against the shared male-bind while drawing on per-member gender-posed bones; female double-mismatched (female-authored verts → male bind, identical ptr). FAITHFUL FIX = per-member RESKIN of verts+weights onto `own` via the existing RndMeshDeform::Reskin pipeline (`BandCharacter.cpp:3111-3115`) — engine lane, Wave 14 headline.** Offset-bake class formally exhausted (6 dead cells + this degenerate 7th framing); no fake flag landed; RB3_NO_SKIN_CLAMP stays the shipped mitigation. |
| UIGRADE G.S1/G.S2 | ✅ machinery landed + verified-inert | Mechanism: menu PanelDirs never flush → single EndFrame composite grades venue+UI together. Machinery: `RB3_UI_POST_GRADE` + FlushPostProcMidFrame venueGrade parameterization via menu-flush latch (engine `f677871`), DC3 zero-blast by construction (RB3-only TU). Baselines: grade exemption is a measurable win ONLY on the hub (1.95→PP_OFF 2.20); song_select + partdiff are grade-INERT (their residual = bar-bleed polarity, separate item). |
| UIGRADE G-TRIGGER | ✅ wired / ⚠️ **FLIP HELD (E1)** | Side agent found TWO more false premises (menus are `mCanEndWorld=1`; `Rnd::EndWorld()` is a permanent no-op on native — BandRnd::BeginDrawing never resets mWorldEnded) → trigger wired via the public `ClearDepthForOverlay()` seam in `PanelDir::DrawShowing` (HX_NATIVE, default-OFF, gameplay-gated; engine hardened `a5cf8d3`, rb3 `82aa81c7`). Gates: hub 1.954→**2.204 PASS** (== PP_OFF target), partdiff in-band, A5 backdrop chroma unchanged, gameplay pixel-invariant, flag-OFF 792 + Wii byte-identical. **COORDINATOR E1 HOLD: song_select flag-ON shows a VISIBLE compositing change (red band on the SETLISTS row; metric 1.110→1.049 below parity) — the ClearDepthForOverlay depth-clear side effect. Clean flush-only seam (no depth-clear) needs a small Rnd_Wgpu_RB3.h grant → Wave 14; flip after.** |
| W4.3-C2a | ✅ **closed: asset difference, not a bug** | The 360-ARK `song_select.milo` has NO quick-view sidebar backing — `difficulty_bg*/raitings_bg` belong to the details DRILL-IN page (blanket-show double-draws it; force-show changes grid ROI <1% = nothing composites behind). Retail Wii's backing = different asset layout. Faithful fix would require AUTHORING a new backing quad → backlog polish item, not a render bug. |
| W4.3-C2b4 | ✅ C4 **FLIPPED default-ON** / C2b held | C4 hub ticker: `message.lbl` authored Z gap ~6u vs ~20.8u for a confirmed-stacked pair → −15u nudge; **coordinator E1 PASS (retail-matching stacking), flipped default-ON `RB3_HUB_TICKER_YFIX` with opt-out `_OFF`** (rb3, drawlog 792 PASS post-flip). Also REFUTED the C34 font-scale/wrap theory with probe data (message.lbl already 16.2/750-wrap). C2b album art −120u: fixes the header overlap but **E1 HOLD — reveals a misaligned grey bezel frame that doesn't move with `album_art.grp` + new left-column overlap**; needs the whole assembly identified and moved together. Shared-family verdict: PARTIAL (both authored-spacing gaps, different mechanisms). |
| W4.3-C34 (Wave-12 carryover) | ✅ closed | C3 hold-labels NOT A BUG (faithful InlineHelp flip-card animation, captured mid-transition). C4 root work superseded by C2b4's fix above. |

**Coordinator actions:** RB3_HUB_TICKER_YFIX flipped default-ON (E1 sign-off); RB3_SS_ART_YFIX +
RB3_UI_POST_GRADE held with documented reasons; C2a closed as asset-difference; regen (`3b5af48`,
354 clean); 792 + lineup PASS; pin bumped `44716f4` → `3b5af48`.

### Wave 14 menu

- **RESKIN (headline, engine lane):** per-member reskin of `hands_naked` (+ `fingernails_resource`)
  verts+weights onto the per-member `own` skeleton via the RndMeshDeform::Reskin pipeline
  (`BandCharacter.cpp:3111-3115` precedent) — the ONLY remaining faithful hands lever; must handle
  the female double-mismatch (female verts currently bound to the shared male bind). Gates already
  built (Instrument-B invariants, wext, Tier-2, crowd oracles, lineup).
- **UIGRADE clean seam (small grant):** flush-only entry into `Rnd_Wgpu_RB3.h/.cpp` (no
  depth-clear) → removes the song_select red-band artifact → then flip `RB3_UI_POST_GRADE`.
- **C2b art assembly:** identify the grey bezel/frame element that doesn't move with
  `album_art.grp`; move the whole assembly; then flip `RB3_SS_ART_YFIX`.
- Carried: song-select sidebar backing quad (authored polish, optional), bar-bleed text polarity
  (song_select/partdiff residual), loader sufficient-fix, WHITE real-lever, 4→8 lights, W2.4.

## Wave 14 results (2026-07-07, run `wf_bf82df0b-570`, 4 agents)

**TWO FLIPS SHIPPED (menu grade-exempt UI + album-art assembly — defaults now NINE) and the hands
reskin was implemented, measured, and honestly REFUTED by its own pre-registered gate — closing
the vert/offset-bake class entirely and reframing the defect one final level: ANIMATION-BASIS.**
Engine → `fdf0ad9` (regen 357 clean).

| Item | Status | Highlights |
|---|---|---|
| RESKIN R1 | ✅ FEASIBLE (with 2 premise fixes) | Verified from MeshDeform.cpp:337-357: the weighted ALL-bones blend matches the GPU palette's LBS model (dodges Seam-B's single-bone wall). KEY: do NOT route through RndMeshDeform::Reskin — ExportWorldXfm returns live pose only for `exo_`-prefixed bones; compute own->WorldXfm directly. A4 answered: **female hands carry FEMALE-AUTHORED offsets** (distinct mOffset on the same shared boundPtr; male 1876v/38b vs female 1256v/40b) → per-mesh source, no cross-gender derivation. Pivotal de-risk: hands meshes are DISTINCT+self-owned per member → in-place mutation, zero cloning/memory. |
| RESKIN R2 | ❌ **REFUTED by own gate (do NOT flip `RB3_HANDS_RESKIN`)** | Implemented exactly per recipe (fires correctly both genders, gender-distinct A8-i PASS, zero engine TUs) — but wext REGRESSES: flag-OFF mean 74.8u → flag-ON 87.7u → flag-ON+no-rebake 136u. **Root cause reframed: the shard is an ANIMATION-BASIS problem (own_rest vs own_live rotation, R·sinθ) — with the default rebake, skinPos(t)=v'·meshWorld·inv(own_rest)·own_live(t), so the live-vs-rest delta is IDENTICAL flag-ON/OFF by construction; a one-time vertex re-pose only moves verts to larger radius and AMPLIFIES the smear.** Same dead-end class as APPENDAGE_REST_ROT/ASSET_REBAKE. Vert/offset-bake class now CLOSED with 7 measured artifacts. The genuine fix is asset/skeleton-side: make own_live's basis track the verts' authored bind (skeleton bind correction), out of scope for any bake. Kept in-tree default-OFF with REFUTED headers. |
| UIGRADE U-CLEAN | ✅ **READY_FOR_FLIP → FLIPPED default-ON** | Root cause corrected AGAIN: the red band was NOT the ClearDepthForOverlay else-branch — the minimal flush-only shim still showed it. It is FlushPostProcMidFrame's OWN depth-clear-on-resume revealing a z-occluded SETLISTS selection quad. Fix = menuBoundary-gated depth **LoadOp::Load** on the menu flush re-open (venue depth preserved → occluded UI stays occluded); gameplay keeps LoadOp::Clear byte-identical. ALL gates: hub 2.204 ≥2.0, song_select 1.125 in band + red band GONE (0% red both arms), partdiff in band, chroma OK, gameplay flush counts equal, 792 PASS, DC3 structural. **Coordinator E1 PASS → `RB3_UI_POST_GRADE` default-ON (opt-out `_OFF`), engine flip.** |
| W4.3-C2b-ASM | ✅ **FIXED → FLIPPED default-ON** | The revealed grey bezel = `album_frame01.mesh` — a group DRAW-member of album_art.grp but NOT its trans-child (TransParent chain: header_goals.grp←header.grp←all.grp; refutes the C2b4 claim it moved). Whole-assembly move: group (x,z)+=(45,−120) + frame (x,y)+=(41.2,107.17) in each node's own coordinate frame; the X component kills the E1 left-column parallax overlap. **Coordinator E1 PASS (art below header, aligned with bezel, no overlaps, retail-matching) → `RB3_SS_ART_YFIX` default-ON (opt-out `_OFF`).** Follow-up filed: "(null)" gamertag stub text revealed at header top-right (profile subsystem). |

**Coordinator actions:** TWO flips (RB3_UI_POST_GRADE engine + RB3_SS_ART_YFIX rb3) after eyes-on
E1 both arms; RB3_HANDS_RESKIN documented REFUTED default-OFF; regen (`fdf0ad9`, 357 clean); 792 +
lineup PASS on flipped defaults; pin `3b5af48` → `fdf0ad9`. **Defaults now NINE: placement, black
head, hands rest-capture, text floor, hub quad, chroma-preserve, hub ticker, UI post-grade,
album-art assembly.**

### Wave 15 menu

- **HANDS-ADJUDICATION (Fable, synthesis-only):** seven measured dead artifacts + three premise
  inversions demand a full-saga adjudication BEFORE any 8th attempt: read every hands STATUS
  (W2.2→RESKIN), reconcile the animation-basis reframe (own_rest vs own_live vs authored bind),
  and either derive the correct fix with a proof-level argument (candidate: correct the per-member
  skeleton bind so own_live's basis tracks the verts' authored bind — what asset/load change does
  that concretely mean?) or declare the option set closed with residual mitigations
  (RB3_NO_SKIN_CLAMP stays).
- **Bar-bleed text polarity:** song_select highlighted row + partdiff GUITAR are grade-inert —
  their focused-text residual (white-on-yellow vs retail black-on-white) is the bar compositing
  through AA text, a different mechanism than UIGRADE fixed on the hub.
- **"(null)" gamertag stub** (revealed by the art fix; profile subsystem text).
- Carried: sidebar backing quad (authored polish), loader sufficient-fix, WHITE real-lever,
  4→8 lights (DC3 gates), W2.4 BandPatchMesh.

## Wave 15 results (2026-07-07, run `wf_06c0e645-e15`, 3 agents)

**The hands saga is ADJUDICATED with a proof-level derivation (one never-measured cell named as
the fix), the focused-row defect is decomposed into two named gaps (one landed, one blocked on a
real engine gap), and the "(null)" gamertag is FIXED AND FLIPPED (defaults now TEN).**
Engine → `84ccb9e` (regen 360 clean).

| Item | Status | Highlights |
|---|---|---|
| HANDS-ADJUDICATION | ✅ **(a) proof-level derivation** | A2 discharged (87.3° reproduced from readings.txt incl. bimodality; mixed-sign gaps from the rescued log). Pre-registered arm-W baseline RUN: `bound` is STATIC (same boneWorld every frame); **male authored offsets match bound's basis EXACTLY (Tier-1 xcheck 0.1°, all 38 bones)**; female 28.9° off. **NUMERIC CLOSURE: angle(B·inv(R))=87.2° for both L/R middlefinger03 — the runtime 87.3° IS the authored-bind-vs-SetDeformation-seed rotation; the default rebake anchors to a TRANSIENT pose.** Bonus arm-S gender-split run: male palette Tier-1=3.1°, 0/1038 blocks >5° → **the W2.8g "6th dead cell" death certificate was CONFOUNDED** (the shared-B bake killed female/gloves/nails, not the composition). **Named minimal fix (never measured): keep AUTHORED per-mesh offsets + repoint appendages to `own` (SetBone calcOffset=false, the torso pattern) in RebindHeadHandsAtRest — flag-first, rb3-only → Wave 16.** Gates re-anchored: gender-split everything; **wext>60 declared NOT a hands-shard oracle** (legit two-hand extents reach 104u); fallback ground truth = Dolphin+milo-trace single-bone capture. |
| W4.4-ROWFIX | ✅ diagnosed / ⚠️ half-landed (flag stays OFF) | **Depth-occlusion prime suspect REFUTED** (both highlight meshes zmode=0 — no depth test; LoadOp variants identical). Real defect = two coupled focus-state gaps: **(A)** the full-row fill quad (`ml_highlight_glasstopp`) is authored as a near-invisible additive sheen (a=0.08); retail's solid bar comes from an un-translated focus anim/trigger (highlight_bar.grp / highlight_yellow.mesh — 0 GPU draws natively). Part-A repaint landed (`RB3_ROWFIX` default-OFF): focused fill p60 94→179 WORKS. **(B)** focused text stays white because **the native RndText glyph shader IGNORES font-material color** (UILabel::Draw sets the dark color at UILabel.cpp:269; luma unchanged 167) — a real engine gap, BLOCKS the flip (bright fill + white text = worse). → Wave-16 engine item. SETLISTS red-band re-checked 0% both arms; RB3PostProc.h:81 stale comment fixed. |
| W4.5-GAMERTAG | ✅ **FIXED → FLIPPED default-ON** | "(null)" = the weak NULL `PlatformMgr::GetName` stub; strong native override ports `PlatformMgr_Wii.cpp:489-496` verbatim ("Player N" via Localize, graceful token fallback) — ONE provider fixes header + overshell + all consumers. **Coordinator E1 PASS ("Player 1" both places) → default-ON, opt-out `RB3_PLAYER_NAME_FALLBACK_OFF`.** |

**Coordinator actions:** Player-N flip (E1 both arms); `RB3_ROWFIX` correctly held per its own
lane; APD_DIAG evidence rescued to `SKEL/evidence/` pre-dispatch (review A2); regen (`84ccb9e`,
360 clean); 792 + lineup PASS; pin `fdf0ad9` → `84ccb9e`. **Defaults now TEN.**

### Wave 16 menu

- **HANDS-FIX (the adjudicated cell):** authored-offsets + repoint-to-own (SetBone
  calcOffset=false, torso pattern) in RebindHeadHandsAtRest apdMesh scope — flag-first, rb3-only,
  gates per VERDICT §5 (gender-split Tier-1 count>5°=0, ceiling-hand E1 gone, Tier-2 ≤1u,
  drawlog-792; wext used descriptively only). If the male visual gate misses: the pre-registered
  Dolphin+milo-trace single-bone capture.
- **RndText font-material color (engine):** make the glyph shader honor font-material color
  (UILabel focus-state darkening) — unblocks `RB3_ROWFIX` Part B → then flip ROWFIX (A+B
  together) + the same pass for partdiff GUITAR.
- Carried: sidebar backing quad (polish), loader sufficient-fix, WHITE real-lever, 4→8 lights,
  W2.4 BandPatchMesh, un-translated list-highlight focus anim (ROWFIX root, asset-side option).

## Wave 16 results (2026-07-07, run `wf_61997c73-0c8`, 2 agents)

**ROWFIX FLIPPED (defaults now ELEVEN) and the hands adjudicated cell was measured — REFUTED by
the decisive VISUAL gate despite passing every numeric gate, closing the band-side offset-bake
class at 8 cells and pointing conclusively at a true engine reskin (verts+WEIGHTS) or closure.**
Engine → `51640ff` (regen 361 clean).

| Item | Status | Highlights |
|---|---|---|
| HANDS-FIX | ❌ **REFUTED BY VISUAL GATE (8th/final cell)** | `RB3_HANDS_AUTHORED_REPOINT` implemented exactly per VERDICT §4-5: ALL numeric gates PASS — Tier-1 count(>5°)==0 BOTH genders (87.3→3.1° male, 42.6→3.1° female — the A6 female gate PASSES, confound broken), gloves improved, Tier-2 ≤1u, provenance clean (A4), 792 byte-identical. **But matched-frame E1 FAILS decisively: flag-ON TEARS multi-bone finger blends into spike-fans** (burst_08 strand-shred, burst_12 triangular fan — coordinator eyes confirm, worse than the coherent flag-OFF "ceiling hand"). ROOT CAUSE (vindicates SKEL Seam-B): **the authored verts encode SHARED-bind INTER-bone geometry** — Tier-1 is a static per-bone check, structurally blind to inter-bone animated tearing; NO repoint/offset choice can fix vert-encoded inter-bone geometry. Band-side offset-bake class EXHAUSTED (8 measured cells). Dolphin fallback correctly assessed mis-targeted for THIS failure (single-bone WorldXfm can't see a multi-bone tear) → the Wave-17 probe spec is refined to **two-adjacent-bone RELATIVE-pose comparison**. Genuine remaining option: engine per-member TRUE reskin (verts+weights re-derived in the own basis — distinct from W14's refuted positions-only re-pose) or closure with RB3_NO_SKIN_CLAMP as the shipped mitigation. |
| W4.4-TEXTCOLOR | ✅ **READY_FOR_FLIP → FLIPPED default-ON** | Mechanism refined once more: the focused row draws with a NULL provider color, so the old Part-B guard skipped it. Two rb3-side fixes folded into `RB3_ROWFIX`: UIListSlot supplies a dark UIColor for the highlight element when the provider returns null; UILabel propagates the override to the ALT font material (title + italic artist both darken). Gates: song_select directional PASS (solid bright bar fill=179, dark strokes=74, contrast 2.41), hub 2.20 unchanged, partdiff no-op (already correct polarity), SETLISTS red-band 0%, 792 PASS, Wave-7 labels + W4.2 floor unaffected. **Coordinator E1 PASS (dark-on-yellow focused row, retail polarity) → default-ON, opt-out `RB3_ROWFIX_OFF`.** |

**Coordinator actions:** ROWFIX flip (E1 both arms); hands refutation E1-confirmed and archived;
regen (`51640ff`, 361 clean); 792 + lineup PASS on flipped defaults; pin `84ccb9e` → `51640ff`.
**Defaults now ELEVEN.** Wave 17+ = the retrospective roadmap
(`RETROSPECTIVE/ROADMAP.md` + per-item plans under `RETROSPECTIVE/plans/`).

## Wave 17 results (2026-07-07→08, the INSTRUMENT wave — full roadmap, one lane per item)

**All six roadmap items delivered; zero default flips (instrument wave, as designed); the hands
saga got external ground truth for the first time and its Wave-16 "genuine fix is a reskin"
conclusion was itself dissolved.** Engine `51640ff` → `49ca0d6` (single close-out bump; regen
372 clean). Ran through a mid-wave duplicate-workflow collision (a killed run's lanes survived
as live orphans; both self-detected via checkpoints, one escalated and yielded — the checkpoint
protocol's first real fire drill, passed) and a wedged workflow (finished via continuation
agents). Close-out review: `WAVE17_CLOSEOUT_REVIEW.md` (`a11b22a0`) — substance survived; F1-F8
record-consistency fixes applied at close-out. Disposition note (F8): workflow `wf_a6d64a07-446`
wedged mid-retry-storm and is permanently paused; its lanes finished as standalone agents.

| Item | Status | Highlights |
|---|---|---|
| R1 Dolphin ground truth (D→D2→D3→D4) | ✅ **GO — the instrument exists** | Bare-DOL boot fails (apploader skipped); retail wbfs boots but layout ≠ map. **Owner's DOL-swap idea = the unlock (D2, `7872bd35`):** Bank-8 debug DOL+SEL swapped into the wit-extracted retail disc; real blocker was the production apploader's 0x80900000 section limit → 1-instruction patch (dev-mode branch); §6.1 ARK risk didn't materialize. 992 map-valid CharBones; guest RAM via `/proc/PID/fd` memfd. D3 (`93e0a37c`) built + validated the join (convention 0.133°, red-team 168.8°, own-bones pointer-proven). D4 (`176480e9`) delivered the FLOOR table — thumbs+anchor <0.06°, middle/ring 14-41°… |
| R5 hands endgame (Fable adjudication) | ✅ **VERDICT C (`fd2f83ac`) — D4's headline DISSOLVED** | Adversarial re-derivation from the raw sweep: different clips per male member; ONE scalar curl parameter (corr 1.0000); Wii settled pose below both sampled curl intervals; slot1's relaxed-region clip floors the same segments at 0.12-0.65°. **The 14-41° "survivor" = clip curl-coverage, NOT the vert-encoded basis mechanism; native skeleton STATICS externally exonerated ≤0.65° on all cleanly comparable channels.** Pre-registered branch table → Wave 18 is a script lookup (GT-A palette forensics prior / GT-B anim-basis / GT-C decode / GT-D closure pre-authorized). §8 standing corrections: Wave-16's "genuine fix is an engine reskin" line is a banned citation; `RB3_NO_SKIN_CLAMP` does NOT touch band hands; bone-world comparisons are structurally blind to offset-class arms → palette/skinned-output gates (G5′) + matched-frame E1 mandatory. **Lane D5 (articulated Wii capture, live clip+frame key) IN FLIGHT.** |
| R2 skinning fixtures + oracle validation | ✅ (`363542e7`, engine `04651e1`) | Palette probe landed FIRST (A1); harness + Suite C committed after finding the prior instance left them uncommitted with a dangling CMake reference. Prior captures were provenance-broken (NO-GO honestly declared); clean A/B recapture reproduces the Wave-15/16 story on REAL data: default arm IS the ceiling (Tier-1 87.3°/42.6° seen), repoint arm IS the torn blend — known-GOOD = body meshes. `M_BlendSpread` VALID 6.95× zero-overlap; rb3-tests 114/0/8-skip. F3 debt: re-capture good-body (legacy provenance, repeated frame) before it backs G5′. |
| R3 uidump + drawlog provenance | ✅ (engine `753ed20`, rb3 ×5 commits) | `RB3_DRAWLOG_PROV` sidecar + `/api/uidump` + ROI killer query (`uidump_query.py`). Validation by RETRODICTION from the shipped build: W14 red-band = one query pair (LoadOp Clear→3.45% red vs Load→0.12%, same quad); ROWFIX main-vs-alt split prints directly (0.18 dark vs 0.75 light). 792 byte-identical prov-off. v1 gap (top Wave-18 UI follow-up): authored-walk misses instanced list rows + overshell HUD (draw-side capture covers them). |
| R4 loader determinism + ledger | ✅ **PRIMARY 10/10** (merge `65c5092e`) | Attribution refuted the inherited guess (Wind fixed-count; real divergents: `RndParticleSys::InitParticle` dominant + `CamShot::Shake` + `CreateParticles` + `CharEyes::NextLook` + `RandomGroupSeq` boot). `RB3LoadDetStream` per-tag isolation → ON-arm stream-position spread 0 (10/10, contention+jitter) vs OFF 7179 fail-red; G2 flag-OFF inert; G3 Wii-match untouched. Classjson row → PASS-PRIMARY (stays opt-in; flag-ON gameplay re-golden precondition recorded). **The Wave-10 visual-gate noise floor is dead — WHITE re-grade + wash co-sampling unblocked.** |
| R6 process lints → standing template | ✅ (`ee3a4dc8` + `a55c53b4`) | `KICKOFF_TEMPLATE.md` with the ten §4 lints as a mandatory pre-dispatch checklist + the owner-directed close-out section (post-wave Fable review → docs → findings summary to owner). This close-out is its first execution. |

**Coordinator actions:** collision reaped (kept the instance with committed state per lane);
R4 branch merged; classjson regen ×2 (372 clean; R4 PASS-PRIMARY + F1 row amendment `49ca0d6`);
pin `51640ff`→`49ca0d6`; gates on final pin: drawlog 792 canonical PASS, lineup PASS, rb3-tests
114/0; F1-F5 corrections applied (D4/STATUS correction blocks, ROADMAP R5 row, LEDGER axes-v2
marker, superseded first-instance evidence deleted in favor of committed aggregates).

### Wave 18 menu (from `WAVE17_CLOSEOUT_REVIEW.md`)

- **Flagship: D5 branch-table execution** — D5's articulated capture lands → run the
  pre-registered branch (GT-A palette forensics prior, substrate = R2 palette dumps; G-D5-1..4
  + G5′ as written) + its funded follow-on (fix per branch letter).
- **R4 M4 cash-in:** WHITE re-grade + wash per-FX co-sampling — charter must embed the
  ledger-PASS validity precondition (F6).
- **R2 net-tightening:** engine-emitted Tier-1 field in `RB3_PALETTE_DUMP` (flips VerdictTable
  SKIP→real) + good-body re-capture (F3) + a cheap middle/ring curl regression assertion.
- **Optional: R3 v1 walk gap** (instanced rows + overshell HUD authored-side join).
- **Deferred:** 4→8 lights; W2.4 BandPatchMesh (twice-burned, same blend machinery as the hands
  endgame — do not run concurrently with the D5 branch); web-build confirmation rides as a tail
  gate on the next flip wave, not a lane.

## Wave 18 results (2026-07-08, cash-in wave — 4 lanes + coordinator GT-D closure + E1 flip)

**THE HANDS SAGA IS CLOSED (GT-D, honest no-articulated-ground-truth label) and its mitigation
SHIPPED — defaults now TWELVE.** Engine `49ca0d6` → `beb89e5`. Close-out review:
`WAVE18_CLOSEOUT_REVIEW.md` (`a9694de4`) — "the wave's three big calls stand"; F1-F5
corrections applied at close-out (STATUS/CLOSURE addenda + registry pass).

| Item | Status | Highlights |
|---|---|---|
| V — VISCAP | ✅ instrument / ❌ capture → **exhaustion per A1** | The visually-guided rig (Xvfb + Vulkan + screenshot-driven nav + guitar-ext Wiimote) SOLVED D5's wall — cleared the profile flow by sight, reached live gameplay on 2 songs. Substrate wall proven: **the Bank-8 debug patched boot never animates band CharBones** (0/992 worlds move in confirmed-live gameplay; `CharClipDrivers=0` everywhere; reads proven live). G-D5-1 fails everywhere → no branch letter assignable. Branch-table classifier committed for any future capture. |
| Coordinator — **GT-D CLOSURE** | ✅ (`CLOSURE.md`, `5d1cc39f` + post-flip addendum) | Closure-honesty condition genuinely met (blind AND sighted routes exhausted). Statics externally exonerated ≤0.65°; residual statement clamp-corrected; reopen condition pre-registered (CharClipDrivers=0 root-cause — parked, per review "uncosted substrate archaeology"). NEW backlog key **FOREARM-FLOAT**: the persistent top-center floating forearm-family structure (unchanged OFF→ON) + drummer E1 coverage gap — recorded so they are NOT misfiled against the closed family. |
| M — R5-MITTEN → **FLIPPED default-ON** | ✅ (engine `4b8a809`, flip `403ff00`, opt-out `RB3_HANDS_MITTEN_OFF`) | Render-side hands-scoped palette blend toward wrist-rigid past a 45°→90° rotation-to-wrist ramp (16.4% of finger-draws, extreme-pose tail only; α=0 below 45° → coherent frames untouched by construction). **Coordinator E1 on burst_08/12/45 OFF/ON pairs: finger spike-fans collapse to attached hands, both genders.** Flag-OFF byte-identical (792). Honest class: workaround. |
| W — R4-M4 WHITE/wash | ✅ verdict with corrections | **WHITE = VOID → HELD substrate-blocked, with numbers** (A2 void semantics honored — the harness refused): OFF-arm ledger 1/10, ~+15/16 gRand draws attributed to 4 venue-path consumers R4-M1 missed (CharClipDriver, WorldCrowd::OnIterateFrac — Fisher-Yates PROVEN live —, CharInterest/LightPresetManager ±1); ON-arm **10/10 stream-matched yet hi_frac 9.5→65.4** → WHITE's driver is a NON-gRand frame-timing axis (owner: W0.3d part-b, scoped per review F9 to frame-assignment timing). `RB3_LOADDET_SEED` subordinated. **F1 correction: the wash co-sampler's "VALIDATED, AUC 0.000, WHITE=particle-lulls" claim is WITHDRAWN** (per-burst-stale join + tie-handling artifact; midrank AUC ≈0.32) — instrument BUILT, validation FAILED, v2 must fix the join + midranks. `loaddet_gate.py --grade-logs` landed; two Wave-10 capture confounds fixed. |
| N — R2-NET | ✅ (engine `e69a35f`, rb3 `10b7caea`) | Engine-emitted Tier-1 field (the `:4820-4841` xcheck quantity via a first-seen rest cache; header-key-only format) → **VerdictTable SKIP → real measured PASS** reproducing Wave-15 exactly (0.06°/3.13° male, 28.88°/28.92° female); the offline analog drifts to 62° under articulation. FINDING: `SHARD_GUARD_OFF` gates the whole diagnostic block incl. the palette dump (why arm-w was never dump-reproducible). Good-body re-captured (md5-distinct shots; broad-selector dump I/O was the black-frame cause). Curl-envelope assertion landed. rb3-tests 116/0. |

**Coordinator actions:** GT-D closure executed + post-flip addendum; E1 mitten flip (twelfth
default); F1-F5 review corrections (W STATUS corrections block, CLOSURE addendum, F4 registry
pass incl. companion-knob classification); pin `49ca0d6`→`beb89e5` (three engine landings:
Tier-1 field, mitten+flip, registry); regen 377 clean; gates on final pin: drawlog 792
canonical PASS, lineup PASS, rb3-tests 116/0. Cross-link (review F6): Lane M's 85%-pixel
cross-process control corroborates Lane W's stream-matched render-timeline variance — same
axis, W0.3d part-b.

### Wave 19 menu (from `WAVE18_CLOSEOUT_REVIEW.md`)

- **PRIMARY: venue-path consumer isolation** (4 named sites) → makes the WHITE precondition
  satisfiable → **re-run the WHITE re-grade** on the then-clean ledger.
- **W0.3d part-b, scoped to the frame-assignment-timing axis** (the named WHITE driver; R4's
  ledger `order` axis is already 10/10 — do not confuse), with the wash co-sampler v2 join-fix
  (per-frame join + midrank AUC) folded in.
- **N-tail:** bad-torn tier1 golden re-capture + any registry stragglers.
- **R3 v1 walk gap** (instanced rows + overshell authored-side join).
- Conditional: W2.4 BandPatchMesh (ONLY under the memory's re-land clauses: patch-bearing
  lineup + reviewer-judged wide frames); 4→8 lights; web-build confirmation tail gate.
- Parked (review): CharClipDrivers=0 root-cause — remains the closure's reopen route, not a
  charter.

## Wave 19 results (2026-07-08, gen-2 instrument wave — 9-agent workflow: per tool Opus plan → Fable review → Opus implement)

**All three OPTIONS §6 tools SHIPPED (default-OFF instruments, no flips) and the WHITE
re-grade precondition is now satisfiable.** Engine `beb89e5` → `6e6387c` (regen 379 clean).
Close-out review `WAVE19_CLOSEOUT_REVIEW.md` (`2b2f0bb7`): **ACCEPT** — all gates real,
fail-reds fired; F1-F4 wording errata appended to the lane STATUS docs at close-out.

| Tool | Status | Highlights |
|---|---|---|
| §6 T1 — frame-timeline tracer + wash v2 | ✅ (rb3 ×6, engine `ce22beb`) | `RB3_LOADDET_TIMELINE` markers (LW-1 ReadDone arrival + songMs from the HTTP poll site per A3 ruling) + `loaddet_gate.py --timeline` with frameAssign/songClock/emitTimeline axes (PIE-stable keys 61/61 across ASLR; R4 `order_sig` byte-untouched). **Wash co-sampler v2** (per-frame join + midrank AUC) proved its fail-red by REFUSING the Wave-18 F1 degenerate dataset (verdict DEGENERATE). **Attribution output (errata'd wording): the frame-assignment residual exists WITHOUT injected jitter** (control-arm divergence at JITTER=0) — ambient thread actors drive it; jitter proven not-necessary (not proven no-effect; n=1/dose). This routes T3: pin ambient actors; jitter-reproduction = NO-GO route. |
| §6 T2 — world-cam ROI provenance | ✅ (engine `ad01ca6`+`515f617`, rb3 ×5) | `RecordDrawProv` skinned-pose branch: rectKind=3 boneRects from re-derived bone worlds (the review-A1 route — no palette tap), owner scope hook, `uidump_query.py --roi` with bones_in_roi. 306/306 skinned draws localized, 0 sphere fallbacks; G1 contrast real (RED sphere arm provably mislocates 302 rects); flag-off 792 byte-identical + Character.o Wii-inert. **M5 production smoke NAMED FOREARM-FLOAT: player3's right forearm/hand chain** (gloves_resource + clearcoat sleeve; bone_R-foreArm/foreTwist1/2/hand; boneFallback=0). Errata F4: the fix charter's FIRST discriminator = own-vs-shared skeleton binding (outfit-mesh RebindOutfitBones family) before any pose investigation. |
| §6.5 W-ISO — venue consumer isolation + capture lints | ✅ (rb3 ×4, zero engine) | 4 enclosing-function guards at the review-A4 sites (LightPresetManager :294-covering scope; Crowd scoped to OnIterateFrac only; zero Rand.cpp edits per A5) + NEW `capture_lints.py` (selftest 4/4) wired into `white_regrade.py`. EXIT: PREGUARD 29/30 RED → guarded 30/30 GREEN; R-A clean (no fifth consumer, divergent boot attributes exactly to guarded sites). Errata F2: the deterministic redirect is the load-bearing proof (N=30 corroborates); divergence rates are environment-specific — Wave-20 gates on the GUARDED build, mechanism-backed. G3 delta 0.0 all touched units; seam-OFF inert. **The Wave-18 WHITE VOID precondition is now satisfiable.** |

**Coordinator actions:** review-seeded plan checkpoints (the pre-dispatch WAVE19_REVIEW
`fb1f00e5` landed 1 min after workflow launch — its A1/A2 spec corrections reached the plan
agents via their check-first checkpoints); regen ×1; pin `beb89e5`→`6e6387c` (`bd5a2eab`);
gates on final pin: drawlog 792 canonical PASS, lineup PASS, rb3-tests 116/0; F1-F4 errata
appended. Cross-lane `rb3_http_handlers.cpp` co-edit verified no-clobber. TWELVE defaults
unchanged.

### Wave 20 menu (from `WAVE19_CLOSEOUT_REVIEW.md`)

- **WHITE re-grade cash-in — GO** (prereq: fix `r4m4_capture.multi_capture` sweep to ≥5
  distinct songMs; entry gate on the GUARDED build, mechanism-backed, N≥30 discipline +
  capture_lints).
- **T3 — GO in PINNING mode / NO-GO for jitter-reproduction**: residual actors = non-loader
  threads (HTTP-poll songMs + audio clock; ThreadCall drains inline under the seam, native
  AsyncFile synchronous). Gate = all-3-axes collapse at N≥6; honest NO-GO exit if songClock
  proves wall-clock-fundamental.
- **FOREARM-FLOAT fix charter** — binding-check FIRST (own-vs-shared skeleton for player3's
  right-arm outfit meshes), then pose only if binding exonerated.
- Half-lanes: N-tail bad-torn tier1 recapture; R3 v1 walk gap.
- Conditional filler only: W2.4 BandPatchMesh (memory re-land clauses), 4→8 lights, web-build
  tail gate.

## Wave 20 — HANDS ROOT-CAUSE (owner redirect: "figure out the actual root cause — is it a bug in decomp code?")

Owner redirected the Wave-20 menu below into a root-cause zoom-out on the hands flagship
bug. Three AUDIT-ONLY Opus lanes (kickoff `67187eed`, pre-dispatch review `7538830c`
DISPATCH-WITH-AMENDMENTS A1–A11, synthesis `145150d6`, close-out review `d1e3faf0`
ACCEPT-WITH-ERRATA E1–E12, errata `6d49499e`). Zero fixes/flips/pin-bumps; probes
default-OFF; drawlog-792 green; GT-D closure untouched (static/load-time only).

| Lane | Result |
|---|---|
| **W — Wii load-truth (Dolphin patched-disc)** | Built a new `WiiMesh`-binding reader (`milo-trace tools/wii_mesh_binding.py`). **Wii hand meshes bind OWN_MEMBER** (per-member `skeleton_unshared.milo`); SHARED_ROOT=0 in every reachable state; determinism-checked, pointer-deref only. Walled: gameplay LOADING wall → only 6/38 hand bones bound (member 0), no female member (Guest lineup all-male) — the *class* (OWN vs SHARED) is unambiguous (parse-time-fixed), per-slot finger/female basis is not in hand. |
| **N — native load-path trace** | On today's build, hand bones bind the **SHARED magnet** at parse-time name resolution. The three remap branches COUNTED: `:4202` sBoneMergeDir + `:4182` sCharSharedDir = **0 fires** shipped (VERDICT §1's "never-firing" is now a counted zero). **Our HX_NATIVE white-texture `FilterSubdir` shim (kMerge→kReplace) is the direct cause** — it kReplaces every on-disk resource subdir incl. skeleton-bearing `char_shared.milo`, and kReplace'd subdirs are never iterated through `Filter`. Shim-OFF: br2 fires **31,488/member**, binding flips to per-member (matches Wii). Supersedes the 2026-06-06 "shim-off didn't change binding" record for hand meshes (that arm measured torso-at-draw). |
| **D — decomp-fidelity audit** | **The chain is FAITHFUL — zero semantic bugs.** `BandCharacter::Filter` (95.6%), `ObjectDir::PreLoad` (100%, the kInlineCached read), LoadSubDir/PostLoadInlined/ReplaceRefs/OnInstallFilter/FileMerger — every sub-100% diff is regalloc/scheduling/SDA-reloc noise; the sBoneMergeDir remap branch is structurally present + correct. Answers "is it a bug in decomp code?" → **No.** |

**Root cause (synthesis, errata-corrected):**
- **Layer 1 (PROVEN, durable):** native binds the shared male-bind magnet because our own
  white-texture shim suppresses the retail per-member bone remap (counted 0 vs 31,488); the
  decomp is faithful; Wii binds per-member. A **native-introduced load-path regression**,
  not a decomp defect and not the origin of the fling (which predates the shim).
- **Layer 2 (necessity PROVEN, mechanism OPEN):** per-member binding alone still flings, so
  it is necessary but not sufficient. WHICH dead composition the shim-off arm reproduces
  (`inv(R)·L_own` default vs `inv(B)·L_own` 8th-cell) is undetermined (draw-time offsets not
  dumped). The "gender-posed rest basis is the missing ingredient" is a **leading candidate
  (L2-a)**, not proven — arm S's male `own`≈B (3.1°) is counter-evidence for males; only the
  female ~29° gender-gap is committed. L2-a vs L2-b (blend-tear) unseparated.

**Wave-21 fix charter:** two-part load-path change, **landable only together** (Layer-1-alone
flings worse): (1) scope the shim to leave `char_shared.milo` at retail kMerge (restore the
remap) OR add a targeted post-merge re-point — **+ a mandatory char-texture-integrity gate**
(E11: restoring kMerge may reintroduce white textures); (2) pose the per-member skeleton to
the gender bind — conditioned on a step-1 **torso-vs-hands discriminator** (why does
`RebindOutfitBonesToOwnSkeleton` work for torso but hand-to-own tears?) that decides L2-a vs
L2-b and must explain arm S's male null. The 8 dead cells + reskin stay banned (a Layer-1-only
landing = the dead class by the back door). Substrate note: the Wii finger/female basis to
fully separate L2-a/L2-b is walled on the headless Dolphin rig — Wave-21's discriminator is
native-side.

### Wave 21 menu
- **HANDS Layer-1+Layer-2 fix (FLAGSHIP)** — the two-part charter above; step-1 torso-vs-hands
  discriminator FIRST (native-side, no Wii articulation needed), then the shim-scope +
  gender-pose fix gated on G-FIX-E1 (both genders, ceiling-hand + spike-fan gone) + texture
  integrity + drawlog-792 + batch_objdiff baseline.
- **Deferred from the Wave-20 menu (owner redirect):** WHITE re-grade cash-in (prereq: fix
  `r4m4_capture.multi_capture` ≥5 distinct songMs, entry on the GUARDED build); T3 pinning
  mode (all-3-axes collapse at N≥6, honest NO-GO if songClock is wall-clock-fundamental);
  FOREARM-FLOAT fix (binding-check first, player3 right-arm outfit meshes).
- Half-lanes: N-tail bad-torn tier1 recapture; R3 v1 walk gap.

## Wave 21 — HANDS FLAGSHIP FIX ATTEMPT → L2-b R5-WALL (honest, no ship)

The two-part load-path fix from the Wave-20 charter. Kickoff `c4ed750e`→acceptance `664d5014`
(pre-review `12b4c389` DISPATCH-WITH-AMENDMENTS A1-A10, reframing the hope from "different
instance" — refuted — to "different rebake/clamp draw regime"), synthesis-by-lanes, close-out
review `4dff4820` ACCEPT-WITH-ERRATA (ERR-1..8), errata applied. Both Opus lanes reached the
**third pre-authorized outcome: L2-b for BOTH genders = the R5-walled animated tear.**

| Lane | Result |
|---|---|
| **FIX — the 2-part fix** | Part 1 `RB3_HANDS_BINDFIX` (default-OFF, `BandCharacter.cpp` `24c2ac1c`): scopes the white-texture shim so char_shared.milo + outfit resources take retail kMerge (restoring the sBoneMergeDir remap, br2=31,488/member) while colorpalettes.milo keeps kReplace. Gates: crash 0/4-members-boot PASS, texture-integrity PASS (0 white cascade), drawlog-792 PASS, batch_objdiff==baseline PASS. **But hand meshes bind at PARSE (upstream of the merge remap) so BINDFIX draw output == flag-OFF byte-for-byte** — no hand-visual change. G-FIX-E1 FAIL-to-fix (expected). Part 2 (gender-pose) correctly NOT dispatched (DISCRIM read L2-b). |
| **DISCRIM — the L2-a/L2-b decider** | Gender-split, draw-frame, mitten-controlled. **Both genders L2-b:** the only coherent-basis regime (Tier-1 3.1° count=0 every draw frame, both genders — the female 28.9° gap dissolves) is the BANNED 8th cell, and it is VISUALLY TORN. The tear is a mesh-shell/weight-blend divergence between coherently-attached joints (joints ≈0.1u, shell shears) — invisible to every bone-level metric (males 15% frames ≤8.4u, females 0% >2u yet equally torn) = the R5 §0-item-4 signature. Torso fail-red PASS. arm-S male 3.1° rest-null explained (rest-coherent, defect is animated). |

**Verdict:** hands are **terminal — doubly walled** (R5 GT-D + this native-side gender-split
mechanism decision). No non-banned, non-walled lever remains: every reachable coherent-basis
regime is measured torn; a parse-time un-share lands in the same torn family and is
unverifiable without articulated Wii GT (CharClipDrivers=0); the fallback post-merge re-point
is refuted by measurement (ERR-7). Mitten (default-ON) is the answer. `RB3_HANDS_BINDFIX` kept
default-OFF as a documented partial (real load-topology+texture restore, substrate for any
future un-share). **Key record correction (ERR-1/E13): the Wave-20 "restore per-member binding
via the merge remap" claim is CONTRADICTED — hands bind at parse; the remap is a no-op for
them. The Layer-1 shim-suppression finding stands; only its causal link to hand binding is
withdrawn.** Engine pin `6e6387c`→`be70ca8` (flag classification only). TWELVE defaults ON.

### Wave 22 menu (from `WAVE21_CLOSEOUT_REVIEW.md` Q7)
- **Hands: CLOSED — do not re-charter.** Mitten is the answer. Optional falsifier only =
  CLOSURE follow-up #2 (root-cause `CharClipDrivers=0` → articulated capture); do NOT spend a
  lane by default.
- **FOREARM-FLOAT fix (recommended first)** — player3 right-arm outfit meshes; binding-check
  first (own-vs-shared, the RebindOutfitBonesToOwnSkeleton family); absorb the Wave-21
  mitten-OFF "arguably worse" observations if they prove forearm-level.
- **WHITE re-grade cash-in** (prereq: fix `r4m4_capture.multi_capture` ≥5 distinct songMs;
  entry on the GUARDED build, mechanism-backed, N≥30 + capture_lints).
- **T3 pinning mode** (all-3-timeline-axes collapse at N≥6; honest NO-GO if songClock is
  wall-clock-fundamental).
- Half-lanes: N-tail bad-torn tier1 recapture; R3 v1 walk gap.

## Wave 22 — VISUAL PUSH: HUD score-position FIXED (13th default) ∥ FOREARM narrowed ∥ Wave-23 menu

Owner: *"keep pushing through the visual bugs we can find."* Kickoff `26893122`→acceptance
`81a81de5` (pre-review `08d62a96` A1-A9), close-out review `6c972e88` ACCEPT-WITH-ERRATA
(ERR-1..5, flip ENDORSED). Engine pin `be70ca8`→`4a72845` (flags), census 386.

| Lane | Result |
|---|---|
| **HUD — score/star (WIN)** | **Score-HUD mid-screen → top-right FIXED + FLIPPED default-ON (13th default, `RB3_HUD_SCOREBOARD_TOPRIGHT`, opt-out `_OFF`).** Root cause: our own default-ON K9 (`RB3_APPLY_HANDLER_FIX_OFF`) zeroed the scoreboard's right.grp.x on a false "retail SP = top-center" premise; the Wii GT (`yt_qRagnZCIMzk_gameplay_*`) is top-RIGHT. Fix re-scopes K9 (single-player-gated → no MP regression; ConfigureTracks objdiff 100% HX_NATIVE-only; rb3-tests 116/0). Coordinator E1 PASS (score pill 88% width, `d15484f1`). Star-meter "never fills" = FALSE ALARM (fills correctly, 3 pips at 3.4★; the 07-02 diff caught it <1.0★). |
| **FOREARM — narrowed, deferred (MED)** | Binding-first discriminator EXONERATED binding (own==bound at draw 75/75) → it's a POSE bug: `bone_R-foreArm` driven to world y≈+182 on in-song band-closeup camera-cut frames, bilateral (not a transpose), no valid repoint target (the member-subtree foreArm/hand IS the mis-posed magnet). Code reverted, tree==HEAD (`669c244b` docs-only). **ERR-2 re-rated LOW→MED** (recurs on camera cuts, visible in HUD + flip captures = the exploded arm/hand spike-fans). Handoff: Wave-23 pose-driver discovery lane. |
| **SWEEP — Wave-23 menu** | Ranked discovery (scripts+docs only): **S1** hub mid-street crowd absent (MED, ROI-confirmed), **S2** hub over-bright grade (MED), **S3** part/diff picker cramped, **S4** player1 avatar edge-crop, **S5** now-bar combo glow. Refuted 07-02 #6 stray-red-bar (= subway backdrop). Confirmed shipped: hub-ticker/song_select-overlap/album-art. |

### Wave 23 menu (from `WAVE22_CLOSEOUT_REVIEW.md` Q6 ordering)
1. **S2 hub over-bright grade discriminator FIRST** (cheapest; A/B `RB3_PP_OFF`/`RB3_UI_POST_GRADE_OFF` + the shipped menu-lighting default; re-scopes S4).
2. **S1 hub mid-street crowd absent** (crowd-rebind family first; dump hub `.milo` owners).
3. **FOREARM pose-driver** — DISCOVERY-scoped lane (upgraded from parked by ERR-2), HARD STOP before fix code, binding closed; find what poses `bone_R-foreArm` high on camera cuts (walk-on/count-in freeze class `67e87ae1`?).
4. **S5** now-bar/combo glow (needs driven-combo capture).
5. Defer S3 (no GT) / S4 (authored, C8+hands overlap).

## Wave 23 — VISUAL PUSH cont.: 2 discriminators + 1 discovery (no fixes shipped, 2 engine bugs teed up)

Owner: *"keep pushing."* Kickoff `97f50c4a`→acceptance `44bfce23` (pre-review `1cdff0ec` A1-A9,
re-anchored GRADE mechanism + CROWD target), close-out review `45f81795`
(GRADE/CROWD ACCEPT-WITH-ERRATA, FOREARM REVISE). Engine pin `4a72845`→`694e1de` (probe flags),
census 389. Discriminator-first discipline paid off — no speculative fixes; two engine bugs
precisely located + handed to Wave 24. (FOREARM re-dispatched standalone after a workflow error.)

| Lane | Result |
|---|---|
| **GRADE — hub wash** | **(c) authored/no-fix.** Venue-light non-engagement REFUTED (`RB3_WASH_PROBE` engaged=1 every hub environ; venue-OFF is BRIGHTER). Wash = authored lit neon signs + camera-phase. Contrast 6.88:1 peak MATCHES wave-5's post-fix 6.8:1 (ERRATA-G1: prior "2× the 2.6:1" used the wrong pre-fix baseline) → holds. OPEN observation (ERRATA-G2): neon-plate relative gap 2.33 vs 0.98 on identical assets = an unexplained native rendering diff, deprioritized. No code (`3af48f67`). |
| **CROWD — hub vignette walkers absent** | **HANDED OFF (candidate root cause).** Re-anchored to `sv3_a.milo` (NOT WorldCrowd, NOT main_hub). The 8 crowd Characters LOAD/show/pose/poll but 0 body-mesh DRAWS + `gAltRev<3`. ERRATA-C1: the "0 verts" metric read `mVerts` only — HX_NATIVE compressed meshes keep `mVerts` empty BY DESIGN (`mNumCompressedVerts`), no positive control → root cause is CANDIDATE. Wave-24 step 0 = re-census w/ `mNumCompressedVerts` + band control. Read-only recon func, no fix (`0fad6137`). |
| **FOREARM — pose-driver (DISCOVERY)** | **DRIVER CANDIDATE (headline retracted).** Named suspect = band vignette clips via `BandRetargetVignette` IK (`sIkfs` verified onto the flinging bones). BUT ERRATA-F1: the y>50 probe also catches legit stage-placement; **upperArm flung MORE than foreArm (29,559 vs 28,758)** → NOT forearm-localized, NOT "persistent"; W22 transition-only framing RESTORED. Un-followed LEAD: crossed member↔clip pairings fling, matched pairing settles sane. Probe-only edit (`%30`→event-triggered, HX_NATIVE inert, drawlog-792 PASS, `94dfe32e`). |

### Wave 24 menu (from `WAVE23_CLOSEOUT_REVIEW.md` Q6)
1. **FOREARM-RECON FIRST** (probe-only, ~1 day): same-frame pelvis+upperArm+foreArm+hand ANATOMICAL
   trigger (child-parent distance > bone length), correlate a spike-fan screenshot to probe lines,
   split walk-on/steady/cut windows, chase the CROSSED-vs-MATCHED clip-pairing lead. THEN choose
   IK-not-constraining / clip-decode / transition-class. **Do NOT dispatch a BandRetargetVignette/
   BandIKEffector fix on W23 evidence.**
2. **CROWD-RECON → engine fix** (highest fix EV): re-census w/ `mNumCompressedVerts` + band control;
   if compressed=0 confirmed → scoped RndMesh `gAltRev<3` skinned-decode fix, MANDATORY gameplay
   WorldCrowd A/B (branch-scoped to keep the protected oracle hot path untouched).
3. **S5 combo-glow confirm** (small). 4. S3/S4 deferred/authored. Open: GRADE neon-plate residual.

## Wave 25 — FIX: FOREARM spike-fan FIXED+FLIPPED (14th default) ∥ CROWD root-caused to async load-merge (partial)

W24-RECON-confirmed bugs. Kickoff `7c791630`→acceptance `5a97db93` (pre-review `232b8cc7` A1-A9:
H-C sub-cause, G3 3-case table, CROWD leak-vector guardrail), close-out review `0809e6ef`
(FOREARM ACCEPT+FLIP YES, CROWD ACCEPT-WITH-ERRATA). Engine pin `e6b3c64`→`2088c68`, census 402.

| Lane | Result |
|---|---|
| **FOREARM — CharIKHand (WIN, 14th default)** | **Spike-fan FIXED + FLIPPED default-ON (`RB3_IK_REACH_CLAMP`, opt-out `_OFF`).** Discriminator REFUTED H-A/H-C (target resolves to the correct member bone) + reframed H-B. Mechanism: IK weight=1 while the target sits d=54-273u away vs arm reach ~20u → over-rotates upperArm → fan. Fix = graduated reach-guard on `mWorldDst` (no-op in reach / clamp to reach-sphere / neutralize beyond k·reach). Match-neutral (all 20 CharIKHand fns == baseline, HX_NATIVE). Gates: in-song upperArm ratio 1.15 (vs OFF 34-85), drawlog-792, rb3-tests 116/0. Coordinator E1 PASS (`96f18e62`). Q1 RESOLVED: 20u reach is CORRECT (no length bug); far targets = mis-posed instrument-prop bones (Wave-26 tail). |
| **CROWD — async load-merge (root-caused, partial)** | Recon theory REFUTED: the streetslomo clip DOES play + skin correctly for ~1.2s, then an **async load-merge at beat 2.4 (pollFrame 72) DESTROYS the playing clip** (`Replace(clip,NULL)`→`mFirst=NULL`) AND swaps `mClips` to a wrong sub-bank → `animating=0` forever. Same native async-interleaving class as the hands/load-order bugs. Partial `RB3_CROWD_CLIP_KEEP` (default-OFF, clipType=='crowd'-scoped, byte-identical #else, WorldCrowd A/B SAFE) recovers only bank-intact drivers — E-C2: ZERO of 8 as-observed → PROPHYLACTIC scaffolding for the W26 engine fix. Engine hand-off charter written (`b6a8980f`). |

### Wave 26 menu (from `WAVE25_CLOSEOUT_REVIEW.md` Q7)
1. **CROWD load-merge ENGINE fix (recon-first):** why does the sv3_a merge fire at beat 2.433 destroying the playing clip? PREFER suppress-duplicate-load or re-fire-play_clip over preservation surgery. Scoped away from the protected WorldCrowd oracle + proven-correct RndMesh loader. Acceptance = `animating>0` + 8 lit isolate figures + the deferred near-black material discriminator (isolate max-pixel vs 17/255) as follow-on.
2. **Instrument-prop target-bone POSING (reframed FOREARM tail, MED EV):** the far IK targets are prop bones mis-posed natively (strum pick +50u z, fret 98-216u, mic-stand below floor + reach=0). Fixing prop posing restores genuine in-song IK (clamp becomes a dormant safety net) + likely retires the ≤4.2 vignette residual + drumstick splay. Same async/posing family.
3. S5 combo-glow (third lane only if capacity).

## Wave 26 — recon-first: 3 hypotheses tested, 0 survived as charted (no visible fixes; 2 root causes pinned)

Kickoff `34811b9f`→acceptance `e56c8daa` (pre-review `3b402960` A1-A9), close-out review `055992be`
ACCEPT-WITH-ERRATA (E1-E7). Engine pin `2088c68`→`8d0e5b0`, census 408. Discriminator discipline:
no speculative fixes; the CROWD merge theory REFUTED, two deeper bugs precisely located.

| Lane | Result |
|---|---|
| **CROWD — A1 REFUTED, W27 handoff** | The W25 "async load-merge destroys the clip" theory is **WRONG** (FMERGE_PROBE: 182 merges, all band-wardrobe, ZERO crowd; CHARDRV_BT backtrace). Real kill = a **UI PANEL-UNLOAD teardown** on splash→main_hub (WorldDir::~WorldDir → CharClipSet dtor → Replace(clip,NULL), UIScreen.cpp:570); no merge, no bank swap (nclips 11→8 = same bank minus 5 deleted crowd clips); streetslomo_clips.milo never reloaded → RB3_CROWD_CLIP_KEEP can never fire. Kill site is ui/world (outside lane grant) → W27. Probes + E-C3 prune landed (`5b7aabc5`). **SUPERSEDES the Wave-25 CROWD row above (E1-E5).** |
| **PROP — clip-binding gap proven, partial** | Discriminator (IK_ROOTCMP same=1 refutes attach/proxy): the instrument TIP bones have a correctly-posed at-hand PARENT but the tip's static LocalXfm (~48-51u) is never driven by a clip natively → flings the IK target. `RB3_PROP_POSE` (default-OFF) redirects to the parent, drops target dist -44..-64%, but does NOT make the clamp dormant (E7: inferred mFinger re-projection, bypass-test in W27) + no visible arm change. Stays default-OFF scaffolding (`1498c400`). |
| **GLOW — no-fix, S5 CLOSED faithful** | The combo/now-bar Nx glow is ALREADY faithful at ≥4x (montage-verified vs GT). `RB3_SMASHER_HALO` doesn't touch it (the combo ring is a HUD/StreakMeter UI-cam element, not game.cam). S5 conflated 3 elements. Keep RB3_SMASHER_HALO OFF, no code (`7b9068fc`). |

**Close-out note (E6):** the coordinator's "unconditional crash-fix landed this wave" was a diff-hunk
MISREAD — the CharDriver::~CharDriver mBones-alias UAF guard is PRE-EXISTING (`65892986`, 2026-05-27),
shown as context. Ruled KEEP unconditional (real UAF, correct, Wii byte-identical). W26's only dtor
change is the E-C3 gCrowdKeep prune (inert). Default-ON count unchanged at 14.

### Wave 27 menu (from `WAVE26_CLOSEOUT_REVIEW.md` Q6 — ONE meaty lane)
1. **CROWD ui/world panel-residency (the real crowd-walkers repair):** BINDING STEP 0 = Wii ground
   truth — does main_hub RELOAD `streetslomo_clips.milo` or KEEP the vignette panel resident across
   the splash→hub transition? Choose the lever (keep-resident vs reload+re-fire `play_clip`) from
   that, not from a guess. Scoped to ui/world/vignette (NOT the protected WorldCrowd oracle). The
   deferred near-black material rides as the acceptance follow-on. Charter tractability: MEDIUM
   (shared ui/world blast radius — flag-gated + boot A/B).
2. **PROP parked** (probe-only; W27 tail if capacity: bind the prop clip tracks + bypass-test mFinger).
3. **No GLOW lane** (S5 closed).

## Wave 27 — sv3 residency PROVEN (charter premise refuted); W26 teardown mechanism REINSTATED w/ owner corrected; PROP E7 confirmed

Kickoff `6dc950ec`→acceptance `c0829acf` (pre-review `cf94e168` A1-A10: refcount-handshake
model correction, Wii-GT pre-pinned via the sv3 INTERSTITIAL, band3 grant), close-out review
`09cca9e8` ACCEPT-WITH-ERRATA both lanes (E1-E7; E1 headline-reversing). Engine pin
`8d0e5b0`→(close-out bump), census 408→410. Fourth CROWD narrative correction — canonical
record now per close-out Q1 (confidence HIGH, build drift RULED OUT — the W26↔W27 window is
docs + 5 semantically-inert micro decomp commits + classjson-only pin).

| Lane | Result |
|---|---|
| **CROWD — residency proven, mechanism reinstated (ERRATA E1-E5)** | STEP-0 discriminator PROVED `sv3_panel` is RESIDENT across splash→main_hub (interstitial→regular-panel refcount handshake `mLoadRefs` 1→2→1, faithful; A7 revisit cycle leak-free 1→2→1→0→1) — the W27 charter premise (ui/world residency lever) is REFUTED: the ui layer is already correct, no in-grant fix exists. BUT the lane's "zero teardown / clip ends naturally" substitute narrative was FALSIFIED at close-out by its own raw logs (E1): **seven `CHARDRV_REPLACE` kills of `crowd1-5.clp/clip` fire at beat 2.433** — the W26 `WorldDir::~WorldDir→CharClipSet dtor→Replace(clip,NULL)` mechanism REPRODUCES — sourced at the **FAITHFUL splash-side panel unload** (`splash_panel`+`sv8_panel` refs→0 in the kill frame, E2). Owner corrected: crowd clips/proxies/walk meshes are raw-string-present in `sv8_a.milo` (splash backdrop), not sv3_a's raw strings → W26 mis-attributed the destroyed clip set to sv3/streetslomo. Root cause reframed (E4): **clip-set ownership/binding divergence** (11-vs-8 same-named `clips` sets; do resident streetslomo drivers resolve against the splash-side copy?). E3: only 7 of 8 drivers ever Play (`crowd_female04` never triggered). E5: `mDefaultClip` is serialized-only — NULL may be faithful; W28 must log the serialized name first. Probes only (byte-inert, 7/7 fns 100% objdiff); no flag, no flips (`a1cf22f3`). |
| **PROP-PROBE — E7 CONFIRMED + binding gap enumerated (minor errata E6-E7)** | (a) mFinger-bypass A/B **CONFIRMS W26-E7**: with `RB3_PROP_POSE` on, bypassing the `CharIKHand::Poll` mFinger re-projection collapses hand-IK over-reach ~120-240u → ~21-25u (reach 20.3u); ALL grossly-unreachable skip fires vanish (strum 46→0), clamp degrades to marginal boundary = effectively dormant. The mFinger finger-compensation feedback IS the blocker (was only inferred). (b) prop-tip clip-track enumeration NEGATIVE: `bone_pick_strum`/`bone_[LR]-tip_*` carry constant LocalXfm while `bone_target_*` parents animate (behavioral inference, E7-errata softened). Real fix needs BOTH prop-tip track binding AND breaking the mFinger feedback. `RB3_PROP_FINGER_BYPASS` probe kept default-OFF; E7 env-parse fix confirmed present; weight-loop comment nit applied (`6a07cc42`). |

**Close-out rulings:** E-C2 (`RB3_CROWD_CLIP_KEEP` removal) **PARKED** — W27's removal
rationale falsified by E1; re-rule at W28 close-out. **drawlog-golden ambient-RED** (both
lanes concur: count=792 stable, 12-72 `field=world` crowd-pose divergences across
identical-binary reruns, clean-HEAD statistically identical): ruling = recalibrate the
per-name `fixedclock-residual.json` eps from N≥5 clean-tree runs (count/structural/non-world
stay strict); NO `--update` re-baseline, NO standing RED.

### Wave 28 menu (from `WAVE27_CLOSEOUT_REVIEW.md` Q6 — discriminator-first, checkpoint-before-fix BINDING)
1. **W28-CROWD-OWNER (MEDIUM):** STEP 0 (≤1 day, probes exist): (i) interleave the beat-2.433
   Replace BACKTRACE (`CHARDRV_BT`) with panel-unload markers → name the torn-down WorldDir
   owner directly; (ii) dump owner `Dir()` chains of the 5 played clips, BOTH `clips` sets
   (11 vs 8; does any driver's `mClips` swap at the kill?), and the 8 crowd char dirs;
   (iii) E5: serialized default-clip name at CharDriver load; (iv) **Wii-GT identity check:**
   are the `crowd_*` chars part of retail main_hub at all, or are the hub walkers
   streetslomo's OWN (differently-named) chars — if the latter, every wave since W23 has been
   measuring the SPLASH crowd. THEN one lever: (A) fix cross-panel clip resolution so
   `play_clip` resolves the resident copies (faithful-restoration carve-out class, A6-style
   drawlog ruling); or (B) the observed crowd is splash-owned and faithfully dies →
   re-charter acceptance around streetslomo's own walkers (fold in the deferred `verts=0` /
   near-black thread). NO fix code before the STEP-0 checkpoint (three consecutive
   supersessions).
2. *(optional tail if capacity)* **W28-PROP-FIX (MEDIUM):** bind/animate prop-tip clip
   tracks + redirect BEFORE the weight loop + break the mFinger feedback (collapse target
   proven by `RB3_PROP_FINGER_BYPASS`). Flag-gated default-OFF. Defer without guilt.
3. No third lane. GLOW closed; E-C2 re-rule at W28 close-out.

## Wave 28 — FIFTH (decisive, backtrace-anchored) CROWD narrative: the measured crowd was the SPLASH crowd; PROP real-fix pieces 1+2 landed (PARTIAL)

Kickoff `b3793f51`→acceptance `1b30bda8` (pre-review `350d3ebc` A1-A8: CHARDRV_CLIPSWAP/
DEFCLIP probes specified, lever-A layer corrected to load-order/ObjPtr/object-ref,
flag renamed CLIPBIND, raw-logs-as-deliverables + probe-count table). Close-out review
`348c8293` ACCEPT-WITH-ERRATA both lanes (E1-E6, all minor; every probe count and A/B
number independently recomputed from the committed raw gz logs — the A7 mechanics
demonstrably worked). Census 410→411 (RB3_PROP_POSE_FULL). No default flips (14 stand).

| Lane | Result |
|---|---|
| **CROWD-OWNER — Lever B re-charter ACCEPTED (E1-E3 minor)** | STEP-0 (all 4 discriminators checkpointed BEFORE any fix, NO fix code written) resolves the five-wave supersession chain with DIRECT evidence: (i) symbolized `CHARDRV_REPLACE_BT` names the kill owner — `UIScreen::UnloadPanels(splash_screen)` → `sv8_panel` WorldDir dtor → `cityscape_clips.milo` CharClipSet dtor (FAITHFUL splash teardown); (ii) unsampled `CHARDRV_CLIPSWAP`+PathName proves the 8 walker figures are SHARED proxies (`char/crowd/crowd_{male,female}0N.milo`) bound to sv8 **cityscape** clips during splash, then **correctly rebound** to the RESIDENT sv3 **streetslomo** set at beat 2.433 — NO binding divergence at any A3 layer; (iii) `CHARDRV_DEFCLIP`: `serialized=''` ×8 → mDefaultClip==NULL is FAITHFUL data (W27 E5 lever dead); (iv) identity: **the crowd measured since W23 is the SPLASH crowd faithfully dying**; the real hub walk (`playerN_f/m` in streetslomo_clips, E2: hypothesis pending W29's working-reference trace) is NEVER TRIGGERED natively (`PanelDir::Enter streetslomo_ao nTriggers=0`, zero PLAY after 2.433). Gap = world/vignette scene-trigger layer, NOT CharDriver/CharClip/ui. RECHARTER.md names the W29 acceptance target set (PLAY of playerN_* after 2.433 + sustained animating>0 with `mClips` PathName-asserted == streetslomo_clips — any census not pinning the set measures the wrong crowd). E1: C13_PROBE lead is CharCache band-member slots (name-collision risk, discriminate first). Probes only, Wii .o byte-identical; `RB3_HUB_CROWD_CLIPBIND` reserved-not-used, name released (`c6ef7795`). |
| **PROP-FIX — RB3_PROP_POSE_FULL landed default-OFF, outcome PARTIAL (E4-E6)** | Pieces 1+2 of the real prop-hand fix, CharIKHand-local: (1) break the mFinger re-projection feedback (folds W27's proven bypass into the fix); (2) redirect the target to its at-hand parent BEFORE the multi-target weight loop so weight and world agree. Flag-ON A/B (committed `analyze_prop_ab.py`, numbers reproduce exactly from raw gz): grossly-unreachable skip 209→0 (window-bounded per E5); strum + fret FULL PASS (skip=0 AND 0 dst>30u); right_hand (drummer floor-tom) **FAIL on the A8(ii) dst bar** (12-13 entries at ~32-33u, the deferred piece-3 residual) → label corrected fix-landed→**PARTIAL**. Guitarist closeup: IK spike-fan GONE, hand posed at the fretboard. Piece (3) (prop-tip clip-track binding) deferred per A8(i) arbitration with the exact site enumerated in PLAN.md — now unblocked for W29. E6 default-ON blockers recorded: piece (1) is globally scoped (vocalist-mic A/B required; undisclosed foot-chain change) — `RB3_PROP_FINGER_BYPASS` KEPT as the piece-1 isolator (`cec9d7e5`). |

**Close-out rulings (Q8):** **E-C2 `RB3_CROWD_CLIP_KEEP` → REMOVE in W29** (the kill it
guards is proven faithful; enabled it would zombie the splash crowd onto a destroyed set
and mask the trigger gap — pure cross-attribution hazard now). `RB3_PROP_FINGER_BYPASS`
KEPT until FULL default-flips. Coordinator process erratum (owned): the E4 misquote
originated in the coordinator's dispatch prompt — dispatch prompts must quote the
acceptance block verbatim, never paraphrase acceptance criteria.

### Wave 29 menu (from `WAVE28_CLOSEOUT_REVIEW.md` Q8 — discriminator-first + checkpoint-before-fix + A7 raw-log mechanics carried unchanged)
1. **W29-CROWD-TRIGGER (primary):** make the 8 hub walkers play `playerN_f/m` per the
   RECHARTER target set. STEP 0 (blocking): (i) trace the WORKING reference — what
   mechanism issues the beat-0 `play_clip crowd1-5` on the cityscape side (caller
   backtrace on `CharDriver::Play`, PathName the scene object/eventanm/trig);
   (ii) E1 discriminator — is CharCache/FileMerger in that path at all;
   (iii) enumerate streetslomo's own `.trig`/`.eventanm`/scene-start objects (runtime
   dump; static milo listing is top-level-only) and name why `nTriggers=0`. THEN one
   lever at the layer STEP 0 names, flag-gated default-OFF unless the A6-class
   carve-out fires with countersigned evidence. Acceptance = RECHARTER target set;
   only then reopen the deferred verts=0/near-black thread. Owned: world/vignette +
   PanelDir/eventanm surfaces; CharDriver/CharClip READ-ONLY (probes exist).
2. *(optional tail, now unblocked)* **W29-PROP-3:** bind/animate the prop-tip clip
   tracks (CharDriver/CharClip* free this wave — arbitration pre-ruled the reverse of
   W28). Riders: E-C2 `RB3_CROWD_CLIP_KEEP` removal + the E6 vocalist-mic A/B via
   `RB3_PROP_FINGER_BYPASS`. Success = right_hand `dst_n→0` on the committed analyzer.
   Defer-without-guilt.
3. No third lane. E-C2 must not survive W29 un-ruled again.

## Wave 29 (2026-07-10) — CROWD CHAIN CLOSED (sixth narrative, decisive) + E-C2 removed + PROP scoping

Kickoff `6c803adb` (+ coordinator pre-work: `scripts/native/boot-to-song.py` canonical
gameplay harness, `CHARDRV_PLAY_BT` probe), pre-review adopted `95df30f2` (CA1-CA10;
A1 caught the `part:vocals` overclaim pre-dispatch), lanes `c4d2d46b` +
`5a430eea`/`0081bae7`/`147eb1b6`, close-out review `WAVE29_CLOSEOUT_REVIEW.md`
(both lanes ACCEPT-WITH-ERRATA, E1-E8 appended to lane STATUS docs).

| Lane | Result |
|---|---|
| **CROWD-TRIGGER — RECHARTER accepted, SIX-WAVE CROWD CHAIN CLOSED (E1-E4 minor)** | W28's premise REFUTED with the mechanism named: "zero PLAY after 2.433 / never triggered" was a `CHARDRV_PROBE=crowd` FILTER ARTIFACT (`_w28_crowd_step0_boot.py:47`; player0-3 lines absent BY CONSTRUCTION — E3 records the epistemic basis: W29 `'*'` reproduction at `5a430eea` + verified no-behavior delta c6ef7795→95df30f2). The REAL walkers are the 4 `player0-3` (`char/main/main.milo`) `main.drv` drivers: `mClips==streetslomo_clips.milo` (CLIPSWAP PathName-asserted), `playerN_{f,m}` plays at beat 2.433 + loop replays at 25.333, `playing=2209/2280` sustained, and screenshots show four textured mid-stride walkers matching retail (coordinator-verified inter-frame motion). Issuing mechanism = `BandCamShot::StartAnim()` (BandCamShot.cpp:357) via CameraManager inside WorldDir::Poll — the SAME camshot-anim path as the beat-0 cityscape plays; `nTriggers=0` reproduces but is a red herring (PanelDir UITrigger ≠ walk mechanism). The 8 `crowd_*` proxies idle FAITHFULLY (streetslomo_clips has only 8 playerN clips = 4 walkers × 2 genders; no crowd clips — the asset precludes them walking). CharCache/FileMerger NOT in the play path (W28-E1 cleared). No lever (would hack correct behavior); `RB3_VIGNETTE_TRIG_REPLAY` released unused. **Census ground truth corrected (Q(b), binding): any future crowd census targets the 4 player0-3 drivers with the mClips PathName pinned; measuring the crowd_* family re-enters the trap. DO NOT REOPEN without new evidence naming player0-3 explicitly.** Deferred verts=0/near-black thread MOOT (walkers have geometry and render). |
| **GAMEPLAY — PARTIAL (E5-E8 minor)** | **Part C LANDED:** `RB3_CROWD_CLIP_KEEP` five-site scaffolding deleted from CharDriver.cpp (E-C2 ruling discharged); gates grep 0 / `CHARDRV_PLAY_BT` 1 / batch_objdiff `Play` 100.0% + `Poll` 93.54% baseline-exact; pre-W26 mBones UAF dtor guard KEPT unconditional. **Part B:** E6(a) vocalist-mic blocker RETIRED — `mic.ikhand` has `mFinger==NULL` (piece 1 provably no-op there; `finger=0` in every PROP_DST row) with the piece-1-immune mic hand as lockstep control for the cross-run variance; E6(b) global-mFinger scoping STILL GATES default-ON (Q(d): W30 needs a global finger=1 census + songMs-matched A/B, or piece 1 re-scoped to prop-chain ikhands). Probe caps parameterized through existing dbg envs (CA6, no new getenv). **Part A PARTIAL per CA2** (analyzer: `ACCEPTANCE (ON): FAIL — right_hand 8 dst>30u @31.4`, coordinator-reproduced from raw): piece-3 is INAPPLICABLE — raw census proves the on-stage band plays ONLY idle+expression clips (zero drum/groove/tip clips; drum tip AND parent bones static, 1 distinct twpos across the window), so there is no clip track to bind; the residual is blend geometry over an idle pose; floor-gaming declined (endorsed). Real driver = performance-clip SELECTION → W30 primary. **Part D:** SEED1 (dormant guitarist hands) = the KNOWN default-OFF PROP state, not a new bug — root cause is the same zero-performance-clip finding; SEED2 (green faces) DEFERRED, no retail closeup pair exists (member/position-specific, not global; C8 char-env vs stage-key-light undecided); all run logs 0-anomaly; exit-time teardown SIGSEGV = pre-existing exit-trap. |

**Close-out rulings:** crowd chain **CLOSED** (Q(a)); corrected census ground truth
binding (Q(b)); Part A label PARTIAL stands, W30 disposition = perf-clip selection
(Q(c)); `RB3_PROP_POSE_FULL` default-ON **NOT YET** — E6(b) still gates (Q(d));
probe retirement rider scheduled for W30 after the perf-clip lane's needs are known —
KEEP PLAY/PLAY_BT/CLIPSWAP, RETIRE ENTER/REPLACE(_BT)/DEFCLIP/STARVE/LIFE/C13/
CROWD_PANEL_DBG (Q(e)); `RB3_VIGNETTE_TRIG_REPLAY` released (Q(e′)). rb3-tests
governing result = coordinator clean 116/0 at HEAD (Lane 1's 10 GPU SEGFAULTs =
environmental/concurrent-run). Census 411→410 (E-C2 row removed), ONE pin bump.
Defaults: **14 ON, unchanged.**

### Wave 30 menu (from `WAVE29_CLOSEOUT_REVIEW.md` §7 — discriminator-first + checkpoint-before-fix + A7 raw-log mechanics + E4 verbatim-quote rule carried)
1. **W30-BAND-PERF-CLIP (primary):** why do on-stage band members play only
   idle+expression clips (zero instrument-performance clips — raw-proven)? Trace the
   performance-clip selection layer (song events → BandDirector/BandPerformer →
   BandCharacter::SetState/PlayGroup) with CHARDRV_PLAY/_BT + CLIPSWAP. This is Part
   A's recharter AND the likely root of SEED1 (dormant hands exist because hand
   placement falls entirely to IK against static props). Success = a named mechanism
   (working-reference style) and either a landed default-OFF lever or an honest
   recharter.
2. **W30-PROP-DEFAULT-ON (decision lane):** discharge E6(b) per Q(d) (global finger=1
   census + songMs-matched A/B, or piece 1 re-scoped to prop-chain ikhands) →
   coordinator flips the 15th default. Note: the right_hand 8×31u residual ships with
   ON either way — visually acceptable per W28/W29 closeups; say so in the flip commit.
3. **Green-faces lane (BLOCKED on reference):** acquire a retail band-member closeup
   lighting reference FIRST, then A/B char-env face lighting vs stage key-light on the
   green-faced models (C8-faces family). Do not dispatch without the reference.
4. **Exit-trap teardown SIGSEGV:** pre-existing exit-time crash (W0.3 S1); bounded,
   well-reproduced, hygiene payoff for every future gate.
5. **`part:` verb tooling:** extend `rb3_game_input.cpp:1080-1085` beyond guitar
   (coordinator-owned file) so future sweeps can charter vocals/drums/keys runs.
   Riders: probe retirement (Q(e)), released flag names ledger (Q(e′)).

## Wave 30 (2026-07-10) — PERF-CLIP MECHANISM NAMED (set_play stream dead in-song) + 15th DEFAULT (RB3_PROP_POSE_FULL) + concurrent visual pass

Kickoff `988b6de7`, pre-review adopted `fdc4d628` (CA1-CA8; A2 caught an unattainable
Path-B decider, A3 an unexecutable retirement charter, A5 a biased-census trap), lanes
`3156c6b1` + `bafa0921`, close-out review `a6022dba` (both ACCEPT-WITH-ERRATA, E1-E7
appended to lane STATUS docs), flip `9b0401ce`. Concurrent (non-workflow) Fable
visual pass `85143cdf` using the W29 `boot-to-song.py` harness: findings F1-F8 in
`W30-VISUAL-PASS/FINDINGS.md` (3 runs, 0 source changes).

| Lane | Result |
|---|---|
| **BAND-PERF-CLIP — PARTIAL (mechanism named + proven; faithful fix rechartered to W31; E1-E4)** | The on-stage band never leaves IDLE because the performance-INTENSITY selector stream is dead in-song: the only in-song band-anim driver is `play_group` from `BandCamShot::StartAnim` DTA anim-scripts (BANDPERF_STATE_BT), which sets group stand/sit but leaves `mPlayFlags` at IR (0x1000); the ONLY intensity rewriter is `OnSetPlay` (`set_play`), which has NO C++ sender (venue-mood/DTA-authored) and fires natively only at beat 0 (census: 190×mask=2, exactly 4×mask=3 all at beat 0.000). Performance clips (`stand_rhythm_*`=P/PM, `stand_solo_*`=PS) ARE resident in the bound groups — NOT a loading gap — but `GetClip` only ever asks for IR. Proven by songMs-matched `--fixed-clock` A/B via default-OFF demo lever `RB3_BAND_PERF_FORCE_PLAY`: OFF=0 rhythm/solo `CHARDRV_PLAY` (reproduces W29 idle-only), ON=55 (49 at positive beats to 50.9, coordinator-recomputed from committed gz). Lever is NON-FAITHFUL (E3: sit-group SetState churn 16→4373) — kept default-OFF as the residency-vs-selection discriminator, RETIRE at W31 close-out. Gates: batch_objdiff SetState 99.08 / PlayMainClip 92.2 / StartAnim 100.0 all baseline-exact; drawlog 792; rb3-tests 116/0; Wii .o byte-identical. |
| **PROP-DEFAULT-ON — LANDED, DECISION: FLIP-SAFE → 15th default FLIPPED (E5-E7)** | E6(b) discharged via Path A: threshold-unbiased `[PROP_CENSUS]` probe (RB3_PROP_CENSUS_DBG, CA5) enumerated 16 ikhands / 11 finger=1 (6 PROP, 5 NON-PROP: player3 feet, vocalist mic_stand + free hands); songMs-matched A/B shows EVERY non-prop chain equal-or-better on median AND max (feet 112/89→36/38u planted-closer; vocalist 118/76→78u; mic_stand 81→60u); `analyze_prop_ab.py --w30-census` → `W30 DECISION: FLIP-SAFE` exit 0 (coordinator-reproduced from committed gz). Coordinator flipped `RB3_PROP_POSE_FULL` default-ON (`9b0401ce`, opt-out `RB3_PROP_POSE_FULL_OFF`), post-flip gates all PASS (drawlog 792/308-residual, boot A/B both paths, rb3-tests 116/0, Poll 96.13 baseline-exact). E5: analyzer deciders were vacuous on empty parse (gz input → FLIP-SAFE exit 0) — guard landed (exit 2 `NO ROWS PARSED`). E6 (binding residual framing, in flip commit): drummer right_hand ships at med ~39u/max ~43u sustained (n≈3.5k uncapped); "8×31u" was the capped focus-probe view. E7: census is name-keyed (hub walkers collapse into player0-3 rows) — future censuses key by pointer or state the caveat. |

**Visual pass (F1-F8, countersigned from evidence):** F1 prop spike-fans/crumpled
cones = visual-blocker → hand fans RETIRED by the PROP flip (post-flip retest
confirmed); kit cones + waist-level hub-walker fans remain (undriven prop meshes,
W27(b) constant authored LocalXfm) → fold into W31 lane 1 acceptance. F2 translucent
score pill + F3 white glyph class + F4 missing star-slot outlines → ONE
HUD-material/texture-bind family lane (retail pairs exist for all three). F5
patch-shard now has a deterministic on-camera repro (`coop_g_cg` closeup) — recorded,
NOT chartered (two prior rewrites bisect-reverted; needs a fresh hypothesis). F6 hub
night grade: DO NOT charter until reconciled with the held RB3_UI_POST_GRADE
rationale. F7 cosmetic backlog; F8 not-a-finding pending settle-frame recapture.

**Close-out rulings:** flip executed per Q(b) with the E6 framing verbatim;
retirement per Q(d)/CA3 (CHARDRV_ENTER/REPLACE/REPLACE_BT/DEFCLIP/STARVE/LIFE,
C13_PROBE, RB3_CROWD_PANEL_DBG; coordinator overrule: the `CHARDRV_BT` env SURVIVES —
it also gates the KEEP-listed `CHARDRV_PLAY_BT`); KEEP `BANDPERF_*` (W31 acceptance
instrument) + `RB3_PROP_CENSUS_DBG` (standing census tool); `RB3_BAND_PERF_FORCE_PLAY`
retire-at-W31. Census regen after flip+retirement (410 → generator-truth; engine
classjson edit ⇒ ONE pin bump — reviewer's "no pin bump" note superseded by the
mechanism). Defaults: **15 ON.**

### Wave 31 menu (from `WAVE30_CLOSEOUT_REVIEW.md` Q(f) — discriminator-first + checkpoint-before-fix + A7 raw-log mechanics + E4 verbatim-quote rule carried)
1. **W31-SET-PLAY-DISPATCH (primary):** make the song-authored venue-mood stream
   (`[play]`/`[intense]`/`[mellow]`/`[solo]`) dispatch `set_play` to BandCharacter
   natively. Discriminator-first: (i) does the mood/venue event data exist parsed
   natively (BandDirector-side census)? (ii) who should send it (DTA venue scripts vs
   song.anim events)? Spans >1 system — expect a scoped multi-edit, not a one-line
   lever. Acceptance: with the DEMO LEVER OFF, sustained rhythm/solo `CHARDRV_PLAY`
   census (Lane-1 A/B rerun, OFF=W29-idle baseline); **no sit-group churn** (E3 bound:
   ON-run `grp='sit'` BANDPERF_STATE same order as OFF, not thousands); F1 gameplay
   retest — drumstick/prop-tip bones driven, cones/fans gone or explicitly re-scoped;
   retire `RB3_BAND_PERF_FORCE_PLAY` at close-out.
2. **W31-HUD-GLYPHS (secondary):** F2 (translucent score pill) + F3 (white glyph
   class) + F4 (star-slot row) as ONE HUD material/texture-bind family lane; trace one
   glyph end-to-end, fix the bind, verify the class across hub/song_select/overshell +
   pill/star row vs retail pairs.
3. **W31-EXIT-TRAP (small, hygiene):** the teardown SIGSEGV every wave's gates must
   tolerate. Riders: F5 recorded-not-chartered; F6 needs UIGRADE reconciliation; F7/F8
   backlog; `part:`-verb tooling coordinator-opportunistic (worth doing before W31
   dispatch if cheap); hub-walker prop-fan residual tracked (may need walk-clip
   prop-track scoping later).

### F6 adjudication note (coordinator, pre-W31 dispatch, per Q(f) rider)

**F6 (hub night grade) is NOT the held/flipped `RB3_UI_POST_GRADE` residual.** UIGRADE
governs the menu **UI-layer** grade exemption (FlushPostProcMidFrame at the venue→UI
boundary; hub text ROI 1.95→2.20; song_select/partdiff grade-INERT) — it exempts UI
from the venue grade, it does not author or apply the venue's own grade. F6 is the hub
**3D scene** (world.cam) missing its authored night grade/bloom — retail's dark neon
street vs native's flat bright daylight; F6's own text notes "menu text/UI itself is
fine — this is the 3D scene behind the UI". Distinct mechanism, distinct layer.
Charterable as its own probe-first item (is a ColorXfm/bloom/lightpreset authored in
`main.milo` skipped on the hub world.cam?) in a future wave; NOT chartered in W31.

## Wave 31 (2026-07-12) — SET-PLAY FIXED (decomp arg-order bug → SyncProperty 100%) + EXIT-TRAP KILLED (rc=0, tolerance removed) + F3 GLYPHS default-ON + hub-shard SKEL_FAMILY_STOP

Kickoff `WAVE31_KICKOFF.md` (review `f9c6559d`, A1–A12 adopted; 3 user reports
folded in after coordinator repro), pre-E1 countersign `b6828fd1` (raw-artifact
re-derivation; one material gap named), lanes `a3916764`/`fa8b5d55` (A) +
`8d46802c` (B, docs-only per A8) + engine `0083bad` (C) + `42d4a59a` (D probe TU),
close-out `6ccc36e3` + engine `24c4f95` (pin `b36bcfc` → `24c4f95`, census
415→418), review `WAVE31_CLOSEOUT_REVIEW.md` (all four lanes ACCEPT / ACCEPT-WITH-
ERRATA, errata E1-E7).

| Lane | Result |
|---|---|
| **SET-PLAY-DISPATCH — ✅ DONE (charter premise REFUTED: decomp bug, not missing feature; E1-E3, E7)** | The band's dead in-song mood stream was NOT a missing native dispatcher — the whole faithful chain (song.anim `<inst>_intensity` SymbolKeys → `BandDirector::SyncProperty` → `BandWardrobe::SendMessage` → CHAR_COMMON mood DTA → `OnSetPlay`) was present and pumped; it was silenced by a **source-level decomp arg-order bug**: 5 `SendMessage(mood, inst)` sites swapped to the correct `SendMessage(inst, mood)` (`BandDirector.cpp`, ONE TU). The residual r4↔r5 REGISTER_SWAP at exactly those 5 call sites WAS the bug — fix is **UNCONDITIONAL** (no flag; a gate would fork faithful behavior — disposition ACKED) and moves `SyncProperty` 99.96 → **100.0% Complete**. Acceptance all-PASS, countersigned from committed gz: rhythm/solo `CHARDRV_PLAY` 3→**80**, quartiles [25,16,20,19] (A3 4/4), 3 distinct intensities (play/idle/intense — not the lever re-badged), sit-churn 24→26 (A4 bound 160; the W30 lever did 16→4373), drummer `idle_play_*` 0→14 (per-driver key, E2). A5 cones/fans leg pre-agreed RE-SCOPED: perf clips are BODY clips; prop-tips = instrument-MIDI drivers = the F1 family (open debt, incl. E7 band-framing crop pairs). `RB3_BAND_PERF_FORCE_PLAY` demo lever RETIRED at close-out (E-C2 precedent; block deleted + registry row removed). |
| **HUD-GLYPHS — ✅ F3 traced + LANDED default-ON at close-out; charter "one family" REFUTED (split memo); E4, E6** | STEP-0 traced F3 end-to-end: `buttons.mat` (button-prompt glyph atlas, 7150 draws/frame) is RGB artwork + alpha cell mask but lacks "icon" in its name → `useAlphaAsRGB` text path collapses every prompt to a solid white blob (footer pills, overshell MENU dot). A8 honored exactly (mechanism checkpoint, `engineAckNeeded`, ZERO lane code); fix landed coordinator-executed (`6ccc36e3`, rb3_render_hook.cpp predicate) — **default-ON, opt-out `RB3_NO_BUTTON_GLYPH_FIX`** (B8 opt-out precedent), earned by ON-vs-OFF captures on the merged tree (white lozenges OFF vs real glyph artwork ON). Charter's "ONE HUD material/texture-bind family" REFUTED: **F2 = pill-fill material/bind issue (MEDIUM, priced), F4 = star-row show-state, not a texture bind (LOW-MEDIUM, priced)** — both split to W32. Late-add difficulty-icons report adjudicated NOT-A-DEFECT (icons PRESENT on a focused song row; user saw a header/shortcut row's correct empty state). Soft gaps: two STATUS crop filenames drifted; F3 close-out evidence still in `/tmp` (re-home rider, W32). |
| **EXIT-TRAP — ✅ DONE (root cause named, fixed, gates un-tolerated; countersign gap DISCHARGED)** | First-ever committed symbolized backtrace pinned the W0.3.S1 teardown SIGSEGV: `BandRnd::Shutdown()` never released two late-added GPU clusters (compose/C8-RTT + billboard-particle); surviving `mComposeDiffView` (== `gBandRnd+1432`) held the last Dawn device ref → real teardown deferred to static-dtor phase → SIGSEGV in the torn-down Vulkan ICD. Fix = release both clusters before `mGpu.Shutdown()` (engine `0083bad`, non-behavioral, NO flag), with an **iterative proof** (compose-only fix MOVED the bt to the particle cluster, then cleared). The countersign's one material gap (post-fix rc=0 10/10 had no committed artifact) DISCHARGED by coordinator re-derivation on the FINAL merged tree: bounded non-HTTP 5-frame boot **rc=0 10/10**; A7 executed — `drawlog-golden.py` rc-tolerance REMOVED (non-zero rc now FAILs as a regression; gate PASS frame=60 count=792, 264 known-residuals within bound); `song-end-test.py:269` keeps its crash-detection band (SIGABRT coverage unproven). **Rider WEB-YELLOW: CONFIRMED_ON_WEB** — the user's floating yellow square is a static detached quad at hub top-level (`joined_default`), does NOT track focus changes, web release build only, native clean (A9 deploy-freshness verified; stale-build dead). Capture-only per charter → W32 item 1. |
| **HUBWALKER-SHARDS — ✅ diagnosis complete, verdict SKEL_FAMILY_STOP (BINDING; no fix, correctly)** | Diagnosis-only mandate honored: 34-row per-mesh per-bone pointer-keyed table (forehead cone = eyebrows/head/goatee/hair meshes → face bones, 648–780u); (ii) undriven-track hypothesis REFUTED (walk clips drive the bones, coherent live rotations); (iii) live-bone probe (A12) pins the basis error to the **SKEL seed-R rotation-basis class** (skinDet=1.0, coherent ~42°, all 33 face bones collapse to one apex ~290u — same class as R5's 87.2°). E7 census-trap payoff: CharCache player0-3 carry **0 skinned meshes** — the visible shards are on CROWD/EXTRAS street chars + outfit fringe, i.e. under TWO closed families (R5-HANDS-ENDGAME + W23-29 CROWD). **STOP is BINDING — no recharter without a new hypothesis** (lint 6, no 7th cell). Probe TU kept as reusable tooling (`RB3_SHARD_PROBE_SCENE/_OUT` registered). Collision lesson E5: the untracked WIP probe TU transiently broke Lane C's shared-tree link — new TUs in globbed dirs must compile clean before yielding. |

**Close-out rulings:** Lane A unconditional-fix + Lane B default-ON dispositions
ACKED — the flag rule is now three tiers (decomp-correctness → UNCONDITIONAL;
retail-proven faithful restoration → default-ON + opt-out; uncertain → default-OFF
opt-in). `RB3_BAND_PERF_FORCE_PLAY` retired; A7 tolerance removal landed
coordinator-executed post-merge. Census 415→418 (−FORCE_PLAY,
+NO_BUTTON_GLYPH_FIX, +3 classified W31 probes); ONE pin bump (`24c4f95`).
Defaults: **16 ON** (F3 buttons.mat colour-icon fix joins as the 16th,
opt-out `RB3_NO_BUTTON_GLYPH_FIX`). User-report dispositions: floating legs =
half FIXED (set_play body performance) / half re-scoped to F1 (prop tips);
skin-tag forehead cones = SKEL family, closed families, STOP; floating yellow
square = web-only CONFIRMED, W32. New audit lesson: **arg-order decomp bugs**
(same-typed adjacent args, call-site REGISTER_SWAP residual at ≥99%) are a
cheap, high-yield sweep class — one swapped SendMessage arg silenced a whole
subsystem for 31 waves. Not-properly-closed list in the review §5 (F3 /tmp
evidence re-home; SyncProperty batch_objdiff countersign; F1 band-framing
crops).

### Wave 32 menu (from `WAVE31_CLOSEOUT_REVIEW.md` §6 — discriminator-first + checkpoint-before-fix + A7 raw-log mechanics + E4 verbatim-quote rule carried)
1. **W32-WEB-YELLOW (primary):** the hub floating highlight quad — user-visible,
   rider-CONFIRMED on web, native clean. Entry hypothesis: highlight-mesh
   instance surviving the `options`→`joined_default` overshell transition
   (static screen-space quad, does not track focus). Candidate surfaces:
   `src/system/bandobj/OvershellDir.cpp` / MainHubPanel highlight mesh + the
   web-vs-native render-hook divergence (why web-only?). STEP-0: name the
   quad's mesh/draw (uidump/drawlog on the web build) BEFORE any fix.
2. **W32-PROP-FAN (F1 family):** undriven prop-tip bones — drumstick tips,
   guitar neck, kit cones, magenta stick-fan guitar (instrument-MIDI drivers
   `strum.dmidi`/`fret.ikmidi`/`right_hand.dmidi`) — the re-scoped second half
   of the floating-legs user report. Opens with the E7 debt: matched-songMs
   BAND-FRAMING crop pairs (boot-to-song closeup harness). Discriminator-first:
   are the dmidi/ikmidi drivers bound and fed natively, or bound-and-starved?
3. **W32-HUD-F2F4:** the two priced Lane-B split sub-charters — F2 score-pill
   fill (MEDIUM: pill-mesh material dump → bind/blend fix) + F4 star-row
   unearned-slot show-state (LOW-MEDIUM). Two mechanisms, two checkpoints; do
   NOT re-merge into "one family" (hypothesis refuted W31).
4. **F7 song-select right-edge clipping** (user-repeated ×2), incl. the
   no-opaque-panel-behind-sidebar variant.
5. **F5 patch-shard `coop_g_cg` repro:** recorded, deterministic; dispatch ONLY
   with a fresh hypothesis + its own oracle (closeup gate is shard-blind; two
   prior bisect-reverts).
6. **Riders (coordinator-cheap):** (a) re-home F3 ON/OFF evidence from
   `/tmp/w31-f3` into `W31-HUD-GLYPHS/evidence/`; (b) SyncProperty 100.0%
   batch_objdiff countersign; (c) keep/retire decision for `RB3_SETPLAY_PROBE`
   + `RB3_SHARD_PROBE_*` per the W30 probe-retirement discipline (SETPLAY
   plausibly KEEP as acceptance instrument; shard probes' family is STOPPED);
   (d) arg-order audit pilot — sweep ≥99% functions whose sole residual is a
   call-site REGISTER_SWAP on same-typed args for swapped-argument decomp bugs.
7. **Carried/blocked:** F6 hub night grade BLOCKED on UIGRADE reconciliation
   (unchanged); F8 pending settle-frame recapture; SKEL/CROWD families CLOSED
   (Lane D STOP binding).

## Wave 32 (2026-07-12) — WEB YELLOW-QUAD FIXED (root: ENTIRE render-hook policy family absent on web since W1) + PROP-FAN FIXED+FLIPPED (17th default) + F4 CLOSED NOT-A-BUG + arg-order sweep class EXHAUSTED

Kickoff `WAVE32_KICKOFF.md` (pre-dispatch review WAVE32_REVIEW.md, A1–A14 ALL
adopted at `30546499` = base SHA for all lanes), pre-E1 countersign `9eb50c3c`
(raw-artifact re-derivation, PASS with two coverage flags), lanes `41d52acb`
(A, STEP-0 + A2 STOP) + `3ed6118a`/engine `c0bc00a` (B) + `b0f6e3b7` (C,
docs-only) + `ad0130f4` (D) + `d07738cb` (F7 rider, docs-only), close-out
`69103c77` + engine `2ea8e34` (pin `24c4f95` → `2ea8e34`, census 418), review
`WAVE32_CLOSEOUT_REVIEW.md` (all four lanes + rider ACCEPT).

| Lane | Result |
|---|---|
| **WEB-YELLOW — ✅ FIXED at close-out (STEP-0 + model A2 STOP; root bigger than the charter)** | The floating yellow quad = the hub focused-menu highlight bar (`highlight_main.mesh`/`highlight_pattern.mesh`, skinned UI mesh) rendering at world ORIGIN because the per-focus placement policy never fires on web. Root is a **build-list divergence, not a code bug**: `native/CMakeLists.txt` excluded `rb3_render_hook.cpp` from the EMSCRIPTEN target since the W1 "clear-frame era" — so `GetGameRenderHook()==nullptr` on web and the **ENTIRE policy family (B1–B13 incl. the W31 F3 glyph fix) had been silently absent on web for ~30 waves**. Lane A named the four-link divergence chain file:line, then hit the A2 fence (mechanism (i) = Lane C's exclusive TU) and STOPPED with a checkpoint + fix proposal — the fence's first live exercise, honored exactly. Fix coordinator-executed at close-out (TU added to `RB3_WEB_NATIVE_GLUE`, parity restoration, no flag — build-level, three-tier N/A): quad GONE at `joined_default` on BOTH debug and release builds, highlight bar contained on the focused row and tracking focus both directions (A10 replay), native unaffected by construction (web-only source list) + drawlog-golden PASS (792, 287 known-residuals) + bounded boot rc=0 5/5. Evidence re-homed to `W32-WEB-YELLOW/evidence/coordinator-fix/`. Residue: web song_select/gameplay with the full family live untested beyond the hub → W33 rider. |
| **PROP-FAN — ✅ FIXED + FLIPPED default-ON (17th; closes the floating-legs report's second half)** | Cleanest discriminator→fix arc of the campaign. E7 crop-pair debt discharged first (baseline census: drummer band-shards **1107** ratio 5.92, guitarist **843**/5.15, vocals 0). Discriminator per prop class (A8): branch **(b) STARVED** — drivers created + polled but **never Entered** (`CTOR 23 / ENTER 0 / POLL 17 / FEED 0`; mechanism: `RndDir::SyncObjects` rebuilds `mPolls` AFTER the one-time `Character::Enter`), so no AddSink → no MIDI events → no hit/strum clips → idle arm → CharIKHand over-reach → prop-tip fan. Fix on owned surface only (`CharDriverMidi.cpp`, HX_NATIVE, lazy one-time Enter on first Poll, no struct member): ENTER 0→21, OnMidiParser 0→**173**, drum-hit clips 0→**416**, drummer shards **1107→2** (residual = shoe mesh, SKEL family, left closed), guitarist **843→0**, fans visually gone; W31 set_play NON-REGRESSED (CHARDRV_PLAY 81/80, SETPLAY_SEND 26/26); Wii objdiff neutrality countersign-verified (Poll/Enter/ctor 100.0 raw). Shipped default-OFF, flip PROPOSED not self-granted; coordinator flipped at close-out: **`RB3_NO_MIDIDRV_ENTER_FIX` opt-out** (engine `2ea8e34`). Probe counts /tmp-only = countersign §2 flag, resolved by accepted-census ruling. |
| **HUD-F2F4 — ✅ F2 named + chartered to W33; F4 CLOSED NOT-A-BUG (W30 premise refuted)** | Zero code, two checkpoints, both halves honest in opposite directions. **F2 (score pill): REAL BUG** — HEADMAT dump proves all three pill layers' textures BOUND; pixel proof shows native pill body == venue (backing contributes ~0) vs retail opaque dark; hypothesis: Wii refraction-material opacity does not derive from diffuse.a, native's generic unlit path multiplies by it. Lane refused the A11-violating render-hook tint AND the un-acked engine write; coordinator withheld engineAck (blend-semantics blast radius) → **W33-F2-PILL** with mandatory cross-screen sweep. **F4 (star row): FAITHFUL NON-BUG** — the W30 "retail always draws 5 slots" finding compared native-at-0-stars vs retail-at-4-stars, i.e. the exact A11 trap, retroactively found in our own evidence. Refuted both ways: BandStarDisplay progressive reveal is 100.0%-matched source retail runs; retail's own screenshots grow the row (2 stars=3 discs, 4=5); native matches at 0/1/3 stars (A11 over-satisfied, five states). Lint-4 registry sweep done pre-claim; `rb3_render_hook.cpp` grant held unused (git-clean, countersigned). |
| **ARG-ORDER — ✅ honest negative: sweep class EXHAUSTED (0/1185, ruling: do NOT renew); 1 behavioral find KEPT** | A4 fresh report regen first; 1185 in-scope fuzzy ≥99 functions swept with a progressive classifier (605 naive → 0 strict; argscan7 cross-opcode 52, all noise: regalloc renames, scheduling, commutative operands, FPR cascades, string-pool offsets). **Zero clean-raw-100 arg-order landings** — the W31 "cheap, high-yield sweep class" thesis is REFUTED as a sweep; the lesson survives as a **behavioral investigation heuristic** (dead subsystem → check the dispatch path's ≥99% call sites), which is also how the one genuine find surfaced: `VocalTrackDir::SetRange` passed `SetFrame(1.0f, 0.0f)` where retail passes `(0.0f, 1.0f)` (lone blend=0.0 anomaly in a TU of blend=1.0f calls) — the pitch-window material anim silently dead on the from-chromatic tonic transition. Retail-byte-verified at the call site; TU stays raw 99.28 (pre-existing orthogonal FPR cascade, permuter-class) — strict-A5 tension FLAGGED transparently, coordinator adjudicated **KEPT** (call-site bytes retail-exact; match% is a means, not the end). A5 unit neutrality + the 99.28 independently countersigned. Ranked backlog routed (FPR cluster → permuter). |
| **F7-CLIP rider — ✅ diagnosis complete; W33 charter drafted** | Q1: **nothing clips the character** — world.cam draws all on-screen; the "clip line" is the boundary between the list column (50% dimmer + opaque row quads) and the sidebar column (authored `header_list_bg.mesh` full-screen 50% dimmer ONLY). Q2: retail has a near-opaque sidebar panel (two photo sources); ours = **missing panel draw / asset gap** — every candidate backing mesh draws=0 (the `song_select_details` drill-in sub-panel never shows in quick-view), NOT depth/stencil. Correctly REUSED W4.3-C2/C2a (Wave 13) + re-confirmed with fresh drawlog/uidump; dead ends documented. Charter draft: **W33-F7-SIDEBAR-BACKING** — native backing quad, retail-proven faithful-restoration tier (default-ON + opt-out), non-goal: don't touch the global dimmer alpha. |

**Close-out rulings:** Lane A fix = parity restoration (no flag); Lane B flip
earned by ON-vs-OFF (three-tier faithful-restoration); Lane D KEPT ruling +
"sweep class not renewable"; F4 CLOSED NOT-A-BUG; F2 → W33 charter. Shard
probes retired per pre-kickoff disposition (`rb3_shardprobe_native.cpp`
deleted, `RB3_SHARD_PROBE_SCENE/_OUT` rows removed); `RB3_MIDIDRV_PROBE` +
`RB3_SETPLAY_PROBE` KEEP as acceptance instruments (retirement condition owed,
W33). Census 418 (engine `2ea8e34`); ONE pin bump. Defaults: **17 ON**
(`RB3_NO_MIDIDRV_ENTER_FIX` joins). A12 dispositions recorded: F8 carried NOT
chartered (third wave unowned — charter or close in W33); E6 taxonomy debt now
**THREE** default-ON fixes in class=workaround (HUB_HIGHLIGHT / BUTTON_GLYPH /
MIDIDRV_ENTER opt-outs) distorting the §W5.3 metric — registry owner owes a
fix-opt-out class. User-report dispositions: floating yellow square = FIXED
(web, both builds); prop fans/floating-legs second half = FIXED (default-ON);
score pill = real bug, chartered W33; star row = not a bug (faithful
progressive reveal); song-select clipping = mechanism named (missing sidebar
backing), fix chartered W33. New process lesson: the **stale-exclusion
class** — a W1-era web source-list exclusion silently forked the web renderer
for ~30 waves; W33 runs a one-time web/native source-list parity audit.
Not-properly-closed list in the review §5 (top two: web full-policy validation
beyond the hub — release users are already on the new build; Lane A leg-4
native uidump control never captured).

### Wave 33 menu (from `WAVE32_CLOSEOUT_REVIEW.md` §6 — discriminator-first + checkpoint-before-fix + A7 raw-log mechanics + E4 verbatim-quote rule carried)
1. **W33-F2-PILL (chartered):** score-pill dark backing — engine
   RB3MaterialBinder coverage handling (refraction-material opacity not from
   diffuse.a). The one chartered engine blend-semantics change: MANDATORY
   cross-screen material sweep before any flip, ON-vs-OFF retail-paired crops,
   A11 (mechanism-named fix, no pill tint).
2. **W33-F7-SIDEBAR-BACKING (rider's drafted charter):** native-only opaque
   backing quad behind the song-select difficulty grid — retail-proven
   faithful-restoration tier (default-ON + `RB3_NO_*`, earned by ON-vs-OFF),
   append-only render-hook predicate, real-song-row focus (W31 heading trap),
   drawlog-golden + list-column-unchanged acceptance. Non-goal: the global
   `header_list_bg` dimmer alpha.
3. **Web full-policy validation pass (cheap Sonnet rider — run FIRST):**
   song_select + gameplay on web, debug AND release, now that the render-hook
   family is live (F3 glyphs + B-family policies verified beyond the hub;
   release users are already on this build). Closes review §5.2.
4. **Web/native source-list parity audit (coordinator-cheap, one-time):** diff
   the native vs EMSCRIPTEN source lists in `native/CMakeLists.txt`; every
   intentional exclusion gets a current justification or gets removed (the
   stale-exclusion class has bitten once).
5. **E6 taxonomy fix (registry owner):** `fix-opt-out` class or §W5.3 metric
   exclusion for the three default-ON fix opt-outs; one edit + single census
   regen.
6. **F8 settle-frame recapture:** charter as a micro-rider or close as
   obsolete — third wave carried unowned.
7. **Riders:** probe soak-retirement condition for `RB3_MIDIDRV_PROBE` +
   `RB3_SETPLAY_PROBE`; Lane A leg-4 native `/api/uidump` hub-mesh control
   capture (rides any lane that boots a native hub control).
8. **Carried/blocked:** F5 patch-shard repro (needs fresh hypothesis + own
   oracle); F6 hub grade BLOCKED on UIGRADE reconciliation; SKEL/CROWD
   families CLOSED (STOPs binding); arg-order NOT renewable as a sweep (Lane D
   ruling — behavioral-heuristic use only).

## Wave 34 (2026-08-01) — HANDS/POSE ROOT CAUSE FIXED (alias-unsafe Multiply)

W34-CHARCLIP-EVAL (kickoff+review+amendments `79a5b84d`/`eeae37bb`, lane
`ec9bd9ff`→`6b0e02d1`, E1 ratify `62f14711`): STEP-0 per-layer value trace
named **L3 world compose**, exonerating clip decode (L1/L2) numerically. Root
cause = native `Multiply(Transform,Transform,Transform)` (math/Rot.cpp #else)
ALIASING-UNSAFE vs the MWCC paired-single asm's load-all-then-store contract;
`BandIKEffector::NeutralWorldXfm` aliases destination recursively per bone →
corrupt neutral pose → IK yank → the entire "spindly hands / 4.2× stretch /
mask-face" family. Fix UNCONDITIONAL; detonations 3650→0, maxRatio 1.000
exact; Wii match byte-identical (full report.json A/B); drawlog 792; rb3-tests
123/116/7/0; coordinator E1 PASS on independent recapture (fingers render).
Same fix covers SetTransParent + CharIKHand.cpp:459 call sites. NOTE: the
SKEL-family STOP is now DISCHARGED-BY-FIX, not merely closed — the 15-wave
bind-side theories were chasing a downstream symptom. New kickoff lint: audit
native #else replacements of MWCC asm for aliasing assumptions.

**Wave 35 menu:** (1) re-measure post-fix: FOREARM-FLOAT, W28-PROP right_hand
~39u, RB3_HANDS_MITTEN + RB3_IK_REACH_CLAMP retirement A/Bs; (2) walk-on-snap
frozen remnant (BandCharacter.cpp:603-632); (3) lighting/wash casts (now the
biggest visual family); (4) W33 leftovers F2-PILL + F7-SIDEBAR-BACKING; (5)
web-build confirmation of the alias fix; (6) engine backlog: Mesh_Wgpu
GetDrawMode()==8 dead cull-overrides (SPIKE-X0 find).

### ★ W35-0 (do this FIRST): the ANAT oracle is rigid-motion invariant

Imported from rb3-xenon X4c (2026-08-02, `docs/plans/x4c-init-audit-2026-08-02.md`),
which lost four milestones to exactly this and named it its parting lesson:
**pick oracles that are not invariant under the defect you are hunting.**

Our ANAT metric is `ratio = |childWorld − parentWorld| / |childLocal.v|`
(BandCharacter.cpp:896-905, :923-930) — a **pairwise distance ratio**. Pairwise
distances are preserved by *any* rigid motion, so the metric is **structurally
blind** to: a whole skeleton (or subtree) rigidly displaced or rotated; a wrong
but still-unit quaternion; and any defect that moves parent and child together.
W34's "maxRatio 1.000 exact" is therefore a real result about *bone rigidity*
and **not** evidence that world poses are correct. Xenon's equivalent claim
(0.9999) coexisted with a posed character that was mostly bind-pose geometry
and wrong throughout — one **absolute landmark position** found in a single run
what four milestones of rigidity checks could not see.

Cheap to fix: the probe **already logs absolute positions**
(`childWorld=(%.3f,%.3f,%.3f)`, BandCharacter.cpp:911-914) — only the PASS
criterion is the ratio. Add absolute landmark checks (a few named bones with
expected world positions/bounds at a known clip+timestamp) and re-run the W34
A/B before treating any part of the SKEL family as settled. This does **not**
reopen the alias-unsafe-`Multiply` fix, which was verified independently by
detonation count, screenshots and a byte-identical Wii match — it bounds what
the *ratio* evidence alone can support, and it is the right instrument for
the still-open items (1), (2) and any residual face/limb defects.
