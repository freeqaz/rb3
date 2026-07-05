# Milo Native Engine — Architecture Review (Verdict)

**Date:** 2026-07-05 · **Synthesis of** the six review lanes in this directory
(`01-renderer-core.md` … `06-arch-crosscut.md`). Read-only review; this is the verdict
document. The phased fix plan is `REFACTOR_PLAN.md`.

> **Coordinator sign-off (Fable, 2026-07-05):** FINAL. I independently spot-checked the
> load-bearing claims against the code before endorsing: the skinned `obj.world = identity`
> branch with stacked name-scoped exceptions is real (`Rnd_Wgpu_RB3.cpp:4522-4529` — SYS-1),
> `DrawMesh` spans `:3837`→`~6510` (~2,670 lines), 64 `strcmp/strstr` sites in the file,
> `RndEnviron::IsValidRealLight` is at `Env.cpp:172` as cited (SYS-4), and
> `rb3/native/src/rb3_band_rnd.cpp` is 0 lines (boundary-graduation precedent). Verdict
> `targeted-refactor` is endorsed; the Phase-0-first sequencing in `REFACTOR_PLAN.md` is
> **binding** for implementation fleets.

---

## Executive summary — is the codebase poorly constructed?

**No, not in general. Overall verdict: `targeted-refactor`** — a sound shared core carrying
**one overhaul-scale chunk** (the RB3 render backend) and **four bounded correctness redesigns**
(skinned transform/bind, lighting selection, BandPatchMesh, UI data layer), all landable
incrementally *once the safety nets exist.*

The codebase is genuinely well-layered where it counts. The engine is **one shared library**
(`milo-native-engine/CMakeLists.txt:445`), not two forks; DC3/RB3 is a configure-time GPU-backend
flavor (`MILO_ENGINE_GPU_BACKEND ∈ {off,dc3,rb3}`, `CMakeLists.txt:89-134`). The native build
compiles the decomp's **own faithful `src/system/` C++** for UI layout, text, skeleton pose, and
material rules (`rb3/native/CMakeLists.txt:250,261`) — so most of the "bugs" are *not* divergent
reimplementations. The GPU skin blend math is algebraically identical to the Wii
`RndMesh::SkinVertex` (lane 02 §Q1). 16 CPU gtests (8,029 lines) pass. `rb3_band_rnd.cpp` already
*graduated* from game-glue into the engine (now 0 lines, `rb3/native/src/rb3_band_rnd.cpp`) — proof
the team can move a boundary correctly.

But the bug family the review was convened over is **systemic, not point-bugs**, and it clusters
into a small number of structural faults. Five of six lanes returned `refactor`; the renderer-core
lane returned `overhaul` for `Rnd_Wgpu_RB3.cpp` specifically — a **7,017-line god-file** with a
**~2,670-line `DrawMesh`**, **113 `getenv` calls**, **~30 hardcoded asset-name `strcmp` branches**,
**5 inline WGSL shader modules**, a **mutable scene bind group re-created an unbounded number of
times mid-frame**, and **no depth-sorted transparent pass** (lane 01 §1-2). That one file is where
the correctness debt is concentrated, and it is the reason "faithful" data-path changes
(BandPatchMesh) keep breaking rendering: correct data hits a *heuristic* renderer whose correctness
depends on name-matched exceptions and draw-order-dependent global state.

The disposition is therefore not "rewrite the engine." It is: **build the missing render/skin
regression nets first**, then **decompose the one monolith** (mechanically, behavior-preserving),
then land the **bounded redesigns** riding on the now-legible pipeline, retiring one default-ON
workaround flag per fix. The sequencing is non-negotiable — the BandPatchMesh double-revert proves
that attempting the correctness rewrites before the nets exist is scheduling the third revert.

---

## Systemic root causes (ranked by how many observed bugs each explains)

These are cross-lane patterns, not per-bug. Ranked by bug-count and by how load-bearing each is.

### SYS-1 — The skinned-mesh transform/bind contract is wrong and bimodal *(explains the most)*
**~7 bugs. Lanes 02, 03, 05 converge here.** For every skinned mesh the live path forces
`obj.world = identity` and derives on-screen position **entirely from the bone palette**
(`BoneOffsetAt(b)·boneTrans->WorldXfm()`) read from `mesh->GeomOwner()`
(`Rnd_Wgpu_RB3.cpp:4556-4557, 3849, 4876`). This is correct *only* when the bones already encode
world placement; it collapses to the origin whenever placement arrives via `SetWorldXfm` on a
parent Dir/group that is not the bones' `TransParent`. Compounded by two bind faults: outfit/
appendage meshes bind the **shared static** `char/main/skeleton.milo` magnet, not the per-member
animated `skeleton_unshared` (lane 02 H1, `CHAR_SKINNING_DEFORM_INVESTIGATION.md`); and the baked
`invBind` (`BoneOffsetAt`) is in the *magnet's* rotation basis, so a full-body rebind shards thin
finger/hair geometry by ~R·sin(θ) (lane 02 H2). The engine already patches this **three times**
with per-mesh-name one-offs (hub bar `:4486-4492`, scrollbar thumb `:4516-4550`, crowd
`RebindCrowdCharBonesToOwnSkeleton` `Crowd.cpp:906-1040`) — the smoking gun that it is one general
fault papered over case-by-case. **Manifests as:** hands/fingers deform, crowd-at-one-point,
drum-kit-at-one-point, hub highlight bar at origin, scrollbar thumb at origin, count-in thin-geo
shards, and (partly) HUD mid-screen placement.

