# W1.7 — Wire GameRenderHook for RB3 + relocate asset-name branches — STATUS

Append-only. Update under `flock /tmp/rb3-docs.lock`. One `## <subtask-id> — done|partial|blocked`
section per subtask, with commit SHAs (engine + rb3), gate evidence, and blockers.

Planner (Opus) wrote `PLAN.md` on 2026-07-06. Implementation not yet started.

<!-- implementers append below -->

## W1.7.S1 — done

Implementer (Opus), 2026-07-06. Design + frame-pass seam wiring + full site
classification. **No B/A-bucket relocation done here** (that is S2/S3/S4); S1
lands the interface + no-op impl + live-but-inert frame-pass seam, all
byte-identical by construction.

### Commits (ordered)
1. engine `9083833` — `W1.7: add per-draw GameRenderHook policy methods (no-op defaults, unused)`
   — `src/platform/GameRenderHook.h`: 3 new NON-pure virtuals with base no-op
   defaults (`QueryDrawGeomPolicy`, `QueryDrawMaterialPolicy`, `QueryHaloPolicy`)
   + 3 policy PODs (`DrawGeomPolicy`, `DrawMaterialPolicy`, `HaloPolicy`).
2. rb3 `7c43b21d` — `W1.7: implement no-op per-draw policy in rb3_render_hook`
   — `native/src/rb3_render_hook.cpp`: BandRenderHook overrides all 3 as no-ops
   returning default (no-override) PODs. Also corrected the stale "GFX off / not
   linked" header comment (RB3 builds GFX=ON, rb3 backend, Rnd_Wgpu_RB3.cpp IS linked).
3. engine `d309e57` — `W1.7: wire frame-pass GameRenderHook dispatch in RB3 renderer (MOVE, no-op hook)`
   — `src/platform/Rnd_Wgpu_RB3.cpp`: `#include platform/GameRenderHook.h` +
   `RenderCharacterImpostors(this)` at end of `BeginFrame` (after `Rnd::DrawPreClear()`)
   + `DrawGameOverlay(this)` in `EndFrame` (after venue/halo composites, before
   `mEncoder.Finish()`). Both guarded by `GetGameRenderHook()`; BandRenderHook
   is a no-op for both → byte-identical.

### Interface design (final)
Hybrid per PLAN recommendation, extending the EXISTING `GameRenderHook` (single
registration path via `SetGameRenderHook`/`GetGameRenderHook`; no second slot):
- **Geometric/guard (B1–B5)** → one discrete method `QueryDrawGeomPolicy(RndMesh*, float* outWorld16)`
  returning `DrawGeomPolicy{ hubBarPlacement, scrollbarBg, scrollbarThumb,
  shardExemptHubBar, shardBandMember }`. Engine keeps its matrix math; hook
  returns the DECISION (+ optional world matrix the engine only reads when
  `hubBarPlacement`) so float ordering never crosses the seam.
- **Material classification (B6–B13)** → one method `QueryDrawMaterialPolicy(RndMesh*,
  RndMat*, bool skinned, RndMesh* owner, const char* camName)` returning
  `DrawMaterialPolicy{ isUiText, isHubHighlight, isSkinRtt, isColorIcon,
  isTailChain, isCrowdExtra, isBandMember, int highwayClass }`. Engine keeps the
  uniform math; hook returns WHICH class. **`camName` is passed IN by the engine**
  so the hook never touches `RndCam::sCurrent` (Bucket-C safety).
- **Halo (B6)** → `QueryHaloPolicy(RndMat*)` → `HaloPolicy{ forceExclude }`.
  Engine keeps the emissive-map/multiplier DATA test; hook answers only the
  name-based exclusion + `RB3_SMASHER_HALO` flag.
- **Header stays game-agnostic**: forward-declares `RndMesh`/`RndMat` only,
  includes nothing, names no RB3 concept. Litmus test passes.
- **DC3 needs no edit**: methods are NON-pure with base no-op defaults, so
  `HamRenderHook` (overrides only the 2 original pure methods) stays concrete.
  PROVEN: `clang++ -fsyntax-only` of a DC3-shape hook against the new header
  compiles clean (instantiable). No dc3-decomp source touched.

### Site classification (MEASURED 2026-07-06, current code — counts: RB3.cpp 49, Binder 21, Halo 2)

**Bucket A — DEBUG PROBES (stderr-only, no rendered-output change; relocate in S4).**
`Rnd_Wgpu_RB3.cpp`: L2072–2073 (CAM_DBG), L2187–2188 (HUB_BAR_PROBE),
L2209 (RB3_ISOLATE_MESH), L2314 (RB3_HEADMAT_DBG/head.mesh),
L2376/L2428 (MESH_DUMP), L2510 (GEM_VTX), L2636 (RB3_HEADMAT_DBG),
L2817–2820 (BONE_PROBE/BONE_PROBE_NAME outfit list), L3096–3097 (HUB_BAR_PROBE),
L3251/3254/3271 (XBONE_TRACK trackjacket+bone), L3413 (C8_EVERY c8 token list),
L3495 (CHAIN_PROBE), L3783 (IK_SHARD_VERT), L3966 (RB3_HEADMAT_DBG head.mesh).
Binder: L175 (RB3_HEADMAT_DBG skin_diffuse_output probe — the *probe* is A; the
*behavior* skin-RTT branch at binder ~175 is B9 — same name, split cleanly in S3).

