# DC3 Render Bridge — Plan (ARCHITECT draft, 2026-07-12)

_Synthesis of recon lanes R1–R4 (`recon/*.md`). Planning only — no code was
changed and no builds were run. Every structural claim cites a recon handoff
(which carries the original file:line) or a direct read noted as such._

---

## 1. Problem statement — and what "better" would have to mean

The charter asks whether RB3-native should render its (Wii-lineage) scene graph
through the mature DC3 native rendering stack instead of / alongside its current
path, because the engine-arch-review campaign has repeatedly fought scene-render
defects on the RB3 path (CHARTER.md:13-24).

Recon reframes the question in two decisive ways:

**(1) The "bridge" is ~70% already built.** RB3-native does not render through
Wii GX or through anything in `rb3/native/src` — it already renders through the
shared `milo-native-engine` WebGPU renderer, the same engine repo DC3 consumes,
sharing the rndobj-free gfx CORE (GpuDevice / PipelineManager / BloomPass /
Screenshot) with DC3 (R1 summary; R1 finding 2, `milo-native-engine/
CMakeLists.txt:261-273`). What differs is the **backend flavor**:
`MILO_ENGINE_GPU_BACKEND=rb3` (`BandRnd : Rnd`, `Rnd_Wgpu_RB3.cpp` + six
`RB3*` pass TUs) vs `=dc3` (`WgpuRnd : NgRnd` + 6 rndobj-coupled gfx TUs + 8
Wgpu platform TUs incl. Mesh_Wgpu / Tex_Wgpu / Part_Wgpu / TransparentQueue)
(R2 finding 1, CMake:96-136,281-329). The split exists for one reason: the dc3
TUs' `.cpp` files include DC3's 2012-era rndobj headers, which RB3's 2010-era
matched-fork rndobj cannot satisfy (the Wave-2.3 finding,
`rb3/native/CMakeLists.txt:62-70`; R2 finding 3 lists the exact edges — no
`NgRnd` base, no `RndCam::GetViewProjectXfms`, packed `RndMesh::Vert`, no
`RndParticleSys::NumTilesAcross/Down`). So the correct framing is **backend
convergence inside one shared engine**, not construction of a new bridge
between two engines (R1 implication 1; R2 implication 1).

**(2) On the campaign's own defect ledger, a bridge fixes almost nothing that
is still open.** R4 classified all ~17 fought defect families against the
campaign's SYS-1..7 taxonomy: only **6 are class (b)** (RB3 render-path code a
renderer swap could touch), and of those, **5 are already fixed inside RB3's
own backend** (placement contract, venue wash/glow, UI-post-grade, state-leak /
DrawContext, UI text color) and the 6th (SYS-4 lighting completeness, where DC3
is genuinely ahead with ShadowPass/DofPass) is **measured near-no-op on RB3's
actual boot-reachable venues** (0 directional approx lights; Lambert already
beats box-ambient) (R4 finding 1 + tally, `execution/README.md:260,317`). The
deep, open, or terminal families — hands shard (terminal,
renderer-independent), patch-mesh composites (LP64 geometry), perf-clip
(game-logic), crowd (measurement artifact) — are all class (a)/(c) and
untouchable by any renderer swap (R4 findings 1, 3).

**Count, explicitly:** of the currently-OPEN defect families, the bridge
plausibly helps **at most one** — the W31 HUD-glyphs texture-bind family, which
R4 rates "plausible (b) — but a texture-bind bug, addressable in RB3's own
binder; not a renderer-architecture gap" (R4 table row W30-F2/F3/F4). Zero open
families *require* the DC3 backend.

**Therefore "better" cannot mean "fixes our bugs."** The only surviving
justifications are:

- **Architecture / maintenance** — SYS-2 content-coupling: RB3's backend grew
  ~30 asset-name `strcmp` branches (now flag-relocated) where DC3's is
  data-driven; and the engine repo currently carries two parallel
  implementations of mesh-cache, material-bind, postproc, and 2D-quad
  (`RB3MeshCache`/`MeshGpuCache`, `RB3MaterialBinder`/`MaterialSetup`,
  `RB3PostProc`/`PostProcPass`, `RB3Quad`/`DrawRect2D` — R1 finding 4; R4
  implication "strongest surviving pro-bridge argument is architectural").
  Two-implementation drift is a real cost in a repo two games consume — but
  it is **front-loaded and now near-static**, not compounding: 11 of the 15
  default fixes landed W6-W16, the last 12 waves produced ~3 flips all
  class (c) (render-path edit rate ≈ 0), and the ~13 asset-name `strcmp`
  branches are already flag-relocated (ROI review A2; R4:110-120, R4:46).
  The retirable duplication is also bounded: ~2k LOC of convergeable twins
  (RB3PostProc 560 + RB3Quad 555 + RB3MaterialBinder 625 + RB3MeshCache 308),
  each encumbered by shipped fixes; `Rnd_Wgpu_RB3.cpp` (6564 LOC) stays as
  frame-graph owner regardless (ROI review A4).
