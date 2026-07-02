#!/bin/bash
#
# setup_worktree.sh — create a buildable + diffable git worktree, cheaply (CoW).
#
# A naive `git worktree add` produces an UNBUILDABLE tree here: the big build
# inputs/outputs are gitignored (build/, orig/*, build.ninja, decomp.db), so a
# fresh worktree has no target binary, no toolchain, no generated build.ninja,
# and a cold object cache. This script fixes that in seconds using btrfs/xfs
# copy-on-write reflinks (with graceful fall-back to full copies on non-CoW
# filesystems like tmpfs/ext4). The repo lives on btrfs, so reflinks are the
# normal path.
#
# Usage:
#   scripts/setup_worktree.sh [path] [branch-name] [base-ref] [--cold-cache]
#
# Arguments:
#   path       - Where to create the worktree (default: .claude/worktrees/wt-<timestamp>)
#   branch     - Branch name for the worktree (default: wt-<basename of path>)
#   base-ref   - Git ref to branch from (default: current HEAD)
#   --cold-cache - Do NOT warm-start the object cache. Use for a guaranteed-clean
#                  A/B test or if a warm cache misbehaves on your setup.
#
# NB: put worktrees on the SAME btrfs as the repo (e.g. .claude/worktrees/ or
# ~/tmp), NEVER /tmp — /tmp is a RAM-backed tmpfs with no reflink support, so
# the object-cache copy silently degrades to a full copy and eats RAM.
#
# Examples:
#   scripts/setup_worktree.sh .claude/worktrees/my-feature my-feature
#   scripts/setup_worktree.sh ~/tmp/wt-test test-branch master
#   scripts/setup_worktree.sh                                # auto-generates path/branch
#   scripts/setup_worktree.sh .claude/worktrees/perf perf --cold-cache
#
# What gets shared, and WHY symlink vs reflink-copy per directory
# ----------------------------------------------------------------
# The rule: anything the BUILD WRITES TO must be a real (reflinked) copy, never
# a symlink into the main tree — a symlink would let this worktree's build
# corrupt the shared main build dir (catastrophic with concurrent agents
# running). Anything only READ can be a symlink (cheapest).
#
#   orig/                    reflink copy   read-only, but reflink is free on
#                                           CoW and avoids any symlink edge case
#   build/compilers/         symlink        read-only MWCC toolchain
#   build/binutils/          symlink        read-only toolchain
#   build/tools/             symlink        read-only tool binaries (dtk,
#                                           sjiswrap, wibo — download_tool
#                                           outputs, no cargo edges here)
#   build/SZBE69_B8/         reflink copy   THE build dir. The `split` rule
#                                           regenerates config.json + obj/ INTO
#                                           this dir, and every compiled .o
#                                           lands in src/ here. Must be a
#                                           private real copy. Reflinking it
#                                           also warm-starts the object cache —
#                                           without it every new worktree pays
#                                           a full MWCC rebuild.
#   decomp.db                reflink copy   the SYNC step + permuter WRITE it
#
# After setup:
#   cd <worktree>
#   ./tools/ninja-locked build/SZBE69_B8/src/system/obj/Object.o
#
# Or via the MCP orchestrator:
#   run_objdiff(symbol, project_dir="<worktree>")
#
# Prerequisite: the main repo must have been built once (build/tools/ +
# build/compilers/ populated by configure.py's download step) and the sibling
# ../objdiff fork must have been built.

set -euo pipefail

MAIN_REPO="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="SZBE69_B8"

# ---- args (positional path / branch / base-ref + --cold-cache flag) ---------
POSITIONAL=()
WARM_CACHE=1
for arg in "$@"; do
    case "$arg" in
        --cold-cache) WARM_CACHE=0 ;;
        *) POSITIONAL+=("$arg") ;;
    esac
done

WORKTREE_PATH="${POSITIONAL[0]:-$MAIN_REPO/.claude/worktrees/wt-$(date +%s)}"
BRANCH="${POSITIONAL[1]:-wt-$(basename "$WORKTREE_PATH")}"
BASE_REF="${POSITIONAL[2]:-HEAD}"

# Resolve the base ref to a concrete commit for clarity
BASE_COMMIT="$(git -C "$MAIN_REPO" rev-parse --short "$BASE_REF" 2>/dev/null)" || {
    echo "ERROR: Cannot resolve ref '$BASE_REF'" >&2
    exit 1
}
BASE_BRANCH="$(git -C "$MAIN_REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "detached")"

