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
#   tools/setup-worktree.sh <name> [base-ref] --engine
#
#   <name>      Worktree + branch name. Worktree is created at
#               .claude/worktrees/<name>, branch is wt-<name>.
#               <name> may also be an absolute/relative path, in which case
#               that exact path is used.
#   base-ref    Git ref to branch from (default: current HEAD).
#   --cold-cache  Do NOT warm-start the object cache (skip reflinking the
#               compiled-object dir). Use for a guaranteed-clean A/B test or
#               if a warm cache triggers a full rebuild on your setup.
#   --engine    ALSO create a PAIRED milo-native-engine worktree, isolating
#               engine edits from the shared engine tree (see below). Opt-in;
#               omit it for the default (shared-engine) behavior.
#
# Paired engine worktree (--engine)
# ---------------------------------
# By default every rb3 worktree shares ONE engine tree via the symlink at
# .claude/worktrees/milo-native-engine -> the sibling milo-native-engine repo.
# That's fine for read-only / rb3-side-only work, but it means ANY engine edit
# leaks into every agent's build — unacceptable for HIGH-RISK work that must
# change BOTH repos at once. `--engine` fixes that: it adds a private
# milo-native-engine worktree (branch wt-<name>, based at the engine's current
# HEAD) and records its path in <wt>/.engine-path. You then point the worktree's
# build at that private engine via the CACHE override:
#
#   cmake -B native/build-native -S native \
#         -DMILO_ENGINE_PATH="$(cat .engine-path)"
#
# native/CMakeLists.txt declares MILO_ENGINE_PATH as a CACHE PATH (default
# ${CMAKE_SOURCE_DIR}/../../milo-native-engine, which resolves to the shared
# symlink from a worktree) and uses it for add_subdirectory + include dirs, so
# the -D override redirects the ENTIRE engine build to the private worktree.
# Because it's a CACHE var, the override only takes on a FRESH build dir (first
# configure wins, then it's sticky) — always pass it to the first `cmake -B`.
#
# Location: the paired engine worktree lives OUTSIDE both repos, at
#   /home/free/code/milohax/milo-native-engine-worktrees/<name>/
# (a sibling dir of the engine repo). Not inside the engine repo (a registered
# worktree there shows as `?? .worktrees/` in the engine repo's git status,
# tripping concurrent agents) and not inside the rb3 worktree (would entangle
# with rb3's reflink/exclude machinery and risk the rb3 build globbing engine
# files). Tear it down with the command printed at the end (and in the docs).
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
#   <wt>/../milo-native-engine
#                            symlink        ONE symlink at .claude/worktrees/
#                                           milo-native-engine -> sibling repo.
#                                           Makes the relative engine path in
#                                           native/CMakeLists.txt
#                                           (${CMAKE_SOURCE_DIR}/../../milo-native-engine)
#                                           resolve from worktrees the same way
#                                           it does from the main repo. Without
#                                           this, cmake configure fails or
#                                           web/native builds pick the wrong
#                                           engine tree. Shared by all worktrees.
#   <wt>/scripts/web/node_modules
#                            symlink        Shared npm install — avoids a slow
#                                           reinstall per agent (and silent
#                                           drift between worktrees). Created
#                                           only if the main tree already has
#                                           the node_modules directory.
#   <wt>/orig-assets/extracted/
#   <wt>/orig-assets/wii/    symlinks       Gitignored heavy subdirs of
#   (and siblings)                          orig-assets/ (extracted content,
#                                           wii/xbox zips). orig-assets/ itself
#                                           is partially tracked (native-refs/
#                                           committed), so we symlink each
#                                           gitignored subdir individually.
#                                           Avoids agents needing --assets-dir
#                                           absolute paths. Created only if
#                                           present in the main tree.
#
# objdiff-cli lives in the sibling repo ../objdiff (build.ninja's report rule
# uses the relative path "../objdiff/target/release/objdiff-cli"). From a
# worktree under .claude/worktrees/, that relative path won't resolve, so we
# pass --objdiff with an ABSOLUTE path to configure.py, baking it into the
# worktree's build.ninja. All OTHER tool paths stay RELATIVE (main's own
# defaults) and main's .ninja_log is seeded into the worktree, so compile
# command hashes match main's log entries and even a full `ninja` in a fresh
# worktree compiles nothing (verified 2026-07-02: 0 MWCC, <1s).
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
PAIR_ENGINE=0
for arg in "$@"; do
    case "$arg" in
        --cold-cache) WARM_CACHE=0 ;;
        --engine) PAIR_ENGINE=1 ;;
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