- **Future capability headroom** — DC3's ShadowPass / DofPass /
  TransparentQueue / data-driven material model are capabilities RB3's path
  lacks, valuable **if and when** content or a chartered feature needs them
  (R3 implication 4: DC3's material model is a superset; Wii/360 assets simply
  leave the extra fields at defaults).

Success criterion for this plan: **reduce duplicated render code in the shared
engine and make DC3's pass capabilities available to RB3 behind flags, at zero
visual regression to the ~15 shipped default-ON fixes — without betting any
open-defect burn-down on it.**

## 2. Scoping options

### Option A — scene-graphics-only bridge (RB3 rndobj → adapter → DC3 stack)

Marshal RB3's live scene objects into DC3-shaped render objects (or feed DC3's
`Mesh_Wgpu`/`Tex_Wgpu`/`Part_Wgpu` directly), leaving UI/track on the current
path.

- **Feasibility: effectively blocked at the object layer.** R3's central
  finding: pointer-level sharing is impossible (Transform 0x30 vs 0x40,
  `RndDrawable` bitfield-vs-bool base, shifted vtables — R3 findings 1, 5), so
  the adapter must *instantiate DC3-shaped objects*. But DC3's rndobj classes
  have the **same class names** as RB3's; both forks cannot compile into one
  binary without namespace-wrapping an entire engine fork (see SPEC §7 —
  the engine is a source library recompiled against ONE consumer's headers;
  there is no second set of symbols to link). A "GPU-intermediate" variant of
  Option A (translate to `GpuVertex` + material params instead of objects) is
  not a new option — it is exactly what `BandRnd` already does today
  (verified direct read: `RB3MeshCache.cpp:102-170` already emits the engine's
  shared `GpuVertex`/`GpuVertexSkinned`; `Rnd_Wgpu_RB3.h:43` —
  `using GpuVertexRB3 = GpuVertex`), i.e. Option A collapses into Option C.
- **Additional trap:** "scene-graphics-only" does not shrink the wall — scene
  meshes ARE the rndobj-coupled path (R4 implication: "scoping to scene
  graphics only does not dodge the wall").
- **Effort:** XL (namespace-fork of one rndobj lineage + per-frame marshaling
  of every mesh/mat/cam/env/light). **Risk:** very high (double scene state,
  per-frame copy cost, two sources of truth). **Defect coverage:** ~0 open
  families. **Maintenance:** worst of all options (three render data models).
  **Web:** marshaling cost lands on the slowest target (-O0 wasm; R1's unpack
  is already "the dominant -O0-wasm cost class", RB3MeshCache.cpp:99-101).
- **Verdict: NO-GO.**

### Option B — adopt the DC3 renderer wholesale (flip RB3 to the `dc3` flavor)

Uplift RB3's rndobj to the NgRnd contract so `WgpuRnd` + all dc3 TUs compile,
then delete `BandRnd`.

- **Feasibility:** requires giving RB3's matched-fork rndobj the DC3 shapes:
  `NgRnd` base, `GetViewProjectXfms`, **unpacked 96-byte `RndMesh::Vert`**,
  particle tile API, `BaseMaterial` base (R2 finding 3; R3 findings 1, 3).
  `RndMesh::Vert` is RB3's *wire format* — the matched decomp's load path and
  the Wii-matching goal both pin it; changing it forks the decomp or demands a
  pervasive `#ifdef HX_NATIVE` dual-layout with loader rewrites (R3 finding 1;
  CLAUDE.md decomp goal). RB3's own rndobj is also only 22/64 TUs clang-clean
  today (R4 finding 2, `rb3/native/CMakeLists.txt:231-244`).
- **Regression exposure is the killer:** `BandRnd` embodies the 31-wave
  campaign's shipped fixes — placement contract, chroma-preserve composite,
  UI-post-grade, DrawContext determinism, halo pass, mitten policy hooks,
  ~15 default-ON flags (R4 finding 1 table). The DC3 flavor hosts none of
  them, and DC3's own stack has known open issues RB3 would inherit (crowd
  impostors buggy, no occlusion queries, missing postproc modes —
  R2 finding 5, `RENDERING_SYSTEM.md:212-220`). We would re-fight fixed wars
  to gain capabilities measured near-no-op on RB3 content (R4).
- **Effort:** XL. **Risk:** very high. **Defect coverage:** ~0 open, negative
  expected (regressions). **Maintenance:** best long-term IF completed (one
  backend), but the transition valley is months of visual churn.
  **Web:** neutral (both flavors share the Emscripten seam; R4 finding 2).
