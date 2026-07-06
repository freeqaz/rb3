## B.S1 — done (planner, Opus)

PLAN.md written (design-only, no source edited). Core deliverables per WAVE8_KICKOFF Lane B / A4 / A5:

- **Per-frame math (§1):** per-bone conjugation `offset_b(t) = inv(A_b)·inv(L_b(t0))·L_b(t)·A_b·inv(L_b(t))`,
  keeping each hand/finger bone bound to its OWN live per-member bone (NOT wrist-collapsed) so the
  live joint's rotation is applied ABOUT the authored magnet frame A_b — cancels the growing R·sin(θ)
  twist per bone. Reduces to the proven rigid transport for uniform glove shells (no regression, §1.3),
  and is strictly a superset of the RB3_HANDS_POSEAWARE win without its per-bone-authored distortion.
  Verified at t0: skin=I (no latch pop), articulating under animation. One falsifiable assumption
  stated (§1.5, magnet/per-member share bone-local rest conventions up to the t0 bridge) → §4 decides.
- **A4 hazards (§2):** (a) write `drawn->GeomOwner()` offsets + `ownersDone` dedupe (reuse
  NativeRepinHandsRigid:1739 pattern); pristine A_b via mutual-exclusion with RebindHeadHandsAtRest
  (preferred) or first-encounter snapshot. (b) force each live bone's TransParent chain root→leaf
  (WorldXfm_Force/DirtyLocalXfm, Trans.h:77,150 — game-callable) BEFORE sampling L_b, mirroring the
  engine :3421-3446 pass, so Poll-time L_b == palette L_b (kills the stale-cache residual).
- **Flag (§3):** new `RB3_HANDS_PERFRAME_CONJ` default-OFF (getenv-cached no-op); RB3_HANDS_POSEAWARE
  stays landed + UNSET in all arms, mutually exclusive in code (A5); classification row append-only.
- **Measurement (§4):** IK_SHARD_VERT wext A/B, same-binary/same-member (B.S4 protocol) as the HARD
  exit — flag-ON worst appendage <20u vs ~106u OFF, FLING=0, nothing in 200-460u, 0 added drops,
  gloves not regressed. Both arms RB3_HANDS_POSEAWARE + RB3_PP_LUMA_CEILING unset (A5/A7), contract
  default-ON, baseline re-established on pin a94762f. RealPathFixture dual-skin arm = optional/staged
  Lane-A-tail (engine probe). Saddleshoe W2.6 bonus check (measure, don't assume).

FENCE respected: rb3 BandCharacter.{cpp,h} + native/tests/ + append-only engine classification row
only; NO engine code edits (design needs none — §6). Confirmed all accessors game-callable:
Trans.h:77 WorldXfm_Force, :150 DirtyLocalXfm; Mesh.h:226 SetBone, :241 GeomOwner, :257 BoneOffsetAt.

Verified source seams: palette compose Rnd_Wgpu_RB3.cpp:3529, owner-vs-own :3183-3200, WorldXfm_Force
pass :3421-3446; Poll seam BandCharacter.cpp:581 (post Character::Poll :527, post
RebindOutfitBonesToOwnSkeleton :572); NativeRepinHandsRigid owner/ownersDone :1739; existing
RB3_HANDS_POSEAWARE classification row engine json:207.

Handoff: W2.8c.S2 (impl, Opus) → W2.8c.S3 (verify, Opus, B.S4 protocol).
