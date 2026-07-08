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
