# R5-HANDS-ENDGAME — VERDICT (Wave 17, Lane H)

**Decision: (C) — conditional.** Neither the TRUE reskin (A) nor CLOSURE (B) is
choosable from D4's table, because the table's headline signal — "middle/ring
divergence SURVIVES frame-matching" — is shown below, from D4's own raw evidence,
to be **better explained by clip-state than by the vert-encoded inter-bone
mechanism**. The exact cheap upgrade that discriminates A vs B is named and
spec'd (**D5: articulated Wii capture with a live clip+frame join key** — D4's
own priced follow-up), with the branch decision pre-registered as a mechanical
table lookup so Wave 18 executes it without judgment calls. On today's evidence,
**option A is UNJUSTIFIABLE** (choosing it would violate PLAN-R5 §0 Step 3's own
condition) and **option B is PREMATURE** (it would close the flagship bug while a
~1-lane-wave discriminator with all machinery already built sits on the table).

Charter respected: zero code written or flipped; engine pin untouched; read-only
over rb3 + engine; deliverables are this verdict + the committed re-derivation
script/output under `evidence/`. Coordinator implements in Wave 18.

---

## 0. M0 contract check (PLAN-R5 §2.4, applied as written)

| # | Contract item | Status |
|---|---|---|
| 1 | Joined inter-bone delta table, per-pair, both hands, per-member/gender | **DELIVERED** — `R1-DOLPHIN/evidence/D4_delta_table.{md,json}` |
| 2 | ≥5 matched frames per clip **across ARTICULATED poses** | **NOT MET (Wii side)** — the Wii artifact is ONE settled, driver-less pose: `D4_wii_drivers.json` `active_player_clip_drivers = 0` (D4_findings.md §"measured reason"). Zero matched articulated frames exist; native-side articulation is fine (mr-pair swing up to 31°). This is exactly the contract's pre-registered "R5 formally requests R1-M4 and pauses" trigger. |
| 3 | Noise band ε_noise from multi-pause static-idle spread | **NOT DELIVERED** — proxies only (convention calibration 0.133°, pelvis pin 2.5–7.8°, male thumb FLOORs 0.01–0.06° bounding magnitude noise ≲0.06°) |
| 4 | G5 known-good/known-bad separation **of the flag arms** | **NOT DELIVERED — and unsatisfiable as spec'd**: `RB3_HANDS_AUTHORED_REPOINT` changes mesh OFFSETS/binding only; bone worlds are identical between arms, so a bone-world inter-bone metric cannot separate them *in principle*. D4's substitute validations (red-team mismatched pair 157.5°, bilateral L/R symmetry, convention self-calibration) are green but are not item 4. Re-spec below (§6, G5′). |
| 5 | Authored-bind inter-bone tables both platforms (`Dbind_wii` vs `Dbind_360`) | **NOT DELIVERED** |

Mechanical outcome per §2.4: items 2–4 missing → R5 does not issue a GT-letter
verdict; it returns the named gap. Per §3.1's population rules the CONSTANT vs
TIME-VARYING test (the GT-B/GT-C discriminator) is **uncomputable** from one
quasi-matched settled pose — and M1's fail-safe is explicit: ambiguous numbers
yield "indeterminate → re-price," never a judgment call. That is this verdict.

What D4 *does* establish externally (and this survives everything below):
**anchors and thumbs are exonerated** — wrist `forearm→hand` FLOOR 0.013–1.2°
and the full thumb cascade FLOOR 0.01–0.06° on the clean male-vs-male cells
(D4_delta_table.md slot0/slot2). Where the vignette does not animate a channel,
native's inter-bone geometry equals the real Wii's to hundredths of a degree.
**The native static skeleton is externally Wii-faithful.** The open question is
solely whether its *animated* finger poses are.

## 1. Adversarial re-derivation — the D4 headline does NOT confirm the mechanism

