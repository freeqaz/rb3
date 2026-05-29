# CROWD_GUARD_PLAN — refine the V24 ratio-guard so legit crowd bodies render (N5b)

**Authored:** 2026-05-29 (Opus read-only planning subagent — NO build, NO game
run, NO source edit; this is the only file written).
**Scope:** Thread (a) of `VENUE_RENDER.md` V38's "Next step for whoever picks up
N5". Refine the engine **V24 ratio-guard** so it stops false-positive-dropping
*uniformly Y-squashed* legit crowd bodies (world det ≈ 0.533) while still culling
the real screen-crossing slivers (teal blade, rainbow prop fan).
**Explicitly OUT OF SCOPE:** thread (b), the deep `CharFaceServo` / hand-servo rig
work that would faithfully pose the tiny-bind face/finger servo bones. That is a
high-risk character-animation task with documented regression danger to the band
players (V21/V26) and is tracked separately. This plan does **NOT** touch any
matched-fork char/skeleton code.
**Builds on:** `CROWD_SLIVER_DIAGNOSIS.md`; `VENUE_RENDER.md` V24 (the guard
itself) + V38 (the false-positive root-cause localization).

---

## 1. Current guard: exact location, metric, why it false-positives

### 1a. Location (engine layer)

- **File:** `/home/free/code/milohax/milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
- **The DROP itself:** lines **1405–1493** (the "V24: degenerate-skinned-triangle
  (teal shard) guard" block), inside the per-mesh skinned draw path of
  `DrawMesh`. The decisive lines:
  - **L1472** — the metric:
    `bool degenerate = (wext > 15.f) && (lext > 0.001f) && (wext > 2.0f * lext);`
  - **L1482–1491** — when `degenerate && guardActive`, it bumps `mDrawnMeshes`
    and `return`s (skips the draw entirely).
  - **L1422–1455** — computes `lext` (bind-pose / local AABB diagonal length) and
    `wext` (the blended **world** AABB diagonal, via the exact 4-bone palette blend
    the shader uses, sampling ≤256 verts), feeding the metric.
- **Related (same file, NOT the guard but co-located):** L1311–1338 the per-bone
  composed-skin finiteness guard (V24's *other* half; correct safety net, leave
  it); L1342–1377 `SHARD_BONE_DBG`; L1351-1377 + V38's `SHARD_M`/`SHARD_CHAIN`
  parent-chain det walk; L1475–1481 `SHARD_RATIO_DBG` logging.

### 1b. Layer — **ENGINE, not matched-fork. UNAMBIGUOUS.**

`src/platform/Rnd_Wgpu_RB3.cpp` is tracked in **milo-native-engine** (`git ls-files`
confirms) and is **not** under any decomp `src/system/**`. It is the engine
runtime layer (Clang LP64, on the native link line) — the deliverable layer. The
permuter never touches it; no `#ifdef HX_NATIVE` shape is required. Edits here are
ordinary engine edits.

### 1c. Current metric and the false positive

Current metric is a single **whole-mesh bind-vs-world AABB diagonal ratio**:
`wext / lext > 2.0` (with absolute floors `wext > 15u`, `lext > 0.001u`).

V38 proved (via `SHARD_M`/`SHARD_CHAIN` det walk) that the crowd-body "ratio 8.8 /
8.9" hits are **FALSE POSITIVES**:
- `female_crowd_body02` (ratio 8.8) / `male_crowd_body01`: bind-offset det = 1.000,
  every bone's `LocalXfm` det = 1.000, **but each bone's `WorldXfm` det = 0.533**.
  Walking neck→spine→pelvis→`crowd_female02` (the character root, no parent,
  constraint kNone), EVERY node reports localDet 1.0 / worldDet 0.533. The root's
  `mWorldXfm` was `SetWorldXfm`'d directly by the crowd-spawn / instancing path
  with a **uniform det-0.533 (Y-squashed) world transform** — a deliberate
  stylized short crowd member.
- Why the ratio mis-reads it: a *uniform* (or near-uniform) scale `s<1` applied to
  a posed body shrinks both axes but the body is still articulated (an arm swung
  out, a lean) so the **world** AABB diagonal can still be a large multiple of the
  rest-pose bind AABB — the ratio sees `wext/lext` ≫ 2 and drops a perfectly
  legitimate body. The metric conflates "articulated pose spread under a uniform
  placement scale" with "one vertex flung far by a broken bone".

Visual confirmation (screenshots): in `v38-crowd-slivers/baseline-guardoff/03_f0540.png`
& `04_f0560.png` the *real* slivers are the large teal blade (top-left) and the
center rainbow prop fan — unmistakable 1–2px screen-crossing triangles. In
`v20-characters/01_f0505.png` & `v21-band-players/01_f0570.png` the crowd bodies
are upright, recognizable silhouettes (some are short/squashed) — these are what
the current guard wrongly culls and what the refined guard must keep.

---

## 2. Refined metric / design (concrete)

**Core idea (from V38 thread (a)): divide out the per-bone uniform world scale
before the ratio test, so a uniform placement-squash no longer inflates the ratio,
while a single mis-posed bone (which is NOT uniform across the palette) still does.**

The composed skin matrices the guard already blends with — `bones.bones[bi]`,
column-major, L1441 — carry exactly the world scale. Each bone `b`'s 3×3 basis
determinant `det_b = det(skin3x3)` is the signed volume scale of that bone in world
space (offset det is ~1.0, so this is the bone's WorldXfm scale). For a uniformly
placed crowd body EVERY `det_b ≈ 0.533`; for a body with one broken servo bone, that
bone's `det_b` is an outlier while the rest are ~1.0 (or ~`s`).

### Design A (PRIMARY, recommended) — uniform-scale-normalized ratio

1. In the same vertex loop that already builds the world AABB (L1431–1453), also
   accumulate, **per contributing bone index actually used by this mesh's sampled
   verts**, that bone's basis determinant. Cheap: compute `det3` of each
   `bones.bones[bi]` once into a small `float boneDet[kMaxBones]` lazily (or compute
   on first use, cache per draw). `det3` of the column-major basis:
   `det = m0*(m5*m10 - m6*m9) - m4*(m1*m10 - m2*m9) + m8*(m1*m6 - m2*m5)`
   using the `MiloXfmToColMajor` indices (cols 0/4/8 = basis rows).
2. Reduce the per-mesh **uniform scale estimate** `sUni` from the bones that this
   mesh's verts actually weight to — use the **median** (robust; an outlier broken
   bone won't move the median) of `cbrt(|det_b|)` over the contributing bones.
   (cbrt converts a volume scale to a linear scale comparable to AABB diagonals.)
3. Normalized metric:
   `ratioNorm = wext / (lext * sUni)`.
   - For a uniform det-0.533 body: `sUni = cbrt(0.533) ≈ 0.811`; the world AABB is
     ~0.811× plus articulation, so `ratioNorm` collapses back into the legit
     1.0–1.9 band → **NOT dropped**.
   - For a broken-bone sliver: `sUni ≈ 1.0` (median is clean), `wext` is huge from
     the one flung vert → `ratioNorm` stays 8–42 → **still dropped**.
4. **Threshold:** keep the existing `wext > 15u` and `lext > 0.001u` floors, and
   set `degenerate = ratioNorm > 2.0`. (Same 2.0 separation V24 measured for the
   legit 1.0–1.9 band vs. the 2.0–12× shard band — the normalization just removes
   the uniform-scale bias before the comparison.)

### Design B (SECONDARY, additive belt-and-suspenders) — per-bone outlier gate

Because the *visible* residual shards (eyebrows ratio 42, goatee 17, clap/lighter
14–16) are **single mis-posed servo bones in a multi-bone tiny-bind mesh** (V38
sub-class 2), an alternative/confirming signal is **per-bone scale dispersion**:
compute `maxDet = max |det_b|`, `medDet = median |det_b|` over the mesh's
contributing bones; flag `degenerate` when `maxDet / max(medDet, 1e-3) > 4.0`
**OR** Design-A `ratioNorm > 2.0`. The dispersion test fires precisely on
"one bone is wildly scaled relative to the rest" (the true sliver signature) and is
**immune** to uniform squash (all dets equal → dispersion ≈ 1.0). This is the
"per-triangle edge-stretch / max-edge-ratio" spirit V38 suggested, expressed at the
bone level (cheaper and more direct than re-walking triangles). Recommend shipping
A as the change and optionally OR-ing B; B alone could be used if A proves noisy.

### What we deliberately do NOT do (rejected, per V24 L1465–1468)

A pure max-triangle-edge test and an absolute world-span cap were already tried and
rejected — crowd-row meshes are large/batched with legitimately long edges and big
AABBs, so those metrics gutted the crowd. The bone-determinant normalization avoids
that trap because it normalizes *out* the legit scale rather than thresholding raw
spans.

### New diagnostics to add (env-gated, OFF by default; same style as L1475)

- Extend the existing `SHARD_RATIO_DBG` log line (L1479) to also print `sUni`,
  `ratioNorm`, `maxDet`, `medDet` so before/after measurement is one grep. No new
  env var needed; reuse `SHARD_RATIO_DBG`.

---

## 3. Files to edit — full paths + layer

| # | File | Layer | Edit |
|---|------|-------|------|
| 1 | `/home/free/code/milohax/milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` | **ENGINE** | The ONLY code edit. Add `det3` helper (or inline), per-bone det cache, `sUni` median + (optional) dispersion in the guard block L1422–1493; replace the L1472 `degenerate` expression with the normalized metric; extend the L1479 `SHARD_RATIO` log. |
| 2 | `/home/free/code/milohax/rb3/docs/sessions/native/VENUE_RENDER.md` | docs | Append a "V39 (or N5b)" section recording the refined metric + before/after DROP counts. (Doc only; not load-bearing for the build.) |

**No matched-fork edits. No char/skeleton edits.** Threads (b)'s `CharFaceServo`
work is explicitly NOT in this plan.

### Engine-file conflict with the RTT-outfit task: **YES.**

This task edits `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — the **same
engine file** the RTT-outfit (render-to-texture) task edits. **These two tasks MUST
serialize** (or be merged into one engine branch / worktree, or hand off the file
sequentially). They touch different regions (this one: the skinned-mesh guard block
~L1405–1493; RTT: the material/texture path), so a textual merge is plausible, but
do not run them as two independent concurrent writers against the same working
tree. Recommend: one engine worktree owns `Rnd_Wgpu_RB3.cpp`; the two changes land
as two commits in that worktree, RTT first or this first, reviewer's choice.

---

## 4. Effort / risk / verification

### Effort: **S** (small)

Single localized engine change: one `det3` helper, a small per-bone det cache, a
median reduction over the contributing bones, and a one-line metric swap, plus a
log-line extension. No new pipeline, no asset change, no matched-fork tracing.
~40–70 lines in one function.

### Regression risk: **LOW**, with one watch-item.

- **Band players (V21/V26) — the priority canary:** LOW risk. The change only
  affects which skinned meshes are DROPPED; band-player bodies already pass (V24
  verified NO band-player body is dropped today). Relaxing the metric (normalizing
  out uniform scale) can only *keep more* meshes, never newly drop a body that
  currently passes — so band players cannot regress from the normalization. The
  ONLY way to newly drop something is the optional Design-B dispersion gate; keep
  its threshold conservative (4.0) and verify band players stay drawn.
- **Re-introducing shards:** MEDIUM-LOW. By design the real slivers
  (eyebrows/goatee/clap/lighter) keep firing because their `sUni` median is clean
  (~1.0) so `ratioNorm` stays high; verify the top-ratio shards still DROP after
  the change (they must not start passing).
- **Cost:** negligible — the det cache is ≤`kMaxBones` cbrt/det ops per skinned
  mesh, dwarfed by the existing ≤256-vert blend loop.
- **Pre-flight (V32/V38 hazard):** the permuter periodically wipes the V21
  (`src/system/math/Mtx.h:~640` empty-body `Multiply`) and V26
  (`src/system/math/Rot.cpp:~485` `MakeRotQuat` half-angle) HX_NATIVE blocks in
  the **rb3** repo. Before measuring, `grep -c HX_NATIVE src/system/math/Mtx.h
  src/system/math/Rot.cpp` → expect `1`/`1`; re-apply if wiped, or the residual
  measurement is contaminated by the dominant explosions returning. (These are
  matched-fork files NOT edited by this task — only checked.)

### Verification (sliver-ratio + DROP-count before/after; frames to screenshot)

Reproducer (V38's, reaches the crowd-cinematic window where these meshes appear):

```
SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1 \
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
RB3_DATA=$PWD/orig-assets/extracted MILO_MAX_FRAMES=500 \
RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@380:track:guitar,@450:msg:overshell:end_override_flow:1:0,@500:nofail" \
./native/build-native/rb3-native
```

1. **Baseline (current tree), guard OFF + `SHARD_RATIO_DBG`:** record the V38
   numbers — DROP/SHARD_RATIO ≈ **376 / 870** lines; top ratios eyebrows 42.1,
   goatee 17.1, clap 15.9, lighter 14.2, crowd_body03 8.9, crowd_body02 8.8.
   Confirm crowd_body02/03 currently report a high (false-positive) ratio.
2. **After the change, guard OFF + `SHARD_RATIO_DBG`:** confirm `crowd_body02/03`'s
   new `ratioNorm` collapses into the **1.0–1.9 legit band** (no longer flagged
   DROP), while eyebrows/goatee/clap/lighter `ratioNorm` stay **>2.0** (still
   flagged). Expected: total DROP count falls (the ~legit-crowd-body DROPs removed)
   but the visible-shard DROPs remain.
3. **After the change, guard ON (production):** the real teal blade + rainbow prop
   fan stay culled; the previously-dropped short crowd bodies now render.
4. **Frames to screenshot** (compare against the existing baselines):
   - `f0540`, `f0560` (the V38 shard frames) — confirm teal blade/prop fan still
     gone, crowd denser/more complete than `after-guardon/01_f0540.png`.
   - `f0505`, `f0570` crowd/band frames (cf. `v20-characters/01_f0505.png`,
     `v21-band-players/01_f0570.png`) — confirm band players intact, more legit
     short crowd bodies present, no new shards.
   Save under `docs/sessions/native/screenshots/v39-crowd-guard/{before,after}/`.
5. **Band-player canary:** with the optional Design-B gate on, grep the
   `SHARD_DBG`/`SHARD_RATIO` output for any band-player body mesh name appearing as
   `DROP`; must be zero.

Success signal: crowd_body DROPs disappear from the SHARD_RATIO log; eyebrows/
goatee/clap/lighter DROPs persist; band players and venue unregressed; the f0540/
f0560 frames show no large shard yet a fuller, shorter-member-inclusive crowd.

---

## TL;DR (for the dispatcher)

- **Guard location:** `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` **L1405–1493**
  (DROP metric at **L1472**), **ENGINE layer** (not matched-fork).
- **Refined metric:** normalize the bind-vs-world AABB ratio by the mesh's
  *uniform* world scale — `ratioNorm = wext / (lext * sUni)` where
  `sUni = median(cbrt(|det(boneBasis)|))` over the mesh's contributing bones;
  `degenerate = ratioNorm > 2.0` (keep `wext>15u`, `lext>0.001u` floors).
  Optional belt-and-suspenders: per-bone det dispersion `maxDet/medDet > 4.0`.
  This keeps culling single-flung-bone slivers (eyebrows 42, goatee 17, clap/
  lighter 14–16) while NOT dropping uniformly det-0.533 short crowd bodies.
- **Files to edit:** ONE code file — `Rnd_Wgpu_RB3.cpp` (engine) — plus an optional
  VENUE_RENDER.md note.
- **Engine conflict with RTT-outfit task: YES** — both edit `Rnd_Wgpu_RB3.cpp`;
  they must **serialize** (different regions, but same file → one worktree, two
  commits).
- **Effort S; risk LOW** (relaxation can't newly drop band bodies; verify shards
  still drop + pre-flight the V21/V26 HX_NATIVE blocks are intact).
