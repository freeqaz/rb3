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