**Bucket B — TRUE BEHAVIOR BRANCHES (item payload; one MOVE commit each in S2/S3).**
Confirmed against current code, matches PLAN table B1–B13:
- B1 RB3.cpp L2718–2719 `highlight_main`/`highlight_pattern` (`RB3_NO_HUB_BAR_PLACEMENT_FIX`) — hub-bar world-xfm override.
- B2 RB3.cpp L2752/2755 `scrollbar_bg.mesh`/`scrollbar.mesh` (`RB3_SCROLLBAR_THUMB_FIX_OFF`) — thumb reuses bg world.
- B3 RB3.cpp L2890–2960 facehair/goatee/hair/…/`skeleton_unshared.milo` + bone-name list (`RB3_NO_SKEL_REBAKE`) — dynamic-mesh skel rebake select.
- B4 RB3.cpp L2718-region + L3731–3732 `highlight_main`/`highlight_pattern` (`RB3_NO_HUB_BAR_SHARD_EXEMPT`) — shard-guard exemption.
- B5 RB3.cpp L3871 `skeleton_unshared.milo` (shard-guard region) — band-member shard discriminator.
- B6 Halo L71–72 `surface`/`gem_smasher_glow` (`RB3_SMASHER_HALO`) — IsHaloSourceMat exclusions.
- B7 Binder L90–102 num*/`_source.mesh`/`_comma.mesh`/`.lbl`/font/label — UI-text classification.
- B8 Binder L134–135 `highlight_main`/`highlight_pattern` (`RB3_NO_HUB_HIGHLIGHT_FIX`) — hub highlight colour.
- B9 Binder L175 `skin_diffuse_output` — skin-RTT diffuse.
- B10 Binder L241 `icon` — colour-icon-font useAlphaAsRGB exclusion.
- B11 Binder L279–292 `tail_` + chain names — tail chain-select material.
- B12 Binder L360/373/380–381 mesh-name `crowd`/`extra`/dir `char/crowd/`,`char/extras/`,`skeleton_unshared.milo` — crowd/extras vs band-member path (embeds cam `world.cam` L360 = Bucket C, see below).
- B13 Binder L442/474/502/515/528 `surface.mat`/`rails.mat`/`gem_smasher_glow.mat`/`peakstate`/`prism_gem` (`RB3_TRACK_LIGHT_*`) — highway per-material shading (embeds cam `game.cam` L440 = Bucket C).

**Bucket C — CAMERA/ENVIRON SCENE-SCOPE NAMES → LIGHTING (deferred to Phase 3).**
`Rnd_Wgpu_RB3.cpp`: L1287 `world.cam` (venue lighting gate, `RB3_VENUE_LIGHT*`),
L1335/L1346 `char` environ (`RB3_CHAR_REAL_LIGHT*`), L2146 `world.cam` (per-environ
scene-uniform re-write). Binder: L360 `world.cam` (crowd/extras cam gate), L440
`game.cam` (highway cam gate). L4088 `game.cam` (halo capture cam guard) — cam-name
scene guard, stays inline; the material test it guards (`IsHaloSourceMat`) is B6.

### Bucket-C scope decision (COORDINATOR SIGN-OFF REQUESTED)
**Recommendation ADOPTED (per PLAN §Bucket C + Risks):** relocate only the
mesh/material/dir **asset**-name behavior branches (Bucket B). **Defer the
camera/environ scene-scope name selectors (`world.cam`/`game.cam`/`char` environ,
sites RB3.cpp L1287/L1335/L1346/L2146, Binder L360/L440, halo-cam guard L4088) to
Phase 3** (lighting rewrite W3.x), which subsumes them. Rationale: camera/environ
names are scene-scope selectors, not asset content; routing them through a
per-mesh hook would couple the hook to `RndCam::sCurrent`/`RndEnviron::sCurrent`
global state and collide with the Phase-3 lighting refactor. For B12/B13, the
**mesh/material-name half moves to the hook; the cam-name half stays inline** as an
engine-owned scene condition (the hook receives `camName` as a string only where a
classification genuinely needs it — it never reads the cam global).

**Exit-criterion scoping (grep-zero):** the S4 `grep -E 'strcmp|strstr|strncmp'`
target is **zero RB3 asset (mesh/material/dir) name strings**. Permitted survivors:
the deferred Bucket-C camera/environ scene-scope names listed above (documented,
Phase-3-owned). Coordinator: confirm this scoping before S3 touches B12/B13 cam gates.

### Final ordered relocation commit list (S2 → S4)
S2 (opus, geometric/guard, capture at menu/song-select + band lineup, lineup-gate):
- B1 hub-bar placement → `QueryDrawGeomPolicy.hubBarPlacement` (RB3_NO_HUB_BAR_PLACEMENT_FIX)
- B2 scrollbar thumb → `.scrollbarBg`/`.scrollbarThumb` (RB3_SCROLLBAR_THUMB_FIX_OFF)
- B3 skel-rebake select → geom policy dynamic-mesh signal (RB3_NO_SKEL_REBAKE) [SkinGolden/ClipPose must stay green]
- B4 hub-bar shard exempt → `.shardExemptHubBar` (RB3_NO_HUB_BAR_SHARD_EXEMPT)
- B5 band-member shard discriminator → `.shardBandMember`
S3 (opus, material/halo, capture at gameplay/menu-text/band):
- B6 halo exclusions → `QueryHaloPolicy` (RB3_SMASHER_HALO)
- B7 UI-text class, B8 hub highlight, B9 skin-RTT, B10 colour-icon, B11 tail chain,
  B12 crowd/extras (mesh-name half; cam gate stays inline), B13 highway shading
  (material-name half, may be one "highway look" commit; cam gate stays inline)
  → `QueryDrawMaterialPolicy`
S4 (sonnet, Bucket-A debug probes, one batched MOVE) + final grep-zero census.

### Evidence (S1 commits — all byte-identical by construction)
- **Build green:** `cmake --build native/build-agent-W17 --target rb3-native` and
  `--target rb3-tests` both succeed (clang; the default gcc configure rejects the
  MWCC-compat flags `-fms-compatibility-version`/`-fdelayed-template-parsing` — MUST
  configure with `-DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang`).
- **gtests:** `DrawLogGolden.*` (11) + `StubCensus.*` (2) green post-seam.
  (SkinGolden/ClipPose live in the engine suite milo-engine-tests, not rb3-tests;
  S1 changes no skinning code so they are S2/S3 gates, not S1.)
