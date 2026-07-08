# Lane DISCRIM — STATUS — VERDICT: **L2-b for BOTH genders (R5-walled animated tear)**

**Headline:** Males = **L2-b**. Females = **L2-b** (same shape, not a distinct tractable
L2-a). The flagship is **R5-walled**, NOT completable and NOT female-only-shippable via a
Layer-2 gender-rest-pose. Lane FIX's Part-1 (`RB3_HANDS_BINDFIX`) is a legitimate
standalone Layer-1 topology+texture fix but leaves the hand in the seed-R ceiling-hand
state; Part 2 as a gender-rest-pose is the WRONG SHAPE (A6/E12) and should NOT be
dispatched.

Measurement lane. Zero BandCharacter.cpp edits. Engine pin `6e6387c` untouched. No default
flips, no pin bumps. New parser script + evidence only. Charter honesty mandate honored:
the coherent-basis regime reaches TIER1 3.1° yet is visually torn — I report the wall
plainly rather than manufacture an L2-a story (the E5/E6 overclaim guard).

---

## Method (A7 verdict logic)

Measured `own`'s per-bone basis vs the authored bind B **at the DRAW frame**, gender-split
(nb=38 male / 40 female), at matched fixed-clock burst frames, using the engine
draw-adjacent instruments (engine `e69a35f`, `RB3_HANDS_ATTACH_PROBE`):
- **TIER1** `off_b·restW_b` = own basis vs authored bind B per bone (own≈B ⇒ ≈0).
- **TIER2 EXACT-joint** `||j_b·P[parent] − j_b·P[bone]||` with the exact bind joint
  `inverse(off_b).v` = the rest-FREE, pose-tracking two-adjacent-bone inter-bone metric
  (HANDS-FIX §Dolphin-fallback's INSTRUMENT, A10 — its "confirms reskin" framing NOT
  inherited). Used COMPARATIVELY vs the matched flag-OFF arm (A7), never as a standalone
  threshold.

All four regimes captured on the SAME binary (`native/build-native/rb3-native`, which
already carries `RB3_HANDS_BINDFIX`), matched fixed-clock burst_03 @ score 575.

## The four draw regimes, MALE hands_naked (nb=38), matched frame 575

| regime | rebound | TIER1 own-vs-B | TIER2 EXACT-joint | visual |
|---|---|---|---|---|
| **flag-OFF** (shipped seed-R) | 1 | **87.3°** uniform | 0.1u | ceiling-hand + spike-web |
| **BINDFIX Part-1** (faithful remap) | 1 | **87.3°** (== flag-OFF) | 0.1u | same spike-fan, **skin OK** |
| **shim-off** (raw per-member, no rebake) | 0 | 3.1° rest → 167-180° anim | med 50 / max 150u | catastrophic whole-hand tear |
| **AUTHORED_REPOINT** (8th cell, BANNED) | 1 | **3.1° count=0 EVERY frame** | med 0.0 / max 8.4u | finger-spike fan TORN |

(FEMALE analog: flag-OFF/BINDFIX TIER1 42.6°; AUTHORED_REPOINT TIER1 3.1° count=0 both,
torn; the 28.9° gap dissolves under authored-repoint.)

## The three load-bearing findings

### 1. BINDFIX Part-1 alone == flag-OFF — Part 1 does NOT reach the coherent hand regime
The scoped shim restores `char_shared.milo → kMerge` (log: `skelBearing=1 bindFixExempt=1`,
the retail bone remap can fire) and preserves skin textures (E11 holds — the burst crop
shows correct skin, only the hand torn). But the hand meshes STILL hit the seed-R rebake
(`rebound=1`), read the **identical 87.3° own-vs-B** basis, and draw the same ceiling-hand
spike-fan. This is the A2-predicted own-bound + clamp-active + rebake regime — Part-1
restoration of the topology/remap does NOT change the hand DRAW composition. Part 1 is a
real Layer-1 fix (per-member topology + textures) but is NOT a hands fix.

### 2. The only regime reaching coherent basis at draw is the BANNED 8th cell — and it tears
`RB3_HANDS_AUTHORED_REPOINT` (keep the authored `inv(B)` offset, repoint to `own`) reads
**TIER1 3.1°, count(>5°)=0 at EVERY draw frame, both genders** — fully coherent basis at
DRAW (not just rest). Yet it is **visually torn** (finger-spike fan, `crop_armREPOINT`),
reproducing the HANDS-FIX Wave-16 visual refutation. This is the **L2-b empirical shape**
(A7): per-bone Tier-1 COHERENT at draw AND visual tear.

### 3. The joint metric UNDER-reads the tear (= the R5 wall, confirmed native)
Comparative TIER2 EXACT-joint, AUTHORED_REPOINT vs matched flag-OFF (MALE 330 / FEMALE 114
matched frames):
- **MALE: median paired delta +0.00u**, but **15% of frames exceed +4u** (up to +8.4u),
  concentrated on the DISTAL finger bones (`ringfinger02`, `middlefinger03`) at high-curl
  poses. So there IS a real, animated, finger-level inter-bone divergence — but it is small
  (≤8u) and intermittent (15% of frames).
- **FEMALE: 0% of frames exceed +2u** — the joint metric reads essentially no separation,
  yet the female hand is equally visually torn.

