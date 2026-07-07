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
