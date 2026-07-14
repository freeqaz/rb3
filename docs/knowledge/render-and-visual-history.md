# Render & Visual Fix History

This document consolidates completed/closed render and visual investigations for RB3's native and web port, migrated from agent-memory notes (point-in-time observations, mostly 2026-06 to 2026-07). Each section captures a symptom, root cause, the fix (file/flag/default/commit), final status, and any "do not re-investigate" warnings. Identifiers (function names, `RB3_NO_*` flags, file paths, commit hashes, defaults) are load-bearing — preserved verbatim where possible. Where a repo doc exists it is linked; those docs are the authoritative expansions. Note that commit hashes and file:line citations are historical and may have drifted; verify against current code before asserting as fact.

Shared context that recurs throughout:
- **Native-first debugging.** Web bugs reproduce identically in `rb3-native` (headless, ~3s rebuilds via `cmake --build native/build-native --target rb3-native`) because web and native share the same `Rnd_Wgpu_RB3.cpp` (BandRnd) backend at the same `MILO_ENGINE_PIN`. Always confirm the final fix once on web. See "Native visual repro loop" below.
- **DC3-safe gating.** `milo-native-engine` is shared with DC3. Engine-wide shader/uniform changes must default to the exact legacy path (byte-identical, DC3 compiles the other `Rnd_Wgpu.cpp` backend) and enable new behavior only on the RB3 path. Most fixes are `#ifdef HX_NATIVE` and Wii-byte-identical, default-on with an `RB3_*_OFF` opt-out.
- **Skinning `SYS-1` family.** Skinned meshes have `obj.world == identity`; the palette is `BoneOffsetAt(b) * boneWorldXfm` (absolute world). A wrong-instance bind or wrong-basis inverse-bind explodes thin geometry as `R·sin(θ)` shards. This one systemic cause underlies most character render bugs below.

---

## Render polish campaign (2026-06-11 → 06-19)

Multi-wave ultracode campaign fixing 8 user-reported rendering gaps in the native/web port. Hub: [docs/native/render-polish-2026-06-11/PLAN.md](../native/render-polish-2026-06-11/PLAN.md) (status log authoritative); close-out `CAMPAIGN_SUMMARY.md`. 8 waves, all issues + surfaced regressions fixed and independently reviewed; Wii match byte-identical-or-improved throughout. Final: rb3 master `0db7c2f8`, engine `1010f5f`; 83 songs play native + web.

Method that worked: scout → implement (worktree) → orchestrator lands (sequential engine cherry-pick + one pin bump/wave) → independent review (pre-fix A/B binary, separate from implementer) → next wave. Adversarial re-diagnosis repeatedly overturned the first hypothesis.

