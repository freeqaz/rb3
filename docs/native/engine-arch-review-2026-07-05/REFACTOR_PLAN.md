# Milo Native Engine — Refactor Plan (path to "amazing")

**Date:** 2026-07-05 · **Companion to** `ARCHITECTURE_REVIEW.md`. This is the implementation-fleet
handoff. Verdict: `targeted-refactor`. The systemic causes are SYS-1..SYS-7 in the review doc;
phases below reference them.

## The one hard rule (learned from two BandPatchMesh reverts)

> **Nothing structural lands before its regression gate exists.** A commit either **MOVES** code
> (behavior-preserving, visual-diff byte-identical) **or CHANGES** behavior (gated by a golden that
> can fail red) — **never both in one commit.** The C8 and BandPatchMesh regressions were exactly
> move-and-change commits that slipped a blind gate. Phase 0 builds the gates; every later phase is
> blocked on the specific gate that would catch its failure mode.

Disposition legend: **OVERHAUL** = rewrite/replace a subsystem · **REFACTOR** = mechanical,
behavior-preserving extraction · **KEEP** = faithful/sound, do not touch.

---

> **STATUS (coordinator, 2026-07-06 after Wave 6): THE SYS-1 PLACEMENT FIX SHIPPED.**
> `RB3_PLACEMENT_CONTRACT` flipped **default-ON** (engine `fced18b`) — the Wave-5 wash hold was
> refuted by a songMs-pinned interleaved measurement (A/A-variable: wash present flag-OFF, Mann-
> Whitney p=1.0; filed as its own flip-independent backlog item). Crowd + drum now draw at faithful
> placements by default; opt-out `RB3_PLACEMENT_CONTRACT_OFF`. Also shipped default-ON: the **black
> singer head fix** (W2.7 — nested-subdir head texture unreachable by non-recursive Find). Landed
> gated: hub grey-quad hide (W4.1), projLight + fog-verified `RB3_ENV_FOG` (W3.1b). Root-caused for
> Wave 7: grayscale song-start (native composite over-exposure, patch staged), menu-text color floor
> (`RB3MaterialBinder.cpp:145`), **finger-shard = rotation-basis invBind mismatch** (W2.2 oracle
> blind to it — far-vertex metric first). W3.2 box-ambient prototype **refuted its own premise**
> (venues have zero directional approx lights; the real SYS-4 gap is point-lights). Wave 6 table +
> backlog in `execution/README.md`.
>
> **STATUS (coordinator, 2026-07-06 after Wave 5):** Draw order made deterministic (W0.3d-fix);
> **the crowd/drum placement fix is ready-to-ship** (W2.1-flip staged as a one-line default toggle +
> opt-out + drum oracle + Dolphin A/B). **Coordinator HELD the flip on visual sign-off** — the
> flag-ON Dolphin captures show an asymmetric bloom blow-out (one frame washes white in flag-ON but
> not flag-OFF), consistent with W2.1.S2's noted crowd-emissive→bloom failure mode; flipping waits on
> characterizing/fixing it (→ W2.1-flip-blocker, Wave 6). Fog fill landed default-OFF but
> asset-blocked from visual verification (no shipped venue authors fog → W3.1b). 91 game flags
> classified (census clean, 321). Engine `8e7eddd`, pin bumped. **The bloom/exposure blow-out is now
> the gating issue for the visible payoff of the placement fix AND squarely in the SYS-4 lighting
> frontier — Wave 6 leads with it.** See `execution/README.md` Wave-5 table + FLIP DECISION.
>
> **STATUS (coordinator, 2026-07-06 after Wave 4):** **SYS-1 placement FIXED** — W2.1 landed the
> skinned-placement contract (`obj.world = WorldXfm()` + bind-relative palette) behind
> `RB3_PLACEMENT_CONTRACT`, a **vertex-invariant** reorg (0.0000 diff) proven by a crowd-spread
> oracle (coordinator reproduced RED-off/GREEN-on), engine `609efb7`, pin bumped. Default-OFF; ships
> at **W2.1-flip (Wave 5)** after subsuming the name-scoped hacks + drum oracle + Dolphin A/B. W2.3
> refuted (crowd already self-owned; rebind retained). W0.3d froze CharEyes RNG (per-name eps, N=36).
> **Three worst mesh bug families now have landed fixes behind flags: hands/fingers (W2.2 partly
> default-ON), crowd/drum placement (W2.1), state-leak (W1.6 shipped).** Open: W2.1-flip (ship it),
> W0.6b (flag-registry census gap — still doesn't scan rb3/src/system), W0.3d-fix (staged loader-order
> patch), and **lighting (SYS-4, barely started) is now the biggest open frontier** → W3.1 (fills,
> DC3-gated) then W3.2 (BoxMap). Then Phase 4 UI. See `execution/README.md` Wave-4 table.
>
> **STATUS (coordinator, 2026-07-06 after Wave 3):** **SYS-3 FIXED** — W1.6 replaced the mutable
> mid-frame scene bind group with an immutable `RB3SceneBinding` threaded via `RB3DrawContext`
> (engine `6221a56`, pin bumped, verified byte-identical). **Hands/fingers fix (W2.2) LANDED
> default-OFF** behind `RB3_SKEL_REBIND_FULL` with a fail-red bind-pose-identity oracle (the invariant
> BandPatchMesh lacked); one coordinator-signed flag-flip from shipping pending head-graze
> adjudication + Dolphin A/B (→ W2.2-flip, Wave 4). W2.5 waypoint diagnostic landed. **W0.3c partial:**
> Exit-A structurally impossible (no transparent sort in rb3 binary), Exit-B canonical-multiset
> comparator delivered but its "15/15 green" bar is blocked by the pre-existing CharEyes eps residual
> (→ W0.3d, Wave 4); W1.6 used it correctly for world-xfm-invariant changes. **Next (Wave 4):** Phase-2
> placement (W2.1 skinned-placement contract + W2.3 GeomOwner) now unblocked on the DrawContext'd
> `DrawMesh`; W0.3d; W2.2-flip; W3.1 lighting fills. See `execution/README.md` Wave-3 table.
>
> **STATUS (coordinator, 2026-07-06 after Wave 2):** Phase 0 nets green (W0.1/W0.2/W0.4/W0.5/W0.6);
> **Phase 1 monolith decomposition DONE** — `Rnd_Wgpu_RB3.cpp` 7,017→4,747 lines, six TUs extracted
> (W1.1–W1.5 ✅), and **W1.7 relocated all 13 asset-name branches to the game hook (SYS-2 structurally
> addressed) ✅.** Engine pin `41b9e3a`. Engine-test drift cleared (W2-TESTFIX: 198/0). **Open:**
> W0.3b ⚠️ partial — draw-COUNT determinism achieved but a **draw-submission-ORDER nondeterminism**
> (~33% flake, mesh-identity swaps) surfaced = live instance of SYS-3; new item **W0.3c** (Wave 3)
> root-causes+fixes it, which also unblocks **W1.6** (deferred, the SYS-3 DrawContext fix). See
> `execution/README.md` Wave-2 table. **Phase-2 skinning-bind work (W2.2, hands/fingers) can proceed
> now** on the deterministic skin/effector goldens; placement work (W2.1/W2.3) waits for W1.6.

