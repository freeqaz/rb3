# Retrospective Roadmap — instruments first, then cash them in

**Status:** ACTIVE. Owner-requested implementation roadmap for the 15-wave retrospective's
findings (`REPORT.md` §4, `OPTIONS.md` §2-5), updated with the **Wave 16 outcomes** (ROWFIX
shipped → item R3's flip-package goal is DONE; the hands adjudicated cell was refuted by the
visual gate → R1's probe spec is refined to inter-bone relative poses, and R5 is added).
Detailed per-item implementation plans live in `plans/PLAN-R<N>-*.md` (Fable-authored, one per
item).

## The thesis (one paragraph)

Fifteen waves shipped eleven default-ON fixes with zero regressions — the gate/discipline layer
works. What burned ~9 waves was the **diagnosis instrument layer**: no external ground truth
(three premise inversions were native-probe-only artifacts), unvalidated oracles used as gates
(`wext>60` gated ~10 waves while invalid), and a known RNG noise floor under every visual gate.
The roadmap buys the three missing instruments (~4-5 lane-waves), adopts ten no-tooling process
lints, and only then returns to the open bugs — each of which sits inside the new instruments'
coverage.

## Items

| # | Item | What it is | Primary consumer | Est. cost |
|---|---|---|---|---|
| **R1** | **Dolphin ground-truth probe** | Headless Dolphin boot of the on-disk wbfs + map-driven memory read → per-bone matrices; **spec refined by Wave 16: two-adjacent-bone RELATIVE-pose comparison** (inter-bone deltas at matched clip time vs native), not single-bone WorldXfm. Thin probe, NOT full milo-trace M3. | Hands close-out (R5); every future skinning/anim claim; eventual true pixel goldens | 1-2 waves |
| **R2** | **Skinning oracle fixtures + oracle-validation harness** | Arm-W/arm-S methodology → permanent gender/mesh-split, matrix-relative gtests in `rb3-tests`; plus the rule-as-harness: any proposed gate metric must demonstrate known-good/known-bad separation before it may gate a wave. | Prevents the next hands-saga (W2.4 BandPatchMesh is queued); locks the 11 shipped defaults | 0.5-1 wave |
| **R3** | **UI render forensics: `/api/uidump` + drawlog provenance** | Scene-graph dump (authored xfm/show/draw-order/material color vs drawn rect) + per-draw provenance; killer query = pixel ROI → which draw wrote it, from which authored object, with what z/blend state. | Sidebar backing quad, partdiff polish, every future UI parity item (UI = 7 of 11 shipped flips) | ~1 wave |
| **R4** | **Scoped loader determinism + per-axis ledger** | Per-consumer isolated Rand streams for the three named rejection-sampler families (CameraShot / Crowd / mAnims walk) behind `RB3_LOAD_DETERMINISM`; standing per-axis PASS/FAIL ledger (count / order / stream-position / clock). PRIMARY = 10/10 identical stream position. | WHITE guard re-grade, wash FX-phase co-sampling, every per-boot numeric visual gate | 1-2 waves |
| **R5** | **Hands endgame: true reskin decision** | The one coherent remaining option after 8 dead offset/repoint cells + the refuted positions-only re-pose: an engine per-member TRUE reskin (verts+**weights** re-derived in the own-skeleton basis) — or an evidence-based CLOSURE with `RB3_NO_SKIN_CLAMP` as the shipped mitigation. Decision gated on R1's inter-bone ground truth; no more native-only hands verdicts. | The flagship remaining visual bug | decision ≤1 wave after R1; fix TBD by plan |
| **R6** | **Process lints → kickoff template** — **DONE** (`execution/KICKOFF_TEMPLATE.md`, Wave 17 lane T) | The ten no-tooling rules from `OPTIONS.md` §4 (matrix-relative+pointer-verified bone claims; gender/mesh-split defaults; no unvalidated oracles; shipped-flag contradiction grep; wide diagnosis grants; option-table before 2nd fix attempt; committed evidence; flag hit-counts; flavor-membership grep; instruments-before-fixes) written into the standing wave-kickoff template as a MANDATORY pre-dispatch checklist. | Every future wave | 0 (doc change) |

## Sequencing

- **Wave 17 — instrument wave (no gameplay fixes):** R1 milestone 1 (Dolphin boots + one
  scripted inter-bone read) ∥ R2 (fixtures reproduce the arm-W verdict table) ∥ R3 (uidump
  reproduces the W14 red-band diagnosis from the shipped build). R6 lands in the Wave-17
  kickoff itself.
- **Wave 18 — cash-in:** R5 decision (hands close-out against R1 ground truth); remaining UI
  polish (sidebar quad option, partdiff confirmation) gated by R3; W2.4 BandPatchMesh becomes
  safe to attempt under R2's fixtures.
- **Wave 19 — determinism:** R4 PRIMARY 10/10 → immediately re-grade the held
  `RB3_VENUE_WHITE_GUARD` on the now-resolving gate + dispatch the wash per-FX co-sampling
  instrument.
- **Standing carried items** (4→8 lights, sidebar quad authoring, web-build confirmation pass)
  slot behind these.

## Ground rules for the implementation waves

Everything in `OPTIONS.md` §4 applies. Additionally: R1's first milestone is a go/no-go (if the
debug DOL won't boot headless in ~a day of effort, fall back to retail-DOL capture and re-price);
R5 does not start until R1 reports; every plan's gates must name their fail-red demonstration.

## Current state anchors (2026-07-07)

Engine pin `51640ff`. Eleven default-ON fixes. Open bug ledger: hands/fingers (R5), venue WHITE
lever + wash FX-phase residual (R4-gated), boot determinism (R4), sidebar backing quad
(authoring polish), 4→8 lights (DC3-gated), W2.4 BandPatchMesh (R2-gated). Campaign hub:
`docs/native/engine-arch-review-2026-07-05/execution/README.md` (per-wave results).
