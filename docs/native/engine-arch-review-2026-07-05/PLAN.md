# Milo Native Engine — Architectural Review (2026-07-05)

> **STATUS: COMPLETE.** All 6 lanes + synthesis landed. Verdict: **`targeted-refactor`**
> (see `ARCHITECTURE_REVIEW.md`); implementation plan: `REFACTOR_PLAN.md` (Phase 0 safety
> nets are a hard prerequisite for all structural work).

**Coordinator:** Claude (Fable), with a fleet of Opus review agents.
**Scope:** `/home/free/code/milohax/milo-native-engine` (our custom native/web engine layer) plus its
boundary with `rb3/native/src` (game-side glue). Read-only review; output is docs + a refactor plan.

## Why

The native port has accumulated a family of rendering bugs that suggest structural, not point, problems:

- **Mesh deformation wrong** — hands/fingers deform incorrectly; count-in thin-geo shards
  (pose-independent skinning residual); BandPatchMesh faithful rewrites broke rendering TWICE.
- **Placement wrong** — crowd characters all standing at the same point; drum kit at the same
  point; only some band characters in correct locations.
- **Lighting fragile** — track lighting is a synthetic game.cam-scoped approximation; venue
  lighting is a per-environ SceneUniforms rewrite; C8 face lighting took 4 root-caused fixes;
  web skin-RTT composite bakes grey (engine shader root-cause deferred, bypassed behind flag).
- **UI near-parity but buggy** — song library layout wrong, stale-slot text overlap, scrollbar
  and highlight-bar point fixes keep landing (`UiRenderHeuristics.h` exists at all).

Pre-review smells (measured):

- `src/platform/Rnd_Wgpu_RB3.cpp` is **7,017 lines** with **113 `getenv` calls** (~30 distinct
  `RB3_*` runtime flags). Next-largest render file is 1,861 lines (`Rnd_Wgpu.cpp`).
- No `.wgsl` files — all shaders are inline C++ strings.
- Render responsibilities leak across the repo boundary into `rb3/native/src`
  (`rb3_render_mesh.cpp`, `rb3_band_rnd.cpp`, `rb3_render_hook.cpp`, ...).
- 40 TODO/HACK/FIXME markers; weak-stub link pattern has produced silent no-op subsystems before
  (particles, NetSession).

## Question to answer

Is this codebase structurally sound enough to fix these bugs incrementally, or does it need an
overhaul (and if so, of which chunks, in what order)? Target end-state: lighting fixed, meshes
deforming properly, UI at parity, graphics at "amazing."

## Review lanes (one Opus agent each, parallel)

| Lane | Doc | Focus |
|---|---|---|
| 1 renderer-core | `01-renderer-core.md` | Anatomy of Rnd_Wgpu_RB3.cpp + Rnd_Wgpu.cpp + gfx/: responsibilities, state flow, inline shaders, flag census, proposed decomposition |
| 2 mesh-skinning | `02-mesh-skinning.md` | Skinning/deform pipeline vs decomp ground truth; hands/fingers root cause; BandPatchMesh; why faithful rewrites keep breaking |
| 3 transforms-placement | `03-transforms-placement.md` | WorldXfm propagation, RndMultiMesh/instancing, crowd/drum-kit/band placement bugs |
| 4 lighting-materials | `04-lighting-materials.md` | Lighting architecture vs faithful RndEnviron/RndMat; material blend modes; path to real lighting |
| 5 ui-2d | `05-ui-2d.md` | UI/text/2D parity; UiRenderHeuristics; song library layout bugs |
| 6 arch-crosscut | `06-arch-crosscut.md` | Engine↔game boundary, weak-stub linkage, env-flag sprawl, testing/gates, layering verdict |

Then a synthesis phase produces `ARCHITECTURE_REVIEW.md` (verdict) and `REFACTOR_PLAN.md`
(phased plan), finalized by the coordinator.

## Agent protocol

- **Read-only** — no code changes, no builds that mutate state (read `build/` outputs freely).
- Write your lane doc to this directory BEFORE returning; also write a structured JSON checkpoint
  to `checkpoints/<lane>.json`. If your checkpoint already exists and is valid, return it as-is.
- Ground claims in file:line references. Distinguish MEASURED facts from hypotheses.
- Faithful references: `rb3/src/system/` (decomp), `/home/free/code/milohax/dc3-decomp/src/system/`
  (~87% complete shared engine), Bank8 Ghidra via `bin/analyze-function`.

## Known-context docs (prior investigations)

- `docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md` — outfit meshes bound the SHARED static
  skeleton, fixed by RebindOutfitBonesToOwnSkeleton **workaround** in BandCharacter::Poll.
- `docs/native/NATIVE_HACK_AUDIT_2026-06-08.md` — 73-hack census.
- `docs/native/render-polish-2026-06-11/PLAN.md` — 8-gap render campaign.
- `docs/native/c8-ground-truth-2026-07-01/`, `render-regress-2026-07-02/` — C8 face saga.
- Memory: BandPatchMesh faithful rewrites broke native twice; visual gate blind to patch-shard
  corruption.
