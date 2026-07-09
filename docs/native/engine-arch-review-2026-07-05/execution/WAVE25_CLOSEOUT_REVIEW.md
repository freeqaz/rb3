# Wave 25 Close-Out Review (Fable)

**Lanes:** W25-FOREARM (`96f18e62`) FIXED / flip candidate; W25-CROWD (`b6a8980f`) narrowed-but-unfixed partial.
**Verdicts:** FOREARM **ACCEPT-WITH-ERRATA**; CROWD **ACCEPT-WITH-ERRATA**. **FLIP: YES** (Q1 resolved in favor of the clamp).

---

## Q1 — Is mAAPlusBB ≈ 20u the CORRECT reach, or does the clamp mask a length bug? → **20u is CORRECT; no length bug; flip-safe**

Verified the math at HEAD, `src/system/char/CharIKHand.cpp:489-503` (`MeasureLengths`):
`len = Length(mHand->mLocalXfm.v)` (forearm), `parentlen = Length(mHand->TransParent()->mLocalXfm.v)` (upperArm);
`mInv2ab = 1/(2ab)` (:495-497), `mAABB = a²+b²` (:498), **`mAAPlusBB = len + parentlen` (:499) — the LINEAR sum a+b in length units**, despite the misleading name. It is total shoulder→hand reach, dimensionally a radius. Four independent confirmations that ~20u is right, not short:

1. **The solver itself uses it as a length-radius:** `PullShoulder` (:465-475) thresholds at `mAAPlusBB * 0.95` and is called with it at :347/:379/:458. The clamp (:248 `reach = mAAPlusBB`) adopts the engine's OWN reach notion and the SAME pivot (`sh = mHand->TransParent()->TransParent()`, :241-244, = the `trans2` upperArm-world origin IKElbow/PullShoulder use, :347).
2. **Anatomical cross-check (the decisive one):** feet ikhands log `reach=38.33` (evidence/clampfire-fires.log:10-11) and the measured pelvis→floor distance is 51.5−13 ≈ 38.5u (rootcmp world y 51.5, floor 13). Legs reach the floor to within 0.5%. Arm/leg ratio 20.3/38.3 ≈ 0.53 matches human shoulder-wrist/hip-floor (~0.5-0.55). If MeasureLengths were systematically short, the legs would be short too — they are exact.
3. **The "hand at y≈209" is the EXPLODED pose, not the arm:** W24-RECON/REPORT.md:61-67 measured y≈209 targets and upperArm flung to (23.3,−118.3,·) DURING the fan — evidence of the bug, not of a longer arm. Rest-pose guitarist hand y≈54 vs pelvis 51.5 is coherent with 20u arms on a 38u-pelvis rig.
4. **MeasureLengths' 81.4% residual is regalloc, re-verified:** my own `run_diff_inspect diagnose` = `diff_op: none`, 59 arg-diffs fully explained by FPR swaps (f0↔f1, f2↔f3...), 0 unexplained; insert/delete clusters are sqrt/Length inline scheduling. `bank_divergence` CAUTION but mismatch-class regalloc. The 6-line source is correct by inspection.

**Does the clamp pull a hand away from a reachable target?** No — `distSq > reach*reach` (:249) is a strict no-op inside reach. **Is it a symptom-mask?** Partly yes, but of a **target-pose defect, not a length defect**: the far targets are instrument-PROP bones grossly mis-posed natively (`bone_pick_strum.mesh` at z=98.2 vs hand z=48.8, rootcmp log:1; `bone_mic_stand_bottom` y≈−30, below the floor; fret targets 98-216u). Real IK solvers do clamp to reach — the guard is sound engineering regardless — but note honestly: with in-song fret/strum targets at 98-273u, the graduated `skip` branch means native in-song arms are effectively clip-posed (the RB3_NO_IK-correct look), not IK-refined. Restoring genuine IK requires fixing prop-bone posing (→ Q7). One behavioral delta in the 1-2×reach band: clamping pre-`PullShoulder` suppresses the authored torso-lean response (excess capped at 0.05·reach) — E1/ANATBEAT show no visible harm; loosen the lower band (fire above ~1.2×reach) if stiffness is ever observed.

## Q2 — Flip safety → **SAFE to flip default-ON**

