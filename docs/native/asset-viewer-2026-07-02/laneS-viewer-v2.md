# Lane S handoff — rb3-viewer v2: visible skinned draw + inspection features

Date: 2026-07-02. Implements PLAN2.md "Lane S". Builds on Lane V (`5b8e0d05`) +
Lane G's CharHair CFG fix (`81f38f3a`). All changes are NATIVE-ONLY, viewer-side:
`native/src/rb3_viewer.cpp`, `scripts/native/render-asset.py`,
`docs/native/asset-viewer-2026-07-02/VIEWER.md`. **No engine, no Wii, no
shared-code change** (the shared `BandRnd::DrawMesh` path was NOT touched).

## TL;DR — what gates skinning, and what v2 changed

The engine ALREADY draws any mesh skinned when `owner->IsSkinned()` (=`!mBones.
empty()`) — `BandRnd::DrawMesh` builds a live bone palette from
`BoneOffsetAt(b) * BoneTransAt(b)->WorldXfm()` (`Rnd_Wgpu_RB3.cpp:3702, 4570`).
Lane V's claim that "the viewer draws the strand mesh un-skinned" was **wrong** —
SKIN_PROBE confirms `crazyhawk_resource.mesh` takes the **SKINNED-PATH** with 19
bones. `Mesh.cpp:903`'s `NOTIFY: Skinned mesh needs to be re-exported` is a benign
`MILO_WARN`, not a bone-drop.

The real reason `--sim` produced a byte-identical PNG was **four** distinct
standalone-milo problems, each of which had to be fixed for bone motion to reach
the pixels. All four are applied automatically when posing (`--sim` or
`--test-bone`; disable with `--no-hair-parent` for A/B):

1. **Sim never ran.** `CharHair::SimulateInternal` (`CharHair.cpp:534`) and
   `SimulateZeroTime` (:706) both early-out on `Root() && Root()->TransParent()`.
   A hair *resource* milo's strand roots are authored parented to a HEAD bone that
   lives in the character milo — loaded alone that parent ObjPtr is null, so the
   whole strand sim is skipped. **Fix:** `SetupHairForSim` synthesizes an
   identity-world parent Trans per rootless strand root (`SetTransParent(p,
   /*recalcLocal*/false)` preserves the root's world xfm exactly → rest render
   byte-unchanged; the sim gate now passes).

