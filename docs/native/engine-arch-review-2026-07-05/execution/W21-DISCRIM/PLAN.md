# Lane DISCRIM — PLAN (Wave 21, the male-coherence / L2-a-vs-L2-b decider)

**Charter:** WAVE21_KICKOFF.md Lane DISCRIM + WAVE21_REVIEW.md A6/A7/A9/A10.
Measurement lane, rb3-side scripts + small probe extensions only. NO BandCharacter.cpp
edits (that is Lane FIX's file). Engine pin `6e6387c` untouched, no default flips, no
pin bumps.

## The decisive question (A7 verdict logic)

Under **per-member binding**, are MALE hands at the DRAW frame:
- **L2-a** — per-bone basis DIVERGENT at draw (own_rest ≠ authored bind B): tractable
  rest/gender-pose fix; OR
- **L2-b** — per-bone basis COHERENT at draw (Tier-1 ≈ 0, own ≈ B) AND a VISUAL TEAR is
  present at the matched frame: the R5-walled *animated* inter-bone question.

Females: the committed 28.9° gender rest-basis gap ⇒ prior L2-a (tractable).

## The reconciliation crux (why "measure at the DRAW frame")

- **arm-S** read male own ≈ B at REST (Tier-1 3.1°, 1038 blocks) — Poll-cadence freshness.
- **Lane-N** read shim-off flings at rebind ENTRY.
- **HANDS-FIX 8th cell**: Tier-1 3.1° BOTH genders (rest-coherent) YET visually TORN at
  animated poses. The tear lives between adjacent finger bones at ANIMATED draw frames.
- **R5 CLOSURE**: whether that tear is genuine skeleton divergence (L2-b) or downstream of
  Wii-faithful bone worlds (GT-A) is **walled** — no articulated Wii ground truth.

So a rest-only Tier-1 read cannot decide this. I must read the basis AT the animated draw
frame and, comparatively, the inter-bone joint-attach tear vs the matched flag-OFF arm.

## Instruments (all exist; small extensions at most)

1. **Draw-frame Tier-1** — engine `RB3_HANDS_ATTACH_PROBE` TIER1 line (engine `e69a35f`,
   draw-adjacent, palette-build side). `off_b · restW_b` per bone; freshness-validated by
   BoneTransAt pointer identity. Reads own-vs-B basis. **Caveat to handle:** rest is
   cached first-seen per bone-pointer, so it reads coherence at the CAPTURE frame; the
   per-frame TIER1 line still reports whether own's basis drifted from B (recap on pointer
   change). Gender-split by member (nb=38 male / 40 female).
2. **Tier-2 joint-attach (PRIMARY, rest-FREE, pose-tracking)** — same probe, TIER2 line.
   `||j_b·P[parent] − j_b·P[bone]||` on the uploaded palette. This is the
   two-adjacent-bone inter-bone metric HANDS-FIX §Dolphin-fallback names (INSTRUMENT spec
   only, A10 — do NOT inherit its "confirms reskin" framing). Reads the ANIMATED tear at
   EVERY draw frame. Has A5 fail-red `RB3_HANDS_ATTACH_PERTURB`.
3. **CharBone-world inter-bone** — `d4-bonedump-sweep.py` + `interbone_framematch.py`
   (D_rel_rot) as cross-check if needed.
4. **Substrate** — `RB3_LOADBIND_NOSHIM` (interim per-member binding; textures white,
   irrelevant to basis). Watch `/tmp/wave21-checkpoints/FIX.json` for `RB3_HANDS_BINDFIX`.

## Steps

- **S0** Fail-red validation: TIER2 must read ≈0 on a known-good body mesh (torso) and
  SEPARATE on the torn hand mesh (R2 good-body vs bad-torn precedent). Also demo
  `RB3_HANDS_ATTACH_PERTURB` reads RED so ≈0 is a real GREEN.
- **S1** Drive per-member binding (shim-off) to gameplay, matched fixed-clock bursts.
  Capture HANDS_ATTACH TIER1 + TIER2 for hands_naked, gender-split, per member.
- **S2** Capture the matched flag-OFF arm (shipped default, shim-ON) at the SAME frames.
- **S3** Comparative TIER2: flag-ON hand tear vs matched flag-OFF arm (raw inter-bone
  divergence is legitimate articulation — the comparison is the signal, A7).
- **S4** Matched-frame crops with per-member attribution.
- **S5** VERDICT: males L2-a or L2-b? females? Explain the arm-S 3.1° rest≈B null against
  the draw-frame reading. Write measured L2 reading to checkpoint (EARLY, for Lane FIX A6).

## Honesty mandate
If males read rest-coherent at draw AND torn ⇒ L2-b = the R5 wall. Report PLAINLY. Do NOT
manufacture an L2-a success story (the E5/E6 overclaim the Wave-20 close-out caught). A
sharp named wall is an ACCEPTED outcome.
