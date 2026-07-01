# Impl: `crowd` — crowd merged/frozen → spread + animating (Fix A)

Wave-2 implementer, 2026-06-11. Implements the scout's **Fix A** (crowd skeleton
rebind). Fix B (2D imposters) and Fix C (venue bridge) remain OUT of scope; their
designs stay in `scout-crowd.md`.

**Branch:** `wt-task-crowd` (worktree `/home/free/code/milohax/rb3/.claude/worktrees/task-crowd`)
**Commit:** `dcad5834`
**Engine:** NO engine change (Fix A is rb3-only). Engine worktree untouched, pin
unchanged (`8fb669d`).

---

## TL;DR — result

`crowd_body` shard-guard drops **63339 → 6680 per song (−89.5%)**. Crowd renders
full-bodied, spread across the floor, idle-animating. BEFORE: only floating heads
(bodies dropped). Verified visually (same camera) + by the engine's SHARD probes.
Wii build **byte-identical** (all edits `#ifdef HX_NATIVE`).

Before/after (same `coop_dir_crowdg.shot`):
`/tmp/rp2-crowd/crowd_before_after_crowdg.png`
(BEFORE = left, only floating heads; AFTER = right, full crowd on the floor).

---

## What changed

`src/system/world/Crowd.cpp` (HX_NATIVE only):

