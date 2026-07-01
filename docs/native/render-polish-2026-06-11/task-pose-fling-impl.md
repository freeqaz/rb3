# task-pose-fling — the band char "pose fling" is a STALE WorldXfm CACHE on the per-member skeleton leaf bones (FIXED)

Render-polish wave-5 marquee task. Ports 9201-9209.
Engine worktree `wt-task-pose-fling` (`/home/free/code/milohax/milo-native-engine-worktrees/task-pose-fling`).
rb3 worktree `wt-task-pose-fling` (`.claude/worktrees/task-pose-fling`).

**STATUS: done — root-caused, fixed, verified. verified: true. needsEngine: true.**
**Wii byte-identical: rb3 src diff is EMPTY (fix is 100% engine-native-only).**

---

## TL;DR (the surprising result, refines wave-4)

The wave-4 framing — "the RAW ANIMATED POSE flings extremity bones to impossible world
coords (finger Y=123, foot Y=-108, a 186u leg); IK-independent; a pose-pipeline space
bug; the RB3 sibling of the unfixed DC3 feet-in-floor" — was on the right track that it
is NOT IK, but **mislocated the defect**. The fling is **NOT in the bone LOCAL pose** and
**NOT in the clip/quat decode**. It is a **STALE `mWorldXfm` CACHE on the per-member
skeleton's LEAF bones** (ankle / toe / finger).

Decisive measurement (engine `CHAIN_COMPOSE` probe, band player0, gameplay):
```
ankle: con=0  parW(knee world)=(-4.4,1.5,24.9)
       manualW (knee.world o ankle.local) = (-14.2,1.3, 9.2)   <- CORRECT compose
       cacheW  (ankle.WorldXfm())          = (-41.2,0.7,-33.5)  <- STALE, what's drawn
       dMag = 50.49  *** CACHE MISMATCH ***   wasDirtyPreRead=0
knee : dMag=0.00 (correct)   thigh: dMag=0.00   pelvis: dMag=0.00
```
The ankle's LOCAL transform is correct (orthonormal, det=1, sane tibia length 18.5u),
the knee/thigh/pelvis all compose correctly, but the **leaf ankle's cached world is
stale** — composed against an EARLIER flung intermediate parent pose and never re-read
after a later pose pass corrected the parent. `RndTransformable::WorldXfm()` returns the
stale cache (dirty bit already cleared). The leaf ankle then reads world Z=-33 (below the
floor) off a knee that is itself at Z=+25 — a geometrically impossible >2x AABB jump that
the engine V24 shard guard correctly refuses to draw. Result: legwear / footwear /
fingernails / gloves are guard-dropped (the documented `char-render` residual).

**Proof it's a cache, not a pose:** a forced top-down `WorldXfm_Force` of the leg chain
at draw time (`CHAIN_FORCE` probe) snaps the ankle from `(-42.9,0.7,-33.5)` to
`(3.6,1.0,4.1)` — at the floor. The LOCAL pose was right all along.

**Native-specific** (the wave-4 doc's instinct was correct that it's LP64/multi-pass):
the band's per-member skeleton is posed across MULTIPLE passes on native (the wave-2
reload-re-entrant rebind + IK + servo), and a `WorldXfm()` read between two of those
passes caches a pre-final (flung) intermediate world on the leaf, which nothing
re-reads before draw. The Wii does a single 32-bit pose pass per frame → no stale leaf.
The bone math itself is byte-identical with the Wii.

This is the same FAMILY as the DC3 feet-in-floor "IK reads an un-composed leg / ankle
reads at the pelvis" symptom (`dc3-decomp/docs/sessions/2026-06-09-xenia-xbox-foot-truth.md`
Push 12c) — both are stale/un-composed-world reads from a pose-order mismatch. RB3's is
larger (whole-leg-below-floor, not a 4u sink) and was fixable here because the fling is a
pure CACHE-COHERENCY problem, not a clip-layer under-bend.

---

## 1. ROOT CAUSE — the evidence chain (engine CHAIN_PROBE family, all render-inert, env-gated)

Walked ONE flung bone's parent chain (leaf -> root) dumping each link's LOCAL transform
(translation magnitude, det, row lengths) + WORLD, then added a manual-compose +
dirty-state + propagation test:

1. **The fling is entirely in the leaf's cached world.** `CHAIN`/`CHAIN_MTX`: every bone
   in the leg chain has det=1.000, row lengths=1.0, and sane bone-offset LOCAL .v
   (pelvis up Z=37.6, thigh -3.6, femur 15-20u, tibia 18.5u, finger 1-3u). No corrupt
   magnitude, no shear. Yet the leaf ankle world is Z=-33 while its parent knee is Z=+25.
   An 18.5u tibia cannot move its endpoint 68u — physically impossible from the local.