The joint-point metric therefore **under-reads the tear by orders of magnitude**: the
visible finger-spike fan is a mesh-SHELL / weight-blend divergence *between*
coherently-attached joints (the joint POINTS barely move; the geometry spanning them
shears). This reproduces **R5 VERDICT §0 item 4** exactly: bone-world/joint-attach metrics
cannot faithfully gate the offset-class arms; the animated shell tear is the half that
required articulated Wii ground truth and was **closed WITHOUT it (GT-D)**. DISCRIM cannot
break that wall from the native side — the tear is real and visible, the small finger-level
joint divergence confirms it is genuinely animated, but the coherent-basis-yet-torn
signature (basis 3.1°, joints ≈coherent, shell torn) is precisely the walled animated
question.

## Explaining the arm-S male 3.1° null vs the draw-frame tear (charter mandate)
arm-S read `own`≈B (3.1°) at Poll-cadence FRESHNESS captures — i.e. the coherent-at-rest
frames. My shim-off trajectory reproduces this: TIER1 = **3.1° at frames 3–63** (idle/rest
pose) then diverges once the clip drives. Under AUTHORED_REPOINT the basis stays 3.1° at
EVERY frame (authored offset composed against the freshness-recaptured own rest), so the
rest-basis is genuinely coherent throughout — and the hand STILL tears. Therefore the arm-S
3.1° null is TRUE and NOT contradicted: it is a rest-basis coherence read, and the defect is
NOT a rest-basis error. The defect is the animated inter-bone shell tear (L2-b), which no
rest-basis or basis-coherence measurement can catch — the exact reason R5 walled it.

## Females: no distinct tractable L2-a
The committed 28.9° female TIER1 gap is a flag-OFF / BINDFIX-Part1 **seed-R artifact only**.
Keeping the female mesh's OWN authored offset (authored-repoint) reads **3.1° count=0**,
same as males, and STILL tears. So there is no female-only rest-pose gap for Part 2 to close
that survives as the operative defect — females are the SAME L2-b as males. The A8 "third
outcome" (female-only per-gender-scoped ship) is NOT available on this evidence.

## Fail-red validation (A7)
- **Torso good-body:** greaserjacket EXACT-joint med 0.1 / max 0.3u; jeansripped
  0.0/0.0u — both ARTICULATE (jx-worst 34–89u legitimate) yet joints stay coherent =
  GREEN. Separates cleanly from shim-off hands (50–150u). **PASS.**
- **Known-good joint:** flag-OFF shipped hands EXACT-joint ≈0.1u (design-doc-confirmed
  ≤0.33u/2214 samples, greaserjacket clean control) = coherent joints. **PASS.**
- The metric reads ≈0 on a known-good body and SEPARATES on the torn shim-off hand mesh
  (R2 good-body/bad-torn precedent). Validated.

## Verdict (mechanical, per A7)
- **Males:** per-bone Tier-1 COHERENT at draw (3.1° count=0 under the authored-repoint
  coherent regime) AND VISUAL TEAR present ⇒ **L2-b** = the R5-walled animated inter-bone
  blend tear. Report the wall PLAINLY.
- **Females:** same — **L2-b**, no distinct L2-a rest-pose gap.
- **Completability:** flagship is **R5-walled**. Not completable via a faithful draw regime
  (BINDFIX Part-1 stays seed-R); not female-only-shippable via gender-rest-pose (females are
  L2-b too); the coherent-basis regime is reachable ONLY via the BANNED 8th cell and is
  visually refuted. Part 2 as "pose to B" is the wrong shape (E12) — DO NOT dispatch it.

## Honesty statement
The coherent-basis regime DOES reach Tier-1 3.1°. A dishonest reading would call that an
L2-a success ("we posed own to B"). It is not: the identical regime is VISUALLY TORN, and
the tear is invisible to every basis/joint metric — the animated question R5 already closed
without articulated Wii ground truth (GT-D). This is a sharp, gender-split, mechanism-decided
NAMED WALL, which the charter pre-authorizes as an accepted outcome.

## Evidence (`execution/W21-DISCRIM/evidence/`)
- `parse_hands_attach.py` — the HANDS_ATTACH block parser (gender-split, TIER1/TIER2).
- `crop_smoke_OFF_burst03_rhand.png` (flag-OFF ceiling-hand),
  `crop_armBINDFIX_burst03_rhand.png` (Part-1: skin OK, hand still torn),
  `crop_armNOSHIM_burst03_rhand.png` (raw shim-off catastrophic tear),
  `crop_armREPOINT_burst03_rhand.png` (coherent-basis 8th-cell finger-spike tear = L2-b).
- Engine logs (regenerable): `/tmp/w21-discrim/arm{OFF,BINDFIX,NOSHIM,REPOINT,TORSO}_engine.log`.
- Regen: `RB3_FIXED_CLOCK=1 RB3_HANDS_ATTACH_PROBE=hands_naked [<arm flag>] python3
  scripts/native/keyboard-to-gameplay.py --bin native/build-native/rb3-native
  --song-downs 3 --game-burst 6 --burst-interval 0.3 --out /tmp/w21-discrim/<arm>`
  where `<arm flag>` ∈ {`` , `RB3_HANDS_BINDFIX=1`, `RB3_LOADBIND_NOSHIM=1`,
  `RB3_HANDS_AUTHORED_REPOINT=1`}.
- Checkpoint: `/tmp/wave21-checkpoints/DISCRIM.json`.
