#!/usr/bin/env python3
"""MOVED — promoted to scripts/native/loaddet_gate.py in R4 (Wave-17 Lane L).

The Wave-12 A-S2 gate harness that lived here was promoted to the repo scripts
tree and extended with --attrib (M1 attribution) and --ledger (M3 per-axis
ledger) modes. This stub preserves the historical path reference.

    python3 scripts/native/loaddet_gate.py --help
"""
import os, runpy, sys

_TARGET = os.path.abspath(os.path.join(
    os.path.dirname(__file__), "..", "..", "..", "..", "..",
    "scripts", "native", "loaddet_gate.py"))

if __name__ == "__main__":
    sys.stderr.write(f"[moved] see {_TARGET}\n")
    runpy.run_path(_TARGET, run_name="__main__")