## Phase 0 — Safety nets first (BLOCKS everything structural)

**Goal:** make every bug on the list *catchable by an automated gate that can fail red* before any
correctness rewrite is attempted. This phase writes tests and infrastructure, changes no rendering
behavior, and can run fully in parallel with Phase 1 (which is behavior-preserving motion).

**In scope / OVERHAUL of the test posture (SYS-6, SYS-7):**
- **W0.1 CPU reference skinner + per-vertex golden gtest** *(highest leverage — lane 02 rec 5a, lane 06 §4.3.1)*. Standalone LP64 function implementing the decomp skinning semantics (`BoneOffsetAt(i)·boneWorld_i`, weighted sum, `idx<NumBones` skip, `GetX()` weights); run in gtest against a real char asset + clip, assert final 4-bone blended **world** positions vs a committed golden within epsilon. This is the oracle the twice-reverted BandPatchMesh rewrite never had. New file `milo-native-engine/tests/test_skin_golden.cpp`.
- **W0.2 Loud-by-default stubs** *(lane 06 §2.3)*. Replace the shared `xorl/ret` no-op (`band3_link_stubs.s:19-21`) with a `__hmx_stub_hit(name)` shim that log-once-per-symbol to stderr, plus a startup census dumping linked-vs-hit stubs, plus a machine-checkable registry classifying each of the 832 weak syms as `assert-unreachable | ok-noop | data-blob`, gated in CI. Turns the prose "none is reached" comment into an enforced invariant. Would have surfaced particles + NetSession on frame 1.
- **W0.3 Per-draw state-log golden** *(lane 06 §4.3.2)*. Emit a structured per-draw record (pipeline id, blend, bind-group handles, world xfm, vert/index counts) to a ring; gtest replays a canned scene and diffs the draw log against a golden. Catches co-location (identical world xfm across instances that should differ → crowd/drum) and the uniform-collapse class (`a0f98ad`). New `tests/test_draw_log_golden.cpp`.
- **W0.4 Bone ground-truth expansion to live pose** *(lane 06 §4.3.4)*. Extend `test_bone_ground_truth.cpp` from topology/symmetry to *live pose*: apply a clip, assert effector world positions (hand, foot, drum-stick tip) against golden. The placement-bug net.
- **W0.5 Non-blind lineup gate** *(lane 02 rec 5e, lane 06 §4.3.3)*. Fix band-closeup blindness: a **patch-bearing** lineup (BandPatchMesh chars in frame) + **wide reviewer-judged** frames + **per-draw-count and per-mesh-bbox numeric assertions layered under the SSIM/image compare** so a shard explosion fails a number even when the image metric passes.
- **W0.6 `NativeCompat` flag registry + generated tracking doc** *(lane 06 §3.3)*. Collapse the ~229 scattered `getenv` reads into one typed registry `{name, default, class ∈ [probe|workaround|feature|perf], owner, faithful-alternative-status}` read once at startup; call sites read the struct. Generate the tracking doc from it. Compile `class=probe` flags out of release builds. This is the burn-down ledger every later phase deletes rows from.

