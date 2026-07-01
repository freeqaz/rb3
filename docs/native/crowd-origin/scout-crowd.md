# Scout 1 — Crowd / Extras Placement (code reading)

Date: 2026-06-20. Scope: map how audience/crowd members and "extras" get
POSITIONED in a venue, from venue load to per-member `WorldXfm`. Find any native
(HX_NATIVE) seam that could land every member at identity/origin. Bug under
investigation: in gameplay the crowd congregates at one location (looks like
origin) AND multiple venue items — **including the drum kit** — are also stuck
there → likely a SHARED placement seam, not crowd-specific.

**TL;DR.** There are TWO INDEPENDENT placement paths that can both collapse to
origin, and the symptom ("crowd AND drum kit at origin") is almost certainly the
UNION of them, not one shared seam:

1. **Band members + their instruments (drum kit, guitars, mic)** are placed by
   `BandWardrobe::SyncTransProxies()` → `RndTransProxy::SetProxy()` →
   `RndTransProxy::Sync()` → `SetTransParent(bandChar, false)`. The drummer's
   drum kit is part of the drummer `BandCharacter` (an outfit/instrument piece),
   so it inherits the drummer's world xfm. If `mVenueNames` is empty or the
   venue's `player_<inst>0_*` `RndTransProxy` objects don't match a target, **no
   proxy is parented and the band character (and its kit) stays at its own
   origin (0,0,0).** This is exactly the failure the existing HX_NATIVE "V23"
   fix at `BandDirector.cpp:701-718` + `BandWardrobe.cpp:695-705` was written to
   prevent — and it is FRAGILE (depends on `LoadCharacters` running before
   `SetVenueDir`, and on the `mic`→`vocals` remap). This is my strongest suspect
   for the drum-kit-at-origin half.

2. **Crowd 3D characters** are placed from **authored data** — the per-member
   transforms are `RndMultiMesh::Instance.mXfm` values baked into the venue
   `.milo`, copied into `WorldCrowd::CharData::Char3D::unk0` and applied via
   `Character::SetWorldXfm` in `WorldCrowd::Draw3DChars` (Crowd.cpp:408). There
   is NO runtime scatter / RNG / instancing math for position — positions come
   straight off the multimesh instance list. A prior scout
   (`docs/native/render-polish-2026-06-11/scout-crowd.md`) **probe-verified that
   these positions are spread and sane** (`(±150, -30..-300, 3.6/4.5)`), and the
   crowd's visible "bunching" was a SKINNING artifact (shared-skeleton bone
   poison → V24 shard guard dropping bodies), NOT a placement collapse. That
   skinning bug has since been FIXED (`adb5240e`, inverse-bind rebake in
   `Crowd.cpp:865-1034`).

So if the crowd is genuinely at ORIGIN now (not merely bunched), the likely
causes — in priority order — are (a) the same band-proxy / venue-tree collapse
also starving the crowd's parent venue dir, (b) a regression in the multimesh
`Instance.mXfm` load (endianness / Xbox-compressed venue), or (c) a re-poison of
the shared skeleton that the rebake latch no longer catches. Sibling scouts
should A/B with the crowd rebake on/off and dump actual `unk0`/proxy worlds.

---

## 1. The two crowd-relevant classes and where positions live

### 1.1 `WorldCrowd` (`src/system/world/Crowd.{h,cpp}`) — the audience

- Per-archetype data: `WorldCrowd::CharData` holds `mDef` (the archetype
  `Character`, height, density, radius, random-color mats), `mMMesh`
  (`RndMultiMesh*` of placement instances), and two vectors of `Char3D`
  (`m3DChars`, `m3DCharsCreated`). `Char3D::unk0` is the **per-member world
  Transform** (Crowd.h:39-45).
- **Where positions come from**: `RndMultiMesh::Instance.mXfm`, loaded from the
  venue `.milo` BinStream (`WorldCrowd::Load` → `mCharacters` → each
  `CharData::mMMesh->mInstances`). `Set3DCharAll()` (Crowd.cpp:216-237) copies
  every `instIt->mXfm` into a `Char3D(instIt->mXfm, idx)` pushed to `m3DChars`.
  `Set3DCharList()` (Crowd.cpp:239-301) does the same for a subset.
