# TRACK A — skinning-rebind collection cache (finding #4) — LANDED

Implements finding #4 from `FRAME_STALL_FINDINGS.md` / `STEADY_STATE_FRAME_STALL_FINDINGS.md`:
the char-skinning rebind walk that re-ran an RTTI-heavy `dynamic_cast` + O(N²)
`std::find` collection **every Poll, for every band member**, for the ~10 s
rebind-latch window of song-start (the unambiguous #1 `__dynamic_cast` caller
chain, ≈650 ms of song-start in the original attribution).

## Root cause (confirmed)

`BandCharacter::NativeCollectSkinnedMeshes` (`src/system/bandobj/BandCharacter.cpp`,
all `#ifdef HX_NATIVE`) collects every skinned mesh a band member draws by:
1. iterating each dir's hashtable with `ObjDirItr<RndMesh>` — an **RTTI
   `dynamic_cast` per directory entry**;
2. recursively walking the whole draw tree, doing **`dynamic_cast<RndMesh*>` per
   drawable**;
3. de-duplicating with **linear `std::find`** against `targets` and `visited`
   (O(N²) in the mesh / drawable count).

It was called from BOTH Poll-time rebinds — `RebindHeadHandsAtRest()` (before
`Character::Poll`) and `RebindOutfitBonesToOwnSkeleton()` (after) — i.e. **twice
per member per Poll**, plus a third time from `NativeCaptureRestPoseAfterDeform()`
at SyncObjects. With 4 members, the full RTTI walk ran ~8×/frame for the entire
load-in window until the rebind latches flipped (the head latch's give-up is 600
Polls ≈ 10 s).

## Fix (native-only, match-neutral)

The **set of skinned meshes a member draws only changes when its dir tree is
re-stuffed** — i.e. at `StartLoad` / `SyncObjects`, which already re-arm the
rebind latches (`mNative*ReboundOnce = 0`). So:

- **`NativeRebuildSkinnedMeshCache()`** — the old walk, now writing into a
  per-member cache (`mNativeSkinnedMeshCache`), with the O(N²) `std::find` dedup
  replaced by **O(1) `std::set` membership** (`seenMesh` / `visited`).
- **`NativeCollectSkinnedMeshes()`** — now a thin front end: rebuild only when the
  cache is invalid (`mNativeSkinnedCacheValid`), otherwise append the cached
  vector to the caller's `out` (an O(1) copy). The per-mesh state the rebinds
  mutate (`SetBone`, `mNativeBonesRebound`) does not change the dir tree, so the
  cached **pointer** list stays valid across rebinds.
- **`NativeInvalidateSkinnedMeshCache()`** — called at the existing
  `StartLoad` and `SyncObjects` re-arm points (right where the latches reset),
  so a re-stuffed dir re-walks exactly once.

The RTTI walk now runs **once per (re)load** instead of ~8×/frame. The whole
collection of `dynamic_cast`s is paid once and cached.

### The 600-Poll give-up

Left at 600. With the cache, an un-latched member's per-Poll cost is now just the
O(1) cached-vector copy (not the RTTI walk), so the give-up is no longer a perf
problem. Lowering it would risk the late-streamed-mesh correctness the
`FRAME_STALL_FINDINGS` head-rebind latch comment warns about (LOD pieces stream in
a second+ after the torso), for zero remaining benefit — the cache already
neutralizes its cost.

### Files

- `src/system/bandobj/BandCharacter.cpp` — split collector into cached front-end +
  rebuild; set-based dedup; invalidate at StartLoad/SyncObjects; ctor init;
  `RB3_SKIN_TIMING` / `RB3_SKIN_NOCACHE` measurement knobs. All `#ifdef HX_NATIVE`.
- `src/system/bandobj/BandCharacter.h` — two method decls + two cache members
  (`std::vector<RndMesh*> mNativeSkinnedMeshCache`, `bool mNativeSkinnedCacheValid`),
  appended after the matched layout, inside `#ifdef HX_NATIVE`.

**Match-neutrality:** no Wii path touched; the Wii image is byte-identical.
Builds clean on native (clang) AND web (`scripts/web/build.sh --debug`).