Durable fixes and findings by wave:
- **Wave 1-2 (landed rb3 `5248158d..cca1869a`, engine `eda796d..469c550`):** highway `camRotX −4→0` (centered); vocal/All-Instruments crash (`TheNet.mSession` wire + `VocalTrack::UpdateScrolling` `&v[0]` guards — the real blocker, not the Singer site); diff-grid icon centering (Text.cpp wide-atlas `CellDiff` top-anchored); crowd inverse-bind rebake in `WorldCrowd::Draw3DChars` (crowd was palette-poisoned by a second same-named resident vignette Character); `Part.cpp InitParticle` decomp fix (95.3→97.7 — menu "green slab" was runaway street-fog particles, not `neon_arcade.mesh`); engine bloom outer-halo-only (`max(bloom−raw,0)` — gems were washed white); unlit `mUseEnviron` + emissive-all-cams menu lighting.
- **Wave 3 — C8 garments (landed master `491288ec`):** invisible garments were a **rest-bake SPACE error**, not a rotation-basis sign-flip. Rebind captured rest in WORLD space (incl. stage placement) vs model-space verts → verts rode a `|placement|`-length lever → `R·sinθ` smear (200-460u) → V24 guard dropped them. Fix = capture rest in CHARACTER space (`L_rest = world·inv(rootWorld)`) + never capture while a clip plays. Drop-rate metric is the authoritative garment gate; lit-venue hero closeups are unreliable (venue pink-wash).
- **Wave 4 (engine `1abd595..58254f7`):** fret-sphere (`gem_smasher_glow.mat` was a halo-bloom SOURCE blown to a ~110px sphere → exclude from `IsHaloSourceMat`, opt-in `RB3_SMASHER_HALO=1`, boost ×2→×1.25); venue-blowout softClipLighting (Reinhard knee); menu-fog (`BandRnd::DrawParticles` dropped material register color → thin street-fog drew 10× opacity, fold matColor into vtx color); endgame-abort (`MetaPerformer::UpdateScores` `MILO_ASSERT(TheNet.GetServer())` OSFatal'd → `TheNet.mServer=&TheServer` offline server, 1 line).
- **Wave 5 — pose-fling SOLVED (engine `15ce606`):** NOT IK, NOT decode — a **STALE `mWorldXfm` cache** on per-member skeleton LEAF bones. Multi-pass posing left a leaf ankle's cached world composed against an earlier flung intermediate, never re-read. Fix = force fresh top-down `WorldXfm_Force` of each bone's parent chain in `BandRnd::DrawMesh` before reading the bone palette. Drops 186073→10027. Opt-out `RB3_NO_SKEL_WORLDFIX=1`. (3-wave arc: IK → pose-pipeline → WorldXfm-cache; the LOCAL was always right.) Also: first-frame flash = postproc composite over-brightening during song-start reveal → Reinhard soft-clip the composite output in `fs_postproc` (opt-out `RB3_PP_OFF`); songselect uninit `SongStatusMgr::mCachedTotalStars` POD → zero in ctor; menu-contrast ambient floor lowered; `PassiveMessageQueue::Poll` timer test was INVERTED (`if(running)` vs Bank8 `!running`) → toast queue never drained → flip byte-faithful 98.8%→100%.
- **Wave 6-7 — chart wiring (the unlock):** native port went from 3 songs → all 83 play. Charts were never missing — they live in `orig-assets/extracted-xbox-full`, not the loader's 3-song dev slice. Fix = symlink 80 `.mid` into `extracted/songs` (machine-local, gitignored) + committed `SongParser::ParseAndStripLyricText` pointer-underflow SIGSEGV fix. Also `SerialGroupSeqInst::Poll` off-by-one (99.38→100%).
- **Wave 8 — wrap-up:** full 83 play in-browser; env-tunable `RB3_VENUE_POINT/DIR_EXPOSURE`; band-aware V24 guard (`RB3_BAND_SHARD_*`) → character work fully closed; crowd venue override Fix C (`EnterVenue` honors `MetaPerformer` venue override).

**Don't re-open:** the C8/IK pose probes were made redundant by the Wave-5 WorldXfm-cache solve (in engine `15ce606`). Non-blocking opens at close: crowd 2D imposters (designed, see below), `festival_01` venue SIGSEGV, menu-contrast taste.

---

## A1 — hit-flame FX (gameplay particles)

**Symptom:** gameplay smasher hit-flame FX absent. **Two real root causes, not the roadmap's "FX absent" premise:**
1. **Visibility (fixed):** per-lane smasher hit-FX `.part` systems collect into `after_gems.grp` wrapped by `after_hide.grp`; `GemTrackDir.cpp:497-498` does `afterhide->SetShowing(false)` (retail composites via `smasher_fx.grp`, inert on native BandRnd). Fix = additive `#ifdef HX_NATIVE` keeping `after_hide.grp` shown.
2. **Rendering (the real gap):** `DrawParticlesBillboard` was a weak no-op stub → `RndParticleSys` never rendered. DC3's `Part_Wgpu.cpp` is NOT compiled for RB3 (RB3 uses `Rnd_Wgpu_RB3.cpp`, BandRnd).

**Fix (2026-06-02, A1 done):** implemented `BandRnd::DrawParticles` in `Rnd_Wgpu_RB3.cpp` — camera-facing billboard quads (`right=camXfm.m.x`, `up=camXfm.m.z`), `Multiply(p->Pos3(), sys->RelativeXfm(), worldPos)` per particle (relative systems need `RelativeXfm()` or they cluster at origin; absolute venue systems keep identity), blend via `mPipelines.MapBlend`, group0=`mSceneBindGroup`, depth test LessEqual / write OFF. Free strong `DrawParticlesBillboard` → `gBandRnd.DrawParticles` displaces the weak stub. Verified: pink radial-flares + white bursts render at the strike line.

**Gotcha:** a review agent false-FAILED this by mistaking band-character costume geometry for blown-out particles AND tracing the uncompiled DC3 `Part_Wgpu.cpp` instead of `Rnd_Wgpu_RB3.cpp`. Always adjudicate reviewer FAILs against actual frames + actual compiled source. Engine log via `MILO_LOG`/`TheDebug` is swallowed in native — use `fprintf(stderr)`, grep with `grep -a`.

**Detail / build gotcha:** the weak no-op stub lived at `native/src/rndobj_synth_link_stubs.s:67`. The per-lane `.part` systems are `radial_flare_center.part`, `spark_burst_random.part`, `radial_shockwave.part`, `gem_cap.part`, `broken_glass_squares.part`, `smasher_smoke.part`, collected by `setup_draworder` (smasher_plate.dta) into `after_gems.grp`. The visibility fix (`GemTrackDir.cpp`) took flame `DrawShowing` calls 0→1498; draw path is `RndDir::DrawShowing`→`RndGroup::DrawShowingBudget`→`RndDrawable::DrawBudget` (native skips frustum cull, `SMASHER_DRAW_FIX`), NOT `Draw`/`DrawShowing`. RB3 has NO UV tiling (no `NumTilesAcross`/`Down`); added accessor `RndParticleSys::RelativeXfm()` (Part.h, HX_NATIVE). **Engine is a SEPARATE cmake target `milo-engine` (`libmilo-engine.a`)** — `cmake --build native/build-native --target rb3-native` does NOT rebuild it; use the default (all-targets) `cmake --build native/build-native`. Minor residual: flare pink vs retail blue/white (per-asset tint). Verify harness `/tmp/a1_drive.py`.

---

## A2/A3/A4 — gameplay emissive glow (shared root cause)

**Symptom:** flat gameplay highway — gems don't glow (A2), highway/lanes not lit (A4), strike-glow missing (A3). **Shared root cause:** `BandRnd::DrawMesh` dropped material EMISSIVE — never read `mEmissiveMultiplier`/`mEmissiveMap`, and `MakeMaterialBindGroup` hardcoded the emissive slot (group1 binding5) to `mBlackView`, though `standard_wgsl.inc:800` already implemented `finalColor += baseColor.rgb * emissiveMultiplier * emissiveSample.rgb`.

**Fix (landed engine `f5ee015`, pin rb3 `7e2fe9a9`), all scoped to `cam->Name()=="game.cam"`, default-on, opt-out `RB3_TRACK_LIGHT_OFF=1`:**
1. Darken prelit `surface.mat` highway ×0.12 (prelit ignores ambient — a per-material scale, not a lighting port; the earlier "blocked on scene-lighting" conclusion was WRONG).
2. Re-enable emissive: `mu.emissiveMultiplier = mEmissiveMap ? mat->mEmissiveMultiplier : 0` (guard essential), bind `mEmissiveMap` to slot 5.
3. Force `rails.mat` prelit (softened ×0.7) — lit lanes.
4. Boost the additive `gem_smasher_glow` emissive — brighter now-bar.

The dark surface provides the contrast that made emissive net-positive (the earlier revert tested emissive against the bright-gray surface). Only 4 game.cam meshes carry emissive maps.

**Polish pass (engine `71f21d0`→`59b7307`):** P3 lane blue-tint `rails.mat ×(0.53,0.64,0.92)`; P2 SP blue overlay `peakstate_plane` was a CAPTURE ARTIFACT (only fades in at 4× streak — verify SP/streak HUD at ≥4× streak); P4 venue lighting default-on (engine `8528923`, opt-out `RB3_VENUE_LIGHT_OFF=1`) — the "wash" was scene uniforms re-written only on RndCam change, so the whole venue used ONE environ; fix re-writes SceneUniforms on `RndEnviron::sCurrent` change under world.cam (`mLastSceneEnv`) + grey fallback only when env has no lights; also bumped scene-uniform ring 16KB→64KB (a busy world.cam frame did 24 writes > the 16KB ring's ~21 slots → mid-frame wrap clobber). P1 gem bloom-halo default-on (engine `59b7307`, opt-out `RB3_HIGHWAY_BLOOM_OFF`) via Design-B capture-and-replay: `DrawMesh` captures per-draw GPU state (incl. the live pose-baked `mSceneBindGroup` handle) for emissive gem/now-bar meshes; `EndFrame` replays into a 2nd BloomPass → additive-blit only the halo. `IsHaloSourceMat = emisMap && mult>0 && name!="surface"`.

**Methodology lesson:** A/B on a venue REQUIRES matched frames or a frozen camera — independent boots land the same songMs on different director shots, so per-env venue lighting differs frame-to-frame. A closing-gate "97% gray wash" FAIL was a camera/venue-desync false positive.

---

## A4 — venue scene-lighting / env empty

Supersedes the old "A4 BLOCKED on venue-environ bring-up" conclusion, wrong on two counts:
1. The venue `.milo` deferral was NOT a hard instancing gap — it was a single transposed `ObjPair` ctor in `WorldInstance::SyncDir`: RB3 had `ObjPair(foundObj, it)` (from = fresh copy, null Dir → trips `MILO_ASSERT(p->from->Dir())`); DC3 + target use `ObjPair(it, foundObj)`. Swapping is match-neutral (96.63135% unchanged) and removed the ~70-line `IsDeferredVenueProxy` hack. Landed rb3 `d988a301`. Venue backdrop geometry now renders.
2. Gameplay highway lighting never needed the venue env — highway/gems/HUD draw under `game.cam`, venue/band/crowd under `world.cam`. The dark look is the game.cam-scoped track-lighting change above (engine `f5ee015`).

**Still open (lower priority):** the venue backdrop's own lighting under world.cam. If pursued, it lights band/stage/crowd, not the highway. RB3 accessor contract: `RndEnviron::sCurrent` (public static, no `Current()`); `AmbientColor()/FogColor()/FogEnable()`; light lists are PUBLIC members `mLightsApprox`/`mLightsReal` (`ObjPtrList<RndLight>`) + `mNumLightsApprox`/`mNumLightsReal` (NO accessor methods) (use `mLightsApprox` only — DC3 `ObjDirItr<RndLight>` WASM-hangs); `RndLight`: `GetColor()`, `GetType()` (kPoint=0/kDirectional=1/kFakeSpot=2), `Range()`, `Showing()`, `WorldXfm()` (dir = m.y), `mTexture`. SceneUniforms/`standard_wgsl.inc` already support 4 dir + 4 point + ambient + projected.

---

## C8 — band faces missing/broken (texture composite)

Hub: [docs/native/c8-ground-truth-2026-07-01/](../native/c8-ground-truth-2026-07-01/) (RESOLUTION.md, HEAD_MESH_FREED_2026-07-02.md). Five distinct root causes, all fixed:

1. **Flesh-skin texture composite** (rb3 `372baf7b`, opt-out `RB3_SKIN_FIX_OFF=1`) — grey/blank skin: runtime 3-arg `SetSkinTextures` never `Recompose()`d + material-identity split (skin.cfg MatSwap `mMat` = dummy-diffuse copy ≠ mesh's `*_naked` mat).
2. **"Floating eyes and teeth" = `head.mesh` geometry FREED** (rb3 `26c5684d`) — `BandCharacter::SetDeformation` → `CharMeshCacheMgr::Disable` → `MeshCacher` dtor → `RndMesh::SetKeepMeshData(false)` cleared verts after the face-shape bake. Wii-safe (GX display lists); native WGPU re-reads CPU verts every draw → `nf==0` → head never drawn. Fix: `SetKeepMeshData` refuses to free on HX_NATIVE. Opt-out `RB3_MESH_FREE=1`. **Gotcha:** symptom is per-character (deform-skipped chars keep heads) → random band lineups made naive A/Bs lie; it falsely looked caused by `372baf7b`.
3. **Glowing eyes** (engine `04c8e1c` + rb3 `fadd179a`, opt-out `RB3_COMPOSE_MULT_OFF=1`) — eye RT (`eyes_diffuse_output.tex`) collapsed to ~white (last composite layer) because native `DrawRect` mapped every `MatSwap::Compose` layer's `kBlendSrc` to REPLACE. Fix: dest-multiply the modulate layers while a compose RT is active. (Teeth were NOT an eye bug — uncomposited near-white albedo, dimmed by fix 4.)
4. **Flat/over-bright face shading** (engine `5587ce0` + rb3 `7f603e17`, opt-out `RB3_CHAR_REAL_LIGHT_OFF=1`, plus `RB3_CHAR_APPROX_AMBIENT`/`RB3_CHAR_AMBIENT_MAX` tuners) — `WriteSceneUniforms` promoted every environ's `mLightsApprox` to full Lambert directionals; char envs' approx set = rim.lit + 4 white spots → flat flesh flood while the dim `main.lit` key in `mLightsReal` was ignored. Fix (world.cam path): char envs (`strstr(env,"char")`) with a usable real key shade from `mLightsReal`, approx demoted to averaged ambient; geometry envs + empty-real char envs keep legacy.
5. **RndTexBlender port = NO-GO** (research-proven, don't revisit): the empty blr is Wii-faithful; backend never binds normal maps.

**Superseded/corrected:** fix 3's dest-multiply was WRONG for clothing (collapsed garment RTs toward black). Correct math landed engine `153beaf` (pin `e4b661ad`): decoded Xbox textures = `_diff` DXT1 RGB detail, `_interp_gw`/`_mask_gw` DXT5 with weight/coverage in ALPHA; composite = `out.rgb = diff.rgb · lerp(color1, color2, interp.a)` then mask dest-multiplies coverage, staged in `BandRnd::DrawRect` under `gRB3OutfitComposeActive`. **Web-only grey/flat skin** fixed separately (`266ffb1b`): even with correct math the SKIN RTT composite bakes grey on web with all inputs proven resident — NOT async/residency (disproven, don't re-chase). Fix binds source `_diff` directly + `SetColor(palette skin tone)`; broken RTT path behind `RB3_SKIN_RTT=1`.

**Triage tools:** engine `RB3_HEADMAT_DBG=1` (per-mesh DrawMesh census + EMPTY-geometry census, caught nf=0), rb3 `RB3_SKINFIX_DBG=1`, `RB3_ISOLATE_MESH=<substr>`. `RB3_NO_DEFORM=1` CRASHES at boot — not a usable A/B. Band flesh draws under merged-instance names WITHOUT head/face/skin substrings — name-greps mislead.

---

## Band-character skinning deformation (the SYS-1 shard family)

Full writeup: [docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md](../native/CHAR_SKINNING_DEFORM_INVESTIGATION.md). Long multi-fix arc; the final landed state:

**Symptom:** gameplay band members render flat triangular SHARDS from arms/hands/fingers/hair/facial-hair (torso/legs/head coherent).

**Root cause:** all 4 band outfit skin meshes bound the SHARED, static `char/main/skeleton.milo` magnet (resolved by NAME at outfit-resource load) instead of each member's OWN animated per-member `skeleton_unshared.milo` instance. So the band looked static AND the female member's female-baked inverse-bind mismatched the static male-bind magnet → `R·sin(θ)` shards on thin geometry. (Earlier "byte-identical across 1424 draws" was a MEASUREMENT ARTIFACT — the probe read the static magnet, never the animated skeleton.)

**Landed fixes (default-on, Wii byte-identical):**
- **Torso rebind** (rb3 `acd9c19a` + engine `12455b0`, opt-out `RB3_NO_SKEL_REBIND`): `BandCharacter::RebindOutfitBonesToOwnSkeleton()` from `Poll()` RIGHT AFTER `Character::Poll()` — the timing where `Find<RndTransformable>(boneName)` returns the LIVE per-member bone (at load it returns the static magnet, which is why the earlier SyncObjects rebind was a no-op). Fixes static + fling at once.
- **Head/hair/hands rebind** (rb3 `0de768a1`+`2580e128`, opt-out `RB3_NO_HEAD_REBIND`): `BandCharacter::RebindHeadHandsAtRest()` from `Poll()` BEFORE `Character::Poll()` (on the first Poll the skeleton still holds the `SetDeformation` rest pose). Snapshot each per-member rest WorldXfm, bake `mOffset = meshWorld·inverse(restWorld)`, bind to the LIVE per-member bone. Binds to the DRIVEN skeleton (so hair/face servos don't fight a static rebake). Hardening: fallback latch on no-progress, finite guard before Invert, GeomOwner skip, `StartLoad` reset of latches (closet/salon outfit change).
- **Gender deform** (rb3 `a5999979`): `RndMeshDeform::IsExoBone` read the bone name via a hardcoded Wii vtable-slot offset → garbage under clang LP64; use `t->Name()`. Plus stop deferring `gDeforms` (opt-out `RB3_NO_DEFORM_LOAD`). gDeforms is a FAITHFULNESS refinement (gender silhouette), NOT the fling fix — the rebind alone fixes the fling.

**Metric gotcha (durable):** a skinned mesh's own WorldXfm is IDENTITY, so "mesh-local skinPos" is a TRAP (reads the char's ~300-500u world pos). The real fling metric is draw-time `|skinWorld − boneWorld|` (limb extent ~50-65u clean; fling thousands). Band closeups are camera-random — capture 40-70 shots. `RB3_BONES_IDENTITY` "clean" only proves geometry (meshes authored in model space always look clean at identity).

**Ruled out** (env-gated, all tested): geometry/weights/indices/stride/materials, `MiloXfmToColMajor` col/row order, shader math, `CharBonesMeshes::PoseMeshes`, clip, IK/twist, body morph. The skinning MATH is correct. See also the engine-arch-review "hands terminal" conclusion below — the residual hand/finger tear is vert-encoded inter-bone geometry, closed as GT-D with mitten mitigation.

---

## Decomp sweeps of SHARED render code — the BandPatchMesh gate lesson

Decomp-sweep commits that rewrite SHARED (non-`#ifdef`'d) geometry/anim/char code for sub-100% fuzzy gains can change runtime semantics with ZERO asm proof — and the whole-binary objdiff regression gate only checks Wii asm, never native-port behavior. `BandPatchMesh` shipped grossly deformed characters TWICE this way:
- **`4a49b1a4`** (reverted `82f390b1`): `ExtendTwin` cross-sign flip + `outUv` swap; `AddEdge` stale hoisted index. Deformed chars on native/web for a day while claiming "the native port is unaffected".
- **`30c51bad`** (reverted AGAIN `f0a95910`): a Fable-research→Opus-impl re-land (`AddEdge` 99.76, `ExtendTwin` 94.47, `TryAddFace` 93.1, `FindXfm` 79.15) that PASSED the closeup gate but shipped the same class — giant pale patch shards over arms/hands, needle-spike hands, slab across the vocalist's jaw. Bisect-confirmed (revert→clean, restore→shards). Twist: HEAD's "proven-correct" code was ALSO wrong (`ExtendTwin` cross sign inverted vs the binary; `FindXfm` genuine UB — uninit `xfm.v`/`m.z` + OOB read past `Faces().end()`).

**CRITICAL GATE LESSON:** `band-closeup-capture` + drop/ratio metrics are BLIND to BandPatchMesh corruption — it reported PASS 34/34 pinned, 0 drops, ratio 0.00 on a visibly exploded frame. Corrupted patch geometry renders WITHOUT tripping the V24 shard-guard, and patches (face paint / logos / hand patches) only appear on SOME lineup members. A future BandPatchMesh re-land needs a gate that (a) verifies a patch-bearing lineup is on stage, (b) judges WIDE crowd-shot frames VISUALLY for pale angular shards on hands/arms/faces (venue props like the pub's flying chairs are look-alikes — zoom to discriminate), across ≥2 rerolls, judged by the land-gate reviewer's OWN eyes, not the implementing lane's claim.

**Safe-to-land rule for sub-100 SHARED code:** semantic changes are OK iff they are asm-derived AND the residual is `diff_op:none` (pure regalloc, zero insert/delete — proves semantics match target) AND the pinned native visual gate passes. Wii-only gains inside `#ifndef HX_NATIVE` blocks (template specializations etc.) are always safe. Semantic red flags in sweep diffs: operand-order reversals in cross products/subtractions (sign flips), hoisting a field read out of a loop whose body mutates the container, swapped `.x`/`.y` result rows, removed early-out zeroing. (`FindXfm` at 79% never had `diff_op:none` established — the "iff" was not met by `30c51bad`.)

---

## Convergence: venue/crowd lighting (2026-06-20/21, 06-30)

Hub: [docs/native/converge-2026-06-20/](../native/converge-2026-06-20/). Deterministic force-band-closeup harness → 5-venue audit. **Band closeup GEOMETRY is CLEAN across all 5 venues** — the convergence frontier moved OFF skin-deform onto venue/crowd lighting.

**Landed:**
- **arena_02 near-black band** (engine `a360e3c`, opt-out `RB3_VENUE_POINT_FALLOFF_LEGACY=1`): WGSL point-light falloff was `saturate(1−d/range)²` (non-physical hard cutoff, =0 at d≥range); arena's range-55 spots sit 70-103u from the band → dead key → black band. Fix = GX-faithful `1/(1+d/range)`.
- **big_club white crowd** (engine `ada6e56`, opt-out `RB3_CROWD_DIM_OFF`): NOT skinned chars/lighting — ~9000 impostor-billboard quads/frame with EMPTY mesh name (so `isTextMeshHeur` mis-tagged them → every prior fix skipped them) + near-white baked diffuse. Fix = dim crowd/impostor base color (`RB3_CROWD_DIM` 0.10, world.cam-gated, band hard-excluded via `skeleton_unshared.milo`). white% 17→0.
- **teal highway watermark** (engine `b8f3cfa`, opt-out `RB3_HIGHWAY_WATERMARK_DIM`): the authored `surface.mat` emissive watermark rendered ~3.4× too bright (shipped `mu.color*=0.12` darkens base but not emissive) → `mu.emissiveMultiplier *= 0.30`.
- **Festival `*_screenmask` white** (engine `998b8734`, opt-out `RB3_SCREENMASK_FALLBACK_OFF`): diffuse RT fed only by a Bink movie with no native decoder → never painted → `DrawRect` blitted `mWhiteView` full-screen. Fix = skip the quad when `!hasTex && diffuse->IsRenderTarget()`.
- **Scrollbar GAP 1** (`7a6525fc`, opt-out `RB3_SCROLLBAR_FIX_OFF`): "scrollbar across the highway" was a MEASUREMENT ARTIFACT (draws only in song_select under `[ui.cam]`, never gameplay). Shipped a Wii-neutral content-gate anyway.

**DC3-SAFE GATING PATTERN (reusable):** add a `SceneUniforms` mode flag reusing a `_pad` slot (struct size unchanged), default 0 = exact legacy, set mode 1 only on the RB3 path. Verify DC3 grep 0 hits.

**Deferred backlog re-assessed 2026-06-30 (CONVERGED):** of 5 items only the screenmask was worth landing. **Festival Bink Option B CLOSED — jumbotron `.bik` were CUT from the 360 build** (reconstructed the full retail 360 disc: XDVDFS is only the ARK, no loose `world/` tree, no mass-crowd biks). Option A (skip white quad → gameplay over black) is plausibly faithful to retail 360. Reusable disc tooling committed in the engine repo: `milo-native-engine/tools/asset-extract/` (`god2iso.py`, `milo_decompress.py`).

**Also from this backlog:**
- **STEP-2 impostor-crowd env-gate = CLOSE_OBSOLETE — do NOT push tag `converge-step2-crowd-wip` (`bae1aae`).** It was structurally correct + DC3-safe (impostor cam reads its dim authored env instead of hardcoded-white) but B(a) above already fixes the visible crowd, and STEP2+B(a) MULTIPLY in the same pipeline → double-dim → near-black. Its `(unnamed cam && TargetTex())` discriminator could also collide with `OutfitConfig`'s char-customize RTT cam.
- **Footwear `_skin.2` + crowd/extras accessory shards = ACCEPT** (blocked on the C8 rotation-basis divergence; off-frame/masked, correctly guard-dropped). CORRECTION: the "no non-Band rebake hook" claim was FALSE — `Crowd.cpp:911 RebindCrowdCharBonesToOwnSkeleton` already ships and fixes crowd BODIES; residual is 3 tiny accessories on ≤3/292.
- **STEP-1 exposure = ACCEPT** (0.70, retail-consistent).
- Engine landing mechanics for this campaign: `tools/setup-worktree.sh <name> --engine` (private engine worktree, build `-DMILO_ENGINE_PATH=$(cat .engine-path)`); coordinator lands via `git -C ../milo-native-engine merge --ff-only <sha>` then bumps `MILO_ENGINE_PIN`. Engine pin moves under you mid-session — branch off current HEAD and re-verify concurrent agents' uncommitted engine files survived the ff-merge.

**Method lesson:** trust measured root-cause over plan hypothesis (the "char-extras flat-white" hypothesis was wrong — it was empty-name impostor billboard quads). An unverified single-agent reversal of a multi-agent finding must be independently re-verified before landing.

---

## Crowd 2D bowl-imposter (Fix B, shipped 2026-06-20)

The arena/festival/big_club render-to-texture bowl-crowd pipeline (`WorldCrowd::DrawShowing` 2D path) was structurally dead on native WebGPU; now renders. rb3 `9a821382` + engine `741f136`. Default on, opt-out `RB3_CROWD_IMPOSTER_OFF` / `RB3_BILLBOARD_OFF`. Test venue = arena_06 (`festival_01` still SIGSEGVs on load). Doc: [docs/native/render-polish-2026-06-11/task-crowd-2d-impl.md](../native/render-polish-2026-06-11/task-crowd-2d-impl.md).

Five gaps (design named 3, verification found 2 more engine bugs):
1. (rb3) `WiiRnd::GetSharedTex` weak-stubbed→null → new `rb3_crowd_imposter_native.cpp` returns one persistent file-static square `RndTex`. (Gotcha: `SetName(...,nullptr)` asserts non-null ObjectDir — don't name a free-standing global tex.)
2. (shared) `RndMultiMesh::DrawShowing` portable path dropped the `kFastBillboardXYZ` constraint (`SetWorldXfm` marks cache clean → `ApplyDynamicConstraint` never runs) → HX_NATIVE branch sets world xfm from `RndCam::sCurrent->WorldXfm().m`.
3. (engine) `WriteSceneUniforms` hardcoded window aspect for every cam; imposter cam is SQUARE on a 256×256 RT → char thrown outside NDC. Fix = aspect from `cam->TargetTex()->W/H` when RT-targeted.
4. (engine) imposter cam inherited venue far (~224) → char at world origin behind far clip → culled BLACK RT. Fix = RT-cam far widen to 8000.
5. (engine) `BeginDrawTarget` must clear `mLastSceneCam=nullptr` — the 2D loop re-Selects the same camera per archetype; the `DrawMesh` camChanged latch misses the re-pose → stale uniforms.

**Verification trap (record):** RT-readback showed all-BLACK while the feature WORKED — RT tex usage lacked `CopySrc`, so `CopyTextureToBuffer` was a silent Dawn no-op. A broken readback masquerades as a broken feature. Gold-standard proof is the RT readback, NOT the in-venue frame (a static baked crowd texture already fills the bowl walls; director cam is non-deterministic). Probe `RB3_RENDER_DBG=1` logs "RTT created 256x256 for tex ''" = the imposter tex.

**Follow-up (open):** `PipelineManager::PreWarm` sweeps the RT pass STATIC-only (assumed skinned meshes never render into an RT — now FALSE). Add a `{rtFmt, hasDepth=false, alphaWrite=true, skinned=true}` `PassVariant` to kill the first-imposter-frame pipeline-compile hitch.

---

## Crowd "at origin" — really *_strings skin explosion

Hub: [docs/native/crowd-origin/](../native/crowd-origin/). **User report "crowd all congregating at origin, drum kit stuck there" was a 2D MISATTRIBUTION — placement was never the bug.** New `{rb3_pos_dump}` DTA tool (rb3 `2f393eaa` family) proved band roots + 300 crowd members are all SPREAD (crowd_at_origin=0/300). All 5 placement hypotheses refuted.

**Actual cause:** engine V24 `[SHARD_GUARD]` correctly dropping the band lead-guitar `*_strings.mesh` of "brain"-class special guitars (chainsaw/guitar_brain) which explode to ~136u world AABB (ratio ~5.0). Those guitars author their string-bend rig on the CHARACTER skeleton (`skeleton_unshared.milo`) with NO own-resource neck → same family as the skinning-deform bug but on `mInstDir` (which `RebindOutfitBonesToOwnSkeleton` deliberately excludes). FINE standard guitars bind strings to their own rigid `<inst>_resource.milo` neck → ratio 1.0.

**Fix** (`2f393eaa`, opt-out `RB3_NO_INST_REBIND=1`): `BandCharacter::RebindInstStringsToRestBasis()` from `Poll()` — rigid-anchors every `*_strings` bone to `bone_bridge`, rebakes offset so the mesh rides one rigid bone (ratio→1.0). Gated: name ends `_strings.mesh` AND a bone resolves to `skeleton_unshared.milo`. Measured ratio 5.0→1.0, drops 1984→0.

**Harness (reusable):** 3 DTA accessors — `{rb3_force_shot "<name>.shot"}` (sets `mDisabled=true` THEN `BandDirector::ForceShot`), `{rb3_director_disable}`, `{rb3_cur_shot}`; driver `scripts/native/band-closeup-capture.py` (matched-`(shot,songMs)` A/B). Shot names are VENUE-specific and need the `.shot` suffix. Use this for ALL future char-pose/skin convergence A/B — the director's nondeterministic cuts otherwise make per-run guard metrics a false-positive trap.

**Lesson:** a "geometry at origin" report can be a 2D projection of an in-place skin EXPLOSION around a correctly-placed bone — measure world positions + `SHARD_GUARD_OFF=1` A/B before assuming placement.

---

## Walk-on / count-in pose

**User report "all crowd members at stage center / legs floating" = the song-start COUNT-IN window, not placement** (posdump shows everything spread during gameplay — don't re-open placement). During the 0-5s count-in the band held the loading-screen tv-vignette poses (seated/lying → "horizontal floater"). **Mechanism (measured):** no frozen blend — the vignette clip is cleared before venue entry, driver empties cleanly, and with an empty driver `Character::Poll` stops driving bones → skeleton freezes on the last vignette frame until a gameplay clip plays. Prominent on web because of slow start; Wii hides the window with a dark pan.

**Fix `67e87ae1`** (BandCharacter, HX_NATIVE, opt-out `RB3_WALKON_SNAP_OFF=1`): `SetContext("venue")` arms `mNativeWalkonSnapPending`; `Poll()` retries the member's default stage idle (sit for drums, else stand) with cleared driver + no blend + `SetTeleported(true)` until any clip is live.

**Residual (known, separate, don't re-attribute to walk-on):** white thin-geo shard clusters (guitar strings/cymbals/drumsticks) + black hair masses during count-in = the C7/C8 skinning-basis/rebind-latch window — PROVEN pose-independent (persist on clean upright poses). Scout doc: docs/native/walkon-2026-07-02/SCOUT.md (documents the `BAND_ANIM_PROBE='*'` gcd(30,4) aliasing gotcha).

---

## Track depth occlusion (note-highway occluded by band/venue)

**Symptom:** the note highway gets occluded by background band characters and venue furniture. **Root cause (NOT material/zMode, NOT z-fighting):** the venue WorldDir renders with the venue camera; the note highway with `game.cam` (near=30 far=224.1). Both write the ONE shared depth buffer (cleared once at BeginFrame). Different near/far means the same NDC-z represents different physical distances → venue geometry near the venue camera wins the shared depth test. Classic two-cameras / one-depth-buffer incomparable-depth bug. Draw order is venue→track.

**Fix (landed, integrated):** clear the depth buffer (preserve color) between venue draw and track draw. Engine `BandRnd::ClearDepthForOverlay()` (ends the active pass, re-opens with color `LoadOp::Load` + depth `LoadOp::Clear`; opt-out `RB3_NO_TRACK_DEPTH_CLEAR=1`) called once from `TrackPanel::Draw()` (rb3, HX_NATIVE) before `UIPanel::Draw()` — the single per-frame top of track render (`TrackDir::DrawShowing` runs many times/frame, wrong spot). Engine `6498fab`; a concurrent agent extended the same method in `132add5` (Tier-2 layering) so the venue is graded but highway+HUD draw over it UNGRADED, matching retail. Repro: `scripts/native/gameplay-depth-capture.py`; `CAM_DBG=1` logs per-mesh cam/depth.

**Lesson:** concurrent agents may move engine main AND rb3 master AND repoint the shared build-native `MILO_ENGINE_PATH` mid-task — always re-check live state (git log / CMakeCache / `strings` the binary) before trusting a build.

---

## Native visual repro loop (tooling)

Web visual bugs USUALLY reproduce in native (which iterates far faster than the ~5-min web brotli build). Web and native share the same `Rnd_Wgpu_RB3.cpp` backend at the same pin, so a *capability* gap can't differ between them — apparent divergence is usually build-staleness or an asset-set difference (real per-song album covers ship in the web bundle but not `orig-assets/extracted/ui/image/`). Char-bone skinning AND mesh-RTT both WORK on native (the earlier "skinning is a native no-op" model is stale). `BandRnd::DrawRect` (2D quad) and per-screen PostProc both landed; song_select cover-hide/etched-hide/details-pane hacks all retired.

`rb3-native` boots with `RB3_GAME=1 RB3_HTTP=1 MILO_HEADLESS=1 RB3_DATA=<repo>/orig-assets/extracted` and serves a debug API: `GET /api/health`, `GET /api/screenshot` (PNG), `POST /api/input`, `POST /api/dta/eval`. Harnesses: `scripts/native/song-end-test.py`, `song-select-capture.py`.

Durable gotchas:
- **DTA-root probe pitfall:** bare symbols/triggers DON'T resolve at DTA root and silently no-op (caused two wrong flip-flop conclusions). Always use PANEL-RELATIVE paths, e.g. `{{song_select_panel find song_select_details} showing}`.
- **Gameplay-color pitfall:** RB3 songs have AUTHORED intro B&W camera shots — judge gameplay color at steady-state (songMs > ~25000), not the intro.
- **Char-textures-render-WHITE class** (master `40bf17b3`): characters render untextured because the native milo loader advances one DirLoader state per poll (vs Wii's atomic per-milo budget-loop), so a concurrent `BandCharacter` `FileMerger` merge DRAINS a shared external palette milo (`colorpalettes.milo_xbox`, 20MB) mid-load → sibling materials resolve to null → `WhiteTexView` fallback. Fix = `BandCharacter::FilterSubdir` (HX_NATIVE): when `DefaultSubdirAction==kMerge` AND the subdir is its own on-disk file, return `kReplace`. (This white-tex shim is later implicated in the hands-binding saga — see engine-arch-review below.)
- **Song_select stale-text class:** `MusicLibrary::Text` (and native UIList `Text()` overrides) only write the slot matching the node type; recycled scroll rows show stale header text. Fix = `SetTextToken(gNullStr)` at the top of the override under HX_NATIVE.
- **Link gotcha:** shared `src/` under HX_NATIVE may call engine symbols (e.g. `InvalidateGpuMesh`) that only exist in the DC3 backend → native fails to link while web silently auto-stubs. Fix = native shim.
- Headless native capture has a readback retention quirk (a 1-2 frame transient can persist across `/api/screenshot`) the real swapchain lacks — another reason to confirm on web.

Full audit/plan: `docs/sessions/native/RTT_HACK_UNWIND_ROADMAP.md`.

---

## Song-select / menu UI polish (2026-07-02)

Two-wave campaign, all verified from pixels, Wii-neutral. Landed:
- **Yellow selection highlight vanishing** on bottom/LONG menu entries = V24 shard-guard FALSE-POSITIVE: `highlight_main`/`highlight_pattern` are SKINNED quads whose corner bones shrink-wrap label text; wide labels exceed the 2× bind-extent ratio → culled. Fix: name exemption in `Rnd_Wgpu_RB3.cpp` (`12ec7df`, opt-out `RB3_NO_HUB_BAR_SHARD_EXEMPT`).
- **Extra 5-icon row over "NO REVIEW"** = review lighters whose `review_anim` frame-0 hide pose native never applies. Fix: `ReviewDisplay::DrawShowing` hides `lighter1..5` at `mScore==0` (rb3 `53e24150`, opt-out `RB3_REVIEW_LIGHTER_FIX_OFF`). This was also what made the difficulty grid look clipped.
- **Red scrollbar thumb at screen center** = skinned `scrollbar.mesh` bones don't inherit `ScrollbarDisplay`'s `SetWorldXfm`; fix stashes sibling `scrollbar_bg` placement as thumb obj.world (engine `a8089c3`, opt-out `RB3_SCROLLBAR_THUMB_FIX_OFF`).
- **Opaque grey body below album art** = `bottom_square_refraction.mesh`: native WgpuRnd has NO refraction pass → frosted materials render as opaque lit quads. Fix: skip that mesh in `MenuVoidDrawHook` (rb3 `83d2ffff`, opt-out `RB3_REFRACTION_FIX_OFF`). Other refraction meshes still flat-render — a real frost shader is the general fix.

**Adjudicated FAITHFUL_AS_IS** (don't re-chase without a 360 oracle): sidebar ~15-22px right/down offset vs Wii refs + star-badge graze by album panel = 360-ARK authored layout variance (sidebar is a skinned milo with independently absolutely-positioned elements — album z+117, dots z-117; no panel-level entry anim skipped, PropAnim applied). Xenia is NOT a quick RB3 oracle (local fork is DC3-hacked, RB3 faults in audio/present init). **Tooling:** `MENU_VOID_DBG2=2` dumps every drawn RndMesh; ui.cam mapping `screen_x≈640+world_x*1.4`, `screen_y≈360−world_z*1.5`; `MENU_VOID_SKIP=<substr>` A/B. GOTCHA: top-level ObjectDir finds on `song_select_details` hit a NON-rendered prototype (showing=0) — the visible panel is a separate proxy instance. Hub PLAY NOW submenu = cheapest bottom-entry-highlight repro.

**Open residuals (known, unfixed):** overshell part-select sprites displaced vs Dolphin GT (SONG DIFFICULTY dots, missing "Player 1" label, ⊖ icon overlapping "CHOOSE"→"IOOSE") — same displaced-sprite family; transient mirrored footer labels; hub "NEXT MESSAGE (1/1)" text overlap (stale-slot family). Dolphin GT: `docs/native/c8-ground-truth-2026-07-01/dolphin-shots/nav_song_sel.png`. Review note: the `RB3_SCROLLBAR_THUMB_FIX` static `sHaveScrollbarPlacement` latch never resets — fine for the single widget, fragile if two `ScrollbarDisplay`s are ever visible.

---

## Part/difficulty selection screen restored (rb3 c9dc059a, 2026-07-02)

The test-infra hack that skipped `part_difficulty_screen` after song confirm is REMOVED. The interactive flow was ALWAYS faithful (pure pad presses reach choose_part → choose_diff → ready → load) — only the synthetic verbs skipped it. **Removed** (`rb3_game_input.cpp`): `track:<sym>`, `difficulty:<d>`, web `WebDrivePartSelect`. **New verbs:** `part:<sym>` (guitar only) and `diff:<easy|medium|hard|expert|0-3>` — readiness-gated multi-frame state machines firing REAL pad presses. **Canonical nav changed** (old `track:`/`difficulty:` scripts are DEAD): `part:` MUST precede `diff:`; the real screen adds ~30s wall-clock so deadlines were bumped. Web: use `down/hold(220ms)/up/gap(400ms)`, not bare `page.keyboard.press()` (races the rAF `_rb3Keys` poll). 16 harnesses migrated + verified. GOTCHA: `part_difficulty_screen` is only briefly on-screen (~frame 355) — 0.5s health polls MISS the window; use 30ms polls to assert it. `scripts/gpu/capture.sh -f/-s` frame windows need recalibration (game_screen lands later now).

---

## rb3-viewer + white-wig fix (2026-07-02)

**rb3-viewer** (`5b8e0d05`) — standalone `.milo` asset renderer built into rb3-native: `rb3-native --viewer <path> [--sim N] [--list] [--test-bone] [--pose-dump] ...`; wrapper `scripts/native/render-asset.py`; doc [docs/native/asset-viewer-2026-07-02/VIEWER.md](../native/asset-viewer-2026-07-02/VIEWER.md). Registers char factories (NOT `Character` — CharacterTest→overlay trap; NOT wholesale CharInit). v2 (`793e718d`) fixed the invisible-sim problem — the engine always skins `IsSkinned()` meshes; the blockers were 4 standalone-milo issues fixed viewer-side (synth identity parents for rootless strand roots; rebind inverse-bind to rest; bake post-sim world→local; `SHARD_GUARD_OFF` while posing). `--test-bone` + in-game gate are the reliable checks (standalone `--sim` diverges — no head frame / CharCollide volumes).

**White-wig bug FIXED (`81f38f3a`)** — "character with white wig / long hair renders wrong" = `CharHair` strand physics collapsing: decomp mis-scoped the closing brace of `if (collides.size()!=0)` in `CharHair::SimulateInternal`, gating the per-point bone-update tail (SetWorldXfm/force/friction/inertia/chain-advance) that the Wii target runs UNCONDITIONALLY. Collide-less free strands (crazyhawk, ziggymullet, longmop, robertplant) draped over faces; tight styles fine. objdiff was a DECEPTIVE 99.6% "match" — the bug hid as one `diff_arg beq` branch-target mismatch. Fix = brace move; 99.6% held. **LESSON: a `diff_arg` branch-target mismatch on a `beq` = possible real CFG bug, not noise.** "White" was the collapsed-pose artifact (flat ribbon faces catching light); matched-lighting native ≈ Dolphin ground truth. `RB3_HAIR_DBG=1` = hookup-coverage probe.

---

## Visual diff snapshot (2026-07-02)

Retail-vs-native visual diff findings (captures `/tmp/visdiff-20260702/`, refs `images/retail-screenshots/`). Ranked bugs at that date (several later resolved in the engine-arch-review waves — cross-check status there):
1. HIGH main_hub: opaque grey rounded rect dead-center = message/ticker panel backing quad drawn untextured (retail has none). → later fixed, hub grey-quad hide `RB3_HUB_MENU_QUAD_OFF`.
2. MED gameplay: score+star HUD pill anchored ~center-top instead of retail top-right. → later fixed, `RB3_HUD_SCOREBOARD_TOPRIGHT`.
3. MED gameplay: star meter renders ONE outlined star that never fills. → later NOT-A-BUG (progressive reveal faithful).
4-6. MED/LOW song_select: album art cropped by grey diff-grid panel; art panel overhangs header (SYS-5 Y-anchor family); stray bright-red vertical bar; grey opaque empty-state panel vs retail dark translucent.
7. Pattern: findings 1+4+6 share an "opaque light-grey where retail is dark translucent" signature — possibly one shared untextured-quad/alpha-material fallback.

**Known/confirmed (don't re-find):** char skin washed pink + eye issues (C8 family); venue backdrop over-bright warm (venue exposure); missing online chrome (intentional offline).

**Not yet captured at that date (need follow-up shots):** drums/vocals/keys gameplay, star-power activation, results screen, filter/sort panel, title screen, career flow, customize, pause overlay, calibration, real-song album thumb in list view.

---

## DC3 feet-in-floor (RB3 does NOT reproduce — do not port)

**RB3 status (measured 2026-06-30):** RB3 does NOT reproduce this. RB3 band feet are FAITHFUL (toe ~0, ankle ~4.3 matching Xbox; 345 samples worst toe −0.7; leg chain composes perfectly, IK inert, no 186u fling). RB3's accumulated skinning fixes already absorbed it. **Do NOT port the DC3 foot-plant into RB3** — it would assert an already-satisfied pose. The RB3 thin-geo `_skin.2` drops are transient off-frame V24-handled false-positives, NOT this bug.

**DC3-native bug (separate) is SOLVED + merged** (dc3 `0f83a3de`, wave-6 lane A, 2026-06-11 — do NOT re-investigate). Root cause = a ~25° knee+ankle CLIP/anim-layer QUAT under-bend at deep-crouch beats (frame-matched vs Xbox via Xenia GDB-RSP, VMX128 `+0x78` bone-world offset): at pelvis-Z 33-35 native knee −32°/ankle +12° vs Xbox −57°/+35°. Shipped fix = `Dc3RunPostPollFootPlant()` (an order-INDEPENDENT deterministic WORKAROUND called from `App::RunWithoutDebugging` AFTER `TheTaskMgr.Poll`), analytic 2-bone clean plant, lifts only a below-floor toe, default-ON, opt-out `DC3_FEET_POST_PLANT_OFF=1`. Verified green (0/804 below floor, worst +0.60). REFUTED theories (don't revisit): ankle IK write-discard, poll-order race, `mMoveElbow` load divergence, CharLocalIKScope, RB3-style wrong-skeleton bind. The remaining FAITHFUL (non-workaround) source fix — the deep-crouch clip-selection/blend under-bend — is a documented prototype-open item. (The full 16-push investigation trail lives in `dc3-decomp/docs/sessions/2026-06-09-xenia-xbox-foot-truth.md`; it is DC3's, not RB3's.)

---

## Engine architecture review (2026-07-05 →, ongoing wave campaign)

Full record: [docs/native/engine-arch-review-2026-07-05/](../native/engine-arch-review-2026-07-05/) (PLAN.md, ARCHITECTURE_REVIEW.md, REFACTOR_PLAN.md, execution/README.md wave rows, execution/RETROSPECTIVE/). A commissioned architectural review of `milo-native-engine` + a long multi-wave refactor/fix campaign (waves 1-32+). **VERDICT: `targeted-refactor`** — the shared engine core is sound; one overhaul-scale chunk (the `Rnd_Wgpu_RB3.cpp` monolith) + 4 bounded redesigns.

### Systemic causes named
- **SYS-1:** skinned `obj.world=identity` + shared-magnet bind + magnet-basis invBind (explains hands/fingers, crowd-at-one-point, drum kit, hub-bar/scrollbar).
- **SYS-2:** asset-name `strcmp` heuristics culture (~30 branches) — structurally addressed by relocating all 13 asset-name branches (B1-B13) into `rb3_render_hook.cpp` via a `GameRenderHook` seam.
- **SYS-3:** mutable `mSceneBindGroup` order-dependent state — FIXED (immutable `RB3SceneBinding` threaded via `RB3DrawContext`).
- **SYS-4:** real/approx light routing inverted + box-ambient absent (biggest open lighting frontier).
- **SYS-5:** 360 `.milo_xbox` assets on a Wii-decomp engine = the UI Y-anchor bugs (not engine fault).
- **SYS-6:** ~832 silent weak stubs. **SYS-7:** no render-correctness test (why BandPatchMesh reverted twice) → Phase 0 safety nets (CPU reference skinner golden, loud stubs, per-draw drawlog golden, non-blind lineup gate, flag registry) were a binding prerequisite.

### Durable conclusions (the load-bearing outcomes)
- **CROWD CHAIN CLOSED — DO NOT REOPEN** (waves 23-29, six successive narratives, closed as a MEASUREMENT ARTIFACT). The "hub walkers never triggered" was a `CHARDRV_PROBE=crowd` FILTER ARTIFACT — real hub walkers are the 4 `player0-3` `main.drv` drivers (`mClips == streetslomo_clips.milo`, PathName-asserted), driven via `BandCamShot::StartAnim()` in `WorldDir::Poll`, sustained animating, matching retail. The 8 `crowd_*` proxies idle faithfully (asset has no crowd clips). **Census trap (binding):** any crowd census must target the player0-3 drivers with `mClips` PathName pinned; measuring `crowd_*` re-enters the trap. Reopen only on new evidence naming player0-3.
- **HANDS TERMINAL — closed as GT-D** (waves 2-21, ~8 measured dead offset/bake cells + reskin refuted). Root cause = native-port BIND-BASIS SPLIT: verts skinned against the shared male-bind, drawn on per-member gender-posed bones (female double-mismatch). The residual tear is a mesh-SHELL/weight-blend divergence between coherently-attached joints — INVISIBLE to every bone-level metric (`R·sin(θ)` vert-encoded inter-bone geometry). Skeleton STATICS exonerated ≤0.65°. NO non-banned/non-walled lever exists; parse-time un-share lands in the same torn family, unverifiable without articulated Wii ground truth (`CharClipDrivers=0` in every blind-navigable Wii state). **The mitten mitigation is the answer** — `RB3_HANDS_MITTEN` default-ON (opt-out `RB3_HANDS_MITTEN_OFF`): hands-scoped palette blend toward wrist-rigid past a relAng-to-wrist tear threshold. `RB3_NO_SKIN_CLAMP` does NOT touch band hands. Do NOT re-charter hands or re-buy "restore remap → per-member hands".
- **`set_play` arg-swap (W31) — a DECOMP BUG, faithful fix.** The band stayed idle all song (the whole W30 "no performance clips" mystery) because the 5 `SYNC_PROP_SET` intensity handlers in `BandDirector::SyncProperty` had SendMessage args SWAPPED (`mood,inst` vs retail `inst,mood`) → `set_play` never fired. Fix `a3916764`: `SyncProperty` 99.96→**100.0%** AND the band performs. **LESSON: a 99.96% function hid a subsystem-killing arg swap; near-100 residuals on message-dispatch code are a cheap high-yield audit class.** (The follow-up W32 arg-order signature SWEEP was EXHAUSTED 0/1185 landable — SyncProperty-class bugs are found BEHAVIORALLY, not by scan. Do NOT renew as a sweep.)
- **Exit-trap SIGSEGV (W31) FIXED (engine `0083bad`).** `~BandRnd` at static-dtor time — `BandRnd::Shutdown()` never released the compose/C8-RTT + billboard-particle GPU clusters (added after Shutdown was written); `mComposeDiffView` held the last Dawn device ref. Fix releases both clusters pre-`mGpu.Shutdown()`. Bounded boot rc=0 10/10; drawlog-golden now hard-fails on rc regression.
- **Yellow-square (W32) = STALE BUILD EXCLUSION, not render code.** `rb3_render_hook.cpp` was filtered out of the web source list in the W1 "clear-frame era" and never re-added → `GetGameRenderHook()==nullptr` on web → the whole B1-B13 policy family (incl. W31 glyph fix) was absent on web for ~30 waves. The skinned highlight bar fell to the world-origin path = a static quad over the center character. Fix = add the TU to `RB3_WEB_NATIVE_GLUE`. **LESSON: audit web/native source-list parity when a defect is web-only.**
- **Prop-fans (W32) FIXED (`RB3_NO_MIDIDRV_ENTER_FIX` opt-out).** Instrument-MIDI prop drivers (`.dmidi` drum-hit/strum/fret) were Polled but NEVER Entered natively → never `AddSink` onto the MIDI parser → `OnMidiParser` never fired → no hit/strum clips → idle arm → `CharIKHand` over-reach → prop-tip fans. Fix: lazy Enter-once-on-first-Poll (`CharDriverMidi.cpp`, HX_NATIVE). `OnMidiParser` 0→173, drummer shards 1107→2.
- **Placement contract (W2.1) SHIPPED default-ON `RB3_PLACEMENT_CONTRACT`** — vertex-invariant reorg (`obj.world=WorldXfm` + bind-relative palette `skin·inverse(meshWorld)`, 0.0000 diff) fixing crowd/drum-kit "at one point". The Wave-5 "wash asymmetry" that held the flip was REFUTED by songMs-pinned interleaved measurement (full-frame magenta env cast ≠ crowd transform). The human-eyes E1 gate caught what the oracle couldn't.
- **Other shipped defaults:** black singer head (`RB3_BLACK_HEAD_FIX_OFF` opt-out — head tex in a nested subdir unreachable by non-recursive `Find`, null diffuse); UI text floor relax (`RB3_UI_TEXT_FLOOR_STRICT` — `RB3MaterialBinder` `max(0.6,color)` clobbered UILabel focus colors); hub grey-quad hide; UI post-grade pass (`RB3_UI_POST_GRADE`); venue chroma-preserve composite (`RB3_PP_CHROMA_PRESERVE_OFF` opt-out — grey venue → colored stage lighting via graded-luminance × ungraded-chroma); FOREARM-FLOAT reach clamp (`RB3_IK_REACH_CLAMP` — CharIKHand IK weight=1 toward targets 54-273u vs reach ~20u); Player-N gamertag fallback; ticker Y-fix; ROWFIX; album-art assembly Y-fix; HUD scoreboard top-right; score/glyph fixes. (~17 shipped defaults by W32.)
- **Ack rule (standing):** decomp-source-bug fixes land UNCONDITIONAL (no HX_NATIVE gate — gating would fork faithful behavior); native-only render fixes land default-ON with an `RB3_NO_*` opt-out once ON-vs-OFF evidence exists.

### Process lessons
- **BOOTRNG noise floor:** per-boot lighting/draw nondeterminism (global `gRand` stream POSITION varies via async-load completion order at fixed seed `0x5EED`) is the visual-gate noise floor. Root owner = W0.3d part-b (frame-assignment TIMING axis). Partial mitigation `RB3_LOAD_DETERMINISM` (opt-in, fixed-clock-scoped).
- **Retrospective verdict (15-wave):** the discipline layer was production-grade (10+ defaults, 0 shipped regressions); the gap was the DIAGNOSIS-INSTRUMENT layer (~6-9 waves lost to native-probe-only premise inversions, and an invalid `wext>60` oracle gated ~10 waves). Ranked tool builds: Dolphin single-frame bone probe, gender-split skinning gtest fixtures, `/api/uidump` + drawlog provenance, scoped per-consumer Rand streams.
- **`DECOMP_FORCEACTIVE` bakes `__LINE__` into its symbol** — deleting lines above it changes the Wii `.o`; preserve physical line count when retiring probes.
- **Standing protocol:** each wave = pre-dispatch Fable review (adversarial, file:line-grounded — caught a false premise in the headline lane on ~5 consecutive waves) → dispatch → post-wave Fable close-out review + docs (README results+menu, regen, ONE pin bump). Record BOTH a workflow's runId and task ID (they differ; using runId to check liveness once double-ran live lanes). Grep RAW logs, not curated excerpts (a crowd narrative was falsified by its own omitted log lines).

### Open at last snapshot (W33 input, post-W31 visual audit)
V1 band members in grossly wrong full-body poses in gameplay (prime suspect = W31 `set_play` body clips exposing the SKEL rotation-basis class on band chars = the new-hypothesis evidence the SKEL-family STOP required); V2 results-screen CONFIRM → SIGSEGV `BandCharDesc::NameToDrumVenue("")` (no sentinel in `sDrumVenueMappings`, index-10 OOB); V3 overshell `MILO_FAIL(OvershellSlotState)` AND the assert formatter itself SIGSEGVs (fix formatter first); F2 score-pill (backing ~0 coverage, venue bleeds through, engine `RB3MaterialBinder` coverage); F7 sidebar missing opaque backing layer (asset gap); F5 patch-shards (2 prior bisect-reverts, needs fresh hypothesis). SKEL family (hub-walker face cones/waist fans/boots) = closed rotation-basis static-bind family on crowd/extras + hippyfringe magnet — NO recharter without a new hypothesis.
