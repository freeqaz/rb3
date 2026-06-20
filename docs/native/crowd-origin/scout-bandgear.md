# Scout 2 — Band-gear / venue-prop placement (code reading)

Date: 2026-06-20. Scope: map how band gear (drum kit, mic stands, amps) and
static venue props get POSITIONED, and find the node COMMON to the drum kit and
the crowd so the "everything at origin" symptom can be attributed to one seam.
Complements `scout-crowd.md` (Scout 1, crowd path) and `scout-repro-tooling.md`
(Scout 4, live repro: confirms a JUMBLED PILE of character+instrument meshes
clumped at one spot on the LEFT of the gameplay frame, highway center correct).

**TL;DR — the drum kit is NOT an independent venue prop. It is the drummer
`BandCharacter`'s instrument geometry, bound to the character's `bone_prop*` /
`bone_mic_stand_bottom` bones.** So "drum kit at origin" == "drummer band
character at origin," and the same is true for guitars, mics, amps that hang off
band members. The shared node with the crowd is therefore NOT a single venue
transform; it is the fact that BOTH placement systems land their objects in WORLD
space via an *absolute* `SetWorldXfm`, fed by data that has gone to zero/identity:

- **Band gear** rides the band character's transform chain. The character root
  (`playerN` of `skeleton_unshared.milo`) is positioned by the venue's
  `player_<inst>0_*` proxy/spot wiring. If `BandWardrobe::SyncTransProxies` finds
  no match (empty/wrong `mVenueNames`) the character keeps its construction-origin
  WorldXfm → the whole member + its kit render at (0,0,0). This is the SAME
  fragile native seam Scout 1 flagged; I confirm it from the gear side and add the
  bone-attachment + merge-filter detail below.
- **Crowd** lands per-member via `Character::SetWorldXfm(spXfm)` with `spXfm.v`
  straight off authored `m3DChars[i].unk0` (no parent compose). At origin only if
  that authored data decoded to zero (endian / multimesh-instance load).

The single most likely UNIFIED explanation for "band gear AND crowd at origin
TOGETHER" is **(2) the venue `.milo` instance/transform data decoding to
zero/identity on the native load path** — because that one cause hits the band
proxy-spot transforms AND the crowd multimesh instances at once. If only the band
pile is at origin (Scout 4's screenshot shows the pile on the LEFT; need to
confirm whether the crowd is *also* origin vs merely dark/unlit), the band-proxy
half (1) alone explains it. Rank both; A/B is in §6.

---

## 1. The drum kit's transform chain (where band gear is anchored)

The drum kit / guitars / mic / amps are instrument geometry merged INTO the band
`BandCharacter`, not standalone venue dirs. Chain, leaf → root:

1. **Instrument geometry → character bones.** Instrument pieces live in
   `mInstDir` (the `instrument` sub-Character of a `BandCharacter`,
   `BandCharacter.cpp:148-149`). They are MERGED in through the character merge
   filter `BandCharacter::FilterMerge` (the `o1->Dir() == sInstrumentDir ||
   o1->Dir() == sInstResourceDir` branch, `BandCharacter.cpp:2738-2748`): an
   instrument `RndTransformable` with a `TransParent` has its `SetLocalXfm` copied
   from the source and its refs replaced, so it ATTACHES to the matching
   character bone rather than carrying its own world placement.
2. **Prop / mic-stand bones → character.** `BandCharacter::SyncObjects`
   (`BandCharacter.cpp:1347-1366`) walks a fixed bone list — `bone_pelvis.mesh`,
   `bone_prop0..3.mesh`, `spot_neck.mesh`, `spot_navel.mesh`,
   **`bone_mic_stand_bottom.mesh`** — `Find`s each and does
   `t->SetTransParent(this, false)`. So the mic-stand/prop carrier bones become
   children of the character `this`. (Note the HX_NATIVE OOB fix here at
   `:1353-1359` — the bone array has no null sentinel; the matched loop reads
   `bones[8]` OOB. Bounded to `bones+8` on native. If that bound were wrong it
   would crash, not silently mis-place — so it is not the origin seam, but it IS a
   native edit in the exact placement function.)
3. **Character bones → character root (`playerN`).** Per-member band skeletons
   are rooted at `playerN` of `char/char/main/skeleton_unshared.milo`
   (confirmed by the engine shard-guard's band detector,
   `Rnd_Wgpu_RB3.cpp:5063-5080`, and the rest-rebake walk
   `BandCharacter.cpp:785-799` which finds the chain ROOT via repeated
   `TransParent()`).
4. **Character root → venue stage spot.** This is the placement that moves the
   whole member (+ kit). It is wired by the band-proxy path in §2.

