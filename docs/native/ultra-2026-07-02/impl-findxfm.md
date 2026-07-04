# impl — BandPatchMesh::FindXfm

Symbol: `FindXfm__13BandPatchMeshFP7RndMeshRC7Vector2R9Transform`
Unit: `main/system/bandobj/BandPatchMesh` (`src/system/bandobj/BandPatchMesh.cpp:1088`)
Worktree: `~/code/milohax/rb3/.claude/worktrees/patch-findxfm` (branch `wt-patch-findxfm`, commit `d0ea0b6a`)

## Result

| | Wii match % |
|---|---|
| HEAD (start) | 58.7% |
| Final | **79.1%** |

+20.4pp. Structure is fully correct: objdiff `diff_op: none` (no wrong operations). Full `tools/ninja-locked` build is green. Native (clang LP64, HX_NATIVE) build **compiles clean** (syntax-checked via the native compile_commands.json).

## What changed

The researcher pre-applied the research-doc sketch to the worktree (58.7 → 79.1). I verified it line-by-line against the Bank8 target disasm (extracted from `build/SZBE69_B8/asm/system/bandobj/BandPatchMesh.s`), fixed a **native-build-breaking bug** the sketch introduced, and added the task-required native OOB guard. The asm-proven semantics (all confirmed against target):

- No cached `Faces()`/`Verts()` base+end — every accessor is re-evaluated inline (13 `operator->` assert sites, all line `0xAB`; HEAD's explicit `0x6C5` assert is not in the binary).
- `found == Faces().end()` real compare + full nearest-edge fallback scan. HEAD's old body OOB-read `Faces().end()[0..2]` on the genuinely-not-found path.
- Writes `xfm.v` (`psq_st f4,0x24(r26)`) and `xfm.m.z` (`psq_st f8,0x18(r26)`) via the **vector-first** `Multiply(const Vector3&, const Hmx::Matrix3&, Vector3&)` overload (Mtx.h:646) + inline `Normalize`. HEAD left both **uninitialized**.
- `centerMV.unk4 = posOut.x` (un-negated); `Scale(posOut.y, -1.0f, centerMV.unk10)` — negation on the posOut.y row (`@F_000080bf` = −1.0f), not on unk4.
- `scale = 0.5f * Length(...)` (target has exactly one `fdivs` total, the fallback `t` divide) — HEAD used `0.5f / Length`.

Tail verified against target `8052D648`–`8052D8B0`: p@0x80, centerVert@0x98 (pos stays 0), centerMV@0xd0 (unk4@0xd4, unk10@0xe0), posOut@0x198, normOut@0x174.

### Native-build fix (important — the sketch was broken on the port)

The sketch cast `(unsigned short*)mesh->Faces().begin()/end()`. On Wii (stlport) the vector iterator **is** a raw `Face*`, so the cast compiles. On native (libstdc++) `begin()/end()` return a class `__normal_iterator`, so `(unsigned short*)iterator` is an **invalid cast → 8 compile errors** (would have broken every native/web build). Rewrote all 8 sites to `(unsigned short*)&mesh->Faces().begin()[0]` / `...end()[0]` — the `&…[0]` folds back to the raw pointer on stlport (Wii codegen byte-identical, still 79.1%) and is a valid `Face*` address on libstdc++.

### Native OOB guard (task-required exception)

Added a minimal `#ifdef HX_NATIVE` block before the triangle gather that resets `found` to `Faces().begin()` if it still equals `Faces().end()`. By construction the nearest-edge fallback always assigns a valid face (first candidate sets `better=1`), so this is defensive-only; the Wii-match build omits it, so match is unaffected.

## Faithfulness / native-behavior note (READ before landing)

FindXfm's only caller is `ProjectPatches` (via `PreRender`) on the patch-projection path (fingernails, **face makeup/tattoos** when `patch.mTexture != -1`). The faithful body **changes native runtime behavior vs HEAD**: it initializes `xfm.v`/`xfm.m.z` (HEAD left them UB → garbage ProjectPatches collide segment) and flips the negation row + scale formula. This is asm-proven against the Bank8 binary, which renders correctly on Dolphin (the correctness oracle) — HEAD's native path was genuinely undefined behavior, so "match HEAD" is ill-defined here.

Per the house rule for sub-100 shared-geom changes (the wave-3 lesson), this **requires the native visual gate before merge**: `scripts/native/band-closeup-capture.py` on a face-paint character, judged for coherence across prefab rotations. I could not run it here (worktree has no `native/build-native`; a cold engine+native build + headless face-paint capture is out of a decomp subagent's budget and is subjective).

- If the gate is clean → land as-is (native gets the fix, which is the task's stated intent — "native behavior changes for the better").
- If the gate can't be run / regresses → wrap the faithful tail under `#ifndef HX_NATIVE` and restore HEAD's exact tail under `#ifdef HX_NATIVE`. HEAD's body is the FindXfm at `git show 82f390b1:src/system/bandobj/BandPatchMesh.cpp`. The loop half is strictly safer than HEAD (no OOB) and picks the same triangle in the common containing-triangle case, so only the tail math would need guarding.

## Why not higher than 79.1% (refused / exhausted ideas)

Residual is permuter-class, exactly as the research predicted:
- **Register allocation**: target uses `cr1` for all 13 accessor-assert compares (we get `cr6`) and saves one more callee GPR (`_savegpr_17` vs `_18`). Root cause: the target hoists the assert format-strings (`kAssertStr`, the `@STRING@__rf__…` operands, `TheDebug`) and the `iter` pointer into callee-saved r17–r22 across the whole function, shifting the CR-field and GPR allocation. Not reproducible by source edits.
- **Frame size**: TGT 0xa60 vs 0xa50 (Δ0x10) + many SWAPPED tail slots — target puts posOut@0x198 / normOut@0x174 (high) while ours land low; MWCC assigns these from lifetime/pressure, not decl order.
- **FPR scheduling** inside the two inlined `Multiply(Matrix3,Matrix3,Matrix3)` and the p-multiplies (f1↔f2 / f2↔f3 families, `ps_madds0`↔`ps_muls1` reorderings).

Tried: swapping `posOut`/`normOut` declaration order → **no change** (identical instruction counts) — MWCC ignores the reorder here. Simple decl reordering does not move the stack assignment or CR allocation. Reaching 100% would need the source permuter (`/permute` / `decomp_synth`) to search FPR/stack permutations, or is genuinely at-limit for hand-decomp. No fake ASM_BLOCK matches were attempted.

## Collateral (no regressions)

Only FindXfm's body changed (single hunk, `BandPatchMesh.cpp:1088`). Other symbols in the unit unchanged at their baselines: ExtendTwin 72.5%, TryAddFace 89.2%, AddEdge (baseline), ProjectPatches 90.6%, PreRender 91.9%.
