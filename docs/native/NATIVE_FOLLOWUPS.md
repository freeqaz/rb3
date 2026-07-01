# Native-port follow-ups (consolidated 2026-06-19)

Open items surfaced by the 5b / viseme / char-preview investigations (incl. the
earlier Fable runs) and the milo-trace wrap-up. Ordered by value × tractability.
Convention from this port: shared `src/system/*` + `src/band3/*` is Wii+native
code with live objdiff match% — every native fix is `#ifdef HX_NATIVE`-gated (or
proven Wii-codegen-neutral). Verify char/menu changes through a real boot
(`song-end-test.py` / closet harness), not just a 5-frame boot.

## 2026-06-21 — Convergence batch: band-closeup harness + multi-venue audit + scrollbar + venue lighting
**Hub: `docs/native/converge-2026-06-20/`.** Built tooling to measure render convergence vs retail, audited 5 venues, landed the first visible venue-lighting fix.
- ✅ **Deterministic force-band-closeup harness** (`363f6549`/`f842efb4`): 3 native DTA accessors (`{rb3_force_shot}`/`{rb3_director_disable}`/`{rb3_cur_shot}`) + `scripts/native/band-closeup-capture.py` (`pinned=N/N` gate, `drops_band`/`max_band_ratio` verdict). Unblocks all char-pose/venue A/B (the long-standing C7/C8 blocker).
- ✅ **Multi-venue audit → `audit/RANKED-GAPS.md`**: band closeup GEOMETRY is CLEAN across all 5 venues (small_club/big_club/video/arena/festival); convergence frontier moved off skin-deform onto venue/crowd LIGHTING.
- ✅ **Scrollbar GAP 1** (`7a6525fc`): premise was a MEASUREMENT ARTIFACT — `scrollbar_bg`/`scrollbar.mesh` only draw in `song_select` under `[ui.cam]`, NEVER in steady-state gameplay (re-derived 2840 calls / 0 game-active, 3 venues × 3 songs × 5 boots). Shipped a Wii-neutral (objdiff 100%) defensive content-gate anyway (suppresses the contentless case; opt-out `RB3_SCROLLBAR_FIX_OFF`). Adversarially verified LAND.
- ✅ **Venue lighting STEP 1 — arena band (GAP 2) LANDED** (engine `a360e3c`, rb3 pin `7baff7b3`): GX-faithful point falloff `1/(1+d/range)` (was the non-physical `saturate(1-d/range)²` hard cutoff) on the `world.cam` venue path → arena_02's range-55 silhouette spots reach the band (luma 46→63, moody not black). **DC3-safe**: gated via SceneUniforms `pointFalloffMode` (reuses a pad slot, struct size unchanged; default 0 = byte-identical legacy; DC3/game.cam/menu never set it). Opt-out `RB3_VENUE_POINT_FALLOFF_LEGACY=1`. Broader than the plan claimed: brightens point-lit characters venue-WIDE (more correct, no regression observed). Adversarially verified.
- ✅ **Big_club white crowd (GAP 3) LANDED** (engine `ada6e56`, rb3 pin `d31c3a10`): the visible white was ~9000 **impostor-billboard quads**/frame (empty mesh name → prior text-heuristic skipped them) with a near-white baked diffuse — NOT skinned chars, NOT lighting (white% invariant across all light knobs). Dim the crowd/impostor base color (`RB3_CROWD_DIM` 0.10, `world.cam`-gated, band + HUD-text hard-excluded): crowd white% 17→0, luma into 20s-30s, **band luma 24.6→24.9 unchanged**. DC3-safe (RB3-only `Rnd_Wgpu_RB3.cpp`). Opt-out `RB3_CROWD_DIM_OFF`. Adversarially verified.
- ✅ **Teal highway watermark (GAP A) LANDED** (engine `b8f3cfa`, rb3 pin `d31c3a10`): the "teal overlay" is the **AUTHORED `surface.mat` watermark** (clef filigree — retail has it too) rendered ~3.4× too bright; dim its emissive (`RB3_HIGHWAY_WATERMARK_DIM` 0.30, `game.cam`+`surface.mat` only) → reads like retail's faint ghost, **pattern preserved**, gems/now-bar/lanes/HUD untouched. DC3-safe (RB3-only TU; A2 desat measure-gated out → shared shader untouched). Opt-out `RB3_HIGHWAY_WATERMARK_OFF`. Adversarially verified.
- ✅ **Festival `*_screenmask` white-blank (GAP 4) — Option A LANDED** (engine `998b8734`, rb3 pin `201a8111`): the screenmask diffuse (`crowd_mass.tex`) is a render target fed ONLY by a Bink movie with no native decoder → never painted → `DrawRect` blitted the 1×1 white fallback full-screen (luma 202, white% 83). Now SKIPS the unpainted-RT quad → gameplay/band visible over a black background (luma ~13, white% 0.4). DC3-safe (RB3-only TU); a painted RT (sky-dome) keeps a view → NOT skipped; null/non-RT diffuse (UI rects) unchanged. Opt-out `RB3_SCREENMASK_FALLBACK_OFF`. Adversarially verified LAND_WITH_NOTES.

