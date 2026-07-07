# Wave 3 — Retrospective (written with Wave 4-8 hindsight)

**Run:** `wf_e0e4d32e-e39`, 22 agents, 2026-07-06. Engine `41b9e3a` → `6221a56`.
**Shape (post-Fable review):** Lane A `W0.3c → W1.6` (engine, sequential); Lane B `W2.2` (rb3, staged S1-S4); Lane C `W2.5` (tiny). W3.1 deferred to Wave 4 by the review.

---

## Goals

Two headline mesh-bug families were the target for the wave: **hands/fingers deform wrong** (SYS-1 bind fault → Lane B / W2.2) and the enabling infrastructure for **crowd/drum placement** (SYS-3 DrawContext state-leak → Lane A / W1.6, deferred-in behind fixing the draw-order nondeterminism W0.3c). W2.5 was a cheap game-side data-gap diagnostic.

## What shipped and held up

- **W1.6 DrawContext (SYS-3 state-leak fix) — SOLID, never refuted.** `WriteSceneUniforms` now returns an immutable `RB3SceneBinding`; the mutable `mSceneBindGroup`/`mSceneOffset` mirrors collapsed into `mActiveScene`; `RB3DrawContext`/`SubmitDraw` thread the scene binding per-draw (engine `9df8349`/`01c2642`/`6221a56`). The verifier's 15-run pre/post A/B was clean (888/888 draws, 20 distinct bind-group tokens identical). This is the real structural win of the wave and it enabled W2.1's placement contract in Wave 4. No later wave disturbed it.
- **W2.5 waypoint assert — held.** `HX_NATIVE`-guarded `MILO_WARN` in `BandConfiguration::SyncPlayMode` (rb3 `082f933d`), fail-red proven, Wii compile untouched. Correctly scoped as a diagnostic.
- **W0.3c reshape + Exit-B comparator — correct call, held.** S1 diagnosis correctly discovered the planned Exit-A transparent-sort fix was **structurally absent** from the rb3 binary (`TransparentQueue.cpp` is DC3-only) and skipped S2. The canonical multiset comparator landed in `drawlog-golden.py` (rb3 `5d254e00`), all fail-red classes RED. The "15/15 fresh-boot green" bar was honestly reported as blocked by the pre-existing CharEyes eps residual → W0.3d. This was disciplined partial reporting.

## What Wave 3 got wrong (refuted by Waves 6-8)

The wave's **headline claim** was: *"the hands/fingers fix exists, is numerically gated, and is one coordinator-signed flag-flip away from shipping — with the exact bind-pose-identity oracle whose absence caused the two BandPatchMesh reverts."* Hindsight says this was **substantially false on three counts**, and Wave 4 even repeated it ("the three worst mesh bug families now have landed fixes behind flags").

1. **The oracle that certified the real-band fix was BLIND to the actual bug.** W2.2's *synthetic* gtest (`test_hands_bind_oracle.cpp`, `SkinnedVertsMatchAuthored`) did include an R~200u fingertip and proved fail-red at 28.87u ≈ 200·sin(0.15). But the metric that certified the fix **on the real band path** was the draw-time `SKINPOS ≤ 92u` tripwire, which read max **68.2u** clean. That draw-time metric is origin/whole-mesh anchored. The actual finger shard is a **far-vertex rotation-basis** error (R·sin θ, largest at the fingertip, ~zero near the bone) — Wave 6's W2.8 side-agent measured "origin skinpos read CLEAN = **W2.2's oracle is BLIND to this**," and Wave 7's BL-A2 far-vertex oracle read **79-107u RED** on the exact build W2.2 called complete. The 92u tripwire was inherited from the failed `RB3_BOUND_REBAKE` rest-rebake and is itself a near-anchor number; the review closed the SYS-7 "no numeric invariant" hole with an invariant that could not see the defect geometry.

2. **The experimental flag whose "no benefit" was measured was INERT.** W2.2 reported `RB3_HANDS_BIND_FIX` (default-OFF) "measured no benefit → correctly NOT flipped." Wave 7 step-0 (WAVE7_REVIEW A5) showed the branch's `clipPlaying` trigger **fires 0× on the real band path** — it is dead code there (STATUS:226/240/377: when `perMemberBoneBindWorld` is 0 the new `if` is skipped and control falls through to the identical `miss++`). So the "measured null" was a false negative: nothing ran. The correct conclusion ("don't flip") was reached for the wrong reason, and the reasoning error masked that the whole approach was untested on the band.

