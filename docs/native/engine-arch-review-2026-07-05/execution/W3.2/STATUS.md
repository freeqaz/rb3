# W3.2 — STATUS

## B.S1 (BoxMapLighting PLAN, Opus) — 2026-07-06 — done

**Deliverable:** `PLAN.md` (this dir) — prototype-only lane per WAVE6_REVIEW A6.

**What landed:**
- File:line inventory of the SYS-4 approx/box-ambient approximations + the real/approx routing
  inversion (`Rnd_Wgpu_RB3.cpp:1312-1434` scalar-ambient/approx-as-Lambert; `Env.cpp:172`
  faithful split; `Mesh.cpp:1463-1480` per-object granularity; `BoxMap.cpp` ground-truth algorithm).
- Faithful-replacement design grounded in the Wii box-ambient source (`BoxMapLighting::ApplyLight`
  cube accumulation `BoxMap.cpp:122-199`; per-vertex sample form flagged as the top correctness risk
  R1, to be Dolphin-validated in S2).
- Prototype build strategy **validated**: engine worktree `/tmp/wave6-boxmap-wt`
  (branch `wave6-boxmap-proto`) created, **HEAD `8e7eddd` == pin** (confirmed);
  scratch build via `-DMILO_ENGINE_PATH=/tmp/wave6-boxmap-wt` into `/tmp/wave6-boxmap-build`
  (pin check is `message(WARNING)` only, non-fatal — `native/CMakeLists.txt:76-86`);
  `BoxMapLighting` already links into `rb3-native` (`BoxMap.cpp.o` present) so no new CMake wiring.
- Visual gate design: G-A Dolphin matched-frame venue A/B (correctness, blocking), G-B songMs-pinned
  before/after venue captures (both luma tails + pink-hue), G-C lineup + drawlog no-regression
  (flag-OFF byte-identical), G-D fail-red anti-blindness.

**Constraints honored:** no engine mainline edits; no flag flips; no pin bump; worktree add does not
touch the engine mainline working tree.

**Handoff to S2 (Wave 7):** implement `RB3_BOX_AMBIENT` in the worktree, run G-A..G-D, rebase onto
post-Lane-A HEAD before any land. Key open risks: R1 (per-vertex cube weighting), R2 (656→752 struct
growth crosses DC3 contract), R3 (per-environ vs per-object granularity), R5 (fakespot double-count).

**Artifacts:**
- `docs/native/engine-arch-review-2026-07-05/execution/W3.2/PLAN.md`
- engine worktree `/tmp/wave6-boxmap-wt` (branch `wave6-boxmap-proto`, HEAD 8e7eddd)
- checkpoint `/tmp/wave6-checkpoints/B-S1.json`
