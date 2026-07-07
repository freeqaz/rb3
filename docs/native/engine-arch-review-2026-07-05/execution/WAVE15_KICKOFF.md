# Wave 15 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE15_REVIEW.md` rb3 `fd9f32f4`) — **all 9
amendments adopted**; dispatched with the corrected shape below.
Parent: `execution/README.md` (Wave 14 results + Wave 15 menu). Engine pin `fdf0ad9`. Nine
defaults ON.

## COORDINATOR ACCEPTANCE (2026-07-07) — final dispatched shape

- **A1 (Lane H) — derivation re-anchored at the WII COMPOSITION** `v·A_b·live_b` (already exposed
  natively behind `RB3_NO_HEAD_REBIND`): the record's decisive syllogism is that identical
  authored data shards natively under that composition too — so the defect is (a) native
  `own_live(t)` bone worlds or (b) the offset↔bone pairing, UPSTREAM of all 7 dead bakes. ONE
  cheap pre-registered baseline discriminates: the Wii-composition arm with reskin OFF (only the
  reskin-contaminated variant of that arm was ever measured, 136u). The rebake equation stands
  (verified `BandCharacter.cpp:1750-1758` + `Rnd_Wgpu_RB3.cpp:3299-3305`) with two footnotes
  (4-bone blend; CHAR-vs-WORLD space mix) but is itself an HX_NATIVE workaround, not the anchor.
- **A2 (Lane H) — mandatory raw-number reproduction:** the adjudicator must re-derive 87.3° from
  committed `W2.8f/evidence/readings.txt` (noting its documented 42.6/87.3 BIMODALITY — not a
  constant) and the mixed-sign per-bone gaps from `SKEL/evidence/apd_diag_gameplay_grep.log`
  (**rescued from /tmp by the coordinator pre-dispatch, this commit**) before trusting either.
- **Ground truth (R-A):** decomp source (structural), rb3-viewer pose/offset dumps (asset
  numeric), **Dolphin + milo-trace (runtime numeric — decisive)**, DWARF/DC3 (corroboration).
- **A3/A4 (Lane B, CRITICAL) — mechanism prior REFUTED at pixel level:** the native focused
  song_select row is white text on a DARK NAVY fill (ROI p60 luma=15), not a bright bar — the
  hub's "bar bleeds through AA text" mechanism does NOT transfer. Prime suspect = the
  **z-occluded selection quad** U-CLEAN found, whose occlusion the now-default
  `RB3_UI_POST_GRADE` depth `LoadOp::Load` deliberately preserves (the real R-C interaction is
  DEPTH, not grade). Retail truth confirmed (black-on-white, both row types). Gate corrected:
  the old [1.06,1.17] parity band would be VIOLATED by a real fix and a wrong-polarity screen
  reads "legible" — use a directional two-region gate (bar-fill luma ≥ threshold AND text-stroke
  darker than fill, retail-calibrated) + the SETLISTS red-band re-check. The fix is probably
  engine-side: **pre-authorized grant = the menu-flush depth handling in `RB3PostProc.cpp` (per-
  quad or selection-pass exception), NOT a blanket LoadOp revert** — the red-band fix must
  survive.
- **A5 (Lane N) — decided from Wii source:** the provider is the `PlatformMgr::GetName` weak NULL
  stub (`dta_link_stubs.s`) behind `AppLabel::SetUserName`/`user_name.lbl`; Wii
  `PlatformMgr_Wii.cpp:489-496` falls back to localized "Player N" — implement THAT in
  `native/src/rb3_platform_native.cpp` (one provider fixes header + overshell + all consumers).
  Hiding would be unfaithful.
- **Collisions:** Lane H restricted to EXISTING probes (no new engine TU edits) → H↔B matrix
  empty. Stale `RB3PostProc.h:81` "default-OFF" comment noted for cleanup in Lane B's commit.
- Nine-defaults tally verified at accessor/opt-out level.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

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
