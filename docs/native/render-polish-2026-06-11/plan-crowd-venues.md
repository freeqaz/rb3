# Plan: `crowd-venues` (deferred wrap-up) — Fix C (venue bridge) + Fix B (2D imposter crowd)

Planner pass, 2026-06-19. Non-blocking wrap-up of the render-polish campaign.
Source designs: `scout-crowd.md` §2.4 / §2.5 / §3. Shipped prior art (DO NOT
regress): `task-crowd-impl.md` (Fix A — gameplay 3D crowd skeleton rebind, master
`dcad5834`, small_club only).

**Verdict: `tractable = partial`.** Fix C (venue bridge) is a small, contained,
match-neutral rb3-side change — IMPLEMENT IT. Fix B (2D bowl-imposter crowd) is a
multi-piece render-to-texture + billboard pipeline bring-up spanning rb3 + engine
+ a new native stub; with Fix C landed it can be **attempted** but is NOT a
one-liner and carries real risk — deliver it as a **complete design (plan-only)**
with the exact hooks, and only let the implementer land it if the first RTT
screenshot is clean. Do NOT force a risky half-feature into the wrap-up.

---

## Part 1 — FIX C: venue bridge honors MetaPerformer  (tractable = YES, do first)

### Root cause (live-verified on rb3-native, port 9831, 2026-06-19)

`BandDirector::EnterVenue` (engine layer, `src/system/bandobj/BandDirector.cpp`)
has an HX_NATIVE bridge at **:627-654** that force-loads the venue because the
retail data-driven `load_venue` dispatch never fires natively. The bridge picks
the venue symbol from the **world's authored `venue` property** only:

```cpp
const DataNode *venueProp = GetWorld()->Property(Symbol("venue"), false);
Symbol venueSym = venueProp ? venueProp->Sym(nullptr) : Symbol("small_club_01");
if (venueSym.Null()) venueSym = Symbol("small_club_01");
...
LoadVenue(venueSym, kLoadStayBack);
```

It never consults `MetaPerformer`, so a `{meta_performer set_venue_override
arena_06}` is stored but ignored — every native run pins `small_club_01`.
Live confirmation (`VENUE_DBG=1`, fresh boot):

```
VENUE_DBG: EnterVenue force-loading venue='small_club_01'   # always
```

### IMPORTANT refinement to the scout's Fix C — use GetVenueOverride(), NOT GetVenue()

The scout's §3 says "prefer `MetaPerformer::Current()->GetVenue()` when non-null."
**That is wrong for the native flow** — live-probed and refuted:

| dta-eval (splash, fresh boot)                  | result |
|---|---|
| `{meta_performer get_venue}` (before/after)    | `""` (empty — `mVenue == gNullStr`) |
| `{meta_performer get_venue_override}` (before) | `no_venue_override` (sentinel) |
| `{meta_performer set_venue_override arena_06}` | ok |
| `{meta_performer get_venue_override}` (after)  | **`arena_06`** (sticks) |
| `{meta_performer get_venue}` (after set)       | still `""` |

`mVenue` is only populated by `MetaPerformer::SetVenue()` /
`SelectRandomVenue()` (MetaPerformer.cpp:1041, 960), which are driven by the
tour/setlist performer flow (`TourPerformerLocal.cpp:59-61`) and never run in the
native quickplay path. So `GetVenue()` stays empty natively. The value that
actually sticks is **`GetVenueOverride()`** (returns the override symbol, or the
sentinel `no_venue_override` when unset). The bridge must read that.

### Exact change

**File:** `src/system/bandobj/BandDirector.cpp`, inside the existing `#ifdef
HX_NATIVE` block at :627-654 (already HX_NATIVE — match-neutral by construction).

**Approach (priority order for the venue symbol):**

1. If `MetaPerformer::Current()` exists and `GetVenueOverride()` is a real venue
   (i.e. `!= no_venue_override` and non-null), use it.
2. Else fall back to the existing world-property `venue` (which yields
   `small_club_01`).

**Cross-layer access — NO layering inversion, NO band3 `#include`.** BandDirector
is engine layer (`src/system/bandobj/`); MetaPerformer is game layer
(`src/band3/meta_band/`). Do NOT `#include "meta_band/MetaPerformer.h"`. Reach
`meta_performer` the same way DTA does — it is a `gDataDir`-findable named global
object (`DataNode::GetObj` → `gDataDir->FindObject("meta_performer", true)`,
verified in `src/system/obj/DataNode.cpp:361`; `meta_performer` is created at
`MetaPerformer.cpp:161`). Two viable mechanisms:

- **(preferred) DTA-eval a query expression** and read the returned symbol. Build
  a `DataArrayPtr` for `{meta_performer get_venue_override}` (or resolve the
  object and `obj->Handle(Message("get_venue_override"), false).Sym()`), guard
  null, and compare to the `no_venue_override` symbol. `HandleType`/`Handle`
  returning a `.Sym()` is the established engine pattern (BandDirector.cpp:182
  reads `HandleType(allowMsg).Int()`; BandWardrobe.cpp:282 returns `pm.Sym()`).
- The `no_venue_override` symbol can be compared by name (`Symbol("no_venue_override")`)
  without including the header.

**Sketch (illustrative — implementer finalizes the exact Handle/eval form):**

```cpp
Symbol venueSym; // resolved below
#ifdef HX_NATIVE
{
    // venue override (MetaPerformer) wins over the world's authored `venue`.
    Symbol ov;
    if (Hmx::Object *mp = gDataDir ? gDataDir->FindObject("meta_performer", true) : nullptr) {
        static Message getOverrideMsg("get_venue_override");
        ov = mp->Handle(getOverrideMsg, false).Sym();
    }
    if (!ov.Null() && ov != Symbol("no_venue_override"))
        venueSym = ov;   // e.g. arena_06
}
#endif
if (venueSym.Null()) {
    const DataNode *venueProp = GetWorld()->Property(Symbol("venue"), false);
    venueSym = venueProp ? venueProp->Sym(nullptr) : Symbol("small_club_01");
    if (venueSym.Null()) venueSym = Symbol("small_club_01");
}
```

(`obj/DataUtl.h` already declares `extern ObjectDir *gDataDir`; BandDirector.cpp
includes `obj/Data.h`/`obj/Object.h`, so `gDataDir`/`Handle`/`Message`/`Symbol`
are all in scope. Add `#include "obj/DataUtl.h"` if `gDataDir` isn't transitively
visible.)

### Why this is sufficient & safe

- The rest of `EnterVenue` (LoadVenue → wardrobe instancing → `LoadCharacters`
  → `SetVenueDir` → `HarvestDircuts`) is **venue-symbol-agnostic** — it already
  works for small_club; swapping the symbol just changes which `.milo_xbox`
  loads. `GetVenuePath` (BandDirector.cpp:727) maps `arena_06` → `world/venue/
  arena/arena_06/arena_06.milo` via the `gVenues[]` prefix match (arena is in the
  table), and native resolves the `.milo_xbox` flavor.
- Asset present: `orig-assets/extracted/world/venue/arena/arena_06/gen/
  arena_06.milo_xbox` (25MB), same layout as small_club_01.
- Default behavior unchanged: with no override set (the normal case),
  `GetVenueOverride()` is `no_venue_override` → falls straight through to the
  small_club_01 fallback → byte-for-byte the current flow.

### Match-neutrality (Fix C)

The change is **entirely inside the existing `#ifdef HX_NATIVE` block** at
BandDirector.cpp:627-654. Wii compilation sees no new lines. `BandDirector.o`
must remain byte-identical. (BandDirector currently sits at sub-100% permuter-
class % in report.json — the gate is "unchanged vs the pre-change baseline," not
100%.)

### Scope / risk (Fix C): LOW

One file, additive, inside an existing native block. The only real risk is the
async-load invariant the bridge documents at :634-648: the venue force-load must
stay SYNCHRONOUS (the enclosing `Enter()` derefs `TheBandWardrobe` on the same
frame). Changing only the venue *symbol* does not affect sync-ness — keep the
existing `NativeVenueSync()` / `mAsyncLoad` handling exactly as-is.

---

## Part 2 — FIX B: 2D bowl-imposter crowd  (tractable = PLAN-ONLY; attempt only after Fix C + first-RTT screenshot)

### Why this can be exercised only after Fix C

The 2D imposter path is structurally dead AND unreachable today: small_club is
`force_3D_crowd=TRUE` (`Set3DCharAll()` clears `mInstances` → the 2D loop's
`numInstances==0` early-continues, Crowd.cpp:447). Arena/festival/big_club use
the non-force3D 2D bowl crowd. Until Fix C loads an arena, Fix B can't be seen.
Arena `arena_06.milo_xbox` references crowd archetypes with B-LOD imposter
variants (`crowd_male01B`, `crowd_female01B` …), consistent with the 2D bowl.

### What the 2D path does (Crowd.cpp `WorldCrowd::DrawShowing`, :429-585)