# ---- tool sanity -------------------------------------------------------------
# dtk + wibo come from main's build/tools (download_tool outputs, reached
# through the build/tools symlink with main's own RELATIVE paths — see the
# configure step for why relative matters). objdiff-cli is the prebuilt
# sibling-repo fork, passed ABSOLUTE because its ../objdiff default doesn't
# resolve from an arbitrary worktree path (only the report edge diverges).
TOOL_DIR="$(cd "$MAIN_REPO/.." && pwd)"
DTK="$MAIN_REPO/build/tools/dtk"
WIBO="$MAIN_REPO/build/tools/wibo"
# Prefer the objdiff-cli that bin/objdiff-cli already points at (its symlink
# target is the canonical built fork); fall back to the sibling path.
OBJDIFF="$(readlink -f "$MAIN_REPO/bin/objdiff-cli" 2>/dev/null || echo "$TOOL_DIR/objdiff/target/release/objdiff-cli")"
COMPILERS="$MAIN_REPO/build/compilers"
for t in "$DTK" "$OBJDIFF" "$WIBO"; do
    [ -e "$t" ] || {
        echo "ERROR: required tool missing: $t" >&2
        echo "  (run a full build in the main repo once, and build ../objdiff)" >&2
        exit 1
    }
done
[ -d "$COMPILERS" ] || {
    echo "ERROR: compilers dir missing: $COMPILERS" >&2
    echo "  (run configure.py + a full build in the main repo at least once first)" >&2
    exit 1
}

# ---- reflink helpers ---------------------------------------------------------
# Reflink-copy a directory tree (CoW). Falls back to a normal copy if the
# filesystem doesn't support reflinks (cp --reflink=auto handles that
# transparently). Retries transient failures: when $src is a shared build dir
# being written by concurrent builds, `cp -a` can abort with "file changed as
# we read it" or a vanished temp file. A retry after a short pause usually
# lands in a quiet window.
reflink_dir() {
    local src="$1" dst="$2" tries="${3:-4}" i
    mkdir -p "$(dirname "$dst")"
    for ((i=1; i<=tries; i++)); do
        rm -rf "$dst"
        if cp -a --reflink=auto "$src" "$dst" 2>/dev/null; then
            return 0
        fi
        sleep $((i))
    done
    return 1
}

# Best-effort reflink for a REGENERABLE cache (the build dir): tolerate a
# partial copy — a few temp files that vanished mid-copy under concurrent
# writes just get recompiled by ninja. Fails only if the copy produced nothing.
reflink_dir_besteffort() {
    local src="$1" dst="$2" i
    mkdir -p "$(dirname "$dst")"
    rm -rf "$dst"
    for i in 1 2 3 4; do
        if cp -a --reflink=auto "$src" "$dst" 2>/dev/null; then return 0; fi
        # partial copy left in place is fine; retry to fill in more, then accept
        sleep "$i"
    done
    [ -d "$dst" ] && return 0
    return 1
}

# Warn if the destination isn't on a reflink-capable fs (script still works,
# just slow because cp falls back to full copies).
DEST_FSTYPE="$(findmnt -no FSTYPE --target "$(dirname "$WORKTREE_PATH")" 2>/dev/null || echo unknown)"
case "$DEST_FSTYPE" in
    btrfs|xfs|zfs) : ;;
    *) echo "WARN: $(dirname "$WORKTREE_PATH") is on '$DEST_FSTYPE'; reflinks may be unavailable — copies will be full (slow, space-hungry). Prefer .claude/worktrees/ or ~/tmp." >&2 ;;
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

# ---- orig/ : reflink copy (read-only, but free on CoW) ----------------------
echo "==> orig/  (reflink copy — target binaries)"
reflink_dir "$MAIN_REPO/orig" "$WORKTREE_PATH/orig"

# ---- build/compilers, build/binutils, build/tools : symlinks (read-only) ----
mkdir -p "$WORKTREE_PATH/build"
for d in compilers binutils tools; do
    if [ -e "$MAIN_REPO/build/$d" ]; then
        echo "==> build/$d  (symlink — read-only toolchain)"
        rm -rf "$WORKTREE_PATH/build/$d"
        ln -s "$MAIN_REPO/build/$d" "$WORKTREE_PATH/build/$d"
    fi
done

