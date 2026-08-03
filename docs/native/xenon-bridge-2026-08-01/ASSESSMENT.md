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

## X4c — LANDED 2026-08-02, all 3 milestones (xenon main `e486d726`..`05a8267d`)

Doc: docs/plans/x4c-init-audit-2026-08-02.md. Gate PASS 18/18 fresh, zero
engine edits (HEAD == pin 138e1606), X360 no-codegen-change verified **by
rebuild** rather than inspection.

**Init audit:** ~23 sub-inits audited using `nm` on the linked binary as
ground truth (targets build `--gc-sections`, so absence from `nm` *proves*
nothing calls it — a good instrument choice). `PreInitSystem` and
`SystemInit` are absent from the binary entirely; none of the 18 drivers
calls either; 19 sub-inits skipped, 5 provably inert. Two new Tier-1 finds:
`ThreadCallInit` (leaves `gMainThreadID == -1`, silently making **every
`MainThread()` assertion in the engine a no-op**, and `DataLoader::LoadFile`
can spin forever undiagnosed) and `ObjectDir::Init` (the `milo`/`milo_xbox`
loader factories never register; `LoadMgr::AddLoader` falls through to a
plain `FileLoader` with no warning). Both charter-named suspects came back
**innocent** — `TrigInit` shares only a *filename* with `TrigTableInit`.
**The stated reason 18 drivers skip `PreInitSystem` is false**: it is 39
lines with no renderer call (`TheRnd.Init`/`PreInit` have zero call sites
tree-wide). `SystemInit` genuinely is unreachable (4 named blockers).

**kNewGfx landed — and the charter's premise was wrong, which is the
headline.** kNewGfx never emptied the frame: a load-vs-draw bisect
(kNewGfx at Load, kOldGfx at draw) was still 0.00%, exonerating all 21
draw-time consumers, and posed bone positions are byte-identical between
modes. The real defect was in **`CharBonesSamples::LoadData`: the big-endian
swap used a 4-byte float width on a section that is three shorts** at
`kCompressVects`, overrunning into the quat section so the root bone's Z
became an unrelated quaternion component. Pelvis Z went from noise to a
standing bob. RB3-specific — DC3 fixed the same line but it was a no-op
there (all DC3 clips are `kCompressRots`).

**Coordinator E1 (I rebuilt `rb3-render` on main and ran the A/B myself,
because the lane removed its worktree and its evidence PNGs with it):
CONFIRMED.** `RB3_GFX_MODE=old` reproduces long flat ribbon spikes streaming
off both shoulders — visually the same family as rb3-Wii's "tree-branch
hands". Default (kNewGfx) has none: coherent torso and legs, cleanly skinned,
arms raised above the frame per the reaching clip. The control makes this a
direct attribution of the spikes to the 20→4 bone truncation. Two honest
caveats of my own: the framing crops head and arms, so "arms are correct" is
*not* verifiable from this render (only that the spikes are gone); and thin
residual slivers remain near the pelvis and right shin — much reduced, not
zero. No retail ground truth in this loop, so the claim is "no detectable
defect", not "matches the shipped game". Also established: **X4b's posed
character was mostly bind-pose geometry and its pose was wrong throughout.**

**Milestone 3 / the log-disambiguation experiment: 675 top-level / 0
persistent.** Every outstanding factory miss is on the recoverable
`ReadDead` path — there is no persistent-object wall. The `BandCamShot`→
`CamShot` bind's precondition is therefore met, but the lane correctly did
**not** land it: the short-read/`ReadDead` interaction needs measuring and
there is no clean signal while the venue still SIGSEGVs. So factory misses
were never the only venue defect. (X4a's documented venue path was also
wrong; the real one is `world/venue/small_club/small_club_01/gen/…`.)

**Invariant checks added** (`native/src/boot_invariants.h`) and validated
against a known-bad control: emptying `TrigTableAutoInit` makes the check
report `Sine(pi/2)=0.0000` and FAIL, i.e. it would have caught X4b's first
defect back at X1.

★ **Method lesson worth carrying:** pick oracles that are not invariant under
the defect you are hunting. X4b's bone-length invariant caught two real
defects and was **structurally blind** here — pairwise distances survive any
rigid motion of the root, and unit quaternions survive being wrong. One
absolute landmark position found in a single run what four milestones of
rigidity checks could not see.

**Cross-repo check (done by me): rb3-Wii is NOT affected by the byte-swap
bug.** Its `HX_NATIVE` `LoadData` reads element-by-element at the correct
width — `short *p` with `bs >> p[0] >> p[1] >> p[2]` at `kCompressVects`
(CharBonesSamples.cpp:609-613) — which is what V38's audit established. The
defect is specific to xenon's transcription.

## X4d — LANDED 2026-08-03: VENUES RENDER (xenon main `972f5a40`..`86b616a2`)

Doc: docs/plans/x4d-venue-root-2026-08-03.md. Evidence:
`/home/free/tmp/laneX4D/evidence/` (15 items, worktree left in place).
**6 of 6 venue roots load; 5 of 6 render** — not partial in the way every
prior milestone was.

**Coordinator E1 PASS, and it is the best frame this campaign has produced.**
`small_club_01` is a legible club interior: plank flooring, brick and panelled
walls, a bar, a staircase with railings, stacked chairs, ceiling beams — all
textured and coherently lit. 114 meshes, 96 drawn, 38.92% coverage, and
crucially **its own shipped `RndEnviron`** (`geom_norim.env`) — the first real
RB3 lighting in the port, where X4a had to synthesize one. Five distinct
environs across the sweep, so lighting is general rather than one asset's
quirk. `arena_01` = 251 meshes at 74.49%; `festival_01` = 307 including the
first skinned ones.

**Root cause — one boolean, in `src/system/obj/Dir.cpp` (`ObjectDir::PostLoad`
tail): the proxy guard was strictly weaker than retail's.** rb3-Wii's oracle
(`rb3/src/system/obj/Dir.cpp:475`) is `IsProxy() && !mProxyFile.empty()`;
xenon had `ShouldSaveProxy(bs)` = `IsProxy() && (!mProxyFile.empty() ||
InlineProxy(bs))`. The extra disjunct fires for a proxy dir with an **empty**
`mProxyFile`, building a `DirLoader` on an empty path and handing it the
parent's live stream. Byte-exact: a phantom `LoadHeader` eats 4 bytes at
offset 906607, so the next object starts 4 bytes late and the `3` it reads is
the *following* object's `WorldInstance` rev. Of 12 proxy dirs, 11 have a
non-empty `mProxyFile` and both guards agree; the 12th is the venue. One line,
HX_NATIVE-gated, X360 arm untouched (0 symbols differ, verified per-symbol —
the lane notes whole-file `cmp` is insufficient here).

**All three charter leads I supplied were REFUTED — usefully.** (1) Factory
misses are not the wall: the venue renders **with all 675 still present**, and
they are not even reached (0 `MISS-SKIP` before the crash). (2) `ObjectDir::
Init` is innocent *by construction* — `nm` shows `--gc-sections` stripped
`DirLoader::New` itself, impossible if any dir load used the factory table.
(3) **The `BandCamShot`→`CamShot` bind retires all 611 misses and BREAKS the
load (`rc=134`)** — because `ReadDead` recovers objects that were *skipped*,
never one that was *mis-parsed*, since the parse never returns. A miss is
strictly better and is already the default. My pre-dispatch caution about
base-class short-reads was therefore load-bearing; it is now measured, not
theoretical. The `ScatterIncludes` build fix enabled nothing here and was not
needed — it remains right for *content*, not loading.

**Riders both closed.** (1) Framing: an explicit ark path always took
`distScale 0.9`, tuned for the flat track piece rather than `1.15` for a tall
figure; with `--dist-scale/--azimuth/--elevation` added, **E1 confirms the
whole character in frame with a verifiably correct two-armed overhead reach,
face and features intact.** (2) The slivers are **not skinning**: present at
bind pose with no clip, and present in the X3 baseline PNG that predates all
pose work. They are the LOD2 body's own surface content amplified by this
cell's synthetic single directional light. The lane retracted its own first
answer (hand props) after `--only-mesh` killed it.

Gates: native 18/18 fresh rc=0; engine pin `138e1606` untouched, zero edits;
X360 0 symbols differ; PNG determinism 4/4; all X3/X4a/X4b/X4c evidence
byte-identical.

★ **Costing lesson:** four documents — including two of my own close-outs —
costed the venue at 13 TUs plus a `ScatterIncludes` lane plus 14 registration
lines. It was **one boolean guard**. Every estimate inherited X4a's framing
that the factory-miss log named the blocker; nobody re-derived the failure
from the stream itself until X4d traced it byte-by-byte. When a cost estimate
is inherited rather than measured, say so.

⚠ **Process, systemic:** the rebase re-gate caught **main broken by a match
lane** (DI-1) — the third such repair of the same shape now in main's history.
Decomp lanes land ungated because the native gate isn't part of their loop.
This is a standing gap, not an incident.

## ⚠ Coordinator hygiene item — uncommitted edit in the shared engine

`../milo-native-engine` has carried **one uncommitted edit since 2026-06-16**
(seven weeks): `src/platform/FxSendNative.cpp`, in
`NativeEffectSlot::SyncParams`. It is not mine and not any X-lane's — every
lane from X4a on has disclosed it and correctly left it alone. It is
semantically real, not whitespace:

```
-  p.mActiveBands = 0x1F;   // all 5 bands
+  p.unk0 = false;          // bypass slot (offset 0x0); SetParameters never reads it
   …
-  p.mBand5Q = eq->mHighPassReso;
```

So it re-interprets the field at offset 0x0 (plausibly a real layout
correction) and drops the band-5 Q assignment (plausibly a regression). **All
three consumers — rb3-Wii, dc3, rb3-xenon — have been building against this
out-of-tree state**, so every gate run and every "byte-identical PNG"
comparison since mid-June was taken against a working tree that does not match
any commit. Audio EQ only, off the load/render paths, so it does not confound
the render milestones. **Not actioned by me: I did not create it and do not
know the intent** (committing or reverting another agent's in-progress work in
a shared repo is exactly the failure mode the concurrent-agent rule guards
against). Owner decision: commit it with a message, or revert it.

## X5 — LANDED 2026-08-03: CHARACTERS RENDER INSIDE THE LIT VENUE (xenon main `678d57c6`..`202bc55e`)

Doc: docs/plans/x5-scene-2026-08-03.md. Evidence: `/home/free/tmp/laneX5/
evidence/` (worktree left at `/home/free/tmp/laneX5/wt`).
`small_club_01` renders **180 meshes / 48 skinned / 162 drawn**, rc=0, lit by
its own shipped `geom_norim.env`. The 48 skinned meshes are **eight crowd
members × six meshes, each driven by a real shipped `CharClip`** at beat 4.0.

**Coordinator E1 PASS, with the caveat the lane itself names.** The close
frame shows a textured, lit character — hair, jacket, boots, a held prop —
standing inside richly detailed venue geometry (beams, panelling, hanging
signs, a bottle-lined bar). Geometry, materials, textures, lighting, skinning,
animation and reference resolution are all alive in one scene. But **every
character sits at the world origin**, which in `small_club_01` is at ceiling
height, so the figure reads as standing on the ceiling and all eight crowd
members are coincident (the wide shot shows one figure, not eight). That is
precisely the missing placement, not a new defect.

