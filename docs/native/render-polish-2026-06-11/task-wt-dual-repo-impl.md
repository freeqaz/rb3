# task-wt-dual-repo — paired rb3 + milo-native-engine worktrees

Wave-1 implementer task `wt-dual-repo`. Polished `tools/setup-worktree.sh` so a
HIGH-RISK agent can edit **both** `rb3` and `milo-native-engine` without leaking
engine changes into every other agent's build.

## What changed

`tools/setup-worktree.sh` gained an **opt-in `--engine` flag**. With it, after
creating the rb3 worktree the script also:

1. Adds a **private** `milo-native-engine` git worktree on branch `wt-<name>`,
   based at the engine repo's current `HEAD`, at
   `…/milo-native-engine-worktrees/<name>/` (a **sibling** of the engine repo).
2. Records that path in `<wt>/.engine-path` and adds `/.engine-path` to the
   worktree's `.git/info/exclude` (keeps `git status` clean, like
   `.worktree-port`).
3. Prints the dual-repo build recipe + the landing rule + the two-repo teardown.

No-flag behavior is **byte-for-byte unchanged** (the `--engine` block is fully
gated; the only always-on delta is one extra harmless `/.engine-path` line in the
worktree's private exclude file). Idempotent: re-running `--engine` reuses the
existing engine worktree/branch.

### Why the sibling location (not inside either repo)

- **Not inside the engine repo** (`milo-native-engine/.worktrees/<name>`): a
  registered worktree there leaks as `?? .worktrees/` in the engine repo's
  `git status` (git auto-hides the registered path, not its untracked parent) —
  trips concurrent agents. Verified with a throwaway repo.
- **Not inside the rb3 worktree**: would entangle with rb3's reflink/exclude
  machinery and risk the rb3 build globbing engine `.cpp` files.

## Build seam (the existing CACHE override)

`native/CMakeLists.txt` already exposes `MILO_ENGINE_PATH` as a `CACHE PATH`
(default `${CMAKE_SOURCE_DIR}/../../milo-native-engine` → the shared symlink from
a worktree) used for `add_subdirectory` + engine include dirs, plus the soft
`MILO_ENGINE_PIN` warning. The flag relies on that — no CMake change needed:

```bash
cd <wt>
cmake -B native/build-native -S native -DMILO_ENGINE_PATH="$(cat .engine-path)"
cmake --build native/build-native --target rb3-native -j"$(nproc)"
```

`MILO_ENGINE_PATH` is a cache var → **first configure of a fresh build dir wins**,
then sticky. (If a fresh configure can't find Dawn, pass
`-DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn` —
the CMakeLists' relative `Dawn_DIR` default resolves wrong from a worktree.)

## Landing rule

Engine has a one-way dep (rb3 pins an engine SHA):

1. **Commit the engine worktree FIRST**, push/merge so the SHA is reachable.
2. **Bump `MILO_ENGINE_PIN`** in `native/CMakeLists.txt` to that SHA in the
   matching **rb3** commit.

## Verified end-to-end (then torn down)

- `tools/setup-worktree.sh wt-test-dualrepo --engine` → rb3 wt + engine wt on
  `wt-wt-test-dualrepo` at engine HEAD `8fb669d`; `.engine-path` + exclude correct;
  engine repo `git status` **clean** (no leak).
- Fresh `cmake -B … -DMILO_ENGINE_PATH=$(cat .engine-path)` → cache
  `MILO_ENGINE_PATH` = the paired engine worktree; cold `rb3-native` build green
  (46 MB binary).
- Whitespace-only edit in the **engine worktree** → incremental rebuild recompiled
  **only** `EngineVersion.cpp.o` → relinked `libmilo-engine.a` → relinked
  `rb3-native`; main repo's `EngineVersion.cpp` hash **unchanged** and the main
  repo's engine object mtime **unchanged** (other agents untouched).
- Idempotent `--engine` re-run reused the engine worktree (count stayed 1).
- Full teardown of BOTH worktrees + BOTH branches; both repos back to baseline,
  main engine file pristine, no leftover dirs. No commits in the engine repo.

## Files

- `tools/setup-worktree.sh` — `--engine` flag + paired-engine-worktree block.
- `docs/decomp/worktree-setup.md` — appended "Dual-repo worktrees (`--engine`)".
- this note.