**Out of scope:** any change to rendered output.

**Exit gate (measurable):**
1. W0.1 golden passes on the current build **AND** a deliberately-broken skin (force full-body rebind, `RB3_SKEL_REBIND_FULL=1`) **fails red**.
2. W0.5 numeric per-mesh-bbox check **fails red** on a captured exploded-patch-shard frame (the exact frame the old gate passed 34/34).
3. W0.2 census emits a non-empty hit-list on boot; CI fails if an unclassified stub resolves to no-op.
4. All gates wired into the integration-worktree full-rebuild CI.

**Flags deleted this phase:** none (registry is built; deletions start Phase 2). Probe flags become compiled-out in release.

**Effort:** 6–10 agent-sessions. **Quick wins (week 1):** W0.2 loud stubs (small, catches the whole invisible-failure class), W0.6 registry skeleton + generated doc.

---

## Phase 1 — Decompose the render monolith (REFACTOR; parallel with Phase 0)

**Goal:** turn `Rnd_Wgpu_RB3.cpp` (7,017 lines, `DrawMesh` ~2,670) into a thin `BandRnd` adapter over
cohesive units, **byte-identical output at every commit**, so the later correctness work happens in
legible code. This is the "one scoped OVERHAUL" from the review — executed as mechanical REFACTOR.

**In scope (lane 01 §5):**
- **W1.1 Externalize the 5 inline WGSL modules** (`kRB3HaloBlitShaderSource:2350`, `kRB3PostProcShaderSource:2703`, `kRB3QuadShaderSource:3267`, `kRB3ComposeShaderSource:3376`, `kRB3ParticleShaderSource:6631`) to `src/gfx/Shaders/*.wgsl` loaded like `standard_wgsl.inc`, with offline `naga`/`tint` validation in CI. String move only.
- **W1.2 Extract `RB3MeshEntry` upload cache** → a `MeshUploadCache` TU (converge onto `gfx/MeshGpuCache` where provably identical; comment at `:389` already cites it).
- **W1.3 Extract material→uniform binder** → `MaterialBinder` TU (converge onto `gfx/MaterialSetup`; comments `:5766/:5974`).
- **W1.4 Extract postproc/halo/quad** → TUs, converging onto `gfx/PostProcPass` + `gfx/DrawRect2D` where the RB3 port is a proven hand-copy (comments `:2665/3033/3060`).
- **W1.5 Dedupe the two uniform-ring classes** (`BandUniformRing` vs `UniformRingBuffer`) into one.
- **W1.6 [OVERHAUL-within-refactor] Replace mutable `mSceneBindGroup`** (SYS-3) with an immutable-per-write scene bind group returned from a `SceneUniforms` unit and bound explicitly per draw via a `DrawContext` value object (world xfm, material params, bone-palette handle, pipeline key). Same bytes, but the §2f state-leak class is gone by construction. **Gated by W0.3 per-draw golden.**
- **W1.7 Wire `GameRenderHook` for RB3** (`rb3_render_hook.cpp:14-20`, currently a permanent no-op) and relocate the ~30 asset-name branches out of engine `DrawMesh` into `rb3/native/src/rb3_render_hook.cpp`, **one behavior per commit**, each still behind its existing `RB3_*` flag. The engine stops knowing RB3 asset names; behavior identical, just relocated.

