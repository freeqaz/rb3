# Skeptic Review — ROI vs alternatives (lens: roi)

_Reviewer posture: refutation. I tried to break the premise that Option C is
worth chartering. Verdict: **GO_WITH_CHANGES** — the plan is intellectually
honest and appropriately narrow, but one load-bearing ROI claim ("the de-risking
spike is cheap / bounded to one wave") is falsified by the plan's own DAG and
must be corrected before charter. Everything else is advisory._

All citations are to real files (`file:line`) or to a specific sentence in
`PLAN.md` / `SPEC.md` / `recon/*`. I ran no builds (read-only rule).

---

## What survives the attack (why this is GO, not NO-GO)

The charter asked me to attack the premise. The premise mostly holds, because
the plan already conceded the hard part:

1. **The plan does NOT bet defect burn-down on the bridge.** It states
   explicitly that of the currently-OPEN defect families the bridge helps "at
   most one" (W31 HUD-glyphs, itself rated a texture-bind bug addressable in
   RB3's own binder) and "zero open families *require* the DC3 backend"
   (`PLAN.md:50-54`). Success criterion disclaims burn-down
   (`PLAN.md:74-77`). I could not find an inflated defect claim to refute —
   R4's tally (`recon/R4-motivation-constraints.md:58-64`) is the discipline
   here: 6 of ~17 families are class (b), 5 already fixed in BandRnd, the 6th
   (SYS-4 lighting) measured near-no-op on boot-reachable venues.

2. **The 77-session effort baseline does NOT apply to the recommended option.**
   Confirmed: DC3's native renderer is at **Session 77**
   (`dc3-decomp/native/NATIVE_PORT_STATUS.md:3`), with render maturity spread
   across dozens of sessions (S25 text, S28 compressed verts, S30 blend, S32-36
   font, S69-77 full gfx coverage). That 77-session cost is the price of
   *building* the renderer — which Options A/B would re-incur and which the plan
   correctly rejects (`PLAN.md:109,136`). Option C **reuses** that mature code
   behind a seam; it does not rebuild it. So the honest effort comparison is
   seam-refactor cost, not 77 sessions. The plan's framing is honest on this.

3. **The opportunity-cost attack fails.** "Would the same effort close more gaps
   on the current path?" — No. R4 shows the current path is *also* at
   diminishing returns: the last 12 waves produced ~3 default flips, all class
   (c), and the residual frontier is terminal/twice-burned (hands terminal
   W21; patch-mesh twice bisect-reverted; perf-clip game-logic)
   (`recon/R4-motivation-constraints.md:114-124`). Effort is not being stolen
   from a productive alternative. The plan's §3 rechartering of defect effort to
   class (c) is orthogonal to the bridge and is the correct call
   (`PLAN.md:186-190`).

4. **The charter is genuinely bounded / need-gated.** Unconditional spend =
   C0 (S) + C1 (M) + C2 spike (S-M) ≈ one small wave; C3-C7 fire only on a
   "need trigger" (go/no-go criterion 2, `PLAN.md:199-202`) with per-node kill
   criteria (`PLAN.md:210-217`). The plan does not overcommit. This is the
   single biggest reason the ROI posture is defensible.

---

## BLOCKING

### B1 — "The de-risking spike is cheap / one-wave / bounded sunk cost" is falsified by the plan's own DAG (C0 gates the spike unnecessarily)

The plan sells the spike as cheap and bounded: *"Size: S-M (one wave, one
agent). If the spike fails … total sunk cost one wave"* (`PLAN.md:241-243`).
But the DAG makes **C2 (the spike) depend on C0**
(`PLAN.md:254` — `C0 ──► C2`; `PLAN.md:282` "Depends: C0, C1"), and C0 is
three-repo pin reconciliation with an **unbounded coordination tail** the plan
itself flags: it needs "a named owner with commit rights on all three repos"
and "DC3's consumers weren't surveyed for pending engine-HEAD incompatibilities"
(`SPEC.md:324-327`, Open Q5). R2 only characterized the skew (RB3 `b36bcfc`,
DC3 `77eb428b`, HEAD `0083bad3`), **not the delta contents**
(`recon/R2-dc3-renderer.md:152-154`). C0 stall is even listed as a *kill
criterion* (`PLAN.md:216-217`).

The refutation: **the spike does not need C0.** The spike runs "in an engine
worktree" building `rb3-native` (`PLAN.md:225-230`); its DC3-bit-preservation
check rebuilds the dc3 flavor + milo-tests against that same worktree HEAD
(`PLAN.md:234-235`), **not** against DC3's production pin. Getting both
production consumers onto one reconciled SHA (C0) is a *landing* prerequisite —
it buys nothing toward answering "is the SceneView seam viable?" Making C0 a
hard predecessor of the gate node inflates the advertised "cheap one-wave spike"
with a cross-repo reconciliation of unmeasured size. As written, the ROI premise
("cheap, bounded de-risk") is not true.

**Fix (trivial):** re-wire the DAG so C0 is a predecessor of C7 / any
production pin-bump, not of C2. Spike runs against an isolated engine worktree
first; C0 is paid only if/when a converged TU actually lands. Then "spike = one
wave, bounded" becomes true and the go/no-go story is honest.

---

## ADVISORY

### A1 — The spike is the LEAST representative node, so a spike PASS does not de-risk the nodes where effort and risk actually live (the "really predictive?" question: NO)

