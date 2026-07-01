# Char-Load 5b — Theme B (char-customize preview cache) — Guarded-Rollout Plan

**Lane:** SCOPE theme B — char-customize preview cache (chars.milo + CharSync + BandCharacter::StartLoad).
**Date:** 2026-06-11. **Status: PLAN — but the plan is ALREADY ~95% EXECUTED on the current tree.**

## Headline finding (correct the validation doc's "designed, NOT landed" status)

The validation doc `BLOCKER_VALIDATION_2026-06-08.md` (theme B) describes this as a
NOT-landed, next-session sub-project with a 5-stage plan. **That plan has since been
implemented and committed.** As of HEAD (2026-06-11) the theme-B preview cache is
**default-ON** (opt-out `RB3_NO_CHAR_PREVIEW`), all the planned guards are in place,
the files are clean/committed, and the hard-crash site is guarded. So this lane's
deliverable is: (1) record that the rollout is done, (2) map the validation-doc
plan onto the commits that satisfied each stage, (3) document the *one* residual
gap and the explicit non-dependency on 5b.

### Commit trail (the staged rollout that already happened)
- `89ff46a1` — **Stage 0 + Stage 1**: opt-in `chars.milo` load in `CharCache::InitMe`; the
  C13_PROBE confirmed in a real App boot that all four `player0..3` are milo PROXIES whose
  proxy-load of `char/main/main.milo` binds `mFileMerger` (FileMerger.fm child) → **non-null
  for all 4**. This RETIRES the validation doc's central fear ("byte-unverifiable, StartLoad
  segfaults if no FileMerger.fm"). The proxy path supplies the FileMerger; previews load the
  same body machinery as gameplay.
- `5a5edee8` — **Stage 2 + Stage 3**: enable full body load (140/13 meshes verified) and
  un-gate `CharSync::UpdateCharCache` (menu-wide).
