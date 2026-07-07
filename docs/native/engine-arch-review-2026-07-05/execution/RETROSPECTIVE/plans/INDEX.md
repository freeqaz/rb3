# Retrospective plans — INDEX + cross-plan cross-check

**Author:** Fable cross-check pass, 2026-07-07 (read-only over `PLAN-R1..R5`, `ROADMAP.md`).
**Scope:** consistency across the five per-item implementation plans (`PLAN-R<N>-*.md`) for the
15-wave retrospective roadmap. This file is a MAP + a mismatch ledger for the coordinator to
resolve — it does NOT edit any plan. Five plans expected, **five present** (R1–R5). R6 (process
lints → kickoff template) has **no plan file by design** — ROADMAP prices it at 0 (doc change
folded into the Wave-17 kickoff), not a missing plan.

---

## Per-plan summary

**R1 — Dolphin ground-truth probe (`PLAN-R1-dolphin-probe.md`).** *Objective:* a one-command
probe that boots the real game headless under the built Dolphin fork (Bank-8 debug DOL + retail
disc via `DefaultISO`; retail-DOL name-scan fallback), pauses via RSP, discovers `CharBone`
instances by vtable scan (`__vt__8CharBone` @ `0x80bfeaa8`), and emits a **two-adjacent-bone
RELATIVE-pose diff** (`D=inv(W_parent)·W_child`, `delta_deg=angle(D_wii·inv(D_native))`) between
Wii and native joined at matched `(member, clip, frame)`, plus the Wii-ARK-vs-360-ARK authored-
bind table. *M1 go/no-go:* an interactive-scene headless boot + one sane named `CharBone` matrix
(map-vtable route A, or retail name-scan route B); NO-GO if neither boots within the ~1-day box →
priced report + STOP. *Cost:* 1–2 lane-waves (matches ROADMAP).

**R2 — Skinning oracle fixtures + oracle-validation harness (`PLAN-R2-skinning-fixtures.md`).**
*Objective:* promote the arm-W/arm-S adjudication into permanent gender/mesh-split matrix-relative
gtests in `rb3-tests`, plus a `ValidateMetric(good,bad)→SeparationReport` harness with a registry
rule ("no metric gates a skinning wave without a demonstrated known-good/known-bad separation").
Centerpiece is `M_BlendSpread`, a multi-bone-blend tear metric designed to read RED on the Wave-16
`RB3_HANDS_AUTHORED_REPOINT` spike-fan that Tier-1/Tier-2 both passed GREEN. One engine edit: an
additive env-gated palette-dump block in `Rnd_Wgpu_RB3.cpp` (~L4736). *M1 go/no-go:* does any
palette+verts+weights metric separate good-body vs bad-torn at ≥3× zero-overlap, gender-split? GO
→ that metric becomes the tear gate; NO-GO → descope to Tier-1/2 + verdict-table pins, flag tear
gate R1-blocked. *Cost:* ~1 lane-wave (ROADMAP 0.5–1).

**R3 — UI render forensics: `/api/uidump` + drawlog provenance (`PLAN-R3-uidump.md`).**
*Objective:* the standing UI-forensics instrument the campaign re-improvised ≥5 times — a
scene-graph dump (`GET /api/uidump`) joined to a provenance sidecar on the existing drawlog (mesh/
mat/cam names, screen rect, render-pass index + depth LoadOp, game-side panel/owner scope stack),
with a pixel-ROI killer query. Engine edits are additive gated blocks in the rb3-flavor TUs
(`Rnd_Wgpu_RB3.cpp` prov sidecar ~L5324, `RB3DrawLogDebug.h`, `RB3MaterialBinder.cpp`,
`RB3PostProc.cpp` menu-depth knob) + new rb3 TU `rb3_uidump.cpp`. *M1 go/no-go:* the 792-draw
`drawlog-golden` stays byte-identical with prov compiled-but-off, AND `?prov=1` on song_select
shows a sane rect for the focused fill quad with `highlight_yellow.mesh` absent; degenerate rects
>20% → fall back to sphere-only rects before proceeding. *Cost:* ~1 wave (matches ROADMAP).

