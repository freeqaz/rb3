#!/usr/bin/env python3
"""T1 survivor analysis (read-only, minutes-bounded): replay the 1:1-global-
uniqueness gate over ALL 610 StringsRefsHasher + 45 StrUniqueFuncRefsHasher matched
pairs and report how many survive.

For each program we build the COMPLETE whole-program func_str_map (exactly
ghidriff.correlators.get_defined_data), invert it to a global string->#funcs count
(build_string_ref_counts), then for every matched pair apply gate_string_keys to
BOTH sides and check whether they still produce the same key. A pair "survives"
only if both sides pass the gate AND key-equal. The historical run matched all 655
(every pair key-equal pre-gate), so survivors = pairs whose shared key is still
1:1-unique post-gate.

Run (read-only, no save, ~3-5 min):
  GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra \
  JAVA_HOME=/usr/lib/jvm/java-17-openjdk \
  /home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-venv/bin/python \
    docs/decomp/xenon-hardening/forensics/survivor_analysis.py
Output: docs/decomp/xenon-hardening/forensics/survivor_analysis_out.json
"""
import json
import os
import sys

os.environ.setdefault("GHIDRA_INSTALL_DIR", "/home/free/code/milohax/ghidra/build/ghidra")

import pyghidra
pyghidra.start()

from ghidra.program.model.symbol import SymbolType  # noqa: E402

# Import the REAL pure gate from the package by file path (no JVM-package import).
import importlib.util  # noqa: E402

_CORR = "/home/free/code/milohax/ghidriff/ghidriff/correlators.py"
# correlators.py imports jpype + ghidra; under pyghidra those resolve, so a normal
# spec-load works here.
_spec = importlib.util.spec_from_file_location("corr_under_test", _CORR)
_corr = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_corr)
gate_string_keys = _corr.gate_string_keys
build_string_ref_counts = _corr.build_string_ref_counts
MIN_STRING_LEN = _corr.MIN_STRING_LEN
NO_KEY = _corr.NO_KEY

P1_GZF = "/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff/gzfs/bank8_target.elf-42264e.gzf"
P2_GZF = "/home/free/code/milohax/rb3/build/SZBE69_B8/ghidra/ghidriff-xenon/rb3_xenon_default_xex.gzf"
OUT = "/home/free/code/milohax/rb3/docs/decomp/xenon-hardening/forensics/survivor_analysis_out.json"

pairs = json.load(open("/tmp/claude/all_str_pairs.json"))  # {'srh':[[p1,p2],...],'stu':[...]}


def parse_off(s):
    s = s.lower()
    if s.startswith("0x"):
        s = s[2:]
    return int(s, 16)


def full_func_str_map(program):
    """Exact replica of ghidriff.correlators.get_defined_data func_str_map half,
    keyed by entry-point OFFSET (int) instead of Address for json-friendliness."""
    fmap = {}
    fm = program.getFunctionManager()
    listing = program.getListing()
    for sym in program.getSymbolTable().getAllSymbols(True):
        if sym.referenceCount == 0:
            continue
        if sym.symbolType == SymbolType.FUNCTION:
            continue
        a = sym.getAddress()
        if a is None:
            continue
        data = listing.getDataAt(a)
        if data is not None and data.hasStringValue():
            for ref in sym.references:
                f = fm.getFunctionContaining(ref.getFromAddress())
                if f is not None:
                    fmap.setdefault(f.getEntryPoint().getOffset(), []).append(str(data))
    return fmap


def analyze(program):
    fmap = full_func_str_map(program)
    rc = build_string_ref_counts(fmap, MIN_STRING_LEN)
    # Gate every function (whole-program) once.
    keys = gate_string_keys(fmap, dedup=False, min_len=MIN_STRING_LEN, ref_counts=rc)
    keys_dedup = gate_string_keys(fmap, dedup=True, min_len=MIN_STRING_LEN, ref_counts=rc)
    return fmap, keys, keys_dedup


print("[surv] opening p1 (wii)...", file=sys.stderr)
with pyghidra.open_program(P1_GZF, analyze=False) as f1:
    p1 = f1.getCurrentProgram()
    fmap1, keys1, keys1d = analyze(p1)
    print(f"[surv] p1 funcs-with-strings={len(fmap1)}", file=sys.stderr)

print("[surv] opening p2 (xenon)...", file=sys.stderr)
with pyghidra.open_program(P2_GZF, analyze=False) as f2:
    p2 = f2.getCurrentProgram()
    fmap2, keys2, keys2d = analyze(p2)
    print(f"[surv] p2 funcs-with-strings={len(fmap2)}", file=sys.stderr)