# ---- build/<VERSION>/ : reflink copy (build WRITES here; warm cache) --------
WT_BUILD="$WORKTREE_PATH/build/$VERSION"
if [ "$WARM_CACHE" -eq 1 ]; then
    # Bring main's object cache up to date BEFORE snapshotting it. Landings
    # advance main's HEAD but do NOT rebuild, so the shared cache goes stale
    # and every worktree would otherwise recompile the newly-landed TUs
    # (cold-cache contention — builds crawl). `ninja-locked all_source` builds
    # the objects only (skips the slow report/db-sync tail), serializes across
    # concurrent worktree creations, and is a NO-OP once main is current — so
    # only the FIRST creation after a landing pays the small incremental
    # rebuild and every other worktree reflinks an already-warm cache for
    # free. Non-fatal: main may be mid-repair (a common reason to spin up a
    # worktree).
    echo "==> Refreshing main's object cache (amortized; no-op if already current)"
    ( cd "$MAIN_REPO" && ./tools/ninja-locked all_source ) >/dev/null 2>&1 \
        || echo "  WARN: main cache refresh failed (non-fatal; worktree will rebuild what's stale)" >&2

    echo "==> build/$VERSION/  (reflink copy — private build dir + WARM object cache)"
    reflink_dir_besteffort "$MAIN_REPO/build/$VERSION" "$WT_BUILD"
    # Critical build inputs must survive the (possibly partial) copy. If a temp
    # .o vanished mid-copy that's fine (ninja recompiles), but obj/ (target
    # split objects) and config.json are required — reflink them individually
    # if the best-effort pass dropped them.
    [ -d "$WT_BUILD/obj" ] || reflink_dir "$MAIN_REPO/build/$VERSION/obj" "$WT_BUILD/obj"
    [ -f "$WT_BUILD/config.json" ] || cp --reflink=auto "$MAIN_REPO/build/$VERSION/config.json" "$WT_BUILD/config.json" 2>/dev/null || true
else
    echo "==> build/$VERSION/  (cold: copying only obj/ + config.json, no object cache)"
    rm -rf "$WT_BUILD"
    mkdir -p "$WT_BUILD"
    # obj/ (target objects from split) and config.json are inputs the build
    # needs even with a cold src/ cache. COPY (not symlink) — the split rule
    # writes to both, and a symlink would let this worktree corrupt main.
    [ -d "$MAIN_REPO/build/$VERSION/obj" ] && reflink_dir "$MAIN_REPO/build/$VERSION/obj" "$WT_BUILD/obj"
    [ -f "$MAIN_REPO/build/$VERSION/config.json" ] && cp --reflink=auto "$MAIN_REPO/build/$VERSION/config.json" "$WT_BUILD/config.json"
fi

# Drop stale ninja state copied from main (own lock + logs per build dir).
rm -f "$WORKTREE_PATH/.ninja_log" "$WORKTREE_PATH/.ninja_deps" \
      "$WORKTREE_PATH/.ninja_lock" "$WORKTREE_PATH/.ninja-build.lock" 2>/dev/null || true

# ---- .ninja_log : seed from main (ninja >=1.13 needs log entries) ------------
# Mtime-stamping alone is NOT enough for a warm cache: ninja 1.13+ marks any
# edge dirty whose output has no .ninja_log entry ("command line not found in
# log"), even when output mtime > input mtimes. Seeding main's log gives every
# reflinked output its entry. The entries validate because the configure step
# below reproduces main's compile commands BYTE-IDENTICALLY (same interpreter,
# same relative tool paths), so the recorded command hashes match. Edges whose
# commands do legitimately diverge (the objdiff report edge) just rebuild.
if [ "$WARM_CACHE" -eq 1 ] && [ -f "$MAIN_REPO/.ninja_log" ]; then
    echo "==> Seeding .ninja_log from main (command hashes match — full builds stay warm)"
    cp "$MAIN_REPO/.ninja_log" "$WORKTREE_PATH/.ninja_log"
fi

