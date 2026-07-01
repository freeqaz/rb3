# verify — pose-footwear-shard (independent adversarial review)

**Reviewer verdict: CONFIRM.** The band-aware V24 shard guard fix is correct, the
named footwear/glove residual is eliminated (band-garment drops → 0), the guard is
NOT neutered (genuinely-torn band meshes + crowd/extras/instrument/UI still drop),
no scene regression, and Wii byte-identity holds by construction (zero shared-src
change). All evidence below is the reviewer's OWN, captured fresh, not the
implementer's.

---

## What was reviewed

- Item: eliminate the last character residual — footwear/gloves/legwear guard-drop
  on a normal pose curl. Fix the GUARD, not the pose.
- Implementer worktree: `/home/free/code/milohax/rb3/.claude/worktrees/task-pose-footwear-shard`
  (branch `wt-task-pose-footwear-shard`, only commit `3c89d897` = pin bump).
- Engine worktree: `/home/free/code/milohax/milo-native-engine-worktrees/task-pose-footwear-shard`
  (branch `wt-task-pose-footwear-shard` @ `5962059`, one commit on pin `15ce606`).

## The change (verified by reading the diff myself)

Engine commit `5962059` touches ONLY `src/platform/Rnd_Wgpu_RB3.cpp`
(`+62/-3`), `BandRnd::DrawMesh`, the V24 `degenerate` predicate (was
`bool degenerate = (wext > 15.f) && (lext > 0.001f) && (wext > 2.0f*lext);`).
New behaviour:
- Scan the mesh's bones (`owner->BoneTransAt(b)->Dir()->mStoredFile`) for
  `"skeleton_unshared.milo"` (the wave-6 rebake's band-member detector, L4042) →
  `bandMember`. `owner` is in scope; identical `NumBones()`/`BoneTransAt()` pattern
  to existing code at L4875.
- For `bandMember` only: drop iff `wext>15 && lext>0.001 && (ratioBad || wext>worldCap)`
  where `ratioBad = wext>worldFloor && wext>ratioCap*lext`. Defaults
  ratioCap=4.0, worldCap=110u, worldFloor=40u, all env-tunable
  (`RB3_BAND_SHARD_{RATIOCAP,WORLDCAP,WORLDFLOOR}`).
- `!bandMember` (crowd/extras/instrument/UI): keeps `wext>2.0f*lext` VERBATIM.
- `[SHARD_RATIO]` log gains `band`/`other` classification.

rb3 side: only `native/CMakeLists.txt` pin `15ce606` → `5962059` (commit `3c89d897`).
The fix matches the plan, including the documented 3-cap deviation (the 40u world
floor for micro-bind submeshes — plan §2c materialised into a principled cap).

## I built it myself

```
cmake -B native/build-native -S native -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DDawn_DIR=.../dawn/lib/cmake/Dawn -DMILO_ENGINE_PATH="$(cat .engine-path)"
cmake --build native/build-native --target rb3-native     # exit 0
```
The fix's new strings are baked into MY binary (proves it linked):
`RB3_BAND_SHARD_{RATIOCAP,WORLDCAP,WORLDFLOOR}` present, and the updated
`[SHARD_RATIO] ... ratio=%.2f %s%s` format (band/other + DROP) present.

## Reviewer's own A/B (ports 9828-9831, evidence `/tmp/rp8rev-pose-footwear-shard/`)

Harness `keyboard-to-gameplay.py --diff hard`; engine stderr in `/tmp/rb3-kbd2game-<port>.log`.

### Symptom 1 — named garments render (band-garment drops → 0). PASS.

Fix ON (default), port 9828, `--game-burst 24`. Full per-mesh `SHARD_GUARD` drop
summary:
- `guitar_brain_strings.mesh` 3746 (instrument, world 128–147u > 110u cap → correctly dropped)
- `scrollbar_bg.mesh` 642 (UI, `other`, 2.0x path)
- `clap.mesh` 120, `male_extras_hair02` 90, `male_extras_eyebrows11` 90 (`other`, unchanged)
- **band footwear/gloves/legwear (kidgloves, lowtopsneaks, eightholedocs/talldocs,
  wrestlingboots, fingernails, parkajacket, loudleggings, …): 0 drops.**

`[SHARD_RATIO]` band-classified DROP lines excluding the instrument: **0**. The
verify-pose-fling.md named residual is gone.

### Symptom 2 — genuinely-broken meshes STILL drop (guard NOT neutered). PASS.

**(a) Crowd / extras / UI / instrument unchanged.** All classified `other` →
unchanged 2.0x path: `scrollbar_bg` 642, `clap` 120, `male_extras_hair02` 90,
`male_extras_eyebrows11` 90. Zero out-of-scope meshes wrongly classed `band`.

**(a′) In-class large-world backstop fires.** `guitar_brain_strings` is now
classified `band` (its bones thread the band skeleton) yet STILL drops 3746× —
world 128–147u exceeds the 110u world cap AND ratio 4.4–5.0 exceeds the 4.0x cap.
A real in-class case proving the world-cap backstop catches a band-classified mesh
that is genuinely large. Stronger than the plan anticipated.

**(b) In-class negative control — REAL band fling.** `RB3_NO_SKEL_WORLDFIX=1`,
port 9829: total `SHARD_GUARD` drops jump **4,688 → 163,660** (35×), proving the
guard is fully active. The exact garments the fix protects on a curl
(`lowtopsneaks_skin.2` 16,803, `lowtopsneaks_resource` 11,154, `wrestlingboots`
7,218, `nailboots` 688, plus `parkajacket` world 228–248u, `bikinichain` 205–224u)
ARE dropped when truly flung. Min world extent of any dropped band tear = **45.85u**
(above the 40u floor); **zero** band-class drops with world < 40u → no floor leak.
Same mesh class, same guard — the ONLY difference is pose sanity (curl vs fling),
and the guard keeps the curl while dropping the fling. This is the decisive
discrimination evidence.

**(c) No NaN/inf/assert/segv/abort** across all runs (0 hits, ports 9828+9829).

### Symptom 3 — threshold is a stable plateau, not a knife-edge. PASS.

`RB3_BAND_SHARD_RATIOCAP` sweep, band-garment drops (excl. instrument):

| ratioCap | band-garment drops |
|---|---|
| 3.0 | 8 (femaledestroyedchucks 7, miniskirt 1 — legit deep curls clipped) |
| **4.0 (default)** | **0** |
| 5.0 | 1 (single lowtopsneaks frame; run/roster variance) |

4.0 sits in a flat interior: lowering to 3.0 clips legit garments, raising to 5.0
gains nothing. The world-cap (110u) is independent of the ratiocap sweep, so the
large-world backstop is unaffected.

### Symptom 4 — no scene regression. PASS.

`song-end-test.py --require-endgame` (port 9828): full song → game-over →
`coop_endgame_screen` STABLE 25s / 2656 frames, no abort. Menu/song-select have no
band skinned meshes → the new branch is a no-op there.

### Visual. PASS.

`/tmp/rp8rev-pose-footwear-shard/burst_10.png`, `burst_17.png` (club venue
gameplay): band members on stage are coherent full-body figures with legs/feet/
hands present, no screen-crossing teal shards, clean note highway. Camera/roster
randomise per run, so per-mesh drop counts are the authoritative evidence (campaign
"measurement, not a hero closeup" principle); the visuals corroborate the absence
of shards.

## Wii byte-identity — independently re-verified. PASS (by construction).

- `git diff 1c46a70e..HEAD -- src/` (the implementer's contribution over their
  merge-base) is **EMPTY** → zero shared-src change.
- The src/ files that differ vs current master (`CharProvider.cpp`, `Object.cpp`,
  `MemMgr.cpp`) differ only because the worktree's merge-base is BEHIND current
  master — those files exist on master and were advanced by other agents, not
  edited here. The implementer's only commit (`3c89d897`) touches solely
  `native/CMakeLists.txt`.
- The single engine file is native-only (`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`,
  not in the Wii DOL, not DC3-compiled). No decomp build input changed → no `.o`
  rebuild / objdiff needed; Wii build is byte-identical. DC3-inert (band branch
  fires only on `skeleton_unshared.milo`, which DC3 does not load).

## Residuals / notes (non-blocking)

- `guitar_brain_strings` is now classified `band` (guitar bones thread the band
  skeleton) rather than the plan's predicted `dir='instrument'`. It still drops
  correctly via the world cap, so behaviour is right. Cosmetic only: the
  `band`/`other` SHARD_RATIO label is not a perfect proxy for "is an outfit
  garment" — a held-prop mesh whose bones touch the skeleton reads `band`. No
  functional impact (the world cap handles it); worth a one-line comment if anyone
  later relies on the label for analytics.
- The pink venue wash visible in some frames is the separate known venue-lighting
  residual, out of scope for this item.

## Landing readiness

Ready to land. Engine cherry-pick `5962059` (single self-contained commit on the
V24 guard region, ~L4912–4946), then set `MILO_ENGINE_PIN` in rb3
`native/CMakeLists.txt` to the final landed engine-main SHA (rb3 commit `3c89d897`
currently pins the wt SHA `5962059` as a placeholder — rewrite to the landed SHA).
Conflict risk with sibling engine tasks is LOW (disjoint region).