**Therefore: drum kit world pos = `bone_prop*` local × character bone chain ×
`playerN` root WorldXfm × (venue stage spot).** If the venue spot wiring fails,
`playerN` stays at its own origin and EVERY instrument hanging off the drummer
(the whole kit) renders at (0,0,0). The engine reads it faithfully: a STATIC
instrument mesh draws with `obj.world = MiloXfmToColMajor(mesh->WorldXfm())`
(`Rnd_Wgpu_RB3.cpp:4004-4005`); a SKINNED instrument mesh uses the bone palette
`BoneOffsetAt(i) * boneTrans->WorldXfm()` (`:4034-4040, 4270-4279`). Both resolve
to origin iff the bone/mesh WorldXfm is origin. No engine seam forces it to zero;
the value it reads is already zero.

## 2. How the band character ROOT is positioned in the venue (the spot wiring)

Three native-specific pieces must all line up (all flagged by Scout 1; I trace
the parenting direction precisely here because it is easy to misread):

- `BandDirector::EnterVenue` (`BandDirector.cpp:614-755`) force-loads the venue
  (native bridge, `:631-688`), then calls `TheBandWardrobe->LoadCharacters(...)`
  BEFORE `SetVenueDir` (`:712-717`) so `mVenueNames` is populated.
- `BandWardrobe::LoadMainCharacters` builds `mVenueNames.names[i] =
  "player_<inst>0"` (`BandWardrobe.cpp:704`), with the HX_NATIVE `mic`→`vocals`
  remap (`:695-702`) because the small_club `.milo` names vocal proxies
  `player_vocals0_*` and has ZERO `player_mic0`.
- `BandWardrobe::SetVenueDir` → `SetDir` → **`SyncTransProxies`**
  (`BandWardrobe.cpp:225, 222, 326-340`): iterate every `RndTransProxy` in the
  venue dir; `strstr(proxy->Name(), mVenueNames.names[i])` match → `it->SetProxy(
  mTargets[i])`.

### 2.1 IMPORTANT — the proxy parenting direction (do not misread)

`RndTransProxy::Sync()` (`TransProxy.cpp:28-45`) on `SetProxy(bandChar)` does
`SetTransParent(bandChar, …)` — i.e. **the PROXY becomes a child of the band
character**, not the other way round. So `player_<inst>0_*.tp` proxies are
camera/closeup TARGETS that FOLLOW the member; they do not themselves drag the
member onto the stage. That means the member-root placement comes from a separate
mechanism (the venue's `player_<inst>0_base` spot / the character being
parented/instanced under the venue stage transform), and `SyncTransProxies`
mainly fixes WHICH dir the closeup proxies resolve to. The WorldDir bridge
comment "player*_base.tp RndTransProxy nodes which re-parent the character
TRANSFORM" (`world/Dir.cpp:431`) is loosely worded; the `.tp` `Sync()` re-parents
the PROXY. **OPEN QUESTION for impl:** confirm exactly what sets the
`BandCharacter` (or its `playerN` root) WorldXfm/TransParent to the stage spot on
native — I did not find a `mTargets[i]->SetTransParent(venueSpot)` or
`SetWorldXfm` in band3/bandobj. Candidates: (a) the character is instanced under
a venue `player_<inst>0_base` dir whose own LocalXfm is the stage spot (so the
member inherits it through the dir tree), or (b) a closeup `BandCamShot`/dircut
drives it. If (a), the seam is the venue dir-tree parenting failing on native
(member never re-parented under the stage spot dir → stays at origin) — this is a
DIFFERENT failure than empty `mVenueNames` and must be probed separately.

## 3. Static venue props (amps, dressing) — the control case

Pure static venue props (the dartboard Scout 4 saw rendering correctly on the
RIGHT; amps; stage dressing) are RndDrawables in the venue WorldDir, placed by
their own saved `mLocalXfm` (+ TransParent) loaded in
`RndTransformable::Load` (`Trans.cpp` `BEGIN_LOADS(RndTransformable)`: reads
`mLocalXfm`, `mWorldXfm`, then the trans-parent at `gRev>8`). Their WorldXfm =
`mLocalXfm × parent` via `WorldXfm_Force` (`Trans.cpp:127-145`).

**This is the discriminator.** Scout 4 reports the dartboard (a static venue prop)
renders in the RIGHT place while the band/instrument pile is collapsed. THAT
RULES OUT a venue-wide WorldDir transform collapse (if the venue root were at
origin, the dartboard would be too). It localizes the bug to whatever is specific
to the BAND CHARACTERS (and, if the crowd is also collapsed, the CROWD multimesh
instances) — NOT the generic venue prop transform path. The static-prop path is
the working control: any fix must leave it intact.

## 4. The COMMON node between drum kit and crowd