**Dependencies:** W1.6 needs W0.3. Otherwise independent of Phase 0.

**Exit gate:** visual-diff byte-identical per commit; `Rnd_Wgpu_RB3.cpp` reduced to a few hundred
lines of `BandRnd` wiring + a thin `DrawMesh` dispatcher; `grep strcmp Rnd_Wgpu_RB3.cpp` on asset
names → 0 (all moved behind the hook); zero inline WGSL strings remain.

**Flags deleted this phase:** none yet (asset-name flags are *relocated* behind the hook, not deleted
— deletion happens when their general fix lands in Phase 2/3/4). W0.3 lets us prove no draw-state regressed.

**Effort:** 10–15 agent-sessions (many small commits). **Quick win (week 1):** W1.1 externalize WGSL + naga validation.

---

## Phase 2 — Skinned transform + bind correctness (meshes deform properly, correct placement)

**Goal:** fix SYS-1 centrally so hands/fingers deform right and crowd/drum/UI place right, then
**delete the stack of name-scoped and clamp/guard hacks** that were masking it. Blocked on Phase 0
nets (W0.1, W0.3, W0.4, W0.5) and Phase 1 legibility.

**In scope — OVERHAUL of the skinned-placement + bind path (lanes 02 rec 3, 03 rec 1-2):**
- **W2.1 Fix the skinned-placement contract centrally** in `BandRnd::DrawMesh`: adopt Wii semantics — compose `obj.world = mesh->WorldXfm()` for skinned meshes too, with the bone palette using **bind-relative** bone matrices (not world). Placement then flows through the `RndTransformable` chain exactly like the Wii; hub bar, scrollbar, and prop placement work with **zero name-scoping**. *(lane 03 rec 1 Option A.)*
- **W2.2 Fix the bind at load** *(the single highest-value change, lane 02 rec 3)*: band-scoped un-share/rebind so outfit + appendage skin meshes resolve to the per-member animated `skeleton_unshared`, **AND rebake `invBind` against the per-member bind pose** (`offset' = meshBindWorld · inverse(perMemberBoneBindWorld)`) captured at skeleton load / first-pose. This removes the H2 basis mismatch and unlocks full-body (hands/fingers) rebind. The blocker is capturing the bind frame at the right seam.
- **W2.3 Kill shared-`GeomOwner` skeleton aliasing** for crowd and props: read the *drawn* mesh's own bones when it has them, so the latched `RebindCrowdCharBonesToOwnSkeleton` becomes unnecessary.
- **W2.4 Resolve BandPatchMesh's half-state** *(lane 02 rec 4)* — **decision required:** either **(a)** port it faithfully with an explicitly LP64-correct `MeshVert` layout (replace every `kMVFaceList`/`unkNN` raw offset with typed members), add a `WorkVerts` topology golden test (W0.1 sibling), then **compile it** so skin decals composite; or **(b)** formally accept the stub, document that skin decals are absent, and stop attempting faithful re-matches. **Whichever — the current half-state (compiled class, stubbed methods, partial-match `WorkVerts` that "work by accident") must end**, because it is the thing that invites the revert cycle.
- **W2.5 Close the band-waypoint data gap** (independent, game-side, SYS-5-adjacent): assert in `BandConfiguration::SyncPlayMode` that every `TargTransform.targName` resolves to a `BandCharacter`; log misses. Fixes "only some members placed."

**Dependencies:** W0.1 (per-vertex golden must exist — attempting W2.2/W2.4 without it is the third
scheduled revert), W0.5 (patch-bearing lineup for W2.4), W0.4 (effector golden for W2.1/W2.3), Phase 1
(legible `DrawMesh`).

