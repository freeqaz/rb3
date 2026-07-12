# W31-HUBWALKER-SHARDS — STATUS (Lane D, diagnosis-first)

Base SHA fd119705 | engine pin b36bcfc | mandate: NO fix code before (iii) verdict.

## Milestones
- [x] Read kickoff (A1-A12, ten lints), R5-HANDS-ENDGAME/CLOSURE.md, W31-REPRO NOTES.
- [x] Target confirmed: hub walkers = `player0..3` = `world/shared/chars.milo` proxies
      to `char/main/main.milo` (CharCache.cpp:47-63). Pointer-keyed per W29 Q(b)/E7.
- [x] Probe infra: distinct TU `native/src/rb3_shardprobe_native.cpp` (A2-compliant —
      NOT BandCharacter/BandCamShot), read-only, env-gated `RB3_SHARD_PROBE_OUT`,
      modeled on `rb3_bonedump_native.cpp`. Emits per-walker per-skinned-mesh bound
      bones {name, pointer, WorldXfm, BoneOffset, d_angle_deg} + vertex-weight
      histogram + the driving CharClip's `ListBones()` driven-track set.
- [x] Static asset scan `char/main/gen/main.milo`: shard-candidate meshes are the
      `bone_*.mesh` deform meshes — forehead cone candidates `bone_forehead.mesh`,
      `bone_hair.mesh`, `bone_head.mesh`, `bone_head_nod.mesh`, `bone_brow-*.mesh`;
      prop/fan candidates `bone_bend_string0N.mesh`, `bone_guitar*.mesh`, drum bones.
- [ ] Build agent dir + run probe on live hub (in progress).
- [ ] Per-mesh per-bone discriminator table (i)/(ii)/(iii).
- [ ] Verdict: SKEL_FAMILY_STOP (memo) vs undriven-track (scoped default-OFF fix).

## Notes
- Reflink-copy of build-native for the build dir FAILED (CMake bakes the absolute
  binary dir → `--build` operated on the shared build-native and hit a dirty-manifest
  error). Recovered by a fresh `cmake -B native/build-agent-W31-HUBWALKER-SHARDS`.
  Did NOT build the shared dir; no lasting mutation intended.

## VERDICT (final) — SKEL_FAMILY_STOP

Discriminators resolved (full memo: VERDICT.md; table: evidence/shard_mesh_table.tsv):
- (i) shard meshes named via draw-path SKIN_CLAMP census. Forehead cone =
  `male_extras_eyebrows11.mesh`→`bone_forehead.mesh` (650u), `male_extra_head03`→
  `bone_L-brow1` (780u), `goatee_resource`→`bone_L-lipcorner` (650u),
  `female_extra_hair02`→`bone_hair_R-front03` (648u). Waist/boot fans = spine/torso/
  toe/knee/thigh/upperArm/finger bones on body+skin meshes (18u–780u mesh-local).
- (ii) bones DRIVEN (player body driver `main.drv` plays `playerN_{m,f}` walk clips,
  70 tracks; crowd/extras servo skeletons animate). Undriven-track hypothesis REFUTED
  (250–780u coherent live rotations, not bind-frozen 0u).
- (iii) SKEL-FAMILY. BONE_PROBE on `female_extra_head.mesh` (33 bones): skinDet=1.0
  orthonormal, coherent ~42° rotation basis, ALL face bones collapse to a shared apex
  skinPos≈(-287,56,123) vs offPos≈(0,-4,-64) — every face vert pulled ~290u to one
  point = the point-radial forehead cone. Same seed-R rotation-basis class as R5 87.2°.

Census-trap (E7, W29 Q(b)): pointer-keyed measurement refines the "player0-3" name-key —
CharCache player0-3 report 0 skinned meshes (+ empty expression.drv); the visible hub
shards are on the CROWD/EXTRAS street characters + the band outfit fringe (hippyfringe →
char/main/skeleton_unshared.milo static magnet). These fall under TWO already-closed
families: R5-HANDS-ENDGAME (band seed-R) and W23-29 CROWD chain.

Existing default-ON mitigations already present (RebindOutfitBonesToOwnSkeleton +
SKEL_REBAKE + V24 SKIN_CLAMP); face/hair/goatee excluded from rebake by design, left to
the clamp-to-bind backstop → the surviving cones are the accepted "650u goatee/hair"
clamp residual the engine comment + R5 §POST-FLIP ADDENDUM (FOREARM-FLOAT) already record.

Lint-4: no contradiction w/ RB3_PROP_POSE_FULL (orthogonal mechanism; W30 flip retest
already said fans survive). Lint-6: bound to R5 five-dead-cell table; STOP, no 7th cell.

NO FIX LANDED (mandate: fix only on the (iii)-undriven branch, which did not obtain).
Probe TU + harness + analyzer committed as reusable diagnosis tooling.