### SYS-2 — Heuristics-instead-of-faithful-mechanisms culture *(explains the fragility of ~all render fixes)*
**Meta-cause across lanes 01, 04, 05.** The faithful Milo engine is **data-driven** (behavior flows
from `RndMat`/`RndEnviron`/`RndTransformable` data). This backend is **content-coupled**: it
branches on ~30 hardcoded RB3 asset-name strings (lane 01 §1c), detects text by `!mesh->Name()[0]`
(lane 05 §1b — which *collides* with nameless crowd billboards, `:6054`), and "lights" the highway
by `cam=="game.cam" && mat=="surface.mat" → color*=0.12` string surgery (lane 04, `:6160-6205`).
Every fix that lands is another `strcmp` special-case, so the file grows without the underlying
pipeline (skinning, transform, lighting) ever being made correct — and **any asset/material/venue
whose name isn't special-cased is mis-rendered.** This is *why* faithful data-path changes break:
they shift draw order / mesh identity outside the name-matched exceptions.

### SYS-3 — State leakage in a monolithic backend *(structural enabler of SYS-1/2/4 instability)*
**Lane 01 §2.** `mSceneBindGroup` is a single **mutable member re-created an unbounded number of
times per frame** (`WriteSceneUniforms:1581`, re-invoked per-mesh on cam re-pose `:3912-3925` and
per-environ `:3934-3940`); every draw implicitly depends on "whatever the last write left in the
member." Correctness hinges on the engine calling `Select()`/re-posing cams in exactly the sequence
the heuristics anticipate (`mLastSceneCamPose` 6-float dirty compare `:3915-3919`). Combined with a
2,670-line `DrawMesh`, no `DrawContext` (state is left in members, not passed), and **no
transparent/depth-sorted pass** (lane 01 §2d; `TransparentQueue` is DC3-only), this makes the
renderer's output **order-dependent global state** — so a faithful mesh rewrite that changes
submission order can silently corrupt lighting/placement of *unrelated* draws.

### SYS-4 — The lighting model is inverted and incomplete *(3 lighting bugs)*
**Lane 04 §1-3.** The real/approx light routing is **inverted vs the engine's own
`IsValidRealLight`** (`Env.cpp:172`): point+fakespot should drive hardware Lambert and directionals
should fold into a 6-axis box ambient — our venue path promotes `mLightsApprox` (directionals) to
Lambert, and the char path reads `mLightsReal` (point+fakespot only, so its directional loop is
dead). The **box-ambient model is absent entirely** (collapsed to a scalar `ambientColor` with
clamp/floor/grey-key knobs). Fog/shadows/spot lights are **hard-off** (`:1566-1568`) despite the
plumbing existing. **Manifests as:** flat/over-bright or wrong-colored faces and venue, and a
highway that is faked by material-color surgery instead of lit.

### SYS-5 — Data-format mismatch: 360 assets on a Wii-decomp engine *(4 UI bugs — NOT the engine's fault)*
**Lane 05 §3 — the crux of the UI lane.** The port forces `kPlatformXBox` and loads Xbox 360
`.milo_xbox` assets (`main_native.cpp:360,527,715`) through Wii-decomp code that expects Wii data.
The 360 milos differ in non-square font atlases (`Font.h`), widget *classes* (plain `UILabel` where
Wii used `AppLabel`), and author-visible-not-hidden slots. **Manifests as:** squished/too-tall menu
text, stale-slot song-row overlap (`MusicLibrary.cpp:1094-1108`), and HUD score-not-updating
(`UILabel.cpp:952`). These want an **asset-layer** fix (load Wii assets, or one normalization
layer), not renderer heuristics. This is the one systemic cause that is *not* an engine defect.

