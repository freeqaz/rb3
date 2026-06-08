# RB3 Native Port — Blocker Validation Sweep & Convergence Plan (2026-06-08)

Follow-up to [`NATIVE_HACK_AUDIT_2026-06-08.md`](NATIVE_HACK_AUDIT_2026-06-08.md).
The audit *enumerated* the hacks; this pass **validated** which "blocked" items
are actually still blocked, **corrected** two audit conclusions that were wrong
on the settled tree, **landed** the verified-safe fixes, and **re-designed** the
char-customize bring-up with the crash surface it really has.

Produced by a 17-agent validation workflow (7 deep investigators → 2 adversarial
skeptics each, lenses = *runtime-crash safety* and *Wii-match/byte correctness*),
then every actionable verdict was empirically re-verified in the main loop before
landing (build → gtest/objdiff/headless boot → commit).

## TL;DR

| # | Item | Audit said | Sweep verdict | This session |
|---|------|-----------|---------------|--------------|
| 1 | VocalPlayer RTTI-cast guard (`Game.cpp:813`) | next (hygiene) | **Blocker GONE** — RTTI is real | ✅ **landed** `579e7416` |
| 2 | BandHeadShaper `gHeadMale` typo (`:161`) | MED-HIGH, blocked | **Real decomp bug**, separable, match-positive | ✅ **landed** `4e49ef34` |
| 3 | BandFaceDeform BE reader (theme A) | blocked, needs new reader | **FALSE ALARM** — already byte-correct | ✅ **gtest** `15e3c048`; audit corrected |
| 4 | CharCache/CharSync char-preview (theme B) | blocked | **Real, mischaracterized** — menu-wide + hard-crash gate | 📋 **re-designed** (below); next-session exec |
| 5 | worldcenter occluder (`Draw.cpp:79`) | re-verify-then-delete | **KEEP** — RTT fixed but orthogonal | 📋 documented |
| 6 | SP scoreboard milo (`TrackPanelDir.cpp:294`) | diagnostic-first | **KEEP** — stopgap correct | 📋 documented |
| 7 | char hair/face rebind residual (theme F) | blocked, AT_LIMIT? | **Blocked, but concrete next experiment** (no Xbox ref needed) | 📋 documented |

Net: **two systems converged** (vocal-percussion tambourine bank now wires for
all players; female char heads load into the right global), **one false blocker
retired** with a permanent guard, and the **biggest remaining system
(char-customize previews) is now precisely scoped** instead of vaguely "blocked."

---

## Landed this session (all HX_NATIVE-safe, empirically verified)

### 1. VocalPlayer RTTI-cast guard deleted — `579e7416`
`Game::SetVocalPercussionBank(Player*, ObjectDir*)` had an `#ifdef HX_NATIVE`
early-return for non-vocal players, on the premise that `VocalPlayer`'s RTTI was
a zeroed `_ZTI11VocalPlayer` stub that `dynamic_cast` would fault on.
**That premise is stale**: `VocalPlayer.cpp` is compiled (not in
`_NATIVE_FORK_EXCLUDE`), `band3_link_stubs.s` documents the RTTI/ctor/vtable stub
*removals*, and two already-compiled sites (`PracticePanel.cpp:149/310`,
`VocalTrainerPanel.cpp:37`) do the same unguarded cast. Deleting the guard
restores the shared Wii body (no `#else` existed → match-positive). The cast now
runs on non-vocal players too, where it returns null (harmless).
**Verified:** `rb3-native` boots to `game_screen` and runs `FinishLoad →
SetVocalPercussionBank` with no segfault.
*Keep `kInvalidPitch__11VocalPlayer` in the stub — it's an orthogonal MWCC
mangled-name cross-TU trick (`VocalPart.cpp:625`), not an RTTI artifact.*

### 2. BandHeadShaper female-branch typo fixed — `4e49ef34`
`BandHeadShaper::Init`'s female branch loaded into `gHeadMale`/`gHeadMaleMapping`
instead of `gHeadFemale`/`gHeadFemaleMapping` (the rest of the branch already
used `gHeadFemale`). **Proven a decomp transcription bug vs target asm**: the
female stores target `0x30`/`0x40` (`gHeadFemale`/`gHeadFemaleMapping`), not
`0x14`/`0x28`. Shared Wii code → fix is **match-positive**
(`Init__14BandHeadShaperFv` raw `98.97276 → 98.99611`; residual is regalloc).
Behaviorally correct: female heads no longer clobber `gHeadMale` and leave
`gHeadFemale` null. *(The head-milo load itself stays gated off via `_tmp0/_tmp1`
— see theme A below.)*

### 3. BandFaceDeform BE-decode guard test — `15e3c048`
`NativeSubsystems.BandFaceDeformDeltaArrayLoadBE` (in `native/tests/`). See theme
A — it locks the proven-correct decode so nobody re-applies the audit's wrong fix.
Suite now **11/11 green**.

