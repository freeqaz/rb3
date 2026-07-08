# W22-FOREARM — STATUS

**Headline: BINDING EXONERATED → POSE bug on the foreArm bone. NOT FIXED (no
band-side repoint fix exists); NARROWED and handed off. Additionally NARROWED:
the float is a TRANSITION/camera-cut-frame artifact, absent in steady gameplay.**

All fix code was flag-gated default-OFF and has been **reverted** — the tree is
byte-identical to HEAD (`81a81de5`). No Wii change, no default flip, no pin bump.

## A1 discriminator verdict (checkpointed before fix code per A9)

The T2 M5 float = `gloves_resource.mesh` + `clearcoat_resource.mesh`, owner
`player3`, arm bones `bone_R-foreArm`/`foreTwist1`/`foreTwist2`/`bone_R-hand`.

Discriminator ran in stages (probe-only, zero-shipping-code first):

1. **Load hook (RebindHeadHandsAtRest, pre-Poll):** the outfit arm bones bind a
   `SHARED_ROOT` static magnet (`distinct=1`), and `Find` resolves a distinct
   instance — the char-skinning-deform binding signature. → looked like a
   BINDING bug. (`evidence/discriminator-loadbind-player3.log`.)

2. **Draw/Poll site (RebindOutfitBonesToOwnSkeleton, post-Character::Poll):**
   `Find` returns the magnet — the outfit arm bones are `own==bound`
   (`distinct=0`, 75/75 samples). There is **no distinct animating instance to
   repoint to at draw time.** (`evidence/pollsite-own-equals-bound.log`.)

3. **Subtree capture (ObjDirItr(this,true) — the member's OWN skeleton):** found
   the arm bones, but the member-subtree `foreArm`/`hand` instance **IS the same
   mis-posed `SHARED_ROOT` magnet** the outfit already binds (`capturedOwn ==
   bound`). Only `upperArm` had a distinct `skeleton_unshared.milo` instance; the
   `foreArm`/`hand` chain does not. So there is **no correct instance anywhere**
   to repoint the sleeve/gloves onto.

**Root cause = POSE, not binding:** the shared `foreArm` bone itself is driven to
world `y ≈ +130…+182` (detached/elevated) on a subset of frames, while the body /
`upperArm` stays stable at `y ≈ -0.8`. Bilateral (both R and L fling) ⇒ NOT a
1-line transpose/sign `ObjPair` (unlike the A4 venue-env `d988a301`).

## Gate table

| Gate | Result |
|---|---|
| A1 discriminator (1 boot, checkpoint early) | DONE — POSE, binding exonerated after 3 repoint variants |
| PRIMARY burst-ROI numeric (OFF vs ON) | Fix INEFFECTIVE: above-head float OFF=28 / ON=34, unchanged (see `evidence/burst-roi-off-vs-on.txt`). `FOREARM_REBIND` fired **0** times (nothing to repoint) |
| Float distribution | **Transition-only:** float present ONLY in low-skinned camera-cut frames (skinned 111–171); ZERO in all steady-gameplay frames (skinned 285–307) |
| SECONDARY R-vs-L symmetry | Bilateral fling (both arms) — rules out transpose |
| flag-OFF drawlog-golden 792 byte-identical | N/A — all fix code reverted; tree == HEAD |
| batch_objdiff == baseline (BandCharacter) | N/A — BandCharacter.cpp unchanged from HEAD |
| rb3-tests 116/0 | N/A — no BandCharacter touch shipped |
| A2 hands-closure guardrail | RESPECTED throughout — never touched finger slots, the clamp, RebindHeadHandsAtRest/mitten, and never set RB3_SKEL_REBIND_FULL |

## Why no fix shipped

The A2-safe fix (repoint the outfit arm bones to player3's OWN animating arm
instance) is **impossible by construction**: there is no separate correctly-posed
`foreArm`/`hand` instance — the body and the outfit share the one mis-posed
`SHARED_ROOT` bone. Three repoint variants (Find-result / captured-Find /
member-subtree) all no-op'd (`own==bound` at draw). A binding fix cannot remove a
pose fling. Shipping a flag that does nothing would be noise; reverted.

## Handoff for a future POSE lane (out of this binding-focused lane's budget)

- The `foreArm` chain is driven to `y ≈ +182` on **camera-cut / low-skinned-count
  transition frames only** — likely the same class as the walk-on/count-in pose
  freeze (`67e87ae1`), i.e. a clip driver / IK / animation-basis artifact on the
  shared char skeleton during transitions, NOT a steady-gameplay bug.
- Because it is transition-only and absent from steady gameplay, its player-visible
  severity is LOW (matches the memory's "intro-cinematic shards ≠ broken gameplay"
  triage gotcha). Recommend re-triaging severity before assigning a POSE lane.
- Next instrument: find what poses `bone_R-foreArm` high on the transition frames
  (BAND_ANIM_PROBE needs a working member-name match; the outfit-dir name did not
  match "player3"/"player" in this run). Compare the driving clip on a steady vs a
  transition frame.

## Evidence (all under `evidence/`)

- `discriminator-loadbind-player3.log` — load-hook distinct=1 + world (the binding signature)
- `pollsite-own-equals-bound.log` — draw-site own==bound + capture (binding exoneration + subtree-own == magnet)
- `burst-roi-off-vs-on.txt` — PRIMARY gate table (fix ineffective; transition-only)
- `burst-off-shot10.png` / `burst-on-shot10-float-persists.png` — the shard visual, OFF and ON (identical)
- `burst-off-dl10-float.json` — the drawlog for the float frame

Checkpoint: `/tmp/wave22-checkpoints/FOREARM.json` (verdict recorded).
