#!/usr/bin/env python3
"""Measure how much DWARF type-porting actually landed on the Bank 8 program.

Step 1 of docs/decomp/patchdiff-rename-recovery-plan.md. Read-only.

Splits Bank 8's functions into three populations and reports, for each, how many
carry a *ported* signature (Ghidra `SignatureSource` IMPORTED/USER_DEFINED — what
`ApplyFunctionSignatureCmd(..., IMPORTED)` and VT apply-markup both stamp) vs a
default/analysis one:

  shared      — mangled name in BOTH maps. Type-ported by port_dwarf_types.py
                (by name) and/or VT's exact-symbol correlator. Sanity population:
                this should be ~fully typed if any porting ran at all.
  bank8_only  — mangled name in Bank 8 map, NOT in Bank 5 map (the ~11,751).
                Can ONLY be typed by a NAME-BLIND correlator (VT dupe/ref/implied)
                — i.e. a recovered RENAME. This count is the headline:
                  typed   -> renames VT already recovered (tiers 2-3)
                  untyped -> genuinely-new OR rename VT missed = patchdiff pool
  bank5_only  — informational (Bank 5 funcs with no Bank 8 by name; the src side
                of potential renames).

Run with the pyghidra-mcp service STOPPED (needs the project; read-only open):
    cd ../pyghidra-mcp && uv run --python 3.10 --project . \\
        python ../rb3/tools/ghidra/measure_rename_recovery.py
    # optional: --dump-untyped FILE  writes the untyped bank8_only symbols (the
    #           patchdiff dst candidate set) one per line.
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

# Match the pyghidra-mcp service env (tools/ghidra/pyghidra-service.sh): a CLEAN
# user home (not ~/.config/ghidra, which has the GhidraXenon extension that
# shadows the stock PowerPC langs and breaks PowerPC:BE:32:Gekko_Broadway).
os.environ.setdefault("GHIDRA_INSTALL_DIR", "/opt/ghidra")
os.environ.setdefault("GHIDRA_USER_HOME", "/tmp/claude/ghidra_user_rb3")
os.environ.setdefault("JAVA_HOME", "/usr/lib/jvm/java-17-openjdk")

RB3 = Path(__file__).resolve().parents[2]
PROJ_LOC = str(RB3 / "ghidra_projects" / "RB3" / "RB3")
PROJ_NAME = "RB3"
BANK8_MAP = RB3 / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"
BANK5_MAP = (RB3.parent / "milo-executable-library" / "rb3"
             / "Wii Proto (Bank 5) (Debug)" / "band_r_wii.map")

# Same map line parser as port_dwarf_types.py (keep in sync).
_LINE = re.compile(
    r"^  [0-9a-fA-F]{8} [0-9a-fA-F]{6} ([0-9a-fA-F]{8}) [0-9a-fA-F]{8}\s+\d+ (\S.*?) \t"
)


def _map_addrs(path: Path) -> dict:
    out: dict[str, int] = {}
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
    ap.add_argument("--dump-untyped", type=Path,
                    help="write the untyped bank8_only symbols (patchdiff dst pool) to this file")
    ap.add_argument("--show-typed-renames", type=int, default=10,
                    help="print up to N example bank8_only symbols that ARE typed (recovered renames)")
    args = ap.parse_args()

    b5_addr = _map_addrs(BANK5_MAP)
    b8_addr = _map_addrs(BANK8_MAP)
    shared = set(b5_addr) & set(b8_addr)
    bank8_only = set(b8_addr) - set(b5_addr)
    bank5_only = set(b5_addr) - set(b8_addr)
    print(f"[measure] Bank5 syms={len(b5_addr)} Bank8 syms={len(b8_addr)} "
          f"shared={len(shared)} bank8_only={len(bank8_only)} bank5_only={len(bank5_only)}",
          file=sys.stderr)

    import pyghidra
    pyghidra.start()
    from ghidra.base.project import GhidraProject
    from ghidra.program.model.symbol import SourceType

    project = GhidraProject.openProject(PROJ_LOC, PROJ_NAME, True)
    b8 = None
    for df in project.getRootFolder().getFiles():
        if "bank8_target" in df.getName():
            b8 = project.openProgram("/", df.getName(), True)  # read-only
            break
    if b8 is None:
        print("error: bank8_target program not found in project", file=sys.stderr)
        return 2

    fm = b8.getFunctionManager()
    space = b8.getAddressFactory().getDefaultAddressSpace()
    PORTED = {SourceType.IMPORTED, SourceType.USER_DEFINED}

    def is_ported(sym: str) -> bool | None:
        """True if the function's signature was ported (IMPORTED/USER_DEFINED),
        False if default/analysis, None if no function at the mapped address."""
        f = fm.getFunctionAt(space.getAddress(b8_addr[sym]))
        if f is None:
            return None
        return f.getSignatureSource() in PORTED

    def tally(names):
        typed = untyped = missing = 0
        typed_examples, untyped_examples = [], []
        for s in names:
            r = is_ported(s)
            if r is None:
                missing += 1
            elif r:
                typed += 1
                if len(typed_examples) < args.show_typed_renames:
                    typed_examples.append(s)
            else:
                untyped += 1
                if len(untyped_examples) < 200000:
                    untyped_examples.append(s)
        return typed, untyped, missing, typed_examples, untyped_examples

    def pct(n, d):
        return f"{100*n/max(1,d):5.1f}%"

    sh_t, sh_u, sh_m, _, _ = tally(shared)
    bo_t, bo_u, bo_m, bo_ex, bo_untyped = tally(bank8_only)

    print()
    print("=== Bank 8 type-porting coverage (signature source IMPORTED/USER_DEFINED) ===")
    print(f"shared     ({len(shared):6d}):  typed {sh_t:6d} ({pct(sh_t, sh_t+sh_u)})  "
          f"untyped {sh_u:6d}  missing-fn {sh_m}")
    print(f"  ^ sanity: by-name porting (port_dwarf_types / VT exact-symbol). Expect ~fully typed.")
    print()
    print(f"bank8_only ({len(bank8_only):6d}):  typed {bo_t:6d} ({pct(bo_t, bo_t+bo_u)})  "
          f"untyped {bo_u:6d}  missing-fn {bo_m}")
    print(f"  ^ typed   = RENAMES recovered by a name-blind correlator (VT dupe/ref/implied)")
    print(f"  ^ untyped = genuinely-new OR rename VT missed  ==>  patchdiff tier-4 candidate pool")
    if bo_ex:
        print(f"  example recovered renames (typed bank8_only):")
        for s in bo_ex:
            print(f"      {s}")

    if args.dump_untyped:
        args.dump_untyped.write_text("\n".join(sorted(bo_untyped)) + "\n")
        print(f"\n[measure] wrote {len(bo_untyped)} untyped bank8_only symbols -> {args.dump_untyped}",
              file=sys.stderr)

    project.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
