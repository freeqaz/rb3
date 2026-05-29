#!/bin/bash
#
# setup_worktree_native.sh — create an ISOLATED, buildable git worktree for the
# rb3-NATIVE port (CMake/Clang LP64), cheaply, via btrfs/xfs copy-on-write
# reflinks.
#
# NOTE: this is the NATIVE (CMake) counterpart to scripts/setup_worktree.sh,
# which targets the asm-match DECOMP build (ninja + dtk + wibo + configure.py +
# build/SZBE69_B8/). They are different build systems; keep both. Use THIS one
# for `native/build-native` work (rb3-native binary, the milo-native-engine
# consumer build).
#
# Why a plain `git worktree add` is not enough here
# -------------------------------------------------
# The CMake build dir (native/build-native/) is gitignored, so a fresh worktree
# has no build dir at all — a cold reconfigure + full compile is minutes. Worse,
# the main build dir's build.ninja + CMakeCache bake ABSOLUTE source paths into
# the MAIN repo (/home/free/code/milohax/rb3/src/...) and the SHARED engine
# (/home/free/code/milohax/milo-native-engine/src). If you naively reflinked the
# build dir and ran `ninja` without reconfiguring, the worktree would recompile
# the MAIN repo's sources into the worktree's build dir — defeating isolation.
#
# This script:
#   1. `git worktree add` (idempotent — reconfigures in place if it exists).
#   2. Reflink-copies native/build-native/ into the worktree: a PRIVATE, real
#      (never-a-symlink) dir that warm-starts the object cache. A symlink would
#      let the worktree's build corrupt the shared main build dir — fatal with
#      concurrent agents — so we assert it is NOT a symlink (like dc3 does).
#   3. Reconfigures the worktree's build dir IN PLACE (cmake -S <wt>/native -B
#      <wt>/native/build-native) so build.ninja is rewritten to point at the
#      worktree's OWN sources, while reusing the warm reflinked .o cache.
#   4. Resolves the engine:
#        default          → SHARED main engine (read-only), via the absolute
#                           MILO_ENGINE_PATH = /home/free/code/milohax/milo-native-engine
#        --private-engine → reflink-copies the engine to <wt>/.private-engine and
#                           points MILO_ENGINE_PATH there, so engine-editing tasks
#                           don't collide on the shared engine.
#
# What is SHARED vs PRIVATE
# -------------------------
#   Game data (orig-assets/extracted)  NOT copied — runs pass an absolute
#                                       RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted
#   milo-native-engine (default)        SHARED  — absolute MILO_ENGINE_PATH, read-only
#   milo-native-engine (--private-engine) PRIVATE — reflink copy at <wt>/.private-engine
#   native/build-native/                PRIVATE — reflink copy (real dir, warm cache)
#
# Usage:
#   scripts/setup_worktree_native.sh [path] [branch] [base-ref] [--private-engine]
#
#   path             worktree location (default: .claude/worktrees/nwt-<timestamp>)
#   branch           branch name        (default: nwt-<basename of path>)
#   base-ref         git ref to branch from (default: current HEAD)
#   --private-engine give the worktree its own reflinked engine copy + repoint
#
# Examples:
#   scripts/setup_worktree_native.sh .claude/worktrees/rtt-outfit rtt-outfit HEAD --private-engine
#   scripts/setup_worktree_native.sh                          # auto path/branch, shared engine
#
# Prerequisite: the main native build dir must exist + have been configured once
#   (cd native && cmake -B build-native && cmake --build build-native -j).

set -euo pipefail

MAIN_REPO="$(cd "$(dirname "$0")/.." && pwd)"
NATIVE_DIR="$MAIN_REPO/native"
MAIN_BUILD="$NATIVE_DIR/build-native"
SHARED_ENGINE="$(cd "$MAIN_REPO/../milo-native-engine" 2>/dev/null && pwd || true)"
RB3_DATA_DEFAULT="$MAIN_REPO/orig-assets/extracted"

# ---- args: positional path/branch/base-ref + --private-engine flag ----------
POSITIONAL=()
PRIVATE_ENGINE=0
for arg in "$@"; do
    case "$arg" in
        --private-engine) PRIVATE_ENGINE=1 ;;
        -h|--help) sed -n '2,60p' "$0"; exit 0 ;;
        *) POSITIONAL+=("$arg") ;;
    esac
done

WORKTREE_PATH="${POSITIONAL[0]:-$MAIN_REPO/.claude/worktrees/nwt-$(date +%s)}"
# Normalize to absolute (worktree may not exist yet, so resolve the parent).
mkdir -p "$(dirname "$WORKTREE_PATH")"
WORKTREE_PATH="$(cd "$(dirname "$WORKTREE_PATH")" && pwd)/$(basename "$WORKTREE_PATH")"
BRANCH="${POSITIONAL[1]:-nwt-$(basename "$WORKTREE_PATH")}"
BASE_REF="${POSITIONAL[2]:-HEAD}"