# ---- bin/objdiff-cli : single symlink inside the worktree's real bin/ -------
# scripts/permuter/project.py + several analysis scripts hard-code
# "bin/objdiff-cli" as a cwd-relative path; without it, permuter runs fail in
# worktrees. Earlier versions of this script replaced the whole `bin/` dir
# with a symlink to main/bin — that shadowed the 9 tracked bin/* scripts and
# created phantom `D bin/*` entries in git status that needed an
# `--assume-unchanged` dance to silence (which sometimes failed).
#
# DC3's setup_worktree.sh has the right shape: let git own the real bin/
# directory (so the tracked wrappers are real files), then drop a single
# symlink for bin/objdiff-cli, which is itself a symlink in main pointing to
# the externally-built objdiff-cli binary. The tracked scripts use
# $(dirname $0)/../tools/... so they work fine from the worktree's bin/.
if [ -e "$MAIN_REPO/bin/objdiff-cli" ]; then
    echo "==> bin/objdiff-cli  (single symlink — host objdiff binary; rest of bin/ is real git checkout)"
    mkdir -p "$WORKTREE_PATH/bin"
    ln -sfn "$OBJDIFF" "$WORKTREE_PATH/bin/objdiff-cli"
fi

# ---- venv : symlink to the MAIN repo's uv-managed .venv (not repo-tree-specific)
# `venv` is gitignored (see .gitignore /venv), so a fresh `git worktree add`
# doesn't carry it over at all — without this the worktree has no
# venv/bin/python and the MCP server / analysis scripts fail to launch.
# Point straight at the main repo's real .venv (absolute path) rather than
# reproducing the main repo's own venv->.venv symlink: the uv env is tooling,
# not build output, so every worktree sharing ONE env is correct (and avoids
# a relative-path depth trap — a worktree-local "../dc3-decomp/venv"-style
# relative link breaks under .claude/worktrees/<name>/, which is one level
# deeper than the main repo root).
if [ -d "$MAIN_REPO/.venv" ]; then
    echo "==> venv  (symlink — main repo's uv-managed .venv, shared across worktrees)"
    ln -sfn "$MAIN_REPO/.venv" "$WORKTREE_PATH/venv"
else
    echo "WARN: $MAIN_REPO/.venv not found — worktree venv/bin/python will be missing (run 'uv sync' in main repo)." >&2
fi

# ---- milo-native-engine : single shared symlink (worktree-relative engine path)
# native/CMakeLists.txt uses `${CMAKE_SOURCE_DIR}/../../milo-native-engine` —
# from a worktree at `.claude/worktrees/<name>/native/`, that resolves to
# `.claude/worktrees/milo-native-engine`. Drop one symlink there so every
# worktree's relative path resolves to the real sibling repo. Idempotent.
ENGINE_REAL="/home/free/code/milohax/milo-native-engine"
ENGINE_LINK_DIR="$MAIN_REPO/.claude/worktrees"
ENGINE_LINK="$ENGINE_LINK_DIR/milo-native-engine"
if [ -d "$ENGINE_REAL" ]; then
    mkdir -p "$ENGINE_LINK_DIR"
    if [ ! -e "$ENGINE_LINK" ] && [ ! -L "$ENGINE_LINK" ]; then
        echo "==> .claude/worktrees/milo-native-engine  (symlink — shared engine path for all worktrees)"
        ln -s "$ENGINE_REAL" "$ENGINE_LINK"
    elif [ -L "$ENGINE_LINK" ]; then
        # Already a symlink; leave as-is (don't disturb other live worktrees).
        :
    else
        echo "WARN: $ENGINE_LINK exists and is not a symlink — leaving alone; web builds in this worktree may pick the wrong engine path." >&2
    fi
else
    echo "WARN: milo-native-engine sibling repo not found at $ENGINE_REAL — web builds in this worktree will fail to find the engine." >&2
fi

