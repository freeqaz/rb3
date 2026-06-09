# 05 — Venue frustum cull: tight spheres + world.cam-scoped cull (P2.3 / spec A1)

## Problem

Splash + menus draw the FULL 3D venue (~823 meshes / 250k tris) behind 2D UI at ~10 fps
because native disables frustum culling entirely. Culling was disabled because the
baked Xbox `mSphere` bounds are wrong-centered/undersized — correct culling against
them drops VISIBLE meshes. Original spec: `docs/native/audio-perf-loop/wave-04.md`.
Blocker (char-skinning edits to Rnd_Wgpu_RB3.cpp) is CLEARED as of 2026-06-06.

## Current code

- **Disabled cull** — rb3 `src/system/rndobj/Draw.cpp`: HX_NATIVE arm of
  `RndDrawable::Draw()` (whole fn :165-181; HX_NATIVE block :167-172) unconditionally
  calls `DrawShowing()` after `MenuVoidDrawHook` (:168), skipping the Wii-side
  `MakeWorldSphere` + `RndCam::sCurrent->CompareSphereToWorld(sphere)` check
  (Wii `#else` arm :174-178) with a stale comment (:169-171) claiming the WebGPU
  frustum doesn't match — it does at 16:9 (proven in wave-04 §A1), the sphere is the
  bug. Same pattern in `DrawBudget()` (whole fn :183-208; HX_NATIVE block :187-198,
  `MenuVoidDrawHook` at :188).
- **Sphere infra** — `src/system/rndobj/Mesh.cpp:180-190` `RndMesh::UpdateSphere()`:
  static meshes get sphere from CPU verts; skinned meshes get `s.Zero()` (= never
  cull, correct). BUT native venue meshes are GPU-compressed (`mVerts` empty, data in
  `mCompressedVerts`) so `MakeWorldSphere` from CPU verts can't fix them — the only
  place real positions exist is the engine's unpack at draw/upload.