2. **Mesh frozen at bind.** The mesh's authored inverse-bind offsets
   (`BoneOffsetAt`) assume the IN-GAME bind world; standalone the bones sit at
   different world positions, so `skin = offset*boneWorld != identity` at rest.
   The engine's per-bone SKIN_CLAMP (active because `mNativeBonesRebound` is unset
   with no `BandCharacter`) then freezes the >12u bones back to bind — so the mesh
   drew its stored bind geometry and **bone motion never reached the pixels
   (MEASURED: rotating a bound bone 60° = 0 pixels changed)**. **Fix:**
   `RebindSkinnedMeshesToRest` recomputes each offset against the current rest pose
   via the engine's own `SetBone(b, bone, calcOffset=true)` (`mOffset = meshWorld *
   inverse(boneWorld)`, and meshWorld is identity for hair, so `skin==identity` at
   rest); setting `mNativeBonesRebound` tells the engine the mesh is correctly
   bound (clamp + rebake heuristics skip it).

3. **Draw reverts to rest.** `CharHair` advances bones via `SetWorldXfm`, which
   sets `mWorldXfm` but LEAVES `mLocalXfm` stale (`Trans.cpp:110`). The draw path
   re-dirties the bone chain and a dirty bone recomputes `world = local *
   parentWorld` from the STALE rest local — silently reverting every strand
   (MEASURED: pose-dump right after sim shows tips moved 10u, but BONE_PROBE at
   draw reads rest). **Fix:** `BakeSimPoseToLocal` snapshots every post-sim world
   and rewrites each local so `parent*local` reproduces it. (`--test-bone` doesn't
   need this — it sets LOCAL directly.)

4. **Guard drops the pose.** The engine's V24 SHARD_GUARD drops any skinned mesh
   spanning >2× its bind extent — a game-time safety net that also drops a
   legitimately spread hair pose. **Fix:** `setenv("SHARD_GUARD_OFF","1")` while
   posing (read per-draw; the static path is untouched so v1 renders stay
   byte-identical).

## S1 acceptance + evidence (`/tmp/rb3-viewer-v2/`)

**The skinned draw now reflects bone motion — PROVEN.** `--test-bone` rotating a
hair bone deforms the mesh coherently; the SAME rotation was 0 pixels before the
rebind fix.

| asset | static (no pose) | `--sim 30` | `--test-bone` |
|---|---|---|---|
| crazyhawk | 64205 (20.9%) | 84716 (27.6%) | 67654 (25° z) |
| ziggymullet | 63964 | 5334 | 76333 (30° z) |
| female_long | 68187 (22.2%) | (droops) | 101494 (30° x) |

- **`crazyhawk_static.png` vs `crazyhawk_sim30.png` visibly differ** (64205 vs
  84716) and the sim **converges without explosion**: sim30 (84716) ≈ sim120
  (84714), 0.2% pixel delta. The hawk fan is retained; one strand settles out to
  the side (a stable settled pose, not a collapse).
- **`crazyhawk_testbone.png` is the CLEAN, deterministic proof** — the full hawk
  with one strand rotated coherently out of the fan, rest of the mesh intact.
  This is the DirectPose-style CPU pose the wig saga wanted.
- Saved: `crazyhawk_{static,sim30,testbone}.png`,
  `ziggymullet_{static,sim30,testbone}.png`, `female_long_{static,testbone}.png`.

### Tradeoff (documented per PLAN2 "STOP at a CPU-side pose fallback")

The **dynamic `--sim` runs the real CharHair physics** and now reaches the pixels
end-to-end, but standalone — with no head-frame and no `CharCollide` volumes
reachable — the free-running solver diverges for some strands (crazyhawk settles
one strand out; **ziggymullet collapses to 5334** as its long back strands droop
off the rest-framed camera). I did NOT destabilize the shared draw path to chase
this. **`--test-bone` is the reliable, deterministic skinned-motion tool**; the
in-game `band-closeup-capture.py` gate (Lane G) remains authoritative for the
actual CharHair fix. An orienting-parent experiment (parent.m = root world rot)
made it worse (two strands fling) and was reverted — the identity parent preserves
rest exactly and is the least-bad frame.

## S2 features (all verified)

- `--test-bone <name> <deg> [x|y|z]` — exact-name then substring match; rotates one
  Trans about a local axis (default z) before draw. Applied AFTER the rebind so
  rest==identity and the rotation deforms.
- `--pose-dump <file>` — every RndTransformable's local+world xfm as JSON
  (`{name,class,parent,local:{m,v},world:{m,v}}`), dumped AFTER `--sim` so it
  captures the settled pose. Verified: 22 transforms for crazyhawk.
- `--pose-dump-bones a,b,c` — CSV name-substring filter (verified: `top-front,
  r-mid` → 4 transforms).
- `--draw-dir` — draw via `RndDir::DrawShowing()` (draw-order/transparency parity)
  instead of the mesh walk (verified: 64205, matches the mesh-walk static).
- VIEWER.md + render-asset.py updated (render-asset already passed args through;
  added the v2 flags to its docstring + surfaces the pose-dump line).

## S3 regression (item 3 — all PASS)

- **v1 static `female_hair_long` = 68187 (22.2%), ZERO `Can't make`** — identical
  to the Lane V acceptance baseline.
- **`RB3_RENDER_MESH=1` unchanged** = 68187 (22.2%) + its ORIGINAL 3 `Can't make`
  (it deliberately doesn't register char factories).
- v1 flags behave identically: `--list` (38 objs), `--hide femalehairlong` → 0
  non-clear, `--cam-dir`/camera flags unaffected. The static (non-posing) path
  never touches the rebind/guard/bake, so v1 output is byte-identical.

## Commit

`tools(native): rb3-viewer v2 — skinned draw + pose inspection` — SHA in
StructuredOutput. Staged ONLY: `native/src/rb3_viewer.cpp`,
`scripts/native/render-asset.py`, `docs/native/asset-viewer-2026-07-02/VIEWER.md`,
`docs/native/asset-viewer-2026-07-02/laneS-viewer-v2.md`.
