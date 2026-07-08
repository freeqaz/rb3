# W23-FOREARM — STATUS

**Headline: DRIVER NAMED. The forearm fling is driven by the BAND VIGNETTE
ANIMATION CLIPS (`player{0-3}_{m,f}`, `clipType='vignette'`) applied through the
`BandRetargetVignette` retargeting path, posing `bone_R/L-foreArm` to world
y≈+110..+193 at `Character::Poll()`. It is PERSISTENT (every clip frame), NOT
camera-cut-transition-only — the Wave-22 "transition-only" framing is CORRECTED.
Bilateral (R and L identical). This is a POSE bug (bone world is high), confirming
the W22 y≈182 finding. DISCOVERY-ONLY — no fix shipped; Wave-24 charter below.**

Only the probe block in `BandCharacter.cpp` was touched (the `%30`→event throttle,
HX_NATIVE + `BAND_ANIM_PROBE` env-gated, inert by default). No skinning/rebind/
mitten/binding edits. No default flips, no pin bumps, no new shipped flags.

## The named driver (evidence: `evidence/r-forearm-clip-driver.log`)

Event-triggered probe on `bone_R-foreArm.mesh`, `BAND_ANIM_PROBE='*'`, long
fixed-clock gameplay burst. First HI (fling) frames, showing the pre→post jump AT
`Character::Poll()`:

```
frame=252 member='player0' clip='player3_m' clipType='vignette' pre=(14.7,-1.5,52.0) post=(8.9,112.0,46.9) moved=113.8
frame=253 member='player1' clip='player2_f' clipType='vignette' pre=(10.4,-2.1,49.1) post=(-13.9,142.2,47.1) moved=146.4
frame=254 member='player3' clip='player1_f' clipType='vignette' pre=(10.4,-2.1,49.1) post=(2.4,183.5,44.4) moved=185.9
frame=255 member='player2' clip='player0_m' clipType='vignette' pre=(14.4,-1.5,51.0) post=(32.9,109.7,47.9) moved=112.7
```

- **Driver = the vignette clip.** `mDriver`/`FirstPlaying` is non-null with a
  named clip `player{0-3}_{m,f}` of `clipType='vignette'`, and the bone jumps from
  pre-Poll y≈-1.5 to post-Poll y≈+112..+183 **inside `Character::Poll()`** (which
  plays the clip). Pre-song heartbeat frames (frame=0/120/240,
  `FirstPlaying=(nil)`, `clip='(none)'`) show the bone correctly LOW at y≈-1.5 —
  the fling appears only once a vignette clip is playing.
- **NOT IK / not a camera-cut reset / not the walk-on freeze class.** The mDriver
  clip is present and named on every flung frame; the fling tracks clip-playback,
  not a transition edge. `mGroupName` cycles through camera-state groups
  (`stand`/`sit`/`closeup`/`extreme_closeup`) while the forearm stays high across
  ALL of them (frame 252 → 35987), so it is not gated on a camera cut.