Per crowd archetype with instances:
1. `SetMatAndCameraLod()` (:93) — grabs a shared render-target tex and binds it to
   `gImpostorCamera->SetTargetTex(tex)` + `gImpostorMat->SetDiffuseTex(tex)`.
2. Positions `gImpostorCamera` at a normalized distance facing the char, square
   FOV (`kAspect=1.0`, :437/471).
3. `gImpostorCamera->Select()` → `curChar->DrawShowing()` (:550-555) — renders
   the skinned char **into the render target**.
4. `curCam->Select()` (:562) — restores the main cam.
5. `mmesh->DrawShowing()` (:576) — draws the billboard instance quads sampling
   that RT tex (material `gImpostorMat`, transform-constraint `kFastBillboardXYZ`,
   :611-612).

### Three concrete native gaps (proven)

**Gap 1 — `GetSharedTex` returns null (no render target).**
`native/src/band3_link_stubs.s:667-668` weak-stubs `WiiRnd::GetSharedTex` to a
no-op returning null, so `SetMatAndCameraLod` leaves `gImpostorCamera` with no
TargetTex and `gImpostorMat` with no diffuse. `PrepareRenderAlley` /
`RestoreRenderAlley` are also no-op stubs (:669-672) — those stay no-ops (the
engine's lazy RTT supersedes them).

**Gap 2 — the billboard quads have no camera-facing orientation natively.**
Native compiles `src/system/rndobj/MultiMesh.cpp` (verified via
`native/CMakeLists.txt:250` GLOB of `rndobj/*.cpp`), NOT `rndwii/MultiMesh.cpp`.
The portable `RndMultiMesh::DrawShowing` (rndobj/MultiMesh.cpp:162-171) just does
`mMesh->SetWorldXfm(it->mXfm)` — NO billboard. The Wii path
(rndwii/MultiMesh.cpp:111-191) composes the current cam's world rotation basis
with each instance's *translation* for `kFastBillboardXYZ`. Without that the
imposter quads render in their authored orientation (edge-on / wrong facing).

**Gap 3 — square-aspect RTT cam (minor).** `BandRnd::WriteSceneUniforms` uses the
window aspect; the imposter cam is authored square (`kAspect=1.0`). When
`cam->TargetTex()` is set, the projection should use the target's aspect. The
shared `RndCam::ScreenAspect` already factors `mTargetTex->Height()/Width()`
(rndobj/Cam.cpp:157-158), so this may already be correct — VERIFY, don't assume.

### The GOOD news — the hard part (RTT) is already built in the engine

The engine has a mature, lazily-driven render-to-texture machinery that the
clouds/sky-dome already use:
- `BandRnd::BeginDrawTarget(RndTex*)` (Rnd_Wgpu_RB3.cpp:1803) lazily creates an
  RGBA8 render-attachment texture in the same `sTexGpu` side-table the diffuse
  bind reads, suspends the main pass, and opens a transparent-clear RT pass.
- It is **triggered automatically** from `DrawMesh` (:3119-3120) and `DrawRect`
  (:3118-3121) when `RndCam::sCurrent->TargetTex()` is non-null and not yet
  active. So `gImpostorCamera->Select()` + char draw → auto-redirect into the RT.
- `RndTex::FinishDrawTarget` (Rnd_Wgpu_RB3.cpp:5639) → `EndDrawTarget` (:1857)
  resumes the main pass; it is fired by `RndCam::Select()` /
  `SetTargetTex(nullptr)` (rndobj/Cam.cpp:54-55, 66-67) — i.e. the `curCam->Select()`
  at Crowd.cpp:562 closes the RT pass for free.
- Opt-out env already exists: `RB3_RTT_OFF=1`.

Skinned meshes already render through this DrawMesh path (engine handles
`skinned` materials, :5185 CHAR_DBG). So a skinned char rendering into the RT is
plumbed; the open question is purely whether the result looks right.

### Fix B design (the exact hooks)

1. **Strong `WiiRnd::GetSharedTex` def in `native/src/`** (e.g. a new
   `rb3_crowd_imposter_native.cpp`, or extend an existing native shim) returning
   ONE persistent square `RndTex` (~256×256) with `Width()`/`Height()` set so
   `BeginDrawTarget` accepts it (:1810-1811 reject w/h<=0). Cache it static; the
   engine lazily makes it a GPU render target on first redirect. Remove the weak
   stub line for `GetSharedTex` from `band3_link_stubs.s` (a strong def wins, but
   removing the `.weak`/`.set` keeps it clean). `Prepare/RestoreRenderAlley` keep
   their no-op stubs.

2. **HX_NATIVE billboard branch in `RndMultiMesh::DrawShowing`**
   (`src/system/rndobj/MultiMesh.cpp:162`). Port the `kFastBillboardXYZ` math from
   `rndwii/MultiMesh.cpp:111-150`: for each instance, build a world xfm = current
   cam's world rotation basis + the instance's translation, set it on `mMesh`,
   draw. Gate other constraints (`kBillboardXYZ/Z/XZ`) too if the imposter mesh
   uses them, else default to the existing pass-through. HX_NATIVE-only →
   Wii-neutral. (Alternatively, ensure the shared `RndTransformable` constraint
   apply runs in the native draw — but the Wii path deliberately does it inline,
   so the explicit branch is the safer port.)

3. **Aspect (Gap 3):** verify the RT cam projection uses the target aspect. If
   `WriteSceneUniforms` ignores TargetTex aspect, change it (engine) to use the
   target's W/H when `cam->TargetTex()` is set — this also benefits the clouds
   RTT. If done, it is an ENGINE change: commit in the PAIRED engine worktree
   (`<wt>/.engine-path`), then bump `MILO_ENGINE_PIN` in `native/CMakeLists.txt`
   in a matching rb3 commit. Currently pinned `15ce606`.

4. **Verify the RT-compatible pipeline variant covers the skinned layout** — the
   imposter render draws a skinned char into the RT; confirm the engine's RTT
   pass binds the skinned pipeline (the clouds path is unskinned). This is the
   single most likely place Fix B needs unexpected engine work.

### Scope decision (Fix B): PLAN-ONLY, attempt-gated

This is NOT a contained one-file hookup — it touches `native/src` (new
GetSharedTex), `src/system/rndobj/MultiMesh.cpp` (billboard), possibly the engine
(aspect + skinned-RTT pipeline variant), and depends on the lazy-RTT machinery
behaving for a per-archetype mid-frame target that is RE-USED across multiple
archetypes per frame (the RTT machinery explicitly bails on NESTED targets,
:1806-1808 — confirm the imposter loop fully closes each RT, via `curCam->Select()`,
before the next archetype's `SetMatAndCameraLod`/`Select`; the code structure at
:548-563 suggests it does, but the SHARED single tex re-used per archetype within
one frame means each archetype overwrites the previous — verify the billboard
draw (:576) samples the tex BEFORE the next archetype repaints it).

**Recommendation for the implementer:** implement pieces 1+2 behind an opt-in env
(e.g. `RB3_CROWD_IMPOSTER=1`), boot an arena via Fix C, capture the FIRST RTT
screenshot. IF the bowl crowd renders as textured camera-facing billboards (or is
clearly one bounded bug away) → finish + land. IF it requires the engine
skinned-RTT pipeline variant or the shared-tex-reuse ordering turns out to need a
per-archetype tex / ping-pong (a genuine new pipeline) → STOP, keep the opt-in
env default-OFF, and deliver the diagnosis. Do not ship a half-rendered crowd
default-on.

### Match-neutrality (Fix B)

- `native/src/` new file + stub removal: not Wii-compiled. Neutral.
- `src/system/rndobj/MultiMesh.cpp` billboard branch: must be `#ifdef HX_NATIVE`
  → `MultiMesh.o` byte-identical on Wii. (Confirm with objdiff on
  `RndMultiMesh::DrawShowing`.)
- Any engine change (aspect / pipeline): engine repo only, Wii untouched, pin
  bump in a matching rb3 commit.

---

## VERIFICATION (per symptom — for implementer AND reviewer)

Harness: `scripts/native/keyboard-to-gameplay.py` (boot to gameplay) +
`scripts/native/crowd-shot-capture.py` (freeze cam, force crowd shots). HTTP API
on YOUR assigned port (9831-9834): `/api/health`, `/api/screenshot`,
`/api/dta/eval`, `/api/input`. Evidence under `/tmp/rp8-crowd-venues/`, lean.
Engine logs contain binary bytes — always `grep -a`.

### Fix C — "a non-small_club venue loads"

1. **Override sticks + is honored (log).** Boot with `VENUE_DBG=1`, set the
   override BEFORE entering a song (`{meta_performer set_venue_override arena_06}`
   via `/api/dta/eval`), drive to gameplay, then:
   ```
   grep -a "EnterVenue force-loading venue=" /tmp/rb3-kbd2game-<port>.log
   ```
   PASS: `venue='arena_06'` (FAIL today = `small_club_01`).
2. **Default unchanged.** Same run with NO override set →
   `venue='small_club_01'` (regression guard: normal flow must be untouched).
3. **Visual — different venue (REQUIRED screenshot).** With the override, capture
   `/api/screenshot` at gameplay. PASS: the venue is visibly the arena (large bowl
   / different geometry + lighting) vs the small club. Save
   `/tmp/rp8-crowd-venues/arena_loaded.png` and the small_club baseline side by
   side. (This is THE deliverable for Fix C: "screenshot the different venue.")
4. **No crash / stable.** Arena reaches `game_screen`, no SIGABRT/SIGSEGV across
   a `--game-burst 8` run; 3 charted songs (20thcenturyboy/25or6to4/antibodies)
   still play.
5. **Fix A not regressed (small_club 3D crowd).** With NO override (small_club),
   `SHARD_DBG=1 SHARD_RATIO_DBG=1` →
   `grep -a SHARD_GUARD <log> | grep -a crowd_body | wc -l` stays ~6680 (the Fix A
   shipped number), NOT back to ~63k+. The crowd renders full-bodied + spread.
6. **Match gate.** `BandDirector.o` byte-identical: objdiff
   `BandDirector::EnterVenue` (and the unit's report.json %) unchanged vs the
   pre-change baseline. (`build/tools/objdiff-cli diff -u
   "system/bandobj/BandDirector" "EnterVenue__13BandDirectorFv" --format
   json-pretty`.)

### Fix B — "the 2D bowl crowd rows render" (only if attempted)

Requires Fix C to load an arena. Opt-in `RB3_CROWD_IMPOSTER=1` (if gated).
1. **RT painted.** `RB3_RENDER_DBG=1` → `grep -a "RTT created" <log>` shows the
   imposter tex created (e.g. 256x256). Non-null TargetTex on `gImpostorCamera`.
2. **No full-screen splat.** Wide arena shot: NO single merged character at world
   origin filling the frame (the current broken-stub symptom, scout §2.4).
3. **Textured camera-facing rows.** Bowl crowd rows visible as textured billboard
   quads facing the camera (not edge-on, not white-fallback). Save
   `/tmp/rp8-crowd-venues/arena_bowl_crowd.png`.
4. **Animation/variation across frames** (nice-to-have): far-crowd imposter
   texture changes between two shots 2-3s apart.
5. **3D crowd still works.** small_club 3D crowd (Fix A) unchanged with the env
   ON and OFF.
6. **Match gate.** `RndMultiMesh::DrawShowing` objdiff byte-identical on Wii;
   if engine touched, Wii report unchanged + pin bumped in a matching rb3 commit.

### REFERENCE SCREENSHOTS NEEDED
- Retail arena/festival wide shot showing the 2D bowl crowd (Fix B look target).
  `images/retail-screenshots/` + `../xenia` for ground-truth capture.

---

## Files

- **Fix C (implement):** `src/system/bandobj/BandDirector.cpp` (inside the
  existing HX_NATIVE block, :627-654). Possibly `#include "obj/DataUtl.h"` for
  `gDataDir`.
- **Fix B (plan-only / attempt-gated):**
  - `native/src/` — new strong `WiiRnd::GetSharedTex` (new file e.g.
    `rb3_crowd_imposter_native.cpp`).
  - `native/src/band3_link_stubs.s` — remove the `GetSharedTex` weak stub
    (:667-668).
  - `src/system/rndobj/MultiMesh.cpp` — HX_NATIVE billboard branch in
    `RndMultiMesh::DrawShowing` (:162).
  - ENGINE (paired worktree, only if needed): `src/platform/Rnd_Wgpu_RB3.cpp`
    (square-aspect on TargetTex; skinned-RTT pipeline variant) +
    `MILO_ENGINE_PIN` bump in `native/CMakeLists.txt`.

## Do NOT
- Do NOT regress the shipped Fix A small_club 3D crowd (`dcad5834`,
  `RebindCrowdCharBonesToOwnSkeleton`, Crowd.cpp:413/911). It runs in `Draw3DChars`
  (3D path) only — disjoint from Fix B's 2D `DrawShowing` path.
- Do NOT use `GetVenue()` for the bridge (live-proven empty natively); use
  `GetVenueOverride()`.
- Do NOT `#include` a band3 header from engine-layer BandDirector — reach
  `meta_performer` via the `gDataDir`/Handle object system.
- Do NOT make the venue force-load async (the sync invariant at :634-648).
- Do NOT broadly `pkill -f rb3-native`; kill ONLY your own PIDs (verify
  `/proc/<pid>/environ` `RB3_HTTP_PORT` in 9831-9834).