# ---- orig-assets/ subdirs : symlinks for gitignored heavy content -----------
# orig-assets/ is partially tracked in git (native-refs/ is committed), so the
# worktree already has a real orig-assets/ directory from the checkout. We can't
# replace it with a symlink. Instead, symlink each gitignored heavy subdir
# (extracted/, extracted-xbox-full/, wii/, wii-extracted/, xbox-zip/, and
# derived/ — the XMA->PCM SFX sidecars server.py serves on demand at
# sfx/gen/xma_pcm/; without it a worktree web server 404s every SFX) from the
# main tree into the worktree's orig-assets/ so web capture/test agents find
# assets at the standard relative path without needing --assets-dir absolute
# paths. Idempotent; skips any subdir that doesn't exist in the main tree.
ORIG_ASSETS_REAL="$MAIN_REPO/orig-assets"
if [ -d "$ORIG_ASSETS_REAL" ]; then
    mkdir -p "$WORKTREE_PATH/orig-assets"
    for sub in extracted extracted-xbox-full wii wii-extracted xbox-zip derived; do
        src="$ORIG_ASSETS_REAL/$sub"
        dst="$WORKTREE_PATH/orig-assets/$sub"
        [ -d "$src" ] || continue   # skip if main tree doesn't have it
        if [ ! -e "$dst" ] && [ ! -L "$dst" ]; then
            echo "==> orig-assets/$sub  (symlink — shared gitignored asset dir)"
            ln -s "$src" "$dst"
        elif [ -L "$dst" ]; then
            : # Already a symlink; leave as-is.
        fi
    done
fi

# ---- song charts : wire the full 83-song set into the native loader root -------
# The loader reads orig-assets/extracted/songs; a dev extract may ship only a few
# songs' .mid charts there while the full set lives in extracted-xbox-full/.
# link-song-charts.sh symlinks the missing charts (idempotent). Worktrees share
# the MAIN tree's extracted/ via the symlink above, so we populate the main tree's
# extract — that benefits this worktree and every other. gitignored => per-machine.
if [ -x "$SCRIPT_DIR/link-song-charts.sh" ] && [ -d "$ORIG_ASSETS_REAL/extracted-xbox-full" ]; then
    echo "==> song charts  (symlink full 83-song set into the loader root)"
    "$SCRIPT_DIR/link-song-charts.sh" "$ORIG_ASSETS_REAL" || \
        echo "WARN: link-song-charts.sh failed — chartless songs will hold at the loading screen." >&2
fi

# ---- node_modules : per-script symlinks (avoid slow per-agent npm install) --
# Find every node_modules dir in the main tree and symlink each one into the
# worktree at the same relative path. Silent if none exist.
if command -v find >/dev/null 2>&1; then
    # -prune so we don't descend into nested node_modules.
    while IFS= read -r main_nm; do
        rel="${main_nm#"$MAIN_REPO/"}"
        wt_nm="$WORKTREE_PATH/$rel"
        # Skip if the worktree doesn't have the parent dir (script isn't tracked here).
        [ -d "$(dirname "$wt_nm")" ] || continue
        echo "==> $rel  (symlink — shared npm install)"
        rm -rf "$wt_nm"
        ln -s "$main_nm" "$wt_nm"
    done < <(find "$MAIN_REPO" -type d -name node_modules -prune \
               -not -path "$MAIN_REPO/.claude/worktrees/*" 2>/dev/null)
fi

# ---- build/<VERSION>/ : reflink copy (build WRITES here; warm cache) --------
WT_BUILD="$WORKTREE_PATH/build/$VERSION"
if [ "$WARM_CACHE" -eq 1 ]; then
    # Bring main's object cache up to date BEFORE snapshotting it. Landings
    # advance main's HEAD but do NOT rebuild, so the shared cache goes stale
    # and every worktree would otherwise recompile the newly-landed TUs.
    # `ninja-locked all_source` builds objects only (skips the report/db-sync
    # tail), serializes across concurrent creations, and is a NO-OP once main
    # is current. Non-fatal: main may be mid-repair (a common reason to spin
    # up a worktree).
    echo "==> Refreshing main's object cache (amortized; no-op if already current)"
    ( cd "$MAIN_REPO" && ./tools/ninja-locked all_source ) >/dev/null 2>&1 \
        || echo "  WARN: main cache refresh failed (non-fatal; worktree will rebuild what's stale)" >&2

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

