#!/usr/bin/env bash
# Build the RB3 Emscripten/WebAssembly target and deploy artifacts next to
# native/web/index.html so server.py can serve them.
#
# Requires: emsdk activated (`source ~/emsdk/emsdk_env.sh`)
#
# Output:
#   native/build-web/rb3-web.{js,wasm}  — emcc build outputs
#   native/web/build/{rb3-web.js, rb3-web.wasm, index.html, audio-worklet.js}
#
# The audio-worklet.js POST_BUILD copy is handled by
# milo_engine_apply_web_target_options in native/CMakeLists.txt.

set -euo pipefail
NATIVE_DIR="$(cd "$(dirname "$0")/../../native" && pwd)"
BUILD_DIR="$NATIVE_DIR/build-web"
DEPLOY_DIR="$NATIVE_DIR/web/build"

mkdir -p "$DEPLOY_DIR"

if [ ! -f "$BUILD_DIR/build.ninja" ] && [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    emcmake cmake -S "$NATIVE_DIR" -B "$BUILD_DIR"
fi

cmake --build "$BUILD_DIR" -- -j"$(nproc)" rb3-web

cp "$BUILD_DIR/rb3-web.js" "$BUILD_DIR/rb3-web.wasm" "$DEPLOY_DIR/"
cp "$NATIVE_DIR/web/index.html" "$DEPLOY_DIR/"

echo "Deployed to $DEPLOY_DIR"