- No-op inside reach verified in code (Q1). Gate: 0 body-clip events >1.5, player0 guitar max 1.15 vs 34-85 OFF (STATUS:81); E1 PNGs verified by this reviewer — fix-OFF shows real flesh-colored spike shards across the frame, fix-ON a coherent guitarist body, hands on the instrument (clip pose supplies playing anims).
- **Nondeterminism (52,954 vs 0 events) does not undermine the claim:** the clamp keys on the deterministic geometric condition (d>reach), not the stochastic explosion. The ON-run clampfire log proves the pathological condition (preDist 120-273u) OCCURRED during ON runs and was contained — the A/B is valid despite run-to-run noise. Single-run OFF gates alone would not carry it; mechanism + fire-log + E1 do.
- Modes: per-ikhand, member-agnostic (all four instruments measured). Vignette residual ≤4.2 is pre-existing and untouched. One hole (not a regression): `mic_stand.ikhand` logs `reach=0.00` (weight-reach-evidence.log:33) — MeasureLengths needs a 2-deep chain, so `mAAPlusBB=0` and the clamp's `mAAPlusBB > 0.0f` guard (:240) skips it; that ikhand takes the direct-set path (no elbow) and keeps pre-fix behavior. Feeds the Q7 prop-posing item.

## Q3 — Match-neutrality → **REAL, verified at HEAD by this reviewer**

Independent `batch_objdiff` at HEAD: `Poll__10CharIKHandFv` 96.13% (baseline 96.127235), `MeasureLengths` 81.34% (81.355), `Poll__10CharDriverFv` 93.54% (93.54499) — all == baseline to rounding; evidence/objdiff-baseline-vs-postfix.md records unit 99.16526% == baseline, all 20 fns, same instruction counts + mismatch pattern. Everything is `#ifdef HX_NATIVE` (CharIKHand.cpp:202-277). Nothing leaked. A2 case-1 satisfied.

## Q4 — CROWD root cause → **REAL, well-evidenced, same native async-interleaving class**

step0-probe-trace.txt independently checked: 7× `[CHARDRV_REPLACE] ... from='crowdN.cl(i)p' to='?'(null) beat=2.433` (:21-27), 7× `[CHARDRV_DIE] pollFrame=72 beat=2.433` (:36-44), second ENTER wave seq=8-15 with `defClip=(nil) mFirstAtEntry=(nil)` (:28-35) — the clip is destroyed by the merge, not popped (`CHARDRV_POP` never fires) and not cleared by re-Enter. Bank-swap evidence (bank-swap-diagnosis.txt): triggered drivers → player-only bank nCrowd=0; never-triggered female04 keeps nCrowd=5. This is the same class as the hands parse-time binding and the load-order fixes: native async loading lets the vignette trigger fire against objects a still-in-flight load-merge later replaces; on Wii the synchronous DVD ordering means the trigger fires once against final objects. The scope statement is honest — with one sharpening (ERRATA C2): in the observed sv3_a repro the flag recovers the **empty set**, since the 7 swapped drivers are unrecoverable AND the one bank-intact driver (female04) never Played, so it has no snapshot for the re-arm to use (CharDriver.cpp:384-388 snapshots at first Play). Census flag-ON animating=0 confirms.

## Q5 — Partial disposition → **KEEP default-OFF through W26, with a removal criterion**

Keep: the probes (ENTER/PLAY/DIE/REPLACE/POP/STARVE) are the instrumentation the W26 engine lane needs, and the re-arm is byte-inert scaffolding that becomes the recovery path if the engine fix preserves banks but a clip still dies. Remove the re-arm (not the probes) if the W26 fix makes the merge preserve the playing clip — it is then dead code. The hand-off charter (STATUS:73-83) is actionable and correctly scoped: targets FileMerger/DirLoader merge + Dir.cpp/Object.cpp:131 Replace rewiring; does not touch the protected WorldCrowd oracle (Crowd.cpp:884-1000) or the proven RndMesh loader. Two charter amendments for W26 (see Q7): first establish WHY the second load-merge fires at beat 2.433 natively, and consider re-firing `play_clip` post-merge as a simpler alternative to preservation surgery.

## Q6 — CROWD safety → **A7 held; residual risk minor**

Verified at HEAD: Poll re-arm gated `gCrowdClipKeepEnabled() && mClipType == Symbol("crowd") && !mFirst && mClips && Starved()` (CharDriver.cpp:630-631); snapshot gated flag + `strncmp(clip->Name(),"crowd",5)` (:384-385); only the driver's own live `mClips` ObjPtr dereferenced (:635); PreEvaluate HX_NATIVE branch calls the identical `PreEvaluate` (:655) with probe-only additions; byte-identical `#else` (:669-671). WorldCrowd A/B flag-ON: zero clipType=='crowd' events in gameplay (gate-results.txt) — dormant where the oracle lives. Wii object byte-identical (Q3). Residual: `gCrowdKeep()` map (:50-53) is never pruned on driver destruction — a stale `const CharDriver*` key could alias a reallocated driver flag-ON (wrong-name re-arm, no crash path). Flag-OFF the map is empty. Fix opportunistically in W26 (erase in `~CharDriver`).

## Q7 — Wave-26 slate