Binding rule discharged: I re-derived the "middle/ring survive, thumbs collapse"
pattern from the committed raw sweep (`D4_native_sweep_raw.tar.gz`, 32 dumps)
independently of `scripts/analysis/interbone_framematch.py`. Script + output:
`evidence/r5_adversarial_rederivation.py` / `.out` (all assertions pass). Facts:

- **F1 — the two male members play DIFFERENT clips.** slot0 = `player3_m`,
  slot2 = `player0_m` (raw `drivers[].playing_clip`, all 32 snapshots). The
  D4 table's per-member FLOOR disparity (slot0 mr mean 7.5° vs slot2 26.6° —
  3.5×, same gender, same rig class) is therefore a **clip-content** variable,
  not a skeleton variable. A per-gender constant basis error `C_i` cannot
  produce member-dependent floors on the same rig.
- **F2 — the join script is vindicated**: my independent envelope recomputation
  reproduces its min/max to <0.05°. The finding below is about *interpretation*,
  not about D4's machinery (which is sound and re-validated).
- **F3 — the two different clips share EXACT envelope endpoints** (slot0's max
  == slot2's min to ≤0.15° on mf01/mf12/mf23/rf01/rf12: 19.89/45.05/43.27/
  25.20/66.06). Two independent basis-divergent skeletons would not do this.
- **F4 — one scalar drives every finger pair**: within every member, the
  cross-pair correlation of |relRot| over the sweep is **1.0000** (mf12↔rf12,
  mf12↔mf23, all four members). The vignette finger animation is a **1-D curl
  parameter** (a blend along an authored open↔grip hand-pose family).
- **F5 — both male clips sample the SAME 1-D family**: pooled (mf12→rf12)
  points from both clips lie on one monotone curve (0/63 violations). The clips
  differ only in **which interval of the curl parameter they animate** —
  `player3_m` a lower (more open) interval, `player0_m` a higher (more curled)
  one, meeting at a shared keyframe pose (F3).
- **F6 — the Wii settled pose sits BELOW both male envelopes on EVERY surviving
  pair, uniformly in the "less curled" direction** — and the one member whose
  clip covers the relaxed region (slot1, `player2_f`) FLOORs the mid-segment
  pairs at 0.12–0.65° (mf12/rf12) with all six pairs ≤5.7°, despite the
  cross-gender confound.

