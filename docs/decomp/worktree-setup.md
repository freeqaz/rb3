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
| `build/SZBE69_B8/` | reflink copy | the build dir. `split` (`dtk dol split`) rewrites `config.json` + `obj/` into it, and every compiled `.o` lands in `src/` here. Must be private. Reflinking it also warm-starts the object cache. |

`objdiff-cli` is referenced in `build.ninja` via the relative `../objdiff/...`
(sibling repo), which won't resolve from a worktree under `.claude/worktrees/`.
The script passes `--objdiff`/`--dtk`/`--wrapper`/`--compilers` with absolute
paths to `configure.py` so the worktree's `build.ninja` bakes them in.

Why ninja accepts the warm cache: `deps="gcc"` was removed from all build rules,
so ninja reads `.d` files directly. Reflinked `.o`/`.d` files keep their mtimes,
so a no-op rebuild is ~0.15s ("ninja: no work to do") instead of a full rebuild.

## CoW savings (measured)

A fresh worktree's `build/SZBE69_B8` + `orig` are **~487M apparent** but only
**~916 KiB exclusive** (truly-new disk) — the rest shares extents with the main
repo. Verify with `btrfs filesystem du -s <dir>` (not plain `du`, which can't see
shared extents). On a non-CoW filesystem the script falls back to full copies and
warns.