SPIKE-TQ targets TransparentQueue precisely because it has "the smallest shape
surface of any dc3 TU" (`PLAN.md:222-223`; surface table `SPEC.md:62`) — and it
is a pass the plan concedes is **default-OFF and refuted as a fix** (W1.6
Exit-A, `PLAN.md:236-239,340`; `execution/README.md:129-130`). So a passing
spike proves the seam mechanism compiles+runs for the *easiest* case. It says
almost nothing about:

- **C4 (PostProcPass):** must absorb RB3PostProc's shipped chroma-preserve +
  UI-post-grade fixes; the plan admits "whether `gfx/PostProcPass` can absorb
  them without forking its DC3 behavior is unknown until C4's audit"
  (`SPEC.md:317-320`, Open Q3).
- **C5 (material-bind):** structurally-different models (see A3).

These two carry the entire payoff (SYS-2 data-driven binder) and the entire
regression risk to the 15 shipped default-ON fixes. Go/no-go criterion 1 treats
spike-pass as the gate to "dispatch downstream" (`PLAN.md:196-198`), but the
spike is cheap *because* it is unrepresentative — it does not predict the
expensive part. Recommend adding a parallel **paper-audit de-risk** of the
C4/C5 fix-preservation surface (diff RB3PostProc/RB3MaterialBinder feature
matrices vs the DC3 twins) so the go/no-go is predictive of the effort that
matters, not just the trivial node.

### A2 — "Compounding drift" is decelerating, not compounding — the code-health justification is weaker than stated

The plan leans on "two-implementation drift is a real, compounding cost"
(`PLAN.md:64-65`; table row `:180` "drift shrinks per merge" implies it is
currently growing). The divergence mechanism is real (RB3-side campaign fixes
land in RB3PostProc/RB3MaterialBinder and are not mirrored to the DC3 twins),
**but it is front-loaded and nearly stopped**: 11 of 15 default fixes landed
W6-W16 (`recon/R4-motivation-constraints.md:110-113`), the last 12 waves
produced ~3 flips all class (c) — i.e. **render-path edit rate ≈ 0**
(`R4:114-120`), and the ~13 asset-name `strcmp` branches are already
containment-relocated behind flags (`R4:46`). The twins are stabilizing, not
diverging faster. This does not sink the plan (capability-headroom still
stands), but the "compounding" framing oversells one of only two surviving
justifications and should be softened to "front-loaded, now near-static."

### A3 — C5 ("L", 3-5d) is likely optimistic; it is the SYS-2 payoff node, so the miss inflates the headroom ROI

C5 unifies `RB3MaterialBinder` with `MaterialSetup` via a `MaterialParams`
struct (`PLAN.md:308-314`). But R3 rates the two material models "**structurally
different**" — RB3 is GX-TEV fixed-function, DC3 is `BaseMaterial` (0x1f8) full
shader-era superset (`recon/R3-rndobj-divergence.md:64-86`, per-class table
:169). Measured: `RB3MaterialBinder.cpp` = 625 LOC and carries shipped fixes
(UI_TEXT_FLOOR floor at `:145-149` per R4 table). Unifying two *binders* (not
just field-mapping the data) while preserving both games' behavior + DC3's 21/21
matrix is plausibly XL, not L. Since C5 is "the SYS-2 payoff node"
(`PLAN.md:312`), an optimistic estimate there directly inflates the
architectural-ROI case. Recommend re-rating C5 L→XL and gating it behind an
explicit SYS-2 charter, not the general convergence sweep.

### A4 — Duplication-elimination payoff is bounded (~2k LOC) and fix-encumbered — reinforces "code-health, not product value"

Measured twin sizes in `milo-native-engine/src`:

| Convergeable RB3 twin | LOC | DC3 counterpart | LOC |
|---|---|---|---|
| RB3PostProc.cpp | 560 | gfx/PostProcPass.cpp | 378 |
| RB3Quad.cpp | 555 | gfx/DrawRect2D.cpp | 227 |
| RB3MaterialBinder.cpp | 625 | platform/MaterialSetup.cpp | 304 |
| RB3MeshCache.cpp | 308 | platform/MeshGpuCache.cpp | 365 |
| **subtotal** | **~2048** | | ~1274 |

`Rnd_Wgpu_RB3.cpp` (6564 LOC) — the bulk of the RB3 render code — **stays** as
frame-graph owner under Option C (`PLAN.md:139-141`). So the maximum
duplication Option C can retire is ≈2k LOC, and each retirement is gated on
re-homing shipped-fix behavior into the merged TU (RB3PostProc carries
chroma-preserve/UI-post-grade; RB3MaterialBinder carries UI_TEXT_FLOOR). The
payoff is real but small, invisible to users, and encumbered by the very fixes
the campaign spent 31 waves landing. This confirms — does not contradict — the
plan's self-assessment that this is a code-health/headroom investment. Charter
it as such with eyes open; do not let a future wave re-narrate it as defect
work.

---

## Bottom line (ROI)

The bridge premise, stripped of its own honest caveats, would be a NO-GO: ~0-1
open defects, ~2k LOC of fix-encumbered duplication, speculative capability
headroom. But the plan **already priced all of that in** and responded with the
correct ROI structure — a bounded, need-gated, spike-first charter that reuses
DC3's 77-session renderer instead of rebuilding it, and that explicitly reroutes
defect effort elsewhere. That structure survives refutation.

The one thing that must change before charter is **B1**: decouple C0 from the
spike, so the advertised "cheap, bounded, one-wave de-risk" is actually true.
With that fix and the four advisories folded in (especially A1 — add a C4/C5
paper-audit so the go/no-go is predictive of the expensive nodes, and A2/A3 —
stop overselling the drift and under-sizing C5), the plan is safe to charter as
a code-health/headroom study.

**Verdict: GO_WITH_CHANGES.**