- **Where the transform is SET on the member**: `WorldCrowd::Draw3DChars()`
  (Crowd.cpp:328-427). For each `m3DChars[i]`:
  - `spXfm.v` = `charIt->m3DChars[i].unk0.v` (the authored instance position),
    then `spXfm.v.z` adjusted by `-(mDef.mHeight*0.5 - z)` (Crowd.cpp:344-349).
  - Orientation `spXfm.m` from `mPlacementMesh->WorldXfm().m` (rotate=none,
    Crowd.cpp:393-396) or camera/focus-facing billboard math (Crowd.cpp:350-392).
  - `curChar->SetWorldXfm(spXfm)` (Crowd.cpp:408) — **absolute world**, NOT
    composed through any parent. So the venue dir's own xfm does not offset it;
    the authored `unk0` IS the world position.
- **`mPlacementMesh`** (Crowd.h:111) is the only object whose `WorldXfm()` the
  crowd reads, and only for the ROTATION basis (and the 2D-imposter cam math at
  Crowd.cpp:457-479). If `mPlacementMesh->WorldXfm()` were identity the crowd
  members would still be at their authored `unk0` POSITIONS, just mis-oriented —
  so a bad placement mesh xfm does NOT by itself put the crowd at origin.
- **2D imposter path** (Crowd.cpp:429-545, the non-`Draw3DChars` branch): for
  bowl/arena crowds (`force_3D_crowd=FALSE`). It sets `charXfm.v=(0,0,0)`
  deliberately (Crowd.cpp:524-526) and renders each archetype into an imposter
  RTT camera, NOT the main framebuffer — UNLESS the native RTT stubs are no-ops,
  in which case it splats every archetype at world origin into the MAIN
  framebuffer (prior scout §2.4). **In small_club_01 (the only venue native can
  load) every crowd is `force_3D_crowd=TRUE`**, so this path is inert there
  (prior scout §2.3). Relevant only if/when arena/festival venues load.

### 1.2 `RndMultiMesh` (`src/system/rndobj/MultiMesh.{h,cpp}`) — instancing

- Holds `mMesh` (the base mesh) + `std::list<Instance> mInstances`; each
  `Instance` is just a `Transform mXfm` (+ optional color in newer revs).
- `Instance::LoadRev` (MultiMesh.cpp:67-77) reads `bs >> mXfm` then color(s) by
  rev. **`mXfm` is a full Transform read via `operator>>(BinStream&, Transform&)`
  (Mtx.h:227) = `bs >> m >> v`.** If the venue is Xbox-compressed / wrong-endian
  on the native load and the Transform read is byte-swapped wrong, every `mXfm`
  could decode to garbage/identity → every instance at origin. (Not proven; flag
  for sibling endian scout.)
- `RndMultiMesh::DrawShowing()` (MultiMesh.cpp:176-213) iterates instances and
  `mMesh->SetWorldXfm(it->mXfm)` per instance — again ABSOLUTE world, parent
  xfm ignored. There is an HX_NATIVE block (MultiMesh.cpp:178-204) that adds
  `kFastBillboardXYZ` constraint handling (cam-basis + `it->mXfm.v`); it still
  uses the instance translation, so it does not introduce an origin collapse.

## 2. How the DRUM KIT (and the rest of the band) gets positioned

This is the half of the bug that is NOT crowd code. The drum kit is part of the
drummer `BandCharacter`; the band is placed into the venue by transform proxies.

Chain (all in `src/system/bandobj/BandWardrobe.cpp` + `rndobj/TransProxy.cpp`):

1. `BandWardrobe::SetVenueDir(dir)` (BandWardrobe.cpp:225) → `SetDir(dir)`.
2. `SetDir` (BandWardrobe.cpp:200-223) sets `mVenueDir`, then calls
   **`SyncTransProxies()`** (BandWardrobe.cpp:222).
3. `SyncTransProxies()` (BandWardrobe.cpp:326-340): iterates every
   `RndTransProxy` in the venue dir; for each, matches its name against
   `mCurNames->names[i]` (= `mVenueNames`, e.g. `player_drum0`,
   `player_guitar0`, `player_bass0`, `player_vocals0`); on match calls
   `it->SetProxy(mTargets[i])` (the matching `BandCharacter`).
