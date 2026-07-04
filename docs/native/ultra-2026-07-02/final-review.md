# FINAL REVIEW — ultra-2026-07-02 run (Fable, independent)

Reviewer: Fable 5, adversarial final pass. Fresh builds + fresh captures taken;
implementers' evidence NOT relied on for the visual verdicts.

Commits under review:
- rb3 `30c51bad` — BandPatchMesh WorkVerts trio + FindXfm (decomp land)
- rb3 `fadd179a` + engine `04c8e1c` — C8 glowing-eyes dest-multiply composite fix
- rb3 `7f603e17` + engine `5587ce0` — C8 char-environ real-light face shading

## OVERALL VERDICT: APPROVE_WITH_FOLLOWUPS

No reverts required. All four items pass faithfulness, the Wii gate, and a fresh
native visual gate. Follow-ups are listed at the bottom; none are blocking.

---

## 1. Faithfulness audit — PASS (all commits)

**30c51bad (BandPatchMesh.cpp/.h):** Read the full diff. Portable C++ only; no
ASM_BLOCK / inline asm anywhere in the file (grep clean). The wave-3-shaped red
flags are present *by design* and are asm-backed, not speculative:
- ExtendTwin cross-product sign flip vs HEAD: the residual after the change is
  pure register/schedule (objdiff `diff_op:none`, zero insert/delete at 94.47%),
  which is only possible if the operand order now matches the target — the
  target binary itself proves HEAD's sign was inverted.
- ExtendTwin degenerate path no longer zeroes `outUv`: verified the ONLY caller
  (`SetVertsAndFaces`, BandPatchMesh.cpp:719-721) zero-initializes both out
  params (`Vector2 v40(0,0); Vector2 v48(0,0)`) — the no-write path is
  behavior-identical to HEAD's explicit zeroing, no native UB.
- FindXfm `0.5f * Length` vs HEAD's `0.5f / Length`, un-negated unk4 /
  negated-unk10 row, vector-first Multiply writing `xfm.v`/`xfm.m.z`: HEAD left
  `xfm.v`/`xfm.m.z` *uninitialized* (genuine UB) and OOB-read `Faces().end()[0..2]`
  on the not-found path, so "preserve HEAD behavior" was ill-defined; the landed
  body is strictly more defined and asm-derived (79.1% with `diff_op:none`).
- Header `MeshVert::faceList[1]`: sizeof unchanged (0x30 + u16 @0x30 pads to
  0x34 before; 0x32+2 = 0x34 after) — layout-neutral, confirmed empirically by
  zero sibling % movement.
- The `#ifdef HX_NATIVE` OOB guard in FindXfm is defensive-only and excluded
  from the Wii build. Correct use of the guard.

**fadd179a / 04c8e1c (eyes):** rb3 side is 100% inside `#ifdef HX_NATIVE`
(RAII ComposeScope covers all return paths); Wii match build untouched. Engine
change is in the RB3-only TU `Rnd_Wgpu_RB3.cpp` (DC3 byte-identical), scoped by
three conditions (compose-scope flag AND RT-active AND colorMod 2/3), default-ON
with `RB3_COMPOSE_MULT_OFF=1` opt-out. Pin bump in the same commit as the flag
consumer — no cross-repo skew.

**7f603e17 / 5587ce0 (shading):** engine change again RB3-only TU, world.cam
venue path only, char-env scoped (`strstr(envNm,"char")` + usable real key),
legacy behavior preserved for all venue-geometry envs and empty-real-list char
envs (converge backdrop tuning protected by construction). Default-ON, opt-out
`RB3_CHAR_REAL_LIGHT_OFF=1` + two tuning knobs. No Wii `src/` change.
**Flag:** the rb3 commit contains an extra, undocumented hunk adding
`--profiling-funcs` to the web release link options — unrelated to the pin bump
and unmentioned in the commit message. Benign (wasm name section only, no
codegen), but it looks like another agent's uncommitted edit to the same file
was swept into this commit. Process follow-up, not a revert.

