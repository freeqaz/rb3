# task-neon-slab — menu-hub "green slab": root cause + fix

Wave-2 implementer, 2026-06-11. Worktree pair `task-neon-slab`
(rb3 branch `wt-task-neon-slab`, engine branch `wt-task-neon-slab`).
Evidence: `/tmp/rp2-neon-slab/`.

## TL;DR — the scout's attribution was wrong; the bug was PARTICLES, and it's fixed

The hub's giant green wash was **not** `neon_arcade.mesh` mis-decoding. That mesh
decodes **cleanly** (proven below) and is *behind the camera* in the affected
shots. The green was the venue's **street-fog particle systems running away**:
two decomp bugs in `RndParticleSys::InitParticle` (shared `src/system/rndobj/Part.cpp`)
gave every particle (a) a velocity equal to the emitter's world position
(~341 u/frame instead of ≤0.01) and (b) a per-step color delta ~100× too big
(colors raced to saturated cyan; cyan × warm `fog.tex` = green). Both fixes are
**proven against the Bank-8 target asm** and **improve the Wii match**
(InitParticle 95.3% → 97.7%). After the fix the hub loop has **zero** green-excess.

## What changed (files + why)

### rb3 — `src/system/rndobj/Part.cpp` (THE FIX) — commit `5be2e34f` on `wt-task-neon-slab`

`RndParticleSys::InitParticle(float, RndParticle*, const Transform*, PartOverride&)`:

1. **vel/bubbleDir transform: rotation-only.** The tail transformed
   `Vel3()`/`Bubble3()` with the full `Transform` overload of `Multiply`
   (rotation **+ translation**). Target asm (`80903878+` in
   `build/SZBE69_B8/asm/system/rndobj/Part.s`): `pos` accumulates from
   `psq_l 0x24/0x2c(tf)` (= tf->v, point transform) but `vel` and `bubbleDir`
   start with `ps_muls1` row products — **no translation load**. Fixed to
   `Multiply(particle->Vel3(), tf->m, particle->Vel3())` (and same for
   `Bubble3()`). The bug added the emitter's world position into every
   particle's velocity — on the shell street (y≈−341) particles flew to
   y≈−11,800 within ~100 frames at sizes 100–200.
2. **midcolVel scaling.** The fancy mid-color block scaled `colVel` a *second*
   time by `1/(midcolFrame−birthFrame)` instead of scaling `midcolVel`
   (target stores the `fmuls` results to `particle+0x70..0x7C` = midcolVel).
   Result pre-fix: `midcolVel` = raw `(midcol − startCol)` applied per step →
   colors clamped to 0/1 within frames (the saturated cyan), `colVel` ≈ 0.
3. Adds `PART_INIT_DBG=<substr>` / `PART_MOVE_DBG=<substr>` probes — both
   inside `#ifdef HX_NATIVE` (compiled out of the Wii build).

**Wii-match safety**: this is non-ifdef'd shared code, deliberately — it is a
genuine decomp bug fix. Rebuilt `Part.o` in the worktree via objdiff:
InitParticle **95.3% → 97.1% → 97.7% normalized**; all insert/delete diffs
eliminated (remaining = pre-existing regswap/permuter noise). No other
function in the TU is affected (no header changes).

### engine — `src/platform/Rnd_Wgpu_RB3.cpp` (diagnostics only) — commit `7d6252d` on `wt-task-neon-slab`

No behavior change; env-gated probes used for the diagnosis, kept for future
render triage (same style as GEM_VTX/SKIN_PROBE/CAM_DBG):

- `MESH_DUMP=<substr>` — one-shot decoded-geometry dump (nv/nf, index range,
  OOB + degenerate face counts, NaN census, top-8 triangles by area, worldXfm)
  + throttled `MESH_FOOT` line projecting the cached local bbox through the
  current cam → NDC rect + behind-camera corner count.
- `RB3_ISOLATE_MESH=<substr>` — DrawMesh draws ONLY matching meshes (pixel
  attribution that survives camera-loop misalignment between runs).
- `PART_PROBE=1` — per-system dump in DrawParticles (sys/mat/tex/blend/count,
  `CalcFrame`, per-particle col/colVel/vel/size/pos/birth/death).

**The rb3 fix does NOT depend on the engine commit** — no `MILO_ENGINE_PIN`
bump is needed or included. Land the engine diagnostics independently if wanted.

## The investigation (how the scout's hide-test attribution fell apart)

1. `MESH_DUMP=neon_arcade` (first draw): nv=5664 nf=3296, **0 OOB / 0 degenerate
   / 0 NaN**, tight local bounds (±10.5 × ±0.56 × ±70.2 — extruded "ARCADE"
   letters, 6 letters × ~23 u), largest triangle area only 13.3. Decode CLEAN.
   Stride fine too (`STRIDE_PROBE`: every mesh in the dir is 36 B/vert).
2. `MESH_FOOT` across the whole hub loop: in the green-slab shots the mesh's
   8 bbox corners are **all behind the camera** (forward depth −120…−233) —
   correctly clipped, contributes nothing; in other shots it's off-screen
   right (ndcX≈4) or a small legit right-edge sign. It can't be the slab.
