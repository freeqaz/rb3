#!/usr/bin/env python3
"""Port Bank 5 DWARF types onto the Bank 8 target program (Ghidra).

The Bank 8 program (the real target body) has accurate control flow but NO types
(names only, from the CodeWarrior map). The Bank 5 program has full DWARF types
but a wrong-era body for ~20% of functions. This bridges them: it copies Bank 5's
typed **function signatures** onto the matching Bank 8 functions, so Ghidra's
decompiler propagates the types through the real Bank 8 body — giving typed
pseudo-C of the actual target.

Matching is by **mangled symbol name** via the two CodeWarrior maps (the builds
have different addresses but the same symbols). `ApplyFunctionSignatureCmd`
resolves each signature's referenced struct/class types into the Bank 8 program
transitively, so the type catalog comes along automatically.

Run with the pyghidra-mcp service STOPPED (it holds the project lock):
    tools/ghidra/pyghidra-service.sh stop
    cd ../pyghidra-mcp && uv run --python 3.10 --project . \\
        python ../rb3/tools/ghidra/port_dwarf_types.py Init__14BandHeadShaperFv
    # ... or --all to port every shared function and save:
    #     python ../rb3/tools/ghidra/port_dwarf_types.py --all
    tools/ghidra/pyghidra-service.sh start

This is the scripted, deterministic port (signatures + transitive types). Ghidra's
Version Tracking tool can additionally port local-variable names, comments, and
plate markup for byte-matching functions — a deeper follow-up.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

RB3 = Path(__file__).resolve().parents[2]
PROJ_LOC = str(RB3 / "ghidra_projects" / "RB3" / "RB3")
PROJ_NAME = "RB3"
B5_MAP = RB3 / "orig" / "SZBE69_B8" / "files"  # (Bank 8 map; Bank 5 map below)
BANK8_MAP = RB3 / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"
BANK5_MAP = (RB3.parent / "milo-executable-library" / "rb3"
             / "Wii Proto (Bank 5) (Debug)" / "band_r_wii.map")

_LINE = re.compile(
    r"^  [0-9a-fA-F]{8} [0-9a-fA-F]{6} ([0-9a-fA-F]{8}) [0-9a-fA-F]{8}\s+\d+ (\S.*?) \t"
)


def _map_addrs(path: Path) -> dict:
    """mangled symbol -> virtual address (first occurrence)."""
    out = {}
    for line in path.read_text(errors="replace").splitlines():
        m = _LINE.match(line)
        if not m:
            continue
        addr, name = int(m.group(1), 16), m.group(2)
        if name.startswith("."):
            continue
        out.setdefault(name, addr)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("symbols", nargs="*", help="mangled symbols to port (default: a demo function)")
    ap.add_argument("--all", action="store_true", help="port every shared function and save")
    ap.add_argument("--no-save", action="store_true", help="don't persist (demo only)")
    ap.add_argument("--show", action="store_true", help="decompile each ported function before+after")
    ap.add_argument(
        "--gdt", action="store_true",
        help="BEFORE applying signatures, pre-seed Bank 8's DTM with the full Bank 5 "
             "type catalog from build/SZBE69_B8/ghidra/bank5_types.gdt (run "
             "export_bank5_gdt.py first). Fixes struct params decoding as 'undefined *'.",
    )
    args = ap.parse_args()

    b5_addr = _map_addrs(BANK5_MAP)
    b8_addr = _map_addrs(BANK8_MAP)
    shared = sorted(set(b5_addr) & set(b8_addr))
    print(f"[port] Bank5 syms={len(b5_addr)} Bank8 syms={len(b8_addr)} shared={len(shared)}", file=sys.stderr)

    targets = shared if args.all else (args.symbols or ["Init__14BandHeadShaperFv"])

    import pyghidra

    pyghidra.start()
    from ghidra.app.cmd.function import ApplyFunctionSignatureCmd
    from ghidra.app.decompiler import DecompInterface
    from ghidra.base.project import GhidraProject
    from ghidra.program.model.symbol import SourceType
    from ghidra.util.task import ConsoleTaskMonitor

    # The .gpr lives at ghidra_projects/RB3/RB3/RB3.gpr -> location is that dir.
    project = GhidraProject.openProject(PROJ_LOC, PROJ_NAME, True)
    root = project.getRootFolder()
    b5 = b8 = None
    for df in root.getFiles():
        n = df.getName()
        if "band_r_wii" in n:
            b5 = project.openProgram("/", n, True)   # read-only
        elif "bank8_target" in n:
            b8 = project.openProgram("/", n, False)   # writable
    if b5 is None or b8 is None:
        print("error: need both band_r_wii (Bank 5) and bank8_target (Bank 8) programs in the project", file=sys.stderr)
        return 2

    monitor = ConsoleTaskMonitor()

    # Pre-seed Bank 8's DTM with Bank 5's complete type catalog from the .gdt archive
    # so that the ApplyFunctionSignatureCmd loop below references already-fully-defined
    # structs and never falls back to "undefined *". Full root-cause + handler choice
    # live in apply_types_from_gdt.py. Done in its own committed transaction before the
    # signature transaction so the seeded types are stable when signatures resolve.
    if args.gdt:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from apply_types_from_gdt import seed_bank8_types

        gdt_tx = b8.startTransaction("pre-seed Bank 5 types from gdt")
        gdt_ok = False
        try:
            seed_bank8_types(b8, monitor)
            gdt_ok = True
        finally:
            b8.endTransaction(gdt_tx, gdt_ok)

    decomp = DecompInterface()
    decomp.openProgram(b8)

    def b8_func(sym):
        a = b8.getAddressFactory().getDefaultAddressSpace().getAddress(b8_addr[sym])
        return b8.getFunctionManager().getFunctionAt(a), a

    def b5_func(sym):
        a = b5.getAddressFactory().getDefaultAddressSpace().getAddress(b5_addr[sym])
        return b5.getFunctionManager().getFunctionAt(a)

    def decompile(func):
        r = decomp.decompileFunction(func, 45, monitor)
        return r.getDecompiledFunction().getSignature() if r.decompileCompleted() else "(decompile failed)"

    # Periodic commit+save so a long --all run can't lose everything to an
    # interruption (the first attempt was killed by a timeout before saving).
    SAVE_EVERY = 8000
    tx = b8.startTransaction("port DWARF signatures")
    applied = skipped = 0
    try:
        for i, sym in enumerate(targets):
            f5 = b5_func(sym)
            f8, a8 = b8_func(sym)
            if f5 is None or f8 is None:
                skipped += 1
                continue
            if args.show:
                print(f"\n# {sym}\n  Bank8 BEFORE: {decompile(f8)}")
            sig = f5.getSignature()
            ok = ApplyFunctionSignatureCmd(a8, sig, SourceType.IMPORTED).applyTo(b8, monitor)
            applied += 1 if ok else 0
            if args.show:
                print(f"  Bank5 DWARF : {f5.getSignature().getPrototypeString()}")
                print(f"  Bank8 AFTER : {decompile(f8)}")
            if args.all and applied and applied % 2000 == 0:
                print(f"[port] applied {applied}/{len(targets)}...", file=sys.stderr)
            if args.all and not args.no_save and (i + 1) % SAVE_EVERY == 0:
                b8.endTransaction(tx, True)
                project.save(b8)
                print(f"[port] checkpoint saved at {i + 1}/{len(targets)}", file=sys.stderr)
                tx = b8.startTransaction("port DWARF signatures")
    finally:
        b8.endTransaction(tx, True)

    print(f"[port] applied={applied} skipped={skipped}", file=sys.stderr)
    if not args.no_save and (args.all or applied):
        project.save(b8)
        print("[port] saved bank8_target program", file=sys.stderr)
    project.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