### Deferred backlog — RE-ASSESSED 2026-06-30 (`docs/native/converge-2026-06-20/deferred/DEFERRED-PLAN.md`)
Every genuinely-visible gap is now landed. The remainder is consciously **accepted/closed** — only the festival Option-B stays a real open item:
- **GAP 4 festival crowd backdrop — Option B CLOSED: jumbotron biks were CUT from the 360 build (DEFINITIVE, `docs/native/bink-albumart-2026-06-30/disc-extract.md`):** Option A (above, landed) killed the white flash. Option B (decode the movie into the crowd RT) is render-code-bounded (~1 day, RB3-only/DC3-safe) but the source movies are **unobtainable**: we reconstructed the FULL retail RB3 Xbox 360 disc (`Rock Band 3.zip` GoD → byte-exact `god2iso.py` → `xdvdfs-cli`) and read it end-to-end — the XDVDFS filesystem is **only the ARK** (17 files, NO loose `world/` tree; the earlier "loose-on-disc" premise is REFUTED), the ARK `.hdr` is byte-identical to `extracted-xbox-full` (6 biks total, none mass-crowd), the patch ARK has 0, and the festival milos only *reference* `fest{1,2}_mass*.bik` as external paths (zero embedded Bink magic). So the jumbotron movies were **cut from the 360 build** (PS3-only or cut entirely; no PS3 disc on hand). **Option A is therefore plausibly FAITHFUL to retail 360** (no 360 movie ever existed to show). Only a PS3 disc that actually contains them could unblock — accept Option A otherwise. Reusable tooling kept at `/home/free/rb3-disc-extract/` (`god2iso.py`/`milo_decompress.py`).
- **Footwear `_skin.2` fling (GAP 5) — ACCEPT:** a rigid ankle anchor CANNOT meet the drop-under-cap gate — anchoring nulls translation but the native **C8 rotation-basis** divergence still smears verts to 200-460u (worse full-screen slabs); off-frame in 100% of closeups + wardrobe-random. Correctly drop-guarded. Reopen only WITH the C8 fix.
- **Crowd/extras servo shards (GAP 6) — ACCEPT (followups CORRECTION):** the prior "no non-Band rebake hook exists" claim was **FALSE** — `Crowd.cpp:911 RebindCrowdCharBonesToOwnSkeleton` already ships and fixes crowd bodies (ratio <2, 0 drops). Residual = 3 tiny accessory meshes (eyebrows/hair/`clap`, the separate `extras.fm` path) on ≤3 of ~292 instances, masked/distant/crowd-dimmed (<2% drop). An extras-path rebake = whole-292 blast radius for zero visible gain; `clap` shares the C8 fling. Accept-as-dropped.
- **Venue lighting STEP 2 impostor-crowd env gate — CLOSE_OBSOLETE** (tag `converge-step2-crowd-wip`=`bae1aae`, do NOT push): GAP B(a) crowd-dim already fixes the visible crowd; STEP 2 + B(a) act on the SAME serial pipeline (STEP 2 dims the impostor-RT bake, B(a) dims the final billboard ×0.10) → they MULTIPLY → stacking = near-black crowd. B(a)'s own verify already showed STEP 2 doesn't move the visible crowd. `RB3_CROWD_DIM_OFF` is the lever.
- **STEP 1 venue exposure tuning — ACCEPT:** the GX-falloff arena fix is a clear win (black% 53-62→24, region luma 31→61, moody not flooded); `sVenuePointExposure=0.70` is retail-consistent. `RB3_VENUE_POINT_EXPOSURE`/`RB3_VENUE_GREY_KEY` are future knobs if an arena/big_club retail frame surfaces.
- **C8 / feet-in-floor — does NOT reproduce on RB3 master (NOT a bug; the prior "STILL UNFIXED / 4u sink / 186u fling / C8-blocked" framing is STALE).** Fresh deterministic measurement + cross-venue confidence capture (`docs/native/c8-bink-2026-06-30/`, the new harness + the now-available Xbox ground truth): RB3 band feet are FAITHFUL — toe ~0 / ankle ~4.3 matching Xbox (toe 0.006-0.53, ankle 4.1-5.4); 345 player-local samples (arena_02+festival_01) worst toe **−0.7**, feet planted on the floor; the leg chain composes perfectly (det=1, dMag=0), IK inert, the 186u fling does NOT reproduce. The accumulated skinning fixes (`491288ec` rotation-basis + the V24 band-aware guard + the 4 rest-rebinds) already absorbed it. **So the footwear/crowd `_skin.2` drops above are transient off-frame V24-handled false-positives, NOT a steady C8 smear** — there is NO real C8 bug gating visible RB3 char fidelity, and porting DC3's foot-plant would assert an already-satisfied pose (do NOT). The DC3 native bug ([[project_dc3_feet_in_floor_anim]]) is SEPARATE and still open (DC3 ankle collapses to −4.3); **RB3 ≠ DC3 here.**

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
- **Residual — RESOLVED/CLASSIFIED (2026-06-21, `docs/native/converge-2026-06-20/`).**
  Built the deterministic band-closeup harness (below) and used it to settle the masked
  shard-guard residual definitively. **Closeup-visible band rendering gets a CLEAN BILL** —
  no band garment in an actual closeup framing (jackets/arms/heads/gloves) is dropped
  (`gloves_resource.1.mesh` closest at ratio 3.97, saved by the 40u world-floor). The three
  masked drops break down as: (1) `scrollbar_bg.mesh` = **71% of all drops, a UI scrollbar
  leaked into the gameplay 3D scene** (not a skin-deform bug; the guard-drop is
  *accidentally correct* since retail doesn't draw it either — real fix is UI-draw-tree
  scoping, cf. the MusicLibrary stale-slot family); (2) `clap.mesh` + `male_extras*` =
  distant **crowd/vignette filler** on a non-band servo skeleton no existing rebind reaches
  (high blast-radius, ~nil payoff → accept-as-dropped); (3) `lowtopsneaks_skin` /
  `saddleshoe` band **footwear thin-skin** = a *real* band gap (explodes, guard drops it,
  no existing rebind covers it) but **off-frame in every club closeup the auto-director
  uses → LOW ROI**; engine note warns a translation-only anchor could draw *worse*. Defer
  (or bundle with a general foot / C8 pose-basis fix).
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
No fix landed (none safe — metric is 0). The named blocker — a **deterministic "force
band closeup" harness** — is now ✅ BUILT (2026-06-21, `363f6549`+`f842efb4`,
`docs/native/converge-2026-06-20/`): 3 native-only DTA accessors (`rb3_force_shot` /
`rb3_director_disable` / `rb3_cur_shot`) pin a venue `BandCamShot` (`mDisabled=1` then
`ForceShot`) + `scripts/native/band-closeup-capture.py` (matched-`(shot,songMs)` A/B,
`pinned=N/N` determinism gate, `drops_band`/`max_band_ratio` verdict). Adversarially
verified (LAND, pinned N/N across 4 runs + negative control). Using it, the C7/C8
closeup band rendering gets a **clean bill** (no band garment dropped in a closeup;
`gloves` closest at 3.97). Adjacent-but-separate: guitar-string over-cap = the
`*_strings` fix `2f393eaa` (resolved); band footwear thin-skin = real but off-frame
(see the crowd-origin residual note above); crowd/extras + UI V24 drops (known). Ref:
the findings doc, `491288ec`/`3c02e08b`/`0f2f5df2`, `[[project_char_skinning_deform]]`.

---
### Done this arc (for context, not follow-ups)
- 5b head-shaper un-gated + ABA freed-addr-guard UAF fix (`c99e28af`).
- char-Load byte-correctness gtest (`6e67c2e3`); 5b/theme-B docs fact-check (`53d46a5e`).
- `MILO_TRY/MILO_CATCH` LP64 message-truncation fix + gtest (`2c5e68c6`).
- char-preview + guest-profile default-on; domino-② fixed (`65f7f0e6`, UPDATE 9).
