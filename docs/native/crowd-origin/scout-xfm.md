# Scout 3 — Transform propagation + native seams (crowd + drum kit at origin)

Read-only code audit, both repos. Goal: audit the transform hierarchy that turns
local placement into `WorldXfm` for venue objects, find every native seam that
could return identity / skip propagation, understand the V24 shard-guard, and
answer: **could ONE shared identity-returning lookup explain BOTH the crowd AND
the drum kit collapsing to the origin?**

Repos:
- rb3 decomp src: `/home/free/code/milohax/rb3/src`
- engine: `/home/free/code/milohax/milo-native-engine` (pin `884ab17` in
  `rb3/native/CMakeLists.txt:74`)

**Build fact (important for whoever fixes this):** the transform machinery the bug
runs through is compiled into `rb3-native` from rb3's OWN `src/` — `rndobj/Trans.cpp`,
`rndobj/Dir.cpp`, `rndobj/MultiMesh.cpp`, `world/*.cpp`, `bandobj/*.cpp`, `char/*.cpp`
(all `file(GLOB)`'d in `rb3/native/CMakeLists.txt:246-275`). The engine
(`milo-engine` target) does NOT compile these — it only supplies the renderer
(`src/platform/Rnd_Wgpu_RB3.cpp`). So fixes split: placement logic → rb3 `src/`
(HX_NATIVE-gated); draw-time world read / guard → engine.

---

## TL;DR — the answer to the central question

**No single lookup explains both — they are TWO distinct placement systems that
share one root assumption.** But there IS a clean shared root cause:

> On native, **objects whose transform parent fails to resolve fall back to their
> raw `mLocalXfm`, which for venue spawn/proxy/instrument nodes is the IDENTITY /
> near-origin.** The fallback path is `RndTransformable::WorldXfm()` →
> `WorldXfm_Force()` with `mParent == NULL` → `mWorldXfm = mLocalXfm`
> (`rb3/src/system/rndobj/Trans.cpp:130-131`). Anything that should have been
> re-parented onto a venue/band node, but wasn't, draws at the origin.

The drum kit and the crowd reach that same fallback through DIFFERENT parents:

- **Drum kit / band**: the BandCharacter root is DELIBERATELY at identity on
  native; its instrument geometry (`mInstDir` = the drum kit) is placed only by
  `RndTransProxy` / bone proxies. If those proxies don't resolve →
  `RndTransProxy::Sync()` calls `SetTransParent(0,0)` → identity → drum at origin.
- **Crowd**: 3D crowd placement comes from `WorldCrowd` multimesh-instance xfms,
  NOT band chars. Its "all at origin" failure mode is the **2D-imposter splat**
  (`Crowd.cpp` zeroes `charXfm.v` and draws into the main framebuffer when RTT
  is unavailable), OR the documented V24 shard-guard mass-drop (different
  symptom: bunched survivors, not a clean origin collapse).

So: the *mechanism* is shared (unparented → `mLocalXfm` identity), the *trigger
lookups are separate*. Fixing one lookup will not fix the other unless the real
regression is upstream of both (see "Single shared seam" below).

---

## 1. The transform machinery (rb3 `src/system/rndobj/Trans.{h,cpp}`)

`RndTransformable` is the base for everything placeable. No HX_NATIVE branches in
this file — it is the matched Wii decomp and is correct.

- Layout: `mParent` (0x8), `mChildren` (0x14), `mLocalXfm` (0x1c), `mWorldXfm`
  (0x4c), `mCache` (DirtyCache*, 0x7c), `mConstraint` (0x80).
- `WorldXfm()` (`Trans.h:104-109`): returns cached `mWorldXfm` if the dirty bit
  is clear, else recomputes via `WorldXfm_Force()`.
- **`WorldXfm_Force()` (`Trans.cpp:127-145`) — THE identity fallback:**
  ```cpp
  if (!mParent)                       mWorldXfm = mLocalXfm;            // <-- origin if local is identity
  else if (kParentWorld==mConstraint) mWorldXfm = mParent->WorldXfm();
  else if (mConstraint==kLocalRotate) Multiply(mLocalXfm.v, parent.World, ...);
  else                                Multiply(mLocalXfm, mParent->WorldXfm(), mWorldXfm);
  ```
  A node with NO parent renders at its `mLocalXfm`. For venue placement
  proxies / instrument dirs whose `mLocalXfm` is identity (their world placement
  was supposed to come from a parent), that means the **origin**.
- `SetTransParent` (`Trans.cpp:56-90`): re-parents + `SetDirty()`. Children are
  walked dirty top-down via `DirtyCache::SetDirty_Force` (`Trans.cpp:99-108`).
- `SetWorldXfm` (`Trans.cpp:110-117`): writes `mWorldXfm` raw, clears the dirty
  bit (so the cached value is returned verbatim), dirties children. Used by the
  multimesh per-instance draw and the crowd/impostor placement.

Propagation is correct: parent's `WorldXfm()` is composed into the child. The bug
is never "the math is wrong"; it is always "the parent is NULL / the wrong node".

## 2. Dir propagation (`rb3/src/system/rndobj/Dir.{h,cpp}`)

`RndDir : ObjectDir, RndAnimatable, RndDrawable, RndTransformable`. A dir IS a
transformable; children `SetTransParent` to it (`Dir.cpp:364` in `OldLoadProxies`;
`BandCharacter::SyncObjects` `BandCharacter.cpp:1365` `t->SetTransParent(this)`).
No per-frame top-down world recompute in `SyncObjects` — the lazy dirty/`WorldXfm()`
cache does it on read. No identity stubs here.

## 3. MultiMesh per-instance placement (`rb3/src/system/rndobj/MultiMesh.cpp`)

This is the CROWD-3D and venue-prop instancing path. **It is correct** — the
per-instance transform IS applied:

- `RndMultiMesh::DrawShowing()` (`MultiMesh.cpp:176-213`): for each instance,
  `mMesh->SetWorldXfm(it->mXfm); mMesh->DrawShowing();`. The native HX_NATIVE
  branch (`:178-205`) only adds `kFastBillboardXYZ` camera-facing (bakes
  `RndCam::sCurrent->WorldXfm().m` rotation + the instance's own translation
  `it->mXfm.v`) — translation preserved. Opt-out `RB3_BILLBOARD_OFF`.
- So a crowd/prop multimesh at origin means its **instance `mXfm` values are at
  origin**, not that the draw path dropped them.

## 4. Engine draw-time world read (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`)

`RndMesh::DrawShowing()` (`:5833`) → `BandRnd::DrawMesh(this)` (`:3353`). The
per-object model matrix:

```cpp
// Rnd_Wgpu_RB3.cpp:3997-4006
if (skinned && hubBarPlacement) { identity rot + mesh WorldXfm translation; }
else if (skinned)               { obj.world = IDENTITY; }   // bones carry world
else                            { MiloXfmToColMajor(mesh->WorldXfm(), obj.world); } // STATIC
```

- **Static meshes (the drum kit)**: `obj.world = mesh->WorldXfm()`
  (`:4005`). If that read returns identity → drum at origin. No native stub
  forces this to identity for static meshes; the origin must come from the rb3
  placement side (§5).
- **Skinned meshes (characters/crowd)**: `obj.world = IDENTITY`; placement comes
  entirely from the GPU bone palette `BoneOffsetAt(b) * owner->BoneTransAt(b)->WorldXfm()`
  (`:4036-4037`, palette build later). So a skinned char at origin means its
  BONE world xfms are at origin — i.e. its skeleton root was never placed.

No HX_NATIVE identity-returning branch in DrawMesh itself; the only identity is
the legitimate skinned-mesh `obj.world` (bones hold world). The seam is upstream.

## 5. The band / drum-kit placement seam (rb3 `src/system/bandobj`, `world/Dir.cpp`)

This is the most load-bearing finding. `rb3/src/system/world/Dir.cpp:422-462`
(WorldDir::DrawShowing, HX_NATIVE band-character draw bridge) documents the native
architecture verbatim:

> "the band characters are instanced ... into a SEPARATE dir
> (BandDirector::mChars / TheBandWardrobe->Dir()) and positioned into the venue
> only via the venue's `player*_base.tp` RndTransProxy nodes — which re-parent the
> character TRANSFORM ..."
>
> "**the band-member BandCharacter objects keep mShowing=false and an identity
> WorldXfm** — their visible geometry lives in mOutfitDir / mInstDir, positioned
> by the bone proxies." (`Dir.cpp:452-454`)

So the BandCharacter root is at identity BY DESIGN. The instrument (`mInstDir` =
the drum kit, set at `BandCharacter.cpp:149` when a sub-dir named `"instrument"`
is added) and the outfit are placed entirely by proxies. The chain:

1. `BandWardrobe::SetVenueDir` → `SyncTransProxies()` (`BandWardrobe.cpp:326-340`):
   iterates every `RndTransProxy` in the venue dir; if its name contains a
   `mVenueNames[i]` (`player_guitar0`, `player_bass0`, `player_mic0`/`vocals0`,
   `player_drum0`), calls `it->SetProxy(mTargets[i])` — binds the proxy to the
   band character.
2. `RndTransProxy::Sync()` (`rb3/src/system/rndobj/TransProxy.cpp:28-45`) — **the
   identity-fallback seam:**
   ```cpp
   SetTransParent(0, false);
   if (mProxy && mPart.Null())   { trans=mProxy; SetTransParent(trans); return; }
   if (mProxy)                   { trans=mProxy->Find<RndTransformable>(mPart); if(trans){SetTransParent(trans);return;} }
   SetTransParent(0, 0);   // <-- UNRESOLVED -> no parent -> WorldXfm == mLocalXfm == identity/origin
   ```
   If `mProxy` is null (proxy never bound) OR `Find(mPart)` (the named bone on the
   target) fails → `SetTransParent(0,0)` → that proxy (and everything parented
   under it, incl. the drum geometry) collapses to its local origin.
3. The known native failure that triggers exactly this is documented in
   `BandDirector.cpp:701-718` (EnterVenue HX_NATIVE block): if `LoadCharacters`
   hasn't run, `mVenueNames` / `mInstrumentType` stay unset →
   **"the venue's `player_<inst>0_*.tp` closeup-target proxies all collapse onto a
   shared stand-in dir."** This is the origin/merge symptom for the band side.

So the drum kit at origin = its instrument proxy / bone proxy resolved to NULL →
`SetTransParent(0,0)` → identity. The trigger is `mVenueNames`/`mTargets`/instrument
assignment not being wired at proxy-sync time on native.

## 6. The crowd placement seam (rb3 `src/system/world/Crowd.cpp`)

The crowd is a SEPARATE system from band chars. Two native failure modes both end
at the origin:

- **3D crowd (`Draw3DChars`, `Crowd.cpp:328-427`)**: per-instance placement is
  `spXfm` whose translation = `charIt->m3DChars[i].unk0.v` (from the multimesh
  instance `mXfm`, `Crowd.cpp:284 push_back(CharData::Char3D(instIt->mXfm,...))`)
  and rotation = `mPlacementMesh->WorldXfm().m`. `curChar->SetWorldXfm(spXfm)`
  (`:408`). If `m3DChars[].unk0` is identity (instances never spread) OR
  `mPlacementMesh->WorldXfm()` is identity (placement mesh itself at origin), the
  whole crowd lands at origin. HX_NATIVE adds `RebindCrowdCharBonesToOwnSkeleton`
  (`:409-414`, defined `:865-1029`) — a skeleton/offset rebind, not a placement
  change.
- **2D imposter (`WorldCrowd::DrawShowing`, `Crowd.cpp:429-560`)**: the impostor
  render EXPLICITLY zeroes the character translation —
  `charXfm.v.x/y/z = 0; curChar->SetWorldXfm(charXfm)` (`:524-527`) — because the
  char is meant to be rendered centered into an off-screen render-target texture
  (`gImpostorCamera`). Prior scout (`render-polish-2026-06-11/scout-crowd.md` §2.4)
  proved that on native the RTT plumbing is stubbed (`WiiRnd::GetSharedTex`,
  `PrepareRenderAlley` = no-op stubs in `native/src/band3_link_stubs.s`), so this
  path draws every archetype **into the MAIN framebuffer at world origin** — a
  literal "all crowd merged at one location" splat. This is the cleanest match for
  the "crowd at origin" symptom, but only fires for non-`force_3D_crowd` venues
  (small_club is force-3D, so the imposter path is inert there per that scout's §2.3).

The prior render-polish scout's §2.3 explicitly verified, for small_club, "No
multimesh transform collapse" and "Draw3DChars roots are spread and sane." So if
the CURRENT bug shows a clean crowd-at-origin in small_club, it is either a
regression since that scout, OR a different venue exercising the 2D imposter
splat. Whoever fixes this should confirm WHICH venue + WHICH crowd path first.

## 7. The V24 shard-guard (engine `Rnd_Wgpu_RB3.cpp:4907-5143`)

What it is: a **skinned-mesh-only** drop. For each skinned mesh it computes the
bind-pose (local) AABB extent `lext` and the EXACT-4-bone-blended (world) AABB
extent `wext` (`:4946-4976`), then drops the mesh if
`wext > 15 && lext > 0.001 && wext > 2.0*lext` (band-member garments get relaxed
caps 4.0×ratio / 110u world / 40u floor, `:5083-5109`). Opt-out `SHARD_GUARD_OFF`;
debug `SHARD_RATIO_DBG` / `SHARD_DBG`.

**Does "object at origin" trip the guard? — Conditionally YES, and only for the
crowd, not the drum kit:**

- The guard measures the BLENDED-WORLD extent vs BIND extent. If a skinned mesh's
  bones split between "placed" and "at origin" (e.g. some bones resolved to the
  real skeleton, some collapsed to identity), the blended verts span the full
  distance from the venue to the origin → `wext` huge → ratio ≫ 2.0 → **dropped**.
  This is exactly the cross-instance 2000u-span case the prior scout proved
  (`scout-crowd.md` §2.2: bindExt≈80 → worldExt≈2240, ratio≈25×, dropped ~20k/song).
  So "some bones at origin" → guard drops → bunched survivors (heads/props) that
  LOOK like "crowd merged at one spot."
- The DRUM KIT is a **static** mesh (or a low-bone instrument). The guard does
  `if (skinned && ...)` (`:4924`) — **it never runs on static meshes.** A static
  drum kit at origin is the engine faithfully drawing `mesh->WorldXfm()==identity`
  (§4), NOT a guard drop.

So the guard is a downstream AMPLIFIER of the origin bug for skinned crowd meshes
(turns "half at origin" into "dropped → bunched"), but it is NOT the cause and it
cannot touch the drum kit. The guard's own comments (`:4907-4923`, `:5051-5071`)
confirm it treats the origin/2000u span as a degenerate pose to refuse — correct
behavior given a broken upstream placement.

## 8. Single shared seam? — where to look if it's ONE root cause

The drum-kit (static, band/proxy-placed) and crowd (skinned, multimesh-placed)
paths only converge if the regression is UPSTREAM of both placement systems.
Candidates, ranked:

1. **The venue WorldInstance/WorldDir root WorldXfm itself is identity** and the
   real venue geometry happens to be authored near origin, so EVERYTHING
   (drum, crowd placement mesh, props) inherits origin. Check `WorldInstance::SyncDir`
   (`rb3/src/system/world/Instance.cpp:304-394`) — it reparents the proxy's copied
   objects via `ObjRef::Replace` (`:358-370`) and relies on `WorldXfm()` (`:316`).
   This is matched Wii code with NO HX_NATIVE, BUT a prior fix here was a
   transposed `ObjPair` (MEMORY: venue-env `WorldInstance::SyncDir`, rb3 `d988a301`)
   — same function, so it is a proven fragile seam. If a co-resident world or a
   `Find` in the reparent loop resolves wrong on native, both the crowd placement
   mesh AND venue props (drum) reparent to a stand-in at origin. **Highest-value
   single-seam suspect.**
2. **`RndCam::sCurrent` shared dependency.** Both `Crowd.cpp` dynamic-constraint
   placement (`:356,368,452,485`) and `Trans.cpp::ApplyDynamicConstraint`
   (`Trans.cpp:161-198`, billboards/look-at) read `RndCam::sCurrent->WorldXfm()`.
   If the wrong cam (or a cam at origin) is current during the crowd/venue draw,
   billboard + look-at constrained objects mis-place. Less likely to put a static
   drum at a clean origin, but worth ruling out for the crowd.
3. **The proxy-target wiring (`mVenueNames`/`mTargets`) being unset** (§5). This
   cleanly explains the drum + band collapse, and IF the venue's crowd placement
   meshes are ALSO `RndTransProxy`-parented to player/venue nodes (verify by
   dumping the venue milo), it would explain the crowd too via the SAME
   `RndTransProxy::Sync()` → `SetTransParent(0,0)` seam. **This is the single
   lookup most likely to hit both** — confirm whether crowd placement meshes are
   proxies.

## 9. Verified negatives (don't re-investigate)

- `rndobj/Trans.cpp`, `Dir.cpp`, `MultiMesh.cpp` transform math: correct, no
  identity stubs (MultiMesh native branch preserves instance translation).
- Engine `DrawMesh`: no identity stub for STATIC meshes; skinned `obj.world=I`
  is correct (bones hold world).
- V24 shard-guard: not the cause; skinned-only, cannot affect the static drum;
  it amplifies the crowd symptom downstream.

## 10. Recommended first probes for the fix agent

1. Confirm which crowd path is live: run with `SHARD_RATIO_DBG=1 SHARD_DBG=1`
   (engine, already present) — if crowd bodies show ratio≈25× DROP, it's the
   skeleton-mix / shard-guard amplifier; if zero shard hits and crowd is still
   a clean origin splat, it's the 2D-imposter path (§6) or a placement-mesh
   collapse.
2. Probe `RndTransProxy::Sync()` (`TransProxy.cpp:44`) — add an HX_NATIVE
   env-gated `fprintf` counting how many proxies hit the final `SetTransParent(0,0)`
   (unresolved) vs resolved. A high unresolved count in the venue dir = the drum
   + band origin seam confirmed.
3. Probe `BandCharacter::mInstDir` world position at draw (`BandCharacter::DrawShowing`,
   `BandCharacter.cpp:1660`) and the band char root `WorldXfm()` — confirm root is
   identity (expected, by design) and whether `mInstDir`'s instrument bones resolve
   to a placed venue node or to origin.
4. Dump the loaded venue milo's `RndTransProxy` names + whether crowd placement
   meshes are proxy-parented — this answers the single-seam question definitively.
