#!/usr/bin/env bash
# Offline WGSL validation for the externalized engine shader modules (W1.1.S2).
#
# The .inc shader files under src/gfx/ carry the C++ raw-string wrapper
# (first line `R"WGSL(`, last line `)WGSL"`) so they can be #include-d as string
# literals. This script strips that wrapper into a temp .wgsl and validates the
# pure shader body with naga or tint, whichever is installed.
#
# On this host NEITHER naga NOR tint is installed (MEASURED 2026-07-05), so the
# script skips-green (exit 0) — the always-on wgpu gtest (test_wgsl_validation.cpp,
# ctest -R WgslValidation) is the real gate. On a CI box that has a CLI, this adds
# a fast, GPU-free syntax check that also covers standard_wgsl.inc.
set -euo pipefail

ENGINE_DIR="${MILO_ENGINE_DIR:-/home/free/code/milohax/milo-native-engine}"

# --- Pick a validator, or skip gracefully -----------------------------------
VALIDATOR=""
if command -v naga >/dev/null 2>&1; then
    VALIDATOR="naga"
elif command -v tint >/dev/null 2>&1; then
    VALIDATOR="tint"
fi

if [ -z "$VALIDATOR" ]; then
    echo "[validate_wgsl] naga/tint not installed — skipping offline validation (gtest covers it)"
    exit 0
fi

echo "[validate_wgsl] using validator: $VALIDATOR"

# --- Collect the wrapper-bearing shader files -------------------------------
shopt -s nullglob
FILES=(
    "$ENGINE_DIR"/src/gfx/*.inc
    "$ENGINE_DIR"/src/gfx/Shaders/*.wgsl.inc
)
shopt -u nullglob

if [ ${#FILES[@]} -eq 0 ]; then
    echo "[validate_wgsl] no shader .inc files found under $ENGINE_DIR/src/gfx — nothing to validate"
    exit 0
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

FAIL=0
for f in "${FILES[@]}"; do
    base="$(basename "$f")"
    out="$TMPDIR/${base%.inc}.wgsl"
    # Strip a lone leading `R"WGSL(` and a lone trailing `)WGSL"` (the C++ wrapper),
    # leaving the pure WGSL body.
    sed '1{/^R"WGSL($/d}; ${/^)WGSL"$/d}' "$f" > "$out"

    if [ "$VALIDATOR" = "naga" ]; then
        if naga "$out" >/dev/null 2>"$TMPDIR/err.txt"; then
            echo "[validate_wgsl] OK   $base"
        else
            echo "[validate_wgsl] FAIL $base"
            cat "$TMPDIR/err.txt"
            FAIL=1
        fi
    else  # tint
        if tint --validate "$out" >/dev/null 2>"$TMPDIR/err.txt"; then
            echo "[validate_wgsl] OK   $base"
        else
            echo "[validate_wgsl] FAIL $base"
            cat "$TMPDIR/err.txt"
            FAIL=1
        fi
    fi
done

if [ "$FAIL" -ne 0 ]; then
    echo "[validate_wgsl] one or more shaders failed validation"
    exit 1
fi

echo "[validate_wgsl] all shaders valid"
exit 0