3. **A flag-naming premise failure was created inside the wave** and only caught one wave later: the Wave-3 results table originally called `RB3_SKEL_REBIND_FULL` "W2.2's flag" — it is actually W0.1's known-broken full-body fail-red control; the real open residual is the foot/shoe path (`saddleshoe_skin.2`, `RebindOutfitBonesToOwnSkeleton`) → W2.6. Corrected retroactively by the Wave-4 Fable review. Separately, `RB3_HANDS_BIND_FIX` went **unregistered** because the flag census didn't scan `rb3/src/system/` (coverage gap, papered over in Wave 4 by `a537c2a3`).

Two static-fix classes for the finger shard were then proven dead ends by numbers only in Wave 7 (rigid-anchor `RB3_HANDS_POSEAWARE` distorts per-bone-authored hands 106→205u). The real fix (per-frame pose-aware basis correction) was still open at Wave 8. So the "headline bug family fixed" claim was optimistic by ~4-5 waves.

## Premise failures — and how they were (or weren't) caught

**The Fable pre-dispatch review (`WAVE3_REVIEW.md`) is a genuine success story worth naming:** it caught *four* false premises in the coordinator draft before a single agent ran — (a) the claim that W0.1/W0.4 goldens gate W2.2 (they run in the DC3-context `milo-engine-tests` and never execute the rb3 band-rebind path — invariance nets only); (b) the `setarch -R` interim gate, already refuted by W0.3b's own 15-run sweep; (c) the W3.1 ⟂ W1.6 file collision at `WriteSceneUniforms`; (d) the `Skeleton_Native.cpp` misattribution (it's the Kinect/COCO provider). That review gate paid for itself.

**The premise it failed to catch is the one that cost the campaign:** the review demanded and got a bind-pose identity gtest, but neither coordinator nor reviewer specified that the *real-band* draw-time gate must sample **far vertices**, not origin/near-bone. The review's own tripwire language (92u, 200-460u) is entirely near-anchor, so it reinforced the blind metric rather than exposing it. Caught 3 waves later by an unplanned user-report side-agent (Wave 6 W2.8), not by the gate design.

## Tooling gaps (concrete)

1. **Far-vertex rotation-basis oracle — the single missing capability.** A per-mesh metric = `max over verts of |skinWorld(v) − boneWorld·offset′·v|`, sampled where vertex-radius-from-bone is largest (fingertips), would in **one run on the default build** have shown 79-107u RED and told Wave 3 that `RebindHeadHandsAtRest` does not fix fingers. The campaign instead: built an origin-anchored oracle (Wave 3), discovered it blind via a user report (Wave 6 side-agent), built BL-A2 properly (Wave 7), then chased per-frame correction (Wave 8+) — roughly **4 waves / ~40 agent-stages** to arrive at a ~50-line gate that Wave 3's own tripwire discussion was one radius-choice away from. This is the highest-leverage tool the wave lacked.

2. **Flag-execution hit-count probe.** A generic `RB3_FLAG_HITCOUNT` that dumps per-flag branch-entry counts at process exit would make every "measured no benefit" claim self-validating. **One instrumented gameplay boot** would have shown `RB3_HANDS_BIND_FIX`'s branch fires 0× and flagged the null as "never executed, not measured-neutral." Instead Wave 3 recorded a false negative and Wave 7 step-0 re-derived the 0× fact by hand.

3. **Flag-registry census coverage of `rb3/src/system/`.** A config one-liner (landed `a537c2a3` in Wave 4). Its absence in Wave 3 let the naming confusion and the unregistered flag persist undetected — a small tool gap with outsized bookkeeping cost.

## Wasted effort

**Moderate.** The anti-revert flag-staging discipline (default-OFF, never blind-flip) meant nothing regressed — no revert, no broken build, the safety rules did their job. But the *analytical* waste was real: Wave 3 built an oracle that structurally could not see the target bug, then declared the headline mesh-bug family fixed and "one flag-flip from shipping," a claim Wave 4's headline amplified. Walking that back consumed Waves 6-8's hands lanes. Had the far-vertex oracle existed at Wave 3, W2.2 would have honestly reported "head/hands rest-capture is a net win but the finger shard is a separate rotation-basis fault requiring per-frame correction" — the exact conclusion reached ~4 waves later. The safety net converted a would-be regression into "only" lost analysis time, which is the system working as designed.

## Recurring bug families touched

- **Hands/finger shard (the wave's headline):** state after Wave 3 = **falsely declared fixed**; actually still open, oracle blind. Reopened Wave 6, reframed Wave 7, still needed per-frame correction at Wave 8. This retrospective's central lesson lives here.
- **Placement (crowd/drum):** correctly deferred to Wave 4 (needed W1.6's DrawContext, which Wave 3 delivered). W1.6 was the load-bearing enabler.
- **Wash / BOOTRNG / UI-text / grayscale venue:** untouched this wave — all surface Waves 5-7.
