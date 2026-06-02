#!/usr/bin/env bash
# convert_xma_banks.sh — offline XMA->PCM SFX conversion for the RB3 native/web
# port. Reproducible entry point the orchestrator can re-run to regenerate the
# converted-sidecar assets. Mirrors dc3/scripts/web/transcode_bik.sh (offline
# decode for the web bundle, which has no runtime ffmpeg).
#
# Builds the standalone converter (rb3-xma-convert; links libav + DC3's
# DecodeXMAToPCM, NOT the milo engine) if needed, then converts every kXMA
# SampleData blob in the RB3 SFX banks to a 16-bit-LE PCM sidecar in a DERIVED
# directory. ORIGINAL banks are never mutated.
#
# kXMA samples are NOT only in sfx/gen — they are spread across MANY .milo_xbox
# containers (UI endgame/tour/trainers, venue crowd loops under world/venue/*,
# closet, and world/vignette/* cutscenes). So by default we scan the ENTIRE
# extracted asset tree recursively for .milo_xbox banks; banks with no kXMA
# blobs produce zero sidecars and are harmless.
#
# Usage:
#   scripts/assets/convert_xma_banks.sh [SCAN_DIR] [OUT_DIR]
#     SCAN_DIR default: orig-assets/extracted  (recursively scanned for *.milo_xbox)
#     OUT_DIR  default: orig-assets/derived/sfx_pcm   (sidecar output)
#
# Runtime: point the SampleInst glue at OUT_DIR via RB3_SFX_PCM_DIR (absolute),
# or place OUT_DIR's contents at <RB3_DATA>/sfx/gen/xma_pcm/ (the glue's default).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCAN_DIR="${1:-$REPO_ROOT/orig-assets/extracted}"
OUT_DIR="${2:-$REPO_ROOT/orig-assets/derived/sfx_pcm}"
BUILD_DIR="$REPO_ROOT/native/build-xma"
TOOL="$BUILD_DIR/rb3-xma-convert"
CXX="${CXX:-/usr/bin/clang++}"

if [[ ! -x "$TOOL" ]]; then
    echo ">> building rb3-xma-convert (standalone, libav only) ..."
    cmake -S "$REPO_ROOT/native/tools/xma_repack" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_CXX_COMPILER="$CXX" >/dev/null
    cmake --build "$BUILD_DIR" >/dev/null
fi

if [[ ! -d "$SCAN_DIR" ]]; then
    echo "ERROR: scan dir not found: $SCAN_DIR" >&2
    exit 1
fi

# Recursively collect every .milo_xbox container. Banks with no kXMA blobs (drum
# kits, already-PCM SFX, geometry-only milos) produce zero sidecars and are
# harmless to pass through. Use a NUL-delimited find so paths with spaces work.
mkdir -p "$OUT_DIR"
mapfile -d '' banks < <(find -L "$SCAN_DIR" -name '*.milo_xbox' -type f -print0)
if [[ ${#banks[@]} -eq 0 ]]; then
    echo "ERROR: no .milo_xbox banks under $SCAN_DIR" >&2
    exit 1
fi

echo ">> scanning ${#banks[@]} .milo_xbox banks under $SCAN_DIR"
echo ">> sidecars -> $OUT_DIR"
# Convert in batches to keep the argv list sane (xargs splits as needed); the
# tool accumulates into one manifest if invoked once, so pass all at once (a few
# hundred paths is well under ARG_MAX).
"$TOOL" "$OUT_DIR" "${banks[@]}"
echo ">> done. To use at runtime: RB3_SFX_PCM_DIR=$OUT_DIR"
