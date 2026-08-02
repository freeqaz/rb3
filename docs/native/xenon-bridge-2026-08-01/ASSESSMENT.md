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
- **DC3 viewer partial-render fix** (owner-requested) — dc3-decomp lane:
  root-cause why milo-viewer draws only 2 disembodied hands of
  crowd_female01 (vs xenon full figure) and blank tracksystem; adjudicate
  correct viewer behavior; fix DC3-side if possible; engine edits
  conservative (3 consumers, verify xenon X3 PNGs unchanged, no pin bumps).
