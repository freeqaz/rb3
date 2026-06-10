#!/usr/bin/env python3
"""For the WHOLE Xenon program, count how many functions reference each of the
strings used as SRH keys by the judged-wrong pairs. If a key string is referenced
by exactly one Xenon function, ghidriff's single-string match was structurally
forced (and BinDiff's contradicting answer is the unreliable one). If referenced
by many, the 1:1 survivor logic was a coin-flip.
Output: /tmp/claude/string_uniqueness_out.json
"""
import json
import os
import sys

os.environ.setdefault("GHIDRA_INSTALL_DIR", "/home/free/code/milohax/ghidra/build/ghidra")
import pyghidra
pyghidra.start()
from ghidra.program.model.symbol import SymbolType  # noqa: E402

P2_GZF = "/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-xenon/rb3_xenon_default_xex.gzf"
P1_GZF = "/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff/gzfs/bank8_target.elf-42264e.gzf"

# strings to probe (contents without the 'ds "..."' wrapper)
PROBE = [
    "LayerDir", "EndingBonusDir", "LightHue", "BandList", "BandTrack",
    "MidiInstrument", "VocalTrackDir", "CamShot", "PartLauncher",
    "ParallelGroupSeq", "ui_trigger_complete", "VocalTrainerPanel",
    "WorldReflection", "component_scroll_start", "component_scroll_select",
    "UIPanel", "EventTrigger", "EditSetlistPanel", "WorldCrowd",
    "StreakMeterDir", "WorldInstance", "RandomIntervalGroupSeq", "LightPreset",
    "user_login", "MeshAnim", "TrackPanelDir",
]
PROBE_SET = set(PROBE)


def count_refs_to_strings(program, probe_set):
    """Return {string_content: set(function_entry_offset)} for probe strings, by
    walking string symbols and their references — same path get_defined_data uses."""
    from ghidra.program.model.data import StringDataInstance  # noqa: F401
    out = {s: set() for s in probe_set}
    fm = program.getFunctionManager()
    listing = program.getListing()
    for sym in program.getSymbolTable().getAllSymbols(True):
        if sym.referenceCount == 0:
            continue
        if sym.symbolType == SymbolType.FUNCTION:
            continue
        sym_addr = sym.getAddress()
        if sym_addr is None:
            continue
        data = listing.getDataAt(sym_addr)
        if data is None or not data.hasStringValue():
            continue
        sval = data.getValue()
        s = str(sval) if sval is not None else None
        if s not in probe_set:
            continue
        for ref in sym.references:
            f = fm.getFunctionContaining(ref.getFromAddress())
            if f is not None:
                out[s].add(f.getEntryPoint().getOffset())
    return out


res = {}
for tag, gzf in (("xenon", P2_GZF), ("wii", P1_GZF)):
    with pyghidra.open_program(gzf, analyze=False) as flat:
        prog = flat.getCurrentProgram()
        print(f"[uniq] {tag}: {prog.getName()}", file=sys.stderr)
        counts = count_refs_to_strings(prog, PROBE_SET)
        res[tag] = {s: sorted(hex(o) for o in offs) for s, offs in counts.items()}
        print(f"[uniq] {tag} done", file=sys.stderr)

json.dump(res, open("/tmp/claude/string_uniqueness_out.json", "w"))
print("[uniq] wrote /tmp/claude/string_uniqueness_out.json", file=sys.stderr)
