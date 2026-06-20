# Native-port follow-ups (consolidated 2026-06-19)

Open items surfaced by the 5b / viseme / char-preview investigations (incl. the
earlier Fable runs) and the milo-trace wrap-up. Ordered by value × tractability.
Convention from this port: shared `src/system/*` + `src/band3/*` is Wii+native
code with live objdiff match% — every native fix is `#ifdef HX_NATIVE`-gated (or
proven Wii-codegen-neutral). Verify char/menu changes through a real boot
(`song-end-test.py` / closet harness), not just a 5-frame boot.

## 2026-06-20 — "Crowd + drum kit congregate at origin" = band instrument `*_strings` skin explosion (FIXED `2f393eaa`)
**Status: ✅ FIXED + verified.** The user-reported "crowd + drum kit (and many items)
congregating at origin" was investigated end-to-end with a new debug tool and turned
out to be **NOT a placement bug at all** — a 2D misattribution. Ground truth (new
`{rb3_pos_dump}` DTA tool, `fe6b5a73`/`5ebcf887`, walks the live object tree + the
`BandDirector` venue dir's `mCrowds`):
- **Band placement WORKS** — 4 member roots spread across the stage (player3 byte-stable
  at (14.43,146.13,13.18); all z≈13.2 floor), each kit/`mInstDir` co-located with its
  drummer. `band_at_origin=0/4`.
- **Audience placement WORKS** — 300 `WorldCrowd` members spread across the venue floor
  (x∈[-161,161], y∈[-297.6,-22.1], z∈[68.6,74.5]), `crowd_at_origin=0/300`.
- All 5 PLAN hypotheses (H1 `SyncDir` reparent / H2 root-never-placed / H4 crowd decode /
  H5 merge / H3 proxies) **REFUTED** by the live data.
- **Actual cause:** the engine's V24 `[SHARD_GUARD]` (`Rnd_Wgpu_RB3.cpp:4924-5141`) was
  *correctly* DROPPING the band lead-guitar `*_strings.mesh` of the "brain"-class special
  guitars (chainsaw / guitar_brain), which **explode to a ~136u world AABB (ratio ~5.0)**.
  Those guitars author their string-bend rig on the CHARACTER skeleton
  (`skeleton_unshared.milo`) and have no own-resource neck; on native the per-member
  skeleton basis diverges from the authored inverse-bind → the rigid-authored strings
  smear. Same **char-skinning-deform family**, now on `mInstDir` (which the existing
  `RebindOutfitBonesToOwnSkeleton` deliberately excludes). The visible "pile/smear" with
  the guard OFF was a dark exploded mass sweeping in from the left — *not* origin
  placement (bone0 |95-100u|, at the staged guitar).
- **Fix (`2f393eaa`, HX_NATIVE, Wii byte-neutral — Poll 98.6% unchanged):** new
  `BandCharacter::RebindInstStringsToRestBasis()` called from `Poll()` after
  `mInstDir->Poll()`; rigid-anchors every `*_strings` bone to `bone_bridge` and rebakes
  the offset so the mesh rides one rigid bone (world AABB == bind AABB, ratio→~1.0),
  matching the FINE own-resource instruments. Narrowly gated (name ends `_strings.mesh`
  AND a bone resolves to `skeleton_unshared.milo`) so the FINE instruments are never
  touched. Sets `mNativeBonesRebound` so the engine guard/clamp skip it. Default-ON,
  opt-out `RB3_NO_INST_REBIND=1` (`RB3_INST_STRINGS_MODE=rebake` for the bend-preserving
  A/B). Engine unchanged (guard stays as backstop). Measured: ratio 5.0→1.0, `dir=instrument`
  drops 1984→0, left-edge smear gone (screenshots), FINE instruments stay 1.0, audience
  unaffected. Adversarially verified (LAND, high confidence).
- **Residual (separate, out-of-scope, NOT visibly wrong):** `scrollbar_bg.mesh` +
  `male_extras*` (venue vignette extras) + a song-dependent band-outfit shoe skin
  (`lowtopsneaks_skin`) still trip the shard guard (masked → silently dropped, screen
  looks clean). Same skin-deform family; address as a follow-up only if it becomes visible.
- Tool: `{rb3_pos_dump}` + `scripts/native/crowd-origin-posdump.py`. Docs:
  `docs/native/crowd-origin/` (PLAN, scouts, measure-results, verify-verdict,
  deform-investigation, fix-impl, audience-measure + `shots/`).

## P1 — ProfilePicture in-place heap corruption (REAL memory-safety bug)
**Status: RESOLVED — this item was STALE.** The OOB write *was* the BandPatchMesh
MeshVert LP64 arena bug, and it was already root-caused + fixed by `3d00d1dd`
(2026-06-09 11:35) — which landed ~2h **after** the UPDATE-9 doc snapshot
(`65f7f0e6`, 09:26), so UPDATE 9 (and this follow-up, copied from it) still listed
it open. `3d00d1dd`'s own message names it exactly: "the char-mesh heap corruption
that (with char-preview on) clobbered the ProfilePicture behind the domino-②
song_select UAF and intermittently aborted boots." `MeshVert` begins with a
`const RndMesh::Vert*` (8 bytes on LP64 vs 4 on Wii), so the hardcoded Wii arena
offsets (face-list `0x32`, slot stride `0x38`) scribbled a face index into the
high halfword of `MeshVert::unk2c` (the twin cursor), producing the documented
`0x<faceidx>FFFF` cursor; the later twin-list walk then subscripts
`mMeshVerts[]` wildly OOB → adjacent-heap (ProfilePicture) clobber. The fix
HX_NATIVE-derives the offsets via `offsetof` (Wii `#else` byte-identical) and adds
a defensive out-of-range face-index skip, with `native/tests/test_bandpatchmesh.cpp`.

**Verified 2026-06-19 (ASan):** built `rb3-native` + `rb3-tests` with
`-fsanitize=address`, drove guest+preview default-on to the closet
(`asan-closet-repro.py`) AND quickplay gameplay (band composite, `song-end-test.py`)
— **zero `heap-buffer-overflow`** on the master fix. A negative control (forcing
the old Wii literals back on the host) reproduces the exact `unk2c = 0x<faceidx>FFFF`
corruption (e.g. `0x3ffff`, `0x6ffff`) and the gtest fails; with the fix it passes.
Note: `ProjectPatches`/`SetMeshVerts` do **not** execute on the headless render
path (Null Dawn adapter) — the gtest is the reproduction vehicle, not the live
closet flow. Two *unrelated* pre-existing latent ASan findings (NOT the
ProfilePicture corruption) were filed here and are now **✅ FIXED (`090f7914`,
2026-06-19):** `CharCollide::Deform` stack-use-after-scope (`CharCollide.cpp:195`,
`upX`/`upY` hoisted above the if, HX_NATIVE) and `BandRetargetVignette::EnterDir`
global-buffer-overflow READ (`BandRetargetVignette.cpp:9` — `sIkfs[]` was missing
its null terminator; the target symbol is 13 pointers, restored → `sIkfs` objdiff
100%, match-positive). ASan-verified: fresh closet + gameplay runs that exercise
both paths show 0 stack-use-after-scope / 0 global-buffer-overflow.
- Ref: `3d00d1dd`, `090f7914`, `docs/native/CUSTOMIZE_PREVIEW_FINDINGS_2026-06-09.md` UPDATE 9.

### Known ASan noise (NOT a bug — do not "fix")
Under ASan, the gameplay path emits a flood of `alloc-dealloc-mismatch (malloc vs
operator delete)` — the engine's custom `MemMgr` allocates via `posix_memalign`
(malloc-family) while objects free via `operator delete`. Benign on native (MemMgr
owns its memory; ASan just doesn't model the new/delete override). Suppress with
`ASAN_OPTIONS=alloc_dealloc_mismatch=0` (the `asan-closet-repro.py` harness already
does). Don't chase it.

## P2 — MemMgr `_MemAlloc` ABA freed-addr range-erase (defensive hardening)
**Status: ✅ DONE (`884f257a`, 2026-06-19).** `HxNoteFreedRangeReused(p,n)` added in
`Object.cpp` (HX_NATIVE, EOF block to keep `DECOMP_FORCEACTIVE` `__LINE__` symbols
byte-identical) — a reentrancy-guarded `lower_bound` range-erase over the freed-set;
called from `_MemAlloc` (`MemMgr.cpp:1060`) on every native allocation, erasing
`[p, p+want)`. Verified: CharLoad5b 4/4, rb3-tests 21/21, song-end-test reaches
game_screen (the head-shaper ABA scenario), Object.o + MemMgr.o byte-identical. The 5b ABA fix (`c99e28af`,
`HxNoteReusedAddr(this)` in `Hmx::Object::Object()`) clears the `HxAddrWasFreed`
mark for **offset-0** Hmx::Object subobjects. A freed `CharBonesObject` whose
*interior* (non-offset-0) memory is later recycled isn't covered. Fable's plan
recommends a range-erase at allocation: under `#ifdef HX_NATIVE` in `_MemAlloc`
(`src/system/utl/MemMgr.cpp:1030`), erase every freed-set entry in `[p, p+size)`.
Belt-and-suspenders for the same ABA class; only strictly needed if a residual
`[HXGUARD-FP]`-class crash reappears.
- Ref: `docs/native/char-load-5b/viseme-uaf-plan.md` ("Optional hardening").

## P2 — CharProvider `pLocalChar` assert on back→manage_band
**Status: ✅ DONE (`02bf3f10`, 2026-06-19).** On native the offline guest current
char is a `PrefabChar` that the guest hack flips customizable (`gPrefabIsCustomizable`),
so `dynamic_cast<TourCharLocal*>` returns null and the assert fired. Fix (HX_NATIVE):
a `PrefabChar` isn't a saved custom char → leave `haschar=false` on null cast instead
of asserting (`#else` byte-identical, objdiff 100%). Verified: back→manage_band reaches
manage_band_screen and renders the band menu, no SIGABRT. (Side note: the route is only
reachable today by forcing a profile — `Profile::SetSaveState` does NOT fire
`ProfileChangedMsg`, contra UPDATE 9, so the guest stays non-primary and
`customize_band.btn` normally routes to the sign-in dialog; the assert was latent but real.)
The original-OPEN detail: The `back→manage_band` native nav route hit
`MILO_ASSERT(pLocalChar, 0x8F)` at `src/band3/meta_band/CharProvider.cpp:79` —
`dynamic_cast<TourCharLocal *>(pCurrentChar)` returns null (the current char on
native isn't a `TourCharLocal`). Repro: reach manage_band, press back. Fix likely
an `HX_NATIVE` guard / correct the current-char wiring so it's a `TourCharLocal`
(or tolerate null). Tracked separately from domino-② in UPDATE 9.

## P3 — milo-trace wrapper-ext cherry-pick (loose end)
**Status: ✅ DONE (`c3d72f40`, 2026-06-19).** Commit `20ce6328` ("W5 wrapper-ext
capture sites, HX_NATIVE env-gated", `BinStream.cpp`/`ChunkStream.cpp`/`mtrace_wrap.h`)
cherry-picked to master once it was free. All sites `#ifdef HX_NATIVE` (verified) →
Wii-neutral; `RB3_MTRACE_WRAP` env-gated off by default.

## P1-but-DEEP — C7/C8 char rotation-basis shard (gameplay head/hair/hands)
**Status: ✅ rotation-basis FIXED (`491288ec`); the named "left-limb IK mispose"
residual NO LONGER REPRODUCES (2026-06-20 investigation).** The C8 rotation-basis
root cause was a rest-bake *space* error (rest captured from `WorldXfm()` → verts on
a `|placement|` lever → R·sinθ smear); `491288ec` captures rest relative to the member
root (locality 27-60u → 5-12u). `491288ec` named a remaining "left-limb IK mispose"
(band V24 guard drops 20.4/frame IK-on vs 4.9 IK-off); a 2026-06-20 deep investigation
(`docs/native/c7c8-ik-mispose-findings-2026-06-20.md`) found that **band-garment
guard-drops now read 0** (IK on AND off) — the C8 fix + the relaxed band-aware V24
guard (engine pin `1010f5f`) already absorbed them. Bisection **refuted** the
CharIKFingers suspect (never runs in guitar gameplay); the candidate DC3
CharForeTwist/UpperTwist `mLocalXfm` back-compute is a **no-op on RB3** (zero cascade
drift); the "176u smear" was a camera-framed guitar-string artifact (IK-independent).
No fix landed (none safe — metric is 0). **Don't re-chase without first building a
deterministic "force band closeup" harness** (the venue director's nondeterministic
cuts make per-run guard metrics unreliable — the real blocker to any further C7/C8
measurement). Adjacent-but-separate: guitar-string over-cap (fret-hand skinning,
low-pri); crowd/extras + UI V24 drops (known). Ref: the findings doc,
`491288ec`/`3c02e08b`/`0f2f5df2`, `[[project_char_skinning_deform]]`.

---
### Done this arc (for context, not follow-ups)
- 5b head-shaper un-gated + ABA freed-addr-guard UAF fix (`c99e28af`).
- char-Load byte-correctness gtest (`6e67c2e3`); 5b/theme-B docs fact-check (`53d46a5e`).
- `MILO_TRY/MILO_CATCH` LP64 message-truncation fix + gtest (`2c5e68c6`).
- char-preview + guest-profile default-on; domino-② fixed (`65f7f0e6`, UPDATE 9).
