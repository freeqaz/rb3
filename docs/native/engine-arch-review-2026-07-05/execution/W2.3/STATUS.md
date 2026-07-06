# W2.3 — STATUS (append-only, update under `flock /tmp/rb3-docs.lock`)

Item: **Kill shared-GeomOwner skeleton aliasing for crowd + props** (SYS-1 bone-source half).
Lane A item 2 (engine). Sequential AFTER W2.1 (`6852caa`). Engine pin `6221a56` (do NOT bump);
engine working-tree HEAD `6852caa`. Default-OFF flag `RB3_CROWD_OWN_BONES`. Rebind RETAINED (A1/R5).

Plan: `PLAN.md` (`## Subtasks` W2.3.S1..S3). Commit prefix `W2.3:`. Own build dir
`native/build-agent-W2.3`. Stage only your own files; flock git per repo.

Implementers/verifiers append one `## <subtask-id> — done|partial|blocked` section per subtask with
commit SHAs, the S1 DECISION (SHARED / SELF+POISON / MIXED), gate results, and blockers. Re-runs read
this + `git log --grep=W2.3` and skip done work.

<!-- append below -->

## W2.3.S1 — done (DECISION = SELF+POISON — W2.3 bone-source thesis REFUTED for crowd)

Characterized the crowd bone-source seam, stood up the CrowdBoneOracle negative-control
gate, and proved it RED on the unchanged `6852caa` build. **No behavior change committed**
(the probe is env-gated + reads-only; the oracle/harness are rb3-tests infra).

**Commits:**
- engine `609efb7` — `RB3_CROWD_BONE_PROBE` diagnostic in `DrawMesh`'s skinned branch
  (`Rnd_Wgpu_RB3.cpp`) + flag registered class:probe in `classification.json` + `gen.inc`
  regen (census exit 0, 318 flags). Render-inert, reads only; placed BEFORE the
  SKEL_REBAKE/RECOMPUTE_OFFSETS pre-passes so it reports the seam as the Crowd.cpp rebind
  state leaves it.
- rb3 `007d49d8` — `crowd_bone_oracle.h` + `test_crowd_bone_oracle.cpp` (wired into
  `rb3-tests`) + `crowd-bone-gate-capture.py` + ledger regen.
- rb3 `a4d076e4` — switch the gate metric from the probe owner-extent to the engine
  SKIN_CLAMP shard-drop EVENT count (see "Metric correction" below); harness extracts both
  markers into `<label>.marker.log`.

### The DECISION: SELF+POISON (thesis refuted for crowd)

The PLAN's highest risk (two contradictory Crowd.cpp root-cause comments) is resolved
**empirically**. MEASURED on `6852caa`, gameplay venue, RB3_NO_CROWD_REBIND=1 (raw seam),
40 distinct crowd/extras skinned meshes, 240 `[CROWD_BONE_PROBE]` lines:

- **Q1 = SELF.** Every crowd/extras mesh is `owner == mesh` (pointer-identical,
  `ownerEqMesh=1`) with `diffInstance=0` (owner->BoneTransAt(b) == mesh->BoneTransAt(b) for
  every bone). **0/40 shared.** The header comment's "SHARED GeomOwner between the gameplay
  venue and the tv3 vignette" thesis (`Crowd.cpp:986-999`) is **REFUTED**; the inline
  comment's "`GeomOwner()==self` … the poison is the OFFSET" (`Crowd.cpp:1000-1019`) is
  **CONFIRMED**.
- **Q2 = POISON.** The drawn mesh's OWN-bone mesh-local skin extent is 21.4u–807.7u, all
  >> the 12u SKIN_CLAMP threshold. Since `own == owner` (self), reading the drawn mesh's
  own bones is a **literal no-op**, and those bones are offset-poisoned regardless.

