# Impl: `crowd-venues` (wrap-up) — Fix C SHIPPED, Fix B PLAN-ONLY (de-risked)

Wrap-up implementer, 2026-06-19. Implements the planner's **Fix C** (venue bridge
honors MetaPerformer) and delivers **Fix B** (2D bowl-imposter crowd) as a
plan-only design with the exact hooks + a NEW de-risking finding.

- **rb3 branch:** `wt-task-crowd-venues`
  (worktree `/home/free/code/milohax/rb3/.claude/worktrees/task-crowd-venues`)
- **rb3 commit:** `1859def9` — `fix(native): venue bridge honors MetaPerformer
  venue override (Fix C)`
- **Engine:** NO engine change. Engine worktree
  (`/home/free/code/milohax/milo-native-engine-worktrees/task-crowd-venues`,
  branch `wt-task-crowd-venues`) is CLEAN at the existing pin `15ce606`. No
  `MILO_ENGINE_PIN` bump.
- **Wii byte-identical:** YES. objdiff `BandDirector::EnterVenue` 100.0%, 520==520
  bytes, diff 0 (the change is entirely inside the existing `#ifdef HX_NATIVE`
  block; new `#include`s are HX_NATIVE-guarded too).

---

## TL;DR — result

**Fix C SHIPPED + VERIFIED.** `{meta_performer set_venue_override arena_06}` is now
honored: native gameplay loads the **arena** venue instead of always pinning
`small_club_01`. Default behavior (no override) is byte-for-byte unchanged
(falls through to small_club_01). Fix A (small_club 3D crowd) is unregressed.

**Fix B is PLAN-ONLY** (per the planner's attempt-gate). With Fix C landed, the
arena 2D imposter path is now *reachable and proven live* (8 archetypes × ~87-88
instances per frame), and the broken-stub symptom is confirmed (`GetSharedTex`
returns null). It is a genuine multi-piece render-to-texture + billboard bring-up
with NO retail reference to verify against — NOT a contained hookup. **NEW
finding that de-risks a future attempt:** the engine's *skinned* DrawMesh pipeline
key ALREADY selects an RT-compatible variant when `rtPass` is active
(`Rnd_Wgpu_RB3.cpp:5384-5410` — `key.layout=Skinned`, `key.targetFormat=mRtFmt`,
`key.hasDepth=false`). So the scout's predicted "biggest unknown" (skinned-RTT
pipeline variant) is already handled at pin `15ce606`. Remaining work is the
native GetSharedTex shim + the billboard branch + verifying the 8-archetype
per-frame RT close-cycle — design below.

---

## FIX C — what changed (SHIPPED)

**File:** `src/system/bandobj/BandDirector.cpp` — TWO regions, both inside the
existing `#ifdef HX_NATIVE`:

1. **Includes (lines 21-24, inside the `#ifdef HX_NATIVE` block opened at :15):**
   ```cpp
   #include "obj/DataUtl.h"   // gDataDir (named-global object lookup)
   #include "obj/Msg.h"       // Message (to Handle the get_venue_override query)
   ```
2. **EnterVenue venue-symbol resolution (lines ~631-665, inside the existing
   `if (!mVenue.Dir() && GetWorld())` native force-load block):** read the
   MetaPerformer override FIRST, fall back to the world's authored `venue` prop:
   ```cpp
   Symbol venueSym;
   if (gDataDir) {
       if (Hmx::Object *mp = gDataDir->FindObject("meta_performer", true)) {
           static Message getVenueOverrideMsg("get_venue_override");
           Symbol ov = mp->Handle(getVenueOverrideMsg, false).Sym();
           if (!ov.Null() && ov != Symbol("no_venue_override"))
               venueSym = ov;   // e.g. arena_06
       }
   }
   if (venueSym.Null()) {
       const DataNode *venueProp = GetWorld()->Property(Symbol("venue"), false);
       venueSym = venueProp ? venueProp->Sym(nullptr) : Symbol("small_club_01");
       if (venueSym.Null()) venueSym = Symbol("small_club_01");
   }
   ```

### Why this exact form (refinements over the scout's §3 sketch)

- **`GetVenueOverride()`, NOT `GetVenue()`** — the planner already proved
  `GetVenue()`/`mVenue` stays empty natively (only `SetVenue`/`SelectRandomVenue`
  in the tour/setlist flow populate it). The value that sticks is the override.
  Live-confirmed: `{meta_performer get_venue_override}` reads `no_venue_override`
  by default, `arena_06` after the set.