| | Drum kit (band gear) | Crowd member |
|---|---|---|
| Placement SET call | inherits `playerN` root via bone chain | `Character::SetWorldXfm(spXfm)` (`Crowd.cpp:408`) |
| Position source | venue `player_<inst>0` spot wiring | authored `m3DChars[i].unk0.v` (= multimesh `Instance.mXfm`) |
| Parent compose? | YES (bone chain → root) | NO — absolute world |
| Static venue dir xfm involved? | only via the stage-spot dir (if mechanism (a)) | only `mPlacementMesh->WorldXfm().m` for ORIENTATION |

The literal common code node is small: both ultimately call
`RndTransformable::SetWorldXfm` / `WorldXfm_Force` with origin inputs. There is NO
single shared runtime object whose collapse moves both. So a TRUE single root
cause must be at the **DATA** layer they share: the venue `.milo` payload. Both
the band stage-spot transforms and the crowd multimesh `Instance.mXfm` are read
from the SAME venue file by the SAME `operator>>(BinStream&, Transform&)`
(`math/Mtx.h:227` = `bs >> m >> v`). If that read mis-decodes on the native load
(endian / Xbox-compressed venue / wrong rev), every transform sourced from the
venue file zeros/identities — band spots AND crowd instances at once. That is the
one hypothesis that explains BOTH with a single cause, and it leaves pure static
props alone only if their transforms come through a different (correctly-decoded)
path — which they may NOT, so this hypothesis predicts the dartboard would ALSO
be affected. Since Scout 4 says the dartboard is fine, the endian/decode
hypothesis is WEAKENED, and the more likely reality is TWO coincident causes
(band-spot wiring + crowd skinning/instance), exactly as Scout 1 concluded.

## 5. Native (HX_NATIVE) seams in the band-gear placement path (inventory)

| Location | What it does | Origin-collapse risk |
|---|---|---|
| `BandCharacter.cpp:1353-1359` | bound the prop/mic-stand bone-parenting loop to `bones+8` (OOB sentinel fix) | LOW — would crash, not mis-place, if wrong; but it IS in the exact placement fn |
| `BandCharacter.cpp:2738-2748` | instrument merge: copy `SetLocalXfm` + ReplaceRefs so kit attaches to char bones | MED — if the source `TransParent()` check fails, kit keeps source LocalXfm (could be off, not necessarily origin) |
| `BandDirector.cpp:631-688` | native venue force-load bridge (`LoadVenue` sync) | HIGH (upstream) — if venue load fails / wrong file, no spots exist |
| `BandDirector.cpp:712-717` | call `LoadCharacters` pre-`SetVenueDir` so `mVenueNames` set | HIGH — guarded on `!mVenue.Name().Null()`; if venue name still null → empty `mVenueNames` → no proxy match |
| `BandWardrobe.cpp:695-702` | `mic`→`vocals` remap | MED — only affects the singer, not the drummer |
| `world/Dir.cpp:422-462` | band-character DRAW bridge (DrawShowing the 4 members on wide shots) | NONE for placement — it DRAWS, doesn't position; but it WILL draw a member that is at origin (makes the bug visible) |

`rndobj/Trans.cpp`, `rndobj/TransProxy.cpp`, `rndobj/MultiMesh.cpp` transform
math is byte-identical (the MultiMesh native block only adds billboard handling
that still uses `it->mXfm.v`). So the parenting/compose mechanism is sound; the
risk is the DATA feeding it (empty `mVenueNames`, the stage-spot dir not parenting
the member, or venue-file transform decode).

## 6. Concrete suspects (ranked) + how to prove each

1. **Band character root never re-parented to the venue stage spot on native.**
   `SyncTransProxies` only re-parents the closeup `.tp` PROXIES to the member
   (TransProxy.cpp:28-44 parents proxy→char). I could NOT find the code that sets
   the `BandCharacter`/`playerN` root WorldXfm/TransParent to the
   `player_<inst>0_base` stage spot. If that wiring is data-driven (member
   instanced UNDER a venue base dir) and the native deferred-venue flow skips it,
   the whole member + kit stays at construction origin.
   **PROVE:** in `world/Dir.cpp` band-draw bridge (`:450-460`), before
   `bandChar->DrawShowing()`, log `bandChar->WorldXfm().v` and
   `bandChar->TransParent()` (name). Also log the root via the §1 chain walk. If
   `v≈(0,0,0)` and parent is null/non-venue → confirmed.
   Files: BandWardrobe.cpp:225-243/326-340; BandDirector.cpp:695-755;
   BandCharacter.cpp:785-799.