# ---- decomp.db : reflink copy (private writable DB) --------------------------
# The default `ninja` build ends with a `SYNC decomp.db` edge (batch_check.py)
# and the permuter's --symbol resolution reads this DB. It's gitignored so a
# fresh worktree has none, and sqlite silently CREATES an empty stub on first
# connect — which then fails with "no such table: functions" and makes bare
# `ninja` exit nonzero. Reflink-copy (never symlink): the SYNC step WRITES to
# it, and a symlink would corrupt the main repo's DB the concurrent fleet
# relies on.
if [ -f "$MAIN_REPO/decomp.db" ]; then
    echo "==> Seeding decomp.db (reflink copy — private writable DB)"
    rm -f "$WORKTREE_PATH/decomp.db"
    cp --reflink=auto "$MAIN_REPO/decomp.db" "$WORKTREE_PATH/decomp.db"
else
    echo "WARN: $MAIN_REPO/decomp.db not found — the SYNC decomp.db build edge will fail" >&2
fi

# ---- warm-cache validation : make the reflinked object cache VALID under ninja
# `git worktree add` stamps every checked-out source with a fresh (now) mtime,
# so ninja sees every reflinked object as stale vs its sources. A fresh
# worktree is byte-identical to its base — that's exactly what produced the
# reflinked objects — so old-stamp every tracked source and bump every build
# output; a later source EDIT gets a fresh mtime and rebuilds normally. If the
# worktree DIFFERS from its base (branch reuse, or main has local mods to
# build inputs) we skip this and let ninja rebuild — correctness over speed.
if [ "$WARM_CACHE" -eq 1 ]; then
    # `grep -c` exits 1 on count 0 — `|| true` keeps `set -e` from aborting.
    _changed="$( { git -C "$MAIN_REPO" diff --name-only 2>/dev/null;
                   git -C "$MAIN_REPO" diff --name-only --cached 2>/dev/null;
                   git -C "$WORKTREE_PATH" diff --name-only "$BASE_REF" 2>/dev/null; } \
                 | grep -cE '^(src/|config/)' || true )"
    # ALSO require main's cache to actually BE current (the refresh above is
    # best-effort and can race a concurrent landing). Old-stamping sources
    # over a byte-stale cache would present stale objects as current — for
    # header changes the main-absolute paths in the reflinked .d depfiles
    # self-correct, but a cpp-only landing would be silently false-cleaned.
    _stale_main="$( cd "$MAIN_REPO" && ninja -n all_source 2>/dev/null | grep -c MWCC || true )"
    if [ "$_changed" -eq 0 ] && [ "${_stale_main:-1}" -eq 0 ]; then
        echo "==> Validating warm object cache (worktree == $BASE_REF; marking outputs current)"
        # NB: run FROM the worktree — `git ls-files` prints worktree-relative
        # paths, so touch must resolve them against the worktree.
        ( cd "$WORKTREE_PATH" && git ls-files -z 2>/dev/null \
            | xargs -0 -r touch -h -d '2020-01-01' 2>/dev/null ) || true
        find "$WT_BUILD" -type f -exec touch {} + 2>/dev/null || true
        echo "  reflinked cache marked current — builds start warm"
    else
        echo "==> Warm cache NOT validated ($_changed changed path(s), $_stale_main stale main obj(s)); first build will rebuild what's needed"
    fi
fi

# ---- depfiles : make the reflinked cache LOCATION-INDEPENDENT (correctness) --
# The reflinked .d files may carry ABSOLUTE paths naming the MAIN repo (any
# object compiled before tools/upstream/transform_dep.py started emitting relative
# prerequisites). Because deps="gcc" is deliberately disabled, ninja reads those
# .d files directly — so a header edit INSIDE this worktree would be checked
# against MAIN's copy of the header, find it unchanged, print "no work to do",
# and hand objdiff a STALE, IDENTICAL result. That failure is SILENT: it reports
# NO CHANGE, indistinguishable from a real negative result (lane x24-rotatez
# lost a build cycle to it, 2026-08-04).
#
# Rewriting only changes path SPELLING, never which file is named, so it cannot
# trigger a rebuild — the warm cache above stays warm. The --check pass then
# HARD-FAILS if anything still points at a foreign checkout: better an aborted
# setup than a worktree that silently measures the wrong tree.
#
# Use the worktree's own copy when it has one, else THIS script's sibling copy:
# a worktree based on a ref that predates the normalizer (bisecting, or
# reproducing an old lane) checks out a tools/ without it, and hard-failing
# there would break a legitimate workflow. The tool is checkout-independent —
# it only rewrites build-cache text — so the sibling copy is safe to use.
NORMALIZER="$WORKTREE_PATH/tools/normalize-depfiles.py"
[ -f "$NORMALIZER" ] || NORMALIZER="$SCRIPT_DIR/normalize-depfiles.py"
if [ ! -f "$NORMALIZER" ]; then
    echo "FATAL: tools/normalize-depfiles.py not found in the worktree or next to" >&2
    echo "       this script. Cannot verify the warm cache is location-independent," >&2
    echo "       and an unverified warm cache silently reports STALE results." >&2
    exit 1