- **No band3 `#include`** (no layering inversion). `meta_performer` is reached the
  way DTA does it — `gDataDir->FindObject("meta_performer", true)` (chains up to
  `ObjectDir::sMainDir` where MetaPerformer registers, Dir.cpp:551-552;
  MetaPerformer.cpp:171 `SetName(cc, ObjectDir::sMainDir)`), then Handle the same
  `get_venue_override` message DTA evaluates (`HANDLE_EXPR(get_venue_override,
  GetVenueOverride())`, MetaPerformer.cpp:1715). `Message`'s `operator DataArray*`
  feeds `Hmx::Object::Handle(DataArray*, bool=false)`; `.Sym()` reads the returned
  Symbol. `Handle(..., /*warn=*/false)` suppresses any MILO_DEBUG miss-warn.
- **Default unchanged:** with no override, the handler returns the
  `no_venue_override` sentinel → the `if (venueSym.Null())` fallback runs → the
  original small_club_01 path, byte-for-byte. The rest of EnterVenue (LoadVenue →
  wardrobe → LoadCharacters → SetVenueDir → HarvestDircuts) is venue-symbol-
  agnostic and the sync invariant (`NativeVenueSync()`/`mAsyncLoad`) is untouched.

`GetVenuePath` (BandDirector.cpp:767) maps `arena_06`→`world/venue/arena/arena_06/
arena_06.milo` via the `gVenues[]` prefix match (`"arena"` is entry 0); native
resolves the `.milo_xbox` flavor. Asset present:
`orig-assets/extracted/world/venue/arena/arena_06/gen/arena_06.milo_xbox` (25MB).

---

## FIX C — VERIFICATION (before/after, evidence under `/tmp/rp8-crowd-venues/`)

Harness: `/tmp/rp8-crowd-venues/venue_override_test.py` (boots rb3-native headless
on an assigned port, navigates splash→hub→song_select via raw pad presses, injects
`{meta_performer set_venue_override <sym>}` at song_select BEFORE the song confirm
that triggers EnterVenue, drives to gameplay, captures screenshots + the VENUE_DBG
log). Ports 9835-9838.

| # | criterion | baseline (no override) | with override=arena_06 | verdict |
|---|---|---|---|---|
| 1 | `get_venue_override` value | `no_venue_override` | set→`arena_06` (sticks) | as designed |
| 2 | VENUE_DBG honoring line | (absent) | `EnterVenue honoring MetaPerformer venue override='arena_06'` | PASS |
| 3 | VENUE_DBG force-loading line | `venue='small_club_01'` | `venue='arena_06'` | PASS (was always small_club) |
| 4 | venueName / LoadCharacters | small_club_01 | `venueName='arena_06'`, `LoadCharacters('arena_06')` | PASS |
| 5 | reaches game_screen, song plays | yes (is_playing=1) | yes (is_playing=1) | PASS |
| 6 | crashes (SIGSEGV/SIGABRT) | 0 | 0 | PASS (clean) |
| 7 | Fix A small_club 3D crowd | crowd_body shard drops NOT in ~63k broken range | n/a | PASS (unregressed) |
| 8 | objdiff EnterVenue (Wii) | 100.0%, 520==520, diff 0 | — | PASS (byte-identical) |

**THE deliverable screenshot — different venue loaded:**
- `/tmp/rp8-crowd-venues/arena_loaded.png` — arena wide shot: large London-arena
  bowl, red/crimson stage lighting, overhead truss, "LONDON" signage, raised stage
  with band members — unmistakably NOT the small club.
- `/tmp/rp8-crowd-venues/arena_loaded_wide.png` — second arena gameplay frame.
- `/tmp/rp8-crowd-venues/smallclub_baseline.png` — small_club baseline (pink/
  checkered close wall) for side-by-side.

Logs: `/tmp/rb3-venuetest-9835.log` (baseline small_club), `…-9836.log` (arena
override), `…-9838.log` (Fix A regression).

---

## FIX B — 2D bowl-imposter crowd (PLAN-ONLY, attempt-gated → NOT attempted this wave)

### Why Fix B can now be EXERCISED (Fix C unlocked it) — and is PROVEN live

With the arena loaded, the 2D imposter path runs. A temporary opt-in probe
(`RB3_CROWD2D_PROBE`, since removed — Crowd.cpp is back to pristine == Fix A
state) logged, in the arena, per frame:

```
CROWD2D_PROBE: 2D-imposter path archetype='crowd_male01'   numInstances=87 sharedTex=(nil)
CROWD2D_PROBE: 2D-imposter path archetype='crowd_female01' numInstances=88 sharedTex=(nil)
... 8 archetypes (crowd_male01-04 + crowd_female01-04), ~87-88 instances each
```

This confirms (a) arena uses the non-force3D 2D bowl path (8 archetypes × ~700
instances total — the bowl crowd), and (b) the broken-stub symptom directly:
**`TheWiiRnd.GetSharedTex(...) == nullptr`** → `gImpostorCamera` gets no TargetTex
→ `BeginDrawTarget` never fires (it requires non-null tex) → the imposter char is
NOT redirected into an RT, and the billboard quads sample a null/empty diffuse.
In the arena screenshots the bowl reads as venue geometry + lighting with NO
rendered camera-facing crowd rows — consistent with the dead pipeline.

### The three native gaps (still accurate)

- **Gap 1 — `GetSharedTex` returns null.** `native/src/band3_link_stubs.s:667-668`
  weak-stubs `WiiRnd::GetSharedTex` to return null. PROVEN above (`sharedTex=(nil)`).
- **Gap 2 — billboard quads have no camera-facing orientation natively.** Native
  compiles `src/system/rndobj/MultiMesh.cpp` (portable `RndMultiMesh::DrawShowing`,
  :162-171 — just `SetWorldXfm(it->mXfm)`, NO billboard), not `rndwii/MultiMesh.cpp`
  (which composes the cam world rotation basis with each instance's translation for
  `kFastBillboardXYZ`). Without it the quads render in authored orientation.
- **Gap 3 — square-aspect RTT cam.** The imposter cam is authored square
  (`kAspect=1.0`, Crowd.cpp:437/471). Likely already correct via
  `RndCam::ScreenAspect` factoring `mTargetTex->Height()/Width()` — VERIFY.

### NEW finding — the scout's "biggest unknown" is ALREADY handled (de-risks Fix B)

The scout/planner flagged the **skinned-RTT pipeline variant** (drawing a skinned
char into the RT) as "the single most likely place Fix B needs unexpected engine
work" (clouds RTT is unskinned). **At the current pin `15ce606`, the engine's
*skinned* DrawMesh pipeline key already threads the RT pass:**
`src/platform/Rnd_Wgpu_RB3.cpp:5384-5410`:
```cpp
key.layout = skinned ? VertexLayoutType::Skinned : VertexLayoutType::Static;
...
bool rtPass = (mRtActiveTex != nullptr);
if (rtPass) { key.targetFormat = mRtFmt; key.hasDepth = false; }
key.alphaWrite = rtPass ? true : false;
```
So a skinned char rendering into the no-depth RGBA8 RT pass selects a valid
RT-compatible *skinned* pipeline variant for free. This materially lowers the Fix B
risk the planner assumed — the remaining work is the two native gaps + verifying
the multi-archetype mid-frame RT close-cycle (below).

### Why it is STILL plan-only (not a contained one-file hookup)

Fix B requires ALL of these, spanning multiple layers, with no ground-truth to
verify against:
1. **New native strong `WiiRnd::GetSharedTex`** returning ONE persistent square
   `RndTex` (~256×256, `Width()`/`Height()` set so `BeginDrawTarget` accepts it,
   Rnd_Wgpu_RB3.cpp:1810-1811 reject w/h≤0). New file e.g.
   `native/src/rb3_crowd_imposter_native.cpp`. Remove the `GetSharedTex` weak stub
   from `band3_link_stubs.s:667-668` (a strong def wins; remove the `.weak`/`.set`
   to keep it clean). `Prepare/RestoreRenderAlley` keep their no-op stubs.
2. **HX_NATIVE billboard branch in `RndMultiMesh::DrawShowing`**
   (`src/system/rndobj/MultiMesh.cpp:162`) porting the `kFastBillboardXYZ` math
   from `rndwii/MultiMesh.cpp:111-150` (per-instance world xfm = current cam world
   rotation basis + the instance translation). HX_NATIVE-only → Wii-neutral
   (confirm `RndMultiMesh::DrawShowing` objdiff byte-identical).