- **DC3 unbroken:** DC3-shape hook `clang++ -fsyntax-only` against new header =
  clean/instantiable; no dc3-decomp edit.
- **Frame-pass seam inert by construction:** diff = 1 include + two guarded calls
  to EMPTY virtual methods; no reachable draw-submission change. Post-seam build
  renders `main_hub_screen` correctly (~1.87 MB PNG, non-degenerate).
- **Deterministic image-hash A/B NOT usable at this scene (documents a gate gap):**
  the hub boot render is wall-clock, non-frame-deterministic (W0.3-documented). Same
  binary, same target frame → landed frame 300 vs 301 and different sha256 across
  boots. So the screenshot-hash MOVE gate cannot add signal for a guaranteed-inert
  seam here; S1 relies on structural proof + gtests. **S2/S3 note:** their MOVE
  commits (which DO change a code path) need the W0.3b frozen-sim-clock seam OR a
  scene where the hash is stable (or the lineup-gate for skinning/shard, which is
  frame-tolerant) — plain screenshot-hash A/B at the hub will false-positive.

### Remains / blockers
- Bucket-C deferral needs coordinator sign-off (above) before S3 touches B12/B13 cam gates.
- S2/S3 byte-identical gating depends on a frame-deterministic capture (W0.3b) or
  frame-tolerant gates (lineup-gate for B3–B5); flagged above.
- No net-new env flags introduced (S1 relocates nothing); NativeCompatFlags ledger untouched.

## W1.7.S2 — done

Implementer (Opus), 2026-07-06. Relocated the 5 geometric-placement + shard-guard
behaviors (B1–B5) out of `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` into the
game-side hook, ONE behavior per commit, each still behind its existing `RB3_*` flag
(flag read moved INTO the hook). `strcmp/strstr/strncmp` count in the renderer dropped
49 → 28 (the remaining 28 are S3/S4 Bucket-A debug probes + Bucket-C camera names, not
S2 scope). All builds green; lineup gate PASS on every shard/skinning commit.

### Commits (engine `milo-native-engine` + rb3, ordered)
| B | behavior | flag | engine SHA | rb3 SHA |
|---|---|---|---|---|
| B1 | hub-bar placement | RB3_NO_HUB_BAR_PLACEMENT_FIX | `c31b387` | `c18841dd` |
| B2 | scrollbar thumb | RB3_SCROLLBAR_THUMB_FIX_OFF | `2c98705` | `e2bace71` |
| B3 | skel-rebake dyn-mesh/bone/dir select | RB3_NO_SKEL_REBAKE | `280347b` | `23badbd1` |
| B4 | hub-bar shard exempt | RB3_NO_HUB_BAR_SHARD_EXEMPT | `0311467` | `05e902e0` |
| B5 | band-member shard discriminator | (shard-guard region) | `657138a` | (engine-only) |

### Interface used / extended
- B1/B2/B4 use S1's `DrawGeomPolicy` POD fields (`hubBarPlacement`, `scrollbarBg`/
  `scrollbarThumb`, `shardExemptHubBar`) via `QueryDrawGeomPolicy(mesh, outWorld)`,
  fetched ONCE at the top of `DrawMesh` (`geomHook`/`geomPolicy`). Engine keeps ALL
  matrix math (identity+label-translation, scrollbar-bg world cache) + the `skinned`/
  `have` application guards, so no float ordering crosses the seam.
- B3/B5 required per-bone / per-dir string classification that a mesh-only POD cannot
  express, so **GameRenderHook.h was extended** (see DEVIATION 1) with two `const char*`
  classifiers + one POD field: `IsBandMemberSkeletonFile(storedFile)` (B3 bandStatic +
  B5 discriminator, shared), `IsRebakeDynamicBone(boneName)` (B3 per-bone exclusion),
  and `DrawGeomPolicy.skelRebakeMesh` (B3 mesh-level gate = rebake-enabled && !dynamic-
  mesh-name). Engine keeps the bone loops + all rebake math (Invert/Multiply/SetBone).

### Byte-identical evidence (per PLAN §evidence procedure)
- **Structural relocation proof (primary):** each commit is a pure decision relocation —
  identical name-match predicate + identical flag (same `!= 0`/cache-once idiom) computed
  in the hook, applied at the identical engine site with the identical guard. Verified
  null-name parity: B1/B2 short-circuit on null name (`if (mesh->Name())`); B3's
  `dynamicMesh = mn0 && (...)` (a null name is NOT dynamic) is mirrored exactly so
  skelRebakeMesh = !off for a null-named mesh.
- **Build:** `cmake --build native/build-agent-W17S2 --target rb3-native rb3-tests`
  green after every commit (clang; gcc rejects the MWCC-compat flags — configure with
  `-DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang`).
- **rb3-tests:** full suite 70 passed / 1 skipped (`DrawLogGolden.PopulatesFromRealDrawMesh`
  is skipped in-process by design). `StubCensus.*` + `DrawLogGolden.*` green.
- **Lineup gate (shard/skin path — B3/B4/B5):** `scripts/native/lineup-gate.py --bin
  native/build-agent-W17S2/rb3-native` = `verdict=PASS img=PASS segA=PASS ratioB=PASS
  countC=PASS pin=PASS` after B3, B4, and B5. Confirmed the numeric metrics land in the
  same nondeterministic envelope as the baseline (baseline itself PASS pre-change): a B3
  run showing fg_fill=0.53 was a capture-nondeterminism artifact — a re-run on the SAME
  B3 binary gave fg_fill=0.16 matching baseline, and `max_band_ratio` swings 3.34–3.94
  run-to-run on one binary. countC (per-slot draw/skinned/vert counts) PASS is the
  decisive "no draw added/dropped/re-tessellated" signal for the skinning/shard path.
- **DC3 not broken:** new header methods are NON-pure with base no-op defaults; a
  DC3-shape hook (overrides only the 2 original pure virtuals) `clang++ -fsyntax-only`
  compiles clean and is instantiable. No dc3-decomp source touched.
