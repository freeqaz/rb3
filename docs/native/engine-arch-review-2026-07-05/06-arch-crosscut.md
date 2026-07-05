# Lane 6 — Arch Crosscut: boundaries, hack economy, safety nets

**Reviewer:** Opus (arch-crosscut lane). Read-only. Evidence is `file:line`; each claim tagged
**[M]** MEASURED (I read the code/data) or **[H]** HYPOTHESIS.

Engine SHA reviewed: `a8089c3d9db9b467c31e10a118e584415b2a50ac` (matches `MILO_ENGINE_PIN` in
`rb3/native/CMakeLists.txt:74`). **[M]**

---

## Executive summary

**Verdict: REFACTOR, with one scoped OVERHAUL.** The engine is *not* a poorly-constructed
codebase in the general sense. The layering model is real and mostly honored: a genuinely
game-agnostic gfx/asset/audio/char core, a documented three-layer source model
(`milo-native-engine/CLAUDE.md`, `README.md`), a working CPU test suite (16 gtests, 8029 lines),
and a soft-SHA engine pin with a configure-time mismatch guard. The DC3/RB3 split is managed as a
**configure-time GPU-backend flavor** (`MILO_ENGINE_GPU_BACKEND={off,dc3,rb3}`), not two forks —
the shared core is one library.

But three crosscutting systems are structurally unsafe and are the common cause behind the
bug list:

1. **The RB3 render backend is a 7,017-line god-file** (`Rnd_Wgpu_RB3.cpp`, 383 KB, 113 `getenv`
   calls) that concentrates ~30 runtime render flags, inline shader strings, and the entire
   default-ON "hack economy." This is the overhaul target. **[M]**
2. **832 weak link-stubs resolve to a single silent `xorl %eax; ret` no-op with zero logging.**
   This is the exact mechanism that produced the particles (`DrawParticlesBillboard`) and
   NetSession (`EndGame`) invisible-failure bugs. The safety property ("none is reached") is a
   hand-maintained code comment, not an enforced invariant. **[M]**
3. **There is no render-correctness test in the engine.** All 16 gtests are CPU data-path tests;
   `test_bone_ground_truth.cpp` validates bone *topology/symmetry* but never a skinned vertex
   position. Render correctness rests entirely on rb3-side Python visual gates that memory records
   as **blind** (band-closeup gate PASSed 34/34 on exploded patch-shard frames; BandPatchMesh
   faithful rewrites broke twice undetected). **[M]**

The bug list (hands/fingers deform, crowd/drum-kit co-located, lighting approximation, BandPatchMesh
double-revert, count-in shards) is what you get when a monolithic render/skin path with a large
default-ON workaround layer has no per-draw / per-vertex regression net. **The sequencing
constraint is firm: the safety nets (loud stubs, CPU reference skinner + per-vertex golden,
per-draw golden-image lineup, flag registry) MUST exist before the mesh/lighting rewrites are
re-attempted** — otherwise the third BandPatchMesh revert is already scheduled.

---

## 1. Boundary map

### 1.1 Where things live (MEASURED)

| Layer | Path | Compiles under | On native link line | Size |
|---|---|---|---|---|
| Matched fork (asm-match only) | `rb3/src/system/**`, `rb3/src/band3/**` | MWCC | **No** | — |
| Engine runtime (the deliverable) | `milo-native-engine/src/**` | Clang LP64 | Yes | see below |
| Per-decomp glue | `rb3/native/src/rb3_*.cpp` + `*.s` | Clang LP64 | Yes | 17,423 lines / 51 cpp |

