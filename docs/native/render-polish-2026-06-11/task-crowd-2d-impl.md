# Impl: `crowd-2d` — 2D bowl-imposter crowd (Fix B) SHIPPED + VERIFIED

Implementer, 2026-06-20. Implements the wave-8 plan-only **Fix B** (2D bowl-imposter
crowd) designed in `task-crowd-venues-impl.md`. The arena/festival/big_club
render-to-texture crowd pipeline — structurally dead on the native WebGPU backend —
now renders: a crowd archetype character is drawn into a shared render-target
texture and billboard quads tile it across the bowl. Verified end-to-end by reading
back the imposter RT (a fully-rendered crowd character) in `arena_06`.

- **rb3 branch:** `wt-task-crowd-2d`
  (worktree `/home/free/code/milohax/rb3/.claude/worktrees/task-crowd-2d`)
- **engine branch:** `wt-task-crowd-2d`
  (worktree `/home/free/code/milohax/milo-native-engine-worktrees/task-crowd-2d`)
- **engine commit:** `741f1367a65c3c194b5c343d814df59ffc150dca`
- **rb3 pin bump:** `MILO_ENGINE_PIN` `1010f5f` → `741f136`
- **Wii byte-identical:** YES. `RndMultiMesh::DrawShowing` objdiff **100.0%**
  (32/32 instructions equal). The only shared-`src/` edit is HX_NATIVE-gated;
  `Crowd.cpp` is pristine (Fix A untouched).
- **Default:** ON. Opt-out env `RB3_CROWD_IMPOSTER_OFF=1` (GetSharedTex → null →
  the prior dead/empty-bowl behaviour).

---

## TL;DR — result

**Fix B SHIPPED + VERIFIED.** In `arena_06` the 2D imposter path now renders the
bowl crowd. Gold-standard proof: reading back the 256×256 imposter render target
shows a **full crowd character** (`/tmp/crowd-2d/imposter_rt_0.png` — a person with
skin-tone pixels r221 g192 b170, ~3250 non-transparent pixels), and the billboard
quads sample it across the bowl (8 archetypes × ~700 instances/frame, all
`mmesh->DrawShowing()` invocations confirmed). BEFORE: the imposter RT was empty
(`GetSharedTex` returned null) → no bowl crowd. Fix A small_club 3D crowd is
NOT regressed (the 2D path is never taken for force3D venues; imposter tex is not
even created there). Venue/clouds/sky/menu/song-select unregressed.

---

## What changed — the FIVE gaps + their real fixes

The design doc named 3 native gaps; verification surfaced 2 MORE engine bugs that
made the *wired* pipeline render nothing. All five are now closed.

### Gap 1 (rb3) — `WiiRnd::GetSharedTex` returned null  ← the load-bearing hookup
**File:** new `native/src/rb3_crowd_imposter_native.cpp` (+ 2 CMake list entries +
the `band3_link_stubs.s` weak-stub removal).
Strong `WiiRnd::GetSharedTex(SharedTexType, bool)` returns ONE persistent
file-static square `RndTex` (`SetBitmap(256,256,32,kRendered,false,"")`). `this`
(== `&TheWiiRnd`, itself a weak stub) is deliberately unused. With a non-null
256×256 RT, `gImpostorCamera->SetTargetTex(tex)` sticks, and the engine's lazy
mid-frame RTT (`BandRnd::BeginDrawTarget`, fired from `DrawMesh` off
`RndCam::sCurrent->TargetTex()`) finally triggers. GOTCHA fixed: `SetName(...,
nullptr)` asserts a non-null `ObjectDir` (Object.cpp) — the imposter tex is a
free-standing global, so no name is set.