★ **The headline is a measurement lesson, not a fix: the crowd was already
there.** X4d's "114 meshes (0 skinned), crowd UNREACHED" was an **instrument
artifact**. `ObjDirItr(dir, recurse=true)` descends only `mSubDirs`, and a
`Character` *is* an `ObjectDir` — so a loaded crowd member sits in its parent's
**hash table**, which that walk never enters. Eight fully populated crowd
characters (65 objects and 6 meshes each, with their `male_base`/`female_base`
clip sets) were resident in X4d's own `rc=0` run, invisible to the census
measuring them. **Not a decomp defect** — `ObjDirItr` is faithful (retail draws
through the `RndDrawable` tree); it was `rb3-render`'s flat mesh vector that
needed the deeper walk. Driver-side fix only.

**`player0` resolved 49 → 0 with no `BandCharacter`, no factory change, and no
`ScatterIncludes` lane.** The lane inherited X4d's "needs BandCharacter"
estimate, re-measured, and found it wrong *as a cost claim* (its facts hold).
`ObjPtr` resolution already falls back to `ObjectDir::Main()`
(`obj/ObjPtr_p.h:246`) and `main_render` calls `DataInit()` which creates
`Main()`; registering a real shipped `Character` named `player0` there before
the venue loads suffices. Crucially this does **not** repeat X4d's refuted
base-class bind — no wrong class is ever asked to *parse* a payload; the four
`BandCharacter` misses stay misses and `ReadDead` still recovers them. Positive
signal: TransProxy **bound** went 1/47 → **38/47**, and 37 is exactly X4d's
count of unresolved `.tp` player anchors. Control (`player1/2/3`, no stand-in)
unchanged at 5/5/4.

Gates: native 18/18 fresh ×3 including immediately pre-land; zero engine edits,
pin `138e1606` unmoved; **gate (c) vacuous — the entire diff is one file,
`native/src/main_render.cpp`**, so no shared-`src/` edit, no X360 codegen risk,
no objdiff debt; PNG determinism 4/4 post-rebase; X4d's venue frame reproduces
byte-identically under `RB3_NO_DEEP_TREE=1` and both X3 cells reproduce *with*
the deep walk on. **Main was not broken by a decomp lane this time — the first
lane in four.**

**Did NOT land: placement.** All nine characters at `(0,0,0)` — measured
(`1 DISTINCT world position`), not inferred. Two independent mechanisms are
missing and **neither needs `BandCharacter` or the ScatterIncludes lane**:
`WorldCrowd` scatter onto `mPlacementMesh` (6 objects load, none runs), and
`BandConfiguration` → `Character::Teleport` for band slots — and
**`BandConfiguration` is not ported in rb3-xenon at all** (factory shim only,
`bandobj/Band.cpp:66-73`; 176 lines in the rb3-Wii oracle). The lane
deliberately did not invent placement transforms, on the grounds that a
hand-picked position would look better and be indistinguishable from the real
thing in the evidence — the right call. Also unlanded: `player1..3` stand-ins
(left as a clean control), camera shots, audio, `video_05`.

**Four retractions, two of X4d's and two of its own** — including a clip
selection rule that silently gave `crowd_male01` a *female* clip and reported
success. And a **third consecutive lane hit the zsh word-split trap**: a camera
sweep silently rendered the default X3 cells instead of the venue and returned
`rc=0`. The failure mode is not "it errors", it is "it renders something else
and passes".

## X6 — LANDED 2026-08-03: THE CROWD IS PLACED (xenon main `9aff0a54`..`d589b78a`)

Doc: docs/plans/x6-placement-2026-08-03.md. Evidence: `/home/free/tmp/laneX6/
evidence/` (17 files). **Coordinator E1 PASS — this is the first frame that
looks like Rock Band.** `small_club_01`: ~30 crowd members standing on the club
floor along the railing, facing the stage, properly spaced and lit.
`arena_01`: a full arena — truss rigging, stage, tiered seating — with **4700
crowd members massed on the floor in front of the stage**. Every position read
from shipped asset data; **the lane authored no transform**, which was the one
instruction I weighted above all others in its charter.

★ **My charter's premise was wrong, and finding that out closed a lane's worth
of planned work.** I handed X6 "write the `WorldCrowd` scatter onto
`mPlacementMesh` — 6 objects load, none runs". **There is no scatter.**
`WorldCrowd::OnRebuild` is `return 0;` in rb3-xenon, in the rb3-Wii oracle,
**and** in DC3 — three independent decomps of the same engine. It is a
Milo-editor routine compiled out of the shipping game. Every crowd position is
baked at author time and deserialized by `WorldCrowd::Load`
(`world/Crowd.cpp:361-368`) straight into `mMMesh->Instances()`. **The data had
been resident since X4d**, inside the same `rc=0` runs that reported "no
crowd". One `grep` for `OnRebuild` across three repos settled it.

Why it was invisible is the same shape as X5's finding, one level up:
`rb3-render` draws a flat `vector<RndMesh*>`, and **`WorldCrowd` is an
`RndDrawable`, not an `RndMesh`** — its 355-line `DrawShowing()` was
unreachable.

| venue | `WorldCrowd`s | baked instances | distinct positions | `mShowing` |
|---|---|---|---|---|
| `small_club_01`/`_02` | 6 | 300 / 300 | **300 / 300** | 1 |
| `arena_01` | 18 | **4700** | **4700** | **0** |
| `big_club_01` | 24 | 4643 | **4643** | **0** |
| `festival_01` | 18 | **8400** | **8400** | **0** |

Instances **equal** distinct positions in every venue — not one coincident pair
across 18,343 members. `small_club_01` spans x∈[-161,161], y∈[-298,-22] at
constant per-archetype **floor** height, i.e. nowhere near the origin (which in
that venue is ceiling height — the X5 symptom).

**Disclosed substitutions and open items:** wiring `WorldCrowd::Draw()` in was
**not sufficient** — the frame stayed byte-identical, because the native
impostor path does a nested render-to-texture composited with **additive**
blend, the RTT emits nothing on this backend, and additive-black is the
identity. Caught only by varying 0→30→300 against a control. The driver
therefore draws the archetype's **real geometry** at each baked transform: a
**mechanism** substitution, never a placement one, and disclosed as such.
**Visibility policy is deliberately unresolved (top handoff):** `mShowing`
selects **0 crowds in 4 of 6 venues**, so the faithful default renders no arena
crowd and `--crowd-all` (measured safe) is what produces the arena frame above.
The lane left that choice to the owner rather than quietly picking one.
**`BandConfiguration` ported but yields less than I hoped:** all deps were
already present and `BandInit` measures 98.1% / 252 instr / 95 diff_arg
**identically before and after**, same mismatch list item-for-item — but
**objdiff cannot score it** (no `splits.txt` entry → no target `.obj` → no
unit), and it places nothing at runtime yet (needs `BandWardrobe`/
`BandCharacter`, behind the ScatterIncludes lane). It lands the path, not the
placement.

**Four retractions**, including X5's scatter estimate, the lane's own
"wiring `Draw()` in will work", its own family-duplicate hypothesis (refuted by
the instrument it built to confirm it — all 15 pairs share **zero** positions),
and X4d/X5's "9 characters ALL STACKED" as a *diagnosis*: that was a correct
measurement of the wrong objects, since archetypes are templates and a
`Character` census can never see the crowd (crowd members are transforms in an
instance list). The lane also **caught a fabricated PNG hash in its own first
draft and recorded it in the doc** — exactly the right response.

Gates: native 18/18 fresh ×2 (own base + post-rebase); X360 full build rc=0
×2; every cited PNG deterministic ×2; legacy-walk control reproduces X4d's
exact SHA; engine pin `138e1606` unmoved. `main` healthy this lane (it was
broken by a decomp lane in 3 of the prior 4). `video_05`'s `rc=1` fails
identically with the crowd draw off — X4d's carried defect, not X6's.

★★ **The rule this campaign keeps re-learning, now three lanes running: ask
what the missing code would DO before planning to write it.** X5's scatter,
X4d's "needs BandCharacter", and X4a's band3 framing were each built from
entirely correct facts and each wrong by a whole subsystem. Every one was
settled by a single cheap measurement that nobody took because the estimate
had been inherited rather than re-derived.

## X7 — LANDED 2026-08-03, MILESTONE DID NOT LAND (xenon main `ba3dd797`..`9931c1b7`)

Doc: docs/plans/x7-band-on-stage-2026-08-03.md. Evidence:
`/home/free/tmp/laneX7/evidence/`. **No frame in this lane contains a band
member** — stated plainly by the lane, and correct. What landed is the path to
one plus a precise account of the wall.

★★ **Band stage placement is baked shipped data, in all six venue roots** — the
same shape as X6's crowd finding. The lane asked what `BandConfiguration::
SyncPlayMode` *does* before porting anything: it computes nothing, it looks up
a stored `Transform` and calls `Teleport`. Each venue ships **12 named slot-rows
(4 band slots × 3 play modes)**, all non-identity. `small_club_01` mode 0:
`player_bass0 (-70.0, 80.7, 13.5)`, `player_drum0 (14.4, 146.1, 13.2)`,
`player_guitar0 (68.8, 51.4, 13.2)`, `player_vocals0 (-10.0, 31.4, 13.2)`.
**The data validates itself against landmarks with no interpretation:** the 12
transforms span 145×146 in x/y but only **0.313 in z** — coplanar, a stage
floor; band y is entirely positive and crowd y entirely negative (opposite
sides of the origin); and `arena_01` ships a **drum riser** — bass/guitar/vocals
coplanar to 0.32 at z≈255.9, drummer at 320.9, exactly 65 higher and ~480
upstage. A canonical Rock Band layout falls out of the shipped file. It was
**one CMake line** away: X6 ported the TU and wired `objects.json` (the X360
build) but never `native/CMakeLists.txt`.

★ **The four-times-deferred `ScatterIncludes` blocker was never a lane: three
`#if !HX_NATIVE` lines.** Every fact in the inherited estimate was true and the
conclusion did not follow — 807 duplicate definitions is the cost of adding all
*ten* bandobj TUs, and **CMake structurally cannot perform that dedupe** (its
rule drops an includee that is also a target source; every collision here is
between two *emitters* that must both stay, so any winner-picking still needs a
source-side guard). Also `BandWardrobe.cpp` **had been compiled into every
`rb3-render` binary since X3** (91 symbols in `Console.cpp.o`), invisible only
to `--gc-sections` — half of X6's handoff was stale. `BandCharacter.cpp` went
**18 errors → 0** (all four defects rb3-Wii-shape ports onto xenon types, all
inside `HX_NATIVE` arms); ten TUs now link with 0 errors, 0 duplicates, 0
undefined. **That is the fourth consecutive inherited cost estimate to be wrong
by a subsystem** — see the standing rule.

**The real wall:** registering `BandCharacter` *does* construct band members,
then desyncs the stream (`FAIL: String chars 290146 > 128`, rc=139).
`chars.milo`'s `player0` is a proxy declared `BandCharacter` whose proxied
`char/main/main.milo` declares root `RndDir`, so `DirLoader::SetupDir:712-748`
`ReplaceObject`s a half-loaded dir mid-stream. Off by default behind
`RB3_BAND_MEMBERS=1`. **The lane generalized the rule correctly: X4d's "a miss
beats a wrong parse" also covers this, where the class is *right* and the
outcome is the same shape.**

★ **Crowd visibility answered with a mechanism, so no default needs picking.**
`mShowing` in the asset is **dead data**. Every camera cut, `CamShot::StartAnim`
pushes the shot's own `mCrowds` into `WorldDir::SetCrowds`, which shows exactly
the crowds that shot names and hides the rest — confirmed in rb3-xenon, rb3-Wii
**and** DC3. The `_ps3` suffix is a red herring (the platform gate is
`CamShot::mPlatformOnly`/`PlatformOk()`). **`--crowd-all` should NOT become a
default.** Blocked on the same surface as the band: `BandCamShot` is 611 of 675
misses.

