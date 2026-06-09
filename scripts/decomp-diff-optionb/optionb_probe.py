#!/usr/bin/env python3
"""Option B prototype: decompile build .o + target ELF for a set of functions,
forcing Gekko_Broadway on both, write pseudo-C to /tmp/optionb/<side>_<sym>.c"""
import sys, pathlib
sys.setrecursionlimit(8000)
import pyghidra
pyghidra.start()
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

OUT = pathlib.Path("/tmp/optionb"); OUT.mkdir(exist_ok=True)
LANG = "PowerPC:BE:32:Gekko_Broadway"

# (symbol, build_obj)
TARGETS = [
    ("UpdateScrolling__10VocalTrackFf",   "build/SZBE69_B8/obj/band3/bandtrack/VocalTrack.o"),
    ("Highlight__8CharEyesFv",            "build/SZBE69_B8/obj/system/char/CharEyes.o"),
    ("ComputeDeformWeights__12BandCharDescCFPf", "build/SZBE69_B8/obj/system/bandobj/BandCharDesc.o"),
]
TARGET_ELF = "build/SZBE69_B8/ghidra/bank8_target.elf"
ROOT = "/home/free/code/milohax/rb3/"

def find_fn(prog, sym):
    fm = prog.getFunctionManager()
    for f in fm.getFunctions(True):
        if f.getName() == sym: return f
    st = prog.getSymbolTable()
    for s in st.getSymbols(sym):
        f = fm.getFunctionAt(s.getAddress())
        if f: return f
    return None

def decomp_one(prog, sym, side):
    f = find_fn(prog, sym)
    if not f:
        print("  [%s] NOT FOUND %s" % (side, sym), flush=True); return
    di = DecompInterface(); di.openProgram(prog)
    res = di.decompileFunction(f, 200, ConsoleTaskMonitor())
    if res.decompileCompleted():
        code = res.getDecompiledFunction().getC()
        (OUT / ("%s_%s.c" % (side, sym))).write_text(code)
        print("  [%s] %s OK %d chars" % (side, sym, len(code)), flush=True)
    else:
        print("  [%s] %s FAILED %s" % (side, sym, res.getErrorMessage()), flush=True)

# TARGET side: one program, all three symbols
print("=== TARGET ELF ===", flush=True)
with pyghidra.open_program(ROOT+TARGET_ELF, project_location="/tmp/optionb_proj",
                           project_name="tgt", analyze=True,
                           language=LANG, compiler="default") as api:
    prog = api.getCurrentProgram()
    print("LANG=%s funcs=%d" % (prog.getLanguage().getLanguageID(), prog.getFunctionManager().getFunctionCount()), flush=True)
    for sym, _ in TARGETS:
        decomp_one(prog, sym, "target")

# BUILD side: per-object program
for sym, obj in TARGETS:
    print("=== BUILD %s ===" % obj, flush=True)
    with pyghidra.open_program(ROOT+obj, project_location="/tmp/optionb_proj",
                               project_name="b_"+sym[:12], analyze=True,
                               language=LANG, compiler="default") as api:
        prog = api.getCurrentProgram()
        decomp_one(prog, sym, "build")
print("DONE", flush=True)
