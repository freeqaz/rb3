#!/bin/bash
#
# setup-worktree.sh — create a buildable + diffable git worktree, cheaply (CoW).
#
# A naive `git worktree add` produces an UNBUILDABLE tree here: the big build
# inputs/outputs are gitignored (build/, orig/*, build.ninja, objdiff.json),
# so a fresh worktree has no target binary, no toolchain, no generated
# build.ninja, and a cold object cache. This script fixes that in seconds using
# btrfs/xfs copy-on-write reflinks instead of full copies.
#
# Usage:
#   tools/setup-worktree.sh <name> [base-ref]
#   tools/setup-worktree.sh <name> [base-ref] --cold-cache
#
#   <name>      Worktree + branch name. Worktree is created at
#               .claude/worktrees/<name>, branch is wt-<name>.
#               <name> may also be an absolute/relative path, in which case
#               that exact path is used.
#   base-ref    Git ref to branch from (default: current HEAD).
#   --cold-cache  Do NOT warm-start the object cache (skip reflinking the
#               compiled-object dir). Use for a guaranteed-clean A/B test or
#               if a warm cache triggers a full rebuild on your setup.
#
# What gets shared, and WHY symlink vs reflink-copy per directory
# ----------------------------------------------------------------
# The rule: anything the BUILD WRITES TO must be a real (reflinked) copy, never
# a symlink into the main tree — a symlink would let this worktree's build
# corrupt the shared main build dir (catastrophic with the permuter fleet
# running). Anything only READ can be a symlink (cheapest).
#
#   orig/                    reflink copy   read-only, but reflink is free on
#                                           CoW and avoids any symlink edge case
#   build/compilers/         symlink        read-only toolchain (mwcceppc); 121M
#   build/tools/             symlink        read-only toolchain (dtk, wibo,
#                                           objdiff-cli); 11M
#   build/SZBE69_B8/         reflink copy   THE build dir. The `split` rule
#                                           (`dtk dol split`) regenerates
#                                           config.json + obj/ INTO this dir,
#                                           and every compiled .o lands in
#                                           src/ here. Must be a private real
#                                           copy. Reflinking it also warm-starts
#                                           the object cache for fast
#                                           incremental builds.
#
# objdiff-cli lives in the sibling repo ../objdiff (build.ninja's report rule
# uses the relative path "../objdiff/target/release/objdiff-cli"). From a
# worktree under .claude/worktrees/, that relative path won't resolve, so we
# pass --objdiff with an ABSOLUTE path to configure.py, baking it into the
# worktree's build.ninja.
#
# After setup:
#   cd <worktree> && tools/ninja-locked build/SZBE69_B8/src/<File>.o
#   build/tools/objdiff-cli diff -u <unit> <symbol> --format json-pretty -o /dev/stdout
#
# Idempotent and safe: never writes into the main repo's build/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MAIN_REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
VERSION="SZBE69_B8"

# ---- args -------------------------------------------------------------------
NAME="${1:-}"
if [ -z "$NAME" ]; then
    echo "Usage: tools/setup-worktree.sh <name> [base-ref] [--cold-cache]" >&2
    exit 2
fi
shift

BASE_REF="HEAD"
WARM_CACHE=1
for arg in "$@"; do
    case "$arg" in
        --cold-cache) WARM_CACHE=0 ;;
        *) BASE_REF="$arg" ;;
    esac
done

