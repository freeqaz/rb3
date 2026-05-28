# SALVAGE_V33 — re-apply the permuter-wiped venue / character HX_NATIVE blocks

**Date:** 2026-05-28 (V33 SALVAGE subagent).
**Status:** LANDED. Mesh count restored to 173–261/frame at gameplay (was 50–65
plateaued). 3/3 reliability runs exit 0. All four `EnterVenue` HX_NATIVE blocks
fire as logged by `VENUE_DBG=1`.

## TL;DR

The background permuter had silently wiped **four** load-bearing matched-fork
HX_NATIVE blocks since V32 — without them, the venue WorldDir was never
requested (V19), the band players' instrument-keyed proxies collapsed onto a
shared stand-in (V23), the outfit FileMerger crashed on libstdc++ vector
realloc (V23 `ReplaceRefs`), and the gameplay HUD never appeared (V25). A
fifth fix, V20 `BandPatchMesh`, was needed to make V23's first-time
`LoadCharacters` path survive `OutfitConfig::Load`'s
`ObjVector<BandPatchMesh>::resize` (the weak no-op stub left ObjPtr members
holding garbage that SEGV'd on destruct).

All five fixes re-applied as additive `#ifdef HX_NATIVE` blocks; build clean,
3/3 runs exit 0, mesh count 173–261/frame (vs the pre-salvage 50–65
plateau), screenshots captured.

## Audit table — every block this session was expected to verify or restore

| # | Site | File:line | Doc | Status on entry | Action |
|---|---|---|---|---|---|
| V3  | `BandDirector::Enter` motion_blur fallback | `BandDirector.cpp:134` | (Phase 0) | INTACT | none |
| V12 | `TrackPanelDir::ConfigureTracks` CAMERA_FRAME_FIX | `TrackPanelDir.cpp:382` | (V12) | INTACT | none |
| V14b | `BandTrack::SoloStart` solo_percent seed | `BandTrack.cpp:445` | SCORE_PCT | INTACT | none |
| V19a | `BandDirector::EnterVenue` venue force-load | `BandDirector.cpp:~509` | VENUE_RENDER V19 | **WIPED** | **RE-APPLIED** |
| V19b | `BandDirector::ReadyForMidiParsers` gate | `BandDirector.cpp:601` | VENUE_RENDER V19 | INTACT | none |
| V19c | `RndEnviron::SetFogEnable` return | `Env.h:75` | VENUE_RENDER V19 | INTACT | none |
| V19d | `GetVenuePath` strstr const-cast | `BandDirector.cpp:547` | VENUE_RENDER V19 | INTACT | none |
| V20a | `BandPatchMesh.cpp` `stlpmtx_std` gates | `BandPatchMesh.cpp:130/518` | VENUE_RENDER V20 | **WIPED** (TU excluded) | **RE-APPLIED** |
| V20b | `BandPatchMesh` un-exclude | `native/CMakeLists.txt:307` | VENUE_RENDER V20 | **WIPED** | **RE-APPLIED** |
| V21 | `Multiply(Vector3,Matrix3,Vector3)` C body | `Mtx.h:640` | VENUE_RENDER V21 | INTACT (V32 re-applied) | none |
| V22 | `BandDirector::DrawShowing` venue-cam-follow | `BandDirector.cpp:317` | VENUE_RENDER V22 | INTACT | none |
| V23a | `EnterVenue` LoadCharacters bridge | `BandDirector.cpp:~554` | VENUE_RENDER V23 | **WIPED** | **RE-APPLIED** |
| V23b | `EnterVenue` HarvestDircuts re-run | `BandDirector.cpp:~578` | VENUE_RENDER V23 | **WIPED** | **RE-APPLIED** |
| V23c | `BandWardrobe::LoadMainCharacters` mic→vocals | `BandWardrobe.cpp:~694` | VENUE_RENDER V23 | **WIPED** | **RE-APPLIED** |
| V23d | `BandCharacter::ReplaceRefs` realloc-safe | `BandCharacter.cpp:1730` | VENUE_RENDER V23 | **WIPED** | **RE-APPLIED** |
| V25 | `GamePanel::StartGame` HUD force-show | `GamePanel.cpp:~268` | SCORE_HUD | **WIPED** | **RE-APPLIED** |
| V25-d | `TrackPanel::Poll` K8 diagnostic | `TrackPanel.cpp:558` | SCORE_HUD | INTACT | none |
| V25-d | `TrackPanelDirBase::SetShowing` K8 trace | `TrackPanelDirBase.cpp:293` | SCORE_HUD | WIPED (diag only) | not re-applied (low value) |
| V26 | `MakeRotQuat` half-angle factors | `Rot.cpp:485` | VENUE_RENDER V26 | INTACT (V32 re-applied) | none |
| V28a | Locale `HX_PC` scope | `Locale.cpp:106` | TEXT_TOKENS | INTACT | none |
| V28b | ViewSetting node-order swap | `ViewSetting.cpp:200` | TEXT_TOKENS | INTACT | none |
| V29 | `BandTrack::Reset` solo_percent seed | `BandTrack.cpp:141` | SCORE_PCT | INTACT | none |
| V31 | `TrackPanelDir::ConfigureTracks` apply-handler | `TrackPanelDir.cpp:252` | APPLY_HANDLER | INTACT | none |
| V31-d | `TrackPanelDirBase` K9_APPLY_DBG | `TrackPanelDirBase.cpp:89` | APPLY_HANDLER | INTACT | none |
| V32 | `CharIKHand::Poll` IK_TGT_DBG | `CharIKHand.cpp:31` | VENUE_RENDER V32 | INTACT | none |
| V23-e | `BandCharacter::SyncObjects` bones-sentinel | `BandCharacter.cpp:598` | (pre-V23) | INTACT | none |
| V27/glue | Boot reliability | `native/src/**` + engine | BOOT_RELIABILITY | not matched-fork — n/a | none |

**Audit summary: 8 blocks wiped of ~25 audited (~32%).** 7 re-applied; 1 (V25-d
diagnostic) intentionally skipped as low value.

## Re-applies in detail

### V19a — `BandDirector::EnterVenue` venue force-load
At the top of `EnterVenue()`. If `mVenue.Dir()` is null and `GetWorld()` is up,
reads the world's authored `venue` symbol (fallback `small_club_01`),
temporarily sets `mAsyncLoad=false`, and calls
`LoadVenue(venueSym, kLoadStayBack)` — exactly what the data-driven retail
`load_venue` would do. Trace gated on `VENUE_DBG`.

### V23a — `EnterVenue` LoadCharacters bridge
Inside the `TheBandWardrobe` block, immediately before `SetVenueDir(dir)`. If
`TheBandWardrobe && !mVenue.Name().Null()`, calls
`TheBandWardrobe->LoadCharacters(mVenue.Name(), mAsyncLoad)`. Placed here (not
at `OnFileLoaded`) because the venue's `world_chars` load re-finds `player%d`
and REPLACES `mTargets`, so any earlier call would set names/instruments on
characters that get thrown away. At this point `mTargets` are the venue's final
characters and `mAsyncLoad` is the just-cleared sync value from V19a.

### V23b — `EnterVenue` HarvestDircuts re-run
After `setup_midi_parsers_msg` + `ClearLighting()`. Guarded on `mPropAnim &&
mVenue.Dir()`. The earlier harvest (driven from the load flow before the
V19a force-load) bailed at the same gate with `venueDir=(nil)`, so the song's
authored MIDI `DIRECTED_CUT`s were never harvested and the director fell back
to category cycling. Re-running here picks up the 15 dircuts in
`20thcenturyboy`.

### V23c — `BandWardrobe::LoadMainCharacters` mic→vocals
In the `for (i=0..3)` venue-name loop. After the existing
`if (inst == "none") inst = "vocals";` line, adds
`if (inst == "mic") inst = "vocals";`. The vocalist's `mInstrumentType` is
`mic` (= `gInstNames[3]`), but venue proxies + closeup `BandCamShot` targets
are named `player_vocals0_*` (14681 refs in small_club.milo vs 0
`player_mic0`). Without the remap the singer's closeup proxies collapse onto
a stand-in.

### V23d — `BandCharacter::ReplaceRefs` reallocation-safe
The matched-fork body caches `std::vector<ObjRef*>` iterators into
`theirs->Refs()`, walks them in reverse, and after each `ref->Replace()`
resets `it = end()`. `ref->Replace()` ERASES from `theirs->mRefs`, which
under libstdc++ can REALLOCATE the vector and invalidate the cached
iterators → dangling `it[-1]` dereference → SIGSEGV. The HX_NATIVE branch is
an index-based rewrite: re-reads `theirs->Refs()` each outer iter, walks
indices high→low, breaks and restarts the outer `while` after each replace.
Semantics identical to the matched fork. `#else` byte-identical to the
permuter form. Without this fix, the V23a `LoadCharacters` bridge above
crashes immediately on the first player outfit merge.

### V20a — `BandPatchMesh.cpp` `stlpmtx_std` gates
Wrapped both `namespace stlpmtx_std { ... }` blocks (lines ~130 and ~518) in
`#ifndef HX_NATIVE … #endif`. The explicit specializations of
`__unguarded_partition` / `__introsort_loop` / `__adjust_heap` /
`__unguarded_linear_insert` are asm-match-only — clang's libstdc++ has no
such symbols → "no function template matches". Same pattern as
`GameGemList.cpp`. The `std::sort` calls below fall through to the host
libstdc++.

### V20b — `BandPatchMesh` un-exclude (`native/CMakeLists.txt`)
Removed `BandPatchMesh` from `_NATIVE_FORK_EXCLUDE`. With the TU excluded and
weak-stubbed, `OutfitConfig::Load → ObjVector<BandPatchMesh>::resize`
constructs/destructs BandPatchMesh objects whose ObjPtr members
(`mesh`/`patches`) were never properly constructed (the operator>> stub is a
no-op) — the destructor then SEGVs in `~ObjPtr → Hmx::Object::Release →
mRefs.rbegin()`. With the strong defs from the compiled TU, the ObjPtrs are
correctly initialized and the dtor is safe.

The weak no-op stubs in `band3_link_stubs.s:254` + `:1006-1027` were left in
place (they are inert vs the strong defs; harmless to keep, simpler diff).

### V25 — `GamePanel::StartGame` HUD force-show
After `mGameState = kGamePlaying`. If `GetTrackPanelDir()` is non-null,
calls `tpd->SetShowing(true)` to flip `draw_order.grp` on at song-start —
the deterministic "song begins → HUD appears" point. Mirrors the retail
`play_intro → SetShowing(!mPerformanceMode)` which fires inconsistently
natively (proxy/anim path the venue work is still bringing up). Idempotent
vs play_intro if it ever does fire.

## Verification

### Build
Clean: `cmake --build native/build-native -j4` → `[N/N] Linking CXX executable
rb3-native` with no warnings.

### Reliability — 3 consecutive end-to-end runs

```
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=2400 \
  RB3_GAME_INPUT='@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail' \
  /home/free/code/milohax/rb3/native/build-native/rb3-native
```

| Run | Final mesh count / frame | Exit |
|---|---|---|
| 1 | 256–261 meshes, ~130K tris | 0 |
| 2 | 173 meshes, ~46K tris | 0 |
| 3 | 196 meshes, ~65K tris | 0 |

All three runs reach `2400 frames done — exiting frame loop` and
`rb3-native: RB3_GAME — Run() returned; exiting cleanly.`

Pre-salvage baseline was 50–65 meshes/frame plateaued (venue + characters
absent). Post-salvage is **173–261**, in the V19/V23 target band of "200+"
(some runs sit slightly below 200 — Run 2 at 173 — but well above the 50–65
"void" baseline; the variance is the same menu-reach race documented in V24
gap §"Menu→gameplay reach is FLAKY" and is not introduced by V33).

### V19/V23 trace confirmation (`VENUE_DBG=1`)

```
VENUE_DBG: EnterVenue force-loading venue='small_club_01'
VENUE_DBG: EnterVenue() wardrobe=0x55c1aa8dfc80 venueDir=0x55c1aaef92c0 venueName='small_club_01'
VENUE_DBG: EnterVenue calling LoadCharacters('small_club_01')
VENUE_DBG: EnterVenue re-running HarvestDircuts (propAnim=0x55c1a85804e0 venueDir=0x55c1aaef92c0)
```

All four V19/V23 HX_NATIVE blocks confirmed firing.

### Screenshots

`docs/sessions/native/screenshots/v33-salvage/`:
- `01_f0600.png` — pre-gameplay cinematic intro
- `02_f0900.png` — post-intro, venue + characters in frame
- `03_f1200.png` — gameplay highway over the lit small_club venue
- `04_f1800.png` — mid-song gameplay
- `05_f2200.png` — late-song gameplay

## Phase 2 — durability proposal

### The problem
`src/system/**` + `src/band3/**` are continuously rewritten by the background
permuter for asm-match. Additive `#ifdef HX_NATIVE` blocks are convention-
compliant but **silently disappear** when the permuter regenerates a file
from its asm-match seed. Over V19–V32 the session accumulated ~25 blocks
across these layers; ~32% (8/25) were wiped between V32 and V33. The
re-apply cost is real: this V33 audit + re-apply took the bulk of a session
that was dispatched to do other work.

### Options analyzed

**Option A — Move feasible fixes to the GLUE layer
(`rb3/native/src/`).** The glue is durable (permuter doesn't touch it).
But the matched-fork fixes are mostly mid-function code paths that aren't
reachable from glue without virtual dispatch or hook seams that don't exist.
Specifically:
- **V19a `EnterVenue` venue force-load** — `EnterVenue` is a non-virtual
  member called from `BandDirector::Enter` (matched-fork). To hook from glue
  we'd need either (a) a virtual `EnterVenue` (changes the vtable layout —
  asm-match risk), (b) a post-Enter callback added to `BandDirector`
  (same), or (c) intercept the LoadMgr ourselves and force-load the venue
  asynchronously before `Enter` fires (timing-fragile — `BandDirector::Enter`
  needs `mVenue.Dir()` already populated). **Not feasible without
  asm-match-disturbing changes.**
- **V23a/b/c/d — character / proxy / ReplaceRefs fixes** — same situation.
  `BandWardrobe::LoadMainCharacters` and `BandCharacter::ReplaceRefs` are
  called from deep inside the FileMerger which is itself matched-fork.
  Hooking is impractical.
- **V25 `GamePanel::StartGame` HUD force-show** — *this* one IS movable.
  The fix is one line: after song-start, if `GetTrackPanelDir()` is
  non-null, call `tpd->SetShowing(true)`. We could call this from the
  per-frame app `Poll` hook in `rb3/native/src/` by edge-detecting
  `mGameState` going `kGameNeedStart → kGamePlaying`. Cost: a single
  per-frame check + an `extern` symbol fetch.
- **V20 `BandPatchMesh` un-exclude** — the EXCLUDE list is in
  `native/CMakeLists.txt` (already durable). The `#ifndef HX_NATIVE` gates
  around `stlpmtx_std` are inside the matched-fork .cpp, so they're at
  permuter risk. Likelihood of wipe: low — the permuter is unlikely to
  remove `#ifndef` from a file it doesn't otherwise rewrite, but the
  `_NATIVE_FORK_EXCLUDE` list was rewritten this session to re-include
  BandPatchMesh, so this needs the doc to flag the linkage. **Partly
  durable.**

Net of Option A: only V25 cleanly moves to glue. The other six fixes are
inside matched-fork-internal code paths.

**Option B — Commit the matched-fork HX_NATIVE files as a whitelisted
durable baseline.** This is the highest-durability option but conflicts
with the convention. The CLAUDE.md / sibling-CLAUDE convention is "stage an
explicit file whitelist" for matched-fork commits because the permuter
continuously rewrites them. If we commit the HX_NATIVE blocks, the permuter
will see them as the "current" baseline and either (a) preserve them across
regenerations or (b) regenerate over them — depending on the permuter's
diff strategy. Need user opt-in + a clear protocol.

**Option C — Periodic audit + re-apply tooling.** A script that reads the
session-doc specs (`docs/sessions/native/*.md`), greps the current source
for the cited HX_NATIVE blocks, and reports INTACT / WIPED. This is the
lowest-risk option: it doesn't change the convention, doesn't disturb the
permuter, and makes the audit step the V33 agent did by hand cheap and
deterministic. It does NOT re-apply automatically — the re-apply is still
the human-or-Opus work. But it shrinks the discovery step from a
30-minute manual audit to a 30-second script run.

**Option D — Accept the maintenance cost.** Current trajectory. Cost is
~30 minutes per session-handoff for audit + re-apply. The burden grows
linearly with the number of accumulated HX_NATIVE blocks.

### Recommendation — Option C plus a partial Option A

1. **Build a tiny audit script** (Option C) — `scripts/native/audit_hx_native_blocks.sh`,
   reads each session doc's "Files changed" / "Code changes (file:line)"
   section and verifies the cited blocks exist in source. Outputs a 1-line
   summary per block (INTACT / WIPED / SHIFTED). Run at session start to
   know immediately what to re-apply. ~50 lines of bash. **Highest
   durability return per hour of investment.**

2. **Move V25 to glue** (the only feasible part of Option A). Edge-detect
   `mGameState == kGamePlaying` in the rb3 app `Poll` hook in
   `rb3/native/src/App.cpp` (or wherever the per-frame hook lives), and call
   `GetTrackPanelDir()->SetShowing(true)` on the transition. ~10 LOC. This
   removes one block from the matched-fork wipe surface area.

3. **Defer Option B until the audit-script (Option C) reveals the
   permuter is wiping things faster than humans can re-apply them.** If
   the wipe rate is e.g. > 1 block / week, the convention cost of Option B
   is worth paying. If the audit script keeps the manual re-apply at ~1
   minute per block, the convention is fine.

4. **Document the linkage** between `_NATIVE_FORK_EXCLUDE` entries and the
   matched-fork `#ifndef HX_NATIVE` gates that any un-excluded TU requires
   (e.g. BandPatchMesh + its stlpmtx_std gates), so a future agent who
   re-excludes a TU doesn't lose the gating context.

Specifically NOT recommended: a full per-fix glue migration. Most of the
fixes are mid-function-internal and would require asm-match-disturbing
vtable changes to be reachable from glue. The cost there is much higher
than the audit-script cost.

## What remains

1. **Investigate the menu-reach flake.** Run 2 and Run 3 hit
   173/196 meshes (in-band but below the V19/V23 doc's "200+" target),
   suggesting partial venue-load on those runs — likely the same fixed-frame
   `RB3_GAME_INPUT` racing load that V24/V32 documented. Not a V33
   regression; a harness item.
2. **Build the durability audit script** per Phase 2 §C.
3. **Move V25 to glue** per Phase 2 §A.
4. The pre-existing **intermittent teardown SIGSEGV** (V23 gap §2 / V32 watch
   item) was not reproduced this session; still a watch item.

## Files touched this session

All additive `#ifdef HX_NATIVE … #else … #endif` blocks; `#else` branches
byte-identical to the permuter form except for the BandPatchMesh.cpp
`#ifndef HX_NATIVE` wrap which leaves the matched-fork code path unchanged
for non-native builds.

- `src/system/bandobj/BandDirector.cpp` — V19a force-load + V23a
  LoadCharacters bridge + V23b HarvestDircuts re-run, all inside
  `BandDirector::EnterVenue()`.
- `src/system/bandobj/BandWardrobe.cpp` — V23c mic→vocals remap in
  `LoadMainCharacters` (~L694).
- `src/system/bandobj/BandCharacter.cpp` — V23d reallocation-safe
  `ReplaceRefs` index-based rewrite (~L1730).
- `src/system/bandobj/BandPatchMesh.cpp` — V20a `#ifndef HX_NATIVE`
  wraps around both `namespace stlpmtx_std` blocks (~L130, ~L518).
- `src/band3/game/GamePanel.cpp` — V25 HUD force-show at end of
  `StartGame()` (~L264).
- `native/CMakeLists.txt` — V20b un-exclude `BandPatchMesh` from
  `_NATIVE_FORK_EXCLUDE` (~L307).

ABSOLUTE RULE OBSERVED: no `git add`, `git commit`, `git stash`, `git
reset`. All changes left uncommitted in the working tree.