Gates: native 18/18 fresh rc=0 with **0 SKIPs** — but it **FAILED first at
8/18**, because stubs went into `native_undecomp_stubs.cpp`, which all 18
targets link ("a stub's blast radius is the source list it sits in"). X360
non-regressed at symbol granularity across all 10 touched units; PNGs
deterministic ×3 and stable across a full toolchain rebuild; `main` not broken
by a decomp lane. Disclosed substitution: 18 declared-never-defined functions
stubbed including five deform/refine passes and all of `CharKeyHandMidi` — a
member would be posed but unrefined; **none on the placement path**, and no
position was computed, guessed or hand-picked.

### ⛔ Coordinator correction — X7's retraction of X6's SHA table is itself wrong

X7 landed a retraction asserting X6's recorded PNG hashes "do not match X6's
own evidence files" and that X6's table carried fabricated hashes. **I
re-measured every artifact: all four of X6's SHAs are correct**
(`d7963b8c1e6d5711` / `5282bd275159f10b` / `2f36c1e369314e11` /
`218cf68dd5a019a7`). Two errors produced the false retraction: (1) X7's own
table asserts `cmp` **IDENTICAL** against an artifact *and* a different
`sha256` than that artifact has — both cannot be true, so its hash came from a
file other than the one it compared; (2) X6 §3's *identical* hashes across
0/30/300 instances are its **finding** ("300 instances and not one pixel
changed"), not a transcription error — §4.1 carries the post-substitution
hashes, which match. Corrected in-place on xenon main (`0ea11c3c`) with the
withdrawal recorded next to the original claim rather than rewriting it.
X7's non-regression conclusion is unaffected and correct. **The durable rule:
before retracting another lane's numbers, re-measure the artifact and check
your own two instruments agree with each other — an accusation of fabrication
against a correct record is more corrosive than the error it alleges.**

## X8 — LANDED 2026-08-03, band still does not render by default (xenon main `1e24f4da`..`db8f09fc`)

Doc: docs/plans/x8-band-render-2026-08-03.md. Evidence: `/home/free/tmp/laneX8/
evidence/` (36 artifacts). **All four members (`player0`–`player3`,
`BandCharacter`, 50 objects each) now instantiate at 4 distinct world
positions — but those are `char/main/main.milo`'s own authored defaults** (a
straight line ~37 units apart at y=28.85, **z=0**), not the venue's shipped
slots. So they are off the stage floor plane, in a row, not on their marks.
Geometry renders (114→148 draws) only under an explicitly-labelled diagnostic
flag. **No position was computed, interpolated or hand-picked anywhere in the
lane** — the standing rule held for the fourth consecutive lane.

**Coordinator E1:** the diagnostic frame shows the venue and its crowd intact
with a white, untextured figure standing mid-room — consistent with the lane's
own account (members resident, positions wrong, outfit/LOD selection blocked
downstream).

★ **X7's wall was misdiagnosed, and retracting it was the lane's headline.**
`char/main/main.milo` declares **`BandCharacter`** (rev 0x1C, read from the
asset bytes), not `RndDir`; `SetupDir`'s body is already equivalent to
rb3-Wii's, so there was no oracle diff to find. The `RndDir` came from
`LoadHeader`'s `mRev<=0xC` arm after reading a garbage rev of **8** from the
four bytes following an `0xADDEADDE` terminator — and the competing arm's
"not registered, defaulting to" message appears **zero** times in X7's own log,
which was the disproof sitting in the evidence all along. Real cause:
**`ObjectDir::InlineProxy`'s `HX_NATIVE` arm read the `mInlineProxyType`
*field* instead of dispatching through the virtual `AllowsInlineProxy()`**,
which `BandCharacter` overrides to `false` in both trees. One guard restored:
`rc=139` → `rc=0`.

**Defect 2 confirmed independently from X360 assets:** 11 shipped `small_club`
roots carry 1322–1360 `player_vocals0` references each and **zero**
`player_mic0`. Exercising it needed two further fixes: `BandCharDesc::Init()`
was never called (leaving `gInstNames` all-null → SIGSEGV — the **4th**
instance of the init/factory-list drift class), and the venue's
`BandConfiguration` never became the wardrobe's mode sink (its own `Load` guard
runs before `TheBandWardrobe` exists). All four slots now resolve.

★★ **A defect class nobody had named: 248 `Symbol` globals were dead dispatch
keys.** `HANDLE_ACTION` compares `sym == symbol` against the *global itself*,
and 139+109 of them are default-constructed to the NULL symbol. Every handler
keyed on one silently did nothing — `rc=0`, no warning. **That is why the
venue's authored band transforms were never applied for four lanes.** As the
lane put it: *a dead key is indistinguishable from a message never sent.*

**Did not land:** the shipped `enter_venue` path still crashes, so placement is
opt-in behind `RB3_BAND_PLACE=1` rather than shipped broken. New wall named to
one function: `obj/ObjPtr_p.h:777-789` suppresses an `ObjPtrList` erase while
`gInReplaceList`, leaving a NULL in a `kObjListNoNull` list;
`BandCharacter::SyncObjects`'s shipped loop (token-identical to rb3-Wii's)
dereferences it, and the entry never leaves, so a null-skip would spin forever.
Downstream: all 140 of a member's meshes have `Showing()==false` (only 34 carry
geometry) because outfit/LOD selection lives inside that blocked path. Flagged
rather than asserted: the four members share **one** 140-mesh set, and band
meshes report `skinned=0` against the crowd's `skinned=6`.

Gates: native 18/18 fresh, 0 SKIPs, on the rebased tree; all three touched X360
units unchanged to every digit; both cited frames reproduce byte-identically.
**X6's SHA table is now independently vindicated a second time** — X8's default
frame hashes `5282bd275159f10b`, exactly X6's recorded E1, with `cmp` and
`sha256sum` agreeing with each other and with X6's document.

### ⚠ My own failed cross-repo check, recorded because the failure is instructive

I tried to determine cheaply whether rb3-Wii shares the dead-`Symbol`-key
class, since it could bear on the remaining Wii visual defects. My grep found
"169 matches in rb3-Wii vs 223 in xenon" — and **that number is worthless**:
sampling showed the matches are struct *members* (`Symbol unk28;` in headers)
and function *locals* (`Symbol s;`), not file-scope dispatch keys. The
instrument never located the population X8 described. This is exactly the
"aggregate consistent with two different worlds" trap this campaign keeps
re-deriving, and I nearly reported it as a cross-repo finding. **Status:
genuinely unverified** — handed to X9 as a named check rather than asserted in
either direction.

## X9 — LANDED 2026-08-03: ★ THE BAND IS ON ITS MARKS (xenon main `bb3a4659`..`8d7c7dc6`)

Doc: docs/plans/x9-band-marks-2026-08-03.md. Evidence: `/home/free/tmp/laneX9/
evidence/`. **Placement: YES. Geometry: partial.** Four members stand at four
distinct world positions and **every one matches the venue's authored slot
transform to every printed digit**. No position was computed, interpolated or
hand-picked — six lanes running.

| slot | shipped transform | member | measured |
|---|---|---|---|
| `player_bass0` | (-70.003, 80.657, 13.495) | `player0` | (-70.00, 80.66, 13.50) |
| `player_drum0` | (14.429, 146.133, 13.182) | `player1` | (14.43, 146.13, 13.18) |
| `player_guitar0` | (68.770, 51.436, 13.248) | `player3` | (68.77, 51.44, 13.25) |
| `player_vocals0` | (-10.026, 31.389, 13.218) | `player2` | (-10.03, 31.39, 13.22) |

★ **The strongest evidence in the whole campaign is `arena_01`, because the
number was predicted before anything could render it.** X7 measured the shipped
drum riser at **+65** from the asset alone; X9's rendered drummer sits at
z=320.90 against bass/vocals/guitar at 255.83/255.79/256.12 — **+65.07**. Eight
slot matches across two venues. A hand-placed position cannot survive a
prediction made two lanes earlier from a different instrument.

**Coordinator E1 PASS.** The stage crop shows a vocalist standing on the stage
floor with arm raised, a full drum kit (snare, toms, cymbals), and guitars —
all on their marks, with the crowd visible behind the railing. This is a Rock
Band stage.

**The wall took one line, and the guard's own comment said where it came
from.** X8's observations were all correct; the *justification* was not. The
comment's last line read *"Matches the guard in `ObjPtrVec::ReplaceNode`"* — it
was reasoned by analogy, and the two containers aren't the same shape. In
`ObjPtrVec` the guard is **real** (Nodes live inline in a `std::vector`, so
`erase()` memmoves survivors via `CopyRef`, rewriting live ring pointers) and
was left untouched. `ObjPtrList`'s Nodes are individually heap-allocated and
never move. The second stated hazard was false too, with the disproof 130 lines
up in the same header: `old` is `SetObj`'s return, i.e. the **new** referent,
so reaching the erase arm means `Release(this)` already ran and the node is
already out of the ring. rb3-Wii erases unconditionally and has no guard.
`rc=139` → `rc=0`; scene 320 → 411 meshes, 114 → 203 draws. X8 had framed this
as needing "its own lane with an A/B on every prior frame" — it perturbs zero
prior frames and zero X360 units.

**Both of X8's flagged items were artifacts and are refuted:** the four members
do *not* share one mesh set (160/169/162/160, each with its own `outfit` and
`instrument` subdirs), and band meshes are not unskinned (19/27/22/18; 134
scene-wide).

★ **Cross-repo dead-key question answered properly: NO, rb3-Wii does not share
it.** The instrument had to start from X8's own enumeration — the 248 are
textually `^Symbol <ident>;` **at column 0**, which is the entire discriminator
and reproduces 139+109=248 exactly. The indent-tolerant variant reproduces
**my** failed attempt (164), and sampling confirms those are members and
locals. **The negative is trustworthy because of a positive control:**
intersecting the 248 names against every bare-identifier `HANDLE*`/`SYNC_PROP*`
first argument finds **227 live dead keys in xenon**; the same instrument on
rb3-Wii finds 2 file-scope globals in `src/`, 1 in `native/`, 0 in the engine —
all three sampled, none a dispatch key. Audited from the other side too: 4002
distinct dispatch keys, 3997 interned, all 5 residuals run to ground as
instrument artifacts. rb3-Wii has the `Symbols*.cpp` files xenon lacks (7146
interned symbols). **This closes the open question I could not answer.**

**Did not land: the band is placed, not fully drawn.** Nine SHOWN-BUT-EMPTY
meshes per member (`head`, `eyes`, `tongue`, upper/lower teeth, `hands_naked`,
`fingernails`, `eyebrows`, hair) — selected correctly by the recompose but
carrying zero vertices — against 34 FULL-BUT-HIDDEN which are tattoo/AO/
placement decals and are *believed* correct (flagged, not asserted). The
obvious next fix was tried and **reverted**: `OutfitConfig::Init()` is the
**fifth** instance of the factory-drift class and gates exactly that
head/hands set (`Can't make OutfitConfig` ×40), but it does not link.

★★ **That failure is the most important finding for the next milestone.**
`BandPatchMesh.cpp` is not compiled standalone — it is scatter-included into
`src/system/world/LightPreset.cpp:1503`, an **X360 TU-packing decision made for
objdiff scoring**, and `LightPreset.cpp` isn't in `rb3-render`'s source list.
**A match-build packing choice silently determines what the native link
contains.** That is X7's "a stub's blast radius is the source list it sits in",
inverted — and it is unlikely to be a one-off.