**Exit gate:** W0.1 golden passes for hands/fingers under a live clip; crowd instances span the bowl
(position-spread assertion, not all-equal); drum-kit prop bones `!= origin` while drummer is placed;
the A/B opt-outs (`RB3_NO_CROWD_REBIND`, `RB3_NO_HUB_BAR_PLACEMENT_FIX`, `RB3_SCROLLBAR_THUMB_FIX_OFF`)
become **no-ops** (proof the central fix subsumed them); if W2.4a chosen, `WorkVerts` topology golden green.

**Flags/hacks DELETED this phase:** `RB3_NO_HUB_BAR_PLACEMENT_FIX`, `RB3_NO_HUB_HIGHLIGHT_FIX`,
`RB3_NO_HUB_BAR_SHARD_EXEMPT`, `RB3_SCROLLBAR_THUMB_FIX_OFF`, `RB3_NO_CROWD_REBIND`, and the four
stacked skinning hacks — `RebindOutfitBonesToOwnSkeleton` (torso-only), `RB3_NO_SKEL_REBAKE`,
`RB3_NO_SKEL_WORLDFIX`, `RB3_NO_SKIN_CLAMP`, the per-frame fling-clamp, and the V24 shard guard
(`SHARD_GUARD_OFF`) — all masking this one fault. Plus the ~30 relocated asset-name branches for
placement.

**Effort:** 8–12 agent-sessions (W2.2 bind-rebake and W2.4 BandPatchMesh are the hard parts).

---

## Phase 3 — Real lighting (SYS-4)

**Goal:** replace the inverted/synthetic lighting stack with the faithful real/approx split + box
ambient, and delete the ~10 default-ON lighting workaround flags. Blocked on W0.6 flag registry
(to know which approximation each fix replaces) and W0.3 per-draw golden (to prove no bind-group collapse).

**In scope (lane 04 §5, staged):**
- **W3.1 Stage 0 — cheap faithful fills (extension, REFACTOR):** wire fog from `RndEnviron` (`FogEnable/GetFogStart/End/FogColor`); bump directional+point light arrays **4→8** (GX cap); populate `projLight` from environ fakespots (plumbing already exists). No hack deleted; fills real gaps `:1566-1568`.
- **W3.2 Stage 1 — light-selection redesign (OVERHAUL of the light path):** replace the inverted heuristic with the faithful `IsValidRealLight` split (`Env.cpp:172`) — point/fakespot → HW slots, directional/fill → ambient. **Port `BoxMapLighting`** (`Env.cpp:308`) to a 6-axis ambient cube (6×vec3 in SceneUniforms), evaluated per environ (per-object if affordable), shader samples by world normal. Replaces scalar ambient + char-ambient average + grey-key fallback at once.
- **W3.3 Stage 2 — highway as real lighting (OVERHAUL of the highway look):** give `game.cam` a track environ so the highway is environ-lit; delete the `surface.mat ×0.12` / rails force-prelit / gem `×2` string-keyed surgery (`:6160-6205`) — the largest hack cluster in the lighting path.
- **W3.4 Stage 3 — faithful exposure/tonemap:** implement the GX pre-TEV `[0,1]` channel clamp and drive exposure from environ `mExposure`/`mWhitePoint`/`mUseToneMapping` (loaded `Env.cpp:122`, currently unused).

**Dependencies:** W0.6, W0.3. Independent of Phase 2 (different subsystem) — can run in parallel.

**Exit gate:** matched-frame A/B vs the Dolphin oracle (`c8-ground-truth-2026-07-01/t2-dolphin-oracle.md`),
per environ, grounded in `RB3_VENUE_PROBE` light dumps; raw-lit channel histograms match Dolphin
**without** the exposure knobs engaged.

**Flags/hacks DELETED this phase:** `RB3_TRACK_LIGHT_OFF`, `RB3_VENUE_LIGHT_OFF`,
`RB3_CHAR_REAL_LIGHT_OFF`, `RB3_VENUE_POINT_FALLOFF_LEGACY`, `RB3_CROWD_DIM_OFF`, `RB3_FRET_GLOW_OFF`,
`RB3_COMPOSE_MULT_OFF`, and the tuning knobs `sVenueAmbientClamp/Floor`, `sVenueGreyKey`,
`sVenuePointExposure/DirExposure`, `softClipLighting` knee.