3. `RB3_ISOLATE_MESH=neon_arcade` (only that mesh allowed through DrawMesh):
   frames **still show a full-screen green CLOUD texture**
   (`/tmp/rp2-neon-slab/iso_f02700.png`, 82% of frame) — i.e. the green pixels
   come from a path NOT gated by DrawMesh = `BandRnd::DrawParticles`
   (billboards). The scout's hide test was confounded by the camera-shot loop
   advancing between the on/off captures (my tight paired hide-test even showed
   green *increasing* after hiding the mesh).
4. `PART_PROBE`: `forground_smoke01.part` / `background_red_fog*.part`
   (mat `fog_thin/fog.mat`, tex `fog.tex`) particles at age ~100 frames:
   pos y≈−11,800 (emitter at −341), size 100–200, col (0,1,1,1) saturated cyan.
   Birth state sane. File ground truth (parsed `sv3_a.milo_xbox` directly —
   `/tmp/rp2-neon-slab/milo_part_dump.py`): life (75,200), speed (0.01,0),
   start colors warm amber, end colors ~0.2–0.3 — all sane → runtime sim bug.
5. `PART_INIT_DBG`: init outputs all correct (box emit, unit dir, speed scale) —
   so the corruption was in the post-init transform → read the target asm tail
   of InitParticle → found bug 1; after fixing it the colors still raced →
   read the mid-color asm block → found bug 2.

## VERIFICATION (before/after)

Green-excess metric = fraction of pixels with `G − max(R,B) > 0.2`
(scout's metric). Harness: `/tmp/rp2-neon-slab/hub-series.py`
(`RB3_REPO=<worktree> python3 hub-series.py --port P --out-prefix X`).

| run | frames sampled | greenFrac range |
|---|---|---|
| BEFORE (pre-fix, this worktree: `foot_f*.png`) | 12 over f450–2100 | 0.000 – **0.479** (3 shots > 0.18) |
| velocity fix only (`after_f*.png`) | 12 over f450–3200 | 0.000 – **0.392** (cyan fog parked at street) |
| BOTH fixes (`after2_f*.png`) | 12 over f450–3200 | **0.0000 everywhere** |
| BOTH fixes, no probe envs (`final_f*.png`) | 2 spot checks | **0.0000** |

(Shots are not frame-locked across runs — the camera loop drifts with boot
timing — so whole-loop metrics, not per-frame pairs, are the valid comparison.)

- Scene sanity: `after2_f01450/2700.png` — warm amber street, posters/tent
  visible, sky clouds drifting normally (vel ~1.5–2 u/frame), fog subtle at
  emitter. Matches the retail hub's warmth direction
  (`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`).
- Regression — gameplay: `keyboard-to-gameplay.py --diff hard` PASS
  (game_screen, songMs advancing); `/tmp/rp2-neon-slab/gameplay/burst_*.png`
  highway/gems/venue normal.
- Regression — song select: `song-select-capture.py` PASS, 5 frames normal
  (`/tmp/rp2-neon-slab/songselect/`).
- Pass criterion 5 from scout-menu-lighting ("no green slab ≥5% of frame
  across a full hub loop") — **met** (0.0000).

## Side findings (for other tasks)

- **`/api/dta/eval` Color sub-property reads are broken/crashy**: `{$s get
  (start_color_low r)}` SIGABRTs; `(end_color_low r)` returned garbage
  (849/1591 — what initially looked like memory corruption) on one build and
  crashed on another. The earlier "end colors corrupted in memory" readings
  were getter artifacts. Worth a small DTA-eval fix sometime (not render).
- This particle fix is venue-generic: gameplay venues' fog/smoke/pyro and the
  smasher hit-flames (A1) all run the same InitParticle; their motion/color
  ramps are now per the Wii asm. The A1 flame look may change slightly —
  *more* correct (velocities no longer carry the relXfm translation).
- scout-menu-lighting §2d should be considered RESOLVED by this task with a
  different root cause; §2a/2b (unlit materials / emissive) remain open and
  unaffected by this change.

## LANDING NOTES (orchestrator)

- **rb3 `wt-task-neon-slab` @ `5be2e34f`** — single-file change
  (`src/system/rndobj/Part.cpp`). This is a **shared Wii-matched source edit
  NOT behind ifdef** (intentionally: it's a decomp bug fix, match-verified
  95.3→97.7 via objdiff in the worktree). After landing, the permuter DB may
  want a re-ingest for InitParticle.
- **engine `wt-task-neon-slab` @ `7d6252d`** — optional, diagnostics only,
  independent of the rb3 fix. **No pin bump included or required.** If landed
  with other engine work, it touches `Rnd_Wgpu_RB3.cpp` DrawMesh (near the
  GEM_VTX probe, the RB3_SKIP_* block) and DrawParticles — likely conflict
  surface with `fret-held` / `menu-lighting` / `gem-polish` engine edits in
  the same file; resolve by keeping both probe blocks (they're additive).
- Conflict risk in rb3: low — only `rndobj/Part.cpp`; no header changes; no
  other wave-2 task is known to touch it.
- Wave follow-up suggestion: re-run the menu-lighting scout's hue/contrast
  measurements AFTER this lands — the green wash contaminated its global hue
  metrics (R:B ratio), so fixes 1/2 of scout-menu-lighting should be re-baselined.
