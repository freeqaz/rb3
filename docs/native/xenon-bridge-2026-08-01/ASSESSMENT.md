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

Two recon lanes (this session) converged independently; SPIKE-X0 (empirical
compile of the engine's dc3 flavor against xenon headers) dispatched — verdict
to be appended below / in `rb3-xenon/docs/plans/spike-x0-engine-dc3-flavor-2026-08-01.md`.

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