- **Verdict: NO-GO** as a project; its end-state (one backend) is instead
  approached incrementally by Option C.

### Option C — no bridge: converge shared pass/technique code into the current path

Keep `BandRnd` as RB3's frame-graph owner. Inside the shared engine, make the
DC3-flavor pass TUs **rndobj-shape-neutral** via a small accessor seam
(SPEC §3: `SceneView`), so one implementation of TransparentQueue / ShadowPass /
DofPass / PostProcPass / DrawRect2D / material-bind compiles against *either*
consumer's rndobj headers; RB3 adopts each converged pass behind a
NativeCompatFlags flag only when a chartered need exists. Retire the
corresponding `RB3*` twin when a converged TU proves byte-stable.

- **Feasibility: strong, and cheaper than recon assumed.** Direct reads
  confirm the hard part is smaller than the "shape wall" suggests: the shared
  pass **headers are already rndobj-opaque** (forward decls only —
  `ShadowPass.h:7-9`, `TransparentQueue.h:3-5`); the coupling is confined to
  `.cpp` includes and member access (`ShadowPass.cpp:6-11`,
  `TransparentQueue.cpp:6-9`); TransparentQueue's only draw-side dependency is
  `extern void DrawMeshImmediate(RndMesh*)` (`TransparentQueue.cpp:17`),
  which BandRnd can supply. The vertex seam is **already converged**: both
  flavors emit the same `GpuVertex`/`GpuVertexSkinned`
  (`gfx/VertexFormats.h:9-29`; `RB3MeshCache.cpp:102-170`). Blend-mode enums
  are numerically identical across forks (R3 finding 3) but **not
  source-compatible**: `IsTransparentBlend`'s body qualifies them as
  `BaseMaterial::kBlend*` (`TransparentQueue.cpp:58-64`; identical
  `IsShadowTransparentBlend` in `ShadowPass.cpp:106-110`), and RB3 has no
  `BaseMaterial.h` — its blend enum lives on `RndMat`
  (`rb3/src/system/rndobj/Mat.h:129-142`, verified by finalizer). Porting
  requires a small enum re-sourcing (fork-neutral constants or a
  SceneView-side blend classification), not a verbatim copy — corrected per
  feasibility review B / integration review A1.
- **Effort:** S per pass after a one-time M seam; total M–L spread over
  need-gated increments. **Risk:** low-medium, bounded per-TU by flags and
  matched-frame A/B (camera-desync lesson, MEMORY). **Defect coverage:** same
  as any option (~0-1 open families) — but Option C doesn't pretend otherwise.
  **Maintenance:** each merge deletes a duplicated TU; drift shrinks
  monotonically; DC3 keeps its 21/21 stability untouched because dc3-flavor
  behavior is bit-preserved during the seam refactor. **Web:** no new wasm
  weight beyond the passes RB3 turns on.
- **Verdict: GO (recommended)** — with the explicit caveat that it is a
  code-health / capability-headroom investment, not a defect campaign.

### Comparison table

| | A: scene adapter | B: wholesale flip | C: pass convergence |
|---|---|---|---|
| Effort | XL | XL | M–L, increments |
| Feasibility | blocked (ABI + symbol collision, R3 f.5, SPEC §7) | blocked-ish (Vert = wire format; 22/64 LP64) | verified seam (headers already opaque) |
| Open-defect coverage | ~0 | ~0, negative expected | ~0-1 (honest) |
| Regression risk to 15 shipped defaults | high (dual state) | very high (loses BandRnd fixes) | low (flags, per-TU) |
| Two-engine drift | worse (3 models) | none at end, months of valley | shrinks per merge |
| Web build | worst (marshal cost on -O0 wasm) | neutral | neutral |
| DC3 blast radius | none | none | zero by construction (dc3 bit-preserved gates) |

## 3. Recommendation

**No bridge. Adopt Option C** — shape-neutral convergence of the shared
engine's pass TUs, dispatched as small, flag-gated, need-triggered increments —
and **recharter the visual campaign's remaining effort at class-(c) families**
(perf-clip, HUD-glyphs, patch-mesh), which R4 shows is where the open pain
lives.

### Go / no-go criteria

Proceed with a convergence increment only when ALL hold:

1. **Spike gate passed** (below) — the SceneView seam is proven on
   TransparentQueue end-to-end before any further node is dispatched.
   Because TransparentQueue is the *least* shape-divergent TU (feasibility
   review E; ROI review A1), the spike's PASS is made load-bearing two ways:
   (a) C2's acceptance includes a minimal ShadowPass-shaped read through
   `SceneView_RB3` (light enumeration + `CamViewProj` recompute — the
   member-vs-method and missing-method gaps ShadowPass actually hits), and
   (b) a parallel paper-audit node **C2b** diffs the C4/C5 fix-preservation
   surface (RB3PostProc/RB3MaterialBinder feature matrices vs their DC3
   twins) so the go/no-go also predicts the expensive nodes, not just the
   easy one.
