# BandPatchMesh WorkVerts trio — implementation handoff

Worktree: `~/code/milohax/rb3/.claude/worktrees/patch-trio`
Commit: `8fa16dd2` (inside the worktree; NOT landed to main).
Base: HEAD `26c5684d`. Files changed: `src/system/bandobj/BandPatchMesh.cpp`,
`src/system/bandobj/BandPatchMesh.h` (only).

## Results (objdiff-cli fuzzy_match_percent, from worktree build)

| Function | Start (HEAD) | Final | Δ | Residual class |
|---|---|---|---|---|
| AddEdge     | 76.403 | **99.758** | +23.4 | 3× `cmpw r0,r24` vs `cmpw r24,r0` operand order — permuter |
| TryAddFace  | 89.393 | **92.942** (report.json 93.105) | +3.5 | FPR scheduling + branch layout in the clamp/dot block — permuter |
| ExtendTwin  | 72.593 | **94.474** | +21.9 | pure register-number cascade, `diff_op:none`, `insert/delete:none` |

Whole-unit `report.json` diff (HEAD vs worktree): **exactly 3 functions changed,
all three the trio, all improved. Zero sibling regression.** Full `tools/ninja-locked`
green (exit 0). Header `MeshVert::faceList[1]` is layout-neutral (verified: no
sibling % moved).

## What was done

Applied the researcher's asm-derived reconstruction (`workverts-asm-derived.patch`),
which was independently re-verified here by full build + per-function objdiff. The
substance (all derived from Bank-8 target asm, semantics = the shipping binary that
renders correctly in Dolphin):

- **AddEdge** — hoist `mv1->unk28` once (callee-saved, never reloaded); drop the
  bogus `mv1 &&` null guard; replace the else-if corner chain with a fall-through
  `for (b=0;b<3;b++) if (idx==face[b] && mv1Idx==...face[(b+1)%3]...) {TryAddFace;break;}`
  loop (this is what emits the runtime `%3` magic-divide the binary has); member-array
  `mv->faceList[i]` addressing (needs the header `faceList[1]`).
- **ExtendTwin** — assign edge delta directly into persistent accumX/accumY (no
  dx/dy temps); asm-correct cross sign `accumX*(prevTwin.y-prevOther.y) -
  accumY*(prevTwin.x-prevOther.x)` (HEAD's sign was inverted vs the running game);
  memory-homed Vector2 `rowT`/`rowA` 2×2 matrix inverted IN PLACE with explicit
  `d1`/`d2` det temps and `float absDet` vs `1e-15f`; `int ok` flag; degenerate path
  returns WITHOUT writing outUv. Decl order `accumX,accumY,prevTwin,prevOther,anchor`;
  `rowA` before `rowT`; `da` before `dt`; `negAx`/`negTy` hoisted.
- **TryAddFace** — the one real HEAD behavior bug: temp built with the **two-arg copy**
  `temp.SetVert(verts[0], verts[0]->mVert)` (HEAD used the one-arg zeroing overload,
  which mis-centered AddUV's 0.25-radius monotonicity gate at the origin instead of at
  verts[0]); `int fi=face[i]` cache; `while(added--)` + `unk10.back()`; `1.0f` float
  literal; projy-before-projx.

## My own tuning attempts (beyond the researcher patch)

1. **AddEdge cmpw swap** — tried `...unk28 == mv1Idx` (loaded operand first). Rebuilt:
   still 99.758, byte-identical residual. Confirms the researcher's finding that neither
   spelling flips the 3 `cmpw` operands. Reverted to the researcher's spelling.
   → permuter-class, not LLM-reachable.

2. **TryAddFace dot-term reorder** (KEPT) — swapped the two products of
   `dot = (vp.x-projx)*(vp.x-0.5) + (vp.y-projy)*(vp.y-0.5)` to y-term first, matching
   the projy-first proj ordering. **Bit-identical** (IEEE-754 add is commutative;
   the two products are unchanged, only the final `+` operands swap → same result;
   `reject = dot<0` unchanged). Gained +0.026 (92.916→92.942). Non-negative and
   behavior-preserving, so kept.

## Refused / not pursued (would need a non-100% semantic change — declined)

- Nothing was refused on semantic grounds: every change kept is either asm-proven
  (the researcher patch) or bit-identical (the dot reorder). No change altered
  observable behavior without reaching 100%.
- TryAddFace 93 → 100 and ExtendTwin 94.5 → 100 and AddEdge 99.8 → 100 are all
  **permuter-class register/schedule cascades** (ExtendTwin: 83 FPR + 31 GPR swaps,
  zero insert/delete/diff_op; TryAddFace: clamp-branch layout + dot FPR interleave;
  AddEdge: 3 cmpw). Restructuring semantics to chase them is explicitly disallowed
  and pointless. Recommend `/permute` (source permuter) as the next lever, not LLM.

## CRITICAL for the land agent — behavior gate before merging

These changes DELIBERATELY restore semantics that differ from current HEAD's
rendering (AddEdge hoist/no-guard, ExtendTwin cross sign + solving outUv,
TryAddFace recentered monotonicity gate). The Wii binary renders correctly, so the
asm-true code is semantically safe *for Wii*. But this is SHARED code driving the
native/web port, and the wave-3 attempt (`4a49b1a4`) that raised the same fuzzy %
grossly broke the native port (spike hands / melted faces) and was reverted
(`82f390b1`).

Mandatory before landing (from research doc `re-land protocol`):
1. `scripts/native/band-closeup-capture.py` with **PINNED forced shots**
   (`{rb3_force_shot}` — the prefab rotates per launch, an A/B trap that likely
   confounded the wave-3 bisect). A/B against current HEAD on the SAME shot list;
   judge coherence (no spike/melt, no missing/extra patch faces), not lineup diffs.
2. Toggle ONE function at a time if anything fails — the three hunks are independent.
3. Watch TryAddFace specifically for missing/EXTRA patch faces (the two-arg SetVert
   moves which faces pass the monotonicity gate), not only spikes.
4. If the gate fails on the asm-proven code: split with `#ifdef HX_NATIVE` keeping
   HEAD's runtime behavior on native and landing the binary-true code for the Wii
   match build — do NOT discard the Wii gains again. File the native-amplification
   finding (near-degenerate uv triangle → `1/sqrt(0)` blow-up; add a native det-floor/
   finite guard rather than reinstating HEAD's wrong math).
5. After landing, update decomp.db (`report_result`) — these were AT_LIMIT-listed.

## Separate follow-up (not in this change)

HEAD `FindXfm` has a real OOB face read on the no-exact-hit path (fallback loop runs
0 iterations, `endFace[i]` reads 3 shorts past the face array → garbage Transform).
Wave-3 fixed it; the revert restored the bug. Tracked in `research-findxfm.md` /
`impl-findxfm.md` — out of scope for this trio.