---

## Theme A (BandFaceDeform) — the audit was WRONG; no reader needed

The audit (`NATIVE_HACK_AUDIT_2026-06-08.md:52`) claimed `DeltaArray::Load` does
"two *overlapping* 2-byte reads … corrupting the boundary byte and desyncing
`thisoffset()`," requiring a new HX_NATIVE reader. **This is false**, proven
against the real asset:

- `BinStream::ReadEndian` on a LE host (`HX_NATIVE`) swaps **iff the file is
  big-endian** (`!mLittleEndian`) — so each multi-byte `>>` from a `.milo_xbox`
  is correctly host-swapped (`BinStream.cpp:161-176`).
- On-disk each `Delta` record (BE) is: `u16 startVertexIndex` @0-1, `u16 num`
  @2-3, then `num*3` signed-char deltas; stride `= num*3+4`. The `char unk0`
  field name is a **misnomer** — it's the *high byte* of the 2-byte start index.
- The two `>>` reads (`(short&)d->unk0` @0-1, `d->num` @2-3) are **non-overlapping**.
  `num`/`thisoffset()` are read correctly → **the stream never desyncs**; only the
  (unused-as-a-standalone-`char`) start index is stored host-native, which is
  exactly how the consumer `BandHeadShaper::AddFrame` reads it back
  (`*(unsigned short*)d`).
- **Decisive check:** simulating the current code over `head_male.milo_xbox`
  `frame[1]` (mSize=422, 38 records) reproduced the BE ground truth **38/38,
  remaining=0**. The audit's proposed "read `unk0` as 1 byte" fix would consume
  3 header bytes instead of 4 and read `rec0 num=0xE800` → total desync.

Real headshaper blocker is the head milo's `CharClip`/`CharBonesSamples`/`CharBones`
`Load` (version-desync + string-len overflow), **not** this reader. The new gtest
guards the correct behavior. No DC3 analog (RB3-specific).

---

## Theme B (char-customize previews) — REAL, re-designed with the true crash surface

**Status: designed, NOT landed.** Both skeptics confirmed the original audit
plan's **safety premise was factually wrong** and its **guard list was
incomplete**. This is a genuine multi-precondition sub-project, correctly gated on
runtime verification — execute next session with a focused workflow + a headless
customize-screen harness.

### What's actually true
- **gDeforms prerequisite: satisfied** (`a5999979`, default-on, opt-out
  `RB3_NO_DEFORM_LOAD`).
- **The "world_chars proves chars.milo parses" premise is FALSE.**
  `world_chars.milo_xbox` (12 KB) is an RndDir of `TransProxy` bone-proxies with
  **zero** `BandCharacter` objects. The real working gameplay BandCharacter path
  is `BandWardrobe.cpp:890 Find<BandCharacter>("player%d")` from the **venue**
  milo, and `BandCharacter::Poll` is already natively hardened
  (`RebindOutfitBonesToOwnSkeleton`). So BandChars *do* parse + Poll natively —
  but `chars.milo`'s parse itself is **unproven**; the only evidence is that it's
  the shipped asset (3.47 MB, 4× `BandCharacter` player0-3, milo magic/version
  `0x810`).

### The real crash surface (wider than the audit said)
Three native hacks keep the preview cache fully off:
`CharCache::InitMe` (`#ifndef HX_NATIVE` around the `chars.milo` load),
`CharCache::GetCharacter` (`#ifdef HX_NATIVE if(!unk1c.Ptr()) return nullptr`),
and `CharSync::UpdateCharCache` (`#ifdef HX_NATIVE return`). **Un-gating
`UpdateCharCache` runs it menu-WIDE on every screen transition** (`CharSync.cpp:139-208`),
not just on customize — so the whole chain must survive:
1. **Hard crash (no assert):** `BandCharacter::StartLoad` (`BandCharacter.cpp:1359`)
   does `mFileMerger->StartLoad(b1)` **unconditionally**; `mFileMerger` defaults
   null and is only bound if a `FileMerger.fm` child exists. If `chars.milo`'s
   players lack it, the *first* `Request` segfaults. **Byte-unverifiable** (milo
   class-names are a binary string-id table) → **runtime gate**. The engine
   already guards this at `OnSetFileMerger:2434`, confirming null is anticipated.
2. **Null-derefs** at `CharSync.cpp:179` (`GetCharacter(n)->InCloset()`),
   `CharCache.cpp:65` (`RecomposePatches`), `CharCache.cpp:120` (`IsLoading`, via
   the `characters_are_loading` handler) — all fault if `GetCharacter` returns
   null (which the opt-out / a transient unloaded slot makes possible).
3. **Asserts** needing the profile/prefab subsystem populated:
   `CharSync.cpp:185` (0x114), `:197` (0x128), `:203` (0x12D); `ClosetMgr.cpp:57`
   (0x62); `CustomizePanel.cpp:1086` (0x6fb).

