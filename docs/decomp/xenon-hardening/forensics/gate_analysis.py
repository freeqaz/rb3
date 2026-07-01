#!/usr/bin/env python3
"""Failure-mode taxonomy + per-gate would-have-been precision for the Wii<->Xenon
ghidriff run. Pure-python, reads ONLY existing artifacts:
  - eval_report.json          (judged holdout + dc3 high-conf pairs w/ verdicts)
  - <run>/json/*.matches.json (all 2645 pairs w/ names + match_types)
  - strkeys_out.json          (per-matched-func string multiset + size; produced
                               by recompute_strkeys.py, a read-only pyghidra pass)
  - string_uniqueness_out.json(global per-string func-ref counts for the 26 wrong
                               SRH key strings; produced by string_global_uniqueness.py)
  - the Wii CodeWarrior map    (func size, for the VT gate)

Run:
  python3 gate_analysis.py
All four input JSONs live next to this script (forensics/) except the run's
matches.json + the map, which are referenced by absolute path below.
"""
import json
import re
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
RB3 = HERE.parents[3]
RUN = RB3 / "build/SZBE69_B8/ghidra/ghidriff-xenon"
MATCHES = RUN / "json/bank8_target.elf-42264e.gzf-rb3_xenon_default_xex.gzf.ghidriff.matches.json"
EVAL = RUN / "eval_report.json"
MAP = RB3 / "orig/SZBE69_B8/files/band_r_wii.map"

sk = json.load(open(HERE / "strkeys_out.json"))
uniq = json.load(open(HERE / "string_uniqueness_out.json"))
rep = json.load(open(EVAL))


def pa(a):
    a = a.lower()
    return int(a[2:], 16) if a.startswith("0x") else int(a, 16)


def wii_sizes():
    sizes = {}
    line = re.compile(r"^\s+([0-9a-f]{8})\s+([0-9a-f]{6})\s+([0-9a-f]{8})")
    in_code = False
    for ln in open(MAP, errors="replace"):
        s = ln.rstrip("\n")
        if s.endswith("section layout"):
            in_code = s.startswith((".text", ".init"))
            continue
        if not in_code:
            continue
        m = line.match(s)
        if m:
            sizes[int(m.group(3), 16)] = int(m.group(2), 16)
    return sizes


SIZES = wii_sizes()


def is_stl(sym):
    return any(x in (sym or "") for x in
               ("ObjVector", "_M_", "stlpmtx", "_Vector_impl", "KeylessHash",
                "push_back", "resize", "__as__", "__ct__", "__dt__"))


# ---------------------------------------------------------------------------
# SRH judged set (all dc3 high_conf; SRH had 0 holdout entries)
# ---------------------------------------------------------------------------
def srh_rows():
    rows = []
    for r in rep["lists"]["dc3"]:
        if not (r.get("high_conf") and "StringsRefsHasher" in r["match_types"]
                and r["verdict"] in ("agree", "disagree")):
            continue
        p1 = hex(pa(r["wii_addr"]))
        p2 = hex(pa(r["xenon_addr"]))
        d1 = sk["p1"].get(p1, {})
        ks = [s[4:].strip('"') for s in (d1.get("strings") or [])]  # 'ds "X"' -> X
        wmult = max((len(uniq["wii"].get(s, [1])) for s in ks), default=1)
        xmult = max((len(uniq["xenon"].get(s, [1])) for s in ks), default=1)
        rows.append({"sym": r["wii_symbol"], "nuniq": d1.get("nuniq", 0),
                     "size": d1.get("size"), "wmult": wmult, "xmult": xmult,
                     "correct": r["verdict"] == "agree"})
    return rows


def vt_rows():
    rows = []
    for r in rep["lists"]["holdout"]:
        if (r.get("match_types") and "VTCombinedReference" in r["match_types"]
                and r["result"] in ("recovered_correct", "recovered_wrong")):
            rows.append({"sym": r["wii_symbol"], "dc3": None,
                         "size": SIZES.get(pa(r["wii_addr"])),
                         "correct": r["result"] == "recovered_correct"})
    for r in rep["lists"]["dc3"]:
        if (r.get("high_conf") and "VTCombinedReference" in r["match_types"]
                and r["verdict"] in ("agree", "disagree")):
            rows.append({"sym": r["wii_symbol"], "dc3": r["dc3_name"],
                         "size": SIZES.get(pa(r["wii_addr"])),
                         "correct": r["verdict"] == "agree"})
    for r in rows:
        r["stl"] = is_stl(r["sym"] or "")
    return rows


def gate(rows, name, pred):
    kept = [x for x in rows if pred(x)]
    kc = sum(x["correct"] for x in kept)
    kw = len(kept) - kc
    prec = (kc / len(kept)) if kept else None
    print(f"  {name:46s} kept={len(kept):2d} correct={kc:2d} wrong={kw:2d} "
          f"killed={len(rows)-len(kept):2d} prec={prec if prec is None else round(prec,3)}")


def main():
    srh = srh_rows()
    print(f"=== SRH judged: {len(srh)} (correct {sum(r['correct'] for r in srh)} "
          f"wrong {sum(not r['correct'] for r in srh)}) ===")
    gate(srh, "NONE (baseline)", lambda x: True)
    gate(srh, "distinctive-string-count >= 2 (nuniq>=2)", lambda x: x["nuniq"] >= 2)
    gate(srh, "1:1-unique key (wmult==1 AND xmult==1)", lambda x: x["wmult"] == 1 and x["xmult"] == 1)
    gate(srh, "exclude key strings ref by >1 wii func", lambda x: x["wmult"] <= 1)
    gate(srh, "min wii size >= 120 bytes", lambda x: (x["size"] or 0) >= 120)

    vt = vt_rows()
    print(f"\n=== VT judged: {len(vt)} (correct {sum(r['correct'] for r in vt)} "
          f"wrong {sum(not r['correct'] for r in vt)}) ===")
    gate(vt, "NONE (baseline)", lambda x: True)
    gate(vt, "exclude STL/ctor/dtor/ObjVector internals", lambda x: not x["stl"])
    gate(vt, "min wii size >= 128 bytes (HURTS VT)", lambda x: (x["size"] or 0) >= 128)
    gate(vt, "min wii size >= 256 bytes (HURTS VT)", lambda x: (x["size"] or 0) >= 256)


if __name__ == "__main__":
    sys.exit(main())
