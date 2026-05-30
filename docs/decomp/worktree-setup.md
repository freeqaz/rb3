# CoW worktree setup (`tools/setup-worktree.sh`)

A plain `git worktree add` here produces an **unbuildable** tree: the big build
inputs/outputs are gitignored (`build/`, `orig/*`, `build.ninja`, `objdiff.json`),
so a fresh worktree has no target binary, no toolchain, no generated
`build.ninja`, and a cold object cache. `tools/setup-worktree.sh` makes a
worktree buildable + diffable in ~1.5s using btrfs copy-on-write reflinks.

## Usage

```bash
tools/setup-worktree.sh <name> [base-ref] [--cold-cache]
```

- `<name>` → worktree at `.claude/worktrees/<name>`, branch `wt-<name>`
  (or pass a path with a `/` to place it elsewhere).
- `base-ref` defaults to current `HEAD`.
- `--cold-cache` skips the warm object cache (reflinks only `obj/` + `config.json`);
  use for a guaranteed-clean A/B or if a warm cache ever forces a full rebuild.

Then:

```bash
cd .claude/worktrees/<name>
tools/ninja-locked build/SZBE69_B8/src/<File>.o            # warm cache => fast
build/tools/objdiff-cli diff -u <unit> <symbol> --format json-pretty -o /dev/stdout
```

Remove when done:

```bash
git worktree remove --force .claude/worktrees/<name>
```

## What's shared, symlink vs reflink — and why

The rule: **anything the build WRITES must be a real (reflinked) copy**, never a
symlink into the main tree — a symlink would let the worktree corrupt the shared
main `build/` (catastrophic with the permuter fleet running). **Read-only** dirs
can be symlinks (cheapest).

| Path | Mode | Why |
|---|---|---|
| `orig/` | reflink copy | target binaries; read-only but reflink is free on CoW |
| `build/compilers/` | symlink | read-only toolchain (mwcceppc); 121M |
| `build/tools/` | symlink | read-only toolchain (dtk, wibo, objdiff-cli); 11M |
| `bin/` | symlink | host-tool wrappers (`bin/objdiff-cli` etc.) — see "bin/ overlay" below |
| `build/SZBE69_B8/` | reflink copy | the build dir. `split` (`dtk dol split`) rewrites `config.json` + `obj/` into it, and every compiled `.o` lands in `src/` here. Must be private. Reflinking it also warm-starts the object cache. |
| `.claude/worktrees/milo-native-engine` | symlink | ONE symlink shared across all worktrees → sibling `milo-native-engine` repo. See "engine path" below. |
| `scripts/web/node_modules/`, others | symlink | shared npm install — avoids slow per-agent reinstall. Created only if main tree has it. |
| `orig-assets/` | symlink | reference screenshots + extracted song assets (`orig-assets/extracted`, `orig-assets/native-refs`). Build never writes here; symlink is safe. Web capture/test agents use it without needing `--assets-dir` absolute paths. Created only if main tree has it. |

`objdiff-cli` is referenced in `build.ninja` via the relative `../objdiff/...`
(sibling repo), which won't resolve from a worktree under `.claude/worktrees/`.
The script passes `--objdiff`/`--dtk`/`--wrapper`/`--compilers` with absolute
paths to `configure.py` so the worktree's `build.ninja` bakes them in.

## Engine path (`milo-native-engine`)

`native/CMakeLists.txt` resolves the engine via `${CMAKE_SOURCE_DIR}/../../milo-native-engine`.
From the main repo that goes `rb3/native/../../` → `/home/free/code/milohax/`,
correct. From a worktree at `.claude/worktrees/<name>/native/` it goes
`<wt>/native/../../` → `.claude/worktrees/`, wrong. The script writes ONE symlink
at `.claude/worktrees/milo-native-engine` → the real sibling repo. Every worktree's
relative path then resolves correctly, with no per-script defense needed.

The symlink persists across `git worktree remove` (it's shared infrastructure,
not per-worktree state). Safe to leave; re-created idempotently if deleted.

## bin/ overlay (`git status` cleanliness)

The `bin/` symlink shadows the tracked `bin/*` files in the worktree, so naively
every script would appear as "deleted" in `git status`. The setup script marks
each indexed `bin/*` entry `--assume-unchanged` in the worktree's index — this is
per-worktree (doesn't touch the main repo) and silences the noise. Reset with
`git update-index --no-assume-unchanged bin/*` if you ever need to track a real
change to the bin scripts from a worktree (you almost certainly don't — bin/ is
host-tool wrappers, not decomp output).

The only `bin/`-related noise that remains in `git status` is the untracked
symlink itself (`?? bin`), shown once.

## Dev-server port

Each worktree gets a deterministic dev port in `[8500, 8999]` (hash of the
worktree's basename via `cksum`). Persisted to `<worktree>/.worktree-port` so
scripts can read it without re-hashing. The "Next:" output prints the port +
the `python3 native/web/server.py --port $(cat .worktree-port)` recipe. Two
agents picking the same worktree name would collide; otherwise each web-port
agent gets a private port automatically.

Why ninja accepts the warm cache: `deps="gcc"` was removed from all build rules,
so ninja reads `.d` files directly. Reflinked `.o`/`.d` files keep their mtimes,
so a no-op rebuild is ~0.15s ("ninja: no work to do") instead of a full rebuild.

## CoW savings (measured)

A fresh worktree's `build/SZBE69_B8` + `orig` are **~487M apparent** but only
**~916 KiB exclusive** (truly-new disk) — the rest shares extents with the main
repo. Verify with `btrfs filesystem du -s <dir>` (not plain `du`, which can't see
shared extents). On a non-CoW filesystem the script falls back to full copies and
warns.
