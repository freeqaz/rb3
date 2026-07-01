#!/usr/bin/env bash
# Install the Xenon (VMX128) SLEIGH support from the
# /home/free/code/milohax/ghidra fork into /opt/ghidra.
#
# WHY: install-gekko-sleigh.sh installs the fork's ppc.ldefs, which declares
# PowerPC:BE:64:Xenon (slafile ppc_64_xenon.sla) — but only the Gekko sleigh
# files were copied. The dangling entry makes SleighLanguageProvider drop the
# Xenon language at runtime ("Missing sleigh file: ppc_64_xenon.slaspec"),
# which breaks opening ANY program stored with that language (e.g. a .gzf
# exported from the rb3-xenon project for the Wii<->Xenon ghidriff run).
#
# Files written to /opt/ghidra/Ghidra/Processors/PowerPC/data/languages/:
#   - vmx128.sinc           (new, fork-only; @include'd by ppc_64_xenon.slaspec)
#   - ppc_64_xenon.slaspec  (new)
#   - ppc_64_xenon.sla      (new, pre-compiled)
# All other @includes (ppc_common.sinc fork version, ppc_isa/ppc_a2/quicciii/
# FPRC/altivec .sinc) are already present in /opt.
#
# Requires sudo. Companion to install-gekko-sleigh.sh (run that first).

set -euo pipefail

FORK=/home/free/code/milohax/ghidra
DEST=/opt/ghidra/Ghidra/Processors/PowerPC/data/languages

if [[ "$EUID" -ne 0 ]]; then
  echo "This script must be run as root (sudo $0)" >&2
  exit 1
fi

if [[ ! -f "$DEST/gekko.sinc" ]]; then
  echo "ERROR: gekko install not found in $DEST — run install-gekko-sleigh.sh first" >&2
  exit 1
fi

for f in vmx128.sinc ppc_64_xenon.slaspec ppc_64_xenon.sla; do
  install -m 644 "$FORK/Ghidra/Processors/PowerPC/data/languages/$f" "$DEST/$f"
  echo "Installed $f"
done

# Keep all .sla mtimes past the .sinc sources so Ghidra doesn't trigger a slow
# on-the-fly sleigh recompile on next launch (same trick as the gekko installer).
touch "$DEST"/*.sla

echo
echo "Done. PowerPC:BE:64:Xenon is loadable again in /opt (validates against the"
echo "pre-compiled .sla; no recompile needed). This unblocks:"
echo "  - opening Xenon-language programs/gzfs under /opt (run_ghidriff_xenon.sh)"
echo "  - the ghidriff run1 project's (mis-imported) Xenon-language programs, if ever needed"
