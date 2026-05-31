#!/usr/bin/env bash
# Build the RB3 Emscripten/WebAssembly target and deploy artifacts next to
# native/web/index.html so server.py can serve them.
#
# Emscripten: auto-activates from $EMSDK or ~/emsdk if emcc isn't on PATH yet.
#
# Usage:
#   scripts/web/build.sh                          # dev build (-O0 -g2, debuggable)
#   scripts/web/build.sh --release                # W4a size-opt build (-O0 -g0)
#   scripts/web/build.sh --release --opt Os       # opt-in to higher -O (BROKEN as of W4a;
#                                                 # matched-fork throws during App ctor at -O>0)
#   scripts/web/build.sh --release --closure      # opt-in to --closure 1 (BROKEN as of W4a;
#                                                 # closure renames break rb3_pre.js stub patch)
#
# All --release builds pre-compress .wasm + .js with brotli (-q 11) and gzip
# (-9) so server.py can negotiate Content-Encoding without runtime CPU cost.
#
# Output:
#   native/build-web/rb3-web.{js,wasm}  — emcc build outputs
#   native/web/build/{rb3-web.js, rb3-web.wasm, rb3-web.wasm.br, rb3-web.wasm.gz,
#                     rb3-web.js.br,  rb3-web.js.gz,
#                     index.html, audio-worklet.js}
#
# The audio-worklet.js POST_BUILD copy is handled by
# milo_engine_apply_web_target_options in native/CMakeLists.txt.
#
# Worktree note: `${CMAKE_SOURCE_DIR}/../../milo-native-engine` (in
# native/CMakeLists.txt) DOES NOT resolve from .claude/worktrees/<name>/.
# We pass MILO_ENGINE_PATH explicitly as an absolute path so the worktree
# build mirrors the main-repo build.

set -euo pipefail
NATIVE_DIR="$(cd "$(dirname "$0")/../../native" && pwd)"
REPO_ROOT="$(cd "$NATIVE_DIR/.." && pwd)"
BUILD_DIR="$NATIVE_DIR/build-web"
DEPLOY_DIR="$NATIVE_DIR/web/build"

# Default flags; --release / --closure flip them on.
RELEASE=OFF
CLOSURE=OFF
OPT_LEVEL=""
FORCE_RECONFIGURE=0
while [ $# -gt 0 ]; do
    case "$1" in
        --release)  RELEASE=ON ;;
        --closure)  CLOSURE=ON ;;
        --reconfigure) FORCE_RECONFIGURE=1 ;;
        --opt) shift; OPT_LEVEL="$1" ;;
        --opt=*) OPT_LEVEL="${1#--opt=}" ;;
        -h|--help)
            sed -n '2,18p' "$0"
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

CMAKE_ARGS=(
    -DMILO_ENGINE_PATH="$MILO_ENGINE_PATH"
    -DRB3_WEB_RELEASE="$RELEASE"
    -DRB3_WEB_CLOSURE="$CLOSURE"
)
if [ -n "$OPT_LEVEL" ]; then
    CMAKE_ARGS+=(-DRB3_WEB_OPT_LEVEL="$OPT_LEVEL")
fi

# Reconfigure if release-mode flag changed since last configure. CMake won't
# pick up new -D values otherwise (option() honours the cache).
NEEDS_CONFIGURE=0
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    NEEDS_CONFIGURE=1
elif [ "$FORCE_RECONFIGURE" = "1" ]; then
    NEEDS_CONFIGURE=1
else
    CACHED_RELEASE="$(grep -E '^RB3_WEB_RELEASE:BOOL=' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || echo OFF)"
    CACHED_CLOSURE="$(grep -E '^RB3_WEB_CLOSURE:BOOL=' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || echo OFF)"
    CACHED_OPT="$(grep -E '^RB3_WEB_OPT_LEVEL:STRING=' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || echo Os)"
    if [ "$CACHED_RELEASE" != "$RELEASE" ] || [ "$CACHED_CLOSURE" != "$CLOSURE" ] || ( [ -n "$OPT_LEVEL" ] && [ "$CACHED_OPT" != "$OPT_LEVEL" ] ); then
        echo "==> RB3_WEB_RELEASE/CLOSURE/OPT_LEVEL changed; reconfiguring + relinking"
        # Force re-link by removing the wasm so cmake rebuilds it with new flags.
        rm -f "$BUILD_DIR/rb3-web.wasm" "$BUILD_DIR/rb3-web.js"
        NEEDS_CONFIGURE=1
    fi
fi

if [ "$NEEDS_CONFIGURE" = "1" ]; then
    emcmake cmake -S "$NATIVE_DIR" -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"
fi

cmake --build "$BUILD_DIR" -- -j"$(nproc)" rb3-web

cp "$BUILD_DIR/rb3-web.js" "$BUILD_DIR/rb3-web.wasm" "$DEPLOY_DIR/"
cp "$NATIVE_DIR/web/index.html" "$DEPLOY_DIR/"

# W4a — pre-generate gzip + brotli for the wasm + js so server.py can serve
# Content-Encoding: br / gzip without re-compressing on every request. The
# .br is sized with `-q 11` (max compression; ~10x slower at build time but
# halves the wire transfer vs gzip for wasm). The .gz at `-9` is the fallback
# for browsers without brotli support.
echo "==> Pre-compressing artifacts (brotli q11 + gzip -9)"
for f in rb3-web.wasm rb3-web.js; do
    src="$DEPLOY_DIR/$f"
    if command -v brotli >/dev/null 2>&1; then
        brotli -q 11 -f -k -o "$src.br" "$src"
    else
        echo "  brotli not installed — skipping .br"
    fi
    gzip -9 -k -f "$src"  # produces $src.gz, keeps the original
done

echo "Deployed to $DEPLOY_DIR"
ls -lh "$DEPLOY_DIR"/rb3-web.{js,wasm}{,.br,.gz} 2>/dev/null | awk '{printf "  %-32s %s\n", $9, $5}'