`FxSendNative.cpp` (the other agent's engine edit) is untouched/unstaged in both
engine commits — confirmed via `git show --stat` and current `git status`.

## 2. Wii gate — PASS (independently re-verified)

Fresh `batch_objdiff` (ninja-locked build) + landed report.json:

| Function | Claimed | Verified |
|---|---|---|
| AddEdge | 99.758 | **99.76** (batch) / 99.758 (report) |
| ExtendTwin | 94.474 | **94.474** (report; MCP symbol had a `PC` mangling typo in my batch call, report is authoritative) |
| TryAddFace | 92.942/93.105 | **92.94** (batch) / 93.105 (report) |
| FindXfm | 79.079/79.15 | **79.08** (batch) / 79.15 (report) |

Siblings at documented baselines: ProjectPatches 90.76, PreRender 91.99 —
unchanged. Build green (batch_objdiff would have failed otherwise). decomp.db
already reflects FindXfm at 79.15 / AT_LIMIT with attempt history.

## 3. Fresh native visual review — PASS

Built `rb3-native` at HEAD (`7f603e17`) + engine `5587ce0` (up-to-date, "no work
to do"). Ran my own captures (NOT the implementers'):

- `/tmp/finalreview/drums` — PASS, pinned 6/6, 0 drops, ratio 0.00
- `/tmp/finalreview/vocals` — PASS, pinned 6/6, 0 drops, ratio 0.00
- `/tmp/finalreview/guitar` — PASS, pinned 10/10, 0 drops, ratio 0.00

Frames personally inspected (+ zoom crops in /tmp/finalreview/):
- `drums/cap_coop_d_n03_0.png` + face zoom (`crop_drummer_face.png`): coherent
  female face; **eyes dark with visible iris/pupil structure — NOT glowing white
  dots, NOT blacked out**. Eyebrows/nose/lips intact. (b-check + c-check PASS)
- `guitar/cap_coop_g_b_0.png`: guitarist face under warm key — subtle natural
  eyes with a small specular glint, dim skin with a clear shadow side. Matches
  the GT direction (`face_guitarist_ambient.png` = dim ambient figure;
  `face_singer_rimlit.png` = near-silhouette). Native remains a bit brighter
  than GT's near-silhouette but is directional and restrained, nothing like the
  flat flood the OFF path shows in the impl A/B. (c-check PASS)
- `vocals/cap_coop_front_n00_0.png` wide shot: three members coherent. A
  transient tan spiky shape near the mid-air kicking character (zoomed:
  `crop_spike.png` vs frame-1 `crop_spike_f1.png`) **moves/disperses between
  frames** and matches a kicked bar-stool prop in the intro animation — not a
  static mesh explosion; patch-mesh surfaces (faces, close-up hands/arms in
  `guitar/cap_coop_g_cg*`) are all coherent. Consistent with the known
  "intro-cinematic props ≠ broken gameplay" triage gotcha. No spike-hands, no
  melted faces, no shard tangles on characters. (a-check PASS)

## 4. Per-item verdicts

| Item | Verdict |
|---|---|
| Decomp land `30c51bad` (trio + FindXfm) | **APPROVE** |
| Trio impl (worktree `8fa16dd2`, as landed) | **APPROVE** |
| FindXfm impl (worktree `d0ea0b6a`, as landed) | **APPROVE** (visual gate it couldn't run is now done — clean) |
| C8 eyes `fadd179a`/`04c8e1c` | **APPROVE_WITH_FOLLOWUPS** |
| C8 shading `7f603e17`/`5587ce0` | **APPROVE_WITH_FOLLOWUPS** |

## Follow-ups (none blocking)

1. **Bright-venue skin-tone A/B** (eyes fix): dest-multiply now applies to ALL
   two-color composites (skin/hair/clothing/instruments); only dark-venue frames
   were judged. Spot-check lit skin tone in a bright venue vs Dolphin GT.
2. **Other-venue char shading spot-check** (shading fix): verified in one venue
   (subway/street `main.lit` key). Check festival/arena char envs; the
   empty-real-list guard makes regressions per-env and opt-out revertable.
3. **`--profiling-funcs` hunk in `7f603e17`**: confirm ownership with the
   sharpen-fetch agent; it landed undocumented under a pin-bump commit message.
   Keep (benign) but attribute it in that lane's docs.
4. **Permuter pass** on the four residuals (AddEdge 3×cmpw, ExtendTwin pure
   regalloc, TryAddFace FPR schedule, FindXfm cr1/callee-save/frame) — all
   permuter-class; do NOT spend more LLM passes.
5. **Magenta full-frame wash** (pre-existing, intermittent, reproduced in the
   implementers' baseline runs): unrelated to this run; deserves its own
   investigation lane.
6. **Worktree cleanup**: `.claude/worktrees/patch-trio` + `patch-findxfm` were
   left for audit; remove after this review is accepted.
7. **Exact TEV colorMod fidelity** (eyes): dest-multiply is an approximation;
   if tint fidelity is ever questioned, validate against dumped Xbox
   eyes_* textures or replace the 4-pass emulation with one shader pass.

Capture paths: /tmp/finalreview/{drums,vocals,guitar}/ + zoom crops
/tmp/finalreview/crop_{drummer_face,center,spike,spike_f1}.png.