- **Unpack site** — engine `Rnd_Wgpu_RB3.cpp` DrawMesh :3070-3167: verts unpacked
  into `gpuVerts`/`gpuVertsSkinned`; `nv` finalized at :3167, before the GPU upload
  that is gated `if (needUpload)` at :3208. VERIFIED: the unpack runs
  **UNCONDITIONALLY** (engine comment at :3071 — "the unpack runs UNCONDITIONALLY …
  because the skinned shard-guard re-blends … every frame"), for BOTH skinned and
  static/compressed meshes (the `if (nv>0) … else if (compressed) … else return;`
  block at 3078-3166). So the unpacked positions are present at :3167 regardless of
  `needUpload` — gating the sphere recompute on `needUpload` (Part 1) just makes it
  once-per-geometry-generation, and is implementable as written.
- **Camera scoping precedent** — engine already string-compares
  `RndCam::sCurrent->Name()` against "world.cam" (venue lighting, :2994) and
  "game.cam" (bloom/glow, :4211, :4373). (Verified; the doc's earlier ~:2993/~:4372
  were each off by one.)

## Changes

**Part 1 — recompute tight LOCAL sphere at first unpack (engine, Rnd_Wgpu_RB3.cpp):**
After unpack (~:3167), when `needUpload` (so once per geometry generation, not per
draw): min/max over unpacked positions → center + radius (AABB-diagonal radius is
fine), then store onto the mesh: `mesh->SetSphere(localSphere)`.
- ACCESS CHECK — RESOLVED (verifier): `RndDrawable::SetSphere(const Sphere&)` is
  **PUBLIC** (src/system/rndobj/Draw.h:72 — the class is `public:` from line 27 with
  no intervening `protected:`/`private:`; `GetSphere`/`MakeWorldSphere` are public
  too). The engine (a non-member) can call `mesh->SetSphere(localSphere)` directly —
  **no accessor / no friend needed.** (Precedent: `RndMesh::UpdateSphere` already
  calls `RndDrawable::SetSphere(s)` at Mesh.cpp:189.)
- Sphere API (src/system/math/Sphere.h): `Sphere` has **public direct members
  `center` (Vector3) + `radius` (float)**, plus `Set(const Vector3&, float)`,
  `Zero()`, and ctor `Sphere(const Vector3&, float)`. There is **no `SetCenter` /
  `SetRadius`** — build the local sphere with `localSphere.Set(center, radius)` (or
  the `Sphere(center, radius)` ctor).
- Skip skinned meshes (leave their zero sphere — never culled).
- Sphere is LOCAL space; the cull test transforms via `MakeWorldSphere` (WorldXfm), so
  static venue meshes with animated transforms still cull correctly.

**Part 2 — world.cam-scoped cull (rb3, Draw.cpp HX_NATIVE arm):**

> **SEMANTICS — VERIFIED (do not invert):** `RndCam::CompareSphereToWorld(s)`
> (Cam.h:63) returns `s > mWorldFrustum`, i.e. `operator>(Sphere, Frustum)`
> (Geo.cpp:932-941). It returns **`true` ⇒ the sphere is FULLY OUTSIDE the frustum
> ⇒ CULL (skip the draw)**; `false` ⇒ visible (or intersecting) ⇒ DRAW. Confirmed
> against the Wii arms of `Draw()`/`DrawBudget()`: `Draw()` draws iff
> `worldSphere == 0 || !CompareSphereToWorld(sphere)` (Draw.cpp:176), and
> `DrawBudget()` skips iff `worldSphere != 0 && CompareSphereToWorld(sphere)`
> (Draw.cpp:202). So the helper below returning `CompareSphereToWorld(s)` as
> "true → skip" is **CORRECT** — returning its negation would cull everything
> visible and draw only off-screen geometry.

Add file-static helper in the existing HX_NATIVE block (before the `#endif` at
Draw.cpp:156):
```cpp
static bool RB3VenueFrustumCull(RndDrawable* d) {
    static int sOn = -1;
    if (sOn < 0) { const char* e = getenv("RB3_VENUE_FRUSTUM_CULL");
                   sOn = (e && e[0] && e[0] != '0') ? 1 : 0; }
    if (!sOn) return false;
    RndCam* cam = RndCam::sCurrent;
    if (!cam || !cam->Name() || strcmp(cam->Name(), "world.cam") != 0) return false;
    Sphere s;
    if (!d->MakeWorldSphere(s, false)) return false;   // false/radius==0 → never cull
    return cam->CompareSphereToWorld(s);               // true → fully outside → skip
}
```
(This matches the already-vetted reference helper in `wave-04.md` §8, lines 627-638.)
Call after the `MenuVoidDrawHook` early-outs in `Draw()` (~:168) and `DrawBudget()`
(~:188). world.cam-only scoping means game.cam (highway), UI cams, and the 2D overlay
are untouched by construction.

**Flag policy:** land default-OFF (`RB3_VENUE_FRUSTUM_CULL=1` to enable); flip to
default-ON (with `_OFF` opt-out) in a follow-up commit once acceptance passes — same
pattern as track-lighting/bloom rollouts.

## Acceptance

1. **Pixel A/B, flag off** = byte-identical (it's all flag-gated).
2. **Flag on, correctness**: `_framepin_capture.py` / visual_diff_capture.py on
   splash, main_hub, song_select, in-song venue: no missing visible geometry vs
   baseline (strict gate caveat: culling legitimately changes nothing visible — diff
   should be EMPTY, making strict mode the right tool).
3. **Flag on, perf**: RENDER_DBG `mDrawnMeshes` on splash drops from ~983 (the
   full venue draw count — wave-04.md / wave-03.md) by the genuinely-off-screen
   fraction. CAVEAT on the target: wave-04's measured 983→677 was with the BROKEN
   Xbox sphere over-culling (~31% / 306 meshes, some of them visible); with the
   recomputed tight sphere (Part 1) the surviving count should be HIGHER than 677
   (~700-ish) with NO visible loss. `web-stutter-probe.mjs` splash rAF p50 well
   below the current ~100 ms.
4. Wii match untouched (Draw.cpp edits inside HX_NATIVE; run a match check on the TU).
5. Engine commit first, then rb3 commit (Draw.cpp + pin bump if bundled with 04/06).

## Coordination

Same engine file (`Rnd_Wgpu_RB3.cpp`) as 04/06 — check `git status` in the engine
repo before editing; serialize with those handoffs. As of verification (2026-06-09):
engine branch is **`main`** (not `master`), HEAD `b5309b3`, working tree CLEAN; rb3
`MILO_ENGINE_PIN` already `b5309b3`. The Part-2 rb3 edits land in
`src/system/rndobj/Draw.cpp` (a DECOMP/Wii-match TU — keep them strictly inside the
existing `#ifdef HX_NATIVE` arms; the `#else` Wii path must stay byte-identical).

## IMPLEMENTED (2026-06-09)

**Status: DONE.** Implemented as prescribed; default-OFF as instructed.

- **Part 1 — engine** (`milo-native-engine` commit `f8544a5`): in
  `Rnd_Wgpu_RB3.cpp` DrawMesh, inside the `if (needUpload)` block (so once per
  geometry generation), recompute a tight LOCAL sphere from the just-unpacked
  positions for **non-skinned** meshes only: AABB min/max over `gpuVerts[i].pos`
  -> center = AABB midpoint, radius = 0.5 * diagonal length, then
  `localSphere.Set(center, radius); mesh->SetSphere(localSphere);`. Skinned
  meshes keep their zero sphere (never culled). `Sphere`/`SetSphere` reachable
  via the already-included `rndobj/Mesh.h`; `<cmath>` already present.
- **Part 2 — rb3** (commit `1f09fce2`): added file-static `RB3VenueFrustumCull`
  before the `#endif` at Draw.cpp:156 (env-gated `RB3_VENUE_FRUSTUM_CULL`,
  world.cam-scoped, returns `CompareSphereToWorld(s)` un-inverted = "true => skip"),
  and called it after the MenuVoidDrawHook early-outs in both `Draw()`
  (`if (RB3VenueFrustumCull(this)) return;`) and `DrawBudget()`
  (`if (RB3VenueFrustumCull(this)) return true;` — matches the Wii culled path).
- No pin bump.

**Acceptance measured (native):**
- (#1) Flag OFF is structurally a no-op: `RB3VenueFrustumCull` returns false
  immediately when the flag is unset, so `Draw()`/`DrawBudget()` fall through to
  the pre-existing `DrawShowing()`/`DrawShowingBudget()` unchanged. (Pixel A/B on
  the main_hub venue is NOT a usable gate — the rooftop venue is non-deterministic
  across boots: two flag-OFF boots frame-pinned at f800 already differ by 53% of
  pixels purely from backdrop/lighting animation. song_select, which draws under
  UI cams where the cull is a no-op by construction, is visually identical OFF vs
  ON, confirming the gate.)
- (#3 perf) RENDER_DBG world.cam drawn-mesh count: OFF median **299** (max 359)
  -> ON median **187** (max 253), a ~37% / ~112-mesh-per-frame drop. (Lower
  absolute numbers than wave-04's 983 because this `meshes=` field is the per-cam
  draw count, not the whole-frame total; the relative drop is the signal.)
- (#2 correctness) Temporary `RB3_CULL_DBG` audit (added, run, then removed
  before commit) logged the 302 unique culled meshes under main_hub world.cam:
  ALL are distant city backdrop (bgbuildings_* / building_* at x=-2000..-6000,
  factory_* / chimney_05 at z=-10241), peripheral signage (adboard/billboard/
  capitolsign), off-screen crowd/cars, and the off-frame drum kit (kick/snare/
  tom/cymbal at x~-100, outside the menu camera frustum). NO visible foreground
  band/guitar/bass/character geometry is culled — confirmed by side-by-side band
  crops (same 3 standing band members present OFF and ON). The menu camera does
  not frame the drum kit, so culling it loses nothing visible.

**Wii match safety:** all Draw.cpp edits are HX_NATIVE-gated; `rndobj/Draw`
stays 91.9789% code / 96.7742% funcs (report.json unchanged after
tools/ninja-locked).

**Deviations:** the strict-empty pixel-diff gate (#2) is not applicable to the
main_hub venue (53% OFF-vs-OFF animation noise floor swamps any cull signal); I
substituted the more rigorous per-mesh cull-name audit + foreground crop check,
which directly proves no visible geometry is dropped. Default-ON flip deferred to
a follow-up per the handoff's flag policy.
