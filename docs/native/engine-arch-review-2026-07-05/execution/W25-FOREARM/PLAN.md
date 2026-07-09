# W25-FOREARM — PLAN (CharIKHand upper-arm target-space fix)

Lane: FOREARM (Opus). Parent: W24-RECON/REPORT.md + WAVE25_REVIEW.md (A1-A9 binding).

## A8 FILE-OWNERSHIP DECLARATION
This lane OWNS and may edit:
- `src/system/char/CharIKHand.cpp`
- `src/system/bandobj/**` (BandCharacter.cpp, BandWardrobe.cpp, etc.)
- `native/src/**` (native-only flow/shim, if the fix is native-side)
- `docs/native/engine-arch-review-2026-07-05/execution/W25-FOREARM/**`
- `scripts/native/_w24_forearm_capture.py` (probe extension, shared with recon lineage)

NEVER stage: `native/src/rb3_session_trace.cpp`, engine `src/synth/FxSendNative.cpp`.
CROWD lane owns `char/CharDriver.cpp` + band3 flow — do not touch.

## STEP 0 — baselines + discriminate (CHECKPOINT before fix code)

Baselines RECORDED (objdiff, this tree):
- unit `main/system/char/CharIKHand` = 99.16526%
- `Poll__10CharIKHandFv` = 96.127235% (raw 96.1)
- `MeasureLengths__10CharIKHandFv` = 81.355% (raw 81.3)

A3 diagnose verdict (DONE):
- Poll: regalloc/branch-polarity noise in the multi-target weight loop (r30<->r31,
  f0<->f26; one bne/bge polarity; fmuls/fnmsubs strength-reduction). NOT a
  CFG/semantic bone-displacement bug. bank_divergence=TRUST m_ratio=0.97.
- MeasureLengths: paired-single regalloc noise (f0<->f1,f2<->f3). Math intent
  faithful. bank_divergence=CAUTION, mismatch-class=regalloc not semantic.
- CONCLUSION: this is NOT the CharHair (99.6%-hid-a-CFG-bug) case. The IK math is
  faithful. The bug is DATA/FRAME placement (target-space), not decomp infidelity.
  => G3 case-1 (native-only fix; batch_objdiff MUST equal baseline exactly).

DISCRIMINATOR (H-A/H-B/H-C): dump `bone_target_snare.mesh` TransParent chain-to-root
vs member skeleton root (`player0`), compare world roots.
- Roots differ ~100u  -> H-A (resource-dir parent frame wrong) OR H-C (proxy resolved
  to wrong/stale object — V23 SyncTransProxies class). Distinguish parent-frame-wrong
  (H-A) vs proxy-resolution-wrong (H-C) before editing.
- Roots equal, arm rest basis ~100u high pre-IK -> H-B (skeleton basis; fix NOT in
  CharIKHand).

## THE FIX (flag-first, default-OFF)
Correct the target-space so IK reaches instrument tips without over-rotating the
upper arm. Site chosen by discriminator. Do NOT ship RB3_NO_IK. Do NOT reopen forearm
binding / hands family.

## GATES (recon acceptance)
- in-song (game_screen) upperArm max stretch ratio < 2.0 WITH IK ON
  (`scripts/native/_w24_forearm_capture.py`, parse `[BAND_ANIM] evt=ANAT`).
- E1 guitarist/drummer closeup (coop_g_cg / coop_d_*) intact arms, NO spike-fan.
- flag-OFF drawlog-golden 792 byte-identical (`drawlog-golden.py --fixed-clock --canonical-order`).
- batch_objdiff per A2 case-1: MUST equal baseline exactly (Poll 96.127235, unit 99.16526).
- rb3-tests 116/0.