**What these facts force:** the D4 FLOOR — "no vignette frame closes the
middle/ring gap" — measures *the distance from a driver-less Wii settled pose to
the curl interval a particular clip happens to animate*. The Wii side was not
playing any vignette (`active_player_clip_drivers = 0`); its settled hand is a
relaxed pose at the low end of the same authored curl family, below the interval
either sampled male clip visits. That is a **clip-state difference, not a
skeleton basis error**. The Wave-16 Seam-B tear mechanism ("own's animated
inter-bone poses diverge from the shared bind") is neither confirmed nor
refuted by D4 — it was never tested, because no articulated Wii frame exists.

D4's own caveat said this ("native-active-vignette-envelope vs Wii-settled-pose
comparison"); the re-derivation upgrades the caveat to a positive alternative
explanation with discriminating evidence (F1+F3+F5+F6). **Standing correction:**
D4_findings.md's line "the finger-specific divergence R5's pre-registered
contract flags as confirming the mechanism" must not be cited — applying §3.1's
population rule to a settled-vs-envelope comparison would have manufactured a
false GT-B.

## 2. Why not (A) — the true reskin is unjustifiable today

PLAN-R5 §0 Step 3 is the item's own condition: a true reskin has exactly one
non-dead form — anchor derived from external ground truth showing a
**divergence that is CONSTANT over matched frames**. After §1:

- No constant divergence is established. The only externally-measured deltas
  that survive are explained by clip curl-interval coverage; the statics that
  are cleanly comparable (anchor + thumb + slot1's covered mid-segments) read
  **≤0.65°** — at or under the plan's own ε floor.
- Per §0 Step 3's other branch: "if the divergence is ~zero (bones already
  Wii-faithful) the reskin is unjustified — it would deform correct geometry to
  fit correct bones, pure error." Today's external evidence leans exactly there
  for everything measurable.
- Cost/risk (priced, §5 option table): +1.5–2 lane-waves of NEW verts+weights
  mutation code touching the same multi-bone blend machinery whose tearing
  refuted Wave 16 (`HANDS-FIX/STATUS.md` §Root cause), carrying the
  anchor-poisoning trap that killed W14 (`RESKIN/STATUS.md`: seed-R anchor,
  wext 74.8→87.7 regression), user-visible shape drift, and mesh-churn
  re-latch risk — all to chase a signal that dissolves under F1–F6.

A is therefore **not choosable now**, and per the pre-registration below it can
only re-enter as **B2-fallback under a D5-proven GT-B** (PLAN §3.3 order: B1
anim-basis correction first, B2 only on B1's named recomposition failure).

## 3. Why not (B) — closure is premature by the campaign's own standard

- The discriminating instrument is **cheap and already built**: D2's patched-disc
  boot (rb3 `7872bd35`), the Wii driver census (`wii_bone_dirboot.py drivers`),
  the native sweep + join (`d4-bonedump-sweep.py`, `interbone_framematch.py`,
  validated to 0.133° convention floor with a red-teamed metric) — the ONLY
  missing piece is a Wii capture *while a hand-animating clip driver is live*.
  Closing the flagship bug with that unpriced-out is the exact "native-only
  verdict by default" failure mode this campaign spent R1 buying its way out of.
- A closure verdict would itself lean on "native exonerated," which §0/§1 show
  is externally proven only for **static** inter-bone geometry. The Wave-16
  tear (`HANDS-FIX/evidence/matched_zoom_burst08_burst12.png`) lives at
  **animated** poses — precisely the untested half. GT-D closure is honest only
  when ground truth is *unobtainable in budget* (PLAN §3.2); it is obtainable.
- Mitigation shorthand corrected (must ride any eventual closure): per PLAN-R5
  §2.2, `RB3_NO_SKIN_CLAMP` does **NOT** touch band hand meshes — rebound meshes
  skip the clamp (engine `Rnd_Wgpu_RB3.cpp:3777,3812`; `BandCharacter.cpp:1898`
  sets `mNativeBonesRebound`). The band-hands shipped state is the default
  seed-R rebake — the coherent displaced "ceiling hand" + wrist spike-webbing
  (87.2° mechanism, HANDS-ADJUDICATION/VERDICT.md §2). Any closure package must
  state *that* residual, not clamp tuning.

## 4. THE DECISION — (C): D5, then a mechanical branch

### D5 — articulated Wii capture (the named upgrade; D4's priced follow-up)

One lane, hard-boxed at ~1 lane-wave, Wave 18:

1. Boot the D2 patched disc (Bank-8 debug DOL under retail apploader — map
   valid by construction). Drive the shell past the frozen overshell instant to
   a state where `playerN_{m,f}` vignette CharClipDrivers are **LIVE**
   (main_hub band lineup; Dolphin pipe-input navigation, or simply let the
   shell run — D2 froze early). Feasibility already half-proven: the `drivers`
   census machinery exists and found the clips resident.
2. Capture **≥5 Wii frames spanning finger articulation** (mr-pair curl swing
   ≥15° across captured frames), each labeled with `(clip, TopClipFrame)` from
   the live driver — the TRUE join key D4 had to substitute with pose-anchoring.
   Label each Wii member chain's gender (38 vs 40 hand-bone name census) so
   joins are male↔male / female↔female — this **subsumes D4's second priced
   upgrade** (gender-split Wii chain).
3. Deliver ε_noise (contract item 3): ≥3 repeated captures at one held
   `(clip, frame)`; spread = ε_noise.
4. Stretch (non-gating, funds contract item 5 + GT-A forensics): dump the
   Wii-side authored binds / bound-skeleton worlds for one hand mesh from the
   same boot (`RndMesh` bone offsets at known Bank-8 layout).
5. Join with `interbone_framematch.py` at matched `(clip, frame)`; primary
   metric stays the convention-invariant |Δmag|, now per matched frame.

**Fallback inside the box:** if no shell state with live vignette drivers is
reachable, capture during GAMEPLAY at a matched song clip (richer articulation;
same machinery). **If the box is exhausted with no articulated capture → GT-D
CLOSURE per PLAN §3.4**, honestly labeled "closed without articulated ground
truth," mitten mitigation optional and E1-decided, residual statement per §3
above. That closure needs no further permission — this verdict pre-authorizes it.

### The branch table (pre-registered NOW; Wave-18 executes it mechanically)

Thresholds inherited from PLAN-R5 §3.1 verbatim (ε = max(3°, 2·ε_noise);
DIVERGENT = median-over-matched-frames > 5° AND > 3·ε_noise; CONSTANT =
per-pair std-dev < max(2°, ε_noise); ≥2 finger pairs to call a branch; anchors
must read ≤ ε for interpretability; power floor ≥5 frames × ≥2 pairs, below it
= indeterminate, not forced):

| D5 shows | Verdict | Endgame |
|---|---|---|
| all finger pairs ≤ ε at matched articulated frames | **GT-A** — native animates Wii-faithfully; W16 tear is DOWNSTREAM of bone worlds | **A is REFUTED permanently.** One hard-boxed §3.5 forensic lane over palette INPUTS — substrate already committed: R2's per-mesh per-frame palette dumps (`R2-FIXTURES/evidence/{good-body,bad-ceiling,bad-torn}/dumps/`) + D4/D5 bone worlds + the item-5 bind tables. Suspects, in order: 360-vs-Wii authored-bind delta; per-bone instance mixing in pass-A resolution; palette build defect. Names a concrete fixable input → targeted fix lane (gates §6); else → CLOSURE §3.4 |
| ≥2 pairs DIVERGENT + CONSTANT | **GT-B** | **B1 anim-basis correction** (PLAN §3.3, preferred): derive per-bone `C_i` from D5, recomposition self-check BEFORE any engine edit, flag-first `RB3_HANDS_ANIMBASIS`. **B2 true reskin ONLY on B1's named failure mode** (C_i fails to recompose the measured deltas), with §6's corrected B2 gate design |
| pairs DIVERGENT + TIME-VARYING | **GT-C** | Channel-decode forensics as a NEW scoped item (clip decode vs D5 timeline); neither reskin nor closure; R5 closes "redirected" |
| anchors themselves > ε | **GT-U** | Upstream arm/torso item; hands deferred |
| D5 box exhausted, no articulated capture | **GT-D** | CLOSURE §3.4 as pre-authorized above |

Prior (stated as prior, not promise): §1's evidence leans GT-A — the same 1-D
curl family decoding coherently natively, statics exact to ≤0.06°, and slot1
crossing the Wii magnitudes at its clip's relaxed end. Expected total: D5
(0.5–1) + GT-A forensic (0.5–1) or closure (0.25–0.5) ≈ **1–2 lane-waves to the
family's terminal state**.

## 5. Option table (OPTIONS §4 lint 6 — full family, measured/unmeasured)

| Option | Class | Status |
|---|---|---|
| 7 offset-bake cells (anchor × bone) | runtime-anchored bake | **measured-dead** (HANDS-ADJUDICATION/VERDICT.md §3) — not re-attemptable |
| 8th cell: authored-offset repoint (`RB3_HANDS_AUTHORED_REPOINT`) | offset repoint | **measured-dead** (numeric PASS, VISUAL refuted — HANDS-FIX/STATUS.md) — not re-attemptable |
| Positions-only reskin, seed-R anchor (`RB3_HANDS_RESKIN`, W14) | vert re-pose | **measured-dead** (wext 74.8→87.7 regression — RESKIN/STATUS.md) — not re-attemptable |
| Reskin with `Ownref ≈ B` (any runtime-captured play-basis anchor) | vert re-pose | **dead by derivation** (≡ 8th cell; PLAN-R5 §0 Step 2) |
| **B2 TRUE reskin, external-GT anchor (verts+weights)** | new code | **UNMEASURED — gated**: only reachable as B1-fallback under D5-proven GT-B; unjustifiable today (§2) |
| **B1 anim-basis source correction** | bone local rest from external `C_i` | **UNMEASURED — gated**: same GT-B gate; preferred over B2 (zero geometry/weights mutation) |
| Mitten palette-blend fallback (`RB3_HANDS_MITTEN`) | render mitigation | **UNMEASURED** — closure-package option, default decided by E1 (PLAN §3.4) |
| CLOSURE + residual statement | process | available as GT-D; premature while D5 unfunded (§3) |
| **D5 articulated Wii capture** | instrument | **CHOSEN** — the A-vs-B discriminator; all machinery pre-built |

## 6. Pre-registered gates for the chosen path (each with its fail-red demo)

| Gate | PASS criterion | Fail-red demonstration |
|---|---|---|
| G-D5-1 capture validity | ≥1 Wii member with LIVE `playerN` CharClipDriver (census shows clip + `TopClipFrame`); ≥5 matched frames; Wii mr-pair swing ≥15° across them; per-chain gender labels | the committed `D4_wii_drivers.json` (0 active drivers) IS the red baseline — the harness must FAIL loudly on the D2 frozen state and pass only with live drivers |
| G-D5-2 noise band | ε_noise from ≥3 same-(clip,frame) captures, committed | capture at two DIFFERENT frames → spread must exceed ε_noise (instrument sees pose change) |
| G-D5-3 join validity (inherited from D4, re-run on the new capture) | convention self-calibration ≤0.5°; red-team mismatched pair >90°; pelvis pin ≤8°; male↔male/female↔female joins only | red-team row shown RED in the artifact (D4 precedent: 157.5°) |
| G-D5-4 decision mechanicalness (= PLAN G-D1) | branch named by the committed script from the D5 JSON alone, thresholds §4 verbatim | frame-shuffled/pair-permuted copy of the table → script must output indeterminate, never a confident branch |
| **G5′ (re-spec of contract item 4)** | known-good/known-bad separation demonstrated at the **PALETTE level** (R2's `good-body` vs `bad-ceiling` vs `bad-torn` fixture dumps — these arms DO differ in palettes), since flag arms share bone worlds (§0 item 4) | run the palette diff on two same-arm boots → must read ≈0 (specificity) and on good-vs-bad arms → must separate |
| G-FIX-E1 (any fix branch, B1/B2/GT-A-targeted) | **matched fixed-clock burst_08/burst_12 frames: ceiling-hand AND spike-fan morphology GONE, BOTH genders, gloves+fingernails non-regressing** — numeric gates alone are insufficient (the Wave-16 lesson: every numeric gate passed while the visual failed) | the flag-OFF arm at the same matched frames is the red baseline (HANDS-FIX evidence protocol, `matched_zoom_burst08_burst12.png`) |
| G-FIX-GT (B1) | D5 join re-run flag-ON: previously DIVERGENT pairs ≤ ε, anchors unchanged | same join on the W16 `RB3_HANDS_AUTHORED_REPOINT=1` arm — bone worlds identical to default (§0 item 4), so it must read UNCHANGED (proves the metric moves only when bones move) |
| G-FIX-GT (B2) — **corrected design** | bone-world metrics CANNOT gate B2 (verts/offsets are invisible to them — the same blindness as §0 item 4). B2's ground-truth gate = **skinned-output level**: composed palette / skinned-vertex comparison vs the Wii side (R2 palette-dump machinery + D5 worlds + item-5 binds), plus G-FIX-E1 | point the weight-rederivation at a mesh with a known-absent bone name → reassigned-vert count > 0 reported LOUDLY (must be == 0 for hands_naked); anchor-poisoning guard: assert `angle(Bbind_i·inv(Ownref_i))` == R1-measured `C_i`, loud abort on the ~87° seed signature |
| G-FIX-REG regression net | `rb3-tests` R2 skinning fixtures green (gender-split Tier-1 count(>5°)==0, Tier-2 EXACT ≤1u, `test_hands_bind_oracle`); guard-DROP census 0; crowd oracle untouched; drawlog-792 flag-OFF byte-identical | `RB3_HANDS_ATTACH_PERTURB` shipped fail-red pattern (0.15 rad → ≈8.6°); drawlog diff with flag forced ON |
| G-C1/G-C2 (closure, if GT-D) | as PLAN §3.4/§5 verbatim (mitten E1 on burst frames; residual stated in user-visible terms, **with the §3 clamp-shorthand correction**) | mitten threshold 0 → all-rigid articulation loss visible in E1, restore calibrated value |

## 7. What would falsify this verdict

1. **A matched articulated Wii frame showing ≥2 mr pairs DIVERGENT** (>5°,
   >3·ε_noise) — my GT-A lean is wrong; the procedure catches it by design
   (that IS the D5 test; the verdict letter stays C either way).
2. Evidence that the Wii D2 settled pose **is** a pose native's curl family
   cannot reach *for basis reasons* (e.g. the native family's relaxed endpoint,
   extrapolated, provably excludes the Wii magnitudes) — would resurrect the
   GT-B reading without D5. Note today's data says the opposite (slot1 crosses
   the Wii mid-segment magnitudes at 0.12–0.65°).
3. An error in F1–F6 — all six are asserted by the committed script over the
   committed raw evidence; re-run `evidence/r5_adversarial_rederivation.py`.
4. D5 proving infeasible AND someone later producing cheap articulated ground
   truth by another route — would make the pre-authorized GT-D closure look
   premature in hindsight; accepted and stated.

## 8. Standing corrections to the record (carry forward)

1. **D4's interpretation line is corrected** (§1): the surviving middle/ring
   FLOOR is clip-curl-interval coverage vs a driver-less settled pose — do NOT
   cite it as confirming the Seam-B/vert-encoded mechanism. D4's *caveat*
   already gestured at this; the raw-data re-derivation makes it positive.
2. **Contract item 4 (G5) is unsatisfiable at the bone-world level** for
   offset-class flag arms; the valid separation surface is palettes (G5′, §6).
   The same blindness means **bone-world ground truth can never gate B2** —
   PLAN-R5's G-F1 applies to B1 only (§6 corrected design).
3. **`RB3_NO_SKIN_CLAMP` is not the band-hands mitigation** (it skips rebound
   meshes); the shipped band-hands state is the default rebake's ceiling-hand
   (PLAN-R5 §2.2 correction, re-anchored here because the ROADMAP row R5 and
   several memory entries still carry the shorthand).
4. The Wave-16 root-cause paragraph's "the genuine fix is an engine per-member
   RESKIN" (HANDS-FIX/STATUS.md §Root cause) was itself a **native-only
   inference** — under GT-A a reskin onto Wii-faithful bones degenerates to the
   refuted Wave-16 cell (PLAN §0 Step 2). It must not be cited as a mandate
   for option A.

## Evidence

- `evidence/r5_adversarial_rederivation.py` + `.out` — F1–F6, asserted, over
  committed D4 raw evidence only (offline, reproducible).
- Consumed (committed, unchanged): `execution/R1-DOLPHIN/evidence/
  D4_delta_table.{md,json}`, `D4_native_sweep_raw.tar.gz`,
  `D4_wii_drivers.json`, `D4_findings.md`, `D3_findings.md`, `D2_*`;
  `execution/HANDS-ADJUDICATION/VERDICT.md`; `execution/HANDS-FIX/STATUS.md`
  (+ `evidence/matched_zoom_burst08_burst12.png`); `execution/RESKIN/STATUS.md`;
  `execution/SKEL/STATUS.md`; `execution/R2-FIXTURES/evidence/`;
  `RETROSPECTIVE/plans/PLAN-R5-hands-endgame.md`; `RETROSPECTIVE/OPTIONS.md` §4.
- Checkpoint: `/tmp/wave17-checkpoints/H.json`.