Gates: native 18/18 fresh ×2, 0 SKIPs, all relinked; X360 same-`main` A/B with
**0 units changed** (39.154087% / 62.961410% both sides); PNG determinism ×3;
prior lanes byte-identical **against artifacts** — X6's SHA table vindicated a
**third** time; `main` not broken by a decomp lane. Two notes: `obj/ObjPtr_p.h`
is a header with no `splits.txt` entry and **cannot be scored at all** (blast
radius measured instead), and the gate cache trap has a **third** ingredient
beyond the two I documented — `native_build_gate.sh:228` also sets the
compiler, so a two-flag seed picks up system `g++` and fails with ~104 errors
that look exactly like a broken `main`. I corrected my CLAUDE.md note
accordingly (xenon `7f18adab`).

## X10 — LANDED 2026-08-03: X9's gap was mostly an instrument artifact (xenon main `163ec8ac`..`c2344a87`)

Doc: docs/plans/x10-band-geometry-2026-08-03.md. Evidence:
`/home/free/tmp/laneX10/evidence/` (23 files).

**Six of the nine "empty" band meshes were never empty.** X9's probe asked
`NumVerts()`, which reads a mesh's own `mVerts` — but the native loader
deliberately does `mVerts.resize(0)` and parks the shipped blob in
`mCompressedVerts`. The hair X9 called empty carries **2348 verts / 3012
faces**. The positive control was sitting in X9's own log: the venue's
`stage.mesh` reports `verts=0 cverts=140`, and the stage has rendered in every
frame since X6. Quantitatively the old predicate said 30 while the renderer
issued **203 draws**. Per member: SHOWN-BUT-EMPTY 9 → **3**, DRAWABLE 5–9 →
**16–26**. Genuinely empty: `head.mesh`, `hands_naked.mesh`,
`eyebrows*_resource.mesh`. **This was a measurement change, not a render
change — the frame is `cmp`-identical before and after.** My E1 confirms the
frame is unchanged and correct: crowd along the railing, vocalist plus drum kit
and guitars on the stage.

