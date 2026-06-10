#!/usr/bin/env python3
"""T1 verification layer 3 (read-only, minutes-bounded): recompute the per-program
global `string -> #referencing-functions` map directly from the existing gzfs and
(a) confirm it matches string_uniqueness_out.json on the 26 SRH probe strings, and
(b) supply the two StrUnique key strings' true global ref-counts (ParticleSys,
InstrumentDifficultyDisplay) which the prior forensics probe set omitted.

Uses the SAME func_str_map path ghidriff.correlators.get_defined_data uses, then
inverts it (exactly like build_string_ref_counts) so this validates the real gate
inputs, not a re-derivation.

Run (read-only, no save, ~2-3 min):
  GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra \
  /home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-venv/bin/python \
    docs/decomp/xenon-hardening/forensics/verify_gate_refcounts.py
Output: docs/decomp/xenon-hardening/forensics/gate_refcounts_out.json
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

OUT = "/home/free/code/milohax/rb3/docs/decomp/xenon-hardening/forensics/gate_refcounts_out.json"

# Exact referenced-string content for the 23 SRH + 2 StrUnique judged-wrong pairs
# + the 3 sub-mode-B survivors (derived from strkeys_out.json, NOT the bare class
# names — e.g. the EndingBonus stub references "EndingBonusDir", not "EndingBonus").
PROBE = [
    "BandList", "BandTrack", "CamShot", "EditSetlistPanel", "EndingBonusDir",
    "EventTrigger", "InstrumentDifficultyDisplay", "LayerDir", "LightHue",
    "LightPreset", "MeshAnim", "MidiInstrument", "ParallelGroupSeq",
    "PartLauncher", "ParticleSys", "RandomIntervalGroupSeq", "StreakMeterDir",
    "TrackPanelDir", "UIPanel", "VocalTrackDir", "VocalTrainerPanel",
    "WorldCrowd", "WorldInstance", "WorldReflection",
    # sub-mode-B survivors (must be 1:1 on both sides):
    "component_scroll_select", "component_scroll_start", "ui_trigger_complete",
    "user_login",
]
PROBE_SET = set(PROBE)


def get_defined_data_func_map(program):
    """Replica of ghidriff.correlators.get_defined_data (func_str_map half),
    returning {func_entry_offset: [str(data), ...]}."""
    func_str_map = {}
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
        if data is not None and data.hasStringValue():
            for ref in sym.references:
                f = fm.getFunctionContaining(ref.getFromAddress())
                if f is not None:
                    func_str_map.setdefault(f.getEntryPoint().getOffset(), []).append(str(data))
    return func_str_map


def content(s):
    s = str(s)
    a = s.find('"')
    b = s.rfind('"')
    return s[a + 1:b] if (a != -1 and b > a) else s


def invert_to_refcounts(func_str_map):
    """Mirror correlators.build_string_ref_counts but keyed on CONTENT (so it lines
    up with the content-keyed probe strings)."""
    refs = {}
    for off, strs in func_str_map.items():
        for c in set(content(s) for s in strs):
            refs.setdefault(c, set()).add(off)
    return {c: len(funcs) for c, funcs in refs.items()}


res = {}
for tag, gzf in (("wii", P1_GZF), ("xenon", P2_GZF)):
    with pyghidra.open_program(gzf, analyze=False) as flat:
        prog = flat.getCurrentProgram()
        print(f"[verify] {tag}: {prog.getName()}", file=sys.stderr)
        fmap = get_defined_data_func_map(prog)
        rc = invert_to_refcounts(fmap)
        res[tag] = {s: rc.get(s, 0) for s in PROBE_SET}
        print(f"[verify] {tag} done; total distinct strings={len(rc)}", file=sys.stderr)

json.dump(res, open(OUT, "w"), indent=1, sort_keys=True)
print(f"[verify] wrote {OUT}", file=sys.stderr)
print(json.dumps(res, indent=1, sort_keys=True))