4. `RndTransProxy::SetProxy(dir)` (TransProxy.cpp:14-19) → `Sync()`.
5. `RndTransProxy::Sync()` (TransProxy.cpp:28-45): `SetTransParent(bandChar,
   false)` — **this is what places the band character at the venue spot**: the
   proxy is a child of the venue authored at the player-spot location, and the
   band char becomes a child of the proxy (or the proxy's named `mPart`).
   `SetTransParent(0,0)` is the fallback when nothing matches → the char keeps
   its OWN (origin) world xfm.

**Failure mode → band + drum kit at origin:** if `mVenueNames` is empty (no
`player_*0` strings) OR the venue's proxies don't match, step 3 finds no match,
no `SetProxy` runs, `SetTransParent` is never called, and the band character
(drum kit included) renders at its construction origin (0,0,0).

`mVenueNames` is populated only in `BandWardrobe::LoadMainCharacters`
(`mVenueNames.names[i] = MakeString("player_%s0", inst)`, BandWardrobe.cpp:704),
which is reached from the venue load flow. Natively the venue load is DEFERRED,
so the existing HX_NATIVE "V23" fix calls `TheBandWardrobe->LoadCharacters(...)`
from `BandDirector::EnterVenue` BEFORE `SetVenueDir` (BandDirector.cpp:701-718)
specifically so `mVenueNames` is set and "the venue's `player_<inst>0_*.tp`
closeup-target proxies" don't "all collapse onto a shared stand-in dir." There
is ALSO a `mic`→`vocals` remap (BandWardrobe.cpp:695-705) because the
small_club `.milo` has `player_vocals0` refs and zero `player_mic0`.

These two native patches are the load-bearing seam for band placement. If
either regresses, or `LoadCharacters` returns with `mVenue.Name()` still null
(the `if (TheBandWardrobe && !mVenue.Name().Null())` guard at
BandDirector.cpp:712), `mVenueNames` stays empty → band + drum kit at origin.

## 3. Native (HX_NATIVE) seams in the placement path (inventory)

| Location | What it does | Origin-collapse risk |
|---|---|---|
| `BandDirector.cpp:701-718` | V23: call `LoadCharacters` pre-`SetVenueDir` so `mVenueNames`/instruments are set | **HIGH** — if guarded out (venue name null) → empty `mVenueNames` → band+kit at origin |
| `BandWardrobe.cpp:695-705` | `mic`→`vocals` remap before building `mVenueNames[i]` | MED — wrong-named vocal proxy never matches → singer at origin (one player, not the drummer) |
| `Crowd.cpp:865-1034` | crowd skeleton inverse-bind REBAKE (latched via `mNativeBonesRebound`) — landed `adb5240e` | NOT placement; fixes bone-poison "bunching", not origin. But if the latch mis-fires it can re-bunch. |
| `Crowd.cpp:409-415` | per-draw `RebindCrowdCharBonesToOwnSkeleton(curChar)` call (older comment block at :865 describes an earlier draw-time variant) | NOT placement |
| `MultiMesh.cpp:178-204` | `kFastBillboardXYZ` cam-facing for 2D bowl imposters | NOT origin — still uses `it->mXfm.v` |
| `MultiMesh.cpp:112-120` | STL iterator default-construct shim in `InvalidateProxies` | none (lifecycle) |
| `Mtx.h:293-298` | `TransformNoScale::operator=` return `*this` (clang) | none |
| `world/Dir.cpp:138-164` | force-poll vignette WorldDirs every frame | indirectly relevant — keeps the co-resident vignette crowd posing the shared skeleton (the bunching root cause); not origin |

No HX_NATIVE seam in `rndobj/Trans.cpp` (the transform tree
`WorldXfm_Force`/`SetTransParent`/constraint code) — that path is byte-identical
and intact, so the proxy-parenting mechanism itself is sound; the risk is the
DATA feeding it (empty `mVenueNames`, unmatched proxies, or bad authored xfm).

## 4. DC3 vs RB3 divergence in the crowd path

Both repos have the SAME `WorldCrowd` / `RndMultiMesh` design. DC3's
`Crowd.cpp` placement (`dc3-decomp/src/system/world/Crowd.cpp`) reads identical:
positions from `mPlacementMesh->WorldXfm()` basis + per-instance `unk0`
(DC3 Crowd.cpp:830-853, 1148, 1266), and `SetWorldXfm` on the char. No DC3 RNG
scatter either. DC3 has its OWN HX_NATIVE blocks at the same spots
(Crowd.cpp:27/39/160/363/668/1070) — worth a diff if a DC3 reference fix exists,
but the placement algorithm is shared, so no DC3-vs-RB3 divergence drives this
bug. (DC3 does NOT have the band `SyncTransProxies` path — that is RB3/band3
game code; DC3 places its dancers differently — so the band-proxy half is
RB3-specific and cannot be cross-checked against DC3.)

## 5. The "V24 shard-guard drops garments with crowd/extras + scrollbar trips"
note from the task is consistent with #2 above, NOT a separate cause

The V24 shard guard (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4346+`)
rejects a composed skin matrix whose translation/rotation is non-finite or
`>1e5`. When characters are at huge bone/world deltas (which happens both when
the shared skeleton is poisoned AND, plausibly, when an instance is at origin
while its skeleton/owner is elsewhere), the composed skin flings vertices and
the guard drops the mesh. So "objects at origin → huge deltas → guard drops"
is a SYMPTOM of the placement collapse, reinforcing that fixing placement
(restoring real `unk0` / proxy worlds) is the lever, not relaxing the guard.
The crowd/extras skin clamp + rebake (Crowd.cpp rebake + engine clamp at
:4373) are backstops for the skinning half, already landed.

## 6. Concrete suspects (ranked) for sibling scouts / impl

1. **`BandWardrobe::SyncTransProxies` finds zero matches** because `mVenueNames`
   is empty or the venue proxies are named differently than `player_<inst>0`.
   → drum kit + whole band at origin. Verify by dumping `mVenueNames[0..3]` and
   the count of `RndTransProxy` matched in `SyncTransProxies` (the V23 comment
   says "was 0"). Files: BandWardrobe.cpp:326-340, 704; BandDirector.cpp:701-718.
2. **`RndMultiMesh::Instance.mXfm` decodes to identity** on the native venue
   load (endian / Xbox-compressed `.milo`). → crowd at origin even though the
   draw code is correct. Verify by logging `it->mXfm.v` in `Set3DCharAll`
   (Crowd.cpp:227) for small_club_01. Files: MultiMesh.cpp:67-78; Mtx.h:227.
3. **Crowd skeleton re-poison after the rebake latch** (`mNativeBonesRebound`
   captured against a non-rest pose, or the co-resident vignette re-binds the
   owner). → bunching that reads as origin. A/B with `RB3_NO_CROWD_REBIND=1` and
   the engine `RB3_NO_SKIN_CLAMP=1`. Files: Crowd.cpp:865-1034.
4. **`mPlacementMesh` null / identity** would only mis-orient the crowd, not
   move it to origin — LOW priority, but check `DrawShowing` early-out
   (Crowd.cpp:431 `if (!mPlacementMesh) return;`) isn't silently dropping draws.

## 7. Verification harness (for impl)

```bash
# Build native (canonical -O0 Debug):
cmake -S native -B native/build-native -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON \
  -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build native/build-native --target rb3-native -j16
# Drive to gameplay + screenshot (RB3_HTTP=1 debug API):
python3 scripts/native/keyboard-to-gameplay.py --port 862X --diff hard \
  --out /tmp/crowd-origin --game-burst 24 --verbose
# Frame the crowd / band:
curl -X POST localhost:862X/api/dta/eval -d '{band_director force_shot "coop_dir_crowd.shot"}'
# Existing engine diagnostics already present: SHARD_DBG=1 SHARD_RATIO_DBG=1
# Prior scout's CROWD_DBG probes live in worktree .claude/worktrees/scout-crowd
```

Pass: drum kit renders at the venue drum spot (not 0,0,0); crowd members spread
across the floor (not clustered at origin); `SHARD_GUARD ... crowd_body` count
≈ 0.

## 8. Key file:line index

- `src/system/world/Crowd.h:39-45` — `Char3D::unk0` per-member Transform.
- `src/system/world/Crowd.cpp:216-237` — `Set3DCharAll` copies `Instance.mXfm` → `unk0`.
- `src/system/world/Crowd.cpp:239-301` — `Set3DCharList` (subset variant).
- `src/system/world/Crowd.cpp:328-427` — `Draw3DChars` sets per-member `SetWorldXfm`.
- `src/system/world/Crowd.cpp:408` — `curChar->SetWorldXfm(spXfm)` (the placement SET).
- `src/system/world/Crowd.cpp:865-1034` — HX_NATIVE crowd skeleton rebake (skinning, not placement).
- `src/system/rndobj/MultiMesh.cpp:67-78` — `Instance::LoadRev` (reads `mXfm` from BinStream).
- `src/system/rndobj/MultiMesh.cpp:176-213` — `DrawShowing` per-instance `SetWorldXfm`.
- `src/system/math/Mtx.h:227-230` — `operator>>(BinStream&, Transform&)`.
- `src/system/bandobj/BandWardrobe.cpp:326-340` — `SyncTransProxies` (BAND/DRUM placement).
- `src/system/bandobj/BandWardrobe.cpp:695-705` — `mVenueNames` build + `mic`→`vocals` remap.
- `src/system/bandobj/BandDirector.cpp:701-718` — V23: `LoadCharacters` pre-`SetVenueDir`.
- `src/system/rndobj/TransProxy.cpp:14-45` — `SetProxy`/`Sync`/`SetTransParent` (the parenting).
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4346+` — V24 shard guard.
