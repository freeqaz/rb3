# WAVE20_CLOSEOUT_REVIEW — Fable adversarial close-out (Wave 20)

**Reviewer:** Fable. **Date:** 2026-07-08. **Target:** `W20-SYNTHESIS/SYNTHESIS.md` + the
three lane STATUS files. Evidence re-checked from the committed JSONs/tables, not trusted
from prose. **Verdict: ACCEPT-WITH-ERRATA** — the Layer-1 mechanism is genuinely proven
and the wave's core join stands, but the Layer-2 (A11) section asserts a mechanism the
committed record partially CONTRADICTS, and two lane files carry stale/banned shorthand.
Errata E4–E6 are BINDING before Wave-21 dispatch.

## Q1 — Layer 1 ("shim suppresses the retail remap") — ACCEPT-WITH-ERRATA

The counted core is solid: br2=br3=0 shipped vs 31,488/member shim-off, all members
(`branch_hitcount_table.md:16-37`), the kReplace-never-iterates-Filter mechanism
(SYNTHESIS:33-42, matches Lane D's structural audit), and the 205/205→own topology flip
(N/STATUS:40-51). The supersession of the 2026-06-06 record is correctly scoped to hand
meshes at rebind entry (SYNTHESIS:44-47; N/STATUS:55-59). Four overstatements:

- **The join table's Wii column overclaims** (SYNTHESIS:29): "hand-mesh bone binding …
  60/60 rows". Committed `wii_binding_gameplay.json`: only **6/60 rows are hand-mesh**
  (`hands_naked.mesh`, member 0 ONLY); the other 54 are jacket/jeans/strap outfit meshes;
  members = {0:36, 1:18, 2:6}, **member 3 has zero rows**; 3 distinct bound dirs. Lane W's
  own headline says "hand/outfit-mesh" (W/STATUS:10) — the synthesis compressed it to
  "hand-mesh". Hand-specific Wii evidence = 6 bones + one non-reproducible 38-bone
  `drivinggloves_resource.1.mesh`→OWN sighting (W/STATUS:73-74, offset_derivation.md:57).
- **"exactly matching Wii's topology"** (SYNTHESIS:40) — it matches at the A10
  `owningDirClass` level on every *reachable* row. Full 38-slot per-slot uniformity on
  Wii is walled, and slot-level MIXING is a real failure mode in this system (native 8/46,
  HANDS-ADJUDICATION/VERDICT.md:59-61). "Exactly" should be class-scoped. (E2)
- **"native-introduced regression"** (SYNTHESIS:40-42) — fair for the load-path topology,
  but reads as if the shim caused the hands bug. It didn't: the female fling predates the
  shim (2026-06-05, HANDS-ADJ:62-64) and shim-off flings WORSE. (E3)
- **"separable in principle"** (SYNTHESIS:49-52) — SPECULATIVE. The skeleton-scoped bypass
  arm was chartered as the contingency if the full arm diverged (WAVE20_REVIEW:80-82) —
  the full arm DID diverge, and the scoped arm was never run/reported. Worse, the
  synthesis's own line 36-38 concedes `char_shared.milo` carries the texture palette
  ("not just the texture palette it was written for") — restoring kMerge on it may
  reintroduce the white-texture bug. The Wave-21 gate list has NO texture-integrity gate. (E11)

## Q2 — Layer 2 / A11 discharge — REVISE (repaired by E4–E6; this was the near-blocker)

The **necessity** half is discharged: per-member binding alone fails the visual gate,
counted + photographed (N/STATUS:40-51). Three real problems in the mechanism half:

- **The "same composition as the 8th cell" identity (SYNTHESIS:61-65) is not established.**
  Lane N measured binding at rebind ENTRY (A2, pre-mutation). The shim-off arm set no
  banned flag (N/STATUS:86), so the shipped seed-R rebake in `RebindHeadHandsAtRest`
  (HANDS-ADJ §2, :96-99) still runs — the draw-time composition is plausibly
  `inv(R)·L_own` (**dead cell #1, DEFAULT**, HANDS-ADJ:123), not the 8th cell's
  `inv(B)·L_own`. Draw-time offsets were never dumped. Lane N's own interpretation blames
  the seed-R problem (N/STATUS:71-73), not inv(B). "Two independent arms reach that
  composition" is therefore one arm and one inference.
- **The 87° number is misattributed** (SYNTHESIS:70-72: "Native's fresh kInlineCached
  per-member skeleton is the raw male-template bind — 87° off B"). VERDICT §2: 87.2° =
  `angle(B·inv(R))` for the SetDeformation-time **transient seed R**, whose provenance is
  "deliberately left open" (HANDS-ADJ:111-113) — NOT a measured rest basis of the fresh
  skeleton. Worse, **arm S measured the male `own` ≈ B (Tier-1 3.1°, all 1038 blocks)
  during play** (HANDS-ADJ:91-94, 96-99 "(≈B)") — standing COUNTER-evidence to L2-a for
  males. Only the female ~29° (authored-female offsets vs the shared male bind) is
  committed evidence of a gender-basis gap.
- **L2-a stated as fact**: "Gender/bind-posed per-member rest basis is the missing
  sufficient ingredient — corroborated three ways" (SYNTHESIS:74-75). None of the three
  corroborations tests SUFFICIENCY, and one of them (VERDICT §2) contains the male
  counter-result above. The L2-a/L2-b hedge at :77-90 is correctly placed and names the
  torso tension honestly — but it cannot repair a bolded "is the missing sufficient
  ingredient" three paragraphs earlier. The section header's "(mechanism inferred)" is
  the right register; the body escapes it.

## Q3 — GT-D "closure untouched" — ACCEPT-WITH-ERRATA

No articulated-capture claim is made; the static/load-time framing (SYNTHESIS:100-106)
holds and the (b)-at-load class placement is per the adjudication's own §1. One slip:
**"On Wii the outfit/clip poses each per-member skeleton to its gender"** (SYNTHESIS:72-73)
is an unevidenced Wii ANIMATION-mechanism assertion (substrate CharClipDrivers=0; no Wii
capture shows any clip posing anything). It does not trip the reopen condition (that
requires a capture, CLOSURE:62-66), but it breaks the A11 no-animation-claims discipline
the synthesis credits Lane W with keeping. Restate as hypothesis. (E7)

## Q4 — Lane W walled limits — ACCEPT (with E1 at the point of use)

The Honest-limit paragraph (SYNTHESIS:77-82) carries 6-bones/no-female/LOADING-wall
faithfully, matching W/STATUS:65-78. "Topology finding unambiguous despite the wall" is
justified for the CLASS question: binding is parse-time-fixed, three states agree,
SHARED_ROOT=0 everywhere, and the one 38-bone sighting was OWN. But the headline table
(:29) leans on the 60-row count without the 6-hand-bone caveat — fix at point of use (E1),
and name the slot-mixing caveat where "exact" is claimed (E2).

## Q5 — Wave-21 charter — ACCEPT-WITH-ERRATA

"Landable only together" is supported (shim-off flings worse; a Layer-1-only landing fails
G-FIX-E1 by the wave's own photograph). The back-door ban (SYNTHESIS:128-130) is correct
and correctly A11-framed. Torso-precedent-first is right — it IS the L2-a/L2-b
discriminator and is native-side (matches the substrate note). Gaps: (i) no white-texture/
texture-integrity gate despite modifying the white-texture shim (E11); (ii) part 2's shape
("pose to B") presumes L2-a — must be conditioned on the step-1 discriminator, with the
reskin ban carried if it lands L2-b (E12); (iii) the discriminator must explain arm S's
male 3.1° null (E5's counter-evidence) or the mechanism story is wrong.

## Q6 — Banned citations — one shorthand violation, otherwise clean

No §8.4 "genuine fix is engine reskin" citation; the 6th-cell confounded death cert is not
cited; angles used are matrix-level relative rotations (no scalar slips); dead cells cited
only as dead. VIOLATION: **"the shipped clamped state"** (SYNTHESIS:58-59) and Lane N's
"shipped clamp/head-rebind then holds it" / "hands coherent (clamp holds)" (N/STATUS:32,
:50) repeat the clamp shorthand R5 VERDICT §8.3 + CLOSURE explicitly retired — band hand
meshes are rebound-skipped by the clamp; the shipped state is the seed-R rebake + mitten
default-ON (CLOSURE:89-91). Both arms were mitten-ON, which makes the coherent-vs-flung
asymmetry a real topology effect — worth one sentence, and it also means the control row's
"coherent" is mitten-mediated, not clamp-mediated. (E8)

## Q7 — Inherited lane overclaims

- **Lane D STATUS:61-62** — "Already proven by the 2026-06-06 record to NOT change
  skeleton binding topology (shim-off = same shared root)" is CONTRADICTED intra-wave by
  Lane N's counted arm. The synthesis correctly did not inherit it, but the file will
  mislead future readers. (E10)
- **Lane D STATUS:8-9 / SYNTHESIS:96-97** — bundling the FilterSubdir shim under
  "semantics-preserving" is self-contradictory with Layer 1 indicting it; only the
  ReplaceRefs reimpl is semantics-preserving. (E9)
- Lane W "Lane N joins row-for-row" (W/STATUS:31) — loose (60 mixed Wii rows vs 205 native
  hand slots; the join is class-level per A10). Covered by E1's note; no separate erratum.

## ERRATA (append-only; E4–E6 binding before Wave-21 dispatch)

- **E1** SYNTHESIS:29 (table row): relabel "hand-mesh bone binding" → "hand/outfit-mesh
  bone binding — hand-specific: 6 `hands_naked` bones (member 0) + one 38-bone
  `drivinggloves` sighting, all OWN; 60 rows span members 0/1/2 only".
- **E2** SYNTHESIS:40: "exactly matching Wii's topology" → "matching Wii's binding class
  (A10 `owningDirClass`) on every reachable row; Wii per-slot uniformity at full 38 bones
  remains walled (slot-mixing precedent: native 8/46, HANDS-ADJ §1)".
- **E3** SYNTHESIS:40-42: after "native-introduced regression", append "— of the retail
  load-path topology, not of hand visuals: the fling predates the shim (2026-06-05 female
  component) and worsens with the shim off".
- **E4** SYNTHESIS:61-65: replace the 8th-cell identity claim with "re-enters the dead
  offset-bake class: draw-time offsets were not measured in the shim-off arm; with the
  default seed-R rebake still running, the composition is plausibly `inv(R)·L_own` (dead
  cell #1) rather than the 8th cell's `inv(B)·L_own`. Which dead cell it reproduces is
  undetermined; that per-member topology alone fails the visual gate is what is proven."
- **E5** SYNTHESIS:70-72: replace with "87.2° is `angle(B·inv(R))` for the SetDeformation-
  time TRANSIENT seed R (provenance expressly open, HANDS-ADJ §2) — not a measured rest
  basis of the fresh per-member skeleton; arm S measured the male `own` ≈ B (3.1°, 1038
  blocks) during play. The committed gender-basis gap is the female ~29° (authored female
  offsets vs shared male bind)."
- **E6** SYNTHESIS:74-75: "is the missing sufficient ingredient — corroborated three ways"
  → "is the leading CANDIDATE (L2-a) for the missing ingredient; sufficiency untested, and
  arm S's male 3.1° is standing counter-evidence for males — the Wave-21 step-1
  discriminator decides".
- **E7** SYNTHESIS:72-73: "On Wii the outfit/clip poses each per-member skeleton to its
  gender" → "IF the Wii object reaches a gender-correct rest basis, some Wii-side
  mechanism poses it — hypothesis, not observed (substrate CharClipDrivers=0; A11)".
- **E8** SYNTHESIS:58-59 "the shipped clamped state" → "the shipped default (seed-R rebake
  + mitten-ON; clamp skips rebound band-hand meshes, R5 VERDICT §8.3)". Same correction to
  N/STATUS:32 and :50; add: "both arms mitten-ON — the asymmetry is topology, not clamp".
- **E9** SYNTHESIS:96-97 + D/STATUS:8-9: "semantics-preserving" → "ReplaceRefs reimpl is
  semantics-preserving; the FilterSubdir shim is a DELIBERATE semantics-CHANGING
  divergence — the Layer-1 mechanism itself — intentional, not a decomp defect".
- **E10** D/STATUS:61-62: append "SUPERSEDED intra-wave by Lane N: shim-off DOES change
  hand-mesh rest-entry binding (0→31,488; 205/205 slots flip to own). The 2026-06-06
  record stands only for its torso-draw measurement."
- **E11** SYNTHESIS:49-52 append "(untested — the chartered skeleton-scoped arm was never
  run; separability is a Wave-21 hypothesis to gate)"; SYNTHESIS:123-126 gate list add:
  "char-texture integrity on the scoped-shim arm (no white-texture regression —
  `char_shared.milo` carries palette content)".
- **E12** SYNTHESIS:117-121: append "Part 2 presumes L2-a; if the step-1 discriminator
  lands L2-b, part 2 as written is the wrong fix shape — re-charter, reskin stays banned
  (R5 §8.4/§5)."

## FINAL VERDICT

**ACCEPT-WITH-ERRATA.** Layer 1 (counted suppression + topology flip + decomp-faithful) is
the wave's real, durable result and survives adversarial re-derivation from the committed
evidence. Layer 2's A11 discharge is valid at the necessity level and OVERCLAIMED at the
mechanism level (E4–E6): the L2-a story misattributes the 87° figure and omits arm S's
male counter-result — exactly the class of confident-mechanism slip this campaign's
close-outs exist to catch. GT-D closure is untouched (one hypothesis-register fix, E7).
Wave 21 may dispatch once E4–E6 (+ the E11 texture gate) are folded into its kickoff.
