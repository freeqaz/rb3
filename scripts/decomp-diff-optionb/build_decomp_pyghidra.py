#!/usr/bin/env python3
"""Standalone pyghidra build-side decompile probe.
Usage: build_decomp_pyghidra.py <object.o> <SYMBOL> <OUTFILE> [LANGUAGE_ID]
Imports a relocatable PPC .o forcing Gekko_Broadway, analyzes, decompiles one fn.
"""
import sys, pathlib
sys.setrecursionlimit(8000)
import pyghidra
pyghidra.start()

obj, sym_name, outfile = sys.argv[1], sys.argv[2], sys.argv[3]
lang = sys.argv[4] if len(sys.argv) > 4 else "PowerPC:BE:32:Gekko_Broadway:default"

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

with pyghidra.open_program(obj, project_location="/tmp/rb3_buildghidra_py",
                           project_name="bp", analyze=True, language=lang) as api:
    prog = api.getCurrentProgram()
    print("LANGUAGE_ID=%s" % prog.getLanguage().getLanguageID())
    fm = prog.getFunctionManager()
    target = None
    for f in fm.getFunctions(True):
        if f.getName() == sym_name:
            target = f; break
    if target is None:
        st = prog.getSymbolTable()
        for s in st.getSymbols(sym_name):
            f = fm.getFunctionAt(s.getAddress())
            if f: target = f; break
    if target is None:
        pathlib.Path(outfile).write_text("FUNCTION_NOT_FOUND: %s\n" % sym_name)
        print("FUNCTION_NOT_FOUND %s" % sym_name); sys.exit(2)
    print("FOUND %s @ %s size=%d" % (target.getName(), target.getEntryPoint(),
                                     target.getBody().getNumAddresses()))
    di = DecompInterface(); di.openProgram(prog)
    res = di.decompileFunction(target, 180, ConsoleTaskMonitor())
    if res.decompileCompleted():
        code = res.getDecompiledFunction().getC()
        pathlib.Path(outfile).write_text(code)
        print("DECOMPILED ok %d chars -> %s" % (len(code), outfile))
    else:
        pathlib.Path(outfile).write_text("DECOMPILE_FAILED: %s\n" % res.getErrorMessage())
        print("DECOMPILE_FAILED %s" % res.getErrorMessage()); sys.exit(3)
