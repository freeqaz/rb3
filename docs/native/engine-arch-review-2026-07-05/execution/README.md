# Refactor Execution — Wave Protocol

Coordinator: Fable. Implementation: ultracode workflows, one wave at a time. Parent plan:
`../REFACTOR_PLAN.md` (binding; Phase-0-first rule applies).

## Per-item artifact protocol (resume contract)

Each work item `<KEY>` (e.g. `W0.1`) owns `execution/<KEY>/`:

- **`PLAN.md`** — written by the item's Opus planner BEFORE implementation. Subtask breakdown
  (each tagged `model: opus|sonnet`), files touched, build/verify commands, exit criteria.
  If it already exists with a `## Subtasks` section, planners return it unchanged (resume).
- **`STATUS.md`** — append-only log updated by implementers/verifiers under
  `flock /tmp/rb3-docs.lock`. One `## <subtask-id> — done|partial|blocked` section per subtask
  with commit SHAs and blockers. Re-runs read this + `git log --grep=<KEY>` and skip done work.

## Hard rules (all agents)

1. A commit either **MOVES** code (behavior-preserving) or **CHANGES** behavior — never both.
2. Commit-early, commit-often; commit message starts with `<KEY>:`. Stage only your own files;
   never `git add -A`/`-a`. Engine commits go to `../milo-native-engine` (its own repo).
3. **Never bump `MILO_ENGINE_PIN`** — the coordinator bumps it once per wave.
4. Serialize git ops per repo: `flock /tmp/milo-engine-git.lock` / `flock /tmp/rb3-git.lock`
   around add+commit.
5. Use your own CMake build dir (`native/build-agent-<KEY>`); never touch `native/build-native`
   or `native/build-web*`; never run `scripts/web/build.sh`.
6. Do not run decomp (ninja) builds; if ever needed, only via `tools/ninja-locked`.

## Waves

- **Wave 1 (2026-07-05):** W0.1 skin golden · W0.2 loud stubs · W0.3 draw-log golden ·
  W0.4 bone live-pose · W0.5 non-blind lineup gate · W0.6 flag-registry skeleton ·
  W1.1 WGSL externalization. Lane chaining: W1.1 → W0.3 (both edit `Rnd_Wgpu_RB3.cpp`).
- Wave results are appended below by the coordinator after each workflow completes.