fi

echo "==> Normalizing depfiles to repo-relative paths (kills the silent stale-header read)"
if ! python3 "$NORMALIZER" \
        --root "$WORKTREE_PATH" --build-dir "$WT_BUILD" --quiet; then
    echo "FATAL: depfile normalization failed — this worktree would silently report" >&2
    echo "       stale results for header edits. Aborting rather than lying." >&2
    exit 1
fi
if ! python3 "$NORMALIZER" \
        --root "$WORKTREE_PATH" --build-dir "$WT_BUILD" --check --quiet; then
    echo "FATAL: depfiles still reference a foreign checkout after normalization." >&2
    echo "       Recover: find $WT_BUILD -name '*.d' -delete" >&2
    exit 1
fi
echo "  depfiles verified location-independent"

# ---- configure.py : reproduce main's build.ninja commands byte-identically --
# The seeded .ninja_log validates by COMMAND HASH, so the worktree's compile
# commands must match main's exactly:
#   - same interpreter: $python is baked from sys.executable, so run
#     configure.py with the python recorded in main's build.ninja
#   - same configure args (--version/--map) as main
#   - NO --dtk / --wrapper / --compilers overrides: main uses relative
#     build/tools/* + build/compilers, which resolve in the worktree through
#     the build/ symlinks — absolute paths here would change every mwcc
#     command hash and re-trigger the full-rebuild tax
#   - --objdiff absolute is the one exception: the ../objdiff default doesn't
#     resolve from an arbitrary worktree path, and it only appears in the
#     report edge (which regenerates anyway)
MAIN_PYTHON="$(sed -n 's/^python = "\(.*\)"$/\1/p' "$MAIN_REPO/build.ninja" 2>/dev/null | head -1)"
[ -x "$MAIN_PYTHON" ] || MAIN_PYTHON="python3"
# Extracting configure_args needs care on two counts (both hit when the source
# tree is ITSELF a worktree, i.e. creating a worktree from a worktree):
#   1. ninja WRAPS long lines with a trailing `$` continuation. A naive
#      `sed -n 's/^configure_args = //p' | head -1` grabs only the first
#      physical line and yields a dangling `--objdiff $`, so configure.py dies
#      with "argument --objdiff: expected one argument".
#   2. A worktree's build.ninja already records `--objdiff <abs path>` (we added
#      it), so re-appending ours duplicates the flag.
# Unwrap continuations, then drop any existing --objdiff pair; we re-add it
# absolutely below.
MAIN_CFG_ARGS="$(python3 - "$MAIN_REPO/build.ninja" <<'PY' 2>/dev/null || true
import re, sys
try:
    text = open(sys.argv[1]).read()
except OSError:
    sys.exit(0)
# ninja line continuation: "$" at EOL + leading whitespace on the next line
text = re.sub(r"\$\n\s*", " ", text)
for line in text.splitlines():
    if line.startswith("configure_args = "):
        args = line[len("configure_args = "):].split()
        out, i = [], 0
        while i < len(args):
            if args[i] == "--objdiff":
                i += 2          # drop flag AND its value
                continue
            out.append(args[i])
            i += 1
        print(" ".join(out))
        break
PY
)"
[ -n "$MAIN_CFG_ARGS" ] || MAIN_CFG_ARGS="--version $VERSION --map"
echo "==> configure.py $MAIN_CFG_ARGS (main's interpreter, relative tool paths)"
(
    cd "$WORKTREE_PATH"
    # shellcheck disable=SC2086  # MAIN_CFG_ARGS is intentionally word-split
    "$MAIN_PYTHON" configure.py $MAIN_CFG_ARGS \
        --objdiff "$OBJDIFF"
)