- **Flags:** zero net-new env flags (all 4 relocated, not created) → NativeCompatFlags
  ledger untouched (no regeneration needed).
- **Grep:** B1–B5 asset-name literals (`highlight_main`/`highlight_pattern`/
  `scrollbar_bg.mesh`/`scrollbar.mesh`/`facehair`…/`skeleton_unshared.milo`/bone-name
  list) have ZERO remaining `strcmp|strstr|strncmp` CALLS in `Rnd_Wgpu_RB3.cpp` (only
  prose comments remain). Renderer total 49 → 28 (S3/S4/Bucket-C remainder).

### DEVIATIONS from PLAN.md (recorded per hard-rule requirement)
1. **GameRenderHook.h extended in S2** (PLAN's S2 file list named only Rnd_Wgpu_RB3.cpp
   + rb3_render_hook.cpp). B3/B5 classify per-BONE and per-DIR name strings inside engine
   loops, which S1's `QueryDrawGeomPolicy(RndMesh*, float*)` (mesh-only) cannot express;
   removing those `strstr` calls to hit the grep-zero exit criterion REQUIRES a hook
   surface that takes the string. Added 2 `const char*` classifiers + 1 POD field, all
   with base no-op defaults (DC3-safe). Within S2's goal (relocate B1–B5), not scope
   expansion. Engine `geomHook` was hoisted from B1's if-scope to `DrawMesh` function
   scope (in the B3 commit) so B3/B5 reuse the single fetch.
2. **S1's `DrawGeomPolicy.shardBandMember` field left UNUSED.** B5's discriminator needs
   the owner's bone iteration, so it is answered by `IsBandMemberSkeletonFile` (engine
   keeps the loop) rather than a mesh-only POD field. `shardBandMember` retained for
   source-compat; documented in the header.
3. **Engine skin gtests (SkinGolden.*/ClipPoseFixture.*) NOT run.** `milo-engine-tests`
   is the dc3-FLAVOR engine build and does NOT compile `Rnd_Wgpu_RB3.cpp` (RB3-flavor-
   only per native/CMakeLists.txt); it is also a heavy dc3-coupled build with a
   pre-existing `MeshVertexLoading` failure (W0.1 note) and concurrent dc3 owners. The
   only shared change (GameRenderHook.h additive non-pure virtuals) is proven to compile
   via the DC3-shape syntax check, and the REAL RB3 skinning/shard path (which the skin
   golden abstracts) is directly gated by the lineup gate = PASS for B3/B5. Building the
   dc3 suite fresh would add no signal for this specific renderer name-relocation.
4. **Screenshot-hash A/B not usable for UI behaviors (B1/B2/B4-menu).** The menu render
   is wall-clock non-deterministic even under RB3_FIXED_CLOCK (VERIFIED: two boots of the
   SAME binary → different song-select PNG sha256), exactly the gate gap S1 documented.
   Evidence for the UI relocations is therefore structural proof + build + rb3-tests
   (+ lineup gate for the B4 shard-guard code path, PASS). Frame-deterministic UI capture
   awaits W0.3b maturity.

### Remains / handoff to S3/S4
- S3: material-classification behaviors B6–B13 (RB3HaloPass + RB3MaterialBinder) via
  `QueryDrawMaterialPolicy`/`QueryHaloPolicy` (S1 interface, unused so far).