## Measurement (native, JSPI-immune — the right tool for a CPU-compute stall)

Driver: `scripts/native/char-burst-capture.py` (sets `RB3_DTA_OVERLAY` so the band
actually loads + Polls in headless gameplay — the plain `frame_profiler.py`
`--into-song` nav reaches `game_screen` with **no band**, so it cannot measure
this). A/B is done **in one binary**: `RB3_SKIN_NOCACHE=1` forces the
pre-change behavior (rebuild every call); default is cached. `RB3_SKIN_TIMING=1`
sums rebuild vs cache-hit cost. Matched workloads (~3.1–3.6 s of song, ~6500
HEAD_REBIND member-polls each).

### Skinning portion specifically (`RB3_SKIN_TIMING`)

| | full RTTI walks | Σ walk time | cache-hit copies |
|---|---|---|---|
| **before** (`RB3_SKIN_NOCACHE=1`) | **12,650** | **3778 ms** | 0 |
| **after** (cached) | **50** | **17.8 ms** | 12,000+ |

The heavy walk count drops **253×** (12,650 → 50) and cumulative walk time drops
**−99.5%** (3778 ms → 17.8 ms) over the same load-in window. The 50 residual
rebuilds == the legitimate StartLoad/SyncObjects reload churn (~12–13 reloads × 4
members), each ~0.3 ms — exactly the intended invalidation events.

### Frame-level impact (`RB3_FRAME_TRACE`, game_screen frames)

| metric | before (NOCACHE) | after (cached) |
|---|---|---|
| game dt p50 | 14.3 ms | **12.2 ms** |
| game dt p95 | 22.9 ms | **18.6 ms** |
| game dt p99 | 29.3 ms | **24.4 ms** |
| worst frame | 116.0 ms | **95.6 ms** |
| frames >16 ms | 102 | **93** |
| **load-in burst** (first 150 game frames) mean | **18.1 ms** | **13.1 ms** |
| load-in burst Σ time | 2718 ms | **1966 ms** (−752 ms) |

≈ **5 ms/frame** removed during the band-load-in burst (−28%), **−752 ms** across
the burst — directly restoring the audio-producer headroom that this burst was
starving (finding #4's ~650 ms song-start estimate, confirmed). On web the same
shared engine code runs, so the same per-frame work is removed (the JSPI suspend
multiplier amplifies any main-thread CPU saved during the load burst).

## Correctness (no skin explosion)

The band still rebinds identically:
- cached & uncached both collect the same per-member target counts (16–21 meshes),
  reach the same final latch state (all 4 members `latched=1`, same residual
  `pending=1–5` = the known 1-permanently-unresolvable-mesh-per-member case), and
  the same `restPose` snapshot sizes (105–111).
- `reboundBones` totals match within run-to-run variance (738 cached vs 780
  nocache — timing of when late meshes become reachable, not a regression; both
  converge to the same latched end-state).
- Screenshots are visually identical: coherent guitarist/drummer/vocalist, no
  shards/fling, highway + gems render correctly (`/tmp/skin-cached3/shot_07.png`
  vs `/tmp/skin-nocache2/shot_07.png`).

## Reproduce

```bash
# after (cached) skinning-portion + frame trace
python3 scripts/native/char-burst-capture.py --shots 8 \
  --extra-env "RB3_SKIN_TIMING=1 HEAD_REBIND_PROBE=1 RB3_FRAME_TRACE=/tmp/ft-c.jsonl"
# before (uncached) — same binary, A/B knob
python3 scripts/native/char-burst-capture.py --shots 8 \
  --extra-env "RB3_SKIN_TIMING=1 HEAD_REBIND_PROBE=1 RB3_SKIN_NOCACHE=1 RB3_FRAME_TRACE=/tmp/ft-n.jsonl"
# grep SKIN_TIMING / HEAD_REBIND in /tmp/rb3-charburst-*.log; analyze dt from the jsonl
```

Worktree branch `wt-fstall2-skincache` (rb3). No engine change → no `MILO_ENGINE_PIN` bump.
