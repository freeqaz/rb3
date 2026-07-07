# Lane HANDS-FIX (Wave-16 Lane F) — PLAN

**Goal:** implement the adjudicated never-measured cell (HANDS-ADJUDICATION/VERDICT.md
§4–5) — for appendage meshes KEEP the authored per-mesh offset and REPOINT to the
per-member animating bone (`SetBone(b, own, false)`, no rebake) — flag-first
default-OFF `RB3_HANDS_AUTHORED_REPOINT`, rb3 `BandCharacter.cpp` ONLY. Then run the
VERDICT §5 gates (gender-split) and, if the visual misses after a correct
implementation, the pre-registered Dolphin/milo-trace fallback.

## Steps
1. [x] Add `sHandsAuthoredRepoint` flag + join `sApdAny` (opens `apdMesh` scope).
2. [x] Pass-A branch: resolve `own`, `apply[b]=1`, no rest; own==bound + clipPlaying
       handled explicitly (A4); `[HANDS_REPOINT]` provenance probe.
3. [x] Pass-B guard: repoint only, SKIP the `:1775` offset overwrite for apdMesh.
4. [x] A7: correct the inverted bound/own comment block (`:1558-1572`).
5. [x] Build in isolated worktree `.claude/worktrees/wave16-handsfix` (clang, Dawn_DIR).
6. [x] Gates: arm-C control (reproduces adjudication) + flag-ON, gender-split.
   - Tier-1 count(>5°)==0 male AND female — PASS (both 3.1°).
   - Tier-2 EXACT ≤1u — PASS.  gloves/nails measured separately — no regression.
   - drawlog-792 flag-OFF byte-identical — PASS.  A4 provenance — PASS.
   - **VISUAL E1 (ceiling-hand/spike-web GONE) — FAIL** (matched-frame zoom decisive).
7. [x] A6: extend `offset_basis_derivation.py` with the female axis (offline + runtime).
8. [x] Fallback assessed: `../milo-trace` is a stub (not executable) AND mis-targeted
       for the multi-bone tear — documented, do NOT redesign blind.
9. [x] STATUS.md, checkpoint `/tmp/wave16-checkpoints/F.json`, classjson (REFUTED),
       commit under flock (rb3 only, my files only).

## Verdict: REFUTED by the decisive VISUAL gate. See STATUS.md.