### Execution plan (next session)
All `#ifdef HX_NATIVE`-only; every `#else` must keep the **exact** original line
(the `CharSync.cpp:179` edit sits inside the 100%-matched `UpdateCharCache` body —
highest match-attention edit). Opt-out env `RB3_NO_CHAR_PREVIEW` (mirrors
`RB3_NO_DEFORM_LOAD`).

0. **Runtime gate FIRST.** A headless probe (or gtest that pumps `LoadMgr::Poll`):
   load `chars.milo`, assert `GetCharacter(0..3)` non-null **and** each has a
   `FileMerger.fm` child (`Find<FileMerger>("FileMerger.fm")`). Do not wire Stage 3
   until this is green.
1. **Stage 1** — enable the `chars.milo` load (env-gated).
2. **Stage 2** — harden **all** GetCharacter deref sites (StartLoad `mFileMerger`,
   CharSync:179, CharCache:65, CharCache:120), each with byte-identical `#else`.
3. **Stage 3** — un-gate `UpdateCharCache`; verify the prefab assert chain holds
   on real menu state.
4. **Stage 4 (quality, not a crash gate)** — heads stay un-shaped until the head
   milo `Load` lands; `BandHeadShaper::Start` early-returns on empty `mMapping`
   (null-safe), so previews render bodies/outfits/deform-morph + generic heads.

**Verification:** headless boot to customize/closet (nav path TBD — open question),
A/B the preview podium screenshot, run thousands of frames for stability, and
`RB3_NO_CHAR_PREVIEW=1` to confirm clean opt-out. Add the Stage-0 gate as a gtest.

---

## Keep-as-is (documented, no code change)

- **worldcenter occluder** (`Draw.cpp:79`). The RTT/clouds blocker **is** fixed
  (engine `9f635b7`, in pin `59b7307`), but it was never the void's cause:
  `worldcenter.mesh` is an opaque-black, untextured, **depth-writing**
  (`kZModeNormal`) box that occludes the sky layers regardless of RTT. Deleting
  the skip re-introduces the void. The skip is tightly scoped (1 mesh, ~2 draws,
  the sv8 rooftop hub only) and revertible (`RB3_MENU_VOID_FIX_OFF=1`). A future
  *principled* fix is an HX_NATIVE ZMode-no-depth-write branch for this backdrop
  (NEEDS_NEW_CODE), not deletion. **A/B to confirm before any change:** sv8 hub,
  `RB3_MENU_VOID_FIX_OFF` unset vs `=1` (RTT on both), vs
  `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`.
- **SP scoreboard milo** (`TrackPanelDir.cpp:294`). The synthetic `right.grp`/`left.grp`
  X-neutralization is correct and idempotent. The real mechanism (apply-script
  iterating `mConfigurableObjects` to `set_local_pos`) isn't exercised natively —
  likely `mConfigurableObjects` is empty for the `1_player_*` config node
  (a `PostLoad`/`LOAD_REVS` revision-gating question). Convergent fix is
  diagnostic-first (log `mConfigurableObjects.size()` via `K9_APPLY_DBG`) with no
  user-visible benefit over the stopgap. Keep, comment-reference this doc.

## Blocked, with a concrete lead

- **char head/hands/face rebind residual** (theme F, `BandCharacter.cpp:802`,
  `sTorsoOnly=1`). Hair/fingers shard under full rebind (`RB3_SKEL_REBIND_FULL=1`)
  from a rotation-basis mismatch: the animated per-member bone's worldRot basis
  sign-flips vs the static magnet the authored invBind offsets were baked against.
  **Next experiment (no Xbox/Xenia ref needed — deterministic from assets):**
  capture `perMemberBoneBindWorld` per bone at the skeleton first-load/rest frame
  (before the director assigns the idle clip), then at the Poll rebind site apply
  `offset' = BoneOffsetAt(b) · inverse(boundBoneWorldAtLoad) · ownBoneWorldAtLoad`.
  Open question: is the per-member skeleton in rest/identity pose at
  `SyncObjects`? Test: `RB3_SKEL_REBIND_FULL=1` + `char-burst-capture.py --shots 40`,
  `REBIND_DRAW_FLING` fires 0× on hair/face, `XBONE_TRACK` shows animation.
  Independent of themes A and B.

---

## Corrections applied to the original audit doc
- Theme A: the BandFaceDeform "overlapping reads desync" mechanism is wrong;
  the current code is already byte-correct (proven 38/38 vs `head_male.milo_xbox`).
- Theme B: the "world_chars produces working BandCharacters of the same types"
  premise is wrong (world_chars is 0 BandCharacters); crash surface widened
  (`mFileMerger` hard-crash + menu-wide `UpdateCharCache` + extra GetCharacter sites).