# <name> may be a bare name (-> .claude/worktrees/<name>) or a path.
case "$NAME" in
    */*) WORKTREE_PATH="$(cd "$(dirname "$NAME")" 2>/dev/null && pwd)/$(basename "$NAME")" ;;
    *)   WORKTREE_PATH="$MAIN_REPO/.claude/worktrees/$NAME" ;;
esac
BRANCH="wt-$(basename "$WORKTREE_PATH")"

BASE_COMMIT="$(git -C "$MAIN_REPO" rev-parse --short "$BASE_REF" 2>/dev/null)" || {
    echo "ERROR: cannot resolve ref '$BASE_REF'" >&2
    exit 1
}

# ---- reflink helper ---------------------------------------------------------
# Reflink-copy a directory tree (CoW). Falls back to a normal copy if the
# filesystem doesn't support reflinks (cp --reflink=auto handles that), but we
# warn so the operator knows the "instant + free" property was lost.
reflink_dir() {
    local src="$1" dst="$2"
    rm -rf "$dst"
    mkdir -p "$(dirname "$dst")"
    cp -a --reflink=auto "$src" "$dst"
}

# ---- tool sanity ------------------------------------------------------------
DTK="$MAIN_REPO/build/tools/dtk"
WIBO="$MAIN_REPO/build/tools/wibo"
COMPILERS="$MAIN_REPO/build/compilers"
# Resolve objdiff-cli to a real absolute path (it's a symlink chain).
OBJDIFF="$(readlink -f "$MAIN_REPO/bin/objdiff-cli" 2>/dev/null || echo "$MAIN_REPO/bin/objdiff-cli")"
for t in "$DTK" "$WIBO" "$OBJDIFF"; do
    [ -e "$t" ] || { echo "ERROR: required tool missing: $t" >&2; exit 1; }
done
[ -d "$COMPILERS" ] || { echo "ERROR: compilers dir missing: $COMPILERS" >&2; exit 1; }

# Warn if the repo isn't on a reflink-capable fs (script still works, just slow).
FSTYPE="$(findmnt -no FSTYPE --target "$MAIN_REPO" 2>/dev/null || echo unknown)"
case "$FSTYPE" in
    btrfs|xfs|zfs) : ;;
    *) echo "WARN: $MAIN_REPO is on '$FSTYPE'; reflinks may be unavailable — copies will be full (slow, space-hungry)." >&2 ;;
esac

# ---- worktree (idempotent) --------------------------------------------------
if [ -e "$WORKTREE_PATH/.git" ]; then
    echo "==> Worktree already exists at $WORKTREE_PATH (reconfiguring in place)"
else
    echo "==> Creating worktree at $WORKTREE_PATH"
    echo "    branch=$BRANCH  base=$BASE_REF ($BASE_COMMIT)"
    if git -C "$MAIN_REPO" show-ref --verify --quiet "refs/heads/$BRANCH"; then
        git -C "$MAIN_REPO" worktree add "$WORKTREE_PATH" "$BRANCH"
    else
        git -C "$MAIN_REPO" worktree add "$WORKTREE_PATH" -b "$BRANCH" "$BASE_REF"
    fi
fi

# ---- orig/ : reflink copy (read-only, but free on CoW) ----------------------
echo "==> orig/  (reflink copy — target binaries)"
reflink_dir "$MAIN_REPO/orig" "$WORKTREE_PATH/orig"

# ---- build/compilers, build/tools : symlinks (read-only toolchain) ----------
mkdir -p "$WORKTREE_PATH/build"
for d in compilers tools; do
    echo "==> build/$d  (symlink — read-only toolchain)"
    rm -rf "$WORKTREE_PATH/build/$d"
    ln -s "$MAIN_REPO/build/$d" "$WORKTREE_PATH/build/$d"
done

# ---- bin/ : symlink (read-only — wraps objdiff-cli + other host tools) ------
# scripts/permuter/project.py + several analysis scripts hard-code "bin/objdiff-cli"
# as a cwd-relative path; without this symlink, permuter runs fail in worktrees.
if [ -d "$MAIN_REPO/bin" ]; then
    echo "==> bin/  (symlink — read-only host-tool wrappers)"
    rm -rf "$WORKTREE_PATH/bin"
    ln -s "$MAIN_REPO/bin" "$WORKTREE_PATH/bin"
fi

# ---- build/<VERSION>/ : reflink copy (build WRITES here; warm cache) --------
WT_BUILD="$WORKTREE_PATH/build/$VERSION"
if [ "$WARM_CACHE" -eq 1 ]; then
    echo "==> build/$VERSION/  (reflink copy — private build dir + WARM object cache)"
    reflink_dir "$MAIN_REPO/build/$VERSION" "$WT_BUILD"
else
    echo "==> build/$VERSION/  (cold: copying only obj/ + config.json, no object cache)"
    rm -rf "$WT_BUILD"
    mkdir -p "$WT_BUILD"
    # obj/ (target objects from `dtk dol split`) and config.json are inputs the
    # build needs even with a cold src/ cache. Reflink them so split/diff work.
    [ -d "$MAIN_REPO/build/$VERSION/obj" ] && reflink_dir "$MAIN_REPO/build/$VERSION/obj" "$WT_BUILD/obj"
    [ -f "$MAIN_REPO/build/$VERSION/config.json" ] && cp --reflink=auto "$MAIN_REPO/build/$VERSION/config.json" "$WT_BUILD/config.json"
fi

# Drop stale ninja state copied from main (own lock + logs per build dir).
rm -f "$WORKTREE_PATH/.ninja_log" "$WORKTREE_PATH/.ninja_deps" \
      "$WORKTREE_PATH/.ninja_lock" "$WORKTREE_PATH/.ninja-build.lock" 2>/dev/null || true

# ---- configure.py : bake absolute tool paths into this worktree's build.ninja
echo "==> configure.py --version $VERSION --map (absolute tool paths)"
(
    cd "$WORKTREE_PATH"
    python3 configure.py \
        --version "$VERSION" \
        --map \
        --dtk "$DTK" \
        --wrapper "$WIBO" \
        --compilers "$COMPILERS" \
        --objdiff "$OBJDIFF"
)

# ---- safety assertion : worktree build/ must be its own real dir -------------
if [ -L "$WT_BUILD" ]; then
    echo "FATAL: $WT_BUILD is a symlink — the build would corrupt the main tree. Aborting." >&2
    exit 1
fi

echo ""
echo "Worktree ready:  $WORKTREE_PATH"
echo "  branch:        $BRANCH  (from $BASE_COMMIT)"
echo ""
echo "Next:"
echo "  cd $WORKTREE_PATH"
echo "  tools/ninja-locked build/$VERSION/src/<File>.o   # build one object (warm cache = fast)"
echo "  build/tools/objdiff-cli diff -u <unit> <symbol> --format json-pretty -o /dev/stdout"
echo ""
echo "Remove when done:  git -C $MAIN_REPO worktree remove --force $WORKTREE_PATH"