Three-layer model documented at `milo-native-engine/CLAUDE.md` (three-layer table) and
`README.md`. **[M]** The model is coherent and the docs are honest about drift ("separate copies
that drift").

### 1.2 Is the engine actually shared, or two forks in one repo? (MEASURED)

**It is shared, with a per-game *renderer backend flavor* selected at configure time — not two
forks.** `milo-native-engine/CMakeLists.txt:445` builds a single `add_library(milo-engine STATIC …)`.
The DC3/RB3 divergence is a cache STRING `MILO_ENGINE_GPU_BACKEND ∈ {off,dc3,rb3}`
(`CMakeLists.txt:89-134`) that swaps *mutually exclusive* source lists:

- `dc3` flavor → `MILO_ENGINE_GPU_PLATFORM_SOURCES` (`CMakeLists.txt:303-312`): `Rnd_Wgpu.cpp`
  (1,861 lines), `Mesh_Wgpu.cpp`, `Part_Wgpu.cpp`, `MaterialSetup.cpp`, `MeshGpuCache.cpp`, … —
  `WgpuRnd : NgRnd`, reads DC3's 2012-era rndobj.
- `rb3` flavor → `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` (`CMakeLists.txt:321-323`):
  `Rnd_Wgpu_RB3.cpp` + `RB3TexSharpen.cpp` — `BandRnd : Rnd`, reads RB3's older 2010-era rndobj
  via consumer-injected context.

So the genuinely-**shared** surface is the gfx/ core (`GpuDevice`, `PipelineManager`, `BloomPass`,
`DofPass`, `ShadowPass`, `PostProcPass`, `Screenshot`, `TextureConvert`) plus the data/asset
(`DataParser_Native`, `CDReader`, `File_*`), audio (`FFmpeg*`, `miniaudio`), and char/skeleton
(`BoneSetup.cpp` 605 lines, `Skeleton_Native.cpp`). The *draw path itself is NOT shared* between
DC3 and RB3 — each game has its own renderer TU. This is defensible (the rndobj eras differ), but
it means "the engine renders RB3" reduces to "one 7 KLOC file renders RB3." **[M]**

### 1.3 Where the boundary is wrong (MEASURED + HYPOTHESIS)

- **RB3-specific code inside the "shared" engine.** `src/platform/Rnd_Wgpu_RB3.cpp` (7,017),
  `Rnd_Wgpu_RB3.h` (461), `RB3TexSharpen.cpp` (415), `RB3TexSharpen.h`, `RB3TexSharpenDebug.h`.
  **[M]** These are game-named files in a repo whose `CLAUDE.md` states "no per-game classes."
  They compile only under the `rb3` flavor, so this is a *tolerated* boundary violation (a
  per-game renderer must name game rndobj types somewhere). The problem is not that they exist —
  it's that the RB3 one is a monolith and carries the hack economy (§3).
- **Render code leaking into game glue — mostly a false alarm now.** `rb3_band_rnd.cpp` is **0
  lines** (`rb3/native/src/rb3_band_rnd.cpp`) — it graduated into the engine (CMake note:
  "BandRnd : Rnd, graduated from rb3/native/"). **[M]** `rb3_render_mesh.cpp` (583) and
  `rb3_render_tri.cpp` (371) are **debug milestone drivers** gated by `RB3_RENDER_MESH=1` /
  `RB3_RENDER_TRI=1` (`rb3_render_mesh.cpp:10`), not the production draw path. **[M]** So the
  "rendering in game glue" smell has largely been cleaned up already — a *positive* signal about
  refactor capacity.
- **A permanently-dead seam.** `rb3_render_hook.cpp` implements the engine's `GameRenderHook`
  (`DrawGameOverlay`/`RenderCharacterImpostors`) but both are no-ops, and its own header comment
  says the hook belongs to DC3's `Rnd_Wgpu.cpp` which "is NOT linked into rb3-native"
  (`rb3_render_hook.cpp:14-19`). **[M]** The RB3 renderer (`Rnd_Wgpu_RB3.cpp`) does its overlay/RTT
  work inline instead. Net: a cross-layer seam that is architecturally the *right* shape but is a
  permanent no-op on RB3 — dead scaffolding that will mislead a future reader. **[H]** the doc
  comment is also stale (it implies RB3 links no WebGPU renderer at all, contradicting the `rb3`
  flavor that links `Rnd_Wgpu_RB3.cpp`).

### 1.4 DC3/RB3 divergence management + the masking hazard (MEASURED)

- DC3 consumer keeps **105 `.cpp` under `dc3-decomp/native/src`** with subdirs that *mirror the
  engine* (`audio/`, `char/`, `gfx/`, `platform/`, `stl/`, `export/`, `viewer/`, `render_test/`).
  **[M]** This is the structure behind the documented 2026-06-06 masking incident (stale engine
  duplicates shadowed the shared engine; `AudioDevice.h`). The parallel-copy shape *persists*, so
  the masking hazard is latent, not eliminated. The divergence is managed by convention (glue = SDK
  shims only) rather than by a structural guard that would *prevent* a `native/src/**` file from
  redefining an engine symbol. **[H]** a link-time duplicate-symbol audit in CI would close this.

---

## 2. Weak-stub linkage audit

### 2.1 Census (MEASURED)

| File | Lines | Weak syms | Shape |
|---|---|---|---|
| `rb3/native/src/band3_link_stubs.s` | 1,350 | 582 | 521 FUNC → shared no-op; 61 DATA → `.bss` reservation |
| `rb3/native/src/dta_link_stubs.s` | 431 | (part of 832) | fn + data |
| `rb3/native/src/rndobj_synth_link_stubs.s` | 145 | (part of 832) | Bink/vorbis/tomcrypt/GX off-path |
| `rb3/native/src/web_data_stubs.cpp` | 88 | ~24 | weak 256B/4096B blobs for manager singletons |

**832 weak symbols total** across the `.s` files (`grep -c '.weak'` summed). **[M]** The function
stub body is:
```
__hmx_band3_noop_stub:
    xorl %eax, %eax
    ret
```
(`band3_link_stubs.s:19-21`). **Silent. Returns 0. No log, no counter, no trap.** **[M]** DATA
stubs resolve to a zeroed `.bss` blob (read/write-safe but semantically empty). `web_data_stubs.cpp`
mirrors the data half with `__attribute__((weak, aligned(16))) char TheNetSession[256] = {}` etc.,
added *after* an ASan null-write bug (its header comment documents `gInitComplete = false` writing
to address 0 on web). **[M]**

### 2.2 The invisible-failure mechanism (MEASURED)

The `.s` header comment claims safety by assertion: *"None is on the matched-fork asm-match line …
never reached offline; noop"* (`band3_link_stubs.s:1-14, 24-28`). **[M]** This is a hand-maintained
belief, not an invariant. The exact failure mode has already shipped twice:

- **particles** — `DrawParticlesBillboard` was a weak no-op stub; particles silently never rendered
  (memory: A1 hit-flame diagnosis — "no-op weak stub on RB3 BandRnd").
- **NetSession** — `EndGame` weak-stub meant `GameEndedMsg` never fired; "song never ends"
  (memory: song-end native fix). The comment block in `band3_link_stubs.s:24-37` is *itself* a scar
  from this — it documents having had to promote NetSession virtuals from weak stub to strong def
  because a zeroed `_ZTI10NetSession` crashed a `dynamic_cast`.

A stub that is genuinely unreachable and a stub that is silently swallowing a live call are
**byte-identical at runtime** under this design. Every one of the 521 function stubs is a
potential dormant version of the particles bug.

### 2.3 Proposed loud-by-default policy (RECOMMENDATION)

1. **Log-once on first hit.** Replace the shared no-op with a tiny C shim
   `__hmx_stub_hit(const char* name)` that atomically log-once-per-symbol to stderr
   (`[STUB] first call to <demangled>`), then returns 0. Cost: one `.text` trampoline per symbol
   (or pass the name in a register). This alone would have surfaced particles + NetSession the
   first frame they were exercised.
2. **Startup census dump.** Emit, behind a default-ON `MILO_STUB_CENSUS` (loud unless silenced),
   the count + list of weak stubs that *actually resolved to the no-op* (i.e., no strong def won).
   A generator already knows the full set; diff "linked stubs" vs "hit stubs" at exit.
3. **Stub registry as data, not a comment.** Machine-checkable manifest (symbol → category →
   "assert-unreachable | ok-noop | data-blob") checked in CI: any symbol newly resolving to a
   no-op that isn't classified fails the build. This turns §2.2's prose assertion into a gate.

---

## 3. Hack economy

### 3.1 Flag inventory (MEASURED)

- **Engine (`milo-native-engine/src`): 140 distinct `getenv` flags.** **[M]**
- **Glue (`rb3/native/src`): 89 distinct `getenv` flags.** **[M]**
- **~229 runtime env flags total.** `Rnd_Wgpu_RB3.cpp` alone holds **113 `getenv` calls** (PLAN.md
  smell, confirmed). TODO/HACK/FIXME markers: 35 in engine, 9 in glue. **[M]**

Three classes (by naming + read of gating sites):

**(a) Debug probes — one-shot investigation scaffolding, never removed.** e.g. `BONE_PROBE`,
`BONE_PROBE_NAME`, `SHARD_DBG`, `SHARD_BONE_DBG`, `CHAIN_PROBE/MTX/FORCE/COMPOSE`, `XBONE`,
`XBONE_TRACK`, `C8_PROBE`, `C8_EVERY`, `SKIN_PROBE`, `SKEW_PROBE`, `VERT_PROBE`, `SLOT_PROBE`,
`GEM_VTX`, `SMASH_DBG`, `HUB_BAR_PROBE`, `IK_SHARD_VERT`. **[M]** These are archaeological — each
is a frozen record of a past bug hunt (skinning shards, C8 faces, chain sim). Dozens of them.

**(b) Shipped default-ON workarounds — the load-bearing hacks.** Gate pattern confirmed at
`Rnd_Wgpu_RB3.cpp:6157`: `sTrackLight = getenv("RB3_TRACK_LIGHT_OFF") ? 0 : 1;` — **default ON,
env only disables.** Same shape for `RB3_VENUE_LIGHT_OFF` (`:1196`), `RB3_HIGHWAY_BLOOM_OFF`
(`:2278`), `RB3_SCROLLBAR_THUMB_FIX_OFF` (`:4538`). **[M]** Full default-ON set (flag name = the
*opt-out*, so the workaround ships):

  Engine (21): `RB3_TRACK_LIGHT_OFF`, `RB3_VENUE_LIGHT_OFF`, `RB3_BLOOM_OFF`,
  `RB3_HIGHWAY_BLOOM_OFF`, `RB3_HIGHWAY_WATERMARK_OFF`, `RB3_CHAR_REAL_LIGHT_OFF`,
  `RB3_CROWD_DIM_OFF`, `RB3_COMPOSE_MULT_OFF`, `RB3_FRET_GLOW_OFF`, `RB3_PART_HAZE_OFF`,
  `RB3_PART_MATCOLOR_OFF`, `RB3_PART_NEARFADE_OFF`, `RB3_NOISE_OFF`, `RB3_PP_OFF`, `RB3_RTT_OFF`,
  `RB3_BC_TEX_OFF`, `RB3_SCREENMASK_FALLBACK_OFF`, `RB3_SCROLLBAR_THUMB_FIX_OFF`,
  `RB3_UNPACK_CACHE_OFF`, `RB3_PIPELINE_PREWARM_OFF`, `SHARD_GUARD_OFF`. Plus the skinning
  workaround family `RB3_NO_SKEL_REBAKE`, `RB3_NO_SKEL_WORLDFIX`, `RB3_NO_SKIN_CLAMP`,
  `RB3_NO_STEM_ANCHOR`, `RB3_NO_MESH_CACHE`, `RB3_NO_PRECLEAR`, and hub-layout fixes
  `RB3_NO_HUB_BAR_PLACEMENT_FIX`, `RB3_NO_HUB_HIGHLIGHT_FIX`, `RB3_NO_HUB_BAR_SHARD_EXEMPT`. **[M]**

  Glue (17): `RB3_GAMEWARM_OFF`, `RB3_HEAP_TRIM_OFF`, `RB3_MOGG_RANGE_OFF`,
  `RB3_MOGG_READAHEAD_OFF`, `RB3_SCREEN_BUNDLES_OFF`, `RB3_SFX_CACHE_OFF`, `RB3_SFX_OGG_OFF`,
  `RB3_TEX_PREWARM_OFF`, `RB3_XMA_PREFETCH_OFF`, `RB3_PREVIEW_PREFETCH_OFF`, `RB3_ASYNC_OPEN_OFF`,
  `RB3_BOOT_BUNDLE_OFF`, `RB3_CROWD_IMPOSTER_OFF`, `RB3_NO_SFX`, `RB3_NO_SETLIST_FIX`,
  `RB3_NO_GUEST_PROFILE`, `RB3_NO_AV_CALIBRATION`. **[M]**

**(c) Baked heuristics with no env at all.** `UiRenderHeuristics.h:5` —
`NativeShouldForceTextAlpha(isTextMesh, blend==kBlendSrcAlpha, alpha<0.01f)` — a named "heuristic"
with a magic `0.01` threshold hard-wired into the text draw path. **[M]** The *file existing* (per
PLAN.md) is itself the smell.

### 3.2 Quantified: how much shipped behavior is default-ON workaround? (MEASURED lower bound)

At least **~38 default-ON workaround flags** ship as the live rendering/perf behavior (21 engine +
17 glue), on top of the baked heuristics with no flag. The *entire* synthetic lighting stack the
bug list complains about — track lighting, venue lighting, char real-light, crowd dim, highway
bloom, fret glow, compose-mult — is default-ON-workaround-gated. **The lighting is not "fragile
because it's hard"; it's fragile because it is a stack of ~10 independently-toggled approximations
with no combined regression gate.** **[M/H]**

### 3.3 Proposed quarantine + burn-down (RECOMMENDATION)

1. **One compat layer, not 229 scattered `getenv`s.** A single `NativeCompat` module owning a typed
   registry: `{name, default, class ∈ [probe|workaround|feature|perf], owner, tracking-doc-anchor,
   faithful-alternative-status}`. Reads happen once at startup into a struct; call sites read the
   struct, not `getenv`. Kills the 113-`getenv`-in-one-file pattern.
2. **A tracking doc that is generated from the registry**, not hand-written — every default-ON
   workaround gets a row with "why faithful path is not yet live." This is the `NATIVE_HACK_AUDIT`
   made continuous instead of a point-in-time census (the 2026-06-08 audit was 73 hacks; the flag
   count says it has grown).
3. **Burn-down rule:** a workaround flag may be added only with (a) a linked tracking-doc entry and
   (b) a test asserting the *faithful* path is the target. Debug probes (`class=probe`) must be
   `#ifdef`-compiled-out of release builds — they should not be `getenv`-live in shipping binaries
   at all. Net goal: probe flags → 0 in release; workaround flags monotonically decreasing with each
   faithful fix.

---

## 4. Test & gate posture

### 4.1 What exists (MEASURED)

16 engine gtests, 8,029 lines (`milo-native-engine/tests/`). **All CPU data-path**: asset loading
(1,902), object lifetime (1,224), bone ground truth (1,005), mogg decode (473), chunk/bin stream,
charbones serialization, charclipgroup, dirloader, mesh loading (loads + counts verts — not pixels),
rndcam projection. **[M]** `test_bone_ground_truth.cpp` asserts bone **existence, hierarchy,
child-count, rest-pose non-identity, L/R symmetry, limb-distance sanity** (`:115-225`) — it never
asserts a **skinned vertex output** against a reference. **[M]**

**No golden-image / SSIM / pixel-diff / per-draw-state test exists in the engine**
(`grep -liE 'golden|ssim|pixel.?diff|screenshot.?compar|render.?regress' tests/` → empty). **[M]**

Render correctness lives entirely in rb3-side Python harnesses (`visual_diff.py`,
`scripts/native/*-capture.py`, band-closeup gate). Memory records these as **blind**: the
band-closeup drop/ratio gate PASSed 34/34 on exploded patch-shard frames, and the BandPatchMesh
faithful rewrites broke rendering **twice** and were only caught by human review, not the gate
(memory: `feedback_decomp_sweep_native_visual_gate`). **[M]**

### 4.2 The structural gap (why the bug list is invisible to the tests)

Every bug on the list is a **per-draw or per-vertex geometry/state defect**:
hands/fingers deform (skinning output), crowd/drum-kit co-located (per-instance transform),
count-in shards (skinning under a specific pose), BandPatchMesh (patch expansion → vert buffer).
The test suite validates the *inputs* (bones load, topology sane, mesh vert count) and the Python
gates validate a *downsampled full-frame image* — neither observes the actual skinned vertex
positions or the per-draw GPU state. There is a hole exactly the size of every bug. **[M/H]**

### 4.3 What would catch these (RECOMMENDATION, ordered by leverage)

1. **CPU reference skinner + per-vertex golden.** A standalone LP64 function that applies
   `boneMtx * vert` for the documented skinning model (the decomp semantics), run in gtest against
   a real char asset, asserting output positions within epsilon of a committed golden. This directly
   nets hands/fingers + count-in shards, and — critically — it gives BandPatchMesh a *faithful
   oracle*: the next rewrite is graded against reference vertices, not eyeballed. This is the single
   highest-value net.
2. **Per-draw state assertion capture.** Have `Rnd_Wgpu_RB3` (once decomposed) emit a structured
   per-draw record (pipeline id, blend, bind-group handles, world xfm, vert/index counts) to a
   ring; a gtest replays a canned scene and diffs the draw log against a golden. Catches
   co-location (identical world xfm across instances that should differ) and the "uniform collapse"
   class memory already hit (mesh-cache multi-draw uniform collapse).
3. **Golden-image lineup gate that is NOT blind.** Fix the band-closeup blindness by (a) a
   *patch-bearing* lineup (BandPatchMesh chars in frame), (b) *wide reviewer-judged* frames, (c)
   per-draw-count + per-mesh-bbox assertions layered under the image compare so a shard explosion
   fails a numeric check even when SSIM passes. Memory prescribes exactly this
   ("re-land needs patch-bearing lineup + reviewer-judged wide frames").
4. **Bone ground-truth expansion:** extend `test_bone_ground_truth.cpp` from topology to *live pose*
   — apply a clip, assert effector world positions (hand, foot, drum-stick tip) against golden. This
   is the placement-bug net (drum kit, band member locations).

**Sequencing:** (1) and (2) are prerequisites for touching the skinning/mesh path; (3) and (4)
gate the lighting/placement work.

---

## 5. Verdict + shape of the target

### Verdict: **REFACTOR** (sound core; one scoped overhaul + a safety-net buildout)

Not `overhaul` — the layering is real, the shared core is clean, the tests exist, `rb3_band_rnd.cpp`
already graduated into the engine (proof the team can move a boundary correctly), and DC3/RB3 is a
managed backend flavor, not a fork. Not `sound` either — three crosscutting systems (the RB3
render monolith, the silent stub layer, the missing render test net) are actively generating the
bug list. The right disposition is a targeted overhaul of the RB3 render backend riding on top of a
safety-net buildout, with the rest of the engine refactored incrementally.

### Target architecture (one page)

**Layers (unchanged model, enforced):** matched-fork (asm only) → shared engine runtime →
per-decomp glue (SDK shims only). Add one *enforced* rule the current codebase only states as prose:
**no `native/src/**` file may define a symbol the engine defines** (CI duplicate-symbol audit),
closing the DC3 masking hazard structurally.

**Render backend:** decompose `Rnd_Wgpu_RB3.cpp` (7 KLOC) into cohesive units mirroring the passes
already factored on the DC3 side (`gfx/BloomPass`, `DofPass`, `ShadowPass`, `PostProcPass` exist —
RB3 should reuse them, not re-inline): `BandRnd` core (draw dispatch), `BandMaterial` (blend/emissive
resolve), `BandSkin` (skinning), `BandLighting` (the environ/track lighting, today's default-ON
stack), `BandCompose` (RTT/skin composite). Inline shader strings → `.wgsl` files (there are
currently **zero** `.wgsl`; all shaders are C++ string literals — PLAN.md, confirmed by absence).

**Faithful-first policy (the core cultural fix):** the engine implements the *decomp semantics*;
every divergence is (a) quarantined behind the single `NativeCompat` registry, (b) documented with
a faithful-alternative status, and (c) covered by a test that asserts the faithful target. A
default-ON workaround is a *tracked debt with a burn-down owner*, not a permanent fixture. Debug
probes are compiled out of release.

**Safety nets as first-class engine code:** loud stubs (log-once + census), CPU reference skinner +
per-vertex golden, per-draw state log + golden, patch-bearing lineup gate. These live in
`milo-native-engine/tests/` and run in CI, not in ad-hoc rb3 Python scripts.

### Sequencing constraint graph (what must exist before mesh/lighting rewrites land safely)

```
[loud stubs + startup census] ─────────────┐
                                            ├─▶ safe to touch skinning / BandPatchMesh
[CPU reference skinner + per-vertex golden]─┤       (3rd revert avoided)
[per-draw state log + golden]───────────────┘
                                            └─▶ safe to touch placement (crowd/drum/band)
[NativeCompat flag registry + tracking doc]─────▶ safe to burn down default-ON lighting hacks
[patch-bearing + wide reviewer lineup gate]─────▶ safe to re-attempt faithful BandPatchMesh
                                                   AND real lighting
[Rnd_Wgpu_RB3.cpp decomposition]────────────────▶ everything above is tractable to apply per-unit
```

**Hard ordering:** you cannot safely re-attempt the faithful BandPatchMesh rewrite (which failed
twice) until the CPU reference skinner + per-vertex golden + non-blind patch-bearing gate all
exist. Attempting it before is the definition of scheduling the third revert. Lighting rewrites
require the flag registry (to know which of ~10 default-ON approximations you are replacing) plus
the per-draw golden (to prove you didn't collapse a bind group). Decomposing the render monolith is
the enabling substrate but is itself safe to do incrementally *because* it is behavior-preserving
motion — do it in parallel with net-building, land the rewrites after both.

---

## Appendix — key evidence index

- Engine single lib + backend flavor: `milo-native-engine/CMakeLists.txt:89-134, 303-323, 445`
- RB3 render monolith: `src/platform/Rnd_Wgpu_RB3.cpp` 7,017 lines, 113 `getenv`
- Default-ON gate pattern: `Rnd_Wgpu_RB3.cpp:1196, 2278, 4538, 6157`
- Weak no-op stub body: `rb3/native/src/band3_link_stubs.s:19-21`; 832 weak syms across 3 `.s`
- Stub scar (NetSession promotion): `band3_link_stubs.s:24-37`
- Data-blob stubs: `rb3/native/src/web_data_stubs.cpp:1-25`
- Dead seam: `rb3/native/src/rb3_render_hook.cpp:14-19, 33-45`
- Graduated boundary (positive): `rb3/native/src/rb3_band_rnd.cpp` = 0 lines
- Baked heuristic: `src/platform/UiRenderHeuristics.h:5`
- Bone test = topology only: `tests/test_bone_ground_truth.cpp:115-225`
- No render test: `grep -liE 'golden|ssim|pixel.?diff' tests/` → empty
- DC3 parallel-copy masking hazard: `dc3-decomp/native/src` = 105 cpp mirroring engine dirs
- Three-layer model docs: `milo-native-engine/CLAUDE.md`, `README.md`