WT_NATIVE="$WORKTREE_PATH/native"
WT_BUILD="$WT_NATIVE/build-native"
WT_PRIVATE_ENGINE="$WORKTREE_PATH/.private-engine"

# ---- sanity -----------------------------------------------------------------
[ -d "$MAIN_BUILD" ] || {
    echo "ERROR: main native build dir missing: $MAIN_BUILD" >&2
    echo "  (run: cd $NATIVE_DIR && cmake -B build-native && cmake --build build-native -j)" >&2
    exit 1
}
[ -f "$MAIN_BUILD/CMakeCache.txt" ] || {
    echo "ERROR: $MAIN_BUILD has no CMakeCache.txt — configure the main build first." >&2
    exit 1
}
[ -n "$SHARED_ENGINE" ] && [ -d "$SHARED_ENGINE" ] || {
    echo "ERROR: shared engine not found at $MAIN_REPO/../milo-native-engine" >&2
    exit 1
}

BASE_COMMIT="$(git -C "$MAIN_REPO" rev-parse --short "$BASE_REF" 2>/dev/null)" || {
    echo "ERROR: cannot resolve ref '$BASE_REF'" >&2
    exit 1
}
BASE_BRANCH="$(git -C "$MAIN_REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo detached)"

# ---- reflink helper ---------------------------------------------------------
# CoW copy a directory tree. cp --reflink=auto falls back to a full copy on
# non-CoW filesystems transparently; we warn separately so the operator knows
# the "instant + free" property was lost.
reflink_dir() {
    local src="$1" dst="$2"
    rm -rf "$dst"
    mkdir -p "$(dirname "$dst")"
    cp -a --reflink=auto "$src" "$dst"
}

# Warn (don't fail) if the dest fs can't do reflinks — copies will be full/slow.
DEST_FSTYPE="$(findmnt -no FSTYPE --target "$(dirname "$WORKTREE_PATH")" 2>/dev/null || echo unknown)"
case "$DEST_FSTYPE" in
    btrfs|xfs|zfs) : ;;
    *) echo "WARN: $(dirname "$WORKTREE_PATH") is on '$DEST_FSTYPE'; reflinks may be unavailable — copies will be full (slow, space-hungry)." >&2 ;;
esac

# ---- worktree (idempotent) --------------------------------------------------
if [ -e "$WORKTREE_PATH/.git" ]; then
    echo "==> Worktree already exists at $WORKTREE_PATH (reconfiguring in place)"
else
    echo "==> Creating worktree at $WORKTREE_PATH"
    echo "    branch=$BRANCH  base=$BASE_REF ($BASE_COMMIT, on $BASE_BRANCH)"
    if git -C "$MAIN_REPO" show-ref --verify --quiet "refs/heads/$BRANCH"; then
        git -C "$MAIN_REPO" worktree add "$WORKTREE_PATH" "$BRANCH"
    else
        git -C "$MAIN_REPO" worktree add "$WORKTREE_PATH" -b "$BRANCH" "$BASE_REF"
    fi
fi

# ---- build-native/ : reflink copy (build WRITES here; warm object cache) ----
echo "==> native/build-native/  (reflink copy — PRIVATE build dir + WARM object cache)"
reflink_dir "$MAIN_BUILD" "$WT_BUILD"

# Drop stale CMake/ninja lock + log state copied from main (each build dir owns
# its own). .ninja_log/.ninja_deps are regenerated by the reconfigure+build.
rm -f "$WT_BUILD/.ninja_lock" "$WT_BUILD/.ninja-build.lock" 2>/dev/null || true

# Re-home the reflinked build dir. CMake refuses to reconfigure a cache whose
# CMAKE_HOME_DIRECTORY / CMAKE_CACHEFILE_DIR (baked at original-configure time)
# differ from the dir it now lives in — and the engine's imgui FetchContent
# leaves a NESTED subbuild (_deps/imgui-subbuild) with its own baked cache +
# build.ninja. Rewrite every literal occurrence of the MAIN build dir path →
# the worktree's build dir, across the top-level cache AND all nested
# CMake/ninja metadata, so the reconfigure below is accepted and rewrites
# build.ninja to the worktree's own sources, while the reflinked .o cache +
# _deps stay reusable.
#
# We rewrite the longer "<native>/build-native" path (covers CMAKE_CACHEFILE_DIR,
# *_BINARY_DIR, FETCHCONTENT_BASE_DIR, the redirects dir, and every nested
# subbuild reference) then the bare "<native>" source-dir entries
# (CMAKE_HOME_DIRECTORY, *_SOURCE_DIR). We deliberately do NOT touch Dawn_DIR /
# MILO_ENGINE_PATH — both use "<native>/../../" which neither pattern matches,
# and the engine is repointed explicitly via -DMILO_ENGINE_PATH below.
echo "==> Re-homing reflinked CMake/ninja metadata ($NATIVE_DIR -> $WT_NATIVE)"
while IFS= read -r -d '' f; do
    sed -i "s|$MAIN_BUILD|$WT_BUILD|g" "$f"
