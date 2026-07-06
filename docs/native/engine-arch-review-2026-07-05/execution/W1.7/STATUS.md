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