2. **Need trigger** — a chartered defect, capability request, or measured
   drift cost names the specific pass. (E.g. ShadowPass waits for evidence of
   a boot-reachable venue with real directional/spot lights — R4's SYS-4
   near-no-op measurement is venue-sampled, not universal,
   `README.md:317`.)
3. **DC3 bit-preservation** — the seam refactor of a dc3-flavor TU must leave
   DC3 behavior identical (milo-tests 371/371 green; DC3's own consumers
   unaffected at their pin).
4. **RB3 A/B clean** — matched-frame headless screenshots (native harness,
   `boot-to-song.py` pattern) show zero diff with the new flag OFF, and an
   explainable, wanted diff with it ON.

Kill criteria (stop and revert the increment):

- A converged TU needs per-game `#ifdef`/branching in more than ~3 sites —
  the shapes are telling us they don't want to converge; keep the twins.
- Any default-ON regression on the 15 shipped fixes.
- Pin-reconciliation (node C0) stalls **at landing time** — a converged TU
  may not ship to production consumers while their pins diverge (R2
  implication 5: RB3 `b36bcfc`, DC3 `77eb428b`, HEAD `0083bad3`). Note C0
  gates *landing* (C7 / any production pin-bump), NOT the spike — the spike
  runs in an isolated engine worktree (ROI review B1, adopted). Integration
  review de-fanged this risk: the skew is a benign **linear fast-forward**
  (DC3 pin ⊂ RB3 pin ⊂ HEAD, `git merge-base --is-ancestor` re-verified by
  finalizer; ~130 commits, exactly 1 touching the shared gfx CORE).

### De-risking spike (charterable as-is)

**SPIKE-TQ: sorted transparency for RB3 through the shared TransparentQueue,
headless.** Smallest shape surface of any dc3 TU (Mesh blend/showing, Cam
world-pos, Env pass-through; one `extern DrawMeshImmediate`).

1. In an engine worktree (NO C0 dependency — the spike never touches
   production pins, ROI review B1), add `SceneView.h` + `SceneView_RB3.cpp`
   (SPEC §3-4) and port `TransparentQueue.cpp`'s **divergent** rndobj
   accesses to it (the fork-common virtual surface — `Name()`, `Select()` —
   stays as direct calls in the `.cpp`, SPEC §3 revised rule). Known
   in-scope work items, named so they aren't discovered mid-spike
   (feasibility review B/D):
   - re-source the `BaseMaterial::kBlend*` enum references in
     `IsTransparentBlend` (RB3 lacks `BaseMaterial.h`);
   - add the **new rb3 strong-def** of `extern void
     DrawMeshImmediate(RndMesh*)` (rb3 flavor currently supplies neither
     draw seam; DC3 defines both in `Mesh_Wgpu.cpp:159,332`).
   Compile the TU into the **rb3 flavor** behind new flag
   `RB3_TRANSPARENT_SORT` (default OFF). `BandRnd::DrawMesh` gains a
   queue-or-draw fork + flush at pass end.
2. **Gate-hardening read (feasibility review E):** also implement and
   exercise (a debug dump suffices) a minimal ShadowPass-shaped read via
   `SceneView_RB3`: `EnvLightCount`/`EnvLightAt`/`LightType`/`LightColor`
   over `mLightsApprox` (RB3 member vs DC3 method) and a `CamViewProj`
   recompute (RB3 lacks `GetViewProjectXfms`) — so a PASS de-risks the C1/C3
   accessor surface, not just extern-draw plumbing.
3. Build `rb3-native` (~3s loop, CLAUDE.md), run the existing headless
   boot-to-song harness, capture matched frames OFF vs ON at 3 checkpoints
   (hub, song-select, gameplay). **Run the web smoke too** — the rb3 flavor
   has never compiled a Tier-2 pass TU under Emscripten; native-green is not
   web-green (integration review caveat).
4. **Pass:** OFF is byte-identical to baseline; ON renders without crash and
   any diff is explainable as draw-order change; the step-2 reads return
   sane values; web smoke green. Also rebuild the **dc3** flavor
   (compile-only + milo-tests at the worktree HEAD) to prove the seam didn't
   disturb DC3.
5. **Interpretation guard:** W1.6 Exit-A already refuted sorted transparency
   as a *fix* for the state-leak family (R4 table, `README.md:129-130`) — the
   spike's success metric is **seam viability**, not a visual win. ON stays
   default-OFF until some defect actually wants it.

Size: S-M (one wave, one agent) — now genuinely bounded, since C0 no longer
gates it. If the spike fails on LOC blow-up or wasm/perf cost, Option C halts
at "keep the twins, document why" — total sunk cost one wave.

