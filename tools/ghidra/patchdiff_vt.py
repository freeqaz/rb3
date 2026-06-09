#!/usr/bin/env python3
"""Run ghidra-patchdiff-correlator Bulk correlators on the RB3 VT session.

These correlators produce PARTIAL similarity scores (0..1) for functions that
don't byte-match — covering the ~20% of Bank5/Bank8 functions whose bodies
diverged between the 2009 debug and 2010 release builds. They run AFTER the
exact correlators (SymbolName + ExactMatchInstructions) have already accepted
all byte-identical pairs, and with "Only match accepted matches" = True (the
default), so they only run on the already-accepted set and annotate divergence.

Correlator factory class names registered by the extension (patchdiffcorrelator pkg):
    "Bulk Basic Block Mnemonics Match"  -> BulkBasicBlockMnemonicProgramCorrelatorFactory
    "Bulk Mnemonics Match"              -> BulkMnemonicProgramCorrelatorFactory
    "Bulk Instructions Match"           -> BulkInstructionProgramCorrelatorFactory

Typical workflow (service must be stopped):
    tools/ghidra/pyghidra-service.sh stop
    cd ../pyghidra-mcp && uv run --python 3.10 --project . \\
        python ../rb3/tools/ghidra/version_track.py        # first: exact symbol+instr pass
    cd ../pyghidra-mcp && uv run --python 3.10 --project . \\
        python ../rb3/tools/ghidra/patchdiff_vt.py         # then: bulk fuzzy pass
    tools/ghidra/pyghidra-service.sh start

Options:
    --threshold 0.5   minimum similarity score to report (default 0.5)
    --dry             report counts only, apply no markup
    --correlator bb   which bulk correlator: bb (basic-block mnemonics, default),
                      mn (flat mnemonics), instr (flat instructions)

Prerequisites:
    Build and install the extension first:
        bash tools/patchdiff/build_and_install.sh
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

RB3 = Path(__file__).resolve().parents[2]
PROJ_LOC = str(RB3 / "ghidra_projects" / "RB3" / "RB3")
PROJ_NAME = "RB3"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--threshold", type=float, default=0.5,
                    help="Minimum similarity score to emit (0..1, default 0.5)")
    ap.add_argument("--dry", action="store_true",
                    help="Correlate and report counts only; apply no markup")
    ap.add_argument("--correlator", choices=["bb", "mn", "instr"], default="bb",
                    help="Bulk correlator variant: bb=BasicBlockMnemonics (default), "
                         "mn=FlatMnemonics, instr=FlatInstructions")
    ap.add_argument("--all-functions", action="store_true",
                    help="Compare ALL src/dst function pairs (O(N^2); very slow). "
                         "Default: only accepted matches (fast, fingerprints divergence).")
    args = ap.parse_args()

    import pyghidra

    pyghidra.start()

    from java.lang import Object as JObject
    from ghidra.base.project import GhidraProject
    from ghidra.feature.vt.api.db import VTSessionDB
    from ghidra.feature.vt.api.main import (
        VTMarkupItemApplyActionType,
        VTAssociationStatus,
    )
    from ghidra.util.task import ConsoleTaskMonitor

    monitor = ConsoleTaskMonitor()
    consumer = JObject()

    project = GhidraProject.openProject(PROJ_LOC, PROJ_NAME, True)
    src = dst = None
    for df in project.getRootFolder().getFiles():
        n = df.getName()
        if "band_r_wii" in n:
            src = project.openProgram("/", n, True)   # Bank 5 DWARF, read-only
        elif "bank8_target" in n:
            dst = project.openProgram("/", n, False)  # Bank 8 target, writable

    if src is None or dst is None:
        print("error: need both band_r_wii (Bank5) and bank8_target (Bank8) in project",
              file=sys.stderr)
        return 2

    # Load factory via ClassSearcher (extension must be installed in Ghidra/Extensions/)
    # The factories self-register as ExtensionPoint subclasses of VTProgramCorrelatorFactory.
    factory_class_name = {
        "bb":    "patchdiffcorrelator.BulkBasicBlockMnemonicProgramCorrelatorFactory",
        "mn":    "patchdiffcorrelator.BulkMnemonicProgramCorrelatorFactory",
        "instr": "patchdiffcorrelator.BulkInstructionProgramCorrelatorFactory",
    }[args.correlator]

    try:
        from java.lang import Class
        factory_cls = Class.forName(factory_class_name)
        factory = factory_cls.getDeclaredConstructor().newInstance()
    except Exception as exc:
        print(f"error: could not load {factory_class_name}: {exc}", file=sys.stderr)
        print("  Is the extension installed? Run: bash tools/patchdiff/build_and_install.sh",
              file=sys.stderr)
        return 3

    session = VTSessionDB.createVTSession("RB3 b5->b8 patchdiff", src, dst, consumer)
    src_set = src.getMemory().getLoadedAndInitializedAddressSet()
    dst_set = dst.getMemory().getLoadedAndInitializedAddressSet()

    opts = factory.createDefaultOptions()
    opts.setDouble("Minimum similarity threshold (score)", args.threshold)
    opts.setDouble("Minimum confidence threshold (score)", 0.0)
    opts.setBoolean("Only match accepted matches", not args.all_functions)
    opts.setBoolean("Symbol names must match", False)

    print(f"[patchdiff] running {factory.getName()} "
          f"(threshold={args.threshold}, all_functions={args.all_functions})",
          file=sys.stderr)
    corr = factory.createCorrelator(src, src_set, dst, dst_set, opts)
    match_set = corr.correlate(session, monitor)

    total = 0
    accepted = 0
    for ms in session.getMatchSets():
        for m in ms.getMatches():
            a = m.getAssociation()
            score = m.getSimilarityScore().getScore()
            if score < args.threshold:
                continue
            total += 1
            src_fn = src.getFunctionManager().getFunctionAt(a.getSourceAddress())
            dst_fn = dst.getFunctionManager().getFunctionAt(a.getDestinationAddress())
            print(f"  {score:.3f}  {src_fn.getName(True) if src_fn else a.getSourceAddress()}"
                  f"  ->  {dst_fn.getName(True) if dst_fn else a.getDestinationAddress()}")
            if not args.dry:
                try:
                    a.setAccepted()
                    accepted += 1
                except Exception:
                    pass

    print(f"[patchdiff] {total} matches above threshold={args.threshold}; "
          f"accepted={accepted} (dry={args.dry})", file=sys.stderr)

    if not args.dry and accepted > 0:
        from ghidra.framework.model import DomainObject
        dst.save("patchdiff VT pass", monitor)

    return 0


if __name__ == "__main__":
    sys.exit(main())
