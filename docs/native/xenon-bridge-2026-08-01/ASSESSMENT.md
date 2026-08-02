# rb3-xenon → milo-native-engine bridge — feasibility assessment (2026-08-01)

**Question (owner):** rb3-xenon (X360 RB3 retail decomp) has made substantial
progress — can we leverage it with milo-native-engine to get the game running
natively, like the Wii path, and get the newer/better rendering engine?

**Verdict: YES — structurally EASIER than the Wii bring-up was on the render
axis, and rb3-xenon's own planning docs already charter exactly this** (its
2026-07-08 RFC `docs/plans/paths-to-100/20-native-port-and-engine-reuse.md`,
milestone M3 = "asset render via milo-native-engine with
`MILO_ENGINE_GPU_BACKEND=dc3`"). The xenon repo also carries the owner
directive in its memory: "rb3-xenon native is the real goal — matching is the
means. The Wii game is the inferior cut-down version."

Two recon lanes (this session) converged independently, and **SPIKE-X0
confirmed empirically (rb3-xenon `443070fe`,
`docs/plans/spike-x0-engine-dc3-flavor-2026-08-01.md`): COMPOSES.**

## SPIKE-X0 result (2026-08-01)

- **14/14 flavor-critical TUs compile** against xenon headers (all 6
  rndobj-coupled gfx TUs + all 8 dc3 Wgpu backends). `libmilo-engine.a`
  (4.99 MB) links clean with a **5-entry** `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`
  (vs rb3-Wii's 18; two of the five are one-line reconciliations →
  effectively 3). Zero xenon header gaps on the spike subject; the
  `-fsyntax-only` fallback was not needed (Dawn from dc3-decomp-deps +
  system glfw3 resolved directly).