- **POSE, not skinning-compose.** The probe reads the bone's own `WorldXfm().v`,
  which is high — the bone world itself is elevated (confirms W22's y≈182). The
  exploded arm/hand spike-fan (`evidence/forearm-fling-burst03.png`) is the skin
  faithfully following a bone that is genuinely mis-posed high.

### The retargeting path (source anchor)

`src/system/bandobj/BandRetargetVignette.cpp` is the vignette driver. `Poll()`
(`:29-46`) walks `mEffectors`; for each `playerN` entry it calls
`TheBandWardrobe->FindTarget(cur, mVignetteNames)` then `bchar->Poll()` — i.e. it
plays the generic authored vignette clips (`player0..3`) retargeted onto the four
live band members. `EnterDir()` (`:58-94`) also wires an IK layer
(`sIkfs` = `bone_R-foreArm.ikf`/`bone_L-foreArm.ikf`/`bone_R-hand.ikf`/... , the
exact forearm/hand bones that fling) via `BandIKEffector::mMore`. So the forearm/
hand are precisely the bones the vignette IK retarget is supposed to constrain.

### Bilateral + parent-bone contrast

- **L twin (`evidence/l-forearm-bilateral.log`):** `bone_L-foreArm.mesh` flings
  identically — pre y≈-1.5 → post y≈+98..+193, 30,333 HI events. Bilateral ⇒ not a
  1-line transpose/sign `ObjPair`.
- **Parent `bone_R-upperArm.mesh` (`evidence/r-upperarm-contrast.log`):** in the
  clip's settled pose the upperArm sits at y≈-48 (LOW/sane) while the child
  foreArm is at y≈+110+ — the fling is localized to the forearm/hand chain, not
  the whole arm. (upperArm also logs HI during the early offset window, but its
  settled clip pose is correct.)

## Root-cause hypothesis (for Wave-24, NOT proven this lane)

The vignette animation clips are authored/decoded such that the forearm/hand
chain's world pose lands ~+110..+190 units too high on native. Two candidate
mechanisms, both to be discriminated in Wave-24:

1. **Vignette IK retarget not constraining the forearm** — `BandRetargetVignette`
   wires `BandIKEffector` `_more` chains onto the forearm/hand `.ikf` bones
   (`sIkfs`); if `BandIKEffector::Poll`/`ApplyConstraints` isn't pulling the
   forearm/hand endpoint back to its effector target on native, the raw retargeted
   clip leaves the forearm flung. (BandIKEffector is NOT stubbed on native — the
   `#ifndef HX_NATIVE` at line 1 is only a MWCC paired-single asm guard.)
2. **Vignette clip decode / bone-basis** — the `player{0-3}_{m,f}` CharClip
   forearm channel decodes to a bad local/world basis on native (analogous to the
   char-skinning shared-magnet class), independent of IK.

## Wave-24 FIX CHARTER

**Goal:** stop `bone_R/L-foreArm` (+ hand) posing to world y≈+110..+193 during
in-song band vignettes, so the arm/hand skin stops exploding into spike-fans.

**Named target (start here):** `src/system/bandobj/BandRetargetVignette.cpp`
(`Poll`/`EnterDir`, the vignette retarget) + `src/system/bandobj/BandIKEffector.cpp`
(`Poll`/`ApplyConstraints`/`Solve`, the forearm/hand IK constraint). The vignette
clips `player{0-3}_{m,f}` (clipType `vignette`) are the animation source.

**Discriminator (do FIRST, before any fix):**
- A. **Is the vignette IK layer executing on native?** Instrument
  `BandIKEffector::Poll` for the forearm/hand effectors (`bone_R-foreArm.ikf` etc.)
  under an HX_NATIVE env gate: does `ApplyConstraints` run with a valid
  `mEffector`/`mTarget`, and does it move the forearm world-y back down toward the
  effector target? If it never runs (or runs with weight 0 / null target), the fix
  is in the retarget wiring (`EnterDir` `mMore` resolution / `mEffectors`
  population). If it runs but doesn't correct, the fix is in the IK solve.
- B. **Is it the clip decode?** Probe the forearm channel of the vignette CharClip
  directly (before IK) — is the raw clip pose already flung, or only after retarget?
  Compare against a steady-gameplay (non-vignette) forearm pose. This separates
  mechanism (1) from (2).

**Fix shape (match-neutrality):** prefer a match-neutral engine/source fix
(the retarget/IK wiring or clip decode). If none is match-neutral, an HX_NATIVE-
gated, default-OFF seam scoped to the vignette forearm/hand IK path (never the
skinning/rebind/mitten paths, which are CLOSED). Wave-22 proved no BAND-SIDE
REPOINT fix exists (own==bound at draw) — do NOT re-attempt a binding repoint;
the fix is upstream in the POSE (retarget/IK/clip), not the skin bind.

**Is it the walk-on count-in freeze class (`67e87ae1`)?** NO — probe evidence
refutes it. The fling is clip-driven and persistent through steady gameplay (not
a frozen empty-driver pose), and the driver clip is present + named on every flung
frame. Per memory `walkon_countin_pose`, the count-in thin-geo shards were a
SEPARATE pose-independent residual; do not conflate.

**Fix discriminator (acceptance for Wave-24):** with the fix ON, the
event-triggered probe (this lane's tool) should report the forearm world-y
back in the sane band (roughly matching upperArm's ~y≈-48..0, i.e. NO more HI
events at threshold 50 during vignette gameplay), bilateral, with a matched
band-closeup burst showing the arm/hand no longer exploding. drawlog-golden 792 +
batch_objdiff==baseline on any touched src/system unit + gameplay A/B.

## Gate table

| Gate | Result |
|---|---|
| Step 0 — BandCharacter.cpp compiles into rb3-native | PASS (`build.ninja:5772`; built clean) |
| Probe edit event-triggered, HX_NATIVE + env-gated | DONE (`BandCharacter.cpp:696-749`) |
| R-foreArm burst — driver named AT event | DONE (vignette clip `player{0-3}_{m,f}`; 28,758 HI events) |
| L-foreArm bilateral | DONE (30,333 HI events, identical fling) |
| upperArm contrast | DONE (settled clip pose LOW y≈-48; fling localized to forearm chain) |
| Pose vs skinning-compose | POSE (bone WorldXfm.v.y itself is +110..+193; confirms W22 y≈182) |
| drawlog-golden 792, probe env UNSET | PASS — exit 0, canonical-order match, 792 draws (probe inert by default) |
| No skinning/rebind/mitten/binding edits | RESPECTED (only the probe block touched) |
| No default flips / pin bumps / new shipped flags | RESPECTED |

## Evidence (all under `evidence/`)

- `r-forearm-clip-driver.log` — R-foreArm HI onset (pre→post fling AT Poll) + heartbeat samples + counts
- `l-forearm-bilateral.log` — L twin flings identically (bilateral confirmation)
- `r-upperarm-contrast.log` — parent upperArm settled pose LOW while foreArm HIGH
- `fling-persistence.log` — fling persists across all camera-state groups (not transition-only)
- `forearm-fling-burst03.png` / `forearm-playing.png` — the exploded arm/hand spike-fan visual

Checkpoint: `/tmp/wave23-checkpoints/FOREARM.json`.

---
## ERRATA (Wave-23 close-out review `45f81795`, ERRATA-F1) — HEADLINE RETRACTED
**DRIVER NAMED is retracted to DRIVER CANDIDATE.** The y>50 instrument ALSO fires on legitimate
stage-mark placement (in-song HI events are guitar_body-driven, pre-y already >50); **upperArm
logged MORE HI events (29,559) than foreArm (28,758)** and jumps identically at onset; player3's
forearm settles LOW mid-vignette. So "PERSISTENT every clip frame" and "localized to the forearm
chain" BOTH FAIL — **W22's transition-only characterization is RESTORED** pending a same-frame
multi-bone ANATOMICAL probe (child-parent distance > bone length) that reproduces the W22
divergence AND correlates it with a spike-fan capture. What HELD UP: BandRetargetVignette really
wires IK onto the flinging bones (`sIkfs`); pose-not-skinning; bilateral; freeze-class refuted;
+ an un-followed lead — CROSSED member↔clip pairings fling, the one MATCHED pairing settles sane.
Severity stays MED. Wave-24 = FOREARM-RECON FIRST (anatomical trigger + crossed-pairing lead);
do NOT dispatch a BandRetargetVignette/BandIKEffector fix on W23 evidence.
