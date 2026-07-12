# DC3 Render Bridge — Charter (2026-07-12)

## Ask (verbatim intent)

Draft a plan + spec for a **layer between the Wii RB3 game engine and the Xbox
DC3 native renderer** — i.e., let RB3's (Wii-lineage) scene graph drive the
mature DC3 native rendering stack (`dc3-decomp/native/src/{gfx,platform}`,
WebGPU/Dawn) instead of / alongside RB3's current render path. Goal: evaluate
whether that renderer would work **better**, even if scoped to **scene graphics
only**. Deliverable: docs + spec on disk, with an implementation task DAG.
This wave is **planning only** — no implementation.

## Why now (motivation)

The engine-arch-review campaign (docs/native/engine-arch-review-2026-07-05/,
31 waves) has repeatedly fought scene-render defect families on the RB3 native
path (point-radial shard meshes, emissive glow, depth occlusion, patch-mesh
composites). DC3's native port is the mature model: full boot-to-gameplay,
21/21 venue×song stability, dedicated gfx passes (Bloom/PostProc/Shadow),
WebGPU platform classes (Mesh_Wgpu, Tex_Wgpu, Rnd_Wgpu, Part_Wgpu,
TransparentQueue, RenderState). The open question is whether RB3's residual
render-quality gap is (a) shared-engine code both ports use anyway, or
(b) render-path code where DC3's is genuinely ahead — only (b) justifies a
bridge.

## Structure

Ultracode workflow, Fable-driven DAG:

1. **Recon** — 4 parallel Opus lanes (read-only, NO builds — Wave 31 owns the
   build dir): R1 RB3-native render path today; R2 DC3 native renderer
   inventory; R3 rndobj divergence (RB3-Wii vs DC3-Xbox); R4 motivation +
   constraints (defect ledger, engine-pin mechanics, build integration).
2. **Architect** — Fable agent reads all recon handoffs, drafts PLAN.md +
   SPEC.md (bridge architecture, scoping options, implementation task DAG,
   risk register, go/no-go criteria).
3. **Review** — 3 parallel Opus skeptics (feasibility, ROI-vs-alternatives,
   asset/integration risk).
4. **Finalize** — Fable agent folds review into final docs + README index +
   recommendation.

## Conventions

- Handoffs are tracked markdown in `recon/` and `review/` under this dir.
- Checkpoint JSONs (workflow-resume recovery) in `/tmp/dc3bridge-ckpt/` —
  agents check-first, write-before-return.
- Read-only outside this directory. No edits to `src/`, `native/`, engine, or
  DC3 repos. No `ninja`, no cmake builds.