1. **CROWD load-merge engine fix (HIGH EV, medium risk — recon-first).** Tractable: single deterministic repro (beat 2.433, pollFrame 72), full probe kit landed, charter written. Risk is the ObjPtr-rewiring surgery in shared Dir/Object merge code. De-risk by sequencing: (a) recon WHY a second Enter/merge fires at beat 2.433 natively (duplicate load? progressive load Wii also does?) — if native-only double-load, suppressing it is the cheap fix; (b) else prefer "preserve the crowd bank + re-fire play_clip post-merge" over general merge-preservation correctness. Bundle the near-black-material A6 discriminator as this lane's acceptance follow-on (it only unblocks once animating>0), not a standalone lane.
2. **FOREARM tail — reframed from "length bug" to TARGET-PROP POSING (MEDIUM EV).** Q1 cleared the length theory; the open defect is instrument-prop target bones grossly mis-posed natively (strum pick +50u in z, fret 98-216u, mic stand below floor, `mic_stand.ikhand` reach=0 unguarded). Fixing prop posing restores genuine in-song IK (clamp becomes a dormant safety net) and likely retires the ≤4.2 vignette residual + drumstick splay. Same async/posing family as lane 1 — cheap to recon alongside it.
3. **S5 combo-glow (SWEEP tail, LOW).** Cosmetic, well-scoped, good third lane if capacity allows.
4. Crowd near-black material — folded into #1's acceptance; do not schedule separately.

## ERRATA (append-only)

- **E-F1 (W25-FOREARM/STATUS.md:103):** commit hash `80c7037b` is the pre-rebase hash; the landed commit is `96f18e62`. Append a correction line; do not rewrite.
- **E-F2 (W25-FOREARM/STATUS.md:52-53):** "upperArm 6.2u + foreArm 9.6u ≈ 16u" is inconsistent with the logged `mAAPlusBB` reach 19.7-20.3 (6.2+9.6=15.8). The authoritative value is the logged 20.27 computed from `mLocalXfm` lengths; the per-segment figures are stale/mis-sourced. Append: "segment figures approximate; logged mAAPlusBB=20.27 is authoritative."
- **E-F3 (W25-FOREARM/STATUS.md §DISCRIMINATOR):** "same=1 on every sample" — the committed rootcmp log contains n=3 samples (3× same=1, 0× same=0). True as stated but thin; append the sample count.
- **E-F4 (src/system/char/CharIKHand.cpp:240, note only):** `mAAPlusBB > 0.0f` leaves `mic_stand.ikhand` (reach=0.00, no 2-deep chain) unguarded — pre-existing direct-set path, not a clamp regression; tracked under Q7 item 2.
- **E-C1 (src/system/char/CharDriver.cpp:43-45 and :374-378):** comments say the fix snapshots "the bank (mClips) and clip NAME" / contain a duplicated garbled sentence; the code snapshots the NAME only (:387-388). Fix the comments in the next touch of the file.
- **E-C2 (W25-CROWD/STATUS.md §landed change):** "recovers any crowd driver whose bank SURVIVED the merge intact" — in the observed sv3_a repro that set is {crowd_female04}, which never received `play_clip` and therefore has no snapshot; the re-arm provably recovers ZERO of the 8 drivers as-observed. The flag's value is prophylactic scaffolding for the W26 engine fix. Append this sharpening.
- **E-C3 (src/system/char/CharDriver.cpp:50-53, note only):** `gCrowdKeep()` map never erases on driver destruction — stale-key alias risk flag-ON; fix opportunistically in W26.

## FLIP DECISION — FOREARM `RB3_IK_REACH_CLAMP` default-ON: **YES**

mAAPlusBB is the correct linear reach (a+b; leg cross-check exact to 0.5%; solver's own PullShoulder radius); the clamp is a standard IK reach guard, a strict no-op inside reach, match-neutral (independently re-verified at HEAD), E1-verified, and contains a condition proven present during ON runs. The deeper defect it partially masks is target-prop POSING, not arm length — schedule that as W26 item 2; it does not gate the flip. Ship as the 14th default with `RB3_IK_REACH_CLAMP=0`-style opt-out semantics preserved (flag inverted to opt-out at flip time).

## WAVE-26 RECOMMENDATION

Two lanes: (1) CROWD load-merge engine fix, recon-first per Q7 with the material discriminator as acceptance follow-on; (2) instrument-prop target-bone posing investigation (FOREARM tail + mic-stand reach=0 + drumstick splay, same family). S5 combo-glow only as a third lane if capacity allows.

## FINAL VERDICT

**FOREARM: ACCEPT-WITH-ERRATA (E-F1..E-F4) + FLIP YES. CROWD: ACCEPT-WITH-ERRATA (E-C1..E-C3), partial stays in-tree default-OFF through W26 with a removal criterion, hand-off charter ACCEPTED with the two Q7 amendments.** Evidence quality this wave was high: both lanes' headline claims survived independent re-verification (objdiff at HEAD, probe logs, E1 screenshots, and a from-source audit of the reach math).
