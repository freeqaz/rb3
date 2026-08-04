# CoW worktree setup (`tools/setup-worktree.sh`)

A plain `git worktree add` here produces an **unbuildable** tree: the big build
inputs/outputs are gitignored (`build/`, `orig/*`, `build.ninja`, `objdiff.json`),
so a fresh worktree has no target binary, no toolchain, no generated
`build.ninja`, and a cold object cache. `tools/setup-worktree.sh` makes a
worktree buildable + diffable in ~1.5s using btrfs copy-on-write reflinks.

## Usage

```bash
tools/setup-worktree.sh <name> [base-ref] [--cold-cache] [--engine]
```

- `<name>` → worktree at `.claude/worktrees/<name>`, branch `wt-<name>`
  (or pass a path with a `/` to place it elsewhere).
- `base-ref` defaults to current `HEAD`.
- `--cold-cache` skips the warm object cache (reflinks only `obj/` + `config.json`);
  use for a guaranteed-clean A/B or if a warm cache ever forces a full rebuild.
- `--engine` ALSO creates a paired, private `milo-native-engine` worktree for
  edits that span both repos (see [Dual-repo worktrees](#dual-repo-worktrees---engine--isolated-engine-edits) below).

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

## Dual-repo worktrees (`--engine`) — isolated engine edits

The shared engine symlink above is exactly that: **shared**. Every rb3 worktree
builds against the one real `milo-native-engine` tree, so any engine source edit
leaks into every concurrent agent's build. That's fine for rb3-side-only or
read-only work, but **unsafe for HIGH-RISK work that must change BOTH the engine
and rb3 at once**. The `--engine` flag fixes this by pairing the rb3 worktree
with a **private engine worktree**:

```bash
tools/setup-worktree.sh <name> --engine
```

This:

1. Creates the rb3 worktree exactly as the no-flag run does (byte-for-byte same).
2. Adds a private `milo-native-engine` worktree on branch `wt-<name>`, based at
   the engine repo's **current HEAD**, at
   `…/milo-native-engine-worktrees/<name>/` (a sibling of the engine repo).
3. Records that path in `<wt>/.engine-path` (and adds `/.engine-path` to the
   worktree's `.git/info/exclude` so it never shows in `git status`).

**Why that location?** Not inside the engine repo: a registered worktree at
`milo-native-engine/.worktrees/<name>` shows as `?? .worktrees/` in the engine
repo's `git status` (git only auto-hides the *registered* path, not its untracked
parent dir), tripping concurrent agents. Not inside the rb3 worktree either: it
would entangle with the rb3 worktree's reflink/exclude machinery and risk the rb3
build globbing engine `.cpp` files. A sibling dir keeps both repos' `git status`
clean.

### Build (the CACHE override)

`native/CMakeLists.txt` declares `MILO_ENGINE_PATH` as a `CACHE PATH` (default
`${CMAKE_SOURCE_DIR}/../../milo-native-engine`, i.e. the shared symlink from a
worktree) and uses it for `add_subdirectory` + the engine include dirs. Override
it to redirect the **entire** engine build at the private worktree:

```bash
cd <wt>
cmake -B native/build-native -S native -DMILO_ENGINE_PATH="$(cat .engine-path)"
cmake --build native/build-native --target rb3-native -j"$(nproc)"
```

Because `MILO_ENGINE_PATH` is a **cache** variable, **first configure wins** and
the value is then sticky — always pass `-DMILO_ENGINE_PATH=…` on the **first**
`cmake -B` against a **fresh** build dir (re-running `cmake` on an existing build
dir without the flag keeps the cached value; passing a *different* value on a
re-run does update it). Verified: a whitespace-only edit in the engine worktree
recompiles only that engine TU → relinks `libmilo-engine.a` → relinks the
worktree's `rb3-native`, while the main repo's engine tree and every other
agent's engine objects are left untouched.

> If a fresh configure fails with `Could not find … Dawn`, the rb3 CMakeLists'
> default `Dawn_DIR` (`${REPO_ROOT}/../dc3-decomp-deps/dawn/…`) resolves to the
> wrong relative path from a worktree. Pass it absolutely, same as the main
> build's cache has it:
> `-DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn`.

### Landing rule (commit order)

The two repos have a one-way dependency (rb3 pins an engine SHA), so land in this
order:

1. **Commit the engine worktree FIRST** (`git -C "$(cat .engine-path)" commit …`),
   then push/merge that engine commit so its SHA is reachable from engine `main`.
2. **Bump `MILO_ENGINE_PIN`** in `native/CMakeLists.txt` to that engine SHA in the
   matching **rb3** commit.

This keeps the soft pin honest: an rb3 commit never references an engine SHA that
doesn't exist yet.

### Teardown (BOTH repos)

`git worktree remove` only knows about its own repo, so remove both, and delete
both branches:

```bash
git -C <rb3-repo>    worktree remove --force <wt>
git -C <rb3-repo>    branch -D wt-<name>
git -C <engine-repo> worktree remove --force "$(cat <wt>/.engine-path)"   # read .engine-path BEFORE removing the rb3 wt
git -C <engine-repo> branch -D wt-<name>
```

The full commands (with absolute paths) are printed in the script's "Remove when
done:" footer. `--engine` is idempotent: re-running it on an existing worktree
reuses the existing engine worktree/branch instead of erroring.

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

## ⚠ Header edits in a worktree can produce a SILENT FALSE NEGATIVE

**Found 2026-08-04 by lane `x24-rotatez`, which lost a build cycle to it.**

The reflinked `.d` depfiles carry **main-repo absolute paths**. Combined with the
`deps="gcc"` removal above (ninja reads `.d` files directly), a header edit
inside the worktree can invalidate **nothing**: ninja sees every dependency
satisfied against the *main repo's* copy of that header, reports "no work to
do", and `objdiff` then returns a **stale, identical** result.

The failure mode is what makes this dangerous — it doesn't error, it reports
**no change**. An experiment that should have moved the number reads as
"tried it, made no difference," which is indistinguishable from a genuine
negative result and will be recorded as one.

```bash
# Before trusting ANY header experiment in a worktree, force the rebuild:
find build/SZBE69_B8 -name '*.d' -delete     # x24 had to delete 730 of them
tools/ninja-locked build/SZBE69_B8/report.json
```

Sanity check that costs nothing: after the first edit, confirm ninja actually
*compiled* something. If a change to a header included by hundreds of TUs
produces "no work to do", the depfiles are stale — not the change.

This only bites `.h` edits. A `.cpp` edit names the file ninja is asked to
build, so it rebuilds normally.
