#!/usr/bin/env bash
# build-macos.sh — turnkey macOS (Apple Silicon / Intel) build for rb3-native.
#
# THIS SCRIPT IS RUN ON A MAC. It cannot be verified on the Linux dev host; the
# CMake wiring it drives was regression-tested on Linux (rb3-native builds +
# runs), and mirrors the proven dc3-native macOS recipe. See
# docs/native/MACOS_BUILD.md for the full writeup and troubleshooting.
#
# What it does:
#   1. Sanity-checks prerequisites (Xcode CLT, cmake, ninja, brew deps, Dawn).
#   2. Configures native/ with the macOS Metal Dawn.
#   3. Builds the rb3-native target.
#   4. Prints how to run it (headless HTTP debug API + windowed).
#
# Usage:
#   scripts/native/build-macos.sh                 # configure + build rb3-native
#   DAWN_DIR=/path/to/dawn scripts/native/build-macos.sh
#   BUILD_DIR=build-macos TARGET=rb3-native scripts/native/build-macos.sh
#   scripts/native/build-macos.sh --reconfigure   # wipe the build dir first
set -euo pipefail

# --- Resolve repo layout -----------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RB3_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
NATIVE_DIR="${RB3_ROOT}/native"
ENGINE_DIR="$(cd "${RB3_ROOT}/../milo-native-engine" 2>/dev/null && pwd || true)"

BUILD_DIR="${BUILD_DIR:-build-macos}"
TARGET="${TARGET:-rb3-native}"
# Default Dawn location matches the docs (~/dc3-deps/dawn). Override with DAWN_DIR.
DAWN_PREFIX="${DAWN_DIR:-$HOME/dc3-deps/dawn}"
DAWN_CMAKE_DIR="${DAWN_PREFIX}/lib/cmake/Dawn"

RECONFIGURE=0
for arg in "$@"; do
    case "$arg" in
        --reconfigure) RECONFIGURE=1 ;;
        --help|-h)
            grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown arg: $arg (see --help)"; exit 2 ;;
    esac
done

# --- Platform guard ----------------------------------------------------------
if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "ERROR: this script is for macOS. On Linux use:"
    echo "  cd native && cmake -B build-native -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build-native --target rb3-native"
    exit 1
fi

echo "==> rb3-native macOS build"
echo "    repo:    ${RB3_ROOT}"
echo "    engine:  ${ENGINE_DIR:-<missing ../milo-native-engine>}"
echo "    build:   ${NATIVE_DIR}/${BUILD_DIR}"
echo "    Dawn:    ${DAWN_CMAKE_DIR}"
echo "    target:  ${TARGET}"

# --- Prereq checks -----------------------------------------------------------
fail=0
need() { command -v "$1" >/dev/null 2>&1 || { echo "MISSING: $1 ($2)"; fail=1; }; }

xcode-select -p >/dev/null 2>&1 || { echo "MISSING: Xcode Command Line Tools — run: xcode-select --install"; fail=1; }
need cmake  "brew install cmake"
need ninja  "brew install ninja"
need brew   "install Homebrew from https://brew.sh"

# Homebrew libs the native link needs (glfw, vorbis/ogg, zlib is in the SDK).
for pkg in glfw libvorbis libogg; do
    if ! brew list --versions "$pkg" >/dev/null 2>&1; then
        echo "MISSING brew pkg: $pkg — run: brew install glfw libvorbis libogg"
        fail=1
    fi
done

# Metal Dawn must exist (built separately — see MACOS_BUILD.md "Building Dawn").
if [[ ! -f "${DAWN_CMAKE_DIR}/DawnConfig.cmake" && ! -f "${DAWN_CMAKE_DIR}/dawn-config.cmake" ]]; then
    echo "MISSING: Metal-enabled Dawn at ${DAWN_CMAKE_DIR}"
    echo "  Build it once (see docs/native/MACOS_BUILD.md 'Building Dawn for macOS'),"
    echo "  or point DAWN_DIR at an existing install:  DAWN_DIR=/path/to/dawn $0"
    fail=1
fi

if [[ -z "${ENGINE_DIR}" || ! -f "${ENGINE_DIR}/CMakeLists.txt" ]]; then
    echo "MISSING: shared engine at ${RB3_ROOT}/../milo-native-engine"
    echo "  Clone milo-native-engine as a sibling of rb3/ (see CLAUDE.md 'Shared Engine')."
    fail=1
fi

[[ $fail -eq 0 ]] || { echo; echo "Fix the above and re-run."; exit 1; }

# --- Configure ---------------------------------------------------------------
cd "${NATIVE_DIR}"
if [[ $RECONFIGURE -eq 1 ]]; then
    echo "==> --reconfigure: removing ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

echo "==> configure"
cmake -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DDawn_DIR="${DAWN_CMAKE_DIR}"

# --- Build -------------------------------------------------------------------
echo "==> build ${TARGET}"
cmake --build "${BUILD_DIR}" --target "${TARGET}" -j"$(sysctl -n hw.ncpu)"

BIN="${NATIVE_DIR}/${BUILD_DIR}/${TARGET}"
echo
echo "==> DONE: ${BIN}"
echo
echo "Run it (needs extracted Xbox ARK assets — set RB3_DATA to their root):"
echo "  export RB3_DATA=/path/to/rb3/orig-assets/extracted"
echo "  # headless full-game boot + HTTP debug API on :8080 (see scripts/native/*.py):"
echo "  RB3_GAME=1 RB3_HTTP=1 \"${BIN}\""
echo "  #   curl localhost:8080/api/health ; curl -o shot.png localhost:8080/api/screenshot"
echo "  # or dump a .milo scene tree:"
echo "  \"${BIN}\" /abs/path/to/scene.milo_xbox"
