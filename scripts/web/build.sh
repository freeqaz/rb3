#!/usr/bin/env bash
# Build the RB3 Emscripten/WebAssembly target(s) and deploy artifacts next to
# native/web/index.html so server.py can serve them.
#
# DUAL BUILD: this script produces TWO deployable builds you can switch between
# at runtime via a URL param:
#   - release/  — size-opt, debug-info-stripped (-g0). Served with long-lived
#                 immutable HTTP caching (server.py), so RELOADS ARE FAST: the
#                 browser reuses the cached + already-compiled wasm. This is what
#                 http://host:8421/ loads by default.
#   - debug/    — full debug info (-O0 -g2), served no-store for fast iteration.
#                 Loaded by http://host:8421/?debug=true (bypasses the cache).
#
# Emscripten: auto-activates from $EMSDK or ~/emsdk if emcc isn't on PATH yet.
#
# Usage:
#   scripts/web/build.sh                # build BOTH release + debug (default)
#   scripts/web/build.sh --release      # build release only
#   scripts/web/build.sh --debug        # build debug only (fast iteration loop)
#   scripts/web/build.sh --reconfigure  # force a fresh cmake configure
#   scripts/web/build.sh --opt Os       # higher -O for release (BROKEN as of W4a;
#                                       # matched-fork throws during App ctor at -O>0)
#   scripts/web/build.sh --closure      # --closure 1 (BROKEN as of W4a; renames
#                                       # break rb3_pre.js stub patch)
#
# The release build pre-compresses .wasm + .js with brotli (-q 11) and gzip (-9)
# so server.py negotiates Content-Encoding with no runtime CPU cost. The debug
# build only gzips (brotli q11 on the 28M -g2 wasm is the multi-minute step, and
# debug is served no-store for local iteration anyway).
#
# Output:
#   native/build-web/         — emcc build dir for debug   (RB3_WEB_RELEASE=OFF)
#   native/build-web-release/ — emcc build dir for release (RB3_WEB_RELEASE=ON)
#   native/web/build/
#     index.html, audio-worklet.js          — shared, served from the root
#     release/{rb3-web.js,.wasm,.br,.gz}     — cached (immutable)
#     debug/{rb3-web.js,.wasm,.gz}           — no-store
#
# The audio-worklet.js POST_BUILD copy (to web/build/ root) is handled by
# milo_engine_apply_web_target_options in native/CMakeLists.txt. It stays at the
# root because the engine loads it via a document-relative
# `audioWorklet.addModule('audio-worklet.js')`, which must resolve for BOTH
# builds regardless of which subdir the wasm came from.
#
# Worktree note: `${CMAKE_SOURCE_DIR}/../../milo-native-engine` (in
# native/CMakeLists.txt) DOES NOT resolve from .claude/worktrees/<name>/.
# We pass MILO_ENGINE_PATH explicitly as an absolute path so the worktree
# build mirrors the main-repo build.

set -euo pipefail
NATIVE_DIR="$(cd "$(dirname "$0")/../../native" && pwd)"
REPO_ROOT="$(cd "$NATIVE_DIR/.." && pwd)"
DEPLOY_DIR="$NATIVE_DIR/web/build"

# Which builds to produce. Default: both (so ?debug=true switching works out of
# the box). --release / --debug narrow it for a faster single-build loop.
BUILD_DEBUG=1
BUILD_RELEASE=1
CLOSURE=OFF
OPT_LEVEL=""
FORCE_RECONFIGURE=0
while [ $# -gt 0 ]; do
    case "$1" in
        --release)     BUILD_DEBUG=0; BUILD_RELEASE=1 ;;
        --debug)       BUILD_DEBUG=1; BUILD_RELEASE=0 ;;
        --both|--all)  BUILD_DEBUG=1; BUILD_RELEASE=1 ;;
        --closure)     CLOSURE=ON ;;
        --reconfigure) FORCE_RECONFIGURE=1 ;;
        --opt)         shift; OPT_LEVEL="$1" ;;
        --opt=*)       OPT_LEVEL="${1#--opt=}" ;;
        -h|--help)
            sed -n '2,33p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
    shift
done

# Ensure the Emscripten toolchain is on PATH. The build needs emcc/emcmake; if
# they aren't already active, source emsdk_env.sh from $EMSDK (set by a prior
# activation) or the default ~/emsdk checkout, so the script runs from a fresh
# shell without a manual `source ~/emsdk/emsdk_env.sh` first.
if ! command -v emcc >/dev/null 2>&1; then
    EMSDK_DIR="${EMSDK:-$HOME/emsdk}"
    if [ -f "$EMSDK_DIR/emsdk_env.sh" ]; then
        echo "==> Activating emsdk from $EMSDK_DIR"
        set +u                       # emsdk_env.sh references unset vars under -u
        # shellcheck disable=SC1091
        source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1 || true
        set -u
    fi
fi
if ! command -v emcc >/dev/null 2>&1; then
    echo "ERROR: emcc not found. Install emsdk or point \$EMSDK at your emsdk dir." >&2
    echo "       (looked for emsdk_env.sh under \${EMSDK:-\$HOME/emsdk})" >&2
    exit 1
fi

# Try ../../milo-native-engine relative to native/; that path is correct in the
# main repo but breaks in .claude/worktrees/<name>/. Probe both, fall back
# to the canonical milohax checkout if neither resolves.
ENGINE_PATH_CANDIDATES=(
    "$NATIVE_DIR/../../milo-native-engine"
    "$REPO_ROOT/../milo-native-engine"
    "/home/free/code/milohax/milo-native-engine"
)
MILO_ENGINE_PATH=""
for candidate in "${ENGINE_PATH_CANDIDATES[@]}"; do
    if [ -d "$candidate/.git" ]; then
        MILO_ENGINE_PATH="$(cd "$candidate" && pwd)"
        break
    fi
