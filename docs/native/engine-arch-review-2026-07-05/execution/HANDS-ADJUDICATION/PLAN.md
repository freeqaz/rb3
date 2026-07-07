# Lane H — HANDS-ADJUDICATION — PLAN (pre-registered BEFORE measurement)

**Charter:** WAVE15_KICKOFF Lane H + COORDINATOR ACCEPTANCE A1/A2/A9. Synthesis +
adversarial derivation, NO fix landing. Deliverable = `VERDICT.md` with (a) proof-level
derivation / (b) one missing measurement / (c) closure.

Engine pin `fdf0ad9` (untouched — ZERO engine edits; existing probes only).
Binary: `native/build-native/rb3-native` (built 2026-07-07 12:31, pin fdf0ad9) — no
source change needed, so no rebuild; arm C below doubles as the binary/protocol
validation gate (it must reproduce the RESKIN R2 flag-OFF signature).

## A2 raw-number reproduction (done BEFORE this plan's runs)

- **87.3° reproduced** from committed `W2.8f/evidence/readings.txt`:
  `hands_naked.mesh 38 | min=42.60 med=87.30 max=87.30 n=2214`. Bimodality carried:
  the co-sample table flips 87.3 (t1cnt=34) <-> 42.6 (t1cnt=36) across freshness
  recaptures — 87.3° is a MODE, not a constant, and t1cnt co-flips with it (the
  per-bone application/recapture state is a variable, per ACCEPTANCE A1).
- **Mixed-sign per-bone gaps reproduced** from committed
  `SKEL/evidence/apd_diag_gameplay_grep.log`:
  R-index01 bound=103.0 vs own 109.5/120.1 (+6.5/+17.1);
  L-index01 bound=142.1 vs own 121.6/119.6 (−20.5/−22.5);
  R-middlefinger03 bound=129.9 vs own 106.0/119.7 (−23.9/−10.2);
  L-middlefinger03 bound=112.6 vs own 147.6/127.2 (+35.0/+14.6).
  `bound` ptr identical across members (e.g. `0x…730140`), `own` distinct per member.
  NEW datum from the same log: player3 gloves rows show `bakedRest.ang != ownNow.ang`
  (e.g. 106.0 baked vs 119.7 ownNow) — at least one member's appendage bake basis is
  STALE/cross-member relative to its own bone's current rest.

## The one discriminating baseline (ACCEPTANCE A1, pre-registered)

**Question:** the record's decisive syllogism says the defect is UPSTREAM of all seven
dead bakes — either (a) the native animating bone worlds `own_live(t)` diverge from
Wii's live bone worlds for these bones, or (b) the authored-offset↔bone PAIRING is
wrong natively (offsets applied against instances/bases they were not authored for).
The Wii composition `v·A_b·live_b(t)` is exposed natively behind `RB3_NO_HEAD_REBIND`
— but ONLY its reskin-contaminated variant was ever measured (R2: mean 136u).

**Protocol (identical for both arms):** `scripts/native/keyboard-to-gameplay.py
--song-downs 3 --game-burst 15 --burst-interval 0.3` (the RESKIN R2 sighting
protocol), `RB3_FIXED_CLOCK=1`, headless HTTP, own free port, pgid-only cleanup.
Instruments (existing, engine-untouched): `RB3_HANDS_ATTACH_PROBE=1` (per-block
`[HANDS_ATTACH]` line: wext co-sampled UNGATED + rebound flag + Tier-1/xcheck angle
of the UPLOADED palette = pairing evidence) and `IK_SHARD_VERT='*'` (worst-vert bone
world rows + off.v when wext>60).

- **Arm C (control / protocol validation):** all hands flags UNSET (default build
  behavior; refuted flags UNSET incl. `RB3_HANDS_RESKIN`; `RB3_NO_SKIN_CLAMP` unset =
  clamp ON, but the clamp skips rebound band meshes so it is inert for hands_naked
  here). MUST reproduce the R2 flag-OFF signature: hands_naked wext ~60–106u when >60
  (R2 gate window), [HANDS_ATTACH] wext spanning ~35–106u, mean ~70–75u, many distinct
  values (animating), rebound=1. If this fails, STOP: the binary/protocol is not the
  record's and no cross-run comparison is valid.
