#!/usr/bin/env python3
"""Port Bank 5 DWARF signatures onto RENAMED Bank 8 functions, by address pair.

Complements port_dwarf_types.py (which ports by shared mangled NAME). ghidriff
matched some functions structurally despite a changed mangled name (signature
evolution between mid-2009 Bank 5 and early-2010 Bank 8). distill_ghidriff.py
emits those as renames.json; the `port_safe` subset (same base identifier, body
>= 16 bytes — filters out trivial byte-collisions) is safe to inherit Bank 5's
signature on the Bank 8 body.

CAVEAT: a rename means the signature CHANGED, so Bank 5's is the OLD one. We only
apply when the PARAMETER COUNT matches (const/type tweaks on the same arity) — the
struct/class types come across correctly and the body is (near-)identical. When
the param count differs we SKIP (the interface genuinely changed). Already-typed
Bank 8 functions (signature source IMPORTED/USER_DEFINED, e.g. from a VT pass) are
also skipped so we never overwrite a better result.

Run with the pyghidra-mcp service STOPPED (needs the project; writable):
    cd ../pyghidra-mcp && uv run --python 3.10 --project . \\
        python ../rb3/tools/ghidra/port_renames.py            # apply + save
    #   --dry   report what WOULD apply, change nothing
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

# Match the pyghidra-mcp service env (clean user home avoids the GhidraXenon
# extension that breaks PowerPC:BE:32:Gekko_Broadway). See pyghidra-service.sh.
os.environ.setdefault("GHIDRA_INSTALL_DIR", "/opt/ghidra")
os.environ.setdefault("GHIDRA_USER_HOME", "/tmp/claude/ghidra_user_rb3")
os.environ.setdefault("JAVA_HOME", "/usr/lib/jvm/java-17-openjdk")

RB3 = Path(__file__).resolve().parents[2]
PROJ_LOC = str(RB3 / "ghidra_projects" / "RB3" / "RB3")
PROJ_NAME = "RB3"
RENAMES = RB3 / "build" / "SZBE69_B8" / "ghidra" / "ghidriff" / "renames.json"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--renames", type=Path, default=RENAMES)
    ap.add_argument("--dry", action="store_true", help="report only; apply nothing")
    ap.add_argument("--all-renames", action="store_true",
                    help="port ALL true renames, not just the port_safe subset (risky)")
    args = ap.parse_args()

    data = json.loads(args.renames.read_text())
    pool = [r for r in data if (r.get("port_safe") or (args.all_renames and r.get("base_match")))]
    print(f"[port-renames] {len(pool)} candidate pairs from {args.renames.name} "
          f"(port_safe only={not args.all_renames})", file=sys.stderr)
    if not pool:
        return 0

    import pyghidra
    pyghidra.start()
    from ghidra.app.cmd.function import ApplyFunctionSignatureCmd
    from ghidra.base.project import GhidraProject
    from ghidra.program.model.symbol import SourceType
    from ghidra.util.task import ConsoleTaskMonitor

    project = GhidraProject.openProject(PROJ_LOC, PROJ_NAME, True)
    b5 = b8 = None
    for df in project.getRootFolder().getFiles():
        n = df.getName()
        if "band_r_wii" in n:
            b5 = project.openProgram("/", n, True)
        elif "bank8_target" in n:
            b8 = project.openProgram("/", n, not args.dry)  # writable unless dry
    if b5 is None or b8 is None:
        print("error: need both Bank 5 and Bank 8 programs in the project", file=sys.stderr)
        return 2

    monitor = ConsoleTaskMonitor()
    PORTED = {SourceType.IMPORTED, SourceType.USER_DEFINED}

    def fn(prog, hexaddr):
        a = prog.getAddressFactory().getDefaultAddressSpace().getAddress(int(hexaddr, 16))
        return prog.getFunctionManager().getFunctionAt(a), a

    applied = skip_typed = skip_paramcount = skip_missing = 0
    tx = b8.startTransaction("port renamed DWARF signatures") if not args.dry else None
    try:
        for r in pool:
            f5, _ = fn(b5, r["b5_addr"])
            f8, a8 = fn(b8, r["b8_addr"])
            if f5 is None or f8 is None:
                skip_missing += 1
                continue
            if f8.getSignatureSource() in PORTED:
                skip_typed += 1
                continue
            if f5.getParameterCount() != f8.getParameterCount():
                skip_paramcount += 1
                print(f"  skip(paramcount {f5.getParameterCount()}!={f8.getParameterCount()}) "
                      f"{r['b5_sym']} -> {r['b8_sym']}")
                continue
            if args.dry:
                print(f"  WOULD apply {f5.getSignature().getPrototypeString()}  ->  {r['b8_sym']}")
                applied += 1
                continue
            ok = ApplyFunctionSignatureCmd(a8, f5.getSignature(), SourceType.IMPORTED).applyTo(b8, monitor)
            if ok:
                applied += 1
            else:
                skip_paramcount += 1
    finally:
        if tx is not None:
            b8.endTransaction(tx, True)

    print(f"[port-renames] applied={applied} skip_already_typed={skip_typed} "
          f"skip_paramcount={skip_paramcount} skip_missing={skip_missing}", file=sys.stderr)
    if not args.dry and applied:
        project.save(b8)
        print("[port-renames] saved bank8_target program", file=sys.stderr)
    project.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