done < <(grep -rlZ -- "$MAIN_BUILD" "$WT_BUILD" 2>/dev/null)
# Source-dir entries appear only in the top-level cache.
sed -i \
    -e "s|:INTERNAL=$NATIVE_DIR$|:INTERNAL=$WT_NATIVE|g" \
    -e "s|:STATIC=$NATIVE_DIR$|:STATIC=$WT_NATIVE|g" \
    "$WT_BUILD/CMakeCache.txt"

# ---- safety assertion : worktree build dir must be its own REAL dir ---------
# A symlink here would let this worktree's build corrupt the shared main build
# dir — catastrophic with concurrent agents / the permuter. Abort if so.
if [ -L "$WT_BUILD" ]; then
    echo "FATAL: $WT_BUILD is a symlink — the build would corrupt the main tree. Aborting." >&2
    exit 1
fi

# ---- engine resolution ------------------------------------------------------
if [ "$PRIVATE_ENGINE" -eq 1 ]; then
    echo "==> engine: PRIVATE (reflink copy at $WT_PRIVATE_ENGINE)"
    reflink_dir "$SHARED_ENGINE" "$WT_PRIVATE_ENGINE"
    ENGINE_PATH="$WT_PRIVATE_ENGINE"
else
    echo "==> engine: SHARED (read-only) $SHARED_ENGINE"
    ENGINE_PATH="$SHARED_ENGINE"
fi

# ---- reconfigure the worktree's build dir IN PLACE --------------------------
# This is the load-bearing step: it rewrites build.ninja + CMakeCache so all
# absolute source paths point at the WORKTREE's sources (and the chosen engine),
# while keeping the warm reflinked .o cache. We drop the sticky MILO_ENGINE_PIN
# cache entry (-U) so CMake re-reads the current CMakeLists default and the
# pin-vs-HEAD warning reflects reality. We pass MILO_ENGINE_PATH explicitly so
# --private-engine takes effect (it's a CACHE PATH; an existing reflinked value
# would otherwise stick).
echo "==> Reconfiguring worktree build dir (cmake -S $WT_NATIVE -B $WT_BUILD)"
cmake -S "$WT_NATIVE" -B "$WT_BUILD" \
    -UMILO_ENGINE_PIN \
    -DMILO_ENGINE_PATH="$ENGINE_PATH" \
    >/tmp/nwt_reconfigure.$$.log 2>&1 || {
        echo "FATAL: cmake reconfigure failed — full output:" >&2
        cat /tmp/nwt_reconfigure.$$.log >&2
        rm -f /tmp/nwt_reconfigure.$$.log
        exit 1
    }
# Surface the engine-pin warning (if any) without dumping the whole log.
grep -A3 "milo-native-engine HEAD is" /tmp/nwt_reconfigure.$$.log >&2 || true
rm -f /tmp/nwt_reconfigure.$$.log

# Verify the cache now points where we intend.
ACTUAL_ENGINE="$(grep '^MILO_ENGINE_PATH:' "$WT_BUILD/CMakeCache.txt" | cut -d= -f2-)"
echo "==> MILO_ENGINE_PATH in worktree cache: $ACTUAL_ENGINE"

echo ""
echo "Worktree ready:  $WORKTREE_PATH"
echo "  branch:        $BRANCH  (from $BASE_COMMIT on $BASE_BRANCH)"
echo "  engine:        $([ "$PRIVATE_ENGINE" -eq 1 ] && echo "PRIVATE  $ENGINE_PATH" || echo "SHARED   $ENGINE_PATH")"
echo ""
echo "Next:"
echo "  cmake --build $WT_BUILD -j        # warm reflinked cache = fast incremental"
echo ""
echo "Run headless (short frame budget; absolute RB3_DATA — never copied):"
echo "  RB3_GAME=1 MILO_HEADLESS=1 RB3_DATA=$RB3_DATA_DEFAULT \\"
echo "    MILO_MAX_FRAMES=120 $WT_BUILD/rb3-native"
echo ""
echo "Remove when done:"
echo "  git -C $MAIN_REPO worktree remove --force $WORKTREE_PATH"
