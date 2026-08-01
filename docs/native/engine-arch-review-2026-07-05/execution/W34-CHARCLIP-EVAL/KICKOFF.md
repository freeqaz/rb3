# W34-CHARCLIP-EVAL — dynamic pose-pipeline attribution (the un-audited layer)

**Date:** 2026-08-01. **Coordinator:** Fable (main session). **Charter basis:**
W33-V1-POSE STEP-0 (committed `deea5e95`) + owner report 2026-08-01 ("hands are
still nightmare spindly tree branches, faces with missing bits").

## Why this lane exists (read before anything else)

The SKEL rotation-basis family was closed TERMINAL after ~15 waves
(R5-HANDS-ENDGAME GT-D + W21 L2-b double wall). **Every one of the 8+ dead fix
classes attacked the STATIC bind/offset layer** (rebake, repoint, reskin,
authored-offset provenance). Meanwhile:

- Dolphin D4/R5 exonerated native skeleton **statics** to ≤0.65° on cleanly
  comparable channels.
- W33-V1-POSE proved the detonation is **dynamic**: gameplay *vignette* body
  clips (`player2_f`, `player1_f`, `player3_m`, `player3_f`) stretch upperArm to
  **4.2× authored length** (world-Y flung +194) **with parentage intact**, while
  set_play performance clips stay near-anatomical (≤1.5). set_play REFUTED as
  cause (A/B re-swap identical). Clip DATA is authored-correct.
- Error magnitude tracks animation amplitude: rest pose ≈ exact, hands
  (high-sensitivity chains) = spindly branches, full-body vignette = explosion.

Conclusion: the un-audited layer is the **dynamic clip-evaluation path** —
compressed CharClip key decode → CharBones locals → world compose → palette.
Nobody has ever put per-layer taps on a detonating vignette frame. The
FAMILY-STOP reopen condition is already satisfied and documented
(W33-V1-POSE/PLAN.md §FAMILY-STOP; closure premise "band CharBones never
animate" is void).

**A bone STRETCH is an invariant violation** — rigid skeletons preserve
parent→child bone lengths under any valid pose. No Wii ground truth is needed
to localize this. That is the oracle the whole prior campaign lacked.

## Current-state evidence (2026-08-01 HEAD baseline, this dir /evidence/)

`gameplay_000/003/005.png` from `boot-to-song.py` (defaults, first song,
guitar/expert): 005 = vocalist tree-branch hands + branch-shard cluster behind
guitarist; 000 = floating horizontal leg on right member; 003 = magenta hand
shards + detached spike cluster. Skin-tone magenta/green casts are the separate
lighting/wash family — OUT OF SCOPE here, do not chase.

## STEP-0 (mandatory, NO fix code before checkpoint): per-layer attribution

On a detonating vignette frame (BAND_ANIM_ANAT probe, BandCharacter.cpp:711-918
already computes liveDist/authoredLen — reuse it), tap the chain for ONE
worst-case bone (e.g. the 4.2× upperArm) and its parent:

  L1 raw clip keyframe data as loaded (compressed rotation/scale/pos keys —
     dump bytes + decoded floats at the eval timestamp)
  L2 CharClip evaluation output (the local rot/pos the clip evaluator hands to
     the bone) — src/system/char/CharClip*/CharBones* decomp code
  L3 CharBone local→world composition (parent chain product)
  L4 skinning palette entry for that bone

The FIRST layer where the value goes wrong names the defect. Discriminators to
carry: (a) decode error (quantized-key decode/endianness/scale — L1/L2 wrong);
(b) composition error (locals fine, world wrong — L3); (c) space error (world
fine, palette wrong — L4, would contradict W2.8f joint-attachment GREEN, note
that was hub/hands not vignette). Cross-reference the SAME functions against:
DC3 decomp (~87%, /home/free/code/milohax/dc3-decomp/src/system/char/) — diff
implementations; Bank-8 Ghidra (`bin/analyze-function`) for the decode
functions' target semantics; and report.json match% of every TU on the eval
path — the W31 lesson says a 99.9x% message/dispatch-adjacent function can hide
a subsystem-killing divergence (SyncProperty precedent). Also audit HX_NATIVE
ifdef paths and any paired-single/intrinsic rewrite sites in the eval chain.

## Fix rules (post-STEP-0 only)

- Fix at the named layer ONLY. The 8 dead bind-side classes + reskin STAY
  BANNED (do not re-derive them; read at-limit catalog + R5 CLOSURE first).
- Decomp-source divergence → fix UNCONDITIONAL (no HX_NATIVE gate), unit match%
  must be baseline-exact-or-improved (batch_objdiff).
- Native-glue/engine fix → default-ON with `RB3_NO_*` opt-out once ON-vs-OFF
  evidence exists (W31 ack rule).
- Engine repo edits: commit in ../milo-native-engine first; do NOT bump
  MILO_ENGINE_PIN yourself — record the SHA for the coordinator.

## Acceptance

1. STEP-0 attribution table committed (layer × value × verdict, raw logs
   gzipped in evidence/, per-probe-tag grep -c count table in STATUS.md).
2. If fix lands: bone-anatomy A/B on the SAME vignette clips — maxRatio 4.2 →
   ≤1.5, world-Y fling gone, across ≥2 songs; set_play census non-regressed
   (RB3_SETPLAY_PROBE still shows rhythm/solo dispatch).
3. Screenshot A/B vs this dir's baseline (boot-to-song.py, same songMs ±150ms):
   branch-hands/shard-clusters visibly reduced or gone; coordinator does E1.
4. Gates: batch_objdiff touched units baseline-exact-or-improved; drawlog
   canonical-order 792 PASS; rb3-tests 116/0 (7 skip ok); bounded boots ≤10.
5. Checkpoint JSONs to evidence/ BEFORE returning (step0.json, fix.json).

## Fences

- Owned: src/system/char/**, src/system/bandobj/BandCharacter.cpp (probe reuse),
  engine char/pose path TUs if STEP-0 names them (record exact files).
- Do NOT touch: Rnd_Wgpu_RB3.cpp mitten/clamp code, bind/rebake sites,
  crowd chain (CLOSED — census trap: measure player0-3 with mClips pinned),
  lighting/wash/postproc, UI.
- Build via tools/ninja-locked (Wii gates) + own native build dir
  (native/build-agent-W34 — copy CMake config from native/build-native).
- Stage only your own paths; no git stash; no push.
