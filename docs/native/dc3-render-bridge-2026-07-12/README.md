# DC3 Render Bridge study — hub index (2026-07-12)

Planning-only study (no code changed, no builds run) answering: should
RB3-native render through DC3's mature native rendering stack? Full DAG:
charter → 4 recon lanes → architect synthesis → 3 adversarial reviews →
finalizer adjudication (this index).

## Documents

| Doc | Role |
|---|---|
| [CHARTER.md](CHARTER.md) | Mission statement and study rules |
| [recon/](recon/) | Recon handoffs: R1 (RB3 render path today), R2 (DC3 renderer + engine flavors), R3 (rndobj fork divergence), R4 (motivation vs the campaign's defect ledger) |
| [PLAN.md](PLAN.md) | Options A/B/C analysis, recommendation, go/no-go + kill criteria, spike charter, implementation DAG (C0-C7 + C2b), risk register, **§7 review adjudication** |
| [SPEC.md](SPEC.md) | `SceneView` convergence-layer technical spec: boundary, interface, data flow, vertex/texture strategy, compile/link model, lifetime rules, open questions |
| [review/feasibility.md](review/feasibility.md) | Skeptic lens: feasibility/correctness — **GO_WITH_CHANGES** (0 blocking, 5 advisories: read-only invariant, enum re-sourcing, ShadowPass couplings, missing rb3 draw-seam defs, spike gate-validity) |
| [review/roi.md](review/roi.md) | Skeptic lens: ROI vs alternatives — **GO_WITH_CHANGES** (1 blocking: C0 wrongly gated the spike; advisories: spike unrepresentative, drift decelerating, C5 undersized, payoff ~2k LOC) |
| [review/integration.md](review/integration.md) | Skeptic lens: integration/asset-risk — **GO** (0 blocking; advisories overlap feasibility's; verified pin skew is a benign linear fast-forward) |

All review findings — including the one blocking finding — were adjudicated
and fixed in place; see **PLAN.md §7** for the finding → disposition →
evidence table. No lane returned NO_GO. All checkpoints
(`/tmp/dc3bridge-ckpt/{R1..R4,ARCH,REV-*,FINAL}.json`) present; no missing
lane.

## Final recommendation

**No bridge.** The "bridge" premise dissolves on inspection: RB3-native
already renders through the same shared `milo-native-engine` WebGPU core as
DC3 — the difference is a per-game backend *flavor*, and R4's defect-ledger
audit shows zero open defect families require the DC3 backend (at most one is
even plausibly helped). Options A (scene adapter) and B (wholesale flavor
flip) are NO-GO — A on one-binary/one-rndobj-fork grounds (verified in
engine CMake), B on wire-format (`RndMesh::Vert`) and shipped-fix regression
grounds. **Adopt Option C as amended:** make the DC3-flavor pass TUs
rndobj-shape-neutral via a small `SceneView` accessor seam so one
implementation compiles against either fork, adopted by RB3 behind
default-OFF flags only on chartered need, retiring `RB3*` twins (~2k LOC
bounded payoff) as converged TUs prove out — an explicitly code-health /
capability-headroom investment, **not** a defect campaign; the visual
campaign's remaining defect effort recharters at class-(c) families
(perf-clip, HUD-glyphs, patch-mesh) instead.

## Next action

Charter **SPIKE-TQ + C2b** (PLAN.md §3 spike + §4 nodes C1/C2/C2b; one wave,
one-two agents, bounded sunk cost — C0 pin reconciliation is a landing gate
and does NOT block the spike):

1. **C1+C2 (SPIKE-TQ):** in an isolated engine worktree, add the minimal
   `SceneView.h` + `SceneView_RB3.cpp`/`SceneView_DC3.cpp` slice, port
   `TransparentQueue.cpp` (re-source `BaseMaterial::kBlend*`, add the new
   rb3 `DrawMeshImmediate` strong-def), compile into the rb3 flavor behind
   `RB3_TRANSPARENT_SORT` (default OFF), exercise a ShadowPass-shaped read
   (light enumeration + `CamViewProj` recompute), matched-frame A/B OFF/ON
   via the headless harness, dc3-flavor rebuild + milo-tests, **and a web
   smoke** (rb3 flavor has never compiled a Tier-2 pass TU under
   Emscripten).
2. **C2b (parallel paper-audit):** diff `RB3PostProc`/`RB3MaterialBinder`
   feature matrices vs `PostProcPass`/`MaterialSetup` to size the
   fix-preservation risk before C4/C5 dispatch.

No downstream convergence node (C3-C7) dispatches until C2 reports PASS. If
the spike fails, Option C halts at "keep the twins, document why" — total
sunk cost one wave.
