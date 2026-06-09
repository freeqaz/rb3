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
# Usage (the pyghidra-mcp service may be running — this build touches no project):
#   bash tools/patchdiff/build_and_install.sh
# JAVA_HOME is auto-detected (a JDK with javac); override only if needed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-/opt/ghidra}"
# Building needs a JDK *with javac*. A JRE-only install (e.g. an Arch
# 'java-21-openjdk' headless JRE) fails gradle with "Toolchain ... does not
# provide the required capabilities: [JAVA_COMPILER]". Ghidra 12.1 runs on JDK
# 21+; compiling with a newer JDK that emits compatible bytecode is fine (Ghidra
# itself launches under whatever JDK its launch.sh selects — JDK 26 on this host).
# So: honor a caller-supplied JAVA_HOME only if it actually has javac; otherwise
# auto-detect the first JDK that does.
if [[ -z "${JAVA_HOME:-}" || ! -x "${JAVA_HOME}/bin/javac" ]]; then
    for cand in /usr/lib/jvm/java-26-openjdk /usr/lib/jvm/java-21-openjdk \
                /usr/lib/jvm/java-17-openjdk /usr/lib/jvm/default; do
        if [[ -x "$cand/bin/javac" ]]; then JAVA_HOME="$cand"; break; fi
    done
fi
if [[ ! -x "${JAVA_HOME:-}/bin/javac" ]]; then
    echo "[patchdiff] ERROR: no JDK with javac found (JAVA_HOME=$JAVA_HOME)." >&2
    echo "            Install a JDK (not a JRE) and/or set JAVA_HOME to it." >&2
    exit 1
fi

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
# The Color* correlator path is GUI-only (ColorizingService). It spans three name
# shapes: Color*.java (ColorProgramCorrelator, ColorBasicBlockMnemonic...Factory),
# Abstract*Color*.java (AbstractColorProgramCorrelatorFactory, AbstractFunctionColorer),
# and *Colorer.java (FunctionColorer interface, BasicBlockMnemonicFunctionColorer) —
# the last group extends the now-deleted AbstractFunctionColorer, so it must go too or
# compileJava fails with "cannot find symbol: class AbstractFunctionColorer".
echo "[patchdiff] Removing GUI-only Color* sources..."
rm -f "$DST_SRC"/Color*.java "$DST_SRC"/Abstract*Color*.java "$DST_SRC"/*Colorer.java
echo "  removed Color*.java Abstract*Color*.java *Colorer.java"

# Drop the bundled help module. Upstream's src/main/help/.../help.html references the
# old 'help/shared/DefaultStyle.css' stylesheet, which Ghidra 12.x renamed to
# Frontpage.css — so :buildModuleHelp's validator aborts with "Incorrect stylesheet
# defined". This is a headless correlator extension; its help is cosmetic. With no
# help dir, :buildModuleHelp becomes NO-SOURCE and the extension builds clean.
if [[ -d "$PROJ/src/main/help" ]]; then
    rm -rf "$PROJ/src/main/help"
    echo "  removed src/main/help (stale DefaultStyle.css ref; not needed headless)"
fi

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

# 5. Install. Ghidra discovers extensions in BOTH the system dir
# ($GHIDRA_INSTALL_DIR/Ghidra/Extensions, usually root-owned) and the per-user
# settings dir (~/.config/ghidra/ghidra_<ver>/Extensions, where GhidraXenon/BinExport
# already live). Prefer whichever is writable without root: try the system dir, else
# fall back to the user settings dir (creating it from the install version if needed).
EXT_BASE=""
if [[ -w "$GHIDRA_INSTALL_DIR/Ghidra/Extensions" ]]; then
    EXT_BASE="$GHIDRA_INSTALL_DIR/Ghidra/Extensions"
else
    # Derive the user settings dir from the install's OWN version — NOT a glob over
    # ~/.config/ghidra/ghidra_*, which can match a stale older settings dir (e.g.
    # ghidra_12.0.1_DEV) that the running 12.1 never loads. The settings dir name is
    # exactly ghidra_<application.version>_<application.release.name>.
    APP_PROPS="$GHIDRA_INSTALL_DIR/Ghidra/application.properties"
    VER="$(sed -n 's/^application.version=//p' "$APP_PROPS" | head -1)"
    REL="$(sed -n 's/^application.release.name=//p' "$APP_PROPS" | head -1)"
    if [[ -z "$VER" ]]; then
        echo "[patchdiff] ERROR: could not read application.version from $APP_PROPS" >&2
        exit 1
    fi
    EXT_BASE="$HOME/.config/ghidra/ghidra_${VER}_${REL:-DEV}/Extensions"
    mkdir -p "$EXT_BASE"
    echo "[patchdiff] System Extensions dir not writable; using per-user dir (ghidra_${VER}_${REL:-DEV})."
fi
INSTALL_DIR="$EXT_BASE/PatchDiffCorrelator"
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
