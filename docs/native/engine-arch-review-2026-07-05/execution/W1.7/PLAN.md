# W1.7 — Wire `GameRenderHook` for RB3 + relocate asset-name branches to game side

**Planner:** Opus, 2026-07-06. **Item:** REFACTOR_PLAN W1.7; lane doc `01-renderer-core.md`
§1c + §5 (steps 5a target module map, 5b step 6); ARCHITECTURE_REVIEW SYS-2.
**Lane:** A (sequential on `Rnd_Wgpu_RB3.cpp`: W1.2→W1.3→W1.4→W1.5→**W1.7**→W1.6). Assume
W1.2–W1.5 commits are in (they are — engine `daa0286`…`648dc40`).

---

## Objective

Two parts, both **behavior-preserving relocation** (Phase 1 rule: a commit either MOVES code
or CHANGES behavior, never both; every commit byte-identical output):

1. **Wire the frame-pass seam.** The RB3 renderer `Rnd_Wgpu_RB3.cpp` never calls the existing
   `GameRenderHook` (grep confirms zero `GetGameRenderHook` calls in it, vs DC3's `Rnd_Wgpu.cpp`
   which dispatches at `:552` `DrawGameOverlay` and `:990` `RenderCharacterImpostors`). The
   RB3 hook impl `rb3/native/src/rb3_render_hook.cpp` is a registered no-op. Wire the two
   existing hook methods into the RB3 draw path at the analogous seams so the seam is *live*
   (both stay no-ops today — behavior identical — but the dispatch exists for future RB3
   overlay/impostor passes). This is a small correctness-neutral wiring step.

2. **Relocate the asset-name branches (the core of the item).** The engine renderer branches
   on ~30+ hardcoded RB3 asset/material/dir NAME strings. Move each *behavior* branch out of
   the engine and behind a **new per-draw game hook** implemented in
   `rb3/native/src/rb3_render_hook.cpp`, **one behavior per commit**, each still behind its
   existing `RB3_*` env flag, **byte-identical rendered output per commit**. End state: the
   engine renderer no longer knows RB3 content names for *behavior* decisions.

**Faithful-reference citations.** There is **no existing per-mesh hook precedent** — DC3's
`GameRenderHook` (`milo-native-engine/src/platform/GameRenderHook.{h,cpp}`) is only the two
frame-pass methods (`DrawGameOverlay`, `RenderCharacterImpostors`), and DC3's
`dc3_render_hook.cpp` routes NO asset-name branches through it. So Part 2 requires *designing*
a new hook interface (S1). The design must mirror the existing hook's conventions: engine owns
an abstract interface with no Milo/RB3 headers, RB3 supplies the concrete impl, engine
null-checks and falls back to prior behavior. See `GameRenderHook.h` header comment for the
contract to imitate.

---

## Current-state census (MEASURED 2026-07-06 — re-grep before you touch anything; counts drift as siblings land)

`grep -cE 'strcmp|strstr|strncmp'`:
- `src/platform/Rnd_Wgpu_RB3.cpp` — **49** sites
- `src/platform/RB3MaterialBinder.cpp` — **21** sites (extracted by W1.3 `b206d44`; IN SCOPE)
- `src/platform/RB3HaloPass.cpp` — **2** sites (extracted by W1.4 `e61e6ec`; IN SCOPE)
- `RB3MeshCache.cpp` / `RB3PostProc.cpp` / `RB3Quad.cpp` — 0 (clean)