# ---- warm-cache validation : make the reflinked object cache VALID under ninja
# `git worktree add` stamps every checked-out source with a fresh (now) mtime,
# and the toolchain symlinks resolve to main's dirs (recent mtimes, shared
# implicit inputs to every compile). Ninja is mtime-only (no content hashing),
# so it treats every reflinked object as stale vs those inputs and does a FULL
# MWCC REBUILD on first use — N worktrees per wave => N redundant full rebuilds
# + machine saturation.
#
# A fresh worktree is byte-identical to its base ($BASE_REF) — that's exactly
# what produced the reflinked objects — so the whole reflinked cache IS
# current. Old-stamp every tracked source and bump every build output so ninja
# sees the cache as up-to-date by MTIME; a later source EDIT gets a fresh
# mtime and rebuilds normally. If the worktree DIFFERS from its base (branch
# reuse, or main has local mods to build inputs) we skip this and let ninja
# rebuild — correctness over speed.
#
# This repo's compile rules use on-disk .d depfiles (no ninja binary deps
# cache — see CLAUDE.md "No binary deps cache"), and the depfiles ride along
# inside the reflinked build dir, so after this validation even a full `ninja`
# is a near-no-op (only the always-on report/db-sync tail runs).
if [ "$WARM_CACHE" -eq 1 ]; then
    # Only BUILD INPUTS matter for cache validity — a compiled source/header
    # (src/) or a split/objects/symbols config (config/). Dirty scripts, docs,
    # or tooling in main don't make any reflinked object stale, so exclude
    # them. NB: `grep -c` exits 1 when the count is 0 — the `|| true` keeps
    # `set -e` from aborting the whole script when main is clean.
    _changed="$( { git -C "$MAIN_REPO" diff --name-only 2>/dev/null;
                   git -C "$MAIN_REPO" diff --name-only --cached 2>/dev/null;
                   git -C "$WORKTREE_PATH" diff --name-only "$BASE_REF" 2>/dev/null; } \
                 | grep -cE '^(src/|config/)' || true )"
    if [ "$_changed" -eq 0 ]; then
        echo "==> Validating warm object cache (worktree == $BASE_REF; marking outputs current)"
        # Old-stamp every tracked source (a fixed old timestamp, not main's,
        # which may itself be recent) so no tracked input — including
        # tools/download_tool.py, which feeds the toolchain download edges —
        # is ever newer than the reflinked outputs. A later source EDIT still
        # gets a fresh mtime and rebuilds normally.
        # NB: run FROM the worktree — `git ls-files` prints worktree-relative
        # paths, so touch must resolve them against the worktree, not the
        # setup script's CWD, or it silently touches main's copies instead.
        ( cd "$WORKTREE_PATH" && git ls-files -z 2>/dev/null \
            | xargs -0 -r touch -h -d '2020-01-01' 2>/dev/null ) || true
        find "$WT_BUILD" -type f -exec touch {} + 2>/dev/null || true
        echo "  reflinked cache marked current — builds start warm"
    else
        echo "==> Warm cache NOT validated: worktree differs from $BASE_REF ($_changed path(s)); first build will rebuild"
    fi
fi

# ---- decomp.db : reflink copy (private writable DB) -------------------------
# The default `ninja` build runs a `SYNC decomp.db` step (batch_check.py) and
# the permuter's hill_climber --symbol resolution both read this DB. It's
# gitignored (~20-40MB) so a fresh worktree has none, and sqlite would
# silently CREATE an empty stub on first connect — which then fails with "no
# such table: functions". Reflink-copy (never symlink): the SYNC step and
# permuter WRITE to it, and a symlink would corrupt the main repo's DB that
# the concurrent fleet relies on.
if [ -f "$MAIN_REPO/decomp.db" ]; then
    echo "==> Seeding decomp.db (reflink copy — private writable DB)"
    cp --reflink=auto "$MAIN_REPO/decomp.db" "$WORKTREE_PATH/decomp.db"
else
    echo "WARN: $MAIN_REPO/decomp.db not found — worktree's SYNC decomp.db step" >&2
    echo "      and permuter --symbol resolution will not work until you seed it." >&2
fi

# ---- clangd config + bin/objdiff-cli + venv : read-only symlinks ------------
echo "==> Symlinking clangd config"
[ -e "$MAIN_REPO/compile_commands.json" ] && ln -sfn "$MAIN_REPO/compile_commands.json" "$WORKTREE_PATH/compile_commands.json"
[ -e "$MAIN_REPO/.clangd" ] && ln -sfn "$MAIN_REPO/.clangd" "$WORKTREE_PATH/.clangd"

echo "==> Symlinking bin/objdiff-cli"
mkdir -p "$WORKTREE_PATH/bin"
ln -sfn "$MAIN_REPO/bin/objdiff-cli" "$WORKTREE_PATH/bin/objdiff-cli"

if [ -d "$MAIN_REPO/venv" ]; then
    echo "==> Symlinking Python venv"
    ln -sfn "$MAIN_REPO/venv" "$WORKTREE_PATH/venv"
fi

