# rb3-viewer + white-wig hair fix — implementation plan (2026-07-02)

Synthesized (Fable) from the three scout docs in this directory:
`scout-dc3-viewer.md` (DC3 milo-viewer anatomy), `scout-rb3-infra.md` (rb3
infra + proto-viewer), `scout-wig-bug.md` (root cause, asm-proven).

## Context

- **User goal:** a standalone asset renderer (like DC3's milo-viewer) to load
  individual .milo assets against our engine + BandRnd WGPU backend, so
  rendering bugs can be debugged without booting the game. First customer: the
  "white wig / long hair" character artifact.
- **Wig bug root cause (CONFIRMED by target asm + DC3 + source read):**
  `CharHair::SimulateInternal` (`src/system/char/CharHair.cpp:511`) nests the
  per-point bone-update tail (lines 669–686: `Scale → Cross → Normalize →
  bone->SetWorldXfm → force/friction/inertia → t100.v = pos`) inside
  `if (thisPoint.collides.size() != 0)` (opens :582, closes :687). The Wii
  target's empty-collides `beq` lands AT the tail (`.L_806D002C`) — the
  original runs it for EVERY point; only the collision loop is conditional.
  DC3 (`dc3-decomp/src/system/char/CharHair.cpp:310–381`) closes the `if`
  right after the collision `for`. Collide-less strand points collapse →
  strands drape over the face → "white wig". objdiff shows a deceptive 99.6%
  with `beq 0x346c vs 0x4ad8` (a real CFG bug hiding as diff_arg noise).
- **Proto-viewer already exists:** `RB3_RENDER_MESH=1` mode
  (`native/src/rb3_render_mesh.cpp`, exports via `rb3_render_mesh.h`) —
  boot + headless BandRnd + DirLoader + robust auto-frame cam + mesh walk +
  PNG. Verified rendering `female_hair_long_resource.milo_xbox` today
  (baseline PNG next to scout-rb3-infra.md).

## Lanes

### Lane H — CharHair::SimulateInternal brace fix (Opus)

Move the `if (collides.size() != 0)` closing brace from :687 to immediately
after the collision `for` loop (:667/668), leaving the tail unconditional.
Mirror DC3 exactly (note: RB3-2010 tail legitimately lacks DC3's later wind
block). Keep collision-loop contents untouched.

Gates (this is shared Wii+native anim code —
[[feedback_decomp_sweep_native_visual_gate]] applies):
1. `mcp run_objdiff SimulateInternal__8CharHairFf` — expect 99.6% → ~100%
   (the beq target + the two `lfs 0x50/0x54` deletes at [259–262] are in this
   region). Must NOT regress below 99.6%.
2. Native visual gate runs in the Land stage (Lane G) — do NOT commit in this
   lane; write a handoff doc instead.
Constraints: Wii build ONLY via `tools/ninja-locked` (run_objdiff does this);
do NOT build native (Lane V owns the native build dir concurrently).

### Lane V — rb3-viewer (Opus)

Per scout-rb3-infra.md **Option A**: an `RB3_VIEWER=1`-style mode compiled
into rb3-native, but with a real argv CLI. New TU `native/src/rb3_viewer.cpp`
+ dispatch branch in `main_native.cpp` (next to RB3_RENDER_MESH). Reuse
`rb3_render_mesh.h` exports (`LoadMiloAndWalk`/`RenderFrame`/`RenderToPng`,
`SynthesizeCamera` bounds logic) — extend, don't fork.

v1 features (wig-focused subset of DC3's viewer):
1. Init spine exactly as scout-rb3-infra.md §6 (InitGpu BEFORE chdir;
   kPlatformXBox; band_preinit_keep/band_keep; InjectTypeDefStubs;
   PreInitRender; RB3RegisterGameObjectFactories).
2. **Char factory set** (§3.4): `CharHair::Init()`, `OutfitConfig::Init()`
   (news sMat/sCam — call Init not Register), `RndAmbientOcclusion::Init()`,
   + the `test_charload5b.cpp` RegisterCharLoadFactories list (RndMeshDeform,
   RndTexBlender, RndTexBlendController, CharClipSet, CharClip, CharCollide,
   CharLipSync, CharInterest, CharFaceServo, CharWeightSetter, CharServoBone,
   BandFaceDeform). Do NOT register `Character` (CharacterTest→overlay trap;
   root defaults to RndDir — proven fine) and do NOT call wholesale
   `CharInit()`/`BandInit()`.
3. CLI: `rb3-native --viewer <milo-rel-path> [--out out.png] [--frames N]
   [--sim N] [--subdir <milo>]... [--hide substr]... [--only-showing]
   [--azimuth d --elevation d --distance u | --cam-dir x,y,z] [--width/--height]
   [--list] [--verbose]`. `--list` prints a dir census (class/name per object)
   and exits.
4. **`--sim N`**: after load+SyncObjects, N settle steps driving
   `TheTaskMgr.SetSecondsAndBeat(t, beat, false)` at 30 fps and calling
   `Poll()` on every `ObjDirItr<CharHair>` (and `RndDir::Poll` if cheap/safe).
   This is the wig-repro feature: pre-H-fix long hair collapses, post-fix it
   holds shape. (No Character factory needed — poll hair objects directly.)
5. `--subdir`: pre-load shared-dep milos (e.g. `char/main/shared/gen/
   char_shared.milo`) so cross-milo tex refs resolve; then load subject.
6. Wrapper `scripts/native/render-asset.py`: thin — builds target if stale,
   runs headless, prints PNG path + census summary. Keep it simple.
7. Doc: `docs/native/asset-viewer-2026-07-02/VIEWER.md` — usage + examples.

Acceptance (from scout §6):
- Renders `char/main/hair/female/gen/female_hair_long_resource.milo_xbox` to
  PNG with ZERO "Can't make" NOTIFYs.
- Renders `char/main/hair/male/gen/male_hair_crazyhawk_resource.milo_xbox`
  static + with `--sim 30`.
- Static render visually consistent with the scout baseline
  (`probe-female-hair-long-render-mesh.png`) — added factories must not
  regress the draw.
- Commit ONLY your own new/changed files (viewer TU, main_native dispatch,
  script, doc). Never `git add -A`. rb3-native build must stay green.

Known traps checklist: scout-rb3-infra.md §6 "Known traps" 1–11 (esp. GPU
init before chdir; `_exit` after PNG; missing-Dir-factory = SIGSEGV means
factory gap; MILO_TRY broken on LP64).

### Lane G — land + visual gates (Opus, after H and V)

1. Rebuild rb3-native (now contains H's fix + V's viewer).
2. Viewer A/B: render crazyhawk + ziggymullet with `--sim 30`;
   `RB3_MESH_FREE=1` not needed; compare against Lane-V's pre-fix captures if
   available (V ran before H landed in the build? — if not, reproduce broken
   state via `git stash`-free method: NOT allowed; instead rely on
   /tmp/wig-bug/run1 in-game baselines + the scout's collapsed descriptions).
3. In-game gate: `scripts/native/band-closeup-capture.py --member all` re-roll
   until census shows crazyhawk or ziggymullet (check log `[HEADMAT]`);
   compare vs `/tmp/wig-bug/run1/r1_coop_g_b_0.png` (broken). Verdict PASS +
   hair visibly upright/fan-shaped = gate pass.
4. Commit `src/system/char/CharHair.cpp` only (message: decomp CFG fix +
   native hair fix; cite beq evidence). No Co-Authored-By.
5. Handoff doc `land-report.md` in this dir: capture paths, objdiff %, commit
   SHAs.

### Final — Fable visual review (main loop)

Fable (orchestrator) reads the capture PNGs directly and judges: hair
coherence (judge shape, not style — prefab lineup rotates), no new artifacts,
H2 follow-up assessment (post-fix hair through skull? → collide hookup
probe), H4 color assessment vs Dolphin GT.

## Sequencing / safety

- H and V run in PARALLEL: H touches `src/system/char/CharHair.cpp` + Wii
  build (flock-serialized); V touches `native/src/*` + native build dir.
  No overlap. G runs after both.
- Concurrent-agent hygiene: stage only own files; no stash/revert; engine
  repo untouched (no engine changes expected — if V needs an engine shim for
  a missing symbol, commit engine-first then bump MILO_ENGINE_PIN).
