# W26-PROP — PLAN

**Lane:** PROP — instrument-prop target-bone posing (reframed FOREARM tail).
**Charter:** WAVE26_KICKOFF.md (A4/A5/A6/A8/A9) + WAVE26_REVIEW.md Q3/Q4 + W25-FOREARM/STATUS.md.
**Engine pin at start:** `2088c68`. rb3 HEAD at start: `e56c8daa`.

## OWNERSHIP DECLARATION (A8, ENFORCED)

This lane edits ONLY these files (all in rb3 `src/system/`, compiled into rb3-native
from the rb3 tree — the engine tree does NOT carry them):

- `src/system/char/CharIKHand.cpp`  — probes only (extend IK_ROOTCMP discriminator); the
  reach clamp already lives here and STAYS untouched (RB3_IK_REACH_CLAMP is the safety net).
- `src/system/bandobj/BandCharacter.cpp` — prop-bone reparent / pose site (SyncObjects :2905;
  mInstDir Poll :905) if the fix lands here.
- `src/system/bandobj/BandWardrobe.cpp` — SyncTransProxies (:326) if the proxy-wiring is the gap.
- `src/system/rndobj/TransProxy.cpp` — RndTransProxy::Sync (:28-45) if the proxy resolution is the gap.

NOT owned / not touched: `char/FileMerger.cpp`, `char/CharDriver.cpp` (CROWD),
`native/src/rb3_render_hook.cpp` (GLOW), `native/src/rb3_session_trace.cpp`,
engine `../milo-native-engine/src/synth/FxSendNative.cpp` (NEVER stage).

CLOSURES honored: hands-finger family CLOSED; FOREARM binding CLOSED; RndMesh loader
PROVEN-CORRECT; WorldCrowd/RndMultiMesh oracle PROTECTED; RB3_IK_REACH_CLAMP STAYS.

## STEP 0 — DISCRIMINATOR (A4, checkpoint verdict BEFORE fix code)

CONFIRMED (W25 Q1/Q7): the arm reach (~20u = mAAPlusBB) is CORRECT; the IK targets are
mis-posed instrument-PROP bones (`bone_pick_strum` z=98 vs hand z=48.8; `bone_mic_stand_bottom`
y≈−30 below floor; fret 98-216u). The W25 `IK_ROOTCMP` chain dump proved `same=1` (roots match) —
so the parent chain resolves to the correct member root. That REFUTES the H-A/H-C proxy-root gap
and points at either:

- **(a) PARENT-CHAIN gap** — `RndTransProxy::Sync` (TransProxy.cpp:44) silently `SetTransParent(0,0)`
  on a Find-miss ⇒ subtree stranded in resource-milo local space; or `BandCharacter::SyncObjects`
  (:2905-2924) reparents `bone_prop0-3`/`bone_mic_stand_bottom` only if Find resolves.
  Signature: NULL/identity-rooted chain (root ≠ member).
- **(c) CLIP-BINDING gap** — chain correct (root == member) but the prop bones' LocalXfm is a
  static rest never animated to the playing position. Signature: correct root, but the far bone's
  LocalXfm places it far from its (correctly-posed) parent.

The W25 data already leans (c): roots match, and `bone_pick_strum` sits z≈98 while its correct
parent `bone_target_strum` sits z≈49.88 (right at the hand) — a ~48u LOCAL z offset of the pick
bone from its correctly-posed parent. STEP 0 confirms by dumping the far bone's LocalXfm + its
parent's world, in-song, and naming the mechanism.

## THE FIX (flag-first, default-OFF, HX_NATIVE, G3 case-1 byte-identical #else per A9)

Per the discriminator verdict — correct the prop-bone posing so the IK targets sit at the hand's
playing position. Options table filled AFTER STEP 0. Do NOT remove RB3_IK_REACH_CLAMP; do NOT
reopen hands/binding; do NOT modify MeasureLengths or extend the clamp to reach==0 (A6).

## GATES (A5)

- (i) per-ikhand pre/post target-distance histogram (IK_TGT_DBG): NO ikhand with pre-fix d ≤ reach
  may move its target beyond epsilon (proves I only moved MIS-posed targets).
- (ii) clamp-fire-count large RELATIVE drop over a fixed capture (NOT absolute 0 — intermittency).
- (iii) E1 per-instrument closeup: fret on neck, pick at strings, mic at mouth, no fan.
- flag-OFF drawlog-792 byte-identical; batch_objdiff==baseline on touched src/system units (case-1);
  rb3-tests 116/0.

Bounded: worst post-fix case degrades to today (clamp ON), never the spike-fan. If A5-i fails
(near-but-wrong targets re-engage IK worse than clip-pose), do NOT ship — narrow + report.