- Minimal include set: `native/src`, `src`, `src/system` in that order
  (`/I src` before `/I src/system` is load-bearing); band3/network/oggvorbis
  unnecessary; no consumer STL shim injection needed (engine's
  stl_iterator.h is byte-identical to xenon's).
- **Bonus finding — latent shared-engine defect:** `Mesh_Wgpu.cpp:206,:299`
  test `GetDrawMode() == 8` against a DrawMode enum topping out at
  `kDrawVelocity = 6` (enum byte-identical in DC3) → two two-sided-cull
  overrides are dead on EVERY consumer, hidden by DC3/rb3's blanket `-w`.
  Xenon's `-Werror=` policy is a net asset to the engine. → engine backlog.
- **Recommended next milestone: X1 as a clear-frame + headless-screenshot
  smoke target** linked against the lib — NOT a straight jump to .milo
  render, because xenon's `rndobj/Rnd.h:354-360` documents an Rnd/NgRnd
  member-layout shift vs the DC3 assumption; isolate that silent-offset
  class on a known-good clear frame first.

## Why it composes

1. **rb3-xenon's `src/system/rndobj/` is DC3-shaped, not Wii-shaped.** 90
   headers, name-set IDENTICAL to dc3-decomp's; `RndMat : BaseMaterial`;
   `Rnd_NG.h` (`NgRnd : Rnd`); `PostProcMgr`/`PostProc_NG`;
   `RndCam::GetViewProjectXfms`; and the load-bearing `RndMesh::Vert` is
   byte-identical to DC3's fat layout. Every structural reason the engine's
   `rb3` (Wii) flavor exists does NOT apply to xenon → a xenon consumer uses
   the existing, most-mature `dc3` backend. **No fourth flavor.**
2. **Empirical precedent already exists:** DC3's engine loads and renders
   RB3-360 `.milo_xbox` assets today with zero rb3-xenon code
   (`crowd_female01.milo_xbox` → 57 objects, textured geometry;
   `tracksystem_meshes.milo_xbox` → 130 meshes). See
   `rb3-xenon/docs/plans/engine-reuse-and-asset-rendering.md`.
3. **Same-platform assets.** 360↔360: the BE compressed-vertex (36-byte
   XboxCVert) and 360 DXT decoders are already written and debugged in the
   engine (both flavors). rb3-Wii-native already RUNS on 360 ARK assets — the
   asset side is solved.
4. **195 of xenon's src/system .cpp are byte-identical to dc3's, including
   live `#ifdef HX_NATIVE` engine call-sites** (e.g. rndobj/Cam.cpp includes
   `platform/NativeSettings.h`, an engine header). MSVC-compat is the engine's
   native dialect (dc3 is the reference consumer).
5. **rb3-xenon/native is far along on the game-logic axis:** 15 headless
   targets (M1-M14: song mgr, MIDI, beatmatch, scoring/OD/stars, vocals,
   harmonies, crowd, save, real multi-part .ark mount with 64-bit-offset
   verification gate). Endian fix (`BinStream::ReadEndian` polarity), LP64
   `types.h`, `msvc_compat.h`, STL seam (byte-identical to the engine's) all
   in place; HX_NATIVE gating on 497 files.

## The walls (real but bounded)

1. **~85 of 86 rndobj TUs never clang-compiled.** Measured today with
   `-fsyntax-only` under xenon's own flags: rndobj 72/86 pass, char 55/60,
   ui 36/38, world 19/24, **bandobj 30/52** (weakest — and it's what a visible
   band needs: Band, BandCharacter, BandDirector, TrackPanelDir, VocalTrackDir
   fail). Two of four sampled failure causes are just missing include paths
   the engine itself supplies. Calibration: rb3-Wii was at 42/64 rndobj at the
   comparable stage and cleared it.
2. **Duplicate-definition collisions** — xenon's native/src/platform/ has ~18
   shims with the same basenames the engine compiles; each is an
   exclude-or-delete decision (`MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`). Known
   failure mode (CDReader_Native, DataParser_Native already bitten in-repo).
3. **LP64 landmines** quantified in xenon's own CMake: 455 shorten-64-to-32
   sites, pointer-in-int32 in MemTrack/PoolAlloc/MemHeap.
4. **Retail build, anonymous symbols** — xenon is a retail decomp (honest
   floor ~37.5k matched fns of 69k; per-dir: char 80.5% fn-matched, rndobj
   81.2%, bandobj 81.3%). Bodies exist at near-parity line counts with Wii/DC3
   for the whole char/ stack. But correctness-vs-metric risk is higher than
   the Wii debug build (wrong constants at 100% match; faithful-bug triage).
5. Integration quirks unique to xenon: `-Werror=` policy never run on engine
   sources (strip it from the flags handed to the engine); ScatterIncludes +
   PCH + STLport-first ordering three-way interaction.

## Milestone ladder (X0-X4)

- **X0 — SPIKE (in flight):** build `libmilo-engine.a` with GPU_BACKEND=dc3
  against xenon headers, out-of-tree harness. Answers the crux in hours.
- **X1 — context ON, GFX off:** pin + add_subdirectory in
  rb3-xenon/native/CMakeLists.txt; engine's ~25 platform TUs compile in xenon
  context; resolve duplicate shims. Gate: all 15 existing targets stay green.
- **X2 — rndobj clang-clean:** widen the fork glob (rndobj/char/world/ui),
  burn down ~48 failing TUs. Gate: full object-graph load of a real
  .milo_xbox from the mounted ark.
- **X3 — GFX ON, backend dc3:** GameRenderHook impl (template =
  rb3/native/src/rb3_render_hook.cpp — RB3's per-draw policy, not DC3's).
  Gate: render a venue/character .milo_xbox to PNG from xenon's own target.
- **X4 — band3 on the line:** bandobj/band3 TUs; every undefined symbol is a
  match-worklist item (dual yield: native correctness + X360 matching).

## Relation to the Wii-native visual-bug campaign

Orthogonal near-term, converging long-term. The Wii-native SKEL/seed-R family
(tree-branch hands, face shards, vignette pose explosions — see
`engine-arch-review-2026-07-05/execution/W34-CHARCLIP-EVAL/`) stems from
running 360 assets through Wii-era decomp + native glue; a xenon game layer is
same-era code-on-assets and may dissolve that class wholesale. But X2/X3 are
multiple sessions away — W34's dynamic clip-eval audit is the near-term lever
and its findings (e.g. a decode bug in shared char/ code) likely transfer to
xenon's char/ stack anyway.

## Sources

- Recon reports (this session, 2026-08-01): engine-consumption anatomy +
  xenon repo state. Key file anchors: milo-native-engine/CMakeLists.txt
  :91-140 (flavors) :281-329 (backend TU sets) :433-510 (injection vars);
  rb3/native/CMakeLists.txt:88-204 (pin/injection/exclusions);
  dc3-decomp/native/CMakeLists.txt:220-284; rb3-xenon/native/CMakeLists.txt
  (814 lines, no MILO_ENGINE refs); rb3-xenon report build/45410914 (41,935/
  69,366 fns, honest floor caveat in memory/project_honest_floor_2026-07-29).
- rb3-xenon/docs/plans/paths-to-100/20-native-port-and-engine-reuse.md (RFC)
- rb3-xenon/docs/plans/engine-reuse-and-asset-rendering.md (DC3-renders-RB3
  experiment)

## Post-assessment execution record (same day, 2026-08-01)

X0→X3 ALL LANDED: X1 engine link + deterministic clear frame (xenon
37b97b6e..8c63629b; also restored xenon's silently-broken native build);
X2 object-graph load of real .milo_xbox from the ark (bf03982c..3f3dc0ce,
203 fork TUs, censuses match DC3's independent loads); **X3 FIRST RENDER
(branch x3-first-render, 625b14f9/7b5a08eb): fully textured clothed
character + highway geometry from xenon's own target — output SUPERSETS
DC3's viewer on both test assets.** Coordinator E1 PASS. The predicted
Rnd/NgRnd layout hazard never materialized; all defects found were
xenon-side decomp/config fidelity gaps only a linking-and-running build
exposes (projection-matrix slot bug Cam.cpp:468, DrawMode missing
enumerator = dead shadows, TextureCompressed LP64 override, NodeCmp ODR).
Next: X4 venue + animation, per rb3-xenon/docs/plans/x3-first-render-2026-08-01.md.

## X4a — LANDED 2026-08-02, PARTIAL (xenon main `eb750e84`, coordinator E1 PASS)

Doc: rb3-xenon/docs/plans/x4a-venue-render-2026-08-02.md. Gates all green on
the landed tree (18/18 targets, zero engine edits, X3 PNGs byte-identical,
determinism ×2 with one named exception below). Per-subsystem: geometry→GPU,
materials+textures (no fallback needed), and RndPostProc VERIFIED (shipped
`intro_contrast_flame.pp` A/B: coverage 3.97%→88.47%); environ/lights
SYNTHESIZED (asset ships zero RndEnviron); transparency UNREACHED
(`QueueTransparentDraw` has zero callers — engine gap); shadows UNREACHED
(needs driven Character → X4b).

**The structural finding that recharters X4b: band3 is the critical path,
not last.** Every RB3 venue root carries 506-633 band3 factory refs (684
misses over 14 classes on the test venue), and unregistered persistent
objects CANNOT be skipped (no ReadDead framing → desync → segv). No RB3
asset with lighting avoids band3. BandCharacter/BandWardrobe are in the
missing list, so animation likely needs it too.

Also fixed en route: main was broken at lane start (DD-2's `81d23046`
ObjOwnerPtr save site — gate exists but wasn't run); `DeleteTransientObjects`
silent 11-min hang (`auto refs = obj->Refs()` copies the ring head by value →
unterminated iteration; both X4a defects were in world/Instance.cpp).
Carry-forward: grep for `auto … = …->Refs()` — mechanical to find, silent to
hit. Engine change requests filed as text in the X4a doc (incl. postproc
grain seeded from mFrameCount → nondeterministic at frames≥2); no pin bumps.
Rider A/Bs on the 2 HX_NATIVE fixes: Δ=0 but neither function is scored —
"instrument can't discriminate", ifdefs stay.

## In flight (2026-08-02)

- **X4b (dispatched after X4a close-out)** — reframed per X4a: STEP-0 =
  band3 linkage survey (how much of band3 compiles/links; the 14
  missing-factory classes first), then CharClip/CharDriver animation
  (BoneSetup-vs-xenon-CharBones, pose the T-pose), then venue-root retry if
  the 14 classes land. Same worktree/land-per-milestone discipline.
## DC3 viewer fix — DONE 2026-08-02 (coordinator E1 PASS)

Write-up: dc3-decomp/docs/investigations/2026-08-02-viewer-rb3-asset-render/.
Root cause was three DC3-side defects, none in engine rendering: (1) a
name-based `_lod` visibility override in ViewerScene.cpp:333 — RB3 crowd
characters are authored AS their lod02 asset, so the viewer deleted the whole
body (the engine's Mesh_Wgpu.cpp:136 carried an identical blanket skip;
xenon never hit it only because rb3-render independently re-issues lod-named
meshes via DrawMeshImmediate — same workaround written twice = filter in the
wrong place); (2) tracksystem meshes ship mat=(none) — geometry library,
materials come from the venue; skip was correct, viewer now applies an
announced fallback material; (3) auto-framing blew up on one asset-side
outlier vertex (Y=121458, confirmed by xenon's independent decoder) → robust
percentile framing. Adjudication anchored on Character.h's authored rule
(`mLods` groups are the authority; no groups ⇒ hide nothing by name). A
name-based sibling rule was tried, refuted on DC3's own emilia01, reverted
with evidence. Results: crowd_female01 two-hands → full clothed figure;
tracksystem blank → legible; DC3's own 35-asset sweep byte-identical; xenon
rb3-render at X3 rebuilt against the modified engine → PNGs byte-identical.
Commits: dc3 13b583df/fc40baec/3f66008e/f0275669; **engine 9898a63+138e160
(ShouldSkipMesh seam) — UNPINNED; all three consumers still pin 2ea8e343.**
Their "xenon main can't link rb3-render" flag was stale: same RndEnvAnim
break X4a already fixed on main (dce343a1). dc3 housekeeping noted: canonical
native/build/milo-viewer is a May build; rebuild before render_screenshots.sh
reflects the fix.

## X4b — LANDED 2026-08-02 (xenon main, 7 commits; doc docs/plans/x4b-animation-2026-08-02.md)

**RETRACTS X4a's headline.** The 14 classes gating every venue root contain
**zero `src/band3/` classes**: 5 `system/bandobj/`, 7 `system/synth/`, 1
`world/`, 1 `ui/`. X4a inferred ownership from the failure log rather than
from where the classes are defined — a pattern to watch for. 12/13 TUs
compile clean; `WorldCrowd` + `UIColor` were registerable immediately
(landed, −8 misses); `BandCharacter.cpp`'s 18 errors reduce to ~4 root
defects. The real blocker is **807 duplicate-definition link errors from 3
emitters — a `ScatterIncludes.cmake` dedupe gap, a build-system defect, not
unported code.** Corrected cost of "a venue root loads": one build-system
change + four defects in one file.

**Animation: pose math VERIFIED, skinning BROKEN and root-caused.** Clip
decode (44 shipped CharClips), bone evaluation (39/39 bones, determinants
1.000) and world compose all pass the bone-length oracle at **max ratio
0.9999 (deviation 7.47e-05)**. But coordinator E1 on the posed PNG shows the
character SMEARED into vertical spikes — matching the lane's own honest
"skinning palette ❌ BROKEN" verdict, not contradicting it.

**Root cause (verified independently by me at rndobj/Mesh.h:227):**
`MaxBones() = GetGfxMode() != kOldGfx ? 40 : 4`, and `RndMesh::Load`
enforces it *destructively* (`mBones.resize(MaxBones())`, Mesh.cpp:567-578).
`gGfxMode` is a zero-init global (kOldGfx) and the only thing that sets
kNewGfx is `PreInitSystem` (os/System.cpp:505) — which the hand-rolled
native driver never calls. So every skinned mesh was truncated 20→4 bones at
load; vertices weighted to bones 4..19 stay pinned at bind coordinates while
bones 0..3 animate. That is exactly the "clean at bind, smears with pose
deviation" signature. X3 recorded the warning and left the "4" unexplained;
X4a carried it forward; X4b explained it. **Fix built, measured, and
deliberately NOT landed** (main_render.cpp:1419-1467): kNewGfx has 22
consumers, and while bind-pose coverage improves 11.07%→15.78% (confirming
the diagnosis), the posed frame goes to 0.00% — geometry leaves the camera.
Correctly handed to X4c rather than trading a smear for nothing.

★ **The finding that should drive X4c: the same root-cause shape bit twice
in one milestone.** `TrigTableInit` (so `Sine`/`Cosine` returned 0.0 across
all 17 native targets) and `SetGfxMode` are both `PreInitSystem`/`SystemInit`
sub-inits the hand-rolled bring-up skipped — silent, and latent for four
milestones. **~10 such sub-inits exist; 2 have bitten, 8 are unaudited.**
Renders looked plausible while six bones were singular and world determinants
had reached -3.9e14, so X4c must add boot-time invariants and pick oracles
*before* screenshots. Cross-repo check (done): rb3-Wii is NOT exposed — it
calls TrigTableInit via the real `System.cpp` path, and its `MAX_BONES` is
an unconditional 40. The defect is specific to xenon's hand-rolled bring-up.

Also fixed: `Multiply(Transform,Transform,Transform)` was alias-unsafe **both
ways** — the identical hazard class as rb3-Wii's W34 hands fix, found because
it was flagged in the charter. Rider landed: engine pin → `138e1606`, all
four X3/X4a PNGs byte-identical, `_lod` re-issue retired on a discriminating
A/B. Gate PASS 18/18 fresh on the rebased tree, zero engine edits.

## In flight (2026-08-02, cont.)

- **X4c** — audit the ~8 remaining PreInitSystem/SystemInit sub-inits; make
  the native driver use the real init path instead of hand-rolled stand-up;
  land kNewGfx and find what it breaks in the posed draw (22 consumers);
  boot-time invariant checks before any screenshot claim.
- **band3/venue-unblock roadmap review** — rb3-xenon docs lane; corrected
  mid-flight with X4b's retraction of the band3 premise.
- **Pin currency:** xenon now at 138e1606. rb3-Wii + dc3 still 2ea8e343;
  functionally unaffected (dc3-flavor file) — bump when a Wii-relevant
  engine change next lands.