3. **Verify the multi-archetype, single-shared-tex, mid-frame RT close-cycle.**
   This is now the PRIMARY verification risk. `BeginDrawTarget` BAILS on a nested
   target (`if (mRtActiveTex) return;`, Rnd_Wgpu_RB3.cpp:1808). The 2D loop runs 8
   archetypes per frame, each: `SetMatAndCameraLod()` (binds the SAME shared tex to
   the cam) → `gImpostorCamera->Select()` → `curChar->DrawShowing()` (triggers
   `BeginDrawTarget` once `mRtActiveTex` is clear) → `curCam->Select()` (fires
   `RndTex::FinishDrawTarget`→`EndDrawTarget`, closing the RT) → `mmesh->DrawShowing()`
   (billboard samples the tex). Each archetype must FULLY close its RT (via
   `curCam->Select()`, Crowd.cpp:562) AND the billboard must sample BEFORE the next
   archetype's `SetMatAndCameraLod`/`Select` repaints the shared tex. The code
   structure (Crowd.cpp:548-576) suggests it does, but the single SHARED tex reused
   per archetype means each archetype overwrites the previous — verify the billboard
   draw (:576) samples the RT before the next archetype clobbers it. If the RT
   pass does NOT cleanly close per archetype, this needs a per-archetype tex or a
   restructure (a genuine new pipeline) — STOP if so.
4. **Aspect (Gap 3):** verify (don't assume) the RT cam projection uses the square
   target aspect. If `WriteSceneUniforms` ignores TargetTex aspect, that is an
   ENGINE change (commit in the paired engine worktree, bump `MILO_ENGINE_PIN`).
5. **No retail reference.** `images/retail-screenshots/` has NO arena/festival bowl
   shot; the gameplay refs are small_club. There is no ground truth to gate "the
   bowl crowd renders correctly" against. Capture one from `../xenia` first.

### Recommended Fix B attempt path (for a future wave)

Implement pieces 1+2 behind an opt-in env (`RB3_CROWD_IMPOSTER=1`, default OFF),
boot the arena via Fix C (now landed), enable `RB3_RENDER_DBG=1` to watch
`[dbg] RTT created NxN ...`, capture the FIRST RTT screenshot. IF the bowl crowd
renders as textured camera-facing billboards (or is one bounded bug away) → finish
+ land. IF the 8-archetype shared-tex close-cycle (#3) turns out to need a
per-archetype tex / ping-pong, or the aspect (#4) needs an engine change with
unexpected fallout → STOP, keep the env default-OFF, deliver the diagnosis. Do not
ship a half-rendered crowd default-on.

---

## LANDING NOTES (orchestrator)

- **rb3-only, ONE file, ONE commit.** Branch `wt-task-crowd-venues`, commit
  `1859def9`. File `src/system/bandobj/BandDirector.cpp`:
  - **lines 21-24** — two new HX_NATIVE-guarded `#include`s (`obj/DataUtl.h`,
    `obj/Msg.h`) inserted after the existing `#include <cstdlib>` at :20, INSIDE
    the `#ifdef HX_NATIVE` block opened at :15.
  - **lines ~631-665** — the venue-symbol resolution inside the existing
    `if (!mVenue.Dir() && GetWorld()) { ... }` native force-load block. The
    LoadVenue call + sync handling below it (~:684-688) is UNCHANGED.
- **No engine commit, no `MILO_ENGINE_PIN` bump.** Engine pin stays `15ce606`.
- **Conflict surface:** disjoint from the sibling engine tasks. They touch
  `Rnd_Wgpu_RB3.cpp` / `standard_wgsl.inc` (engine) and other rndobj files; this
  touches ONLY `src/system/bandobj/BandDirector.cpp` (game/engine-layer rb3 src,
  not the engine repo). Crowd.cpp is pristine (== Fix A `dcad5834` state) — the
  temporary Fix B probe was fully removed.
- **Wii gate:** objdiff `EnterVenue__12BandDirectorFv` (note: `__12`, not `__13`)
  100.0% / 520==520 bytes / diff 0. `BandDirector.o` byte-identical.

## Do NOT
- Do NOT regress Fix A small_club 3D crowd (`dcad5834`) — Crowd.cpp untouched.
- Do NOT use `GetVenue()` for the bridge (empty natively) — use
  `GetVenueOverride()`.
- Do NOT `#include` a band3 header from engine-layer BandDirector.
- Do NOT make the venue force-load async (sync invariant at the call site).
- Do NOT ship Fix B half-rendered default-on; it is plan-only until the RT
  close-cycle + a retail reference are confirmed.