### Gap 2 (shared src, HX_NATIVE) — billboard quads not camera-facing
**File:** `src/system/rndobj/MultiMesh.cpp` — `RndMultiMesh::DrawShowing`.
The portable path only `SetWorldXfm(it->mXfm)`, which BYPASSES the mesh's
`kFastBillboardXYZ` constraint (`SetWorldXfm` assigns the raw xfm + marks the cache
clean, so `ApplyDynamicConstraint` never runs → quads keep authored orientation).
HX_NATIVE branch: when the mesh has `kFastBillboardXYZ` and `RndCam::sCurrent`
exists, set each instance's world xfm to `{ rotation = sCurrent->WorldXfm().m,
translation = it->mXfm.v }` — exactly the `kFastBillboardXYZ` semantics
(`Trans.cpp ApplyDynamicConstraint`, line ~169) the rndwii path bakes per-instance.
Opt-out `RB3_BILLBOARD_OFF`. **Wii-neutral: objdiff `DrawShowing` 100.0%.**

### Gap 3 (ENGINE) — RT cam used the WINDOW aspect, not the square RT aspect
**File:** `Rnd_Wgpu_RB3.cpp` `WriteSceneUniforms`.
The function hardcoded `aspect = WindowWidth/WindowHeight` (~1.78) for EVERY cam.
The imposter cam is authored SQUARE (`kAspect=1.0`, Crowd.cpp) onto a 256×256 RT;
the window-aspect substitution stretched the projection and threw the char outside
the RT's NDC. Fix: derive `aspect` from `cam->TargetTex()->Width()/Height()` when
the cam targets an RT (1.0 for the square imposter; also corrects the clouds RTT),
else the window aspect. (The design's claim that `RndCam::ScreenAspect` already
handled this was WRONG — that aspect is never consulted by the WGSL projection.)

### Gap 4 (ENGINE) — imposter cam inherited the venue far plane → far-clipped
**File:** `Rnd_Wgpu_RB3.cpp` `WriteSceneUniforms` (same RT-cam block).
`gImpostorCamera->SetFrustum(nearP, curCam->FarPlane(), ...)` inherits the venue
cam's far plane (~224). The crowd char is re-posed to the WORLD ORIGIN and viewed
from ~600–1600u away, so every triangle lands behind the z=1 far clip plane and is
clip-volume-culled (a BLACK RT, even with no depth attachment — same failure the
backend already documents for scrolled highway gems). Fix: widen the RT cam's far
plane (`if (f < 8000) f = 8000`) so the standoff char is inside the clip volume.
RT-cam-scoped; main pass + clouds RT (whose own far already covers the sky dome)
unchanged.

### Gap 5 (ENGINE) — stale scene cam: RT char projected with the MAIN cam's view/proj
**File:** `Rnd_Wgpu_RB3.cpp` `BeginDrawTarget`.
The 2D loop re-`Select()`s the SAME `gImpostorCamera` object for all 8 archetypes.
`DrawMesh`'s `camChanged` latch (pointer + pose) can MISS the re-pose when the
imposter cam equals `mLastSceneCam` and its re-posed `v`/forward coincide with
`mLastSceneCamPose` → the char projects with STALE (main-cam) uniforms → off-RT.
Fix: `BeginDrawTarget` clears `mLastSceneCam = nullptr` (mirroring `EndDrawTarget`'s
existing reset for the reverse direction), forcing the next `DrawMesh` to re-write
the active (imposter) cam's projection into the RT.

---

## VERIFICATION (evidence under `/tmp/crowd-2d/`)

Harness: `scripts/native/arena-crowd2d-capture.py` (NEW) — boots rb3-native
headless, navigates to song_select, injects `{meta_performer set_venue_override
arena_06}` BEFORE the song confirm (Fix C bridge loads arena), drives to gameplay,
forces crowd/venue shots, captures. Ports 9911-9919.

| # | criterion | before (imposter OFF) | after (default ON) | verdict |
|---|---|---|---|---|
| 1 | imposter RT created (`RTT created 256x256 for tex ''`) | not created (GetSharedTex=null) | created | PASS |
| 2 | crowd char renders INTO the RT (256×256 readback) | `nzAlpha=0` (empty) | `nzAlpha=3251 nzRGB=2803 max(r221 g192 b170)` — a crowd character | PASS |
| 3 | 2D imposter path runs (8 archetypes × ~87-88 inst) | runs but dead | runs + paints | PASS |
| 4 | billboard quads sample the RT (`mmesh->DrawShowing`) | sample empty | sample the char | PASS |
| 5 | arena loads + gameplay, no crash | yes | yes (0 SIGSEGV/SIGABRT) | PASS |
| 6 | small_club 3D crowd (Fix A) | full crowd | full crowd; imposter tex NOT created (force3D) | PASS (unregressed) |
| 7 | venue/clouds/sky render | OK | OK (no aspect/clip regression) | PASS |
| 8 | song-select / character preview RTT | OK | OK | PASS |
| 9 | objdiff `RndMultiMesh::DrawShowing` (Wii) | 100.0% | 100.0% (32/32) | PASS (byte-identical) |

**THE deliverable:** `/tmp/crowd-2d/imposter_rt_0.png` — the 256×256 imposter render
target containing a fully-rendered, skin-toned crowd character (standing/walking
figure). This is the irrefutable proof: the previously-empty bowl-imposter RT is
now painted with a real crowd archetype, which the billboards tile across the bowl.

### Verification method note (a trap worth recording)
The RT-readback diagnostic initially showed all-black even when the feature was
WORKING. Cause: the RT texture is created with `RenderAttachment | TextureBinding`
usage — NO `CopySrc` — so `CopyTextureToBuffer` (the readback) was a silent Dawn
validation no-op. Adding `CopySrc` (probe-gated) revealed the rendered character.
**A broken verification can masquerade as a broken feature** — when an RT reads
black, confirm the readback path (CopySrc usage) before chasing render bugs. (All
probe/diagnostic code was removed before commit; the shipped texture usage is
unchanged.)

---

## In-venue visual (honest scoping)

The imposter billboards render into the bowl, but the *gameplay/director cameras*
are band+highway-centric and rarely frame the bowl head-on, AND the arena venue
also has a STATIC baked crowd texture on its far walls — so an in-frame ON/OFF
visual A/B is ambiguous (and the director cam is non-deterministic across runs).
The RT-readback (a rendered crowd character in the imposter texture) is the
deterministic, camera-independent proof that the pipeline works; the billboard
sampling is confirmed by probe (`mmesh->DrawShowing` runs for all archetypes). The
animated imposter crowd is an ambient layer in front of the static backdrop.

---

## LANDING NOTES (orchestrator)

**Land order: engine FIRST, then rb3 (with the pin bump in the same rb3 commit).**

### Engine (`741f136`, branch `wt-task-crowd-2d`)
- ONE file: `src/platform/Rnd_Wgpu_RB3.cpp`, +35/-1, THREE disjoint regions:
  - **`WriteSceneUniforms` ~line 1177-1199** — RT-cam aspect + far-plane (Gaps 3+4).
  - **`BeginDrawTarget` ~line 1903** — `mLastSceneCam = nullptr` (Gap 5).
- **Conflict surface:** sibling engine work touches `Rnd_Wgpu_RB3.cpp` in OTHER
  regions (DrawMesh material/bloom, EndFrame composite). These three regions
  (WriteSceneUniforms projection math + the BeginDrawTarget RT-open) are disjoint
  from the wave-8 lighting/shard/bloom regions — cherry-pick should be clean. If a
  sibling also edits `WriteSceneUniforms`'s aspect line, hand-merge: keep BOTH the
  RT-aspect branch and their change (the RT branch only takes the `cam->TargetTex()`
  path; the `else` keeps the prior window-aspect line).
- **Pin bump required:** YES → `741f1367a65c3c194b5c343d814df59ffc150dca`.

### rb3 (`wt-task-crowd-2d`)
- `native/src/rb3_crowd_imposter_native.cpp` — NEW (Gap 1, strong GetSharedTex).
- `native/CMakeLists.txt` — 2 list entries (rb3-native + RB3_WEB_NATIVE_GLUE) +
  the `MILO_ENGINE_PIN` bump.
- `native/src/band3_link_stubs.s` — removed the `GetSharedTex` weak stub (strong
  def wins; `Prepare/RestoreRenderAlley` stay no-op).
- `src/system/rndobj/MultiMesh.cpp` — HX_NATIVE billboard branch (Gap 2). Wii 100%.
- `scripts/native/arena-crowd2d-capture.py` — NEW verification harness.
- `Crowd.cpp` is PRISTINE (Fix A `dcad5834` untouched — empty git diff).
- **Conflict surface:** `MultiMesh.cpp` edit is additive + fully HX_NATIVE-gated
  (disjoint from any matching work). `band3_link_stubs.s` edit is a single
  stub-line removal. `native/CMakeLists.txt` edits are in the rb3-native source
  list + the web-glue list + the pin line.

### Do NOT
- Do NOT regress Fix A small_club 3D crowd — `Crowd.cpp` is untouched; the imposter
  tex is never created for force3D venues.
- Do NOT land the rb3 pin bump before the engine commit is on its canonical branch.
- Do NOT scope the far-plane widen to "all cams" — it is RT-cam-gated
  (`cam->TargetTex()`); the main scene cam keeps its authored far.

---

## Follow-ups (non-blocking)
- **PreWarm a skinned-RT pipeline variant.** `PipelineManager::PreWarm` sweeps the
  RT pass with STATIC layout only (it assumed "skinned never renders into the RT").
  The imposter path now renders SKINNED chars into the RT, so the
  `{rtFmt, hasDepth=false, alphaWrite=true, skinned=true}` pipeline compiles
  synchronously on the first imposter frame. Add that PassVariant to avoid a
  first-frame hitch (correctness is fine; this is a warm-up nicety).
- **festival_01** still SIGSEGVs on load (festival-specific, a sibling agent's
  scope); arena_06 is the clean test venue.
- **In-venue billboard A/B** would benefit from a deterministic bowl-facing camera
  pin (the director cam is non-deterministic). Not needed to prove correctness.
