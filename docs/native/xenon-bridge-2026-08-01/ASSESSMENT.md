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

## Next (not yet chartered)

- **X11 — the three genuinely-empty meshes** (`head`, `hands_naked`,
  `eyebrows*_resource`). X10's scoping: start from `RndMesh::LoadVertices` /
  `CopyGeometry`, the only two places geometry is ever populated.
- **Direction B remediation:** 20 files linked into targets that compile none
  of their module, incl. D3D9 into a WebGPU target — latent behind
  `--gc-sections`, worth closing before it stops being latent.
- Camera shots (the real `BandCamShot` TU) — gates crowd visibility too; X6's
  engine change request; `video_05` `rc=1`; audio; `ThreadCallInit`;
  `BandConfiguration`/`BandPatchMesh` (48 symbols) still owed and unscoreable.
- ⚠ The foreign uncommitted `FxSendNative.cpp` engine edit is now **seven
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
