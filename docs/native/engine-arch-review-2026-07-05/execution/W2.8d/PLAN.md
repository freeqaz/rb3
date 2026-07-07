# W2.8d — Bone-level factor attribution for the finger/hand rotation-basis shard

**Stage A.S1 (Wave 9 Lane A, Opus, diagnosis-first).** Charter: NAME the wrong
factor empirically (candidate (a) unfaithful live-bone rotation *evolution* vs
candidate (b) offset conjugated against the wrong frame). No fourth blind fix.

## Method (per WAVE9_REVIEW A1/A2/A3/A7)

1. **A7 baseline** — build agent binary on pin `a320f9d`; reproduce the RED
   `IK_SHARD_VERT` worst-appendage `wext` (~105-107u on `hands_naked`).
2. **A3 dual-skin engine probe** — `RB3_DUALSKIN_PROBE` in `Rnd_Wgpu_RB3.cpp` at the
   `:3617` palette-compose region (additive-only, gated, registered). For the worst
   far vertex of a selected mesh, dump per contributing bone: authored offset
   (`BoneOffsetAt`), live `boneWorld` (`WorldXfm`), live `LocalXfm`, weight, composed
   skin (`offset*boneWorld`), plus determinant + orthonormality of each factor and the
   rest-basis conjugation ΔR. Populate `goldens/w2.8-farvert/live_pose.txt` (asDrawn vs
   per-member-own-rest coherent ref) so the BL-A2 `RealPathFixture` gtest stops SKIPping.
3. **(iii-a)** — discriminate candidate (a) via the live bone's LocalXfm/WorldXfm
   rigidity + faithful pose evolution.
4. **(iii-b)** — discriminate candidate (b) via the offset-basis-vs-per-member-rest ΔR
   and the R·2sin(ΔR/2) fling prediction vs the measured far-vert separation.

## Exit
Factor-by-factor table + a NAMED verdict with magnitudes; probe committed (engine
flock), STATUS committed (rb3 flock). S2 = minimal fix ONLY if S1 names the factor
unambiguously; else honest writeup + Wave-10 design.

## Artifacts
- Engine probe: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (`RB3_DUALSKIN_PROBE`)
- Driver: `rb3/scripts/native/_w28d_probe.py`
- Fixture: `rb3/native/tests/goldens/w2.8-farvert/live_pose.txt`
- Raw log: `/tmp/w28d-probe/raw-probe.log`
- Build dir: `native/build-agent-W2.8d`
