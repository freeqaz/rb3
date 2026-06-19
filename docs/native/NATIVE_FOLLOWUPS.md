# Native-port follow-ups (consolidated 2026-06-19)

Open items surfaced by the 5b / viseme / char-preview investigations (incl. the
earlier Fable runs) and the milo-trace wrap-up. Ordered by value × tractability.
Convention from this port: shared `src/system/*` + `src/band3/*` is Wii+native
code with live objdiff match% — every native fix is `#ifdef HX_NATIVE`-gated (or
proven Wii-codegen-neutral). Verify char/menu changes through a real boot
(`song-end-test.py` / closet harness), not just a 5-frame boot.

## P1 — ProfilePicture in-place heap corruption (REAL memory-safety bug)
**Status: root cause OPEN; symptom mitigated.** The char-customize composite
overruns into adjacent heap: gdb during the domino-② work saw the guest's
per-profile `ProfilePicture` block overwritten *in place* (not freed) — the
clobbered bytes literally contained UI/locale strings ("You have changed the
storage devices…", "Select Style"). UPDATE 9 (`65f7f0e6`) only made it
*unreachable* via the song_select path (`BandProfile::GetPictureTex()` returns
null on native so the DTA never derefs the clobbered block). The underlying
out-of-bounds **write** still exists and can corrupt other allocations.
- Likely source: the char-composite draw path — `OutfitConfig::DrawPreClear`
  (`OutfitConfig.cpp:908`) → `BandPatchMesh::ProjectPatches`
  (`BandPatchMesh.cpp:943`, patch projection / `sRawCollide` region). The `.text`
  `sRawCollide` write was already fixed (`ed9a3e92`); this is a *different*,
  heap-side overrun.
- Approach: build `rb3-native` with **AddressSanitizer**, reproduce the female
  tattooed-char composite (default-on guest + preview, drive to the closet /
  song_select), let ASan report the `heap-buffer-overflow` write + the overrun
  buffer, root-cause, fix `HX_NATIVE`-gated.
- Ref: `docs/native/CUSTOMIZE_PREVIEW_FINDINGS_2026-06-09.md` UPDATE 9.

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
