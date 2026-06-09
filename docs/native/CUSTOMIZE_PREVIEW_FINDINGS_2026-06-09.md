# Customize-Preview (C6/C11–C13) — Deep Findings & Next Steps (2026-06-09)

Continues [`BLOCKER_VALIDATION_2026-06-08.md`](BLOCKER_VALIDATION_2026-06-08.md)
theme B and roadmap rows C6/C11–C13. Produced by two multi-agent deep-work
workflows (each adversarially verified) plus empirical native runs.

## The goal
Reach the band-customize closet and show the 4 default band members **standing +
animating**, without a real sign-in. The user OK'd: stage 1 = single-char closet,
hacked default-on (no flag, so the web build tests it); guest-profile approach #2
("closer to real flows"); set up the hacks first, then the deeper work.

## What's settled

### C13 — the body source is REAL (proxy-load), not "bodyless shells"
The first pass mis-concluded the `chars.milo` `player0..3` BandCharacters were
bodyless. **Corrected** (decompressed both `chars.milo` + `char/main/main.milo`):
- Each `player0..3` is a **milo PROXY** — `mInlineProxy=true`,
  `mProxyFile="../../char/main/main.milo"` (verified in `chars.milo_xbox` decomp at
  0x34ed8c/0x34f211/0x34f67f/0x34fb02, each preceded by `01 00000019`).
- `ObjectDir::PostLoad` (`obj/Dir.cpp:475-481`) loads `main.milo` **into** each
  player → fires `BandCharacter::AddedObject` (`BandCharacter.cpp:123-124`) →
  binds `mFileMerger`/`mOutfitDir`/`mInstDir`/`mEyes`. `char/main/gen/main.milo`
  (the "main" BandCharacter template) carries `FileMerger.fm`, `outfit`,
  `instrument`, `CharEyes.eyes`, all `BandIKEffector` bones, `body_clips`.
- `FileMerger` then `Select`s the 13 bodypart milos (head/torso/legs/hands/feet/
  hair/…) — **the exact path the working gameplay band uses** (`BandWardrobe` →
  `BandCharacter::StartLoad` → `OnSetFileMerger`; gameplay chars render+animate,
  commit `acd9c19a`).

So the route is: **un-defer CharCache to reuse the proven proxy-load + FileMerger
assembly.** Not route (b) load the Wii chars.milo (no body advantage), not (c)
hand-build a FileMerger (the proxy already supplies it).

### Xbox-vs-Wii asset mismatch — RED HERRING for C13/cascade
Both `chars.milo_xbox` (3.47MB) and `chars.milo_wii` (118KB) are
bodyless-proxy-shells with identical 4×`char/main/main.milo` inlines; the size gap
is crowd content only. The Xbox extract already ships the full `char/main/*` outfit
library the FileMerger `Select`s by path → **Xbox assets are sufficient; do not
switch to Wii assets for chars.** The `Mesh.cpp:878` "needs to be re-exported" WARN
(`gAltRev < 3 && NumBones() > 1`) is non-fatal (gameplay renders despite it).

### The profile cascade (RB3_GUEST_PROFILE=1) — root cause still OPEN
Domino ② (`RndMat::SyncProperty` → `PropSync<RndTex>`, `PropSync_p.h:124`, SIGSEGV
at +0x30) is **NOT** the freed-address ring (the proposed `HxAddrWasFreed` guard is
inoperative — the ring is populated from exactly one site, `CharBones.cpp:1342`).
The real null-`RndTex`/material source must still be traced. Domino ① (`MainHubPanel`
ticker → `TheServer`) is fixed (`rb3_server_native.cpp`, committed `2bb6d944`).

## 🎬 UPDATE 4 (2026-06-09) — ANIMATION root-caused (Gap A closed, Gap B is the deeper blocker)
A 3-trace + Opus-synthesis workflow root-caused why the closet char doesn't animate. TWO additive gaps:

**Gap A — the preview char was never Enter()'d / Poll()'d. ✅ CLOSED (`58d9c8e5`).**
The CharCache preview chars live in `TheCharCache->unk1c`, which is NOT in any polled tree
(`TheWorld`/`TheBandDirector`). The gameplay band animates because `BandWardrobe` `Enter()`s each member
and `App::RunOneFrame` polls `TheBandDirector->mCurWorld` — but those are a DIFFERENT BandCharacter set
(`mTargets`), so that poll never reaches the preview chars. Fix: `Enter()` the loaded preview char in
`ClosetMgr::CharacterFinishedLoading` + `Poll()` it per frame in `ClosetMgr::Poll` (HX_NATIVE, default-on,
opt-out `RB3_NO_CLOSET_POLL`, scoped to `mCurrentClosetPanel`). VERIFIED via `BAND_ANIM_PROBE='*'` in the
closet: `mDriver=<non-null> clipType='shell' bones=<non-null>`, `bone_R-upperArm moved=0.90` across
`Character::Poll` (was: no Poll at all).