- `9318bb73`, `92fcb32c`, `65f7f0e6` — default-on / revert / **restore default-on**. The
  default-on was reverted once because un-gating UpdateCharCache menu-wide tripped the
  "domino ②" `song_select` SIGSEGV (a `dynamic_cast` on a clobbered ProfilePicture via the
  `refresh_summary` DTA's `get_picture_tex`). `65f7f0e6` **fixed that at its source**
  (`BandProfile::GetPictureTex()` returns null on native, HX_NATIVE-gated, byte-identical
  `#else`) and restored default-on. This is the menu-wide-impact mitigation the plan asked for.

## Current gate posture per site (all VERIFIED in code, this lane)

| Site | File:line | Posture now | Maps to plan stage |
|---|---|---|---|
| chars.milo load | `meta_band/CharCache.cpp:60-71` (InitMe) | **default-ON**, `if(!getenv("RB3_NO_CHAR_PREVIEW"))` load; `#else` = byte-identical Wii deferred-load line | Stage 1 |
| GetCharacter null | `CharCache.cpp:132-136` | HX_NATIVE `if(!unk1c.Ptr()) return nullptr` | guard |
| Request null | `CharCache.cpp:88-93` | HX_NATIVE `if(!bchar) return` | Stage 2 |
| RecomposePatches null | `CharCache.cpp:99-108` (was cited :65) | HX_NATIVE `if(!bchar) return`, byte-identical `#else` | Stage 2 |
| CharactersAreLoading null | `CharCache.cpp:160-172` (IsLoading, was cited :120) | HX_NATIVE `if(bchar && bchar->IsLoading())`, byte-identical `#else` | Stage 2 |
| UpdateCharCache menu-wide gate | `CharSync.cpp:51-65` | HX_NATIVE `if(getenv("RB3_NO_CHAR_PREVIEW")) return`, `#else` body unchanged | Stage 3 |
| InCloset() deref (cited :179) | `CharSync.cpp:184-190` | HX_NATIVE `cc ? cc->InCloset() : false`, byte-identical `#else` (the highest-match-attention edit, inside the 100%-matched body) | Stage 2/3 |
| BandDirector null (music-video branch) | `CharSync.cpp:147-155` | HX_NATIVE `if(TheBandDirector && ...->IsMusicVideo())` | extra guard the plan didn't list |
| **HARD CRASH: mFileMerger deref** | `bandobj/BandCharacter.cpp:1538-1539` (StartLoad, cited :1359) | **HX_NATIVE `if(!mFileMerger) return;` backstop in place** (also clears native rebind latches on re-load) | Stage 2 — the runtime guard the prompt asked to add |

### chars.milo player path DOES carry a FileMerger.fm child — CONFIRMED
The prompt's open question ("does the chars.milo player path carry a FileMerger.fm child?")
is answered YES, empirically, by the C13_PROBE in `89ff46a1`: `player0..3` are milo proxies
(`mProxyFile = ../../char/main/main.milo`); the proxy-load binds `mFileMerger` from
`FileMerger.fm`. So StartLoad's deref is genuinely safe on the real asset — the `if(!mFileMerger) return;`
at `BandCharacter.cpp:1538` is a defensive backstop, not load-bearing for chars.milo.

## hx_native_gate posture (answer to the schema field)
**YES — already HX_NATIVE-gated throughout.** Every theme-B edit is `#ifdef HX_NATIVE`
with a byte-identical `#else` preserving the original Wii line (the `CharSync.cpp:184`
InCloset edit and `CharCache` RecomposePatches/IsLoading are inside 100%-matched bodies;
their `#else` keeps the exact original). No match-neutral-but-unguarded edits were needed.
The Wii build is byte-identical. No further objdiff verification is required for *new* work
because there is no new shared-code edit to make.

## Prerequisites & the 5b dependency (answers to the prompt's explicit asks)

- **gDeforms prerequisite: SATISFIED** (`a5999979`, default-on, opt-out `RB3_NO_DEFORM_LOAD`).
- **Does char-customize need head-milo loading (5b)? NO — not for crash-safety; only for
  head-shape quality.** This is the validation doc's Stage-4 ("quality, not a crash gate").
  Confirmed in code: with 5b still gated off (`BandHeadShaper::Init` sets `_tmp0/_tmp1=false`
  at `BandHeadShaper.cpp:131-137,156`, leaving `gHeadMale/gHeadFemale` null and the mappings
  empty), `BandHeadShaper::Start` **early-returns on `mMapping->size()==0`** at
  `BandHeadShaper.cpp:221`. So previews render bodies/outfits/deform-morph + **generic
  (unshaped) heads** safely. Landing 5b later upgrades preview heads from generic to
  per-face-shaped; it does not block or crash the preview cache. **Theme B and 5b are
  independent**; theme B is the parent-prompt's "unblocked even without 5b" case.

## Residual gap (the only open item — tracked, NOT a regression)
Per `65f7f0e6`'s own residual note, the char-preview composite still **intermittently
aborts at boot (~1/19 runs)** on a `BandPatchMesh::SetMeshVerts` out-of-range face index —
the same char-mesh-desync/heap family already tracked elsewhere (cf. the songlib
BandPatchMesh LP64/PropSync saga). It is NOT the domino-② UAF and is unaffected by the
theme-B default-on. This is the residual crash risk for the default-on rollout.

### Guarded-rollout recommendation for the residual
1. Keep `RB3_NO_CHAR_PREVIEW` as the documented opt-out (already wired in both
   `CharCache::InitMe` and `CharSync::UpdateCharCache`). This is the kill-switch if the
   intermittent abort regresses CI/boot stability.
2. Treat the `BandPatchMesh::SetMeshVerts` OOB as a **separate lane** (mesh-vert/heap
   desync), not a theme-B gate. It does not need new gates in CharCache/CharSync/StartLoad.
3. **Verification already standing**: the C13 rollout was verified in real App boot
   (≥18 song_select reaches, 0 dynamic_cast crashes across default-on/explicit-flag/gdb/full-nav;
   closet renders a 140-mesh standing character on `customize_clothing_screen`). The
   in-process gtest Stage-0 gate was **retired on purpose** — a headless test can't run the
   GPU `Rnd::Init` rndobj-factory cluster, so the gate lives in App boot. A durable guard
   would be the boot-to-customize headless harness (validation doc's "boot-to-gameplay-flow"
   gtest #5), driving nav to `customize_clothing_screen` and asserting GetCharacter(0..3)
   non-null + N>0 meshes + no abort over thousands of frames. **(nav path still TBD — open.)**

## Net for the fix step
There is **no code change to make in this lane** — the staged guarded rollout is landed and
default-on. The fix step should (a) accept theme B as DONE, (b) not re-touch any
CharCache/CharSync/StartLoad gate (they are correct and byte-identical on Wii), (c) route the
intermittent `BandPatchMesh::SetMeshVerts` OOB to the mesh-desync lane, and (d) keep 5b
(head-milo CharClip/CharBonesSamples Load) as the genuinely-open blocker — independent of
this preview cache, upgrading preview heads from generic to shaped when it lands.
