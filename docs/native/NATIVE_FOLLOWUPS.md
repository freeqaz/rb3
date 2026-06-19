# Native-port follow-ups (consolidated 2026-06-19)

Open items surfaced by the 5b / viseme / char-preview investigations (incl. the
earlier Fable runs) and the milo-trace wrap-up. Ordered by value × tractability.
Convention from this port: shared `src/system/*` + `src/band3/*` is Wii+native
code with live objdiff match% — every native fix is `#ifdef HX_NATIVE`-gated (or
proven Wii-codegen-neutral). Verify char/menu changes through a real boot
(`song-end-test.py` / closet harness), not just a 5-frame boot.

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
closet flow. Two *unrelated* pre-existing latent ASan findings remain (already
filed in the viseme plan, NOT the ProfilePicture corruption): `CharCollide::Deform`
stack-use-after-scope (`CharCollide.cpp:196-214`, hoist `upX`/`upY` above the if)
and `BandRetargetVignette::EnterDir` global-buffer-overflow READ
(`BandRetargetVignette.cpp:56`, 8 bytes past `sIkfs[96]`).
- Ref: `3d00d1dd`, `docs/native/CUSTOMIZE_PREVIEW_FINDINGS_2026-06-09.md` UPDATE 9.

## P2 — MemMgr `_MemAlloc` ABA freed-addr range-erase (defensive hardening)
**Status: spec'd, not implemented.** The 5b ABA fix (`c99e28af`,
`HxNoteReusedAddr(this)` in `Hmx::Object::Object()`) clears the `HxAddrWasFreed`
mark for **offset-0** Hmx::Object subobjects. A freed `CharBonesObject` whose
*interior* (non-offset-0) memory is later recycled isn't covered. Fable's plan
recommends a range-erase at allocation: under `#ifdef HX_NATIVE` in `_MemAlloc`
(`src/system/utl/MemMgr.cpp:1030`), erase every freed-set entry in `[p, p+size)`.
Belt-and-suspenders for the same ABA class; only strictly needed if a residual
`[HXGUARD-FP]`-class crash reappears.
- Ref: `docs/native/char-load-5b/viseme-uaf-plan.md` ("Optional hardening").

## P2 — CharProvider `pLocalChar` assert on back→manage_band
**Status: OPEN.** The `back→manage_band` native nav route hits
`MILO_ASSERT(pLocalChar, 0x8F)` at `src/band3/meta_band/CharProvider.cpp:79` —
`dynamic_cast<TourCharLocal *>(pCurrentChar)` returns null (the current char on
native isn't a `TourCharLocal`). Repro: reach manage_band, press back. Fix likely
an `HX_NATIVE` guard / correct the current-char wiring so it's a `TourCharLocal`
(or tolerate null). Tracked separately from domino-② in UPDATE 9.

## P3 — milo-trace wrapper-ext cherry-pick (loose end)
**Status: landing this session.** Commit `20ce6328` ("W5 wrapper-ext capture
sites, HX_NATIVE env-gated") sits on branch `wt-mtrace-wrap-ext`; never landed on
master because master was perpetually checked-out-with-edits. Master is free now
→ cherry-pick.

## P1-but-DEEP — C7/C8 char rotation-basis shard (gameplay head/hair/hands)
**Status: actively iterated, root cause OPEN.** Gameplay head/hair/hands/garments
smear to 200–460u world extents because the native pose pipeline diverges in the
**rotation basis** (translation now anchored, but far-from-bone verts smear by
R·sin θ). Latest: `491288ec` "capture rest pose in CHARACTER space, not world
space (C8 root cause)"; reload-re-entrancy fixes are default-ON (`3c02e08b`), the
own==bound rest-rebake is opt-in `RB3_BOUND_REBAKE` (default OFF, `0f2f5df2`).
Deep, multi-commit; the `CharBones`/pose-pipeline basis is the remaining
root-cause. Defer unless explicitly prioritized — broad/high-risk and recently
active. Ref: `[[project_char_skinning_deform]]`, the C8 commit chain.

---
### Done this arc (for context, not follow-ups)
- 5b head-shaper un-gated + ABA freed-addr-guard UAF fix (`c99e28af`).
- char-Load byte-correctness gtest (`6e67c2e3`); 5b/theme-B docs fact-check (`53d46a5e`).
- `MILO_TRY/MILO_CATCH` LP64 message-truncation fix + gtest (`2c5e68c6`).
- char-preview + guest-profile default-on; domino-② fixed (`65f7f0e6`, UPDATE 9).