**Still no idle LOOP (clip not started).** `BAND_ANIM` shows `clip='(none)' FirstPlaying=(nil) grp='(none)'`:
`SetContext("closet")` sets `SetClipTypes("shell","shell")` + blend width but does NOT set `mGroupName` or
`driver->SetClips`. The gameplay closet does `driver->SetClips(closetMilo->Find("clips"))` in
`BandWardrobe::OnEnterCloset` (BandWardrobe.cpp:988) — the CharCache path never wires the shell clips. So
`PlayMainClip` (needs `mGroupName[0]!=0`, BandCharacter.cpp:203) plays nothing → char poses static, no loop.
NEXT: wire the closet shell clips + a group to the preview char's driver.

**Gap B — skinned=0 (the body meshes have NO bones). ⬜ OPEN, C7/C8-class, broad/high-risk.**
`NumBones()=mBones.size()`; `mBones` is populated ONLY at mesh LOAD (`RndMesh::Load` `bs >> mBones`, each
bone an `ObjPtr<RndTransformable>` resolved BY NAME against the mesh's dir; `RemoveInvalidBones` ERASES
bones that resolve null — and it IS compiled natively, `MILO_DEBUG=1`). The gameplay outfit meshes get
`NumBones()!=0` because the bone names resolve against `char/main/skeleton.milo`, established when each
`*_resource.milo` loads it as a `share=true` subdir (BandCharacter.cpp:2305-2330). In the proxy/FileMerger
preview path that namespace is NOT established before the bodypart meshes load → every bone resolves null →
stripped → `skinned=0`. `RebindOutfitBonesToOwnSkeleton` CANNOT fix this (its loop `for(b=0;b<NumBones();b++)`
runs zero times when `NumBones()==0` — it only REPOINTS existing bones). So even once an idle clip plays,
the body geometry won't deform until Gap B is fixed at the LOAD/share layer (establish skeleton.milo before
the body Select() runs). The engine itself flags this region broad/high-risk (BandCharacter.cpp:2322-2324) —
do it as a careful separate effort, preview/band-scoped, not touching the working gameplay band/crowd.
(Probe caveat: `{rb3_char_probe N}` walks bc's hashtable+subdir-hashtables, reaching the face/hand skin
meshes — so skinned=0 is a TRUE bone gap there — but NOT the draw-tree-only body clothing meshes; extend
the probe to walk the draw tree to fully measure Gap B before/after a fix.)

## 🎯 UPDATE 3 (2026-06-09) — CLOSET REACHED: char STANDS in the closet, no sign-in, NO crash

Empirical breakthrough. The "domino ② cascade crash" is **confirmed stale** — it does
NOT reproduce on the current build. Booting `RB3_GUEST_PROFILE=1` and navigating to
customize is clean; we simply hit `dialog_need_signin_screen`. The real blocker was
never a crash — it was three DTA **gate predicates** the guest profile doesn't flip.

### Empirical truth table (`RB3_GUEST_PROFILE=1`, at `main_hub_screen`, via DTA eval)
`scripts/native/profile-gate-probe.py`:
```
{profile_mgr has_primary_profile}                          => 0   (blocks customize_band -> manage_band)
{{user_mgr get_user_from_pad_num 0} can_save_data}         => 1   (guest profile DID flip CanSaveData)
{{user_mgr get_user_from_pad_num 0} is_char_customizable}  => 0   (blocks customize_character -> closet)
{profile_mgr get_profile <user>}                           => null (profile not associated with the user)
{platform_mgr is_user_a_guest <user>}                      => 0
```

### The closet route WORKS (verified end-to-end, `scripts/native/closet-reach-test.py`)
`is_char_customizable` flips 0→**1** after a single `{prefab_toggle_customizable}`
(confirms the guest user's `mChar` is a `PrefabChar`, whose `IsCustomizable()` ==
`gPrefabIsCustomizable`, a default-false anon-ns global in `PrefabMgr.cpp:25`).
Then firing the real `customize_character.btn` true branch over the HTTP/DTA API:
```
{prefab_toggle_customizable}
{critical_user_listener set_critical_user <user>}
{closet_mgr set_user <user>}
{ui goto_screen customize_clothing_enter_screen}
```
→ transitions through `tv11_a_screen` → settles on **`customize_clothing_screen`**
(THE closet) and **stays there with NO crash** (clothing_base.milo loads; the
`reflection_01.cube` NOTIFYs are non-fatal). **A character renders, standing,
holding an instrument, with the CUSTOMIZE menu** (`/tmp/rb3-closet*/`).