## 4. Implementation task DAG (Option C)

Nodes are dispatchable work packets for a future ultracode wave. All engine
edits happen in `../milo-native-engine` (own repo, commit there first, then
bump `MILO_ENGINE_PIN` — CLAUDE.md git rules). Sizes: S ≤ ½ day, M ≈ 1-2 days,
L ≈ 3-5 days.

```
C1 (minimal slice) ──► C2 (SPIKE-TQ, gate) ──┬──► C3 ──► C4 ──┬──► C7
C2b (paper-audit, parallel w/ C2) ───────────┼──► C5 ─────────┤
                                             └── (C3) ─► C6 ──┘
C0 (pin reconciliation, LANDING gate only) ───────────────────► C7 / any pin-bump
```

(DAG re-wired per ROI review B1 — C0 no longer gates the spike; C1 re-scoped
per integration review A3; C5 unchained from C4 per integration review A4;
C2b added per ROI review A1.)

**C0 — Engine pin reconciliation** (S) — **landing gate, not a spike
prerequisite**
- Files: `rb3/native/CMakeLists.txt` (`MILO_ENGINE_PIN`, :74),
  `dc3-decomp/native/CMakeLists.txt` (:225-227), engine HEAD.
- Do: land a single engine SHA both consumers pin (RB3 `b36bcfc` and DC3
  `77eb428b` are both behind HEAD — R2 implication 5). Integration review
  verified the skew is a benign linear fast-forward (DC3 pin ⊂ RB3 pin ⊂
  HEAD; ~130 commits, exactly 1 touching shared gfx CORE, dc3-flavor
  touches self-labeled byte-identical MOVEs or default-OFF flags).
  Coordinate with whichever wave owns each consumer. Also confirm/flag the
  stale `dc3-decomp/native/src/{gfx,platform}` duplicates stay out of DC3's
  link line (R1 finding 2 caveat; MEMORY
  `project_dc3_native_engine_masking.md`).
- Accept: both consumers build green at one pin; pins recorded in engine
  README. Required before C7 or any production pin-bump of a converged TU;
  NOT required to run C1/C2/C2b (isolated worktree).

**C1 — SceneView accessor seam, minimal TransparentQueue slice** (S)
- Files (new, engine): `src/gfx/SceneView.h`,
  `src/platform/SceneView_DC3.cpp`, `src/platform/SceneView_RB3.cpp`;
  CMake: add per-flavor impl TU to each flavor's source list
  (CMakeLists.txt:281-288 / :317-329 blocks).
- Do: implement ONLY the accessors C2 needs — the TransparentQueue slice
  (mesh blend/showing/dist, blend classification) plus the gate-hardening
  ShadowPass-shaped reads (light enumeration, `CamViewProj`) — per SPEC §3's
  own grow-on-demand rule (integration review A3: the full §3 surface before
  the gate was speculative). The rest of the surface grows inside C3+ as
  each pass lands.
- Accept: `SceneView.h` includes no rndobj header (grep-enforced **on the
  header only** — pass `.cpp` files may include rndobj headers for the
  fork-common virtual surface, SPEC §3 revised); both flavor impls compile
  in their respective flavor builds; a milo-tests unit exercises the DC3
  impl.

**C2 — SPIKE-TQ (gate node)** (S-M) — exactly §3's spike. Depends: **C1
only** (ROI review B1).
- Accept = spike pass criteria above (incl. enum re-sourcing, new rb3
  `DrawMeshImmediate` strong-def, ShadowPass-shaped read, web smoke).
  **No downstream convergence node dispatches until C2 reports PASS.**

**C2b — C4/C5 fix-preservation paper-audit** (S-M) — parallel with C2, no
build required (ROI review A1).
- Do: diff the twins' feature matrices on paper — `RB3PostProc` (chroma-
  preserve, UI-post-grade) vs `gfx/PostProcPass`, and `RB3MaterialBinder`
  (UI_TEXT_FLOOR + GX-TEV model) vs `MaterialSetup`/`BaseMaterial` surface —
  and classify each shipped fix as absorb-able / fork-forcing / twin-keeping.
- Accept: a written audit that either upgrades confidence in C4/C5 or
  re-scopes them to "share subfunctions only" (SPEC OQ3). C4 and C5 do not
  dispatch without it.

**C3 — Seam-port the remaining dc3-coupled TUs** (M)
- Files (engine): `src/gfx/{ShadowPass,DofPass,PostProcPass,DrawRect2D}.cpp`
  (+`TextureConvert.cpp` only if its RndBitmap edge is trivial; else skip —
  texel decode stays per-platform per R3 finding 6/implication 3).
