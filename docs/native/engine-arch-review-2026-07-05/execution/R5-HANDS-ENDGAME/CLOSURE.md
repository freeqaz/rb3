# R5 HANDS ENDGAME — GT-D CLOSURE (Wave 18)

**Status: CLOSED — honestly labeled "closed without articulated ground truth"** (PLAN-R5
§3.4, GT-D row; pre-authorized by `VERDICT.md` §4 and re-authorized by the Wave-18
acceptance A1 hard stop). Coordinator-executed 2026-07-08. Mitten mitigation (closure
item 1) dispatched flag-first as Wave-18 Lane M; its default is E1-decided and does NOT
reopen this closure either way.

## Why closure, and why it is honest now

The VERDICT's own condition (§3): GT-D is honest only when articulated ground truth is
*unobtainable in budget*. That condition was NOT met at the Wave-17 adjudication (D5 was
unfunded) and was still not met at D5's exhaustion (D5's evidence priced the
visually-guided path feasible). It IS met now:

1. **D5** (rb3 `09fc594d`): blind navigation exhausted — pipe input works, but the live
   band states sit behind prompts not blind-clearable on Null video.
2. **V / VISCAP** (rb3 `5655f499`, milo-trace `1167f42`): built the visually-guided rig
   D5 priced, and BY SIGHT cleared the entire prompt flow (guest-profile → YES →
   nav-help → calibrate-skip), reaching main_hub AND live gameplay on two songs. The
   navigation wall is GONE — and the substrate wall behind it is now measured:
   **the Bank-8 debug patched-disc boot does not drive band CharBone skeleton animation
   in ANY reachable state.** 0/992 CharBone world transforms move during
   confirmed-live gameplay (visual Δ 25.97, MEM2 heap live, unlimited-speed
   reproduction, strumming input); finger span 0.0° everywhere;
   `active CharClipDrivers = 0` at overshell, hub, and gameplay. The same instrument
   returns the D2/D4-validated bind pose, so the reads are live and correct.
3. Native's own gameplay reference (`R1-DOLPHIN/evidence/D3_delta_table_gameplay.json`)
   is animated (54–99°) in the same CharBone-world representation — so the
   representation is the right one; the Wii half simply never articulates in this
   substrate. **G-D5-1 fails everywhere; no branch letter is assignable.**

Per the A1 contract that is exhaustion, twice pre-authorized → GT-D.

## What IS externally established (the evidence-based half)

- Native skeleton **statics** are Wii-faithful on every cleanly comparable channel:
  wrist anchor + full thumb cascade ≤0.06°, slot1 relaxed-region mid-segments
  0.12–0.65° (D4 FLOOR table + VERDICT §1 dissolution of the apparent middle/ring
  divergence — clip curl-interval coverage, not skeleton).
- The engine-emitted Tier-1 field (Wave-18 Lane N, engine `e69a35f`) reproduces the
  Wave-15 adjudication table as a standing regression test (male 0.06°/3.13°, female
  28.88°/28.92°).
- The **animated** half — whether `own`'s animated inter-bone poses leave the ≈B basis
  (GT-B) or the tear is downstream of bone worlds (GT-A) — is the half that required
  articulated Wii ground truth and is closed WITHOUT it. The GT-A prior (VERDICT §4)
  remains a prior, not a finding.

## Accepted-residual statement (VERDICT §8.3 clamp-correction applied)

> Band hands may articulate wrongly / appear displaced at extreme animated poses
> (the coherent displaced "ceiling hand" + wrist spike-webbing — the 87.2° seed-R
> rebake mechanism, `HANDS-ADJUDICATION/VERDICT.md` §2); most visible in close-up
> cams; no mesh tearing on the shipped default. If `RB3_HANDS_MITTEN` ships
> default-ON (E1-decided), the residual becomes: fingers degrade toward rigid-hand
> at exactly those poses, hand stays attached and moving.
> **`RB3_NO_SKIN_CLAMP` is NOT a hands mitigation** — band hand meshes are
> rebound-skipped by the clamp (engine `Rnd_Wgpu_RB3.cpp:3777,3812`;
> `BandCharacter.cpp:1898`). The shipped band-hands state is the default seed-R
> rebake. Closed under the GT-D "no articulated ground truth" label.

## Reopen condition (the closure's falsifier — pre-registered)

An articulated Wii capture in the CharBone-world representation (≥5 matched frames,
mr-pair swing ≥15°, live clip+frame key) reopens the branch table mechanically:
`scripts/analysis/v18_hand_classify.py` is committed and ready. The one known candidate
route is V's follow-up #2 — root-causing WHY `CharClipDrivers = 0` on this substrate
(debug-build stub vs guest-profile/patched-disc director init vs headless subsystem
gating) — recorded here as an OPTIONAL, separately-charterable item. It is NOT a
discriminator lane under the A1 stop; if it ever lands an articulated capture, this
closure's falsifier triggers and GT-A/B/C resumes where the branch table left off.

## Record hygiene (item 3)

- Family ledger: CLOSED (this file). `ROADMAP.md` R5 row updated.
- The two default-OFF dead-cell flags (`RB3_HANDS_AUTHORED_REPOINT`,
  `RB3_HANDS_RESKIN`) registry rows now point at this closure.
- `HANDS-FIX/STATUS.md` "milo-trace is a stub" claim annotated (obsolete since D2).
- Evidence chain: `HANDS-ADJUDICATION/VERDICT.md` (87.2° derivation) →
  `HANDS-FIX/STATUS.md` (Wave-16 visual refutation) → `R1-DOLPHIN/` D2/D3/D4 (+
  CORRECTION blocks) → `R5-HANDS-ENDGAME/VERDICT.md` (decision C + §8) →
  `evidence/D5_findings.md` + `evidence/V_findings.md` + `V_exhaustion.json` (the
  exhaustion pair) → this closure.

---

## POST-FLIP ADDENDUM (Wave-18 close-out, review F5)

- The item-1 conditional above is RESOLVED: `RB3_HANDS_MITTEN` shipped **default-ON**
  (coordinator E1 on the burst_08/12/45 OFF/ON pairs; engine `403ff00`, opt-out
  `RB3_HANDS_MITTEN_OFF`). The operative residual is therefore the mitten-ON variant:
  fingers degrade toward rigid-hand at extreme poses; hand stays attached and moving.
- Correction to the §Accepted-residual wording: "no mesh tearing on the shipped default"
  was too strong for the PRE-mitten default — Lane M's own OFF-arm bursts show
  spike-webbing/tear-class geometry. Post-flip, the mitten collapses the finger-level
  members of that class; the statement holds for the SHIPPED (mitten-ON) default.
- **Out-of-scope residuals, recorded here so they are NOT misfiled against this closed
  family** (new backlog key **FOREARM-FLOAT**): (1) the persistent top-center floating
  flesh-colored structure in the burst_08/12 frames — unchanged OFF→ON, i.e. NOT
  finger-level; consistent with the Wave-9 "disconnected floating forearm" sighting —
  forearm/prop-level, never triaged as its own item; (2) the mitten E1 evidence has no
  DRUMMER close-up coverage. Both are open observations for a future wave, distinct from
  the closed hands-finger family.
