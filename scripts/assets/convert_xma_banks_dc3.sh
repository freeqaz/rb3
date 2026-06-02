#!/usr/bin/env bash
# convert_xma_banks_dc3.sh — offline XMA->PCM SFX conversion for the DC3
# native/web port. DC3 sibling of convert_xma_banks.sh.
#
# DC3 decodes kXMA at LOAD time on native (SampleData::Load, #ifdef HX_FFMPEG ->
# DecodeXMAToPCM) and WriteSidecar's the PCM; the ffmpeg-less WEB build READS the
# sidecars (#elif HX_NATIVE). The natural generator (DC3's own native asset-prep
# run) is blocked on an unrelated milo2gltf/Rnd_Wgpu build-env issue, so this
# uses the STANDALONE converter (rb3-xma-convert) in --dc3 mode instead: it parses
# DC3's on-disk SampleData layout (rev 0x10 with mCRC + per-blob mNumChannels) and
# writes sidecars with the SAME content-hash key + file format DC3's runtime
# (dc3_xma::TryLoad / PayloadKey) expects. No DC3 engine build required, so it does
# NOT collide with any in-flight DC3 render work. Key-match is verified
# byte-for-byte against dc3_xma::PayloadKey (see dc3-xma-sidecar-gen.md).
#
# kXMA is NOT only in sfx/gen — it is spread across many .milo_xbox containers
# (world, ui, flow, ...), so by default we scan the ENTIRE extracted asset tree
# recursively; banks with no kXMA blobs produce zero sidecars and are harmless.
#
# Usage:
#   scripts/assets/convert_xma_banks_dc3.sh [SCAN_DIR] [OUT_DIR]
#     SCAN_DIR default: <dc3>/orig-assets/extracted  (recursive *.milo_xbox)
#     OUT_DIR  default: <dc3>/orig-assets/derived/sfx_pcm  (sidecar output)
#
# Override the dc3 checkout location with DC3_DECOMP_PATH.
# Runtime: point DC3 at OUT_DIR via DC3_SFX_PCM_DIR (absolute), or place OUT_DIR's
# contents at <DC3 asset root>/sfx/gen/xma_pcm/ (the runtime's default).
#
# The RB3 + DC3 sidecar key + file format are IDENTICAL, so one shared OUT_DIR can
# serve both engines if you point both at it.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DC3_DECOMP_PATH="${DC3_DECOMP_PATH:-/home/free/code/milohax/dc3-decomp}"
SCAN_DIR="${1:-$DC3_DECOMP_PATH/orig-assets/extracted}"
OUT_DIR="${2:-$DC3_DECOMP_PATH/orig-assets/derived/sfx_pcm}"
BUILD_DIR="$REPO_ROOT/native/build-xma"
TOOL="$BUILD_DIR/rb3-xma-convert"
CXX="${CXX:-/usr/bin/clang++}"

if [[ ! -x "$TOOL" ]]; then
    echo ">> building rb3-xma-convert (standalone, libav only) ..."
    cmake -S "$REPO_ROOT/native/tools/xma_repack" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_CXX_COMPILER="$CXX" \
        -DDC3_DECOMP_PATH="$DC3_DECOMP_PATH" >/dev/null
    cmake --build "$BUILD_DIR" >/dev/null
fi

if [[ ! -d "$SCAN_DIR" ]]; then
    echo "ERROR: scan dir not found: $SCAN_DIR" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
mapfile -d '' banks < <(find -L "$SCAN_DIR" -name '*.milo_xbox' -type f -print0)
if [[ ${#banks[@]} -eq 0 ]]; then
    echo "ERROR: no .milo_xbox banks under $SCAN_DIR" >&2
    exit 1
fi

echo ">> [DC3] scanning ${#banks[@]} .milo_xbox banks under $SCAN_DIR"
echo ">> sidecars -> $OUT_DIR"
"$TOOL" --dc3 "$OUT_DIR" "${banks[@]}"
echo ">> done. To use at runtime: DC3_SFX_PCM_DIR=$OUT_DIR"
