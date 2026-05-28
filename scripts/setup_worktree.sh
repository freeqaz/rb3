#!/bin/bash
#
# Sets up a git worktree with a fully working build system.
#
# Usage:
#   scripts/setup_worktree.sh [path] [branch-name] [base-ref]
#
# Arguments:
#   path       - Where to create the worktree (default: /tmp/claude/worktree-<timestamp>)
#   branch     - Branch name for the worktree (default: wt-<dirname>)
#   base-ref   - Git ref to branch from (default: current HEAD)
#
# Examples:
#   scripts/setup_worktree.sh /tmp/claude/my-feature my-feature
#   scripts/setup_worktree.sh /tmp/claude/test test-branch master
#   scripts/setup_worktree.sh                         # auto-generates path & branch from HEAD
#
# What this does:
#   1. Creates a git worktree from the current HEAD (or specified ref)
#   2. Symlinks clangd config, orig/, and bin/objdiff-cli
#   3. Re-runs configure.py with absolute tool paths so build.ninja works
#   4. Symlinks shared build artifacts (compilers, downloaded tools, target objects)
#
# IMPORTANT: The worktree branches from the currently checked-out commit by
# default, NOT from 'master'. This ensures worktrees have the latest code
# from whatever branch you're working on.
#
# After setup, you can build normally from the worktree:
#   cd /tmp/claude/my-feature && ninja build/SZBE69_B8/src/system/obj/Object.obj
#
# The MCP orchestrator tools also work:
#   run_objdiff(symbol, project_dir="/tmp/claude/my-feature")
#
# Prerequisite: the main repo must have been built once (so build/tools/dtk
# and build/compilers/ are populated by the configure.py download step).

set -euo pipefail

MAIN_REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORKTREE_PATH="${1:-/tmp/claude/worktree-$(date +%s)}"
BRANCH="${2:-wt-$(basename "$WORKTREE_PATH")}"
BASE_REF="${3:-HEAD}"

# Resolve the base ref to a concrete commit for clarity
BASE_COMMIT="$(git -C "$MAIN_REPO" rev-parse --short "$BASE_REF" 2>/dev/null)" || {
    echo "ERROR: Cannot resolve ref '$BASE_REF'" >&2
    exit 1
}
BASE_BRANCH="$(git -C "$MAIN_REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "detached")"

# Resolve tool paths (relative to main repo's parent, e.g. ~/code/milohax/)
TOOL_DIR="$(cd "$MAIN_REPO/.." && pwd)"
DTK_PATH="$MAIN_REPO/build/tools/dtk"
OBJDIFF_PATH="$TOOL_DIR/objdiff/target/release/objdiff-cli"
WIBO_PATH="$TOOL_DIR/wibo/build/release/wibo"

# Verify tools exist
for tool in "$DTK_PATH" "$OBJDIFF_PATH" "$WIBO_PATH"; do
    if [ ! -f "$tool" ]; then
        echo "ERROR: Required tool not found: $tool" >&2
        echo "  (run a full 'ninja' in the main repo at least once first)" >&2
        exit 1
    fi
done

echo "==> Creating worktree at $WORKTREE_PATH"
echo "    Branch: $BRANCH"
echo "    Base:   $BASE_REF ($BASE_COMMIT, on $BASE_BRANCH)"
git -C "$MAIN_REPO" worktree add "$WORKTREE_PATH" -b "$BRANCH" "$BASE_REF"

echo "==> Symlinking clangd config"
ln -sf "$MAIN_REPO/compile_commands.json" "$WORKTREE_PATH/"
ln -sf "$MAIN_REPO/.clangd" "$WORKTREE_PATH/"

echo "==> Symlinking orig/ (target binary)"
# orig/* is gitignored, so the worktree gets an empty dir
rm -rf "$WORKTREE_PATH/orig"
ln -sf "$MAIN_REPO/orig" "$WORKTREE_PATH/orig"

echo "==> Symlinking bin/objdiff-cli"
mkdir -p "$WORKTREE_PATH/bin"
ln -sf "$MAIN_REPO/bin/objdiff-cli" "$WORKTREE_PATH/bin/objdiff-cli"

echo "==> Symlinking shared build artifacts"
WT_BUILD="$WORKTREE_PATH/build/SZBE69_B8"
MAIN_BUILD="$MAIN_REPO/build/SZBE69_B8"
mkdir -p "$WT_BUILD"

# Target objects (original binary, never changes)
ln -sf "$MAIN_BUILD/obj" "$WT_BUILD/obj"

# Pre-split config — MUST exist BEFORE configure.py runs.
# configure.py generates a two-phase build.ninja: phase 1 only has the
# split rule, phase 2 (triggered by ninja generator re-run) has all
# compile rules. Providing config.json first short-circuits the two-phase
# bootstrap so the worktree's first build works.
#
# COPY instead of symlink: the build.ninja "split" rule writes to
# config.json, so a symlink would corrupt the main repo's file.
# Concurrent worktrees would also race on the shared file.
if [ -f "$MAIN_BUILD/config.json" ]; then
    cp "$MAIN_BUILD/config.json" "$WT_BUILD/config.json"
fi

# decomp.db — the default `ninja` build runs a `SYNC decomp.db` step
# (batch_check.py) and the permuter's hill_climber --symbol resolution both
# read this DB. It's gitignored (~20-40MB) so a fresh worktree has none, and
# sqlite would silently CREATE an empty 4096-byte stub on first connect —
# which then fails with "no such table: functions". Seed it from the main
# repo so the worktree is buildable out of the box.
#
# COW REFLINK-COPY (not symlink): the SYNC step and permuter WRITE to
# decomp.db, so a symlink would let this worktree corrupt the source repo's
# DB that the concurrent permuter fleet relies on. A reflink gives each
# worktree its own private, writable copy for free on CoW filesystems.
if [ -f "$MAIN_REPO/decomp.db" ]; then
    echo "==> Seeding decomp.db (reflink copy — private writable DB)"
    cp --reflink=auto "$MAIN_REPO/decomp.db" "$WORKTREE_PATH/decomp.db"
else
    echo "WARN: $MAIN_REPO/decomp.db not found — worktree's SYNC decomp.db step" >&2
    echo "      and permuter --symbol resolution will not work until you seed it." >&2
fi

# Downloaded tools (compilers, binutils, sjiswrap, dtk)
for dir in compilers binutils tools; do
    if [ -e "$MAIN_REPO/build/$dir" ]; then
        ln -sf "$MAIN_REPO/build/$dir" "$WORKTREE_PATH/build/$dir"
    fi
done

echo "==> Symlinking Python venv (if present)"
if [ -d "$MAIN_REPO/venv" ]; then
    ln -sf "$MAIN_REPO/venv" "$WORKTREE_PATH/venv"
fi

echo "==> Running configure.py with absolute tool paths"
(
    cd "$WORKTREE_PATH"
    python3 configure.py \
        --dtk "$DTK_PATH" \
        --objdiff "$OBJDIFF_PATH" \
        --wrapper "$WIBO_PATH"
)

echo ""
echo "Worktree ready at: $WORKTREE_PATH"
echo "Branch: $BRANCH (from $BASE_COMMIT on $BASE_BRANCH)"
echo ""
echo "Usage with MCP orchestrator:"
echo "  run_objdiff(symbol, project_dir=\"$WORKTREE_PATH\")"