- Do: replace direct **divergent** rndobj access with SceneView (fork-common
  virtuals may stay direct, SPEC §3 revised); move each TU from
  `MILO_ENGINE_GFX_RNDOBJ_SOURCES` to the shared tier; dc3 behavior
  bit-preserved. ShadowPass additionally needs (feasibility review C/D):
  strip/neutralize its `platform/Rnd_Wgpu.h` include (`ShadowPass.cpp:5` — a
  dc3 renderer-class coupling SceneView does not cover), re-source its
  `BaseMaterial::kBlend*` enums (`ShadowPass.cpp:106-110`), and the rb3
  flavor must supply a **new strong-def** of the second extern draw seam
  `DrawMeshShadow(RndMesh*)` (`ShadowPass.cpp:103`; DC3's is
  `Mesh_Wgpu.cpp:332`).
- Accept: DC3 flavor builds + milo-tests 371/371; RB3 flavor *compiles* the
  TUs (not yet enabled) incl. `DrawMeshShadow` supplied and no residual
  `Rnd_Wgpu.h` dependence; no `#ifdef` per-game forks introduced (kill
  criterion).

**C4 — RB3Quad/RB3PostProc convergence merge** (M-L) — Depends: C3, C2b.
- Files: engine `src/platform/{RB3Quad,RB3PostProc}.cpp` vs
  `src/gfx/{DrawRect2D,PostProcPass}.cpp`.
- Do: driven by C2b's audit (RB3PostProc carries the W5-W10 chroma-preserve
  / UI-post-grade fixes, R4 table); merge only shared subfunctions or adopt
  the gfx TU behind a default-OFF flag; the shipped-fix behavior is the
  reference, not DC3's.
- Accept: matched-frame A/B zero-diff at defaults; any deleted twin's flags
  re-homed; NativeCompatFlags census updated (line-count trap:
  DECOMP_FORCEACTIVE `__LINE__` — MEMORY W30).

