#!/usr/bin/env bash
# Build ghidra-patchdiff-correlator for Ghidra 12.1 and install into
# /opt/ghidra/Ghidra/Extensions/ (the system-wide extension dir checked by
# GhidraApplicationLayout.findExtensionInstallationDirectories() in non-dev mode).
#
# Prerequisites:
#   - git, gradle >= 8.5, Java 21 (for building)
#   - /opt/ghidra is Ghidra 12.1
#   - Write access to /opt/ghidra/Ghidra/Extensions/ (use sudo if needed)
#
# Upstream repo: https://github.com/threatrack/ghidra-patchdiff-correlator
# Latest upstream release: v0.5 targeting Ghidra 9.1.2 — NOT compatible with 12.x.
# This script applies the 4-file source patch required for the 12.x VT API change:
#   VTAbstractProgramCorrelatorFactory.doCreateCorrelator() dropped ServiceProvider.
#   VTAbstractProgramCorrelator constructor dropped ServiceProvider.
# The Color* correlators are intentionally left unpatched (they call ColorizingService
# which is a GUI-only service unavailable in headless mode; not needed for VT scripts).
#
# Usage (run with the pyghidra-mcp service already stopped):
#   bash tools/ghidra/patchdiff-correlator-src/build_and_install.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-/opt/ghidra}"
JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk}"

echo "[patchdiff] Ghidra install: $GHIDRA_INSTALL_DIR"
echo "[patchdiff] Java home:       $JAVA_HOME"

# 1. Clone upstream (shallow) into a temp dir
WORK_DIR="$(mktemp -d /tmp/patchdiff-build.XXXXXX)"
trap 'rm -rf "$WORK_DIR"' EXIT

echo "[patchdiff] Cloning upstream..."
git clone --depth=1 https://github.com/threatrack/ghidra-patchdiff-correlator.git "$WORK_DIR/upstream"

PROJ="$WORK_DIR/upstream/PatchDiffCorrelator"

# 2. Overlay patched files (4 files: the 3 factory .java + BulkProgramCorrelator.java)
PATCH_SRC="$SCRIPT_DIR/PatchDiffCorrelator/src/main/java/patchdiffcorrelator"
DST_SRC="$PROJ/src/main/java/patchdiffcorrelator"

echo "[patchdiff] Applying 12.x source patch..."
for f in \
    BulkProgramCorrelator.java \
    BulkBasicBlockMnemonicProgramCorrelatorFactory.java \
    BulkInstructionProgramCorrelatorFactory.java \
    BulkMnemonicProgramCorrelatorFactory.java; do
    cp "$PATCH_SRC/$f" "$DST_SRC/$f"
    echo "  patched $f"
done

# Remove Color* correlator sources — they call ColorizingService (GUI-only) and
# also have the same ServiceProvider API break. Not needed for headless VT use.
echo "[patchdiff] Removing GUI-only Color* sources..."
rm -f "$DST_SRC"/Color*.java "$DST_SRC"/Abstract*Color*.java
echo "  removed Color*.java AbstractColor*.java"

# 3. Build with gradle
echo "[patchdiff] Building..."
cd "$PROJ"
JAVA_HOME="$JAVA_HOME" gradle -PGHIDRA_INSTALL_DIR="$GHIDRA_INSTALL_DIR" buildExtension

# 4. Find the output zip
ZIP=$(find "$PROJ/dist" -name "*.zip" | head -1)
if [[ -z "$ZIP" ]]; then
    echo "[patchdiff] ERROR: no zip found in dist/" >&2
    exit 1
fi
echo "[patchdiff] Built: $ZIP"

# 5. Install: extract into /opt/ghidra/Ghidra/Extensions/
INSTALL_DIR="$GHIDRA_INSTALL_DIR/Ghidra/Extensions/PatchDiffCorrelator"
echo "[patchdiff] Installing to $INSTALL_DIR ..."
rm -rf "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR"
unzip -q "$ZIP" -d "$INSTALL_DIR"
# The zip contains one top-level dir; flatten if needed
INNER=$(find "$INSTALL_DIR" -mindepth 1 -maxdepth 1 -type d | head -1)
if [[ -n "$INNER" && "$INNER" != "$INSTALL_DIR" ]]; then
    mv "$INNER"/* "$INSTALL_DIR/"
    rmdir "$INNER"
fi

echo "[patchdiff] Installed. Contents:"
ls "$INSTALL_DIR/"
echo "[patchdiff] Done. The extension is available to all headless pyghidra invocations."