**X9's `OutfitConfig` root cause is refuted on two independent grounds.**
`LightPreset.cpp` *is* in `rb3-render`'s source list (via `file(GLOB
world/*.cpp)`), and `BandPatchMesh.cpp` is listed **directly** at
`native/CMakeLists.txt:1227` — the build prints the disproof at configure time.
The real cause is that `BandPatchMesh.cpp` is a **191-line partial port** vs
rb3-Wii's 1511, owing 48 symbols (11 bodies + 37 undefined globals). **X10
deliberately did not pay that bill**, on the grounds that `OutfitConfig` is a
material/texture compositor supplying **no geometry** — it would fix tattoos and
skin composites, not a single vertex. Correct call.

★ **Scatter-include audit delivered, instrument validated by positive control
(reproduces 109/109 of the build's own prune decisions, zero disagreements).**
**Direction B is real and nobody had noticed:** `rb3-milo`/`rb3-render` each
compile **zero** TUs of 8 modules yet link **20 files** from them — including
**5 `system/rnddx9` (D3D9) files into a WebGPU target**, via
`rndobj/CubeTex.cpp:284`. Currently masked by `--gc-sections` (105 `Dx*::`
symbols in the object, 0 in the binary): latent, not active. Also 42 files
reach no native target at all, and 22 multi-host duplicate landmines, none
live. **Direction A is not decidable from the graph** — "missing" is a demand
property; the lane's first cut emitted 2110 meaningless rows and it **discarded
them rather than reporting them**, offering a labelled predictor instead.

**X9's "34 FULL-BUT-HIDDEN decals are correct" holds** — but the count was
wrong (140, same broken predicate), and the better reason is that it is the
entire both-gender wardrobe option catalogue. The gender flip X9 asked for is
observed. Caveat: `CharMeshHide::HideAll` is an inert stub, so the mechanism
isn't the shipped one.

**Did not land:** `OutfitConfig` still unregistered, three meshes still empty —
but X10 established **they are not the same problem**: the 40 `OutfitConfig`
failures fire *identically* on the hair milo (geometry loads) and the head milo
(it doesn't). Three of its own hypotheses retracted, two before writing code.

★★ **The finding worth carrying: a cause that is constant across the working
and broken arms is not the cause.** One log line read *comparatively* retired
the entire briefed milestone before any code was written.

Gates: native 18/18 fresh, 0 SKIPs, all relinked, re-gated pre-landing; zero
engine edits; **zero shared `src/` touched → no X360 blast radius**; PNG
determinism ×2; default frame byte-identical to X9's artifact with both
instruments agreeing — **X6's SHA table vindicated a fourth time**; `main` not
broken by a decomp lane. Trap passed on: X9's "E0 default" is `small_club_01`
*without* `RB3_BAND_PLACE`, not the two-cell run — comparing against the wrong
cell looks exactly like a regression.

### ★ Meta-observation: handoff diagnoses are hypotheses, and the process is catching them

Seven of the ten lanes' handoff root causes have been **refuted by the next
lane**: X4a's band3 framing, X4d's "needs BandCharacter", X5's WorldCrowd
scatter, X7's ScatterIncludes lane and its proxy-conversion diagnosis, X8's
"`ObjPtrList` needs its own lane", and now X9's `OutfitConfig` cause plus its
nine-empty-meshes count. **This is the method working, not failing** — each
lane re-derives from measurement instead of inheriting, and the refutations are
consistently cheap (a `grep`, a log line read comparatively, a positive
control). The lesson for charters, including mine: **state a predecessor's root
cause as a hypothesis to test first, never as a premise to build on.**

## X11 — LANDED 2026-08-03: the band's faces and hands are back (xenon main `e52065f8`..`9e57cad2`)

Doc: docs/plans/x11-mesh-geometry-2026-08-03.md. Evidence:
`/home/free/tmp/laneX11/evidence/` (38 files). **All five empty meshes now
carry geometry** — the three named plus two X10 never found
(`malewrist_barbedwire_right`, `malewrist_hercules_right`). Per member
SHOWN-BUT-EMPTY **3–4 → 0**, and **DRAWABLE now equals `showing` exactly**
(19/19, 29/29, 22/22, 19/19); same on `arena_01`, with the +65.1 drum riser
preserved. **Coordinator E1 PASS on the zoom pair: the baseline has a dark void
where the face should be; X11 shows a rendered head with facial features and
hair.**

★ **X10's central conclusion is refuted: the meshes never shipped empty.**
Loaded standalone, `head.mesh` carries **2592 verts / 4726 faces**,
`hands_naked` 1876/3092, `eyebrows1` 302/254. They load every run — something
throws them away. The mechanism is `SetKeepMeshData(false)`
(`rndobj/Mesh.cpp:954-965`), which clears `mVerts` and frees `mFaces` while
leaving `mCompressedVerts` alone: the observed signature exactly. Two sites —
`bandobj/BandCharacter.cpp:1118` (the `RndMeshDeform` drain → hands, eyebrows,
wrists) and `char/CharMeshCacheMgr.h:9-16`, reached because `SetDeformation`
does `mgr->Disable(!mInCloset)` → **head**. `head.mesh` arrives at site 1
*already* at `verts=0`, so as the lane put it: *had I stopped there I'd have
reported a fixed head that was still empty.*

**Both releases are correct on console** — the platform vertex buffer already
exists, so the CPU copy is dead weight. The WebGPU backend uploads lazily at
first draw, so the release destroys geometry **before it is ever uploaded**. A
**lifetime mismatch, not a decomp defect**; both fixes `HX_NATIVE`-gated with
`RB3_RELEASE_MESHDATA=1` to opt back in. **The proof is a set identity, not a
count:** `RB3_TRACE_KEEPMESH=1` names every released mesh, and the released set
*is* the broken set — eyes, tongue, teeth, hair, fingernails and
`male_neck_ao` never appear in it.

**Direction B: the sharpest row is closed** — 105 `Dx*::` symbols → **0**, D3D9
out of the WebGPU target, zero pixels moved. But guarding the edge **broke the
link** on exactly one symbol: `Hmx::Matrix4::Col3`, declared in `math/Mtx.h:128`
but **defined at `rnddx9/Cam.cpp:13`** and called from `rndobj/Lit_NG.cpp`.
★ **`--gc-sections` proved the *symbols* were dead; it did not prove the *file*
was.** Provided natively, copied verbatim. **The other 7 rows are explicitly
not closed** — the best-evidenced row had a hidden dependency, so the lane
assumes the rest do too. Correct inference.

Gates: native 18/18 fresh ×3 (including after both rebases), 0 SKIPs, 18
relinked; **both X360 A/Bs Δ+0 matched, Δ+0.000000pp code%, Δ+0.000000pp
fuzzy** across 8 real recompiles; PNG determinism ×2; E0 byte-identical to
X10's artifact — X6's SHA table vindicated a **fifth** time; `main` not broken.
`CharMeshCacheMgr.h` is a header and **not scoreable**; its consumers measured
unchanged; the two native driver TUs have no `splits.txt` entry at all.

**Did not land, and flagged rather than asserted in either direction: the
restored hands' pose is unverified.** They draw with `nullbones=0`, but at ~40 px
they read as possibly detached from the sleeves. **The lane names this as the
single most important item for X12** rather than claiming success — the right
call, and exactly the discipline that has made this ladder's results credible.
Also untouched: `OutfitConfig` (still unregistered), 7 Direction-B rows, 42
orphan files, 22 landmines, and `CharMeshHide::HideAll` (still an inert stub).

**Near-miss worth carrying:** `ab_measure --revert` leaves the *reverted* patch
in the worktree, so everything built afterwards silently lacked the fix — and
the frame came back byte-identical to X10's baseline, which reads exactly like
"Direction B undid the mesh fix". It hadn't. Caught by `cmp`-ing against three
candidate artifacts instead of one. Recorded in xenon `CLAUDE.md` (`7d5c566b`).

## X12 — LANDED 2026-08-03: hands VERIFIED correctly posed (xenon main, 5 commits)

Doc: docs/plans/x12-hand-pose-2026-08-03.md. Evidence:
`/home/free/tmp/laneX12/evidence/` (14M). **X11's open question is answered
YES, for bind pose**, via four absolute checks with no ratios: recompose
identity `W == L·parentW` elementwise **0.000e+00 over 7434 bones** (7953 in a
second venue); arm chain monotone and L/R symmetric to 3 d.p.; both hand bones
**inside** the hand geometry; `malewrist_barbedwire_right` contains the **R**
hand bone and is 40.5 from the L — handedness, not merely presence; and
`fingernails_resource.mesh`, a mesh *never* in the broken set, nests inside the
restored hands to 0.1u. **"Detached from the sleeves" is refuted**: on player0
the garment spans x −9.24…8.84 while arm skin spans ±25.22 — it never reaches
the arms; on player2, which does have sleeves, the sleeve→forearm→hand join is
continuous at 1600×1200.

★ **The existing `--bone-audit` was a vacuous green.** It walks `ObjDirItr`,
reaches `character: 'lighttarget'`, and reports *"palette-invariant PASS — 0
bone(s) over 0 mesh(es)"* — while the deep walk in the same run saw 134 skinned
meshes. The new `--hand-audit` uses `CollectDeep` and the *shipped*
`RndMesh::SkinVertex`, with a positive control: injected 17.0 → measured
**16.9998**, localized to the bone, right hand untouched. **And the charter's
warning was demonstrated outright — `bone-length-invariant` emitted
byte-identical output in both runs**, i.e. the rb3-Wii-style ratio oracle
literally cannot see this class. That is the W35-0 finding confirmed
experimentally on a second codebase.

**Alias hazard, partially live:** X4b's fix at `mtx.cpp:77` is intact
(0.000e+00 under both aliasings, now gated), but **`Rot.cpp:299`'s `HX_NATIVE`
arm IS unsafe** — stores `vout.x` at :306 then re-reads `vin` at :307-308.
Dormant, and it gates Direction-B row 1.

**Direction B: rows verified, none broken, correctly.** It is **6 rows, not 7**
(row 5 closed transitively by X11; row 7 was a mis-classification — `midi/` *is*
compiled natively). Rows **2 and 3a must stay open** — `PanelDir.cpp:439-455`
is port-added `#ifdef HX_NATIVE` code driving `Flow`, and `GemManager.cpp:1649`
carries `TheFlowMgr`: the exact `Col3` shape again. Rows **4 and 3b are clean**
and should be done first.

**The lane published a wrong table and corrected it in-tree** (`785b7ef6`, with
retraction and evidence): its first §6.1 marked every edge unguarded because its
grep printed `#include` lines without the surrounding `#ifndef` and it did not
open the files — *the same failure its own §3 documents in someone else's
instrument*. Self-caught and self-corrected is the right outcome.

**Did not land:** no Direction-B row closed (correct — two are load-bearing);
**hand pose under animation unverified** (this path applies no clip, so
"correct" means correct *bind* pose — cheapest next check); `Rot.cpp:299`
unfixed; `OutfitConfig` untouched.

★ **Handoff for X13, stated carefully by the lane:** at bind pose the palette
and `meshWorld` disagree by **exactly the character's placement** — for every
worn mesh *and* every never-broken control mesh alike. Constant across the
working and broken arms, so by X10's rule it is **not a hand defect** — but it
is real, and confirmed across two venues whose placements differ by hundreds of
units while hand geometry measures identically.

### Coordinator E1 — PASS, plus one observation the lane did not report

The closeup is the best frame of the campaign: a full character with eyebrows,
moustache and eyes, long hair, a detailed jacket with buttons and shoulder
work, a studded belt, and **hands with individual fingers gripping a mic stand**
— hands plainly attached and naturally posed, which independently corroborates
the numeric verdict. **However**, the top of the frame shows a row of legs and
boots apparently hanging from the ceiling. Most likely benign — `small_club_01`
has an upper level, and this camera sits below it, so the mezzanine crowd's legs
show through a floor that isn't drawn from underneath (back-face culling) — but
it is **unconfirmed**, and it is exactly the kind of thing that reads as correct
until someone looks from a different angle. Handed to X13 as a named check, not
asserted as a defect.

Gates: native 18/18 fresh, rc=0, 0 SKIPs at three points; `main` **not** broken
by a decomp lane (four landed underneath); band frame byte-identical to X11's
artifact before *and* after rebase; PNG determinism ×2; **zero shared `src/`
files touched**.

⚠ Infra: `/tmp` is a tmpfs that hit its 38270M user quota mid-lane and **broke
`git commit` heredocs** (worked around with message files). Now at 63% / 18G
free, but it will bite again.

## X13 — LANDED 2026-08-03 (xenon main `1fc71cff`..`962a9200`)

Doc: docs/plans/x13-animated-pose-2026-08-03.md. Evidence:
`/home/free/tmp/laneX13/evidence/` (25 artifacts).

**Animated hand pose — split verdict, because the two populations differ
structurally.** **Crowd: VERIFIED CORRECT** — eight members driven by the
shipped `crowd_realtime_idle_10` at beat 4.0 (41 polls); the skeleton genuinely
moved (left hand travelled 15.4 units, 112-line oracle diff) and **every arm
segment length was preserved exactly** (6.211 / 9.584 / 10.105, identical
bind→animated), recompose identity **0.000e+00 over 7380 admissible bones**.
**Band: UNDECIDABLE, structurally** — all four members' driver binds to
`body_clips`, which holds **zero** `CharClip`s against 40–44 for every crowd
member. No clip can reach them, and the lane **did not invent a cross-set
fallback to manufacture a frame**. Positive control run *under animation*:
`RB3_HANDPOSE_PERTURB=17` moved parent distance 10.105 → 27.105, exactly
+17.000, left hand only. It also corrected X12's oracle — the recompose check
is invalid under animation as written, because `WorldXfm_Force`
(`rndobj/Trans.cpp:666-687`) picks among four compositions on `mConstraint` and
`SetWorldXfm` bypasses it entirely; bones are now split admissible vs excluded,
and a new `handpose-measured-hand-geometry` gate catches X12's case of passing
on a skeleton while measuring **zero** hand meshes.

★★ **The palette-vs-`meshWorld` gap is NOT benign — and it corrects my own
reporting.** The band's skeleton is parented to an unnamed `Character` **at the
origin**; the placed transform is not on the bone chain. The renderer
deliberately cancels `meshWorld` out (`Rnd_Wgpu_RB3.cpp:4077`), drawing at
`skin·v`, so the palette must be world-space on its own — and isn't. Proved by
set identity, not magnitude: **four members at slots spanning ~140 units all
report drawn hand centroid `(-0.00, ~0.6, ~40)`. The band renders stacked at
the venue origin.**

> **⛔ Correction to my X9 close-out.** I reported "the band stands on its
> marks — four members at four distinct world positions". That is true of the
> `Character` **objects' transforms**, which is what X9 measured, and false of
> the **drawn geometry**, which stacks at the origin. The arena drum-riser
> confirmation (+65.07 against X7's +65 predicted from asset bytes) likewise
> validates the *object* placement pipeline — a real and non-trivial result —
> but it is **not** evidence that the rendered band is spatially correct. I
> should have distinguished "the transform is right" from "the pixels are in
> the right place", and the stage crop I ratified showed a vocalist and
> instruments without my checking whether the other three members were behind
> them. This also explains, mechanically, the "only one member reads as a
> complete figure" symptom that X9 and X10 both carried unexplained.

**`Rot.cpp:299` fixed and demonstrated both ways:** the unfixed arm returns
`y = -2.000` (the already-written `x`), deviation 3.000e+00; fixed arm
0.000e+00. X360 A/B with both objects built in the same worktree: **9378/9378
`.text` sections byte-identical** (deltas confined to symtab/strtab and
`.debug$S`). A first A/B against a main-tree object was confounded by embedded
debug paths and **discarded rather than reported**. `Rot.cpp` has no unit in
`objdiff.json` — **not scoreable at all**, stated rather than implied.

**Retracted mid-lane, its own:** "recompose fails under animation, 3.473e+00 at
`bone_pelvis.mesh`" — actually `DriveSceneCharacters` polling 21 characters
while driving 8 (also the cause of an `rc=139`). Fixed loop, same clip:
0.000e+00, counts reconciling exactly.

**Did not land:** Direction-B rows 4 and 3b were **not** guarded — out of
budget, not out of agreement. Ceiling legs: **benign at medium confidence**,
and the lane was honest that the two frames it rendered to settle the question
did *not* settle it (legs cut by the frame edge, not by geometry); the verdict
rests on a wide frame showing the same crowd with full bodies, and it corrected
my proposed mechanism — a mezzanine fascia beam, not floor back-face culling.

### ⛔ Coordinator correction — the mitten warning is real but misattributed

X13 flags `RB3_HANDS_MITTEN` as default-ON and warns that any "the fingers look
right" claim, **including X12's close-up that I ratified**, is contaminated
until characterised ON vs OFF. I verified the flag: `Rnd_Wgpu_RB3.cpp:3763`
sets `sMittenOn = 1` when the variable is unset, so default-ON is correct.
**But `Rnd_Wgpu_RB3.cpp` is in `MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3` — the
Wii-flavor-only source set — and rb3-xenon builds `MILO_ENGINE_GPU_BACKEND=dc3`.
It is not compiled into any xenon target.** So:

- **My xenon E1 finger observation stands** — no mitten code runs in that build.
- **The warning lands on rb3-Wii instead, where it is sharper than X13 knew.**
  W34's E1 — *my own ratification* — cited "fingers render, branch-claws gone"
  as evidence the alias-unsafe `Multiply` fix worked. That claim **is**
  contaminated by a default-ON finger-lerping hack. The `Multiply` fix has
  independent support (detonation count 3650→0, byte-identical Wii match), so
  the fix is not in doubt; the *finger-appearance* half of the evidence is.
  This sharpens the existing Wave-35 mitten-retirement item rather than adding
  a new one.

Gates: fresh pre-land 18/18, rc=0, 0 SKIPs on the rebased tree; engine HEAD
exactly the pin with only the pre-existing foreign edit; post-rebase frame
byte-identical to X12's artifact.

## X14 — LANDED 2026-08-03: ★★ THE BAND DRAWS ON ITS MARKS (xenon main `6c8944ab`)

Doc: docs/plans/x14-band-placement-2026-08-03.md. Evidence:
`/home/free/tmp/laneX14/evidence/`. **Four distinct drawn centroids in two
venues**, skinned through the shipped `RndMesh::SkinVertex`; the baseline was
one point for all four.

**The load-bearing proof is not that the numbers look right** — it is that
`drawn z − slot z` is a **per-member constant identical to 3 d.p. across two
unrelated venues** (40.045 / 32.168 / 40.042), z being the yaw-invariant axis
where a translation prediction is valid. The `arena_01` riser measures **+64.99
authored vs +57.11 drawn**, and the 7.88 shortfall is *exactly* the drummer's
own hand-height offset measured independently in the other venue — two
measurements reconciling to **0.003**.

**Coordinator E1 PASS.** The frame shows four band members spread across the
stage — guitarist, vocalist, bassist, and a drummer seated at the kit holding
sticks — each with their instrument. The disclosed defects are plainly visible
and match the lane's account exactly: untextured pink skin, bald heads, and a
detached hairpiece mid-stage.

★ **Root cause: the repair was already in the tree, already default-ON, and had
never run.** All four members' `bone_L-hand.mesh` is the *same object* —
bit-identical pointer — but each member also owns a placed skeleton with 492
same-named bones. `RebindOutfitBonesToOwnSkeleton()` is called from
`BandCharacter::Poll()`, and **the band is never polled because it has no
clip**. So both charter milestones were one defect. As the lane put it: *three
lanes measured around a correct, default-ON fix because its own probe was
silent on the failing case.*

**`body_clips` ships empty by design** — the string appears in exactly one
shipped file and it is a runtime-filled container. The clips live at
`char/main/anim/<inst>/body/<gender>/gen/` (16 per set), loaded by tempo/genre
paths built at `BandCharacter.cpp:2703-2718`; no song → never requested.
Whether the merge would *succeed* is undecided, gated on X12's `ObjPtrList`
defect.

⛔ **X13's renderer citation is refuted, and my own correction of it is
confirmed.** The path is `Mesh_Wgpu.cpp:246-256` + `BoneSetup.cpp:219` (dc3
flavor); `Rnd_Wgpu_RB3.cpp` is not compiled into this binary at all
(`native/CMakeLists.txt:983` forces `dc3`, and no `Rnd_Wgpu_RB3.cpp.o` exists).
The conclusion survived but the mechanism differs — dc3 writes identity into the
object uniform outright rather than cancelling `meshWorld`. **Free consequence,
independently reached: `RB3_HANDS_MITTEN` lives in that TU and is therefore dead
here — it cannot contaminate any rb3-xenon frame.** That is exactly the flavor
split I verified when correcting X13, arrived at from the other direction.

**Three retractions, two of them the lane's own** — including "no shard", which
was hands-only: **hair blew up 7–14× and was caught by opening the PNG, not by
any number.** The shard turned out to be a *partial* rebind, not the
rotation-basis mechanism on file; fixed all-or-nothing per mesh.

**Did not land:** hair still draws at the venue origin (its `bone_hair_*` bones
don't exist under the member's own root) — four bald members and two hairpieces
mid-stage. The band renders **untextured**, and this is **proved pre-existing,
not introduced**: 419 texture failures / 40 `OutfitConfig` / 203 draws
*identical* in both arms — the stacking had been hiding it.
`CharWeightable::mWeightOwner` resolves NULL after `Load`, so
`BandCharacter::Poll()` SIGSEGVs; named, not worked around.

Gates: 18/18 at the branch point, 0 SKIPs. X360 **1796/1796 `.text` sections
byte-identical**, with a comparator that **carries a positive control and
refused to report until it detected a known difference** — its first version was
vacuous. Control frame byte-identical to X13's artifact; PNG determinism ×2.

### Coordinator action: `main` was broken, and I fixed it

X14 reported the post-rebase gate at 17/18 and **proved the break was not its
own** — `rb3-song` fails to link on a pristine detached worktree carrying none
of its commits. I confirmed and repaired it (xenon `57093d0a`): lane DP-1's
`BandSongMgr::Handle` match (`c6e8408f`) calls `Jukebox::Jukebox(int)`, which is
defined at `src/system/meta/Jukebox.cpp:7` — but `rb3-song` enumerates its
`meta/` sources explicitly and did not list that file. One line; verified by
building `rb3-song` to a clean link. **This is the fourth time a decomp lane has
broken `main` this way, and it is precisely the class the `CLAUDE.md` note I
added warns about**: the match build compiles `src/`, the native targets link a
superset, so a change can be perfectly matched while leaving a symbol only a
linker ever sees.

## X15 — LANDED 2026-08-03: `BandCharacter::Poll()` runs (xenon main `eed970f5`)

Doc: docs/plans/x15-poll-unblock-2026-08-03.md. Evidence:
`/home/free/tmp/laneX15/evidence/`. **4/4 members, rc=0, two venues — first
time in this ladder.**

★ **X14's root cause is refuted, by pointer identity.** X14 said
`d >> mWeightOwner` resolves NULL "when the named owner is absent". Three
measurements say otherwise: the slot is **not** among the 11,494 empty-name
`ObjRef` loads, it resolves to a real named object every time
(`left_hand.weight` / `right_hand.weight`), and `strum.dmidi @0x558339c1edf8` is
**bit-identically** the object that logged that owner at `Load`. **The owner was
resolved and then taken away.**

★★ **The real cause is a native-only teardown shortcut, and it is a *class*.**
`~ObjectDir` Phase 0 (`obj/Dir.cpp:119-135`, **`HX_NATIVE`-only**) calls
`NullifyAllRefs()`, which by its own documented design does **not** fire
`Replace()` — and `Replace()` is the only place these classes restore their
"null means me" invariant. **Retail's ring teardown does fire it.** Traced to
`FileMerger::PostMerge` ← `BandCharacter::StartLoad` by symbolized backtrace.
Fixing `Weight()` moved the fault immediately to `Character::mSphereBase`,
identical pattern, gdb-confirmed NULL. **The lane did not enumerate the rest of
the class and says so.**

**Did not land, and the judgement is the valuable part: X14's driver-side call
was NOT retired.** `Poll()` *does* perform the rebind — the band stays on marks
with the call fully removed — so it is redundant *for the rebind*. But `Poll()`
also **poses** the skeleton, and **that pose is undecided**: it carries X13's
exact contamination signature (3.565e+00 at `bone_pelvis.mesh` against X13's
retracted 3.473e+00, same bone), `handpose-recompose` FAILs, and X13's own
`Enter()` control makes it **worse** (hands 20–27 units off). Named cause: the
destroyed `CharWeightSetter`s are the blend-weight sources for these IK/MIDI
drivers. **Retiring the call would have adopted an unvalidated pose as every
future lane's default** — exactly the trap this ladder has avoided fourteen
times running. What *is* proved is **placement, not pose**: polled
`drawn z − slot z` is identical across two venues to 0.000/0.001/0.007/0.000 —
X14's invariant at the same precision, now under animation.

**Textures unreached**, bill re-derived rather than inherited (191/1511/48 all
reproduce) with two refinements: the required action is **registration**
(`OutfitConfig::Init()` is compiled but called by nothing; `"Can't make
OutfitConfig"` is purely a factory-registration test), and **X10's scatter-host
citation is stale** — it is `rndobj/TexBlender.cpp`, not `ui/UIListDir.cpp`.

**Hair unchanged, and X14's framing corrected.** X14 said the skip probe "names
them precisely, so the target list is mechanical" — it named the **mesh**, never
the **bone**. The real set is **7 meshes, 3 of them trousers**, across **two
bone-naming conventions** (`bone_hair_l-01` vs `bone_hair-L-01`), so a prefix
match written against one silently misses the other.

⚠ **Self-caught methodological failure worth carrying: the lane retracted three
of its own measurements because it deleted the `rb3-*` binaries for a fresh gate
and then ran three probes before the rebuild finished.** All returned empty, and
it "began reading the silence as findings" (rc=127). **Empty output from a
missing binary is indistinguishable from a negative result** — the same family
as the vacuous-green oracles this ladder keeps finding.

Gates: native **18/18, rc=0, 0 warnings, 0 SKIPs**, fresh both pre- and
post-rebase; `main` **not** broken this time; X360 `.text` **byte-identical
across 6 TUs** (2403/827/627/373/93/1796) with the comparator running a positive
control first, and **all 6 scoreable** in `objdiff.json`; PNG determinism ×2;
**default club frame byte-identical to X14's artifact** — the strongest
available non-regression, and the reason no new E1 was needed.

## X16 — LANDED 2026-08-03: the null class enumerated and repaired upstream (xenon main `72377b61`, `fa0d0914`)

Doc: docs/plans/x16-ownerptr-class-2026-08-03.md. Evidence:
`/home/free/tmp/laneX16/evidence/`.

**14 `ObjOwnerPtr` sites, not 2** — X15 fixed the two it walked into; twelve
were open, including `RndMesh::mGeomOwner`, `RndEnviron::mAmbientFogOwner`,
`Spotlight::mColorOwner` and seven `*Anim::mKeysOwner`. The census instrument
carries **4 positive + 1 negative control that abort the run on failure**, and
**it caught two bugs in itself**: comment pollution (a doc comment *quoting*
`mWeightOwner(this, this)` matched as a real seed), and a `= this`-only restore
regex producing **7 false negatives that both original controls sailed past** —
answered by adding a control per newly-learned spelling. That is the right
response to an instrument failing: not a fix, a new control.

★ **Why the `HX_NATIVE` shortcut exists is documented, not folklore** —
dc3-decomp `07bad0ab` (2026-03-20) plus an adjudicating design doc. All four
named consumers genuinely `delete this` from `Replace()`, verified in xenon
source (`Task.cpp:54`, `:117`, `FlowSetProperty.cpp:138`, `DirLoader.cpp:155`),
and all four are `ObjOwnerPtr`s — so **firing `Replace()` was never available**.
One correction to the record: the comment reads as though it fixed a diagnosed
crash, but **the delete-this use-after-free was never observed**; the real crash
was a stale `ObjPtr<Task>` in `TaskTimeline::Poll`.

**Upstream repair, in one place:** `ObjOwnerPtr` retains its constructor seed and
restores it in `NullifyObj()` — retail's post-`Replace` *value* with **no
consumer callback**, hence zero delete-this exposure. Provably scoped: the four
dangerous consumers seed `nullptr` (bit-identical behaviour) and no
`ObjOwnerPtr` in `src/` is ever seeded with a foreign object. **Proof is a set
identity, not a spot check:** NULL→self one-for-one (+7/+17/+7), `other`
**invariant** (nothing foreign stolen), and the zero-rows (vocalist, crowd)
**bit-identical across arms** — a negative control inside the measurement.

★ **X15's cause for the bad pose is refuted — while its measurements reproduce
exactly.** With every weight owner restored (0 NULL, 0 WOULD FAULT), the polled
pose is **bit-identical** to the unrepaired arm: same deviation to four figures,
same worst bone. So the blend-weight explanation falls, but X15's numbers
(7/17/0/7; `3.565e+00` at `bone_pelvis`) all reproduced. A clean separation of
"the measurements were right" from "the explanation was wrong". **X14's
driver-side call therefore stays — acceptance part two fails**, and the lane
said so rather than retiring it anyway.

**Lead for X17, stated as an untested hypothesis:** the pose residual sits on
**prop/trouser/hair bones** (`bone_mic_stand_bottom` at 61.7, `bone_legs-ring2`,
`bone_hair_back01`) — the same family as X15's unresolved-bone skip list. **The
pose defect and the rebind-skip defect are plausibly one defect.**

**Did not land:** textures/`OutfitConfig` and the 7 skipped hair/trouser meshes
untouched; the 12 newly-repaired sites are fixed by mechanism but not
individually exercised; two `SEEDED_NO_REPL` sites are unresolved residual risk.
**Retracted, its own:** two `--focus`/`--bone-audit` runs that **PASSed over a
zero denominator** — the third instance of that exact vacuity in this ladder —
and one bad `--focus band` argument that produced a FAIL looking like a
regression.

Gates: baseline gate PASS (so `main` was **not** broken by a decomp lane); final
**18/18 fresh, rc=0, 0 warnings, 0 SKIPs**; X360 `.text` **byte-identical across
6 TUs**, sole delta `.debug$S`, comparator positive-controlled first, **all 6
scoreable**; PNG determinism ×2; **both frames opened — no shards, no
explosion**.

## X17 — LANDED 2026-08-03: one condition, sibling symptoms (xenon main `82db5d93`)

Doc: docs/plans/x17-pose-residual-2026-08-03.md. Evidence:
`/home/free/tmp/laneX17/evidence/`.

★ **The lane split X16's hypothesis into two claims, because it was two claims —
and they came out opposite ways.**

**Causal claim REFUTED, decisively.** Three arms from one binary: rebind at full
scope, at the shipped torso scope, and disabled. Every per-figure recompose line
is **byte-identical across all three** — worst deviation `6.172e+01` on
`bone_mic_stand_bottom.mesh` in each. What makes this decisive rather than
merely null is the **positive control inside the same A/B**: it moves all four
members' hand centroids from four distinct authored slots onto a **single
collapsed point**. The suspect is maximally potent and the residual does not
move by one ULP. Mechanism agrees and was read *second*: the rebind only calls
`RndMesh::SetBone`, touching no transformable's local, world or parent.

**Set claim CONFIRMED, more strongly than X16 knew.** Prior lanes reasoned from
six per-figure argmax names; X17 dumped the whole population — **123 of 7380
admissible bones (1.7%)**, and **all 60 distinct names are hair or trouser bones
in both naming conventions**, plus exactly three others (`bone_pelvis`,
`bone_mic_stand_bottom`, `bone_mic`). Every bone in X15's unresolved list is
present.

**Verdict: neither yes nor no — one structural condition** (these bones are not
resolvable under the member's own skeleton) **with three sibling symptoms** (the
rebind skip, the recompose residual, X15's floating hairpieces). Neither is
upstream of the other.

**Pose still does not validate; X14's driver-side call stays — the third lane to
decline it.** The lane narrowed "the pose is untrustworthy" to 123 bones and
~57 real sites and refuted the leading explanation, but **also found a reason to
doubt the gate itself** — and refused to act on it: *"an unproven doubt is not
grounds to adopt an unvalidated pose as the default."* The doubt is worth
carrying: `RecomposeAdmissible` screens constraints but **not** `SetWorldXfm`
publication, so X13's `Enter()` control making `bone_R-hand` deviate **may be IK
working correctly rather than a worse pose**. That ambiguity sits directly under
the gate blocking retirement.

**Handoff:** the residual is **chain-structured** — ~57 ROOT sites, 66 inherited
propagation — rooted at three attachment points (`bone_hair.mesh`,
`exo_pelvis.mesh`, the `Character` object itself), which **recharacterises
`bone_pelvis` and `bone_mic_stand_bottom` as *attachment roots*, not misposed
body bones**. And explicitly: **fixing the rebind will not fix the pose** — arm C
already proves it will move the skip set and the floating hair and leave the
residual untouched.

**Did not land:** textures entirely unreached (fifth lane), X15's bill inherited
and not re-verified. Whether every ROOT bone is frozen is **unresolved** and
said so — six sampled ROOTs are bit-identical across arms but 59 hair/leg bones
are among the 1309 movers, so the population splits and the name-keyed diff is
confounded by names recurring across figures. **Retracted, its own:** a mover
count taken over a diff field that differs between arms by construction —
re-measured, the count held, *"but that was luck rather than method."*

⚠ **Fourth vacuous-pass instance in this ladder:** `RB3_BAND_PLACE=1` is
required or the oracle measures a **bandless scene and PASSes**. It caught the
lane on its first run, and what exposed it was X13's geometry gate — i.e. the
structural fix for this exact class, working as intended.

Gates: **18/18 fresh, rc=0, 0 SKIPs**, binaries deleted first; baseline gate
confirms `main` **not** broken by a decomp lane. Change surface is **one
native-only file**, so the X360 blast radius is zero *by construction* — **"there
is no A/B to run and I say so rather than implying coverage."**

## X18 — LANDED 2026-08-03: the gate was wrong (xenon main `c5b2e4dc`, `c1cb4ba3`)

Doc: docs/plans/x18-gate-and-roots-2026-08-03.md. Evidence:
`/home/free/tmp/laneX18/evidence/` (48 files).

★ **Gate verdict: OVER-REPORTING — settled, not suspected.** X17's doubt was
correct. A last-writer tag on `mWorldXfm` (native-only; **all five** writers
tagged and verified by grep as the only ones) shows **123 of 123 deviating
bones are `PUBLISHED`**, while **4351 `COMPOSED` and 2793 `LOADED` bones deviate
zero**. Not a tautology: 236 published bones exist and 113 of them *don't*
deviate. The key deduction preceded any run — `WorldXfm()` returns the cached
world when `!mDirty`, so the check is structurally capable of firing only on
publication or dirty-propagation failure.

**Three controls, because a gate never seen to fail proves nothing:** `SetLocalXfm`
+5 stays `COMPOSED` and both gates pass; `SetWorldXfm` +5 flips to `PUBLISHED`
and the old gate **fails** while the corrected one passes; and a **forged stale
bone makes the corrected gate fail on demand** at 5.6e+01 — *"the one that makes
the PASS mean anything."*

**X17's three attachment roots dissolve.** Resolving the captured return address
names the publishers: `CharHair::SimulateZeroTime()` ×120 (92 hair + 28
trouser), `CharIKHand::Poll()` ×2, `CharIKScale::Poll()` ×1. The "~57 ROOT sites
to attack" are **the correct output of three engine subsystems — there is
nothing to fix there.** So three lanes were blocked by an instrument artifact,
and the residual they were chasing was never a defect.

★ **The real blocker was hiding behind it, and it is not the pose.** Removing
X14's driver-side call collapses the band's head, eyebrows and fingernails to
the origin — visible in the frame. With `RB3_SKEL_REBIND_FULL=1` the same
removal is **byte-identical** to keeping it. **The blocker was always the rebind
*scope*.** The lane did not flip that shipped default: a consequential shared-
`src/` behavioural change deserving its own lane and X360 A/B. Right call.

**Coverage stated explicitly, and this is the sentence to remember:** the
corrected gate is **algebraic**. It asserts only that composed bones compose
(dirty-propagation intact). It says **nothing** about whether a published world
is the *right* world — *"an IK solver publishing a hand into the floor passes
it."* **The pose is un-invalidated, not validated.**

★★ **Biggest finding, and it reaches backwards through the campaign** — caught
by chasing a control's *missed* prediction (C2 predicted +P, delivered +8P):
**all four band members and their `outfit` sub-Characters resolve
`bone_L-hand.mesh` to one shared object.** 7380 admissible slots are **3306
distinct objects**; the 123 deviating are **63**. ⚠ **Five lanes of per-figure
band numbers are therefore not independent measurements.** Whether this is
genuine sharing or a `FindBoneNamed` artifact is **unresolved** — instrument
committed, not run to conclusion. This does not obviously overturn X14's
four-distinct-centroid result (which post-dates its own rebind fix), but any
per-figure band statistic in X13–X17 should be re-read with it in mind.

**Textures unreached for a sixth lane — but the inherited claim was re-verified
and is WRONG.** `OutfitConfig::Init()` *is* called, from `Band.cpp:114`. The real
blocker is that it does `Register()` **plus** three static `New<>` calls, and the
native driver never calls `BandInit()`. **The 191/1511/48-symbol bill remains
unverified by anyone**, five lanes after it was first quoted.

Gates: **18/18 fresh, rc=0, 0 warnings, 0 SKIPs**, all binaries deleted and
relinked, re-run after rebase; `main` **not** broken by a decomp lane. X360
blast radius **zero in emitted code — measured, not assumed**: all
`.text`/`.data`/`.rdata`/`.bss` byte-identical across `Trans`/`TransAnim`/
`TransProxy`, deltas confined to `.debug`. A naive `cmp` said DIFFERS (143k
bytes) and the lane controlled for it by **rebuilding unmodified source twice to
prove the object is byte-reproducible** before trusting the comparison. PNG
determinism ×2; base frame byte-identical to X17's artifact. Three of its own
mid-lane errors retracted in the doc.

## X19 — LANDED 2026-08-03: the geometric oracle was measuring the wrong bone (xenon main `122b0350`)

Doc: docs/plans/x19-sharing-and-scope-2026-08-03.md. Evidence:
`/home/free/tmp/laneX19/evidence/` (33 files).

★★ **Sharing verdict: BOTH — which is why counting could never settle it.** Every
player figure carries **two** bones named `bone_L-hand.mesh`. `FindBoneNamed`
returns the first — the **shared unplaced** one at `(-21.749, 0.197, 44.274)`,
identical across all eight band entries — and **passes over the member's own
placed bone** (chain roots `player0`…`player3`, four distinct pointers at four
placed worlds). The prediction was named in advance from X14 §1.2, not fitted
after the fact.

⛔ **The consequence, and it corrects a great deal of what I have reported:** the
hand-mesh gap gate — **the only geometric oracle in this ladder** — keys off
`lh`/`rh`. **Five lanes of band hand gaps (32–128u) were read off a bone
belonging to no member.** Measured against each member's *own* bone, all four
`hands_naked.mesh` are **0.000 — INSIDE**. The four apparent residuals
(15.6/10.6/20.3/0.5) are **right** wrist accessories measured against the
**left** hand bone: expected geometry, not defects.

**Blast radius, stated precisely** (7380 slots = 2607 own-rooted + 4773 foreign;
**123 deviating = 3 own + 120 foreign**):

- **Stand:** X14's four-distinct-centroid placement (measured via `SkinVertex`,
  which never touches `FindBoneNamed`, and independently reproduced in the poll
  arm); X18's writer classification and publisher attribution; X17's
  ROOT/inherited structure — whose counts reproduce **exactly** and are now
  *explained*.
- **Do NOT stand as per-figure:** every `bone_L/R-hand` number and gap gate from
  X12–X17, including X15's `1.09e+01` and X17's `1.097e+01`/`1.102e+01` — one
  object read eight times.
- **New structural fact:** the members' own skeletons are essentially **clean**
  (3 deviating of 2207). The residual lives on the shared unplaced skeleton — so
  `CharHair` and the IK solvers are publishing onto the *shared* skeleton.

★ **X14's driver-side call is RETIRED — and the blocker was never a shared-`src/`
default.** The `quiescent → FULL` `setenv` sat **inside** the `!pollOnly` block
alongside the call, so the "poll only" arm skipped the *scope decision* as a side
effect. Hoisting it fixed the collapse. **Methodological catch worth carrying:
the old guard meant `RB3_BAND_POLL=1` ran *both* — so the arm meant to prove
`Poll()` works was in fact carried by the direct call.** Retirement is real:
poll-with-call-removed is byte-identical both to keeping it and to X18's
artifact, with head, eyebrows and fingernails on player0 rather than the origin.
**PNGs opened**; the pre-fix arm is visibly sparse and headless.

⛔ **Correction to my own charter language:** three lanes (mine included) called
the torso rebind scope "shipped behaviour". **`RebindOutfitBonesToOwnSkeleton()`
is entirely inside `#ifdef HX_NATIVE` and does not exist in the X360 build —
there is no retail default to depart from.** The default was left unflipped for a
better reason: FULL scope under a driven clip is **unmeasured**, and X14 §5
refuted the shard mechanism explicitly *"in the un-animated case"* only.

★ **Textures reached for the first time in seven lanes, with a precise cause.**
The band's drawn skin materials carry the **authored `dummy_torso/legs/feet.tex`
placeholders**, plus **NULL** on `head_naked.mat` — a fourth state nobody had
enumerated (not null, not unuploaded, not shader tint). Chain measured in the
lane's own logs: no `OutfitConfig` registration → 40 `Can't make OutfitConfig` →
`SyncOutfitConfig` never runs → `SetSkinTextures` never runs. **Negative proof:
its six "could not find" warnings fire zero times.** Refuted by measurement:
"missing assets"; "upload gap" (0 GPU-texture failures); and **X14's "419
texture-resolution failures", which I recorded — the real count is 417 and
*none* are texture failures** (367 are cache-key notices emitted *after*
successful deserialization). The 191/1511/48 bill: *"I did not re-derive and do
not repeat"* — only the 48 is corroborated. Seven lanes of quoting it, finally
retired properly.

**Coordinator E1 PASS:** four upright band members distributed on the stage with
the drum kit, crowd along the mezzanine railing. Still pink — consistent with the
placeholder-texture finding.

**Did not land:** the scope flip (deliberate, on evidence); **FULL scope under a
driven clip unmeasured** — the sole reason the default stays torso; the crowd
negative control for the gap gate is **unavailable** (crowd figures carry no hand
mesh) and was *stated, not claimed*; the exact pink mechanism (placeholder texel
vs shader tint) undetermined; tattoo heads at origin in **both** arms
(pre-existing). Three own retractions recorded, including a mislabelled evidence
PNG caught by `md5sum`.

Gates: baseline **18/18 fresh** (so `main` **not** broken by a decomp lane),
final post-rebase **18/18 fresh, rc=0, 0 SKIPs, 18/18 relinked**; PNG determinism
×2 on all three arms; default and poll arms byte-identical to X18's artifacts.
**Zero shared-`src/` edits — so no X360 A/B exists to run**, and `main_render.cpp`
is unscoreable (not in `objdiff.json`); said rather than implied.

## X20 — LANDED 2026-08-03: `OutfitConfig` registered; band still pink (xenon main `bd8d88f7`, `18d4adfb`)

Doc: docs/plans/x20-textures-2026-08-03.md. Evidence:
`/home/free/tmp/laneX20/evidence/`.

**Registration works** — `Can't make OutfitConfig` **40 → 0**, 40 instances,
exactly 4 `skin.cfg`. **The band is still untextured: acceptance not met**, and
the lane says so first.

★ **The bill that stalled five lanes is finally derived, and the lane refused to
launder a coincidence.** Referencing `OutfitConfig::Init()` leaves **exactly 48**
undefined symbols: 36 `extern Symbol` globals, 11 `BandPatchMesh` members, one
`gRB3OutfitComposeActive`. The 36 are declared **~7083×** across
`utl/Symbols{,2,3,4}.h` and **defined nowhere in the tree** — they survive
natively only because `--gc-sections` drops the referencing section. Two
*already-existing* macro arms retire them: 48 → 16 → 12 → links. **Total cost: 2
compile definitions, 1 native-only TU, 1 line.** And explicitly: *"the 48
numerically matching the inherited bill is a coincidence I refuse to launder into
corroboration — X19's 48 was a different quantity"*; `191`/`1511` **remain
unmeasured by anyone**, ten lanes after first being quoted.

★★ **The control missed its prediction, and that is the finding.** Textures did
not change. **Registration is necessary and not sufficient** — X19's chain was
right in *every link it named* and was **not the whole chain**. This also
weakens X19's negative proof: `could not find == 0` is *still* 0 after
registration, so **a failure-only predicate cannot separate "never ran" from
"ran and succeeded"**.

**Next link precisely located:** `SyncOutfitConfig` has exactly two callers,
**neither reached**. One candidate was excluded **by measurement** — the lane
predicted the deform-clip guard blocked it and **refuted its own prediction**
(`male=male female=female`). And `SetSkinTextures` binds
`*_skin_diffuse_output.tex` (the compose render target), not the authored
diffuse — **all 22 RTs exist**, so the asset side is complete and the gap is
purely a call path.

⚠ **Handoff caveat that matters most, and it is the flavor split again:**
binding the right RT may still give **blank** skin, because the compose pass
that fills them is gated by `gRB3OutfitComposeActive`, whose engine home
`RB3Quad.cpp` lives in the **`rb3` GPU backend this build does not link**
(xenon is `MILO_ENGINE_GPU_BACKEND=dc3`). **Reaching `SetSkinTextures` and
getting correct skin are two milestones.** This is the third time the Wii/DC3
flavor split has silently shaped a xenon result — after `RB3_HANDS_MITTEN` and
X13's renderer citation — and it may need an engine change request or a dc3-side
equivalent, which is coordinator territory.

★ **The vacuous-pass machinery this ladder built caught a real false negative
prospectively — the first time it has done so.** The lane's first `OutfitConfig`
census used `ObjDirItr` (which does not follow `SubDirs()`) and reported **0**
where the truth was **40**: *"it would have 'confirmed' registration failed at
the moment it succeeded."* It printed **VACUOUS** over its zero denominator and
was re-pointed at `CollectDeep`. Five instances of this class have now been
found; this is the first one the guard stopped *before* it became a wrong
conclusion.

**Also refuted:** X9's in-tree blocker note — 125 `BandPatchMesh` symbols were
*already* linked via `TexBlender.cpp:383`'s unconditional scatter edge; X9 had
read `Rot.cpp:431`, which carries the same edge under `#if !HX_NATIVE`.

**Coordinator E1:** the frame is a fully lit, textured club interior with ~20
**textured** crowd figures on the balcony in distinct clothing, and four
**untextured pink** band members upright on four distinct marks, heads present,
no shards. Gate (f) reported as a **quantified non-identity** rather than "no
regression": 455 differing pixels of 921600 (0.0494%), max channel delta 130.

**Did not land:** milestone 2 (the 120 shared-skeleton publications) **not
advanced at all** — the lane read the dead-ends doc, attempted none of the four,
and **proposes no fifth**; milestone 3 (FULL scope under a driven clip) not
reached for a **seventh** lane; X19's `FindBoneNamed` other-call-sites item
untouched. Two `BandPatchMesh` members are **counted stubs measured at 0 hits
every run**, so nothing is fictional today.

Gates: baseline **18/18 fresh** (`main` **not** broken by a decomp lane);
post-rebase **18/18 fresh, rc=0, 0 SKIPs, all 18 relinked**; PNG determinism ×2;
gate-built and post-rebase binaries both reproduce the same md5. **Zero
shared-`src/` edits → X360 blast radius zero by construction**; none of the four
touched files can be scored at all.

## X21 — LANDED 2026-08-03: two independent causes for the pink band (xenon main `a92a80a9`, `7f4e7289`)

Doc: docs/plans/x21-compose-path-2026-08-03.md. Evidence:
`/home/free/tmp/laneX21/evidence/`.

★★ **X20's central handoff is refuted, and the irony is instructive: X20 warned
that a failure-only predicate cannot prove a success, then reasoned about the
call graph without building a positive one.** X21 built one. **`SyncOutfitConfig`
fires 336 times** (42 with `sym='skin'`) and **`SetSkinTextures` runs on all four
members** with `skin.cfg=FOUND`, correct genders, all five materials and all
three render targets found. **The call path was already complete when X20 handed
it forward as "the whole remaining distance."**

**The band is pink for two reasons neither lane had, and they are independent:**

1. ★ **The census was reading names, not identities — the sixth instance of that
   class on this ladder.** The "58 skin material instances" are `(mesh, mat)`
   pairs over **11 distinct `RndMat` objects**. `SetSkinTextures` rebinds all four
   per-member materials correctly, but **every visible body mesh points at one
   shared material in `char_shared.milo` that no member touches** — only the two
   tattoo meshes use the rebound ones. This is **X19's shared-object defect one
   subsystem over**, it is **independent of the compose work, and it will survive
   it**.
2. **The compose pass never dispatches at all.** `Rnd::DrawPreClear`'s
   list-selection arms are **swapped** relative to the rb3-Wii oracle (offsets map
   1:1). Measured `mPreClearDraws=0 mDraws=40 listUsed=0`, `Compose` 0 calls —
   **against 734 lines from the same predicate in the same run, so the zeros are
   absences, not silence**.

**Milestone 2 decided, and it is engine-side.** Correcting the polarity repairs
dispatch (0→40 calls, 0→44 composes) **and then kills the frame**: a WebGPU
pass-nesting violation, coverage 0.00%. So it shipped **opt-in, not default-ON** —
*"necessary, not sufficient, the same shape X20 hit one link earlier."* dc3 has
RTT and does call `DrawPreClear()`, but its `DrawRect` drops `mat->GetColor()`
entirely, with no `colorMod` awareness, no two-texture pass and no pipeline cache.

**Two inherited things checked rather than quoted:**
`MILO_ENGINE_GPU_PLATFORM_SOURCES_DC3` **does not exist** (zero occurrences
engine-wide; the dc3 list is unsuffixed), and **X18's four-flag worktree recipe
is insufficient** — the compilers must be *pinned*, or the gate wipes the cache
and SKIPs three engine targets. Its first baseline read 15/18 with 3 SKIPs and
**the 0-SKIP rule caught it**. I corrected my `CLAUDE.md` note accordingly
(xenon `a46979fb`).

`Rnd::DrawPreClear` **cannot be scored at all** (`target_size=0`, no ICF alias,
absent from the asm) — stated, not implied. All three shared TUs verified
unchanged at unit granularity with both objects built in the same worktree.

**Did not land:** milestone 3 (the 120 shared-skeleton publications) untouched
for a **third** lane — read the dead-ends doc, attempted none of the four,
**proposed no fifth**.

### Coordinator decision on the two engine change requests

Both are well-scoped, with per-consumer risk and a sequencing note. My call:

- **CR-1** (move `gRB3OutfitComposeActive` out of the rb3-flavor TU, ~3 lines):
  zero behavioural risk to rb3-Wii and dc3, but **it breaks rb3-xenon's link
  unless X20's deliberately-strong stub is deleted in the same change**. That
  cross-repo sequencing plus the pin bump is coordinator work, not a lane's.
- **CR-2** (implement the compose in dc3's `DrawRect`, ~150–190 lines): the real
  risk is **not** the compose — it is folding `mat->GetColor()`, which changes
  modulation for *every* dc3 `DrawRect` caller (UI, postproc, vignette). Must be
  independently gated. Also must resolve the pass-nesting violation or it lands
  dead.
- **Sequencing consequence that decides the order of work:** CR-1 alone buys
  nothing observable, and **§2's shared-material defect is independent of both
  CRs and will leave the visible meshes pink even after CR-2 lands.** So the
  xenon-side material defect is the higher-value next step, and the CRs want a
  dedicated engine lane afterwards.

## Next (not yet chartered)
- The undecided polled pose (blocked on the above); band textures
  (`OutfitConfig` **registration**, 48-symbol bill re-verified); hair (7 meshes,
  2 naming conventions); Direction-B rows 4 and 3b; camera shots; X6's engine
  change request; `video_05` `rc=1`; audio; `ThreadCallInit`.
- **rb3-Wii (Wave-35):** characterise `RB3_HANDS_MITTEN` ON vs OFF before any
  further finger-appearance claim (W35-0b).
- ⚠ The foreign uncommitted `FxSendNative.cpp` engine edit is now **twelve
  lanes** running. Owner decision still outstanding.
- **band3/venue-unblock roadmap review — DONE** (xenon main `7842bdcd`, docs
  only): new `docs/plans/band3-native-unblock-priority-2026-08-02.md` +
  dated corrections to `bandobj-port.md` (its absent-source claim has been
  false since 2026-05-26; 52 cpp/60 h, 51 pins not 7 — and its "defer
  BandCharacter" advice was the costly part) and `paths-to-100/20`;
  `paths-to-100/03` explicitly CORROBORATED rather than amended, which is the
  right call to record. **Verdict on the question I posed: the venue milestone
  needs ZERO new ports and ZERO stubs.** All 13 real TUs are pinned *and
  scored* (67.84%–100.00%; the 14th, `BandConfiguration`, has no class
  anywhere — only a factory-only shim at Band.cpp:66-73; `SynthFader` is class
  `Fader` via OBJ_CLASSNAME). A stub buys nothing on either path: DirLoader
  already re-syncs a miss via `ReadDead`, and on the persistent path a
  short-reading stub desyncs identically to a miss, just later. The milestone
  is 13 TUs onto a link line + 14 registration lines; X4b's WorldCrowd/UIColor
  (two lines, zero source changes) is the empirical proof.
- **Open question handed to X4c (highest leverage on the board):** the
  factory-miss message is emitted from **two** sites with a byte-identical
  format string — `DirLoader.cpp:929` and `world/Instance.cpp:232` (verified)
  — so X4a's log cannot be attributed to a path retroactively. A one-token
  disambiguation decides whether `BandCamShot`'s 611 misses (90% of the
  remaining 676) are recoverable top-level misses retired by a one-line bind
  to the already-registered `CamShot::NewObject` (`BandCamShot : public
  CamShot`, verified), or a genuine persistent-object wall. Caution recorded
  with the lane: a base-class substitute runs the base `Load()` and short-reads
  the derived payload — safe only if `ReadDead` re-syncs it, i.e. only on the
  top-level path.
- **Pin currency:** xenon now at 138e1606. rb3-Wii + dc3 still 2ea8e343;
  functionally unaffected (dc3-flavor file) — bump when a Wii-relevant
  engine change next lands.