So C11 (reach the closet without sign-in) + C6/C13 (char body on screen) are
**substantially achieved**. Route = guest profile (existing) + `gPrefabIsCustomizable=true`
+ the customize_character flow (which `main_hub.dta` fires automatically once
`is_char_customizable=1`).

### What's NOT yet right (the remaining quality gap)
`scripts/native/closet-anim-verify.py` (pixel-diff burst):
- **`skinned=0` on all 4 CharCache chars** even in the closet (bodies loaded:
  140 meshes/15395 verts). The closet renders the char in **static pose** (baked
  PoseMeshes), NOT driven by a live skeleton → **no idle body animation**. The
  motion the diff sees is the closet's **orbiting camera**, not the char.
- **Head is shard-deformed** (top-center mesh explodes into spikes) — the C7/C8
  head-skinning / face-bone rebind bug, now visible in the closet.
- ROOT (to chase): the closet `PreviewCharacter → CharCache::Request → Poll →
  GetCharacter` path loads the body but the char is **not being Character::Poll'd
  into a bound+posed skeleton** in the closet (the C13 assumption that "the closet
  UI Polls it for free" is **empirically false** — skinned stays 0). Fixing
  skinned>0 should yield both idle animation AND correct (un-sharded) deform,
  since both come from the live skeleton bind (cf RebindOutfitBonesToOwnSkeleton,
  gameplay band is skinned+animating).

### New tools (this session)
- `scripts/native/cascade-crash-capture.py` — gdb-batch fault capture (proved no crash).
- `scripts/native/profile-gate-probe.py` — runtime gate truth table.
- `scripts/native/closet-reach-test.py` — drives the closet route + screenshots.
- `scripts/native/closet-anim-verify.py` — pixel-diff burst anim verdict + per-slot skinned probe.

## ✅✅ UPDATE 2 — Stage 1/2/3 LANDED: preview chars load full bodies natively
Beyond the gate: the full opt-in (`RB3_CHAR_PREVIEW=1`) body-load is committed +
verified in the real boot via the new `{rb3_char_probe N}` DTA func:
```
player0..3  meshes=140  skinned=0  verts=15395  loading=0   (no crash)
```
- **Stage 2** — hardened every `GetCharacter`/`mFileMerger` deref (HX_NATIVE, byte-
  identical `#else`): `BandCharacter::StartLoad`, `CharCache::Request`/`RecomposePatches`/
  `CharactersAreLoading`, `CharSync` InCloset, `ClosetMgr::Poll` (0x62).
- **Stage 3** — un-gated `CharSync::UpdateCharCache` behind `RB3_CHAR_PREVIEW`; the
  no-user/prefab branch (no sign-in) satisfies asserts 0x114/0x128/0x12D (gPrefabs
  populated via `ReloadPrefabs`). Menu-wide firing → `Request` → `StartLoad` →
  `FileMerger` loads the 13 bodyparts → 140 meshes. **No crash, decoupled from the
  guest-profile cascade.**
- `skinned=0` is **pre-Poll** (the skeleton binds at `Character::Poll`). The bodies
  are loaded; posing/animation happens when the char is Polled.

### Remaining to the on-screen "standing + animating in the closet"
1. **Animation is NOT a standalone headless poke** — calling `bc->SetContext("closet")
   + bc->Poll()` raw **SIGSEGVs** (the char needs `ClosetMgr::SetUser`/`PreviewCharacter`
   setup first). It happens *for free* once the **closet UI** Polls the chars via the
   milo proxy. So animation is gated on reaching the closet, not separate work.
2. **Reaching the closet UI** needs the **C11 guest-profile cascade** resolved.
   Cascade domino ② (`PropSync<RndTex>`) root cause is still OPEN — the freed-ring
   theory was refuted; re-root-cause empirically (boot `RB3_GUEST_PROFILE=1 RB3_HTTP=1`,
   capture whether it's a `MILO_ASSERT`/`MILO_FAIL` or a raw SIGSEGV + the exact null,
   instrument `ObjPtr<RndTex>::operator=`). `MILO_DEBUG` is ON natively, so asserts are
   live (not no-ops — a prior analysis assumed wrongly).

## ✅ UPDATE — Stage-0 gate PASSED (verified in the real App boot)
The build blocker cleared (the concurrent `App.cpp`/`BandOffline` WIP was committed,
`db54b18f`). The in-process gtest was **retired** — a headless test can't run the GPU
`Rnd::Init` rndobj-factory cluster (`RndCam`/`RndLight`/`RndEnviron`/… are registered
inside the GPU render init), so loading a full real milo there is impractical.
Instead the gate is an **opt-in Stage-1 load + probe in `CharCache::InitMe`**
(`RB3_CHAR_PREVIEW=1`, committed), verified in the real boot:
```
C13_PROBE: player0 char=<non-null>  FileMerger.fm=<non-null>
C13_PROBE: player1..3  … same …      (no crash, no "Unknown class")
```
→ chars.milo loads natively, the proxy-load of `char/main/main.milo` binds
`mFileMerger` for all 4 players, and `BandCharacter::StartLoad`'s `mFileMerger`
deref is safe. **C13 route confirmed.** Stage 2 (harden derefs) + Stage 3 (un-gate
`UpdateCharCache` → StartLoad → FileMerger loads 13 bodyparts → animate) are next,
with a headless body/anim probe that decouples from the guest-profile cascade.

## (historical) The decisive next step — Stage-0 runtime gate (WRITTEN, verification BLOCKED)
Before un-deferring anything, prove the proxy-load actually binds `mFileMerger`
natively (chars.milo's own parse on the clang/LE engine is otherwise unproven;
`world_chars.milo` does NOT prove it — it's 0 BandCharacters/TransProxies).

- **Gate test written:** `native/tests/test_char_preview.cpp` (currently
  **untracked** — see blocker). `CharPreview.CharsMiloPlayersHaveFileMergerBody`:
  `CharInit()`+`BandInit()` → `DirLoader::LoadObjects("world/shared/chars.milo")`
  → assert each `player0..3` is a real `BandCharacter` with a `FileMerger.fm`
  child. Green = route confirmed + `StartLoad:1359` deref safe; red = proxy-load
  needs fixing natively first.
- **Wiring (1 line):** add `${CMAKE_SOURCE_DIR}/tests/test_char_preview.cpp` to the
  `add_executable(rb3-tests …)` list (after `test_subsystems.cpp`).
- **Run:** `cmake --build native/build-native --target rb3-tests` then
  `RB3_DATA=…/orig-assets/extracted ./native/build-native/rb3-tests --gtest_filter=CharPreview.*`

### ⚠️ Verification blocker (2026-06-09)
A concurrent agent's **uncommitted `src/App.cpp`** edit adds `#include
"BandOffline.h"` + `BandOffline::Init()`. `BandOffline.h` uses
`STLPORT::StlNodeAlloc<…>`, and `STLPORT` is not defined under `HX_NATIVE` (it's
`#ifdef STLPORT namespace STLPORT` in `utl/StlAlloc.h`) → `App.cpp` fails to compile
→ the shared `rb3-native`/`rb3-tests` build is red. Not ours; **do not clobber it.**
The fix that unblocks: `BandOffline.h` needs an `HX_NATIVE` branch for its
`STLPORT::StlNodeAlloc` map allocators (or `App.cpp` should gate the
`BandOffline::Init()` include). Once green, wire + run the gate above. (Worktree
verification was attempted but hit engine-subdir `find_package` friction.)

## Full C13 enable plan (after the gate is green)
All `HX_NATIVE`-guarded, byte-identical `#else`; opt-out env (mirror
`RB3_NO_DEFORM_LOAD`). `UpdateCharCache` runs **menu-wide on every screen
transition**, so it must survive the no-profile prefab branch too.
1. Un-defer `CharCache::InitMe` chars.milo load (`CharCache.cpp:50`).
2. Harden every `GetCharacter`/`mFileMerger` deref with byte-identical `#else`:
   `BandCharacter::StartLoad:1359` (null `mFileMerger` → early return),
   `CharSync.cpp:179` (inside a 100%-matched fn — keep `#else` exact),
   `CharCache.cpp:65` (RecomposePatches) + `:120` (IsLoading), `ClosetMgr.cpp:57`.
3. Un-defer `CharSync::UpdateCharCache` (`CharSync.cpp:49`).
4. Factory cluster: `CharInit()`+`BandInit()` already run on the full App boot
   (`App.cpp:363/375`) before `CharCache::Init` — confirm the closet path uses it.
5. Heads stay default/un-shaped (`BandHeadShaper::Init` deferred on the real
   `CharClip`/`CharBonesSamples` desync, item 5b; `Start` is null-safe — quality,
   not crash). gDeforms is already on (`a5999979`).
6. **Visual verify decoupled from the guest-profile cascade:** the closet is only
   reachable with `RB3_GUEST_PROFILE=1` (cascade open). Verify the body/FileMerger
   binding **headlessly** via `/api/dta/eval` (`{{char_cache get_character 0} find
   FileMerger.fm}`) + `BAND_ANIM_PROBE` bone-motion, rather than a full closet
   screenshot, until the cascade is resolved.

## Landed this work (committed)
- `2bb6d944` — faithful offline `TheServer` (kills a latent null-vtable crash class)
  + **opt-in** guest-profile foundation (`RB3_GUEST_PROFILE=1`, default-OFF because
  the cascade is open). Roadmap C11.
