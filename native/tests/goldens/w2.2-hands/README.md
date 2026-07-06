# W2.2 hands bind-pose oracle — real-path fixture

`test_hands_bind_oracle.cpp`'s `HandsBindOracle.RealPathFixture` asserts the
numeric bind-pose identity invariant on **real** captured transforms when a
fixture file is present here (default path
`native/tests/goldens/w2.2-hands/bind_fixture.txt`, override with
`HANDS_BIND_ORACLE_FIXTURE=<path>`). Absent the file the test SKIPs — the
always-buildable math/compose arm still covers the invariant on synthetic data.

## How to produce the fixture (W2.2.S1a in-game probe)

At the captured bind frame — inside `BandCharacter::RebindHeadHandsAtRest`, right
where `Multiply(mesh->WorldXfm(), invRest, mesh->BoneOffsetAt(b))` bakes the
offset (`src/system/bandobj/BandCharacter.cpp:1425-1428`) — dump, per rebound
bone, the `mesh->WorldXfm()` (meshWorld), the captured `rests[b]`
(perMemberBoneBindWorld), and a handful of that mesh's authored mesh-local verts.

## Fixture format (whitespace-separated floats; `#` starts a comment)

One record per rebound bone, concatenated:

```
# meshWorld: m.x.x m.x.y m.x.z  m.y.x m.y.y m.y.z  m.z.x m.z.y m.z.z  v.x v.y v.z
<12 floats>
# restWorld (perMemberBoneBindWorld), same 12-float layout
<12 floats>
# vert count, then that many mesh-local (x y z) authored verts
<int N>
<3*N floats>
```

The test then recomputes `offset' = meshWorld · inverse(restWorld)`, composes
`skin = offset' ∘ restWorld`, and asserts `skin ≈ meshWorld` plus
`skin·v ≈ meshWorld·v` for each vert (same eps as the synthetic arm). A wrong
captured bind basis makes far-from-bone (fingertip) verts fail — the exact
BandPatchMesh hole the oracle closes.