**R4 — Scoped loader determinism + per-axis ledger (`PLAN-R4-loader-determinism.md`).**
*Objective:* make the landed `RB3_LOAD_DETERMINISM` seam *sufficient* — 10/10 boots with identical
post-anchor gRand stream position under jitter — by moving the *measured*-divergent variable-count
consumers onto per-tag isolated `Rand` streams (`RB3LoadDetStream`), decoupling count from order by
construction; plus a standing per-axis (count/order/stream/clock) PASS/FAIL `ledger.json`. Corrects
the inherited "three named families" list (Wind REFUTED as a live consumer; Crowd liveness
unproven; only CameraShot::Shake confirmed) → buys the attribution instrument FIRST. Edits are
HX_NATIVE-gated in `Rand.{h,cpp}` + one-line reroutes at M1-named consumer sites; no overlap with
the render TUs. *M1 go/no-go:* attribution names a small (≤~8) divergent-caller set → proceed; >~8
diffuse → STOP + re-price (walk-granularity or ledger-only landing). *Cost:* 1.25–1.75 lane-waves
(ROADMAP 1–2). Independent of R1–R3.

**R5 — Hands endgame: true reskin decision (`PLAN-R5-hands-endgame.md`).** *Objective:* close the
hands/fingers bug family with an evidence-based verdict decided against R1's ground truth — either
a landed gated fix (B1 anim-basis correction preferred, B2 true verts+weights reskin fallback) or a
documented CLOSURE with the mitten mitigation. Rests on a §0 derivation proving every runtime-
capturable anchor is a dead cell (seed-R = Wave-14, ≈B = Wave-16), so the ONLY non-dead reskin has
an EXTERNAL (R1) anchor; the branch (GT-A/B/C/U/D) is a mechanical table lookup on R1's numbers
against pre-registered thresholds. **Hard-gated: does not start until R1 reports; §2.4 is the
binding artifact contract.** *M1 go/no-go:* apply §3.1 thresholds via a ~100-line committed script
→ a branch is named by the rules alone, else "GT-C-indeterminate → re-price" (fail-safe). *Cost:*
0.5 lane-wave decision (matches ROADMAP "≤1 wave after R1") + prior-weighted ≈1.3–1.6 for the fix.

---

## Cross-plan dependency graph

```
                 R6 (kickoff-template lints; doc-only, no plan file) ──▶ every wave

   Wave 17 (instruments) ───────────────────────────────────────────────────────
     R1·M1 ┐         R2 ┐                         R3 ┐
           │            │                            │
   Wave 18 (cash-in) ───┼────────────────────────────┼──────────────────────────
     R1·M3/M4 ──HARD──▶ R5 (decision; §2.4 contract) │
            │           ▲  soft: R2 Suite D grades    │  soft: R3 G3/G4 retrodictions
            │           │  the R5 reskin/closure       │  double as R2 rule-as-harness proofs
            │        R2 ┘──▶ W2.4 BandPatchMesh (ext)  R3 ──▶ sidebar quad, partdiff (ext)
            └── soft: R2.M_InterBoneRelPose = R1 native-side comparand
   Wave 19 (determinism) ────────────────────────────────────────────────────────
     R4 (independent of R1–R3) ──▶ WHITE-guard re-grade + wash per-FX co-sampling substrate
```

- **R1 → R5: HARD gate.** R5 §2.4 pins the exact R1 artifact (items 1–5); R5 bounces to R1 on any
  gap rather than improvising ground truth. R1's M4 "R5 asks" trigger and R5's M0 M4-request path
  are mutually pre-registered — clean handshake.
- **R2 → R5, R3 → R2: soft.** R2 Suite D is the pre-registered grader for the R5 fix; R3's G3/G4
  retrodictions double as separation proofs feeding R2's registry rule. Neither is a start-gate.