**Effort:** 6–10 agent-sessions (BoxMapLighting port is the bulk). **Quick wins (week 1):** W3.1 fog +
4→8 lights are cheap extensions with data already present.

---

## Phase 4 — UI parity (SYS-5 + residual SYS-2)

**Goal:** eliminate the DATA-bucket UI bugs at the source and replace name-heuristic text detection
with material-driven detection. Largely independent of the engine phases; can start right after Phase 0.

**In scope (lane 05 §5):**
- **W4.1 [DECISION] Re-evaluate loading Wii `.milo` assets** instead of 360 `.milo_xbox`. This deletes the *entire* DATA bucket (squished text, stale-slot overlap, widget-class mismatch, score-not-updating) with **zero heuristics**. If asset availability blocks it, build **one load-time asset-normalization layer** (square-normalize font cell metrics, promote `UILabel`→`AppLabel` where code casts, honor author-hidden slots) instead of scattered `#ifdef HX_NATIVE` patches in faithful source.
- **W4.2 Consume `RndCam::mWorldProjectXfm`** (already computed by `UpdateLocal`, then discarded `:1308-1400`) instead of rebuilding the projection from `YFov`+window-aspect. Honors `mScreenRect`/`mUnknownFloat`/aspect/`mZRange` automatically → fixes non-16:9 and lets you **delete the `zMode=Disable` text heuristic** (`:6318`).
- **W4.3 Material-driven text detection:** key `useAlphaAsRGB`/`prelit` off the material's authored font-texture + `pre_lit`/`use_environ` (which `RndMat` carries) instead of `!mesh->Name()[0]` + name substrings. Removes the impostor-billboard collision (`:6054`), the `"icon"` exclusion, and the `isLikelyUiText` broadening — one data rule replaces six string tests.
- **W4.4 Fold** highlight-bar + scrollbar-thumb placement into the **Phase 2 W2.1** fix (already deleted there — this phase confirms UI parity).
- **W4.5 Delete the ≥0.6 UI text color floor** (`:5910-5916`) — no faithful basis, can only diverge from retail.

**Dependencies:** W0.5-style gate adapted to UI (per-slot text-region SSIM + bbox overlap vs
`images/retail-screenshots/`); W4.4 depends on Phase 2 W2.1.

**Exit gate:** UI-parity screenshot-diff gate (per-slot text-region SSIM + bbox-overlap, **not** global
RMSE) green against `images/retail-screenshots/` Wii song_select + 360 menu_hub, driven headless via
`/api/screenshot` + `song-select-capture.py`, run in the integration full-rebuild gate.

**Flags/hacks DELETED this phase:** `RB3_CROWD_DIM*` (once material-driven detection removes the
text/imposter collision), the `zMode=Disable` text heuristic, the `"icon"` name exclusion, the color
floor, and the scattered `#ifdef HX_NATIVE` UI fixups (folded into the asset layer).