2. **`RB3_NO_POSEMESHES=1` (bones held at loaded bind LocalXfm) leaves the BAND leg ALSO
   flung** (ankle Z=-33 for band-player roots), but the EXTRAS leg is clean (Z=+4). So
   the fling pre-exists the channel write-back; it tracks the band's per-member skeleton
   INSTANCE, not the clip data. (Earlier "NO_POSEMESHES = clean" reads were misattributed
   to an extras character, not a band player — band vs extras must be separated by chain
   root.)

3. **`RB3_NO_CLIP=1` and `RB3_NO_IK=1` do NOT fix the band leg** (still Z=-33). Confirms
   wave-4: IK-independent and clip-independent.

4. **`CHAIN_COMPOSE` is decisive**: `Multiply(ankle.local, knee.world) = (-14.2,1.3,9.2)`
   (correct) but `ankle.WorldXfm() = (-41.2,0.7,-33.5)` (stale). dMag=50.49. The knee /
   thigh / pelvis all match their compose (dMag=0). Only the LEAF is stale, and its
   `wasDirtyPreRead=0` (not dirty → returns cache without recompute).

5. **`CHAIN_PROPTEST`: parent->child dirty propagation is INTACT** (1388 OK / 0 severed) —
   dirtying the knee DOES dirty the ankle. So the cache linkage is fine; the staleness is
   a pose-ORDER race: an intermediate `WorldXfm()` read clears the leaf's dirty bit while
   the parent is mid-pose, then the final parent re-pose dirties the leaf again but nothing
   re-reads it before draw.

6. **`CHAIN_FORCE` (force dirty + `WorldXfm_Force` top-down at draw) FIXES it** — ankle
   `(-42.9,0.7,-33.5)` -> `(3.6,1.0,4.1)`, all dMag=0. This IS the fix mechanism.

(Negative result: a Poll-time force in `BandCharacter::Poll` — even at the very end, after
outfit/inst Poll — did NOT fix it: the leg's correct LOCAL is established AFTER
`BandCharacter::Poll` returns, by a later world-graph / IK pass. So the recompute must run
at DRAW time. This is exactly the DC3 "no in-poll hook wins, the overwriter is later than
both" conclusion.)

## 2. THE FIX (engine, native-only, default-on, opt-out)

`milo-native-engine` `src/platform/Rnd_Wgpu_RB3.cpp`, `BandRnd::DrawMesh`, a pre-pass
just before the bone-palette fill loop (engine worktree commit `fd78455`):

```cpp
// before reading bones.bones[b] = BoneOffsetAt(b) * BoneTransAt(b)->WorldXfm():
//   for each referenced bone, walk its TransParent chain to root (stop at an
//   already-forced node), then force root->leaf:
//     node->DirtyLocalXfm();   // re-arm the cache
//     node->WorldXfm_Force();  // recompute against the parent's fresh world
//   visited-set (std::unordered_set) dedups across the mesh's bones.
```

- Runs for every skinned mesh's bones, so crowd/extras get the same correctness pass —
  but it's a no-op for them (their single pose pass leaves no stale leaf; measured: crowd
  bodies still NOT dropped, only the usual 65 thin extras-hair/eyebrow drops).
- Cheap: short shared chains, deduped per mesh (~50-100 forces/member/frame).
- Default-on; opt-out `RB3_NO_SKEL_WORLDFIX=1`.
- Native-only file (`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`, NOT in the Wii image, NOT
  compiled by DC3) -> Wii byte-identical + DC3-inert by construction.

The same commit also leaves the env-gated, render-inert `CHAIN_PROBE` / `CHAIN_MTX` /
`CHAIN_COMPOSE` / `CHAIN_FORCE` / `CHAIN_PROPTEST` diagnostics that localized the bug
(plus the cherry-picked `C8_PROBE`/`IK_SHARD_VERT` from `wt-task-ik-mispose`).

## 3. VERIFICATION (before/after)

| metric | FIX OFF (RB3_NO_SKEL_WORLDFIX=1) | FIX ON (default) |
|---|---|---|
| band player R-ankle world | **(-41.2, 0.7, -33.5)** below floor | **(3.6, 1.0, 4.1)** at floor |
| band garment guard-drops (matched 10-burst) | **186,073** | **10,027** (-94.6%) |
| top dropped meshes (OFF) | nailboots, talldocs, saddleshoe, kidgloves, fingernails, rolledpants... | gloves (residual), a few boots |
| crowd/extras drops | thin extras hair/eyebrows only | unchanged (no crowd-body regression) |
| crashes / NaN / asserts | 0 | 0 (50+ frames, multiple runs) |