### SYS-6 — Silent weak stubs (invisible-failure class) *(2 shipped bugs)*
**Lane 06 §2.** 832 weak link-stubs resolve to a single `xorl %eax,%eax; ret` no-op with **zero
logging** (`band3_link_stubs.s:19-21`). A genuinely-unreachable stub and one silently swallowing a
live call are **byte-identical at runtime**. Safety is asserted only by a hand-maintained code
comment. This exact mechanism already shipped twice: particles never rendered
(`DrawParticlesBillboard` no-op) and "song never ends" (`EndGame` no-op → `GameEndedMsg` never
fired). Every one of the 521 function stubs is a dormant version of that bug.

### SYS-7 — Gate blindness / no render-correctness test *(explains why all of the above shipped or reverted undetected)*
**Meta-cause, lanes 02, 06.** There is **no golden-image / SSIM / per-draw / per-vertex render test
in the engine** (`grep -liE 'golden|ssim|pixel.?diff' tests/` → empty). All 16 gtests are CPU
data-path; `test_bone_ground_truth.cpp` validates bone *topology/symmetry* but **never a skinned
vertex position**. The rb3-side Python visual gates are documented **blind**: the band-closeup
drop/ratio gate PASSed **34/34 on exploded patch-shard frames**, and both faithful BandPatchMesh
rewrites broke rendering and were caught only by human review. There is a test hole exactly the
size of every bug on the list.

---

## Bug → cause map

Each known bug mapped to its most-supported root cause, with confidence and lane evidence. Where
lanes touch the same bug, agreement/disagreement is called out.

| Bug | Root cause (systemic) | Confidence | Evidence |
|---|---|---|---|
| **Hands/fingers deform wrong** | SYS-1: outfit/appendage meshes bind the shared static skeleton magnet; baked `invBind` is in the magnet's rotation basis so full-body rebind shards thin geo. Fix = rebind to per-member animated skeleton **and** rebake `invBind` against the per-member bind pose. | measured | Lane 02 H1/H2/H3; `Rnd_Wgpu_RB3.cpp:4678-4681` |
| **Crowd all at one point / not animating** | SYS-1: skinned `obj.world=identity` + **shared `GeomOwner` skeleton** → every instance reads the last-posed owner bones; the latched rebind binds only the first archetype (`Crowd.cpp:1026`). 2D imposter path is correct and NOT the bug. | measured | Lane 03 H1 (primary); Lane 02 flags it low-confidence and **defers to lane 03** — no conflict |
| **Drum kit at one point** | SYS-1: instruments are bone-attached prop meshes (`BandCharacter.cpp:775`) → same identity-`obj.world` collapse when prop bones resolve to origin. | high | Lane 03 §Q3 |
| **Only some band members placed** | **Independent game-side data gap**, NOT the transform engine: `Character::Teleport→SetLocalXfm` propagates correctly; mislocated members are those whose `BandConfiguration`/`BandWardrobe` waypoint never resolved/fired. | high | Lane 03 §Q3 (explicitly separates this from SYS-1) |
| **Hub highlight bar / scrollbar thumb at screen center** | SYS-1: `SetWorldXfm` on a UI labelDir doesn't reach the skinned corner-bone parent chain → palette places at world origin. Two name-scoped injections today. | measured | Lanes 01, 03, 05 all agree, same file:lines (`:4459-4535`) |
| **Song-library squished text / stale-slot overlap / headers-as-rows** | SYS-5: 360 `.milo_xbox` assets (non-square atlas, `UILabel`≠`AppLabel`, author-visible slots) on Wii-decomp engine. **NOT layout-math, NOT z-order.** | measured | Lane 05 §2-3 |
| **HUD score not updating / star meter wrong** | SYS-5 widget-class mismatch (`set_score_or_stars` unhandled without the `UILabel.cpp:952` fallback) **+** SYS-1 origin-placement family for the mid-screen position. | high | Lane 05 §2 |
| **Text invisible / black-on-dark; white-icon-blob** | SYS-2 render: font glyph lives in texture *alpha* (RGB==0); faithful mechanism is material-authored alpha-as-color-source; current code re-derives it from mesh/material name substrings. | measured | Lane 05 §1c |
| **Lighting fragility (flat/bright faces, venue, constant string-keyed hacks)** | SYS-4: inverted real/approx routing + absent box-ambient + game.cam has no environ so highway is faked by material surgery. | high | Lane 04 §1-3 |
| **BandPatchMesh faithful rewrites broke rendering twice** | SYS-1-adjacent + LP64: `MeshVert` is 32-bit-ABI-shaped (raw offsets/pointer arithmetic); asm-faithful rewrites reproduce Wii memory/index behavior that under LP64 layout yields degenerate topology. **SYS-7 gate blindness** let both reverts ship green. | measured | Lane 02 §Q3; `3d00d1dd` LP64 fix precondition |
| **Web grey skin (skin-RTT composite bakes grey)** | **Not lighting.** RTT state/colorspace-contract fault: composite RT format/colorspace disagrees between Dawn-native and browser WebGPU/ANGLE (`linearToSrgb` applied only for the non-sRGB web surface), or RT clear/blend defaults collapse the composite. Timing **disproven**; bypassed via direct-bind (`266ffb1b`). | medium | Lane 04 §4 |
| **Particles never render / "song never ends"** | SYS-6: silent weak stubs (`DrawParticlesBillboard`, `EndGame`) → no-op, zero logging. | measured | Lane 06 §2.2 |