done
if [ -z "$MILO_ENGINE_PATH" ]; then
    echo "ERROR: milo-native-engine not found in any candidate path." >&2
    printf '  - %s\n' "${ENGINE_PATH_CANDIDATES[@]}" >&2
    exit 1
fi

mkdir -p "$DEPLOY_DIR"

# Remove stale flat artifacts from the pre-dual-build layout. The deploy now
# lives under release/ and debug/; orphaned root-level rb3-web.* would just
# confuse (and the old /api/version fallback path). Harmless if already gone.
rm -f "$DEPLOY_DIR"/rb3-web.js "$DEPLOY_DIR"/rb3-web.js.br "$DEPLOY_DIR"/rb3-web.js.gz \
      "$DEPLOY_DIR"/rb3-web.wasm "$DEPLOY_DIR"/rb3-web.wasm.br "$DEPLOY_DIR"/rb3-web.wasm.gz

# Build one configuration (mode = release | debug) into its own emcc build dir
# and deploy it to web/build/<mode>/. Each build dir is pinned to its mode, so we
# only reconfigure when the cache is missing, the mode flag drifted (e.g. an old
# single-dir build), or --reconfigure was passed.
build_one() {
    local mode="$1"
    local release_flag bdir ddir
    if [ "$mode" = "release" ]; then
        release_flag=ON
        bdir="$NATIVE_DIR/build-web-release"
    else
        release_flag=OFF
        bdir="$NATIVE_DIR/build-web"
    fi
    ddir="$DEPLOY_DIR/$mode"
    mkdir -p "$ddir"

    local cmake_args=(
        -DMILO_ENGINE_PATH="$MILO_ENGINE_PATH"
        -DRB3_WEB_RELEASE="$release_flag"
        -DRB3_WEB_CLOSURE="$CLOSURE"
    )
    if [ -n "$OPT_LEVEL" ]; then
        cmake_args+=(-DRB3_WEB_OPT_LEVEL="$OPT_LEVEL")
    fi

    local need_configure=0
    if [ ! -f "$bdir/CMakeCache.txt" ] || [ "$FORCE_RECONFIGURE" = "1" ]; then
        need_configure=1
    else
        local cached_release cached_closure cached_opt
        cached_release="$(grep -E '^RB3_WEB_RELEASE:BOOL=' "$bdir/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || echo OFF)"
        cached_closure="$(grep -E '^RB3_WEB_CLOSURE:BOOL=' "$bdir/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || echo OFF)"
        cached_opt="$(grep -E '^RB3_WEB_OPT_LEVEL:STRING=' "$bdir/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || echo O0)"
        if [ "$cached_release" != "$release_flag" ] || [ "$cached_closure" != "$CLOSURE" ] \
           || { [ -n "$OPT_LEVEL" ] && [ "$cached_opt" != "$OPT_LEVEL" ]; }; then
            echo "==> [$mode] build flags changed; reconfiguring + relinking"
            rm -f "$bdir/rb3-web.wasm" "$bdir/rb3-web.js"
            need_configure=1
        fi
    fi

    if [ "$need_configure" = "1" ]; then
        emcmake cmake -S "$NATIVE_DIR" -B "$bdir" "${cmake_args[@]}"
    fi

    echo "==> [$mode] building rb3-web"
    cmake --build "$bdir" -- -j"$(nproc)" rb3-web

    cp "$bdir/rb3-web.js" "$bdir/rb3-web.wasm" "$ddir/"

    # Compression. Release gets brotli q11 (best wire size, ~10x slower at build)
    # + gzip fallback; debug gets gzip only (brotli q11 on the big -g2 wasm is the
    # multi-minute step we skip for the fast-iteration build).
    echo "==> [$mode] pre-compressing artifacts"
    local f src
    for f in rb3-web.wasm rb3-web.js; do
        src="$ddir/$f"
        if [ "$mode" = "release" ] && command -v brotli >/dev/null 2>&1; then
            brotli -q 11 -f -k -o "$src.br" "$src"
        elif [ "$mode" = "release" ]; then
            echo "  brotli not installed — skipping .br (gzip fallback only)"
        fi
        gzip -9 -k -f "$src"  # produces $src.gz, keeps the original
    done
}

[ "$BUILD_RELEASE" = "1" ] && build_one release
[ "$BUILD_DEBUG" = "1" ]   && build_one debug

# Shared, served from the root. index.html picks release/ or debug/ at runtime.
cp "$NATIVE_DIR/web/index.html" "$DEPLOY_DIR/"

echo ""
echo "Deployed to $DEPLOY_DIR"
if [ "$BUILD_RELEASE" = "1" ]; then
    ( cd "$DEPLOY_DIR" && ls -lh release/rb3-web.{js,wasm}{,.br,.gz} 2>/dev/null | awk '{printf "  %-30s %s\n", $NF, $5}' )
fi
if [ "$BUILD_DEBUG" = "1" ]; then
    ( cd "$DEPLOY_DIR" && ls -lh debug/rb3-web.{js,wasm}{,.gz} 2>/dev/null | awk '{printf "  %-30s %s\n", $NF, $5}' )
fi
echo ""
echo "  Serve:  python3 native/web/server.py"
echo "  Play:   http://localhost:8421/            (release, cached — fast reloads)"
echo "  Debug:  http://localhost:8421/?debug=true (debug, no-store)"
