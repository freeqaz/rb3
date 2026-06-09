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
