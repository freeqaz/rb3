#!/usr/bin/env bash
# Setup script for Claude Code web sessions.
# Downloads the open-source tools from GitHub. Skips already-present tools.
# NOTE: the MWCC compilers (build/compilers/) must be provided separately;
# they are hosted on files.decomp.dev which may not be in the network allowlist.
# NOTE: orig/SZBE69_B8/ (game ROM) must be provided by the user.

set -e
TOOLS_DIR="build/tools"
mkdir -p "$TOOLS_DIR"

download_if_missing() {
    local name="$1"
    local path="$2"
    local tag="$3"
    if [ -e "$path" ]; then
        echo "[setup] $name: already present, skipping"
    else
        echo "[setup] Downloading $name..."
        python3 tools/download_tool.py "$name" "$path" --tag "$tag"
        echo "[setup] $name: done"
    fi
}

download_if_missing wibo        "$TOOLS_DIR/wibo"        "0.6.16"
download_if_missing dtk         "$TOOLS_DIR/dtk"         "v1.3.0"
download_if_missing objdiff-cli "$TOOLS_DIR/objdiff-cli" "v4.2.2"

mkdir -p build/binutils
download_if_missing binutils    "build/binutils"         "2.42-1"

echo "[setup] Tools ready (compilers + orig/ must be provided separately)"
