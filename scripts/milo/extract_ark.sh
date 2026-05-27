#!/usr/bin/env bash
# Extract RB3 ark archive to a directory using arkhelper.
#
# Modeled on dc3-decomp/scripts/milo/extract_ark.sh, adapted for RB3.
#
# Source selection (in order, first that exists wins unless overridden):
#   1. RB3 Wii  : orig/SZBE69_B8/files/band_r_wii.hdr  (+ band_r_wii_*.ark)
#   2. RB3 Wii  : orig/SZBE69_B8/files/band_r_wii.sel  (only if it is a real
#                 ark2 header — see note below)
#   3. RB3 Xbox 360: /srv/torrents/games/arbys/rb3/gen/main_xbox.hdr
#
# IMPORTANT — Wii .sel/.map notes:
#   The .sel file shipped in orig/SZBE69_B8/files/band_r_wii.sel in this
#   repo is *not* an ark header — it is a 12 KB binary that starts with
#   zeros and arkhelper refuses with "Ark version of '0' is unsupported".
#   The accompanying band_r_wii.map is the *linker* link-map of __start
#   ("Link map of __start"), not the ark file map. They are decomp-side
#   build artifacts checked into orig/ alongside main.dol.
#
#   The actual RB3 Wii .ark data parts (band_r_wii_0.ark .. _N.ark) are
#   NOT present anywhere under /home/free/code/milohax. If they become
#   available, drop them next to band_r_wii.sel and re-run with --sel
#   pointed at a real ark2 .hdr/.sel.
#
#   In the meantime this script falls back to the RB3 *Xbox 360* ARK,
#   which contains the same songs.dta, config/, world/, ui/ DTA payload
#   (the .dta content is platform-agnostic; only .milo_xbox vs
#   .milo_wii binary assets differ).
#
# Usage: extract_ark.sh [output_dir] [options]
#   output_dir       Where to extract (default: orig-assets/extracted)
#   --hdr PATH       Path to .hdr file (overrides auto-detect)
#   --sel PATH       Path to .sel file (Wii ark2; overrides auto-detect)
#   --dta            Convert .dtb scripts to .dta text
#   --inflate        Decompress .milo_xbox / .milo_wii archives
#   --all            Extract everything (raw, no conversion)
#   --xbox-fallback  Use /srv RB3 360 ARK explicitly
#
# Defaults when no flags are passed: --dta --inflate (readable scripts
# + inflated milos, no raw original-byte copies).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Locate arkhelper
ARKHELPER=""
for candidate in \
    "$REPO_ROOT/../milo-executable-library/dance-central-3-deluxe/dependencies/linux/arkhelper" \
    "$(command -v arkhelper 2>/dev/null || true)"; do
    if [[ -x "$candidate" ]]; then
        ARKHELPER="$candidate"
        break
    fi
done

if [[ -z "$ARKHELPER" ]]; then
    echo "ERROR: arkhelper not found." >&2
    echo "Expected at: ../milo-executable-library/dance-central-3-deluxe/dependencies/linux/arkhelper" >&2
    exit 1
fi

# Candidate sources
WII_HDR="$REPO_ROOT/orig/SZBE69_B8/files/band_r_wii.hdr"
WII_SEL="$REPO_ROOT/orig/SZBE69_B8/files/band_r_wii.sel"
XBOX_HDR="/srv/torrents/games/arbys/rb3/gen/main_xbox.hdr"

# Defaults
OUTPUT_DIR=""
ARK_PATH=""
CONVERT_SCRIPTS=""
INFLATE_MILOS=""
EXTRACT_ALL=""
FORCE_XBOX=0
USER_PICKED_FLAGS=0

# Parse args
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dta)            CONVERT_SCRIPTS="-s"; USER_PICKED_FLAGS=1; shift ;;
        --inflate)        INFLATE_MILOS="-m";  USER_PICKED_FLAGS=1; shift ;;
        --all)            EXTRACT_ALL="-a";     USER_PICKED_FLAGS=1; shift ;;
        --hdr)            ARK_PATH="$2"; shift 2 ;;
        --sel)            ARK_PATH="$2"; shift 2 ;;
        --xbox-fallback)  FORCE_XBOX=1; shift ;;
        -h|--help)
            sed -n '2,/^set -euo/{ /^set -euo/!p; }' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *)
            if [[ -z "$OUTPUT_DIR" ]]; then OUTPUT_DIR="$1"
            else echo "ERROR: Unknown argument: $1" >&2; exit 1; fi
            shift ;;
    esac
