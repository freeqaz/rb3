# W33-V1-POSE — PLAN + STEP-0 discriminator record

**Lane 2, Wave 33.** Base SHA `6186706e`, engine pin `2ea8e34`. Symptom V1:
band members in grossly wrong full-body poses during gameplay (bassist folded
over the bench; a member bent double with limbs radiating; vocalist folded
backwards, leg horizontal). Charter prime suspect: **W31 set_play dispatching
body performance clips exposes a latent transform/basis bug.**

## Outcome headline: charter's set_play suspect REFUTED; real mechanism = vignette-clip seed-R rotation-basis explosion (out of lane fence). NO fix landed.

---

## Environment blocker (declared up front)

Local gameplay **3D render is BLACK** here: every gameplay frame is `max=0`
pixels (`/tmp/w33v1/run1..4`), while `song_select`'s 3D backdrop renders fine
(`mean=74`). This is the **V4 render-infra/GPU-fallback** issue (Vulkan "Found no
drivers" → the gameplay 3D pass produces nothing), NOT the V1 pose bug and NOT in
this lane's scope. It makes **screenshot pose-gating impossible in this
environment.** The charter anticipated exactly this and prescribes a
**render-independent per-bone transform dump** as the discriminator — which is
what STEP-0 uses. V1 itself is reproduced in the committed W31 evidence
(`../W31-VISUAL-PASS/evidence/bath_wide_poses_broken.png`, limbs radiating at
~90°).

## STEP-0 discriminators (checkpointed BEFORE any fix; no fix was made)

**(i) Reproduce + A/B control (CA1, verbatim honored).** The W31 set_play fix is
unconditional (`a3916764`, no flag). A/B control = throwaway worktree
(`tools/setup-worktree.sh w33v1-abswap`, deleted after) with the 5
`SYNC_PROP_SET` intensity sites in `BandDirector.cpp` (`:2143-2150`;
{bass,drum,guitar,mic,keyboard}_intensity) re-swapped to the pre-W31
`SendMessage(_val.Sym(), "<inst>")` order — never committed. Native rebuilt in
the worktree (Dawn_DIR from the main cache). Result: re-swap dispatched **0**
`rhythm/sing_ext` performance clips (band idle-only = pre-W31 state; proof:
`BANDPERF_CLIP` rhythm/sing count ON=64 vs OFF=0), yet **V1 persists** →
"persisting = other layer."

**(ii) Name the clips + bones (DATA vs TRANSFORM).** Existing in-fence
render-independent probe `BAND_ANIM_PROBE`/`BAND_ANIM_ANAT`/`BAND_ANIM_CHAIN`
(BandCharacter.cpp:711-918, W23/W24 FOREARM) dumps per-bone world positions and
the arm+spine stretch ratio (`liveDist/authoredLen`, detached if >1.5).
- Clip DATA is CORRECT: set_play dispatches authored `stand_rhythm_ext_*`,
  `stand_sing_ext_*`, sit idle to player0-3 (run1 census).
- Severe detachment (upperArm ratio up to **4.2**, world-Y flung to **+194**)
  occurs ONLY under `clipType='vignette'` clips **player2_f / player1_f /
  player3_m / player3_f** (the camera-cut vignette/closeup body clips). The
  set_play `guitar_body` performance clips are near-anatomical (maxRatio 1.5,
  heartbeat 1.3). → wrong-**TRANSFORM** apply on vignette clips.
- CHAIN dump: parentage intact (`inParentKids=1` on every bone) at rest → NOT a
  broken-hierarchy / frozen-world bug. The explosion is a dynamic bone
  stretch/detach during vignette-clip playback (a pure rotation-hierarchy break
  would keep bone lengths; a stretch means the bone's composed world transform
  is wrong).

**(iii) Basis test — POSITIVELY names the rotation-basis class.** The signature —
distal-bone detach/fling (upperArm 4.2× its authored length from the clavicle,
world-Y to +194) with parentage intact — is the **seed-R rotation-basis** fling
signature already documented by the SKEL/hands family (the ~87.3° seed-R rebake
that flings distal verts/bones; `RB3_HANDS_MITTEN`, `RB3_HANDS_AUTHORED_REPOINT`
et al., R5-HANDS-ENDGAME). Here it appears on the **upper arm / full body**
because vignette clips animate the **whole band skeleton**, not just the hands.

### FAMILY-STOP reopen justification (new evidence, per binding clause)

The SKEL rotation-basis family is CLOSED with a binding STOP. STEP-0(iii)
positively names it, and the **new-evidence** condition is met: the Wave-16→18
hands-family closure rested explicitly on the premise *"substrate never animates
band CharBones"* (band members sat idle, bones static; hands closed
`R5-HANDS-ENDGAME/CLOSURE.md`). **That premise is now VOID** — W31 set_play +
the gameplay vignette system make the band CharBones animate at full-body scale
for the first time, exposing the same seed-R basis error on the whole skeleton.
This is genuinely new surface (gameplay band chars playing vignette body clips),
not a re-open of the same hands cell.

**However:** the faithful fix for the seed-R basis is the **CharBones
pose-pipeline basis**, which the closure records as *engine, out of the lane
fence* and **15-wave EXHAUSTED without a faithful fix** (only the hands-scoped
`RB3_HANDS_MITTEN` render-side workaround exists; there is no body analog and it
lives in engine `Rnd_Wgpu_RB3.cpp`, outside this lane's `src/system/char/*` +
`BandCharacter.cpp` fence). Suppressing gameplay vignettes would be UNFAITHFUL
(retail plays them). Undoing W31 is wrong (set_play is proven-faithful and is
NOT the cause). **Therefore this lane lands NO fix** and recharter-recommends the
CharBones-basis family with this full-body/vignette evidence.

## Disposition: RECHARTER (see STATUS.md self-grade).
