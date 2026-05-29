# Native worktrees — isolated, buildable rb3-native (CMake) worktrees via CoW

`scripts/setup_worktree_native.sh` spins up an isolated git worktree for the
**rb3-native** port (the CMake/Clang LP64 `native/build-native` build), so
multiple concurrent implementation agents can build + edit without stepping on
each other or on the main tree's build dir. It uses btrfs/xfs copy-on-write
reflinks so the setup is ~3 seconds, not a multi-minute cold build.

> **NOT the decomp worktree script.** `scripts/setup_worktree.sh` (no `_native`)
> targets the *asm-match decomp* build (ninja + dtk + wibo + `build/SZBE69_B8/`).
> They are different build systems — keep both. Use `setup_worktree_native.sh`
> for native (`rb3-native` / `milo-native-engine`) work.

## Usage

```bash
scripts/setup_worktree_native.sh [path] [branch] [base-ref] [--private-engine]
```

| Arg                | Default                                  |
|--------------------|------------------------------------------|
| `path`             | `.claude/worktrees/nwt-<timestamp>`      |
| `branch`           | `nwt-<basename of path>`                  |
| `base-ref`         | current `HEAD`                            |
| `--private-engine` | off (worktree uses the SHARED engine)    |

Idempotent: re-running on an existing worktree path reconfigures in place.

### Coordinator: spin up a worktree for an implementation agent

```bash
# Shared engine (default) — for tasks that DON'T edit the engine:
scripts/setup_worktree_native.sh .claude/worktrees/nwt-<task> nwt-<task> HEAD

# Private engine — for tasks that EDIT milo-native-engine (e.g. RTT-outfit):
scripts/setup_worktree_native.sh .claude/worktrees/nwt-<task> nwt-<task> HEAD --private-engine
```

Then hand the agent the printed build + run + removal commands.

## What is SHARED vs PRIVATE

| Thing                              | Mode                  | Why |
|------------------------------------|-----------------------|-----|
| `native/build-native/`             | **PRIVATE** (reflink) | The build WRITES here. Reflinked into the worktree as a real dir (asserted **never** a symlink) so a worktree build can't corrupt the shared main build dir. Reflink also warm-starts the object cache. |
| `milo-native-engine/` (default)    | **SHARED** (read-only)| `MILO_ENGINE_PATH` resolves to the absolute `/home/free/code/milohax/milo-native-engine`. Worktree compiles against it but must not edit it. |
| `milo-native-engine/` (`--private-engine`) | **PRIVATE** (reflink) | Reflink-copied to `<worktree>/.private-engine`; `MILO_ENGINE_PATH` repointed there so engine edits stay local. |
| Game data (`orig-assets/extracted`)| **NOT COPIED**        | Large + gitignored. The build doesn't need it; runs pass an absolute `RB3_DATA=` (see below). |

## How the engine resolves

The main `native/build-native/CMakeCache.txt` bakes
`MILO_ENGINE_PATH` as an absolute path (`…/rb3/native/../../milo-native-engine`
→ `/home/free/code/milohax/milo-native-engine`), so a reflinked build dir
already points at the shared engine regardless of worktree location.

- **Default (shared):** the script reconfigures the worktree's build dir,
  leaving `MILO_ENGINE_PATH` at the shared engine. `build.ninja` references
  `/home/free/code/milohax/milo-native-engine/src` (verified: 0 private refs).
- **`--private-engine`:** the script reflink-copies the engine to
  `<worktree>/.private-engine` and reconfigures with
  `-DMILO_ENGINE_PATH=<worktree>/.private-engine`. `build.ninja` then references
  the private copy only (verified: 997 private refs, 0 shared refs), so editing
  the worktree's engine recompiles only that worktree and never touches the
  shared engine tree.

### Re-homing the reflinked build dir (implementation note)

CMake bakes `CMAKE_HOME_DIRECTORY` / `CMAKE_CACHEFILE_DIR` (and the engine's
imgui FetchContent leaves a nested `_deps/imgui-subbuild` cache) at
configure-time, so a freshly-reflinked build dir would be rejected on
reconfigure ("CMakeCache.txt directory is different"). The script rewrites every
literal occurrence of the MAIN build-dir path → the worktree's build-dir path
across all reflinked CMake/ninja metadata, then runs `cmake -S <wt>/native -B
<wt>/native/build-native` to rewrite `build.ninja` to the worktree's own
sources while reusing the warm `.o` cache + `_deps`.

## Build + run

```bash
# Build (warm reflinked cache; first build recompiles the fork TUs because
# `git worktree add` checks them out with fresh mtimes — engine .a + imgui +
# link are reused):
cmake --build <worktree>/native/build-native -j

# Headless run — absolute RB3_DATA (data is never copied into the worktree):
RB3_GAME=1 MILO_HEADLESS=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=120 \
  <worktree>/native/build-native/rb3-native
```

For a full menu/song-load/gameplay run, add `MILO_AUDIO=1`, a larger
`MILO_MAX_FRAMES`, and an `RB3_GAME_INPUT=` script (see `SALVAGE_V33.md` /
`SONG_LOAD_ACHIEVED.md` for canonical input strings).

## Remove a worktree

```bash
git -C /home/free/code/milohax/rb3 worktree remove --force <worktree>
```

(`--private-engine` worktrees carry a `.private-engine/` reflink dir; `worktree
remove --force` deletes the whole tree including it.)

## Caveats

- **Reflink-capable fs required for speed.** The filesystem under
  `/home/free/code/milohax` is btrfs (reflinks confirmed). On a non-CoW dest the
  script *warns* and falls back to full copies (slow, space-hungry) but still
  works.
- **`MILO_ENGINE_PIN` mismatch.** The script drops the sticky pin cache entry
  (`-UMILO_ENGINE_PIN`) so CMake re-reads the current `CMakeLists.txt` default.
  If the shared engine HEAD differs from the pin, CMake prints a (non-fatal)
  `milo-native-engine HEAD is … but rb3-native pins …` warning; the script
  surfaces it. Build proceeds regardless.
- **First worktree build is not instant.** Fork-source TUs recompile (fresh
  checkout mtimes); only the engine static lib, `_deps`/imgui, and the final
  link are reused from the warm cache. Subsequent incremental builds in the
  worktree are fast.
