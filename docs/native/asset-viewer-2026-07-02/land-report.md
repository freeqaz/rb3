# Lane G — land + visual gate report (2026-07-02)

**Result: PASS. Committed `src/system/char/CharHair.cpp` only → `81f38f3a`.**

Lane G ran after Lane H (CharHair CFG fix) and Lane V (rb3-viewer) landed on
disk. This report records the objdiff re-verify, the native rebuild, both visual
gates, and the commit.

## 1. Fix confirmed on disk + objdiff re-verify

- `git diff src/system/char/CharHair.cpp` = the Lane-H brace move only (19/19,
  indentation): the per-point bone-update tail was dedented out of
  `if (thisPoint.collides.size() != 0)` so it runs unconditionally per strand
  point (mirrors DC3 :310-395).
- `run_objdiff SimulateInternal__8CharHairFf` = **99.6% (99.6% raw)**, 494 insns,
  only `2 diff_arg + 2 delete` remaining — the `[259-262]` `lfs f0/f1 0x50/0x54,
  r26` regalloc/reload residual. The giveaway CFG mismatch `beq 0x346c vs
  0x4ad8` is **GONE**. Matches the H handoff exactly; no regression.

## 2. Native rebuild

`cmake --build native/build-native --target rb3-native` → `ninja: no work to
do` (Lane V already compiled the on-disk fix; binary mtime 05:52 > CharHair.cpp
05:36, so the fix is in the binary). Build green.

## 3. Viewer gate (rb3-viewer --sim 30)

`native/build-native/rb3-native --viewer <hair.milo_xbox> --sim 30` on:
- `male_hair_crazyhawk_resource.milo_xbox` → `/tmp/rb3-viewer-gate/crazyhawk_sim30.png`
  (exit 0; "--sim 30 steps over 1 CharHair object(s)"; coherent spiky-hawk geo)
- `male_hair_ziggymullet_resource.milo_xbox` → `/tmp/rb3-viewer-gate/ziggymullet_sim30.png`
  (exit 0; "--sim 30 steps over 2 CharHair object(s)"; coherent mullet geo)

Both render coherent hair geometry with zero `Can't make`. Note (per Lane V): the
viewer draws the strand mesh **un-skinned**, so `--sim` does not visually reflect
the CFG fix here — the sim runs (proven by the poll log) but the frame shows rest
pose. This is why the in-game gate below is the authoritative visual check.

## 4. In-game gate (band-closeup-capture.py, authoritative)

`RB3_HEADMAT_DBG=1 python3 scripts/native/band-closeup-capture.py --member all
--frames 2 --frame-dt 600` re-rolled runs 1-8 (prefab lineup rotates per launch)
until `ziggymullet` appeared in the HEADMAT log.

- **run7** (`/tmp/wig-gate/run7/`, engine log `/tmp/rb3-bandcloseup-g7-35725.log`)
  rolled `male_hair_ziggymullet_resource.milo` in the lineup.
  `[HEADMAT] mesh='ziggymullet_resource.mesh' dir='outfit' mat='hair_ziggymullet.mat'`.
- Harness **verdict=PASS**, `pinned=34/34`, `drops_total=0`, `drops_band=0`.
  (All 8 runs PASSED.) The broken baseline run1 had `drops_other=1135`
  (scrollbar_bg/fist/male_extras — the known-deferred non-hair artifacts, not the
  wig); no band-mesh drop in either state → no shard regression from the fix.
- **Visual:** the drummer's ziggymullet reads as a coherent orange/red
  **back-hanging layered mullet** with the face clear above it — no strands
  through the face, no pale draped ribbons. Best view:
  `/tmp/wig-gate/run7/g7_coop_front_n01_0.png` (+ brightened
  `/tmp/wig-gate/bright_g7_coop_front_n01_0.png`). Guitarist (short/shaved hair)
  in `g7_coop_g_b_0.png` shows no drape either.
- Contrast with the pre-fix broken baseline `/tmp/wig-bug/run1/r1_coop_g_b_0.png`
  ("pale ribbons draped over face") / `r1_coop_b_cg_1.png` ("dark slab through
  face"): none of the post-fix members exhibit that collapse.

Caveat: a pixel-exact same-member/same-hair A/B pre/post fix was not possible —
the prefab lineup rotates per launch and reverting the build in the main repo is
disallowed (no stash/checkout). The gate is therefore a coherence judgment,
backed by the asm-level CFG proof (§1) and the harness PASS.

Note on `ratio_lines=0` in my runs: I did not export `SHARD_RATIO_DBG=1`, so that
counter was off. That metric tracks footwear (maleslipons) shard-**expansion**,
not the hair-**collapse** pose bug this fix addresses — so it is not the right
signal here; visual coherence is.

## 5. Commit

`81f38f3a` — `fix(char): CharHair::SimulateInternal bone-update tail ran only for
colliding points`. Staged **only** `src/system/char/CharHair.cpp`. No
Co-Authored-By. Cites the mis-scoped brace, the `.L_806D002C`/`beq` asm evidence,
objdiff 99.6%→99.6% (CFG mismatch eliminated), and native+web hair fix.

## Captures

- `/tmp/rb3-viewer-gate/crazyhawk_sim30.png`, `/tmp/rb3-viewer-gate/ziggymullet_sim30.png`
- `/tmp/wig-gate/run7/` (34 PNGs; ziggymullet lineup), incl.
  `g7_coop_front_n01_0.png`, `g7_coop_d_n03_0.png`, `g7_coop_g_b_0.png`
- `/tmp/wig-gate/bright_g7_coop_front_n01_0.png` (brightened for judgment)
- Broken baselines: `/tmp/wig-bug/run1/r1_coop_g_b_0.png`, `r1_coop_b_cg_1.png`

## Follow-ups (from scout §4, unblocked now)

- H2: collide-hookup coverage → confirm hair doesn't pass through the skull under
  motion (collide probe).
- H4: hair color vs Dolphin GT.