# ---- materialise the manifest : leave the worktree with a CURRENT build.ninja
# `configure.py` above WROTE build.ninja, but that does not make ninja consider
# the `build build.ninja: configure` edge satisfied — ninja compares the mtime
# it RECORDED in .ninja_log when it last ran that edge against the edge's
# inputs, and a fresh worktree has git-stamped (= "now") copies of configure.py,
# tools/upstream/project.py, tools/upstream/ninja_syntax.py and config/<VER>/*.json sitting next
# to a .ninja_log seeded from main with older recorded times.
#
# A stale manifest is not merely slow — it makes `ninja -n` return a FALSE
# NEGATIVE. ninja rebuilds the manifest before anything else and then, in
# dry-run mode, deliberately `exit(0)`s (src/ninja.cc: "In dry_run mode the
# regeneration will succeed without changing the manifest forever"), so the dry
# run reports one bookkeeping edge and exits 0 while hundreds of TUs are dirty.
# Dry runs never write .ninja_log, so the state never clears on its own: every
# repeat of the check returns the same empty answer. Measurement lanes live in
# fresh worktrees, which is exactly where this bites.
#
# One real `ninja build.ninja` settles it. It builds ONLY the manifest chain
# (download_tool -> dtk dol split -> configure.py): measured 4.7s worst case,
# 0.13s when already current. Non-fatal — a worktree with a stale manifest is
# still usable, just not dry-runnable.
# See docs/decomp/ninja-dry-run-false-negative.md and tools/ninja-dry.
echo "==> Materialising build.ninja (so \`tools/ninja-dry\` can be trusted here)"
( cd "$WORKTREE_PATH" && ./tools/ninja-locked build.ninja ) >/dev/null 2>&1 \
    || echo "  WARN: 'ninja build.ninja' failed; \`ninja -n\` in this worktree may report a FALSE 'nothing to do'. Use tools/ninja-dry, which detects it." >&2

# ---- safety assertion : worktree build/ must be its own real dir -------------
if [ -L "$WT_BUILD" ]; then
    echo "FATAL: $WT_BUILD is a symlink — the build would corrupt the main tree. Aborting." >&2
    exit 1
fi

# ---- deterministic dev-server port -----------------------------------------
# Hash worktree NAME (not path) -> [8500, 8999]. Two agents picking the same
# name (very unlikely) collide; otherwise every worktree gets a private port
# without ad-hoc allocation. Persisted to .worktree-port so scripts can read
# it without re-hashing. Safe to override by editing the file or passing
# --port to the dev server.
WT_NAME="$(basename "$WORKTREE_PATH")"
WT_PORT=$(( 8500 + $(printf '%s' "$WT_NAME" | cksum | cut -d' ' -f1) % 500 ))
printf '%s\n' "$WT_PORT" > "$WORKTREE_PATH/.worktree-port"

# ---- silence the script's own infrastructure files in `git status` ----------
# .worktree-port is per-worktree dev-server bookkeeping; node_modules symlinks
# are agent-shared npm caches. Neither belongs in the worktree's commit history
# or the main repo's .gitignore. The per-worktree exclude file at
# .git/info/exclude is the right place — it only affects this worktree's index.
# (Resolve the worktree's gitdir via `git rev-parse --git-path` so this works
# whether the worktree is a linked worktree or the main repo itself.)
WT_GITDIR="$(git -C "$WORKTREE_PATH" rev-parse --git-path info 2>/dev/null || true)"
if [ -n "$WT_GITDIR" ] && [ -d "$WT_GITDIR" ]; then
    EXCLUDE_FILE="$WT_GITDIR/exclude"
    for pat in '/.worktree-port' '/scripts/web/node_modules' '/.engine-path'; do
        grep -qxF "$pat" "$EXCLUDE_FILE" 2>/dev/null || printf '%s\n' "$pat" >> "$EXCLUDE_FILE"
    done
fi