- **R4: independent** (ROADMAP: "no hard dependency on R1–R3").
- **R6:** doc-only, threads through every wave's kickoff; no plan file.

---

## MISMATCHES FOUND (coordinator resolves — plans NOT edited)

**M-1 [MEDIUM] — R2 ∥ R3 concurrently edit `Rnd_Wgpu_RB3.cpp` and each independently bumps
`MILO_ENGINE_PIN` in Wave 17.** R2 adds an additive env-gated palette-dump block (~L4736,
`RB3_PALETTE_DUMP`); R3 adds an additive env-gated provenance sidecar (~L5324, `RB3_DRAWLOG_PROV`)
+ a menu-depth knob in `RB3PostProc.cpp`. ROADMAP sequences both in Wave 17. Neither plan
references the other's engine edit, yet each says "commit engine first, bump `MILO_ENGINE_PIN`"
(R2 §3.2/G7, R3 §M4/§7). Two lanes editing the same engine file and racing the pin is exactly the
concurrent-engine-edit failure mode R2's own risk R-d names. *Not* a strict exclusive-ownership
violation — neither claims exclusivity, and the blocks are in different regions — but a genuine
merge/pin race. **Resolution options for the coordinator:** serialize the two landings (one pin
bump lands, the other rebases), or fold both additive blocks into a single engine commit + one
pin bump. (R5's future engine touch to `Rnd_Wgpu_RB3.cpp` — the mitten — is Wave 18 and
conditional, so it does not add to the Wave-17 race.)

**M-2 [LOW] — Forearm anchor row asymmetry between R1 and R5.** R5 §2.4.1 and §3.1 require BOTH
"wrist/forearm anchor rows" (the GT-U "defect upstream of hands" branch keys on the forearm→wrist
pair reading ≤ ε). R1 §3.7 explicitly promises only "`wrist→hand` as anchor row"; the forearm→wrist
pair is not named in R1's pair list (`hand→finger01`, `01→02`, `02→03`, wrist→hand). R1 dumps the
full parent chain so the datum is *obtainable*, but the deliverable table as specified would leave
R5's forearm-anchor clause unsupported. **Resolution:** R1 should emit the forearm→wrist anchor pair
in its §3.7 table (one added pair row), or R5's GT-U forearm criterion narrows to wrist-only.

**M-3 [LOW] — Inter-bone quantity basis mismatch (bone-world vs composed-palette).** R1 §3.7 defines
its inter-bone delta over bone `WorldXfm` (`D = inv(W_parent)·W_child`), and R5 §2.4/§3.1 consume
exactly that world-based delta — R1↔R5 agree. But R2's `M_InterBoneRelPose` (§3.3) computes the
same-named "two-adjacent-bone relative pose" over composed **palette** matrices
(`angle(P_b·inv(P_p))`, palette = offset·world) and labels itself "R1's future native-side
comparand" (§7). Palette ≠ world, so as written R2's diagnostic is not directly diffable against
R1's native dump. **Resolution:** pin one basis for the R1↔R2 comparand (R1 dumps both worlds and
offsets, so it can compute either) so the "native-side comparand" claim is literally true.

**No mismatch found on:** the R1↔R5 core artifact contract (R5 §2.4 items 1–5 each map 1:1 to an R1
deliverable; the `angle(D_wii·inv(D_native))` quantity is byte-for-byte the same expression on both
sides); R2's coverage of the Wave-16 failure mode R5 describes (R2 `bad-torn` arm =
`RB3_HANDS_AUTHORED_REPOINT=1` captured at animated game-burst frames + `M_BlendSpread` +
`BlendSpreadSeparatesTornBlend` = the exact multi-bone-blend-at-articulation tear R5 §0/§2.1
describes); and cost totals — every plan's self-estimate lands inside its ROADMAP row (R1 1–2 ✓,
R2 ~1 in 0.5–1 ✓, R3 ~1 ✓, R4 1.25–1.75 in 1–2 ✓, R5 0.5 decision + fix TBD ✓).