2. **`mVenueNames` empty / proxies unmatched** (Scout 1's #1). `SyncTransProxies`
   match count == 0. The V23 comment notes it "was 0" before the fix.
   **PROVE:** log `mVenueNames[0..3]` at end of `LoadMainCharacters:704` and the
   matched-proxy count in `SyncTransProxies:333`.
   Files: BandWardrobe.cpp:326-340, 690-705; BandDirector.cpp:712-717.

3. **Crowd multimesh `Instance.mXfm` decodes to identity** (Scout 1's #2). Only
   matters if the crowd is genuinely at origin (vs dark). One cause that would ALSO
   hit band stage-spot transforms (the unified hypothesis) — but WEAKENED because
   the static dartboard decodes fine.
   **PROVE:** log `it->mXfm.v` in `WorldCrowd::Set3DCharAll` (`Crowd.cpp:227`) for
   small_club_01; log a few static venue-prop `WorldXfm().v` for comparison.
   Files: MultiMesh.cpp:67-78; Mtx.h:227; Crowd.cpp:216-237.

4. **Instrument merge LocalXfm wrong** (`BandCharacter.cpp:2738-2748`). If the kit
   attaches but with a bad LocalXfm it would be mis-placed RELATIVE to the
   drummer, not at world origin — distinguishes from #1 (whole member at origin).
   **PROVE:** if the drummer body is at the stage spot but the kit floats away,
   it's this; if body AND kit are both at origin, it's #1/#2.

## 7. Verification harness

```bash
# Build native (canonical -O0 Debug):
cmake -S native -B native/build-native -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON \
  -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build native/build-native --target rb3-native -j16

# Drive to gameplay + screenshot (see scout-repro-tooling.md for the working nav):
RB3_HTTP=1 SHARD_DBG=1 VENUE_DBG=1 CHAR_DBG=1 ./native/build-native/rb3-native ...
# SHARD_DBG already prints dropped band meshes WITH bone0 world pos + owning dir
#   "[SHARD_GUARD] dropped ... dir='playerN...' bone0=(x,y,z)" (Rnd_Wgpu_RB3.cpp:5134)
#   → bone0=(0,0,0) on a player_*0 dir == drum-kit/band-at-origin CONFIRMED.

# Live engine state:
curl -s localhost:PORT/api/dta/eval -d '{band_director force_shot "coop_dir_drum.shot"}'
```

Discriminators:
- Static venue prop (dartboard) at correct pos + band pile at origin → band-spot
  wiring (#1/#2), NOT a venue-wide collapse, NOT pure endian.
- Drummer body at stage spot but kit detached at origin → instrument merge (#4).
- Crowd ALSO genuinely at origin (not just dark) → add #3.

## 8. Key file:line index (band-gear / venue-prop side)

- `src/system/bandobj/BandCharacter.cpp:148-149` — `mInstDir` (instrument sub-char).
- `src/system/bandobj/BandCharacter.cpp:1347-1366` — `SyncObjects` parents prop/mic-stand bones to the character (`SetTransParent(this)`); HX_NATIVE OOB bound at :1353-1359.
- `src/system/bandobj/BandCharacter.cpp:2738-2748` — instrument merge: `SetLocalXfm` + ReplaceRefs (kit → char bones).
- `src/system/bandobj/BandCharacter.cpp:785-799` — `NativeCharSpaceRestXfm` walks TransParent chain to the per-member ROOT (`playerN`).
- `src/system/bandobj/BandWardrobe.cpp:225-243` — `SetVenueDir`→`SetDir`→`SyncTransProxies`.
- `src/system/bandobj/BandWardrobe.cpp:326-340` — `SyncTransProxies` (name-match → `SetProxy`).
- `src/system/bandobj/BandWardrobe.cpp:690-705` — `mVenueNames` build + HX_NATIVE `mic`→`vocals` remap.
- `src/system/bandobj/BandDirector.cpp:614-755` — `EnterVenue` (native force-load + LoadCharacters-before-SetVenueDir).
- `src/system/rndobj/TransProxy.cpp:28-45` — `Sync` parents the PROXY to its `mProxy` (closeup target follows char).
- `src/system/rndobj/Trans.cpp:127-145` — `WorldXfm_Force` (`mLocalXfm × parent`) — the compose used by every prop.
- `src/system/rndobj/Dir.cpp:336-369` — `RndDir::OldLoadProxies`: `SetLocalXfm(t58)` + `SetTransParent(Find(s80))` (per-subdir placement from saved data).
- `src/system/math/Mtx.h:227` — `operator>>(BinStream&, Transform&)` (the shared transform decode for venue spots AND crowd instances).
- `src/system/world/Dir.cpp:422-462` — HX_NATIVE band-character DRAW bridge (draws members on wide shots; renders the origin-collapsed pile).
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4004-4005` — STATIC mesh `obj.world = WorldXfm` (drum kit if static).
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4270-4279` — SKINNED bone palette `BoneOffsetAt × boneTrans->WorldXfm()`.
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:5063-5080, 5120-5141` — band-member shard detector + `[SHARD_GUARD]` drop log (bone0 world pos).
