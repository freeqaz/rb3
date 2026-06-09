#!/usr/bin/env python3
"""Enrich the Bank 8 target program in Ghidra with small-data-area register context.

Our Bank 8 program is a synthetic ELF (no GameCube/Wii loader ran), so Ghidra
never learned the Gekko small-data-area bases. CodeWarrior addresses globals as
`r13 + offset` (.sdata/.sbss) and `r2 + offset` (.sdata2/.sbss2); without the base
values the decompiler emits noise like `*(char *)(unaff_r13 + -0x6400)` instead of
the named global. The CodeWarrior map gives the bases as `_SDA_BASE_` (r13) and
`_SDA2_BASE_` (r2); this sets them as program-wide register context so the
decompiler folds `r13/r2 + off` into real, named global addresses.

(This mirrors what Cuyler36's Ghidra-GameCube-Loader GCAnalyzer does by reading
__init_registers; we read the bases straight from the linker map instead.)

WHY THE RE-ANALYSIS PASS (the fix this revision adds):
    Setting r13/r2 as program context makes the decompiler fold the SDA bases
    *in-session* (it re-derives the symbolic context each decompile). BUT that is
    transient: nothing is written to the program DB, so after `project.save` +
    a fresh service restart the decompiler shows `unaff_r13` again. The persistent
    artifact is the set of `r13+off -> global` *references* in the program's
    ReferenceManager. Those are created by the constant-propagation analysis pass,
    not by setValue. So after setting context we run Ghidra's
    "PowerPC Constant Reference Analyzer" (the PowerPCAddressAnalyzer, a subclass
    of ConstantPropagationAnalyzer) over the executable blocks. Its SymbolicPropogator
    seeds each function's register context from `program.getProgramContext()` at the
    entry (SymbolicPropogator.java: getRegistersWithValues()/getRegisterValue at
    flowStart), folds `r13/r2 + off` to a concrete address, and materializes a DATA
    reference into the DB (ConstantPropagationContextEvaluator.evaluateReference).
    Those references survive save+reload — that is what makes the fix persist.

    This is exactly the GCAnalyzer pattern: set the SDA registers, THEN trigger the
    constant/reference re-analysis that writes the references. We invoke the analyzer
    directly via `analyzer.added(program, execSet, monitor, log)` — identical to what
    AutoAnalysisManager's OneShotAnalysisCommand does — because the headless
    GhidraProject has no GUI tool/background-task thread, so the scheduler-based
    `scheduleOneTimeAnalysis` + `startAnalysis` path is fragile (it no-ops when no
    analysis tool is attached). The direct `added()` call is the robust headless route.

Run with the pyghidra-mcp service STOPPED (it holds the project lock):
    tools/ghidra/pyghidra-service.sh stop
    cd ../pyghidra-mcp && uv run --python 3.10 --project . \\
        python ../rb3/tools/ghidra/enrich_bank8.py
    tools/ghidra/pyghidra-service.sh start

VERIFY PERSISTENCE (orchestrator, in a SEPARATE fresh process after the run above):
    Re-open the project read-only and decompile the demo fn; the r13-noise count
    must still be 0 (proving the references were materialized to the DB, not just
    held in a live decompiler session):
        cd ../pyghidra-mcp && uv run --python 3.10 --project . \\
            python ../rb3/tools/ghidra/enrich_bank8.py --verify-only
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

RB3 = Path(__file__).resolve().parents[2]
PROJ_LOC = str(RB3 / "ghidra_projects" / "RB3" / "RB3")
PROJ_NAME = "RB3"
MAP = RB3 / "orig" / "SZBE69_B8" / "files" / "band_r_wii.map"
DEMO_SYM = "Init__14BandHeadShaperFv"


def _sda_bases() -> tuple[int, int]:
    """(_SDA_BASE_ for r13, _SDA2_BASE_ for r2) from the CodeWarrior map."""
    text = MAP.read_text(errors="replace")
    def find(name):
        m = re.search(rf"^\s*{re.escape(name)} ([0-9a-fA-F]{{8}})\s*$", text, re.M)
        return int(m.group(1), 16) if m else None
    r13 = find("_SDA_BASE_")
    r2 = find("_SDA2_BASE_")
    if r13 is None or r2 is None:
        raise SystemExit(f"could not find SDA bases in {MAP} (r13={r13}, r2={r2})")
    return r13, r2


def _r13_noise(b8, monitor) -> int:
    """Decompile the demo fn and count residual r13-base noise tokens."""
    from ghidra.app.decompiler import DecompInterface
    decomp = DecompInterface()
    decomp.openProgram(b8)
    a = b8.getSymbolTable().getGlobalSymbols(DEMO_SYM)[0].getAddress()
    f = b8.getFunctionManager().getFunctionAt(a)
    res = decomp.decompileFunction(f, 45, monitor)
    c = res.getDecompiledFunction().getC() if res.decompileCompleted() else "(failed)"
    return c.count("unaff_r13") + c.count("in_r13") + len(re.findall(r"r13 \+", c))


def _executable_set(b8):
    """AddressSet covering every executable (.text/.init etc.) memory block."""
    from ghidra.program.model.address import AddressSet
    exec_set = AddressSet()
    names = []
    for bk in b8.getMemory().getBlocks():
        if bk.isExecute() and bk.isInitialized():
            exec_set.addRange(bk.getStart(), bk.getEnd())
            names.append(bk.getName())
    return exec_set, names


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--verify-only", action="store_true",
        help="open bank8_target READ-ONLY, decompile the demo fn, print r13-noise, "
             "and exit (no context set, no analysis, no save). Run this in a FRESH "
             "process after a normal run to confirm the fix persisted across reload.")
    args = ap.parse_args(argv)

    r13_base, r2_base = _sda_bases()
    print(f"[enrich] _SDA_BASE_(r13)=0x{r13_base:08x}  _SDA2_BASE_(r2)=0x{r2_base:08x}", file=sys.stderr)

    import pyghidra

    pyghidra.start()
    from java.math import BigInteger
    from ghidra.base.project import GhidraProject
    from ghidra.util.task import ConsoleTaskMonitor
    from ghidra.app.plugin.core.analysis import AutoAnalysisManager
    from ghidra.app.util.importer import MessageLog

    # --verify-only opens read-only (does NOT take the writable lock / change anything).
    writable = not args.verify_only
    project = GhidraProject.openProject(PROJ_LOC, PROJ_NAME, True)
    b8 = None
    for df in project.getRootFolder().getFiles():
        if "bank8_target" in df.getName():
            b8 = project.openProgram("/", df.getName(), not writable)
            break
    if b8 is None:
        print("error: bank8_target program not found in project", file=sys.stderr)
        return 2

    monitor = ConsoleTaskMonitor()

    if args.verify_only:
        noise = _r13_noise(b8, monitor)
        ok = noise == 0
        print(f"[verify] r13-noise refs in {DEMO_SYM} after reload = {noise} "
              f"({'PERSISTED OK' if ok else 'NOT PERSISTED — fix did not stick'})",
              file=sys.stderr)
        project.close()
        return 0 if ok else 1

    ctx = b8.getProgramContext()
    r13 = ctx.getRegister("r13")
    r2 = ctx.getRegister("r2")
    # CodeWarrior loads r13/r2 once at startup and never changes them, so the SDA
    # bases are constant wherever code runs. Set the context per memory block
    # (setValue requires start/end in one address space; min..max can span spaces).
    blocks = [bk for bk in b8.getMemory().getBlocks()]

    print(f"[enrich] BEFORE: r13-noise refs in {DEMO_SYM} = {_r13_noise(b8, monitor)}", file=sys.stderr)

    # --- Step 1: set r13/r2 as program-wide register context ---------------------
    tx = b8.startTransaction("set SDA register context")
    nset = 0
    try:
        for bk in blocks:
            try:
                ctx.setValue(r13, bk.getStart(), bk.getEnd(), BigInteger.valueOf(r13_base))
                ctx.setValue(r2, bk.getStart(), bk.getEnd(), BigInteger.valueOf(r2_base))
                nset += 1
            except Exception as e:
                print(f"[enrich]   skip block {bk.getName()}: {e}", file=sys.stderr)
    finally:
        b8.endTransaction(tx, True)
    print(f"[enrich] set r13/r2 context over {nset} blocks", file=sys.stderr)

    # --- Step 2: MATERIALIZE references — run the constant-reference analyzer -----
    # This is the persistence fix. setValue alone is transient (re-derived per
    # decompile, never written to the DB). The PowerPC Constant Reference Analyzer
    # (PowerPCAddressAnalyzer extends ConstantPropagationAnalyzer) seeds its
    # SymbolicPropogator from the program context we just set, folds `r13/r2 + off`
    # into concrete global addresses, and writes DATA references into the DB.
    # Those references survive save+reload. We call analyzer.added() directly (the
    # same thing AutoAnalysisManager's OneShotAnalysisCommand does) because there is
    # no GUI analysis tool headless, so the scheduler path would silently no-op.
    exec_set, exec_names = _executable_set(b8)
    print(f"[enrich] executable blocks={exec_names} addrs={exec_set.getNumAddresses()}", file=sys.stderr)

    mgr = AutoAnalysisManager.getAnalysisManager(b8)
    ANALYZER_NAME = "PowerPC Constant Reference Analyzer"
    analyzer = mgr.getAnalyzer(ANALYZER_NAME)
    if analyzer is None:
        # Fallback: instantiate the analyzer directly (public no-arg ctor).
        from ghidra.app.plugin.core.analysis import PowerPCAddressAnalyzer
        analyzer = PowerPCAddressAnalyzer()
        print(f"[enrich] '{ANALYZER_NAME}' not registered on mgr; using direct PowerPCAddressAnalyzer()",
              file=sys.stderr)
    else:
        print(f"[enrich] using analyzer: {analyzer.getName()}", file=sys.stderr)

    # Prime the analyzer's derived defaults (checkStoredRefsOption, speculative
    # ref bounds from the address-space size). The framework normally calls this;
    # on the direct-instantiation fallback path it hasn't, and added() reads them.
    try:
        analyzer.canAnalyze(b8)
    except Exception as e:
        print(f"[enrich]   canAnalyze() priming skipped: {e}", file=sys.stderr)

    log = MessageLog()
    tx2 = b8.startTransaction("materialize SDA references (constant analysis)")
    try:
        # analyzer.added(program, set, monitor, log) -> walks function bodies in
        # exec_set, runs constant propagation, creates r13/r2+off -> global refs.
        analyzer.added(b8, exec_set, monitor, log)
    finally:
        b8.endTransaction(tx2, True)
    status = log.getStatus()
    if status:
        print(f"[enrich] analyzer log: {status}", file=sys.stderr)
    print("[enrich] constant-reference analysis complete (references materialized)", file=sys.stderr)

    print(f"[enrich] AFTER : r13-noise refs in {DEMO_SYM} = {_r13_noise(b8, monitor)}", file=sys.stderr)

    # --- Step 3: persist ----------------------------------------------------------
    project.save(b8)
    print("[enrich] saved bank8_target with SDA context + materialized references", file=sys.stderr)
    print("[enrich] verify persistence in a FRESH process: "
          "python tools/ghidra/enrich_bank8.py --verify-only", file=sys.stderr)
    project.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