# ---- --engine : paired milo-native-engine worktree (opt-in) -----------------
# Create a PRIVATE engine worktree so engine edits in this worktree don't leak
# into the shared engine tree every other agent builds against. Lives OUTSIDE
# both repos (sibling of the engine repo) to avoid git-status noise in the
# engine repo and entanglement with the rb3 worktree. Idempotent: re-running
# reuses an existing worktree/branch. The rb3 build opts in via
# `-DMILO_ENGINE_PATH="$(cat .engine-path)"` (a CACHE override; see header).
ENGINE_WT_PATH=""
if [ "$PAIR_ENGINE" -eq 1 ]; then
    if [ ! -d "$ENGINE_REAL/.git" ]; then
        echo "WARN: --engine requested but $ENGINE_REAL is not a git repo — skipping paired engine worktree." >&2
    else
        ENGINE_WT_NAME="$(basename "$WORKTREE_PATH")"
        ENGINE_WT_PARENT="$(dirname "$ENGINE_REAL")/$(basename "$ENGINE_REAL")-worktrees"
        ENGINE_WT_PATH="$ENGINE_WT_PARENT/$ENGINE_WT_NAME"
        ENGINE_BRANCH="wt-$ENGINE_WT_NAME"
        mkdir -p "$ENGINE_WT_PARENT"
        if [ -e "$ENGINE_WT_PATH/.git" ]; then
            echo "==> engine worktree already exists at $ENGINE_WT_PATH (reusing)"
        else
            ENGINE_HEAD="$(git -C "$ENGINE_REAL" rev-parse --short HEAD 2>/dev/null || echo HEAD)"
            echo "==> Creating paired engine worktree at $ENGINE_WT_PATH"
            echo "    branch=$ENGINE_BRANCH  base=engine HEAD ($ENGINE_HEAD)"
            if git -C "$ENGINE_REAL" show-ref --verify --quiet "refs/heads/$ENGINE_BRANCH"; then
                git -C "$ENGINE_REAL" worktree add "$ENGINE_WT_PATH" "$ENGINE_BRANCH"
            else
                git -C "$ENGINE_REAL" worktree add "$ENGINE_WT_PATH" -b "$ENGINE_BRANCH" HEAD
            fi
        fi
        # Record the engine path for the build override (and for teardown).
        printf '%s\n' "$ENGINE_WT_PATH" > "$WORKTREE_PATH/.engine-path"
    fi
fi

echo ""
echo "Worktree ready:  $WORKTREE_PATH"
echo "  branch:        $BRANCH  (from $BASE_COMMIT)"
echo "  dev port:      $WT_PORT  (deterministic from name; cat .worktree-port; safe to override)"
if [ -n "$ENGINE_WT_PATH" ]; then
    echo "  engine wt:     $ENGINE_WT_PATH  (branch $ENGINE_BRANCH; path in .engine-path)"
fi
echo ""
echo "Next:"
echo "  cd $WORKTREE_PATH"
echo "  tools/ninja-locked build/$VERSION/src/<File>.o   # build one object (warm cache = fast)"
echo "  build/tools/objdiff-cli diff -u <unit> <symbol> --format json-pretty -o /dev/stdout"
echo ""
echo "  # Web dev server (W3+ agents): use the per-worktree port to avoid 8421/8431 collisions:"
echo "  python3 native/web/server.py --port \$(cat .worktree-port)"
if [ -n "$ENGINE_WT_PATH" ]; then
    echo ""
    echo "  # Dual-repo (engine + rb3) build — point rb3-native at the PRIVATE engine worktree."
    echo "  # MILO_ENGINE_PATH is a CACHE var: pass it on the FIRST configure of a FRESH build dir."
    echo "  cmake -B native/build-native -S native -DMILO_ENGINE_PATH=\"\$(cat .engine-path)\""
    echo "  cmake --build native/build-native --target rb3-native -j\"\$(nproc)\""
    echo "  # Landing rule: commit the engine worktree FIRST, then bump MILO_ENGINE_PIN"
    echo "  # in native/CMakeLists.txt to that SHA in the matching rb3 commit."
fi
echo ""
echo "Remove when done:  git -C $MAIN_REPO worktree remove --force $WORKTREE_PATH"
if [ -n "$ENGINE_WT_PATH" ]; then
    echo "                   git -C $ENGINE_REAL worktree remove --force $ENGINE_WT_PATH"
    echo "                   git -C $ENGINE_REAL branch -D $ENGINE_BRANCH   # if no longer needed"
fi