**Where lanes disagree:** there is no substantive disagreement. The only overlap needing
adjudication is crowd-at-one-point, which **lane 02** notes as a low-confidence cross-lane
hypothesis and explicitly defers to **lane 03**, which owns it with measured evidence. All lanes
that touch the hub-bar/scrollbar/crowd placement family (01, 03, 05) independently arrive at the
same SYS-1 root cause and cite the same `:4459-4535` code.

---

## What is GOOD and must be preserved

The overhaul must not throw these out — they are faithful, correct, or sound design, and several are
the *convergence targets* the refactor pulls toward.

**Faithful mechanisms (keep as-is):**
- **GPU skin blend math** — `skinMat=BoneOffsetAt(i)·boneWorld_i`, weighted sum, object-identity, is
  algebraically identical to Wii `RndMesh::SkinVertex`; `MAX_BONES=40` matches; row/column convention
  correctly transposed (lane 02 §Q1). *The arithmetic is never the bug.*
- **Three material lighting modes** — unlit/prelit/lit map 1:1 to Wii `rndwii/Mat.cpp:261-304` /
  `standard_wgsl.inc:841`. `mu.unlit = (!mUseEnviron && !mPreLit)` is a genuine faithful rule (lane 04, 05 §1d).
- **Per-environ draw granularity** — the DrawMesh per-environ SceneUniforms rewrite mirrors
  `RndEnviron::sCurrent` swap (lane 04 §1a, `:3934`).
- **GX point-falloff law** — `pointFalloffMode=1` = `1/(1+d/range)` matches `GXInitLightAttn` (lane 04 §1b).
- **Faithful UI layout** — native compiles the decomp's own `ui/*.cpp` + `rndobj/Text.cpp`; no native
  reimpl of text layout/wrap/width/UIList slot math (lane 05 §exec). Song-library bugs are data, not layout.
- **`CellDiff()` non-square font override** — data-derived, scoped, Wii path untouched; the *good*
  version of asset handling (lane 05 §3).

**Sound infrastructure (keep, converge onto):**
- **`standard_wgsl.inc` + `PipelineManager`** — one file-backed, hot-reloadable main mesh shader with
  runtime `PipelineKey` variant selection (no `#define` permutation). The right model (lane 01 §3).
- **Per-instance object-uniform slot machinery** — each draw of the same mesh gets a distinct
  `UniformSlot`+bind group; the `a0f98ad` multi-draw collapse regression is genuinely fixed
  (lane 03 §Q1). `MiloXfmToColMajor` has no transpose bug.
- **Static-mesh placement path** and the **2D crowd imposter path** — both correct (lane 03).
- **Blend/zmode taken directly from `RndMat`** (1:1 with Wii constants); RB3's **painter-order** draw
  (no DC3 `distSq` re-sort) is actually *correct* for co-planar 2D UI (lane 05 §4).
- **The shared engine as one library**, real three-layer model, 16 passing CPU gtests, soft-SHA pin
  with configure-time guard, and the already-graduated `rb3_band_rnd.cpp` (lane 06 §1) — evidence the
  team refactors boundaries correctly.
- **The `gfx/` passes** (`BloomPass`, `DofPass`, `ShadowPass`, `PostProcPass`, `DrawRect2D`,
  `MeshGpuCache`, `MaterialSetup`, `TransparentQueue`) already exist on the DC3 side — these are the
  convergence targets the RB3 monolith gets extracted onto, not new code to write.

---

## Verdict

**`targeted-refactor`.** The shared core is sound and the faithful pieces are real and worth
preserving. The bug family is systemic and traces to a small set of causes — dominated by the
skinned transform/bind contract (SYS-1) and a heuristics-over-faithful culture (SYS-2) concentrated
in one 7 KLOC monolith with order-dependent state (SYS-3), plus an inverted lighting model (SYS-4),
a data-format mismatch that is not the engine's fault (SYS-5), silent stubs (SYS-6), and — the
reason none of it was caught — an absent render/vertex test net (SYS-7). None of these requires
rewriting the engine. All are fixable incrementally **once the safety nets exist**, in the order
laid out in `REFACTOR_PLAN.md`. The one hard constraint, learned from two BandPatchMesh reverts: the
nets come first.