done

# Default output directory
if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="$REPO_ROOT/orig-assets/extracted"
fi

# Default flag set: --dta --inflate (readable result, no raw dump)
if [[ "$USER_PICKED_FLAGS" == 0 ]]; then
    CONVERT_SCRIPTS="-s"
    INFLATE_MILOS="-m"
fi

# Probe whether a file looks like a real ark2 hdr/sel (non-zero magic).
looks_like_ark_header() {
    local f="$1"
    [[ -f "$f" ]] || return 1
    # First 4 bytes must be non-zero (ark version).
    local v
    v=$(od -An -N4 -tx1 "$f" | tr -d ' \n')
    [[ "$v" != "00000000" ]]
}

# Auto-select source
SRC_LABEL=""
if [[ -z "$ARK_PATH" ]]; then
    if [[ "$FORCE_XBOX" == 1 ]]; then
        ARK_PATH="$XBOX_HDR"; SRC_LABEL="RB3 Xbox 360 (forced via --xbox-fallback)"
    elif looks_like_ark_header "$WII_HDR"; then
        ARK_PATH="$WII_HDR"; SRC_LABEL="RB3 Wii (.hdr)"
    elif looks_like_ark_header "$WII_SEL"; then
        ARK_PATH="$WII_SEL"; SRC_LABEL="RB3 Wii (.sel)"
    elif [[ -f "$XBOX_HDR" ]]; then
        ARK_PATH="$XBOX_HDR"
        SRC_LABEL="RB3 Xbox 360 (fallback — Wii .ark data parts absent)"
        echo "NOTE: RB3 Wii ark not found / not usable; falling back to 360 ARK." >&2
        echo "      DTA payload (songs/, config/, world/, ui/) is platform-agnostic." >&2
    else
        echo "ERROR: No RB3 ark source available." >&2
        echo "  Tried Wii .hdr: $WII_HDR" >&2
        echo "  Tried Wii .sel: $WII_SEL (zero-magic — not a real ark2 header)" >&2
        echo "  Tried 360 .hdr: $XBOX_HDR" >&2
        exit 1
    fi
else
    SRC_LABEL="user-supplied ($ARK_PATH)"
fi

if [[ ! -f "$ARK_PATH" ]]; then
    echo "ERROR: ark header file not found: $ARK_PATH" >&2
    exit 1
fi

# Build arkhelper command
CMD=("$ARKHELPER" ark2dir)
[[ -n "$CONVERT_SCRIPTS" ]] && CMD+=("$CONVERT_SCRIPTS")
[[ -n "$INFLATE_MILOS"  ]] && CMD+=("$INFLATE_MILOS")
[[ -n "$EXTRACT_ALL"    ]] && CMD+=("$EXTRACT_ALL")
CMD+=("$ARK_PATH" "$OUTPUT_DIR")

echo "Extracting RB3 ark..."
echo "  Source: $SRC_LABEL"
echo "  Header: $ARK_PATH"
echo "  Output: $OUTPUT_DIR"
echo "  Flags : ${CONVERT_SCRIPTS:-(none)} ${INFLATE_MILOS:-(none)} ${EXTRACT_ALL:-(none)}"
echo "  Cmd   : ${CMD[*]}"
echo ""

mkdir -p "$OUTPUT_DIR"
"${CMD[@]}"

echo ""
echo "Done. Extracted to: $OUTPUT_DIR"

# Quick summary
MILO_X=$(find "$OUTPUT_DIR" -name "*.milo_xbox" 2>/dev/null | wc -l)
MILO_W=$(find "$OUTPUT_DIR" -name "*.milo_wii"  2>/dev/null | wc -l)
DTA=$(find "$OUTPUT_DIR" -name "*.dta" 2>/dev/null | wc -l)
DTB=$(find "$OUTPUT_DIR" -name "*.dtb" 2>/dev/null | wc -l)
TOT=$(find "$OUTPUT_DIR" -type f 2>/dev/null | wc -l)
echo "  total files       : $TOT"
echo "  .milo_xbox files  : $MILO_X"
echo "  .milo_wii  files  : $MILO_W"
echo "  .dta files        : $DTA"
echo "  .dtb files        : $DTB"
if [[ -f "$OUTPUT_DIR/songs/songs.dta" ]]; then
    echo "  songs/songs.dta   : present ($(wc -l <"$OUTPUT_DIR/songs/songs.dta") lines)"
else
    echo "  songs/songs.dta   : MISSING"
fi
