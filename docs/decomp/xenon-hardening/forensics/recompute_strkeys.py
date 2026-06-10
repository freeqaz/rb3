#!/usr/bin/env python3
"""Read-only recompute of StringsRefsHasher / StrUniqueFuncRefsHasher keys for
the SRH/StrUnique matched pairs, to characterize cross-compiler string collisions.

Opens both gzf programs via pyghidra (no save), replicates ghidriff
correlators.get_defined_data() exactly, and emits per-function string multisets +
the resulting hash keys. Output: /tmp/claude/strkeys_out.json
"""
import json
import os
import sys

os.environ.setdefault("GHIDRA_INSTALL_DIR", "/home/free/code/milohax/ghidra/build/ghidra")

import pyghidra
pyghidra.start()

from ghidra.program.model.symbol import SymbolType  # noqa: E402

P1_GZF = "/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff/gzfs/bank8_target.elf-42264e.gzf"
P2_GZF = "/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-xenon/rb3_xenon_default_xex.gzf"

pairs = json.load(open("/tmp/claude/strkey_pairs.json"))


def parse_addr(s):
    s = s.lower()
    if s.startswith("0x"):
        s = s[2:]
    return int(s, 16)


def get_defined_data(program):
    """Replica of ghidriff.correlators.get_defined_data (the func_str_map half)."""
    from ghidra.program.model.data import StringDataInstance
    from ghidra.program.util import DefinedDataIterator

    func_str_map = {}
    for sym in program.getSymbolTable().getAllSymbols(True):
        if sym.referenceCount == 0:
            continue
        if sym.symbolType == SymbolType.FUNCTION:
            continue
        sym_addr = sym.getAddress()
        if sym_addr is None:
            continue
        data = program.getListing().getDataAt(sym_addr)
        if data is not None and data.hasStringValue():
            for ref in sym.references:
                f = program.getFunctionManager().getFunctionContaining(ref.getFromAddress())
                if f is not None:
                    func_str_map.setdefault(f.entryPoint, []).append(str(data))
    return func_str_map


def addr_str(program, off):
    return program.getAddressFactory().getDefaultAddressSpace().getAddress(off)


def collect(program, offsets):
    """offsets: set of int entry offsets we care about. Returns
    {hex_addr: {'strings': sorted_multiset, 'n': len, 'nuniq': len(set), 'size': func size}}"""
    fmap = get_defined_data(program)
    # index by entry offset
    by_off = {}
    for ep, strs in fmap.items():
        by_off[ep.getOffset()] = strs
    fm_mgr = program.getFunctionManager()
    out = {}
    for off in offsets:
        a = addr_str(program, off)
        f = fm_mgr.getFunctionAt(a)
        strs = by_off.get(off)
        if strs is not None:
            ms = sorted(str(x) for x in strs)
        else:
            ms = None  # uuid branch -> unique, never collides
        sz = None
        if f is not None:
            try:
                sz = int(f.getBody().getNumAddresses())
            except Exception:
                sz = None
        out[hex(off)] = {
            "strings": ms,
            "n": (len(ms) if ms is not None else 0),
            "nuniq": (len(set(ms)) if ms is not None else 0),
            "size": sz,
            "name": (f.getName() if f is not None else None),
        }
    return out


p1_offs = set()
p2_offs = set()
for grp in ("srh", "stu"):
    for a1, a2 in pairs[grp]:
        p1_offs.add(parse_addr(a1))
        p2_offs.add(parse_addr(a2))

print(f"[recompute] p1 offsets: {len(p1_offs)}  p2 offsets: {len(p2_offs)}", file=sys.stderr)

result = {}
with pyghidra.open_program(P1_GZF, analyze=False) as flat1:
    prog1 = flat1.getCurrentProgram()
    print(f"[recompute] opened p1 {prog1.getName()}", file=sys.stderr)
    result["p1"] = collect(prog1, p1_offs)
    print(f"[recompute] p1 collected {len(result['p1'])}", file=sys.stderr)

with pyghidra.open_program(P2_GZF, analyze=False) as flat2:
    prog2 = flat2.getCurrentProgram()
    print(f"[recompute] opened p2 {prog2.getName()}", file=sys.stderr)
    result["p2"] = collect(prog2, p2_offs)
    print(f"[recompute] p2 collected {len(result['p2'])}", file=sys.stderr)

json.dump(result, open("/tmp/claude/strkeys_out.json", "w"))
print("[recompute] wrote /tmp/claude/strkeys_out.json", file=sys.stderr)