- S4: Bucket-A debug probes + final grep-zero census.
- Bucket-C camera/environ names still deferred to Phase 3 (S1 decision; coordinator
  sign-off still pending per S1's note).

## COORDINATOR SIGN-OFF (Fable, 2026-07-06) — Bucket-C scoping CONFIRMED

The S1 scoping recommendation is **approved as-is**: relocate only asset-name
(mesh/material/dir) behavior branches (Bucket B); the camera/environ scene-scope
name selectors (`world.cam`/`game.cam`/`char` environ — RB3.cpp L1287/L1335/L1346/L2146,
Binder L360/L440, halo cam guard) stay inline, documented, and are DELETED (not
relocated) by the Phase-3 lighting rewrite that subsumes them. For B12/B13 the
mesh/material-name half moves to the hook and the cam-name half stays inline as an
engine-owned scene condition. The S4 grep-zero exit criterion is scoped to zero RB3
ASSET-name strings, with the deferred Bucket-C survivors listed as permitted. Proceed.

## W1.7.S3 — done

Implementer (Opus), 2026-07-06. Relocated the halo-source NAME exclusions (B6) +
the material-classification behaviors (B7–B13) out of
`milo-native-engine/src/platform/{RB3HaloPass,RB3MaterialBinder}.cpp` into the
game-side hook, ONE behavior per commit (engine commit + rb3 commit per behavior),
each still behind its existing `RB3_*` flag (flag read moved INTO the hook). The
engine keeps ALL uniform/shading math + the Bucket-C camera gates; the hook returns
only WHICH class → uniforms stay bit-identical. RB3HaloPass is now grep-ZERO;
RB3MaterialBinder's remaining `strcmp/strstr` are only Bucket-C camera names
(`world.cam`/`game.cam`, Phase-3-deferred) + two Bucket-A debug probes (S4). All
builds green; rb3-tests 70/70 (+1 skip); lineup gate PASS on the skinning-touching
(B12) + gameplay (B13) commits.

### Commits (engine `milo-native-engine` + rb3, ordered)
| B | behavior | flag | engine SHA | rb3 SHA |
|---|---|---|---|---|
| B6 | halo-source NAME exclusions (IsHaloSourceMat) | RB3_SMASHER_HALO | `6ede0d3` | `c5e6daf2` |
| B7 | UI-text material-name class (num*/_source/_comma/.lbl/font/label) | (text heuristic) | `310affc` | `e3e485e0` |
| B8 | hub-highlight bar colour (highlight_main/pattern) | RB3_NO_HUB_HIGHLIGHT_FIX | `8a9904a` | `636e5dc6` |
| B10 | colour-icon-font useAlphaAsRGB exclusion (`icon`) | (color-icon) | `54d42cd` | `f9601c3b` |
| B11 | tail-chain fret colour (tail_green/red/...) | (tail chain) | `56e8c55` | `b16092e6` |
| B12 | crowd/extras vs band-member path (crowd/extra/char/crowd//char/extras/) | RB3_CROWD_DIM_OFF (master, stays inline) | `fd48f1a` | `d3d02460` |
| B13 | highway shading (surface/rails/gem_smasher_glow/peakstate) | RB3_TRACK_LIGHT_OFF (master, stays inline) | `efbc981` | `8b68484f` |

### Interface used / extended
- **B6** → `QueryHaloPolicy(RndMat*)` → `HaloPolicy.forceExclude` (S1 field). Engine
  keeps the emissive-map/multiplier DATA test; hook owns the surface / gem_smasher_glow
  name exclusions + the RB3_SMASHER_HALO opt-in (cached once).
- **B7/B8/B10** → S1's `DrawMaterialPolicy` fields `isUiText`/`isHubHighlight`/
  `isColorIcon` via `QueryDrawMaterialPolicy(mesh,mat,skinned,owner,camName)`, fetched
  ONCE at the top of the binder's `if(mat)` block (`matHook`/`matPolicy`). Engine keeps
  the empty-name RndText discriminator (`isTextMeshHeur`, a direct `'\0'` compare — NOT
  an asset-name string) and ORs/applies each class.
- **B11** required the per-fret colour, which S1's `bool isTailChain` cannot express, so
  **GameRenderHook.h was extended** (DEVIATION 1) with `DrawMaterialPolicy.tailForceColor`
  + `float tailColor[3]` (name-derived scalars the engine writes into `mu.color[0..2]` +
  `useTexture=0`; the fret-name match + colour TABLE live entirely in the hook). Float
  ordering never crosses the seam. `isTailChain` retained (unused, source-compat).
- **B12** used per-string classifiers (engine keeps the owner-bone loop + the `world.cam`
  scene gate — Bucket C — inline), mirroring S2's B3/B5 pattern. **Extended the header**
  (DEVIATION 1) with `IsCrowdExtraMeshName(const char*)` + `IsCrowdExtraDir(const char*)`;
  band-member discrimination reuses S2's existing `IsBandMemberSkeletonFile`.
- **B13** used S1's `DrawMaterialPolicy.highwayClass`. **Extended the header**
  (DEVIATION 1) with a NEUTRAL `enum HighwayMaterialClass { kHighwayNone/Surface/Rails/
  Smasher/Peakstate }` so engine + hook share the class contract without the header
  naming a specific asset file. Engine keeps `sTrackLight` + the `game.cam` gate (Bucket
  C) + the EXACT if/else-if/if/if structure + ALL shading math + its sub-flags
  (RB3_HIGHWAY_WATERMARK_*, RB3_FRET_GLOW_OFF); the hook only maps material name → class.

### Bucket-C handled per S1 decision (camera names stay inline)
- B12's `world.cam` and B13's `game.cam` gates were NOT relocated — they remain
  engine-owned scene-scope conditions read from `RndCam::sCurrent` INSIDE the binder
  (engine code). The hook NEVER touches `RndCam::sCurrent`: `QueryDrawMaterialPolicy`
  receives `camName` as a string arg (the engine passes `RndCam::sCurrent->Name()`), and
  no S3 classification actually consumes it (the material-name classes are cam-agnostic;
  the engine applies highwayClass/crowd-dim only inside its own cam gate). This matches
  the S1 Bucket-C scoping exactly.

### B9 (skin_diffuse_output) — reclassified Bucket-A, deferred to S4 (DEVIATION 2)
PLAN listed B9 as a material-classification BEHAVIOR branch, but the ONLY
`skin_diffuse_output` occurrence in the current binder (L168, was ~175) is inside the
`if (getenv("RB3_HEADMAT_DBG"))` DEBUG probe, and the `skinRt` local it computes is a
DEAD variable (never read by the fprintf). It changes NO rendered output — it is a
stderr-only Bucket-A debug probe (exactly as S1's own site classification already
listed L175 under Bucket A). There is no separate skin-RTT behavior branch keyed on
this name in the binder. Left in place; it will move with the other Bucket-A probes in
S4 (byte-identical, logging-only). `DrawMaterialPolicy.isSkinRtt` stays unused.
Similarly `prism_gem` (L517) is inside the `GEM_FORCE` debug probe → Bucket A / S4, not
B13 behavior (PLAN's B13 row listed prism_gem, but it is debug-only).

### Byte-identical evidence (per PLAN §evidence procedure)
- **Structural relocation proof (primary):** each commit relocates a DECISION only —
  identical name-match predicate + identical flag (same `!=0` / cache-once idiom)
  computed in the hook, applied at the identical engine site with identical guards and
  identical uniform/shading math. Null/empty-name parity verified per behavior (e.g.
  B7 hook's `meshName && meshName[0]` + `matName && matName[0]` mirrors the binder's
  prior guards; the engine's `isTextMeshHeur || matPolicy.isUiText` yields the same
  final boolean regardless of whether the hook also runs the font/label check on an
  empty-name mesh). B13 preserves the EXACT if/else-if/if/if control structure; the
  highway classes are mutually exclusive by exact material name, so a single-value
  `highwayClass` is equivalent to the four inline name tests.
- **Build:** `cmake --build native/build-agent-W17S3 --target rb3-native rb3-tests -j8`
  (clang: `-DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang`)
  green after EVERY commit.
- **rb3-tests:** 70 passed / 1 skipped (`DrawLogGolden.PopulatesFromRealDrawMesh`,
  skipped by design). `StubCensus.*` + `DrawLogGolden.*` green after each commit.
- **Lineup gate (skinning/gameplay — B12 crowd path + B13 highway):**
  `scripts/native/lineup-gate.py --bin native/build-agent-W17S3/rb3-native` =
  `verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` after B12 (crowd
  skinned path) AND B13 (gameplay highway materials). countC (per-slot draw/skinned/
  vert counts) PASS = no draw added/dropped/re-tessellated on the skinned path.
- **DC3 not broken:** all new header surface is additive + NON-pure (POD fields, the
  `HighwayMaterialClass` enum, `IsCrowdExtra*` classifiers with base `return false`
  defaults). A DC3-shape hook (overrides only the 2 original pure virtuals) `clang++
  -std=c++17 -fsyntax-only` compiles clean AND is instantiable (no leftover pure
  virtual). No dc3-decomp source touched.
- **Flags:** ZERO net-new env flags (all relocated, not created) → NativeCompatFlags
  ledger untouched.
- **Screenshot-hash A/B:** not usable at menu/hub (wall-clock non-determinism, the
  S1/S2-documented gate gap); the gameplay-scene captures are covered by the
  frame-tolerant lineup gate above.

### Grep census after S3 (S4 will finish grep-zero)
- `RB3HaloPass.cpp` → **0** `strcmp/strstr/strncmp` (B6 fully cleaned it).
- `RB3MaterialBinder.cpp` → 4 survivors, ALL out of S3 scope:
  - L168 `skin_diffuse_output` — Bucket-A debug probe (RB3_HEADMAT_DBG) → S4.
  - L517 `prism_gem` — Bucket-A debug probe (GEM_FORCE) → S4.
  - L340 `world.cam`, L424 `game.cam` — Bucket-C camera scene-scope names → Phase 3.
- `Rnd_Wgpu_RB3.cpp` unchanged by S3 (S2 dropped it 49→28; S4 handles its Bucket-A).

### Remains / handoff to S4
- S4: relocate the Bucket-A debug probes (incl. binder L168 skin_diffuse_output + L517
  prism_gem) and run the final grep-zero census. After S4, the only permitted survivors
  are the Bucket-C camera/environ scene-scope names (Phase-3-deferred, S1-documented).
- Bucket-C deferral still pending explicit coordinator sign-off (S1's open note); S3 did
  NOT touch the cam gates, so no sign-off was required to proceed.

## W1.7.S4 — done

**Goal.** Relocate the Bucket-A stderr-only debug-probe name matches (CAM_DBG,
RB3_HEADMAT_DBG, GEM_VTX/GEM_FORCE, BONE_PROBE, XBONE_TRACK, HUB_BAR_PROBE) out of
the engine renderer so `grep -E 'strcmp|strstr|strncmp'` on Rnd_Wgpu_RB3.cpp +
RB3MaterialBinder.cpp + RB3HaloPass.cpp shows zero RB3 asset (mesh/material/dir)
name literals, then run the final verification sweep.

### Commits (3, interface → implementation → wiring, per hard rule 1)
1. Engine `013ec6a` — "add debug-probe name classifiers to GameRenderHook (no-op
   defaults, unused)": 7 new non-pure virtuals (`IsCamDbgHighwayMesh`, `IsHubBarMesh`,
   `IsHeadMesh`, `IsSkinDiffuseOutputTex`, `IsGemMesh`, `IsBoneProbeDefaultMesh`,
   `IsTrackjacketMesh`), base default `return false`. Scaffold only.
2. rb3 `d194da76` — "implement debug-probe name classifiers in rb3_render_hook":
   `BandRenderHook` overrides with the real RB3 literal matches moved in verbatim
   from the engine call sites.
3. Engine `41b9e3a` — "relocate Bucket-A debug-probe asset-name literals to hook":
   rewired all 9 call sites (CAM_DBG L2097-2100; HUB_BAR_PROBE draw-level L2213-2215
   and per-bone L3118-3120; RB3_HEADMAT_DBG head.mesh L2339-2340/L2661-2662/L3990-3991;
   GEM_VTX L2534-2536; BONE_PROBE default-list L2847-2850; XBONE_TRACK mesh filter
   L3272-3273; RB3MaterialBinder.cpp skin_diffuse_output L166-168 and GEM_FORCE
   prism_gem L516-518) to call the new hook methods. Pure MOVE — every probe's env-flag
   gate, one-shot/throttle bookkeeping, and fprintf/uniform-override body is byte-for-
   byte unchanged; only the name-match predicate is now hook-mediated.

### Dedup decisions (documented, not scope creep)
- `GEM_VTX` (Rnd_Wgpu_RB3.cpp) and `GEM_FORCE` (RB3MaterialBinder.cpp) both test the
  same literal `"prism_gem"` → share ONE classifier `IsGemMesh`, consistent with the
  existing `IsBandMemberSkeletonFile` reuse pattern from S1/S2.
- Draw-level and per-bone `HUB_BAR_PROBE` occurrences share ONE classifier
  `IsHubBarMesh`. Deliberately NOT reusing the existing B1/B4
  `hubBarPlacement`/`shardExemptHubBar` policy fields — those are ANDed with the
  unrelated `RB3_NO_HUB_BAR_*_FIX` opt-out flags, and the debug probe must fire
  regardless of those production toggles. `IsHubBarMesh` is a pure name test with no
  flag entanglement.
- `BONE_PROBE`: only the *default* 5-name list (`plaidshirt`/`trackjacket`/`shirt`/
  `jacket`/`vestdenim`) was relocated (`IsBoneProbeDefaultMesh`). The
  `BONE_PROBE_NAME=<substr>` env-override branch stays inline in the engine — it
  carries no baked-in RB3 literal, just a runtime user-supplied selector string.
- `RB3_VENUE_PROBE`: investigated, has no hardcoded-literal strstr/strcmp of its own
  to relocate (only gates an `fprintf` off a plain int flag). The nearby
  `strstr(envNm, "char")` at L1336/1347 is a pre-existing, separate Bucket-C concern
  (`sCharRealLight()`), already correctly classified as such by S1/S3 and out of S4
  scope.

### Final grep census (exit criterion met)
```
$ grep -nE 'strcmp|strstr|strncmp' Rnd_Wgpu_RB3.cpp RB3MaterialBinder.cpp RB3HaloPass.cpp
```
- `RB3HaloPass.cpp` → **0** (already clean since S3/B6).
- `RB3MaterialBinder.cpp` → 2 survivors, both Bucket-C (unchanged from S3, S4 didn't
  touch them): L341 `world.cam`, L425 `game.cam` (camera scene-scope names,
  Phase-3-deferred, coordinator-approved in S1).
- `Rnd_Wgpu_RB3.cpp` → 11 survivors:
  - Bucket-C (5, deferred, unchanged): L1288 `world.cam`, L1336/L1347 `char` (env-name,
    `sCharRealLight`), L2173 `world.cam`, L4110 `game.cam` (halo-capture cam guard).
  - Generic env-var-driven selectors with **no hardcoded RB3 literal** (6, left in
    place by design — the compared value is a runtime `getenv()` string, not game
    content baked into engine source, so these do not violate the "zero RB3 asset
    name" wording): L2235 `RB3_ISOLATE_MESH`, L2402/L2454 `MESH_DUMP` (x2), L2847
    `BONE_PROBE_NAME` override branch, L3276/L3293 `XBONE_TRACK`/`XBONE` bone-name
    filters, L3435 `C8_PROBE` token, L3517 `CHAIN_PROBE`, L3802 `IK_SHARD_VERT`. This
    is the same category S1/S3 already used for `RB3_ISOLATE_MESH`; documenting here
    rather than silently leaving undocumented, per PLAN.md's "if excluded, document"
    fallback (no coordinator sign-off needed since these were never in the literal-
    relocation set — S1's Bucket-A/B/C census only flagged the *hardcoded-literal*
    sub-expressions within these probes, which are now gone).

All hardcoded RB3 asset-name literals inside `strcmp/strstr/strncmp` calls in these
3 files are now relocated to `rb3_render_hook.cpp`. Remaining survivors are either
Bucket-C (explicitly deferred to Phase 3, already coordinator-signed-off in S1) or
carry zero compiled-in RB3 asset knowledge.

### Verification
- **Build:** `cmake --build native/build-agent-W17S4 --target rb3-native rb3-tests -j8`
  (clang: `-DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang`)
  green after every commit, including a from-scratch re-verify after all 3 commits
  landed.
- **rb3-tests:** 70 passed / 1 skipped (`DrawLogGolden.PopulatesFromRealDrawMesh`,
  pre-existing skip, unrelated to this subtask). `StubCensus.*` + `DrawLogGolden.*`
  all green.
- **Rendered-output risk:** none by construction — every relocated site is either
  stderr-only (`fprintf`) or an opt-in debug override gated behind a `getenv()` check
  that defaults off (`GEM_FORCE`'s magenta-material override). No production
  (non-debug-gated) code path was touched. Did not attempt a screenshot-hash A/B —
  per S1/S2's established finding that menu/hub scenes are wall-clock-nondeterministic
  and this subtask's own description states "no rendered-output risk (logging-only)",
  so build-green + rb3-tests-green + the grep census constitute the evidence bar here,
  consistent with how prior logging-only moves in this item were verified.
- **DC3 not broken:** all 7 new `GameRenderHook` methods are non-pure with `return
  false` defaults — purely additive header surface, same pattern as S1-S3's
  classifiers. No dc3-decomp source touched.
- **Flags:** ZERO net-new env flags (CAM_DBG, HUB_BAR_PROBE, RB3_HEADMAT_DBG, GEM_VTX,
  GEM_FORCE, BONE_PROBE/BONE_PROBE_NAME, XBONE_TRACK all pre-existing, relocated not
  created) → `NativeCompatFlags` ledger untouched.

### Deviations from PLAN.md
- None. Followed the suggested commit sequence (interface scaffold → rb3
  implementation → engine wiring → grep-zero census) exactly.

### Remains / handoff
- W1.7 Bucket-A relocation is now complete. Only Bucket-C camera/environ scene-scope
  names remain inline across all 3 files, per the S1-documented, coordinator-signed-off
  Phase-3 deferral — no further action needed from this item.
- The item-level exit criteria (per PLAN.md) should now be re-checked in full by a
  verifier/coordinator pass over S1-S4 collectively; S4 did not re-verify S1-S3's own
  claims beyond the grep census and a full rebuild.

## VERIFY — complete (all exit criteria MET, gates green)

Verifier (Opus), 2026-07-06. Ran the PLAN exit criteria for real in own build dir
`native/build-agent-W1.7-verify` (clang configure) + re-derived byte-identical
evidence for a sample of MOVE commits myself (did not just trust S1–S4 claims).
Engine HEAD `41b9e3a`, rb3 HEAD `5a85c7e1`. All 16 W1.7 engine commits + all rb3
hook commits present in git log.

### Per-criterion evidence
1. **Frame-pass seam live — MET.** `Rnd_Wgpu_RB3.cpp` dispatches both existing
   methods via `GetGameRenderHook()`: `RenderCharacterImpostors(this)` at BeginFrame
   (L1621–1622, after DrawPreClear) and `DrawGameOverlay(this)` at EndFrame
   (L1669–1670, before Finish). INERT: `BandRenderHook` overrides both as no-ops
   (`native/src/rb3_render_hook.cpp` L44–52) → byte-identical.
2. **Asset-name grep-zero — MET.** `grep -cE 'strcmp|strstr|strncmp'`:
   RB3HaloPass.cpp=0, RB3MaterialBinder.cpp=2, Rnd_Wgpu_RB3.cpp=14. Inspected ALL 16
   survivors: ZERO hardcoded RB3 asset (mesh/material/dir) name literals. Survivors =
   (a) 7 Bucket-C camera/environ scene-scope names (`world.cam`×3, `game.cam`×2,
   `strstr(envNm,"char")`×2) — Phase-3-deferred, coordinator-signed-off in S1; (b) 9
   generic env-var-driven selectors whose compared token is a runtime `getenv()`
   string, not baked-in RB3 content (verified L3435 C8_PROBE tok = getenv token list,
   L3802 IK_SHARD_VERT sSel = getenv, L2235 RB3_ISOLATE_MESH, L2402/2454 MESH_DUMP,
   L2847 BONE_PROBE_NAME, L3276/3293 XBONE_TRACK, L3517 CHAIN_PROBE).
3. **Every relocated behavior byte-identical — MET (sampled + re-derived by verifier).**
   Re-derived TWO MOVE commits from the diffs myself:
   - **B6 halo** (engine `6ede0d3` + rb3 `c5e6daf2`): engine keeps the emissive-map/
     multiplier DATA test + the `if(!mat)return false` guard; hook `QueryHaloPolicy`
     reproduces the exact `surface`→exclude and `gem_smasher_glow`→exclude-unless-
     RB3_SMASHER_HALO logic with the identical `static int sSmasherHalo=-1` cache-once
     idiom. Truth-table equivalent on all 4 name/flag cases. Byte-identical.
   - **B1 hub-bar** (engine `c31b387` + rb3 `c18841dd`): engine keeps the `skinned &&`
     gate and ALL matrix math; hook returns `hubBarPlacement = name!=null &&
     (strncmp highlight_main==14||highlight_pattern==17) && !RB3_NO_HUB_BAR_PLACEMENT_FIX`.
     Combined `skinned && policy` == prior inline expression exactly. Same strncmp
     lengths, same getenv, same cache-once. Byte-identical.
   - **Lineup gate (real skinning/shard path, B3–B5/B12) PASS on the fresh build**:
     `scripts/native/lineup-gate.py --bin build-agent-W1.7-verify/rb3-native` →
     `verdict=PASS img=PASS segA=PASS ratioB=PASS countC=PASS pin=PASS` (max_band_ratio
     3.53, in golden envelope). countC PASS = no draw added/dropped/re-tessellated.
   - Screenshot-hash A/B not re-attempted: menu/hub wall-clock non-determinism is the
     S1/S2-documented gate gap; structural re-derivation + lineup gate are the correct
     substitute (same conclusion the implementers reached).
4. **Flags preserved — MET.** All relocated RB3_* flags (RB3_NO_HUB_BAR_PLACEMENT_FIX,
   RB3_SCROLLBAR_THUMB_FIX_OFF, RB3_NO_SKEL_REBAKE, RB3_NO_HUB_BAR_SHARD_EXEMPT,
   RB3_SMASHER_HALO, RB3_NO_HUB_HIGHLIGHT_FIX, RB3_TRACK_LIGHT*, RB3_CROWD_DIM*) now
   read INSIDE `rb3_render_hook.cpp` with prior semantics. ZERO net-new env flags →
   NativeCompatFlags ledger untouched (correct — relocation, not creation).
5. **DC3 not broken — MET.** `GameRenderHook.h` is forward-decl-only, game-agnostic;
   all 15 W1.7 methods are NON-pure with base `return {}`/`false` defaults. DC3's
   `HamRenderHook` overrides ONLY the 2 original pure virtuals (dc3_render_hook.cpp
   L57,L68) → stays concrete. Verifier compiled a DC3-shape hook (overrides only the 2
   pure virtuals) `clang++ -std=c++17 -fsyntax-only` against the CURRENT header →
   clean + instantiable. No dc3-decomp source touched.
6. **gtests green — MET.** `rb3-tests` (own build): 70 passed / 1 skipped
   (`DrawLogGolden.PopulatesFromRealDrawMesh`, skip-by-design). StubCensus.* +
   DrawLogGolden.* green. Engine skin/bone safety nets (SkinGolden.* / ClipPoseFixture.*)
   run from a current engine-tests build (from dc3 orig-assets root): 15 passed / 1
   skip-by-design (`SkinGolden.CaptureGolden`); ALL of ClipPoseFixture (incl.
   `EffectorWorldPositionsMatchGolden`) green, and the fail-red control
   `SkinGolden.BrokenSkinDivergesFromGolden` PASS = the golden provably discriminates a
   broken skin. (Engine suite NOT rebuilt at W1.7 HEAD — accepted per S2 Deviation 3:
   the suite is dc3-flavored/doesn't compile Rnd_Wgpu_RB3.cpp, GameRenderHook.h's
   additive change is proven to compile in the DC3 shape, and SkinVertex/CharBones math
   is not in W1.7's edit set, so the tests are structurally immune; the real rb3
   skinning path is directly gated by the lineup gate above.)

### Build/config used
`cmake -B native/build-agent-W1.7-verify -S native -DCMAKE_CXX_COMPILER=/usr/bin/clang++
-DCMAKE_C_COMPILER=/usr/bin/clang` then `cmake --build … --target rb3-native rb3-tests
-j8` → both green.

### Fail-red posture (MOVE-only item)
W1.7 is a pure behavior-preserving relocation, so there is no NEW behavior to prove
fails-red. The GATES used to verify it are proven-discriminating: SkinGolden's
`BrokenSkinDivergesFromGolden` control is green, DrawLogGolden's `CatchesPipelineChange`
is green, and the lineup gate's broken-skin FAIL was proven in W0.5. The gate machinery
can fail red; the good build passes it.

### Verdict
**W1.7 COMPLETE. All 6 exit criteria MET, all gates green.** No blockers. No small
fixes needed (no in-scope defects found). Non-blocking follow-up (owned elsewhere): the
7 Bucket-C camera/environ scene-scope name survivors are deleted by the Phase-3 lighting
rewrite (W3.x), as designed; the engine milo-engine-tests suite retains its 29
pre-existing dc3-drift failures (W2-TESTFIX lane), none touching W1.7 files.