# ---- configure.py : reproduce main's build.ninja commands byte-identically --
# The seeded .ninja_log validates by COMMAND HASH, so the worktree's compile
# commands must match main's exactly:
#   - same interpreter: $python is baked from sys.executable, so run
#     configure.py with the python recorded in main's build.ninja
#   - same configure args (--version/--map) as main
#   - NO --dtk / --wrapper overrides: main uses relative build/tools/dtk +
#     build/tools/wibo, which resolve in the worktree through the build/tools
#     symlink — passing absolute paths here would change every mwcc command
#     hash and re-trigger the full-rebuild tax
#   - --objdiff absolute is the one exception: the ../objdiff default doesn't
#     resolve from an arbitrary worktree path, and it only appears in the
#     report edge (which regenerates anyway)
MAIN_PYTHON="$(sed -n 's/^python = "\(.*\)"$/\1/p' "$MAIN_REPO/build.ninja" 2>/dev/null | head -1)"
[ -x "$MAIN_PYTHON" ] || MAIN_PYTHON="python3"
MAIN_CFG_ARGS="$(sed -n 's/^configure_args = //p' "$MAIN_REPO/build.ninja" 2>/dev/null | head -1)"
[ -n "$MAIN_CFG_ARGS" ] || MAIN_CFG_ARGS="--version $VERSION --map"
echo "==> Running configure.py (main's interpreter + args, relative tool paths)"
(
    cd "$WORKTREE_PATH"
    # shellcheck disable=SC2086  # MAIN_CFG_ARGS is intentionally word-split
    "$MAIN_PYTHON" configure.py $MAIN_CFG_ARGS \
        --objdiff "$OBJDIFF"
)

# ---- safety assertion : worktree build/ must be its own real dir ------------
if [ -L "$WT_BUILD" ]; then
    echo "FATAL: $WT_BUILD is a symlink — the build would corrupt the main tree. Aborting." >&2
    exit 1
fi

# ---- safety assertion : configure.py must have produced build.ninja ---------
# Any silent configure failure that leaves no build.ninja produces an
# UNBUILDABLE worktree: every build dies with `ninja: error: loading
# 'build.ninja'` and stale reports read as false results. Fail LOUD here
# instead of downstream.
if [ ! -f "$WORKTREE_PATH/build.ninja" ]; then
    echo "FATAL: configure.py did not produce $WORKTREE_PATH/build.ninja." >&2
    echo "       The worktree is unbuildable; refusing to hand back a broken tree." >&2
    exit 1
fi

# ---- prune zero-byte orphan .o files -----------------------------------------
# The main build dir can accumulate zero-byte orphan objects with no ninja
# rule. Reflinking them into the worktree can confuse downstream tooling.
if [ -d "$WT_BUILD/src" ]; then
    pruned=$(find "$WT_BUILD/src" -name '*.o' -type f -size 0 -print -delete 2>/dev/null | wc -l)
    [ "$pruned" -gt 0 ] && echo "==> Pruned $pruned zero-byte orphan .o file(s)"
fi

# ---- prime ninja state : settle SPLIT + the configure generator edge --------
# Without this, the worktree's first `ninja -t commands <obj>` query (used by
# the permuter, MCP orchestrator, and objdiff scripts) can return commands
# derived from a not-yet-fully-consistent build.ninja. Priming the
# `config.json` target re-runs SPLIT if needed and settles the configure.py
# generator edge, leaving the build graph fully consistent — WITHOUT building
# any objects (a bare `ninja` prime would run the full default target).
# With the warm validated cache above, this is typically a fast no-op.
#
# A failed prime is a loud WARNING, not fatal: the main tree is frequently
# mid-repair (often the very reason to spin up a worktree), so a prime failure
# must not block worktree creation. `WT_SKIP_PRIME=1` skips it entirely.
if [ "${WT_SKIP_PRIME:-0}" -eq 1 ]; then
    echo "==> WT_SKIP_PRIME=1 : skipping ninja prime (worktree configured but not primed)"
else
echo "==> Priming ninja state (scoped to config.json — no object builds)"
(
    cd "$WORKTREE_PATH"
    NINJA="./tools/ninja-locked"
    [ -x "$NINJA" ] || NINJA="ninja"
    prime_log="$(mktemp)"
    if "$NINJA" "build/$VERSION/config.json" >"$prime_log" 2>&1; then
        tail -5 "$prime_log"
    else
        echo "WARN: ninja prime failed (the main tree may not build yet)." >&2
        echo "      The worktree is still configured and usable; fix the build inside it." >&2
        echo "      ---- prime output ----" >&2
        cat "$prime_log" >&2
        echo "      ----------------------" >&2
    fi
    rm -f "$prime_log"
)
fi

echo ""
echo "Worktree ready:  $WORKTREE_PATH"
echo "  branch:        $BRANCH  (from $BASE_COMMIT on $BASE_BRANCH)"
echo ""
echo "Next:"
echo "  cd $WORKTREE_PATH"
echo "  ./tools/ninja-locked build/$VERSION/src/<File>.o   # warm cache = fast"
echo ""
echo "Usage with MCP orchestrator:"
echo "  run_objdiff(symbol, project_dir=\"$WORKTREE_PATH\")"
echo ""
echo "Remove when done:  git -C $MAIN_REPO worktree remove --force $WORKTREE_PATH"
