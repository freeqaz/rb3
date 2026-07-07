# Wave 15 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** DRAFT — under Fable pre-dispatch review.
Parent: `execution/README.md` (Wave 14 results + Wave 15 menu). Engine pin `fdf0ad9`. Nine
defaults ON.

## Where we are

The UI-parity front is nearly clear (two Wave-14 flips). What remains splits into one deep
adjudication and two small diagnostics:

1. **Hands: 7 measured dead artifacts + 3 premise inversions.** Wave 14's reskin proved the shard
   survives ANY rest-shape/offset change by construction (with the default rebake,
   `skinPos(t)=v'·meshWorld·inv(own_rest)·own_live(t)` — the live-vs-rest delta is invariant).
   The defect is in how `own_live(t)`'s rotation basis relates to the verts' authored bind. NO 8th
   fix attempt without a synthesis: the option space must be adjudicated from the full record.
2. **Bar-bleed focused-text polarity:** song_select highlighted row (white-on-yellow vs retail
   black-on-white) + partdiff GUITAR are grade-INERT (UIGRADE measured PP_OFF==default there) —
   different mechanism than the hub fix (candidates: focus-state color route on these specific
   labels, bar quad compositing through AA glyph alpha).
3. **"(null)" gamertag stub** at the song_select header (revealed by the art fix): profile/account
   text subsystem returns null natively — small diagnosis + fix or hide.

## Proposed Wave 15 lanes

**Lane H — HANDS-ADJUDICATION (Fable, synthesis + adversarial derivation, NO fix landing):**
Read the complete hands record (W2.2, W2.8 through W2.8g, SKEL, RESKIN STATUS docs + the probe
logs they cite + CHAR_SKINNING_DEFORM_INVESTIGATION.md + the in-source investigation block).
Deliverables, in order of preference:
(a) a proof-level derivation of the correct fix — starting from the one uncontested equation
(the rebake makes skinPos depend only on own_live·inv(own_rest) and the vert positions) derive
what SHOULD own_live/own_rest/verts be for a correct hand, identify which factor is wrong
against Wii ground truth (what does the WII build compute for these bones — can dc3-decomp or
the decomp source answer what CharBonesMeshes/SetBone do on Wii for hands?), and specify the
minimal change;
(b) if underdetermined, the exact missing measurement (one experiment, pre-registered);
(c) if the record proves the option set closed, say so — RB3_NO_SKIN_CLAMP remains the shipped
mitigation and the item is retired to a "needs asset-pipeline work" backlog with the evidence.
Constraint: the deliverable is `execution/HANDS-ADJUDICATION/VERDICT.md` — arguments grounded in
the record + source, no new fix code (probes allowed).

**Lane B — bar-bleed polarity (Opus, diagnosis→fix flag-first):**
Establish the retail-truth first: in retail, the song_select highlighted row is BLACK text on a
white/yellow bar; natively it is white text. Trace the focused-row label's color route (same
method as C1: probe the material color reaching the shader for the highlighted row) — is the
focus-state color absent, inverted, or overridden natively? Fix flag-first game-side; gate =
A11-style percentile contrast on the highlighted row (retail-calibrated) + no regression on the
hub (now grade-exempt) + drawlog 792.

**Lane N — "(null)" gamertag (Sonnet, small):**
Find the provider (profile/gamertag accessor returning null natively), decide fix-or-hide
(retail shows the profile name; native has no profile subsystem — a sensible native default like
"PLAYER 1" or hiding the label both acceptable; pick with evidence of what other screens do).
Flag-first; E1 capture.

## Process rules (carried)

Locks, checkpoints (`/tmp/wave15-checkpoints/`), commit-per-review-cycle, PLAN/STATUS per item,
append-only classjson + coordinator regen, own build dirs, no flips/pin bumps by lanes, refuted
flags UNSET (incl. RB3_HANDS_RESKIN), pgid-only cleanup, settle by frame count.

## Risks / open questions for the reviewer

- **R-A:** Lane H's ground-truth question — what CAN establish the Wii-correct relationship
  (decomp source? DC3? Dolphin? the Bank-5 DWARF)? If nothing can, is (b) well-posed?
- **R-B:** Lane B may rediscover C1's dead ends — the acceptance must import the C1/UIGRADE
  findings (focus color IS applied and reaches the shader on the HUB; these screens are
  grade-inert) so the lane starts past them.
- **R-C:** any interaction between Lane B's label-color work and the now-default-ON UI post-grade
  flush on those screens?