1. New file-static helper `RebindCrowdCharBonesToOwnSkeleton(Character *curChar)`
   (before `WorldCrowd::Poll`). For a crowd archetype Character it collects every
   skinned mesh (hashtable `ObjDirItr` + `mDraws` + each `mLods[i].Group()/
   TransGroup()`, recursing the draw tree via `RndDrawable::ListDrawChildren` —
   the band fix's exact collection), and for each mesh's **`GeomOwner()`** (where
   the engine reads the palette) re-bakes every bone's inverse-bind offset against
   the current rest pose: `owner->SetBone(b, own, /*calcOffset=*/true)`, where
   `own = curChar->Find<RndTransformable>(boneName, /*parentDirs=*/false)`
   (dir-scoped → the archetype's own bone; `CharUtlFindBoneTrans` as fallback).
   Latched once per mesh via `RndMesh::mNativeBonesRebound`.
2. Call site in `WorldCrowd::Draw3DChars`, right after `curChar->SetWorldXfm(spXfm)`
   and before `DrawShowing()` (the point where the mesh is reliably posed +
   reachable). Latched, so it is a no-op after the first draw of each mesh.
3. Opt-out `RB3_NO_CROWD_REBIND=1`; diagnostics `CROWD_REBIND_PROBE=1`.

Also added `scripts/native/crowd-shot-capture.py` — navigates to gameplay,
freezes the band director cam (`{$band_director set disabled 1}`), forces the
crowd shots, captures an animation pair. Reused for before/after.

## ROOT CAUSE (probe-proven — refines the scout's mechanism)

The scout was right about the geometry (two co-resident same-named WorldCrowds —
gameplay venue + tv3 vignette theater — ~2040u apart, body meshes torn across
that span, ratio ~25x → V24 shard guard drops them ~20k/song) but wrong about the
seam:

- The drawn mesh's **own** bones read clean (`own == bound`, single-location span
  ~64u). A bone-INSTANCE rebind (the scout's prescribed `SetBone` to a different
  instance) is a **no-op** — `diffInstance=0` for every bone, both at Poll and at
  draw. The engine never reads the drawn instance's bones anyway.
- The engine builds the GPU palette from `mesh->GeomOwner()->BoneTransAt(b) *
  BoneOffsetAt(b)` (`Rnd_Wgpu_RB3.cpp:3148/3833`). For these meshes
  `GeomOwner()==self` (NOT shared between the two worlds, despite same names).
- The poison is the **OFFSET**: the crowd meshes' authored inverse-bind
  `BoneOffsetAt` does not match the native skeleton's bind pose, so composing it
  with the bone's posed WorldXfm flings vertices ~2000u. Confirmed: forcing
  `calcOffset=true` (recompute offset) collapsed worldExt 2240u → ~270-650u and
  drops 63k → 9920; latching the recompute at rest got 6680. The engine's own
  crowd skin-clamp comment already says exactly this ("crowd ... bind does not
  match their meshes' inverse-bind offsets under the native load").

So Fix A is the band fix's **family** (mesh skinned against a wrong bind) but a
different seam: an inverse-bind **offset** rebake, not a bone-instance rebind.

## VERIFICATION

Pass criteria from `scout-crowd.md` §4:

| criterion | before | after | verdict |
|---|---|---|---|
| 1. crowd_body shard-guard drops | 63339 (121924 w/ opt-out) | **6680** | PASS (−89.5%) |
| 2. crowd_body ratio | ~22-27x (DROP) | mostly 0.99-1.07 (PASS); residual 3-8x on 2 archetypes | PASS (no ~25x) |
| 3. visual: full bodies, spread, animating | only floating heads | full crowd on the floor (composite) | PASS |
| 4. band chars unregressed | — | drummer/guitarist render coherent, no shards | PASS |

Commands (in the worktree):
```bash
cmake --build native/build-native --target rb3-native -j
SHARD_DBG=1 SHARD_RATIO_DBG=1 python3 scripts/native/crowd-shot-capture.py \
    --port 8744 --diff hard --out /tmp/rp2-crowd/after --tag fin
grep -a SHARD_GUARD /tmp/rb3-crowdshot-8744.log | grep -a crowd_body | wc -l   # 6680
# A/B: RB3_NO_CROWD_REBIND=1 -> 121924 drops, only floating heads
```

Evidence:
- `/tmp/rp2-crowd/crowd_before_after_crowdg.png` — before/after composite (decisive)
- `/tmp/rp2-crowd/before_final/beforevis_coop_dir_crowdg_0.png` — BEFORE (heads only)
- `/tmp/rp2-crowd/after_final/finvis_coop_dir_crowdg_0.png` — AFTER (full crowd)
- `/tmp/rp2-crowd/after_final/finvis_coop_dir_crowdb_0.png` — band char coherent (no regression)
- logs `/tmp/rb3-crowdshot-{8741(before),8744(after)}.log`

Wii byte-identity: `WorldCrowd::Poll` objdiff fuzzy **100.0** (size 120==120,
diff_score 0); `Draw3DChars` 92.0840 / `DrawShowing` 96.3362 / `Enter` 98.9906 —
**identical to the main-repo report.json baseline** (those were already sub-100,
permuter-class, unrelated to my edits). Main-repo `Crowd.cpp` left pristine.

## Known residual (honest)

~6680 drops remain (vs 63339): brief flicker on 2 archetypes (male_crowd_body01,
female_crowd_body02) at peak arm-swing. Cause: the rest offset is baked at first
draw (idle clip already slightly advanced), so it drifts as the arms swing
(R·sin θ); setting `mNativeBonesRebound` to capture-once also disables the
engine's per-bone skin-clamp backstop on that mesh (the clamp gates on
`mesh->mNativeBonesRebound`, and for self-owned crowd meshes that is the same
flag). A clean kill needs an **engine change** to separate the rebake-skip from
the clamp-skip (e.g. a distinct `mNativeOffsetRebaked` flag), so the clamp stays
active on a rebaked mesh. Out of scope this wave (Fix A is rb3-only); noted as a
follow-up. The user-visible symptom (crowd merged into one spot / invisible
bodies) is fixed.

## LANDING NOTES (orchestrator)

- **rb3-only, one file** + one new script. Branch `wt-task-crowd`, commit
  `dcad5834`. No engine commit, no `MILO_ENGINE_PIN` bump.
- **Conflict surface:** `src/system/world/Crowd.cpp` — additive, fully
  `#ifdef HX_NATIVE`, no Wii lines touched. Low conflict risk. The sibling
  `char-render` task touches `BandCharacter.cpp` (band), NOT Crowd.cpp — disjoint.
  This task deliberately did NOT edit `BandCharacter.cpp` (copied semantics).
- Depends on `RndMesh::mNativeBonesRebound` (already exists in `rndobj/Mesh.h`)
  and the engine's existing SKEL_REBAKE/skin-clamp gating on it (engine pin
  `8fb669d`) — no new engine symbol required.
- If a future engine change adds a separate offset-rebake flag, revisit the
  residual (see above) to also keep the clamp active.

## Process note (for the orchestrator's awareness)

Early in the session I mistakenly edited the **main-repo** `Crowd.cpp` (passed the
main path to Edit instead of the worktree path). Caught it before any main-repo
commit, moved the changes to the worktree, and restored main's `Crowd.cpp` to
pristine via `git show HEAD:... > file` (single-file, only my own change — verified
`git diff` empty afterward). No concurrent agent's work was affected. The final
fix lives only in the worktree branch.