⇒ **DECISION = SELF+POISON.** Per PLAN, this **REFUTES W2.3's bone-source ("read own
bones") thesis for crowd**: `boneSrc = mesh` is definitionally identical to `boneSrc =
owner` here, so it cannot make the rebind unnecessary. **S2 STOPS and escalates** — the
sharding is a pure OFFSET-rebake problem (exactly what `RebindCrowdCharBonesToOwnSkeleton`'s
`SetBone(b, own, calcOffset=true)` does). The generalization of that offset rebake is a
**separate future item**; `RebindCrowdCharBonesToOwnSkeleton` is **RETAINED** this wave
(A1/R5) as still load-bearing (see fail-red delta below). This is the honest-refutation
branch the PLAN accepts as a valid exit; **W2.3 lands NO engine behavior change.**

### Gate + fail-red evidence

- **Synthetic oracle (always-run):** `rb3-tests --gtest_filter='CrowdBoneOracle.*'` → **9
  passed / 1 live-skip.** Covers: probe parse, SKIN_CLAMP parse + crowd/extras filter,
  not-elevated GREEN, shard-spike **fail-red RED**, the 3 decision classifiers
  (SELF+POISON / SHARED / MIXED), inconclusive-not-pass, worst-instance reduction.
- **Full rb3-tests:** **88 passed / 4 live-skip, 0 failed** (no regression).
- **Fail-red RED on `6852caa` (the free proof):** `crowd-bone-gate-capture.py` captures the
  crowd SKIN_CLAMP shard-drop under rebind-ON baseline vs rebind-OFF candidate (both
  `RB3_PLACEMENT_CONTRACT=1` so only the bone axis varies). **Crowd SKIN_CLAMP events:
  baseline 330 → candidate 7719 (23.4×)** ⇒ `CrowdBoneOracle.RealCaptureNotElevated` goes
  **RED**. (Total, incl. non-crowd: 1642 → 14500.) Reproduced across two runs (332→7800,
  330→7719).
- **Gate not tautological:** A/A (baseline-vs-baseline, 330 vs 330) → **GREEN**. The gate
  discriminates the rebind state; it is the exact condition a real crowd fix must satisfy.
- **The rebind is load-bearing (A1/R5 justification):** disabling it multiplies crowd
  shard-drop ~23× — it genuinely fixes broken meshes (via offset rebake), so it must be
  retained, not deleted.

### Metric correction (recorded honestly)

The first oracle metric — the CROWD_BONE_PROBE owner-extent — could NOT separate rebind-ON
from rebind-OFF: (a) the crowd meshes are self-owned so owner-extent == own-extent
(poisoned in both states), and (b) the probe gates `!mesh->mNativeBonesRebound`, which the
rebind sets (`Crowd.cpp:1050`), excluding latched meshes. The faithful shard-drop signal is
the engine's SKIN_CLAMP fallback-to-identity EVENT count (`sFallbackBones` / `[SKIN_CLAMP]`)
on crowd/extras meshes, which the rebind's offset rebake genuinely reduces. The gate now
asserts on that (candidate <= baseline*2.0 + 500). The CROWD_BONE_PROBE still drives the
Q1/Q2 DECISION.

### Files
- engine `src/platform/Rnd_Wgpu_RB3.cpp` (probe), `NativeCompatFlags.classification.json`,
  `NativeCompatFlags.gen.inc`.
- rb3 `native/tests/crowd_bone_oracle.h`, `native/tests/test_crowd_bone_oracle.cpp`,
  `native/CMakeLists.txt`, `scripts/native/crowd-bone-gate-capture.py`, ledger regen.
- Captured artifacts: `/tmp/w23-crowd-final/{baseline,candidate}.marker.log` (+ engine logs).
  NOT committed (large, boot-nondeterministic; the oracle derives everything from a fresh
  capture, mirroring W2.1.S1's no-committed-golden decision).

### Coordinator handoff / next
- **S2 is gated OUT (DECISION = SELF+POISON):** do NOT land `RB3_CROWD_OWN_BONES` — it would
  be a placebo no-op for crowd. S3 (flag-ON verification) is consequently N/A.
- **Escalation:** the residual crowd sharding is offset-poison, fixed today by
  `RebindCrowdCharBonesToOwnSkeleton` (RETAINED). A faithful generalization (load-time
  rest-offset rebake for crowd/extras, cf. W2.6's foot/shoe `RebindOutfitBonesToOwnSkeleton`
  work) is a separate future item — NOT a DrawMesh bone-source change.
- **NOT touched (BINDING honored):** `src/system/world/Crowd.cpp` (toggled only via its
  existing `RB3_NO_CROWD_REBIND` env), `src/App.cpp`, the sibling `FxSendNative.cpp` edit.
