#!/usr/bin/env bash
# Install the Gekko/Broadway paired-single SLEIGH support from the
# /home/free/code/milohax/ghidra fork into /opt/ghidra.
#
# Patched files (all written to /opt/ghidra/Ghidra/Processors/PowerPC/data/languages/):
#   - ppc_common.sinc     (+ 8 lines: psq_* bit-field definitions)
#   - ppc.ldefs           (+ <language> entry for PowerPC:BE:32:Gekko_Broadway)
#   - gekko.sinc          (new)
#   - ppc_32_gekko_be.slaspec (new)
#   - ppc_32_gekko_be.sla (new, pre-compiled)
#
# Existing files are backed up to <file>.bak-pre-gekko on the first run only.
# Requires sudo.

set -euo pipefail

FORK=/home/free/code/milohax/ghidra
DEST=/opt/ghidra/Ghidra/Processors/PowerPC/data/languages

if [[ "$EUID" -ne 0 ]]; then
  echo "This script must be run as root (sudo $0)" >&2
  exit 1
fi

for f in ppc_common.sinc ppc.ldefs; do
  if [[ ! -f "$DEST/$f.bak-pre-gekko" ]]; then
    cp -p "$DEST/$f" "$DEST/$f.bak-pre-gekko"
    echo "Backed up $DEST/$f -> $DEST/$f.bak-pre-gekko"
  fi
done

for f in ppc_common.sinc ppc.ldefs gekko.sinc ppc_32_gekko_be.slaspec ppc_32_gekko_be.sla; do
  install -m 644 "$FORK/Ghidra/Processors/PowerPC/data/languages/$f" "$DEST/$f"
  echo "Installed $f"
done

# The new ppc_common.sinc only ADDS fields (psq_W, psq_I, ...) — every existing
# .sla in this directory remains functionally correct, but Ghidra would
# otherwise see the touched .sinc and trigger a slow on-the-fly recompile of
# each .sla on next launch. Bump every .sla's mtime past the new sources.
touch "$DEST"/*.sla

echo
echo "Done. Restart the pyghidra service to pick up the new variant:"
echo "  rm -rf /home/free/code/milohax/rb3/ghidra_projects/RB3   # force re-import under Gekko_Broadway"
echo "  tools/ghidra/pyghidra-service.sh restart"