- **Bone-world is deterministic + decisive**: ankle Z=-33 -> Z=+4 every run.
- **Visual**: `char-burst-capture` shot_05/shot_06 (fix ON) show all 4 band members
  standing upright, fully dressed, legs in place, NO screen-crossing shards
  (`/tmp/rp5-pose-fling/evidence/band_fixON_shot05.png`, `..._shot06.png`). The pink wash
  is the SEPARATE known venue-lighting residual, not this fix.
- **No regression**: crowd bodies render (not dropped); menu/song-select have no band
  skinned meshes so the pre-pass is a no-op there.

### Residual (now much smaller, a follow-up — NOT this fix's bug)
The remaining band drops are dominated by `gloves` (finger chains) + a small footwear tail.
Finger bones post-fix read sane WORLD (Z ~26-67 hand height, no Z=300 fling), but gloves
have a tiny bind extent so a moderate finger pose excursion can still exceed the 2x guard
ratio on some frames. This is the same hand/finger residual DC3 chased (`fingernails`/IK)
and is a tighter, separate problem — see wave-6 below.

## 4. WHAT I CHANGED

- **rb3 src: NOTHING.** `git diff src/` is EMPTY in the worktree -> Wii build byte-identical
  by construction. (I prototyped a `BandCharacter::Poll`-time force, proved it does NOT
  fix it — the correct local lands later — and fully reverted it; rb3 has zero net change.)
- **engine (`wt-task-pose-fling` @ `fd78455`)**: `src/platform/Rnd_Wgpu_RB3.cpp`
  (+`<unordered_set>` include @ L34) — the WorldXfm recompose pre-pass in `BandRnd::DrawMesh`
  (~L4046-4100 region, just before the palette fill loop) + the CHAIN_* diagnostics
  (~L4522 region, inside the C8_SLOT block). The branch is based on engine main `58254f7`
  (the current pin) and includes two cherry-picked diagnostic commits from
  `wt-task-ik-mispose` (`0d1b20c` C8_PROBE, `a037d3c` IK_SHARD_VERT).

## 5. LANDING NOTES (for the orchestrator)

- **Needs an engine pin bump.** Land engine `wt-task-pose-fling` (3 commits on top of pin
  `58254f7`: `0d1b20c` + `a037d3c` diagnostics, `fd78455` the fix), then bump
  `MILO_ENGINE_PIN` in rb3 `native/CMakeLists.txt` to the landed SHA.
  - If you want ONLY the fix without the diagnostics, cherry-pick just `fd78455` onto
    engine main — it is self-contained (the fix block + the `<unordered_set>` include).
    But `fd78455` ALSO carries the CHAIN_* diagnostics in the same commit; the C8_PROBE /
    IK_SHARD_VERT diagnostics are the two earlier commits. All are env-gated + render-inert.
- **Conflicts with siblings**: the fix touches `Rnd_Wgpu_RB3.cpp` — shared with the wave-2/4
  fret-sphere / venue-blowout / menu-fog / mesh-cache regions. My behavioral hunk is a
  self-contained block in `BandRnd::DrawMesh` immediately BEFORE the existing bone-palette
  loop (`for (int b = 0; b < numBones; b++) { ... bones.bones[b] ... }`, the `sBonesIdentity`
  loop). It does not touch the SHARD_GUARD ratio block, the bloom/halo composite, the
  lighting soft-clip (`standard_wgsl.inc` — different file), or DrawParticles. Low conflict
  risk; land after the other engine fixes and re-anchor on the `sBonesIdentity` loop.
- **No rb3-side land** (rb3 src unchanged). Only the pin bump commit.
- **Match-neutrality**: rb3 src byte-identical (empty diff). Engine file is native-only
  (`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`), not in the Wii image, not DC3-compiled.

## 6. WAVE-6 FOLLOW-UP (the residual)

The leg fling is closed. The residual ~gloves/footwear drops are a tighter hand/finger
case. P0 to chase next:
1. Apply the SAME stale-cache reasoning to the finger chains — verify with
   `CHAIN_PROBE='bone_R-middlefinger03' CHAIN_COMPOSE=1` whether the residual glove drops
   are (a) still-stale finger leaves (the pre-pass should already force them — confirm the
   glove's bones are in `owner->BoneTransAt`), or (b) a genuine pose excursion that exceeds
   the 2x guard for a small-bind-extent glove (then per-character-scale or relax the guard
   for known-small garments).
2. If (b): the V24 shard-guard `ratio > 2.0` threshold is the lever — a small glove with a
   normal finger curl can legitimately exceed 2x its tiny bind AABB. A bind-extent-aware or
   absolute-world-span guard (rejected before for crowd, but band gloves are a narrower
   case) could pass the gloves while still dropping true flings.
3. Ground-truth is no longer strictly needed for the leg (the cache fix proves the LOCAL is
   correct); for the glove residual, an Xbox/Wii hand-pose capture would only be needed to
   decide faithful-curl vs native-pose.
