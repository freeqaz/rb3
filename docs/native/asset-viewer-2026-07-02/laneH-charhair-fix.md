# Lane H — CharHair::SimulateInternal brace fix (handoff)

**Date:** 2026-07-02
**Symbol:** `SimulateInternal__8CharHairFf`
**File changed:** `src/system/char/CharHair.cpp` (indentation + one brace move only)
**Status:** FIX APPLIED + Wii objdiff-verified. NOT committed (Land stage / Lane G
commits after the native visual gate — sub-100 shared anim code rule).

## What changed

In `CharHair::SimulateInternal` (starts :511), the closing brace of
`if (thisPoint.collides.size() != 0)` was at the old :687, wrongly enclosing the
per-point bone-update tail (`Scale(m128.y, rsa, t100.m.y)` … `t100.v = thisPoint.pos;`).

Fix: closed the `if` immediately after the collision `for` loop ends (old :667),
dedented the tail by one level so it now runs **unconditionally** for every point,
and removed the old trailing `}`. Mirrors `dc3-decomp/src/system/char/CharHair.cpp`
:310-395 (DC3 closes the `if` right after the collision for). Per the scout, RB3-2010
legitimately lacks DC3's later wind block — none was added. Collision-loop contents
and tail contents were left untouched (indentation-only aside from the brace move).

## objdiff result (Wii, via mcp run_objdiff)

| | Match | Giveaway mismatch |
|---|---|---|
| **Before** | 99.6% | `[257] diff_arg: beq 0x346c vs beq 0x4ad8` (the real CFG bug — empty-collides branch skipped the tail) + `[259-262]` lfs deletes |
| **After** | 99.6% | `beq` CFG mismatch GONE; only `[259-262]` residual remains |

`run_diff_inspect mode=diagnose` after the fix:
- `diff_op: none (good!)`, **zero branch-dest diffs** — the CFG divergence is fully resolved.
- Remaining 4 non-equal instructions = **permuter-class regalloc noise**: one f0↔f1
  FPR register swap + one redundant `lfs 0x50/0x54, r26` reload (lines ~555-563,
  `sideLength`/`mMinSlack`/`mMaxSlack` region). The scout already enumerated this as
  pre-existing at [259-262]; the brace fix neither introduced nor regressed it.

**Net: match did NOT regress (still 99.6% raw; the beq mismatch was eliminated so it
is strictly better). The behavioral fix — tail runs for every strand point — is the
deliverable; the sub-1% residual is regalloc/reload noise, out of scope to chase.**

## Next (Lane G)

- Rebuild rb3-native (now contains this fix) and run the native visual gate
  (`band-closeup-capture.py`, re-roll until a crazyhawk/ziggymullet lineup rolls;
  A/B vs `/tmp/wig-bug/run1/r1_coop_g_b_0.png` broken baseline). Collide-less long
  strands should now stand up instead of draping ("white wig").
- Commit `src/system/char/CharHair.cpp` only, citing the beq CFG evidence. No Co-Authored-By.
- Follow-ups from scout §4: H2 (collide-hookup coverage → hair through skull) and
  H4 (hair color vs Dolphin GT) after the pose fix is confirmed visually.