**Effort:** 4–8 agent-sessions (W4.1 decision-dependent; the asset-normalization layer is the bulk if
Wii assets can't be used). **Quick win (week 1):** W4.5 delete the color floor.

---

## Phase 5 — Polish to "amazing" (SYS-2/SYS-3 residuals + web)

**Goal:** the last-mile fidelity and the structural cleanups that don't gate the correctness phases.

**In scope:**
- **W5.1 Real transparent/depth-sorted pass** — adopt `TransparentQueue` for the RB3 backend (currently DC3-only, lane 01 §2d); retire any residual submission-order fragility for arbitrary overlapping transparent geometry. **KEEP** the painter-order for co-planar 2D UI (lane 05 §4 — it's correct there); the sort is for 3D transparents.
- **W5.2 Web skin-RTT colorspace fix** (SYS-independent web track, lane 04 §4): FrameCapture-dump the composite RT native vs web, diff, audit RT `TextureFormat` sRGB-ness on both backends, and fix the RTT state/colorspace contract so the direct-bind bypass (`266ffb1b`) can be retired. Belongs in the `BandCompose` unit extracted in Phase 1.
- **W5.3 Burn down remaining workaround flags to zero** using the W0.6 registry ledger; confirm probe flags are compiled out of release; drive the tracking doc's default-ON-workaround count monotonically to 0.
- **W5.4 Close the DC3 masking hazard structurally** (lane 06 §1.4): add a CI link-time duplicate-symbol audit so no `native/src/**` file can redefine an engine symbol (the 2026-06-06 `AudioDevice.h` incident structure persists as latent risk).
- **W5.5 Converge RB3 ↔ DC3 backends** onto the shared `src/render/` core where data shapes allow (lane 01 §5a target structure) — opportunistic, not required for correctness.

**Exit gate:** default-ON workaround-flag count = 0 in the tracking doc; transparent geometry renders
correctly regardless of submission order (golden); web skin composite matches native without the
bypass; CI duplicate-symbol audit green.

**Effort:** open-ended / opportunistic.

---

## Risk register

| # | Failure mode (history-informed) | Mitigation |
|---|---|---|
| R1 | **Faithful mesh rewrite breaks rendering again (BandPatchMesh 3rd revert).** Both prior rewrites were Wii-correct and shipped green through a blind gate. | **Hard ordering:** W0.1 CPU reference skinner + per-vertex golden + W0.5 patch-bearing lineup MUST be green-and-able-to-fail-red before *any* Phase-2 mesh work. W2.4 grades against reference vertices + a `WorkVerts` topology golden, never eyeballing. |
| R2 | **Visual-gate blindness** — global drop/ratio/SSIM passes on exploded frames (34/34). | W0.5: layer **numeric per-mesh-bbox + per-draw-count assertions under** the image compare; patch-bearing + wide reviewer-judged frames. A shard explosion fails a *number*, not just a human's eye. |
| R3 | **Concurrent-agent collisions** — duplicate ports, `land.sh` clobbers, stale-lane wiring gaps (ghidriff saga: partially-landed lanes, casing-dup keys, reset-hard forbidden with uncommitted peer work). | Integration-worktree full-rebuild gate; **checkpoint-commit-first** (survive quota/rate-limit death 0-loss); one-behavior-per-commit; re-grep TU/flag wiring on **current main** before merge; never `git add -A`. |
| R4 | **Move-and-change in one commit** — the literal cause of the C8/BandPatchMesh regressions slipping the gate. | The one hard rule: a commit MOVES (visual-diff byte-identical) **xor** CHANGES (golden can fail red). Enforced per Phase-1 commit. |
| R5 | **Deleting a hack that was load-bearing for an unrelated screen.** The ~38 default-ON flags each guard *something*. | Before deleting any flag, prove the central fix subsumes it: the **A/B opt-out becomes a no-op**. UI-parity + placement + per-draw goldens catch cross-screen regressions. Deletions happen only in the phase whose general fix lands, never in Phase 1 (relocate, don't delete). |
| R6 | **`mSceneBindGroup → DrawContext` refactor subtly changes output** (bind-group collapse, the `a0f98ad` class). | W0.3 per-draw state-log golden diffs pipeline/blend/bind-group/world-xfm/counts; W1.6 is byte-identical-gated. |
| R7 | **W4.1 asset decision (Wii vs 360) is a product call that stalls Phase 4.** | Don't block: build the normalization layer as the fallback path in parallel; the layer is useful regardless of the decision. |
| R8 | **Scope creep / never-ending "polish."** | Every phase has a *measurable* exit gate tied to the Dolphin oracle or `images/retail-screenshots/`. "Amazing" = those gates green + workaround-flag count 0, not an open-ended aesthetic. |

---

## Sequencing at a glance

```
Phase 0 (nets) ─────────────┬─▶ Phase 2 (skinning/placement)  [needs W0.1,W0.3,W0.4,W0.5]
                            ├─▶ Phase 3 (lighting)            [needs W0.6,W0.3]  ∥ Phase 2
Phase 1 (decompose) ────────┘   (behavior-preserving; ∥ Phase 0)
                                 │
Phase 4 (UI) ────────────────────┴─ starts after Phase 0; W4.4 needs Phase 2 W2.1
Phase 5 (polish) ── after 2/3/4 land, opportunistic
```

**Total rough effort:** ~34–55 agent-sessions across Phases 0–4, plus open-ended Phase 5.

**Week-1 quick wins (independent, low-risk, high-signal):** W0.2 loud stubs · W0.6 flag-registry
skeleton + generated doc · W1.1 externalize WGSL + naga validation · W3.1 fog + 4→8 lights ·
W4.5 delete the ≥0.6 color floor · W2.5 band-waypoint assert.