**C5 — Material-bind convergence (`RB3MaterialBinder` vs `MaterialSetup`)**
(L, **plausibly XL** — ROI review A3) — Depends: C1, C2, C2b; parallelizable
with C3/C4 (integration review A4: `MaterialSetup.cpp` is in the dc3
platform list, untouched by C3's gfx seam-port).
- Do: introduce the resolved `MaterialParams` struct (SPEC §5; R3 finding 3's
  superset mapping — RB3 fields map into DC3's `BaseMaterial` surface,
  DC3-only maps default null/off); both binders emit it; then unify.
  This is the SYS-2 payoff node (data-driven binder replaces strcmp pile) —
  but it unifies structurally-different material models (RB3 GX-TEV
  fixed-function vs DC3 BaseMaterial 0x1f8 shader-era superset, R3:64-86)
  while RB3MaterialBinder is 625 LOC carrying shipped fixes (UI_TEXT_FLOOR).
  **Gate behind an explicit SYS-2 charter**, not the general convergence
  sweep; C2b's audit sizes it before dispatch.
- Accept: RB3 matched-frame A/B clean across hub/song-select/gameplay +
  the W6-W16 text/color flags still honored; DC3 21/21 matrix untouched.

**C6 — Capability enablement, need-gated** (S each, unbounded count)
- Do: per go/no-go criterion 2, enable a converged pass for RB3 behind a
  default-OFF flag when a charter names it (ShadowPass ← venue with real
  lights; DofPass ← authored DOF postproc; TransparentQueue ON ← an actual
  ordering defect). Depends: C3.
- Accept: per-pass A/B + the naming charter's own acceptance.

**C7 — Retire twins + docs** (S) — Depends: C0 (the landing gate) + whichever
of C3-C6 landed.
- Do: delete superseded `RB3*` TUs from the rb3 flavor list
  (CMakeLists.txt:317-329), update `GRAPHICS_REFACTOR.md` /
  `RENDERING_SYSTEM.md` and this hub's README with the end-state; final pin
  bump in both consumers.
- Accept: engine builds both flavors green; grep shows no orphaned twin.

## 5. Risk register

| Risk | Where it bites | Mitigation |
|---|---|---|
| **Asset-format mismatch** — Wii GX texel layouts (CMPR/tiled) and Wii compressed-vert blobs corrupt through DC3's Xbox decoders (`ByteSwapDXT`/`UntileMilo`/`UnpackCompressedVertices` are Xbox-360-specific; R3 findings 2, 6) | Any node that touches TextureConvert or vertex unpack | Hard rule in SPEC §6: platform decode stays consumer-side; shared code consumes only decoded RGBA + `GpuVertex`. NOTE (direct read): RB3-native already decodes an **Xbox** compressed-vert path (`RB3MeshCache.cpp:141-168` `XboxCVert`/`BeDec4n`) because the port prefers 360 assets (MEMORY `feedback_prefer_xbox_assets`) — see OPEN QUESTION 2. |
| **LP64** — RB3 rndobj only 22/64 TUs clang-clean (`Trans/Tex/Mesh/Rnd.cpp` among the dirty; R4 finding 2) | SceneView_RB3 compiles against those headers | Seam accessors read only fields the current BandRnd already reads (proven LP64-viable); no new header surface. Patch-mesh LP64 trap (`MeshVert` 32-bit-shaped, twice bisect-reverted — R4 table) is explicitly out of scope. |
| **MWCC-vs-MSVC lineage types** — Transform 0x30 vs 0x40, Vector3 padding, RndDrawable base divergence (R3 finding 5) | Any attempt to share object layouts (Option A/B residue) | Option C never shares layouts: one binary = one rndobj fork; SceneView is compiled per-consumer (SPEC §7). Kill criterion catches creep. |
| **Two repos / pin skew** — engine + 2 consumers at 3 SHAs (R2 implication 5) | Every convergence increment | C0 first; every landing = engine commit + same-day pin bumps; soft pin only warns (`rb3/native/CMakeLists.txt:80-85`) so CI/agents must check the warning. |
| **Perf / wasm budget** — accessor indirection on hot per-draw paths; -O0 wasm magnifies (R1: unpack already dominant wasm cost) | C2/C3 on web | SceneView accessors header-inline where possible; A/B a frame-time counter native + one web smoke; 28 MB brotli budget check in C7. |
| **Behavioral regression to shipped defaults** — BandRnd embodies 15 default-ON fixes (R4) | C4/C5 especially | Matched-frame A/B at defaults is a hard accept on every node; shipped behavior is the reference implementation. |
| **Transparency semantics** — W1.6 Exit-A refuted sort-fix; RB3 deliberately submits traversal-order (`Rnd_Wgpu_RB3.cpp:2529`, direct read) | C2/C6 | TransparentQueue stays default-OFF for RB3 until a defect charters it; spike measures seam viability only. |
| **Engine mid-refactor** — DC3 graphics stack is "a compatibility shape mid-refactor" (R2 finding 5, `GRAPHICS_REFACTOR.md:31-57`) | Converged TUs could be moved again by DC3's Phase 1-6 | Convergence *reduces* that risk (refactors then happen once, shared); coordinate C3 scope with the DC3 refactor phases doc. |
| **Opportunity cost** — this is code-health work while open defects are class (c) (R4) | Campaign planning | Recommendation §3 explicitly recharters defect effort to class (c); Option C nodes are small and interruptible. |
| **License** | Cross-repo reuse | No LICENSE file in any of the three trees (R4); same-org inference. Flag to owner; low risk. |

## 6. Recon gap check

All four chartered lanes delivered (`recon/R1..R4-*.md`; checkpoints
`/tmp/dc3bridge-ckpt/R{1,2,3,4}.json` all present). No missing lane. R3's one
flagged open question — "does milo-native-engine already have a Wii-aware
GpuVertex near DC3's?" — was resolved by direct read during synthesis: **yes,
stronger — it is the same struct** (`gfx/VertexFormats.h:9-29`,
`Rnd_Wgpu_RB3.h:43`, `RB3MeshCache.cpp:102-170`). Remaining open questions are
listed in SPEC §9 and the structured output.

## 7. Review adjudication (FINALIZER, 2026-07-12)

Three review lanes ran, all delivered (`review/{feasibility,roi,integration}.md`;
checkpoints `/tmp/dc3bridge-ckpt/REV-*.json`). Verdicts: feasibility
**GO_WITH_CHANGES** (0 blocking), roi **GO_WITH_CHANGES** (1 blocking),
integration **GO** (0 blocking). No lane missing. Every blocking finding is
adjudicated below; advisories were folded into the sections above where cheap.
Load-bearing review claims were independently re-verified by the finalizer
before editing (direct reads of `TransparentQueue.cpp`, `ShadowPass.cpp`,
`rb3 Mat.h:129-142`, grep for the extern draw seams, and
`git merge-base --is-ancestor` on the three pins).

| Finding | Disposition | Where |
|---|---|---|
| **ROI B1 (BLOCKING)** — "spike is cheap/one-wave" falsified by C0→C2 edge; spike doesn't need pin reconciliation | **FIXED** — DAG re-wired: C0 is now a landing gate (prerequisite of C7/pin-bumps only); C2 depends on C1 alone and runs in an isolated engine worktree. Kill criterion re-worded to landing-time. Verified premise: spike builds `rb3-native` from a worktree and checks DC3 at worktree HEAD, never touching production pins. | §3 spike step 1, §3 kill criteria, §4 DAG + C0 + C2 |
| Feasibility A / Integration A2 — SPEC §3 "read-only" invariant false for SPIKE-TQ (`cam->Select()` / `env->Select(nullptr)`, `TransparentQueue.cpp:137-145,205-217`, finalizer-verified) | **FIXED** — adopted integration's resolution (b): rndobj-free is grep-enforced on `SceneView.h` ONLY; pass `.cpp`s may keep rndobj includes for the fork-common virtual surface (`Name()`, `Select()`); only divergent accesses route through SceneView. SPEC §1/§3 and PLAN C1 acceptance updated. | SPEC §1, §3 rules; PLAN C1/C3 |
| Feasibility B / Integration A1 — "`IsTransparentBlend(int)` ports as-is" false at source level (`BaseMaterial::kBlend*`, RB3 has no `BaseMaterial.h`; enum on `RndMat`, `Mat.h:129-142` — finalizer-verified) | **FIXED** — Option C evidence bullet corrected; enum re-sourcing named as explicit spike work item (also `IsShadowTransparentBlend` for C3). | §2 Option C, §3 spike step 1, C3 |
| Feasibility C — SPEC §2 under-states ShadowPass (`platform/Rnd_Wgpu.h` include at `ShadowPass.cpp:5`; second extern seam `DrawMeshShadow` at `ShadowPass.cpp:103` — finalizer-verified) | **FIXED** — SPEC §2 table row and §3 draw-dispatch rule updated; C3 do/accept lines added. | SPEC §2, §3; PLAN C3 |
| Feasibility D — rb3 flavor supplies NEITHER extern draw seam today (grep empty in `Rnd_Wgpu_RB3.cpp`/`RB3*.cpp`; DC3 defines both at `Mesh_Wgpu.cpp:159,332` — finalizer-verified) | **FIXED** — named acceptance lines: C2 adds rb3 `DrawMeshImmediate`, C3 adds rb3 `DrawMeshShadow`. | §3 spike, C2, C3 |
| Feasibility E / ROI A1 — SPIKE-TQ is a weak proxy (smallest shape surface; doesn't exercise the C1/C3 accessor gaps or the C4/C5 fix-preservation risk) | **FIXED** — two-part hardening: C2 acceptance now includes a minimal ShadowPass-shaped read via `SceneView_RB3` (light enumeration over member-vs-method, `CamViewProj` recompute); new parallel node C2b paper-audits the C4/C5 fix-preservation surface and gates their dispatch. | §3 criterion 1, spike step 2, §4 C2b |
| ROI A2 — "compounding drift" oversold; divergence is front-loaded, now near-static | **FIXED** — §1 wording softened with R4's numbers; bounded-payoff figure (~2k LOC, fix-encumbered) added per ROI A4. | §1 |
| ROI A3 — C5 undersized (L → plausibly XL; structurally different material models) | **FIXED** — C5 re-rated "L, plausibly XL", gated behind an explicit SYS-2 charter, sized by C2b before dispatch. | §4 C5 |
| ROI A4 — payoff bounded ~2k LOC, confirms code-health framing | **FOLDED** into §1 (see A2 row). No recommendation change needed — the plan already framed Option C as code-health, not defect work. | §1 |
| Integration A3 — C1-vs-C2 contradiction (full-surface C1 before the gate violates SPEC's grow-on-demand rule) | **FIXED** — C1 re-scoped to the minimal TransparentQueue slice + gate-hardening reads (S, was M); surface grows inside C3+ as passes land. | §4 C1 |
| Integration A4 — C4→C5 over-serialized | **FIXED** — C5 unchained from C4; parallelizable with C3/C4 after C1+C2+C2b. | §4 DAG, C5 |
| Integration web-caveat — "web neutral" is structural, not demonstrated; rb3 flavor never compiled a Tier-2 pass TU under Emscripten | **FIXED** — C2 acceptance requires the web smoke explicitly (native-green ≠ web-green). | §3 spike steps 3-4 |
| Integration verification bonus — pin skew is a benign linear fast-forward (finalizer re-verified: DC3 pin ⊂ RB3 pin ⊂ HEAD, ~130 commits, 1 shared-CORE touch) | **FOLDED** — de-fangs SPEC OQ5 and the C0 kill criterion; C0 stays S-sized with high confidence. | §3 kill criteria, §4 C0, SPEC §9 OQ5 |

**Deferred (not folded):** none of the reviews' remaining notes require plan
changes — feasibility's -O0 inlining note is already covered by the risk
table's accessor-indirection row; the license flag (OQ6) stays an owner
call-out; OQ2 (Wii asset census) remains a 5-minute pre-C3 task.

**Post-adjudication verdict: GO for Option C as amended** (no bridge;
spike-gated, need-gated pass convergence; code-health/headroom framing). No
reviewer returned NO_GO; the single blocking finding was a DAG mis-wiring that
strengthened the plan once fixed rather than undermining it. The
recommendation in §3 stands unchanged in substance.
