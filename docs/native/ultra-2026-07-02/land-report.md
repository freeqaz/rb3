# LAND report — BandPatchMesh WorkVerts trio + FindXfm

**Commit:** `30c51bad` (main, `src/system/bandobj/BandPatchMesh.cpp` + `.h` only)
**Base:** both worktrees branched from `26c5684d`, which is an ancestor of the
main HEAD they landed onto (`3ef8af2e`); BandPatchMesh files were byte-identical
in main since that base, so both patches applied cleanly with no rebase.

## Merge

The two lanes edit **non-overlapping** function bodies of the same file:
- `patch-trio` (`8fa16dd2`): AddEdge, TryAddFace, ExtendTwin + header
  `MeshVert::faceList[1]`.
- `patch-findxfm` (`d0ea0b6a`): FindXfm.

Applied as `git apply` of both worktree diffs (`git apply --check` passed
combined). Worktrees left in place for audit (not removed).

## Wii gate — PASS

`tools/ninja-locked` green (exit 0). objdiff-cli fuzzy % on the four targets
(matches or exceeds every impl-doc claim):

| Function | HEAD | Landed | impl-doc target |
|---|---|---|---|
| AddEdge     | 76.40 | **99.758** | 99.758 |
| ExtendTwin  | 72.59 | **94.474** | 94.474 |
| TryAddFace  | 89.39 | **92.942** (report.json 93.105) | 92.942 / 93.105 |
| FindXfm     | 58.70 | **79.079** (report.json 79.15) | 79.1 |

**Collateral: none.** Cross-checked the landed report.json against the
patch-findxfm worktree report (= base + FindXfm only): the *only* functions that
differ are exactly the 3 trio functions (AddEdge/ExtendTwin/TryAddFace). The
header `faceList[1]` addition is layout-neutral — no sibling % moved. FindXfm is
identical (79.15) in both. Spot-checked siblings unchanged at documented
baselines: ProjectPatches 90.76, PreRender 91.99, plus all 100% siblings intact.

## Native visual gate — PASS (the mandatory gate for this file)

`cmake --build native/build-native --target rb3-native` green — confirms the
FindXfm native-safe iterator casts (`&begin()[0]`) and the header change compile
on libstdc++/HX_NATIVE.

`scripts/native/band-closeup-capture.py`, pinned forced shots (determinism gate
satisfied — cur_shot == forced on every frame):

- `--member drums`: verdict **PASS**, pinned 6/6, drops_total=0, drops_band=0,
  max_band_ratio=0.00. Captures in `/tmp/patchgate/drums/`.
- `--member guitar`: verdict **PASS**, pinned 10/10, drops_total=0, drops_band=0,
  max_band_ratio=0.00. Captures in `/tmp/patchgate/guitar/`.

**Direct frame inspection (judged coherence, not lineup/hairstyle):**
- `drums/patchgate-drums_coop_d_n01_0.png` — female drummer, arm reaching to kit;
  coherent limb/torso/face.
- `drums/patchgate-drums_coop_d_n03_0.png` — drummer face clearly visible;
  coherent human face (glowing eyes = normal eye specular highlight), intact
  shoulders/arms.
- `guitar/patchgate-guitar_coop_g_cg_0.png` — guitarist arms/torso; coherent
  (dappled polka-dot pattern is stage-lighting gobo, not deformation).
- `guitar/patchgate-guitar_coop_g_cg01_0.png` — extreme close-up of the
  guitarist's arm crossing frame; coherent limb, no shards.

No spike-exploded hands, no melted faces, no shard tangles — the exact wave-3
(`4a49b1a4`) regression mode that broke the port yesterday is **absent**. These
are patch-mesh characters (fingernails/makeup via BandPatchMesh) and they render
as coherent humans.

## Notes

- Every landed change is asm-proven against the Bank-8 target or bit-identical
  (IEEE-add reorder). No ASM_BLOCK / instruction transcription. Residuals are
  permuter-class register/schedule cascades (recommend `/permute` next, not LLM).
- These bodies restore the shipping-binary semantics that differ from the prior
  HEAD native rendering (FindXfm's tail in particular fixes HEAD's uninitialized
  `xfm.v`/`xfm.m.z` UB and an OOB face read). The native gate confirms the change
  is safe on the port — no HX_NATIVE fallback was needed.
- Out-of-scope follow-up noted by the trio agent: HEAD had a real FindXfm OOB
  face read; the landed FindXfm fixes it (real found==end() + nearest-edge scan).