- **Arm W (the missing measurement — clean Wii-composition):** `RB3_NO_HEAD_REBIND=1`
  with reskin OFF and every other hands flag UNSET. Because unrebound meshes are
  guard-DROPped and clamp-frozen by default (which would both censor the probe and
  fake a "coherent" reading), arm W adds `RB3_NO_SKIN_CLAMP=1 SHARD_GUARD_OFF=1
  SHARD_RATIO_DBG=1` (the last one keeps the probe block alive under
  SHARD_GUARD_OFF; all three are existing, documented A/B levers). This exposes the
  RAW authored composition `v·A_b·bound_live_b(t)` exactly as the loader paired it.

**Pre-registered predictions:**

- **P-frozen (pairing defect, (b)):** hands_naked in arm W draws at a NEAR-CONSTANT
  wext (<=3 distinct values across the burst window; IK_SHARD_VERT boneWorld.v static
  if any lines fire) and does NOT track the animation — because the instance the
  loader paired the authored offsets with (`bound`, the shared skeleton) never
  animates natively. This confirms the 2026-06-05 loader finding at the composition
  level: NO native instance both ANIMATES and CARRIES the authored bind basis, i.e.
  the Wii pairing (one instance = bind carrier AND animation target) does not exist
  natively. Verdict path (a)-derivation: defect = loader/instancing (the bound/own
  split), every downstream bake was structurally unable to fix it, and the minimal
  fix is per-member skeleton instancing at load (honor `kInlineCached` per-reference
  copy semantics) OR an animation-transfer that drives `bound` from `own` — both
  out-of-band of any offset/vert bake.
- **P-anim-diverge (bone worlds defect, (a)):** hands_naked in arm W ANIMATES
  (many distinct wext values) at large wext (>=100u sustained, like the contaminated
  136u variant) — the bound instance DOES receive animation natively but in a basis
  ~87° off the authored bind. Then the defect is in how native animation poses those
  bones (channel decode / parent chain / convention), and the follow-up is the
  Dolphin+milo-trace bone-world capture (WAVE15_REVIEW R-A rank 3) comparing the SAME
  bones' worlds.
- **P-coherent (premise death):** hands_naked in arm W animates at CLEAN wext
  (<=70u envelope, tracking the clip) — the authored composition is fine natively and
  the shard is INTRODUCED by the default rebake itself. Then the fix derivation
  targets the rebake's rest capture (stale/cross-basis bake, cf. the player3 gloves
  datum) and the arm-C-vs-arm-W per-bone Tier-1 delta names the wrong factor.

**Interpretation guard:** wext in arm W under SHARD_GUARD_OFF+NO_SKIN_CLAMP is not
numerically comparable to arm C's clamped/rebound values; the discriminator is the
SHAPE of the signal (constant vs animating; tracking vs exploded), plus rebound=0 and
the Tier-1 xcheck angle (authored invOff vs the freshness rest of the SAME palette
bones = the pairing gap under the authored composition).

**Secondary pairing evidence (asset-side, no emulator):** the R1 authored-offset dump
(`RESKIN/evidence/reskin-probe-gameplay.log` `[RESKIN_OFF]` rows) gives A_b per
member/gender; `SKEL/evidence/apd_diag_gameplay_grep.log` gives bound/own rest angles
per bone. Cross-check: does `inv(A_b)`'s rotation equal `bound`'s bind basis (pairing
authored-for-bound) or `own`'s gender rest (pairing authored-for-own)? This decides
WHICH instance the offsets were authored against, from committed evidence alone.

## Checkpoint / process

- Checkpoint `/tmp/wave15-checkpoints/H.json` (check-first, write-before-return).
- Commits under `flock /tmp/rb3-git.lock`, own files only. No engine TU edits, no
  flips, no pin bumps, no new flags. FxSendNative.cpp untouched.
- Evidence into `HANDS-ADJUDICATION/evidence/` (arm logs greps + analysis).
