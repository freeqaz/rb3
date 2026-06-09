#!/usr/bin/env python3
"""Headless Ghidra decompiler for the **Bank 8 target** (the real DOL).

Unlike the pyghidra-mcp service (which decompiles the Bank 5 DWARF ELF, ~mid-2009,
and so describes a different-era body for ~20% of functions), this points Ghidra
at a symbolized ELF transcoded from the actual Bank 8 ``main.dol`` via
``pyghidra_mcp.gamecube_dol``. The decompiled pseudo-C therefore matches the
function we are actually trying to match (names from the CodeWarrior map; no
DWARF types).

Usage (run through the pyghidra-mcp venv):
    cd ../pyghidra-mcp && uv run --python 3.10 --project . \\
        python ../rb3/tools/ghidra/bank8_decompile.py Init__14BandHeadShaperFv

First run imports + analyzes the 17 MB ELF into a persistent project under
build/SZBE69_B8/ghidra/proj (minutes); later runs reuse it (seconds).
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

RB3 = Path(__file__).resolve().parents[2]
DOL = RB3 / "orig" / "SZBE69_B8" / "sys" / "main.dol"
MAP = RB3 / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"
ELF = RB3 / "build" / "SZBE69_B8" / "ghidra" / "bank8_target.elf"
PROJ = RB3 / "build" / "SZBE69_B8" / "ghidra" / "proj"
GEKKO = "PowerPC:BE:32:Gekko_Broadway"


def _map_size(symbol: str) -> int | None:
    """Look up a symbol's byte size from the Bank 8 CodeWarrior map."""
    import re

    if not MAP.exists():
        return None
    pat = re.compile(
        r"^  [0-9a-fA-F]{8} ([0-9a-fA-F]{6}) [0-9a-fA-F]{8} [0-9a-fA-F]{8}\s+\d+ "
        + re.escape(symbol) + r" \t"
    )
    for line in MAP.read_text(errors="replace").splitlines():
        m = pat.match(line)
        if m:
            return int(m.group(1), 16)
    return None


def _ensure_elf() -> Path:
    # Reuse the fork's converter so the ELF is identical to the import path.
    sys.path.insert(0, str(RB3.parent / "pyghidra-mcp" / "src"))
    from pyghidra_mcp.gamecube_dol import convert

    if not ELF.exists() or ELF.stat().st_mtime < DOL.stat().st_mtime:
        ELF.parent.mkdir(parents=True, exist_ok=True)
        info = convert(DOL, MAP if MAP.exists() else None, ELF)
        print(f"[bank8] built ELF: {info['functions']} functions, entry 0x{info['entry']:08x}", file=sys.stderr)
    return ELF


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("symbol", help="mangled symbol (e.g. Init__14BandHeadShaperFv) or 0xADDR")
    ap.add_argument("--analyze", action="store_true", help="full auto-analysis (slow, first run)")
    ap.add_argument("--no-persist", action="store_true", help="use a throwaway project")
    args = ap.parse_args()

    elf = _ensure_elf()
    import pyghidra

    pyghidra.start()
    from ghidra.app.decompiler import DecompInterface
    from ghidra.util.task import ConsoleTaskMonitor
    from ghidra.program.model.lang import LanguageID
    from ghidra.program.util import DefaultLanguageService

    lang = DefaultLanguageService.getLanguageService().getLanguage(LanguageID(GEKKO))
    cspec = lang.getDefaultCompilerSpec()

    proj_loc = None if args.no_persist else str(PROJ)
    if proj_loc:
        Path(proj_loc).mkdir(parents=True, exist_ok=True)

    with pyghidra.open_program(
        str(elf), project_location=proj_loc, project_name="bank8",
        language=str(GEKKO), compiler=str(cspec.getCompilerSpecID()),
        analyze=args.analyze,
    ) as flat:
        program = flat.getCurrentProgram()
        monitor = ConsoleTaskMonitor()

        # Resolve the target symbol -> function.
        from ghidra.program.model.address import GlobalNamespace  # noqa: F401
        st = program.getSymbolTable()
        func = None
        addr = None
        if args.symbol.startswith("0x"):
            addr = program.getAddressFactory().getDefaultAddressSpace().getAddress(int(args.symbol, 16))
        else:
            syms = st.getGlobalSymbols(args.symbol)
            if syms:
                addr = syms[0].getAddress()
        if addr is None:
            print(f"symbol not found: {args.symbol}", file=sys.stderr)
            return 2

        fm = program.getFunctionManager()
        func = fm.getFunctionAt(addr)
        if func is None:
            # Symbol present but no function object yet (analyze=False). Disassemble
            # the function's full declared extent from the CW map so the body + flow
            # are complete, then create the function over that range.
            decl = _map_size(args.symbol)
            if decl:
                from ghidra.program.model.address import AddressSet
                from ghidra.app.cmd.disassemble import DisassembleCommand
                end = addr.add(decl - 1)
                flat.disassemble(addr)  # seed; then cover the whole declared range
                DisassembleCommand(AddressSet(addr, end), None, True).applyTo(program, monitor)
            else:
                flat.disassemble(addr)
            func = fm.getFunctionAt(addr) or flat.createFunction(addr, args.symbol)

        size = func.getBody().getNumAddresses() if func else 0
        print(f"// Bank 8 target  {args.symbol}  @ {addr}  body={size} bytes "
              f"(map={_map_size(args.symbol) or '?'})", file=sys.stderr)

        decomp = DecompInterface()
        decomp.openProgram(program)
        res = decomp.decompileFunction(func, 60, monitor)
        if not res.decompileCompleted():
            print(f"decompile failed: {res.getErrorMessage()}", file=sys.stderr)
            return 3
        print(res.getDecompiledFunction().getC())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