def survivors(pair_list, k1, k2):
    n_total = len(pair_list)
    n_surv = 0
    surv_single = 0   # survived on a single (globally-unique) string key
    surv_multi = 0    # survived on a >=2-distinct-string key
    surv_examples = []
    for a1, a2 in pair_list:
        o1, o2 = parse_off(a1), parse_off(a2)
        w = k1.get(o1, NO_KEY)
        x = k2.get(o2, NO_KEY)
        # funcs with no strings won't be in keys map -> NO_KEY default
        alive = (w is not NO_KEY) and (x is not NO_KEY) and (w == x)
        if alive:
            n_surv += 1
            nuniq = len(set(w)) if isinstance(w, tuple) else 1
            if nuniq <= 1:
                surv_single += 1
            else:
                surv_multi += 1
            if len(surv_examples) < 60:
                surv_examples.append([a1, a2, list(w) if isinstance(w, tuple) else str(w)])
    return {"total": n_total, "survivors": n_surv, "killed": n_total - n_surv,
            "survived_via_unique_single_string": surv_single,
            "survived_via_multi_string": surv_multi,
            "survivor_examples": surv_examples}


def check_specific(pairs_named, k1, k2):
    """pairs_named: list of (label, a1, a2). Returns label->killed/survived."""
    out = {}
    for label, a1, a2 in pairs_named:
        o1, o2 = parse_off(a1), parse_off(a2)
        w = k1.get(o1, NO_KEY)
        x = k2.get(o2, NO_KEY)
        alive = (w is not NO_KEY) and (x is not NO_KEY) and (w == x)
        out[label] = {"survived": bool(alive),
                      "wii_key": (list(w) if isinstance(w, tuple) else None),
                      "xenon_key": (list(x) if isinstance(x, tuple) else None)}
    return out


SUBMODE_B = [
    ("ui_trigger_complete", "0x807faf00", "0x827ffbf8"),
    ("component_scroll_start", "0x807d7630", "0x827d2588"),
    ("component_scroll_select", "0x807ae8c0", "0x827f7cc0"),
]
JUDGED_WRONG_SRH = [
    ("LayerDir", "0x805886d0", "0x82313e70"), ("EndingBonus", "0x80584390", "0x8226a848"),
    ("LightHue", "0x808523f0", "0x82497800"), ("BandList", "0x8051c320", "0x82328da8"),
    ("BandTrack", "0x805f0d80", "0x8233c160"), ("MidiInstrument", "0x8099ed70", "0x826de188"),
    ("VocalTrackDir", "0x805e49d0", "0x8226abc8"), ("CamShot", "0x8080ab50", "0x824aa4c8"),
    ("RndPartLauncher", "0x8090f7f0", "0x823fb208"), ("ParallelGroupSeq", "0x809ad080", "0x826de108"),
    ("VocalTrainerPanel", "0x8021b900", "0x82659a98"), ("WorldReflection", "0x80858f90", "0x82497880"),
    ("UIPanel", "0x8014d590", "0x826abf20"), ("EventTrigger", "0x808a49c0", "0x824884d0"),
    ("EditSetlistPanel", "0x802be2a0", "0x82557e50"), ("WorldCrowd", "0x80820880", "0x82497700"),
    ("StreakMeter", "0x805cdca0", "0x8226a8c8"), ("WorldInstance", "0x80874260", "0x82497980"),
    ("RandomIntervalGroupSeq", "0x809ad460", "0x826e7900"), ("LightPreset", "0x808389d0", "0x82497780"),
    ("user_login", "0x802dbd30", "0x823d9940"), ("RndMeshAnim", "0x808e6690", "0x823fb288"),
    ("TrackPanelDir", "0x805fc820", "0x8226ac48"),
]
JUDGED_WRONG_STU = [
    ("RndParticleSys", "0x80902ac0", "0x823fb188"),
    ("InstrumentDifficultyDisplay", "0x805bac90", "0x82310fa8"),
]

result = {
    "srh": survivors(pairs["srh"], keys1, keys2),
    "stu": survivors(pairs["stu"], keys1d, keys2d),
    "submode_b_check": check_specific(SUBMODE_B, keys1, keys2),
    "judged_wrong_srh_check": check_specific(JUDGED_WRONG_SRH, keys1, keys2),
    "judged_wrong_stu_check": check_specific(JUDGED_WRONG_STU, keys1d, keys2d),
}
json.dump(result, open(OUT, "w"), indent=1)
summary = {k: {kk: vv for kk, vv in v.items() if kk != "survivor_examples"}
           for k, v in result.items() if k in ("srh", "stu")}
summary["submode_b_survived"] = sum(1 for v in result["submode_b_check"].values() if v["survived"])
summary["judged_wrong_srh_survived"] = sum(1 for v in result["judged_wrong_srh_check"].values() if v["survived"])
summary["judged_wrong_stu_survived"] = sum(1 for v in result["judged_wrong_stu_check"].values() if v["survived"])
print(json.dumps(summary, indent=1))
print(f"[surv] wrote {OUT}", file=sys.stderr)