**CRITICAL classification — not all 49+ sites are in-scope "behavior" branches.** Sort every
site into one of these buckets before relocating (this is S1's first job):

**Bucket A — DEBUG PROBES (relocate LAST or leave; they print to stderr, do NOT change
rendered output).** Gated on debug env flags; a name match only decides *whether to log*.
Examples in `Rnd_Wgpu_RB3.cpp`: `CAM_DBG` (L2072), `HUB_BAR_PROBE` (L2187, L3096),
`RB3_HEADMAT_DBG` (L2314, L2636, L3966), `GEM_VTX` (L2510), `BONE_PROBE`/`BONE_PROBE_NAME`
(L2817), `XBONE_TRACK` (L3251), `C8_EVERY` (L3413), `CHAIN_PROBE` (L3495), `IK_SHARD_VERT`
(L3783), `RB3_VENUE_PROBE` (L1310 region). These do not couple *behavior* to content — they
couple *diagnostics*. The exit-criterion grep-zero counts them, so they must eventually move
too, but they carry **zero behavior-change risk** and can be relocated in a single mechanical
sweep (S4) or, if the coordinator agrees, tracked as debug-only and excluded from the
grep-zero target (decide in S1; default: relocate them, they're cheap).

**Bucket B — TRUE BEHAVIOR BRANCHES (the item's real payload; one-per-commit).** Each changes
rendered output and is behind a real `RB3_*` workaround flag:

| # | Where (current) | Names | Flag | Decision produced |
|---|---|---|---|---|
| B1 | Rnd_Wgpu_RB3 ~2710–2725 | `highlight_main`/`highlight_pattern` | `RB3_NO_HUB_BAR_PLACEMENT_FIX` | hub-bar world-xfm override (identity + label translation) |
| B2 | Rnd_Wgpu_RB3 ~2744–2790 | `scrollbar_bg.mesh`/`scrollbar.mesh` | `RB3_SCROLLBAR_THUMB_FIX_OFF` | scrollbar-thumb world-xfm = previous bg draw's world |
| B3 | Rnd_Wgpu_RB3 ~2875–2960 | `facehair`/`goatee`/`hair`/…/`skeleton_unshared.milo` + bone-name list | `RB3_NO_SKEL_REBAKE` | dynamic-mesh skel rebake selection (skinning workaround) |
| B4 | Rnd_Wgpu_RB3 ~3731 | `highlight_main`/`highlight_pattern` | `RB3_NO_HUB_BAR_SHARD_EXEMPT` | shard-guard exemption for hub bar |
| B5 | Rnd_Wgpu_RB3 ~3871 | `skeleton_unshared.milo` | (shard-guard region) | band-member discriminator for shard guard |
| B6 | RB3HaloPass ~71–72 | `surface`/`gem_smasher_glow` | `RB3_SMASHER_HALO` | `IsHaloSourceMat` exclusions |
| B7 | RB3MaterialBinder ~90–102 | `num*`/`_source.mesh`/`_comma.mesh`/`.lbl`/`font`/`label` | (text heuristic) | text-mesh material handling (useAlphaAsRGB/prelit) |
| B8 | RB3MaterialBinder ~134–135 | `highlight_main`/`highlight_pattern` | (hub color) | hub highlight bar material color |
| B9 | RB3MaterialBinder ~175 | `skin_diffuse_output` | (skin RTT) | skin-RTT diffuse handling |
| B10 | RB3MaterialBinder ~241 | `icon` | (color-icon) | color-icon-font useAlphaAsRGB exclusion |
| B11 | RB3MaterialBinder ~279–292 | `tail_` + chain names | (tail chain) | tail chain-select material |
| B12 | RB3MaterialBinder ~360–381 | `world.cam`/`crowd`/`extra`/`skeleton_unshared.milo`/`char/crowd/`/`char/extras/` | (crowd/extras) | crowd/extras vs band-member material path |
| B13 | RB3MaterialBinder ~440–528 | `game.cam`/`surface.mat`/`rails.mat`/`gem_smasher_glow.mat`/`peakstate`/`prism_gem` | `RB3_TRACK_LIGHT_*` family | highway per-material shading (surface/rails/smasher/gem) |

**Bucket C — CAMERA/ENVIRON NAME branches that are really LIGHTING (Phase 3), NOT asset
content.** `world.cam`/`game.cam`/`"char"` environ matches at Rnd_Wgpu_RB3 L1287, L1335,
L2146 (and the `world.cam`/`game.cam` matches embedded in binder B12/B13). These gate the
venue/char lighting stack (`RB3_VENUE_LIGHT*`, `RB3_CHAR_REAL_LIGHT*`, `RB3_TRACK_LIGHT*`).
**Scope call (S1 must decide and document):** camera-name matches (`world.cam`/`game.cam`)
are *scene-scope* selectors, not *asset-content* names — the lighting rewrite (Phase 3 W3.x)
subsumes them. Relocating a `game.cam` check behind a per-mesh hook is awkward and risks
coupling the hook to camera state. **Recommended:** relocate the pure *mesh/material-name*
behavior branches (Bucket B) this wave; leave the *camera/environ-name* lighting selectors
(Bucket C) for Phase 3, and record that decision so the grep-zero exit criterion is scoped to
"RB3 **asset** names (mesh/material/dir), excluding camera/environ scene-scope names."
Confirm this scoping with the coordinator via STATUS.md before finalizing exit criteria.

The B-list above is ~13 behavior clusters. Several share a flag/family (B1+B8 hub-bar,
B4 hub-shard, B13 highway) — **still one commit per behavior**, but a commit may move a
tight cluster that is a single behavior (e.g. all of B13's highway-material shading is one
"highway look" behavior). S1 produces the final ordered commit list.

---

## Design: the per-draw game hook (S1 deliverable)

The existing `GameRenderHook` two-method shape does not fit per-mesh decisions. Add a small,
data-in/decision-out extension the engine consults inside `DrawMesh` and `RB3BuildMaterialUniforms`.
**Design constraints (mirror `GameRenderHook.h`):** engine owns an abstract interface with NO
Milo/RB3 headers beyond forward decls; passes only primitive/opaque args (name strings already
available, plus the few pointers the branch reads: `RndMesh*`, `RndMat*`, `bool skinned`,
`RndMesh* owner` — but as `void*`/forward-declared to keep the engine header game-agnostic, OR
accept that these are engine types already visible to the engine renderer and pass them
typed — S1 decides; the litmus test is "does the ENGINE header name an RB3-only concept?" It
must not). The hook returns a small POD "decision" the engine applies; the *policy* (which
name → which decision) lives entirely in `rb3_render_hook.cpp`.

Two viable shapes — S1 picks and justifies:

- **(a) One method per behavior family** (e.g. `bool HubBarPlacement(RndMesh*, float outWorld[16])`,
  `bool ScrollbarThumb(...)`, `HaloDecision Halo(RndMat*)`, `MaterialNameClass Classify(RndMesh*,RndMat*)`).
  Explicit, easy to A/B, but grows the interface.
- **(b) One general `QueryDrawPolicy(const DrawQuery&, DrawPolicy&)`** where `DrawQuery` carries
  the name(s) + flags read and `DrawPolicy` is a bitset/struct of possible overrides
  (worldXfmOverride+matrix, materialClass enum, haloExclude, shardExempt, …). Fewer methods,
  one call site per decision point, but a wider POD.

**Recommendation:** hybrid — (b)-style `DrawPolicy` for the material-classification cluster
(B7–B13, which all feed `RB3BuildMaterialUniforms`) since they're naturally "classify this
mesh/material", and (a)-style discrete methods for the geometric placement/guard overrides
(B1–B5 world-xfm + shard) since each returns different data. The env-flag reads move INTO the
hook impl (the engine stops reading `RB3_NO_HUB_BAR_PLACEMENT_FIX` etc.; the hook impl reads
them and returns "no override" when the flag is off). Register any NET-NEW env flag in the
`NativeCompatFlags` registry (none expected — flags are relocated, not created — but if a
relocation needs a new probe, register it and regenerate the ledger).

**Where the hook object lives:** the engine's existing `GetGameRenderHook()` returns the
registered `GameRenderHook*`. Extend that base class (or add a sibling `GameDrawPolicyHook`
registered the same way) so the engine renderer can reach the RB3 impl. S1 decides whether to
extend `GameRenderHook` (single hook object, simplest) or add a second registration slot.
**Prefer extending `GameRenderHook`** — one registration path, `BandRenderHook` already
implements it, DC3's `HamRenderHook` gets default no-op overrides (engine base provides
`virtual … { return false; }` defaults so DC3 need not implement them). Verify the DC3 build
(dc3-decomp is a separate repo with concurrent owners — do NOT break it; if extending the base
class forces a DC3 recompile, provide default implementations so `dc3_render_hook.cpp` needs
no edit).

---

## Subtasks

### W1.7.S1 — Design the per-draw hook interface + classify all sites + wire the frame-pass seam
**model:** opus (design + correctness-critical: the interface shape determines every later commit)

**Goal.** Produce (1) the final hook interface, (2) the frame-pass seam wiring (Part 1), and
(3) the ordered relocation commit list (the B-bucket, one behavior per commit) with the
Bucket-C scope decision recorded.

**Files.**
- `milo-native-engine/src/platform/GameRenderHook.h` (+ `.cpp` if adding storage) — extend
  with the per-draw policy methods, all with base-class no-op defaults.
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — wire the two EXISTING frame-pass hook
  calls (`DrawGameOverlay`, `RenderCharacterImpostors`) at the seams analogous to DC3
  `Rnd_Wgpu.cpp:552` / `:990`. Find the RB3 post-processing-resolve / pre-scene-pass seams by
  reading the current frame flow (do NOT trust line numbers; re-grep for the overlay-resolve
  and encoder-open points).
- `rb3/native/src/rb3_render_hook.cpp` — implement the new methods as no-ops initially (they
  return "no override" so behavior is unchanged until S2–S4 move policy in).
- This `PLAN.md` is NOT edited by implementers; S1 records the final commit list + Bucket-C
  decision in `STATUS.md`.

**Approach.**
1. Re-grep all `strcmp|strstr|strncmp` in `Rnd_Wgpu_RB3.cpp`, `RB3MaterialBinder.cpp`,
   `RB3HaloPass.cpp`. Bucket each site A/B/C per the census above (verify against CURRENT code).
2. Write STATUS.md note: final Bucket-C scope decision (recommend: relocate B-bucket mesh/
   material/dir names; defer camera/environ scene-scope names to Phase 3) — flag for coordinator.
3. Design the hook interface (hybrid per recommendation). Add methods with base no-op defaults
   to `GameRenderHook.h`. **This is a MOVE/wiring commit that adds an unused-but-registered
   seam — no behavior change** (defaults return "no override"; DrawMesh still runs its inline
   branches). Commit: `W1.7: add per-draw GameRenderHook policy methods (no-op defaults, unused)`.
4. Separately, wire Part 1: call `GetGameRenderHook()->DrawGameOverlay(this)` and
   `RenderCharacterImpostors(this)` at the RB3 frame seams. Since `BandRenderHook` is a no-op,
   output is byte-identical. Commit: `W1.7: wire frame-pass GameRenderHook dispatch in RB3 renderer (MOVE, no-op hook)`.
5. Implement the new per-draw methods in `rb3_render_hook.cpp` as no-ops returning "no
   override". Commit with the interface (or right after).

**Verification (each commit).**
- Build own dir: `cmake --build native/build-agent-W17 --target rb3-native -j8` (and
  `rb3-tests`). Configure once: `cmake -B /home/free/code/milohax/rb3/native/build-agent-W17
  -S /home/free/code/milohax/rb3/native`.
- **Byte-identical evidence** (REQUIRED for every commit — see the shared procedure below).
- Confirm DC3 still builds if `GameRenderHook.h` changed: the base-class defaults mean
  `dc3_render_hook.cpp` needs no edit; verify by grep that DC3's hook compiles against the new
  header (do NOT modify dc3-decomp; if it would break, add defaults until it doesn't).

### W1.7.S2 — Relocate the geometric placement + guard behaviors (B1–B5)
**model:** opus (world-xfm overrides feed skinned placement; getting the matrix hand-off exactly
byte-identical is correctness-critical)

**Goal.** Move B1 (hub-bar placement), B2 (scrollbar thumb), B3 (skel-rebake dynamic-mesh
selection), B4 (hub-bar shard exempt), B5 (band-member shard discriminator) out of
`Rnd_Wgpu_RB3.cpp` and behind the S1 hook, **one behavior per commit**, each still behind its
existing flag (the flag read moves into the hook impl).

**Files.** `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (remove inline branch, call hook),
`rb3/native/src/rb3_render_hook.cpp` (implement the policy + read the env flag).

**Approach (per behavior, repeat).**
1. Read the inline branch and every value it feeds (e.g. B1 feeds `hubBarPlacement` → the
   `obj.world` identity+translation block ~2762). Determine the exact POD the hook must return
   (e.g. `bool wantHubBarWorld; float world[16]` — or a signal that the engine keeps computing
   the same matrix). To stay byte-identical, prefer: hook returns the DECISION (bool + any
   name-derived params), engine keeps the matrix math it already has, so float ordering is
   untouched. Only move the matrix build into the hook if it can be proven bit-identical.
2. Replace the inline `strcmp/strstr` + flag read with a single hook call; the hook impl
   contains the moved `strcmp` + `getenv`.
3. `RB3_*` flag semantics must be preserved exactly (default-on/off, `!= '0'` parsing, the
   static-cache-once idiom).
4. Commit one behavior: `W1.7: relocate <behavior> to rb3_render_hook behind <FLAG> (MOVE, byte-identical)`.
5. Byte-identical evidence per commit (shared procedure). B1/B2 are UI placement — capture at a
   menu/song-select scene; B3/B4/B5 are skinning/shard — capture at a band-lineup scene and run
   the lineup gate (these touch the exact skinning/shard path W0.5 guards).

**Verification.** Build rb3-native + rb3-tests; byte-identical evidence (screenshot hash +
lineup gate for B3–B5; SkinGolden/ClipPose gtests must stay green — B3 touches skel-rebake).

### W1.7.S3 — Relocate the material-classification behaviors (B6–B13)
**model:** opus (material shading feeds the highway/char look; B12/B13 embed camera-name checks
that must be handled per the Bucket-C decision)

**Goal.** Move the `RB3HaloPass` halo exclusions (B6) and the `RB3MaterialBinder`
classification branches (B7–B13) behind the S1 hook (the material-classification `DrawPolicy`
half), one behavior per commit.

**Files.** `milo-native-engine/src/platform/RB3HaloPass.cpp`,
`milo-native-engine/src/platform/RB3MaterialBinder.cpp`, `rb3/native/src/rb3_render_hook.cpp`.

**Approach.**
1. B6 (halo): `IsHaloSourceMat` name exclusions → hook returns `haloExclude`/`haloForce`. The
   `RB3_SMASHER_HALO` flag read moves into the hook. Engine keeps the emissive-map/multiplier
   test (that's data, not a name) and only asks the hook for the name-based exclusion.
2. B7–B13 (binder): `RB3BuildMaterialUniforms` currently name-classifies inline. Relocate each
   classification to the hook returning a `MaterialClass`/policy the binder applies. **Keep the
   binder's uniform math in the engine** — the hook returns *which class*, the engine applies
   the same math it already has, so uniforms stay bit-identical.
3. **Bucket-C handling inside B12/B13:** these branches mix mesh-name (`crowd`/`extra`/
   `surface.mat`/`rails.mat`) with camera-name (`world.cam`/`game.cam`). Per S1's decision,
   relocate the *mesh/material-name* half to the hook; the *camera-name* guard stays inline as
   a scene-scope condition (it is not an asset name) OR the hook also takes the cam name — S1
   specifies. Do NOT let the hook reach into `RndCam::sCurrent` global state; if the cam gate
   is needed, the engine passes the cam name string into the query.
4. One commit per behavior (B6, B7, B8, … B13 — B13's highway shading may be one "highway
   material look" commit if it's a single behavior with sub-cases; S1 decides granularity).

**Verification.** Build; byte-identical evidence. B13 (highway) capture at gameplay scene; B7/
B10 (text/icon) capture at menu with text; B9/B12 (skin RTT / crowd) at a band scene. The
drawlog diff (RB3_DRAWLOG) is useful here where stable — material uniform changes show up as
bind-group/pipeline deltas in the log.

### W1.7.S4 — Relocate debug-probe name matches (Bucket A) + verify grep-zero exit
**model:** sonnet (mechanical: move stderr-only debug name matches; no behavior change by
construction since they only gate logging)

**Goal.** Move the Bucket-A debug-probe name matches (CAM_DBG, HEADMAT_DBG, GEM_VTX,
BONE_PROBE, XBONE_TRACK, C8_EVERY, CHAIN_PROBE, IK_SHARD_VERT, HUB_BAR_PROBE, VENUE_PROBE)
out of the engine renderer so the `grep -E 'strcmp|strstr' → 0 RB3 asset names` exit criterion
is met, then run the final verification sweep.

**Files.** `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, `RB3MaterialBinder.cpp`
(any remaining probe sites), `rb3/native/src/rb3_render_hook.cpp` (a debug-probe query the
hook answers, or a `GameDebugProbe` helper). These are stderr-only — zero rendered-output risk.

**Approach.**
1. For each debug probe, the name-match decides "should I log for this mesh". Move the
   name-match into a hook debug helper (`bool DebugProbeMatch(const char* probeSel, RndMesh*)`)
   or, if the coordinator agreed in S1 to exclude pure-debug from grep-zero, leave them and
   document. **Default: relocate** — they're cheap and finish the grep-zero cleanly.
2. Keep each probe's env-flag gate and throttle behavior identical.
3. Final sweep: `grep -E 'strcmp|strstr|strncmp' Rnd_Wgpu_RB3.cpp RB3MaterialBinder.cpp
   RB3HaloPass.cpp` → the only survivors are Bucket-C camera/environ scene-scope names (if
   S1 deferred them) and non-asset-name uses (if any). Zero RB3 **asset** (mesh/material/dir)
   names remain.

**Verification.** Build; byte-identical evidence for the whole S4 batch (one capture set is
enough since these are logging-only — but still run the deterministic screenshot hash compare
to prove no accidental behavior slip). Produce the final grep census in STATUS.md.

---

## Byte-identical evidence procedure (REQUIRED for every MOVE commit)

This is a Phase-1 item: every commit must be provably behavior-preserving. Procedure per commit:

1. **Deterministic scene + fixed clock.** Boot headless with the frozen-sim-clock seam so draw
   output is reproducible: `RB3_HTTP=1 RB3_FIXED_CLOCK=1` (registered by W0.3b `834954b`;
   confirm the exact var name in the NativeCompat ledger before use). Drive to the scene that
   exercises the moved behavior via `/api/input` (menu/song-select for UI branches; a band
   lineup for skin/shard; a gameplay frame for highway/halo). Reuse the harness patterns in
   `scripts/native/song-select-capture.py`, `patch-lineup-capture.py`.
2. **Screenshot hash compare BEFORE vs AFTER.** Capture `/api/screenshot` → PNG at the fixed
   scene on the pre-commit build and the post-commit build; assert identical hash
   (`sha256`). Identical hash = byte-identical output. Record both hashes in STATUS.md.
3. **Lineup gate** for any branch touching skinning/shard/placement (B1–B5, B9, B12): run
   `scripts/native/lineup-gate.py` against the committed golden in
   `scripts/native/goldens/w0.5-lineup/`; it must PASS (img + segA + ratioB + countC + pin
   layers). This is the non-blind gate that catches shard explosions the image hash can miss.
4. **Draw-log diff where stable** (RB3_DRAWLOG / `/api/drawlog` +
   `scripts/native/drawlog-golden.py`): dump the per-draw log before/after; diff must be empty.
   NOTE: integration draw-log golden stability is still pending W0.3b's frozen-clock seam — if
   the log is not yet stable at your scene, rely on the screenshot hash + lineup gate and note
   the drawlog was non-deterministic (do not block on it).
5. **gtests:** `SkinGolden.*`, `ClipPoseFixture.*`, `StubCensus.*` green (esp. B3 skel-rebake).
6. Record all evidence (scene, hashes, gate result) in STATUS.md under the subtask section.

If any hash differs, the commit CHANGED behavior — STOP, do not commit as a MOVE; split out the
behavior change (there should be none — this is pure relocation).

---

## Exit criteria (measurable)

1. **Frame-pass seam live:** `Rnd_Wgpu_RB3.cpp` calls `GetGameRenderHook()->DrawGameOverlay`
   and `RenderCharacterImpostors` at the RB3 frame seams (grep shows the two calls); rendered
   output byte-identical (hook is no-op today).
2. **Asset-name grep-zero:** `grep -E 'strcmp|strstr|strncmp'` on `Rnd_Wgpu_RB3.cpp` +
   `RB3MaterialBinder.cpp` + `RB3HaloPass.cpp` (the TUs this lane owns/extracted) shows **zero
   RB3 asset (mesh/material/dir) name strings**. Camera/environ scene-scope names
   (`world.cam`/`game.cam`) may remain IF S1+coordinator deferred them to Phase 3 — documented
   in STATUS.md with the explicit deferred list.
3. **Every relocated behavior byte-identical:** each B-bucket commit has a recorded screenshot-
   hash match (+ lineup gate PASS for skinning/shard commits) in STATUS.md. No commit both
   moves and changes.
4. **Flags preserved:** every existing `RB3_*` flag that gated a relocated branch still exists
   and still A/B-toggles the same behavior — now read inside `rb3_render_hook.cpp`, not the
   engine. Zero flags deleted (Phase 1 relocates, does not delete). Any net-new flag registered
   in `NativeCompatFlags` + ledger regenerated (`scripts/analysis/native_compat_census.py`).
5. **DC3 not broken:** `GameRenderHook.h` extension has base-class no-op defaults;
   `dc3_render_hook.cpp` needs no edit and DC3 still compiles (verified, no dc3-decomp source
   changes made by this item).
6. **gtests green:** `SkinGolden.*`, `ClipPoseFixture.*`, `StubCensus.*`, plus `rb3-tests`
   build, all pass on the final build.

---

## Risks / conflicts

- **Lane A sequential on `Rnd_Wgpu_RB3.cpp`:** W1.2→W1.5 are IN (engine `daa0286`…`648dc40`);
  **W1.6 runs AFTER W1.7 on the same file.** W1.6 replaces the mutable `mSceneBindGroup` with a
  `DrawContext`. Keep W1.7's hook calls at the DrawMesh decision points cleanly separable so
  W1.6's DrawContext refactor can absorb them. Commit small and often (hard rule 2) so W1.6
  rebases minimally. **Do not touch the scene-bind-group member** (W1.6's territory) — hard rule 8.
- **Bucket-C ambiguity is the top design risk.** If S1 relocates camera-name lighting selectors
  into a per-mesh hook, it couples the hook to camera/environ global state and will collide with
  Phase 3's lighting rewrite. Mitigation: the recommended scoping (defer Bucket-C to Phase 3)
  — get coordinator sign-off in STATUS.md before S3 touches B12/B13's cam gates.
- **Byte-identical fragility on the matrix/uniform hand-off (B1–B2 world xfm, B13 uniforms).**
  Keep the float math in the engine; the hook returns only the DECISION + name-derived scalars.
  Never move a matrix build across the seam unless proven bit-identical. This is why S2/S3 are
  opus.
- **Debug-probe scope (Bucket A) vs grep-zero.** If moving ~10 debug probes bloats the hook,
  coordinate whether pure-stderr probes count toward grep-zero. Default: relocate them (S4,
  sonnet) — behavior-free.
- **Concurrent trees / hard rules:** OWN build dir `native/build-agent-W17` only; never touch
  `native/build-native` / `build-web*`. `flock /tmp/milo-engine-git.lock` (engine) /
  `/tmp/rb3-git.lock` (rb3) around add+commit; stage only your files. **NEVER** `git reset/
  rebase/checkout--/restore` on shared trees (rule 7). Never bump `MILO_ENGINE_PIN` (rule 3).
  STATUS.md appends under `flock /tmp/rb3-docs.lock`. Do NOT edit dc3-decomp.
- **Parallel lanes elsewhere:** W0.3b (frozen-clock seam — provides the deterministic-capture
  var this plan depends on; `834954b` already landed the registration) and W2-TESTFIX run in
  parallel; they don't touch these files.
- **Engine working tree has an unrelated agent's uncommitted `FxSendNative.cpp` edit** (Wave-1
  note) — leave it untouched; stage only your files.

---

## Suggested commit sequence (S1 finalizes)

1. `W1.7: add per-draw GameRenderHook policy methods (no-op defaults, unused)` [engine]
2. `W1.7: implement no-op per-draw policy in rb3_render_hook` [rb3]
3. `W1.7: wire frame-pass GameRenderHook dispatch in RB3 renderer (MOVE, no-op)` [engine]
4. B1…B5 — one MOVE commit each (S2)
5. B6…B13 — one MOVE commit each (S3)
6. Bucket-A debug probes — one batched MOVE (S4)
7. Final grep-zero census + evidence roundup in STATUS.md
