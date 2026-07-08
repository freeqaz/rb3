#!/usr/bin/env python3
"""iso_ledger_gate.py — Wave-19 Lane I (W-ISO) EXIT gate.

Captures N=10 OFF-arm eng_hot boots under the R4 determinism seam and grades their
OWN logs (A2/F6 VOID discipline — never discard-and-rerun a failing boot) to test the
BINDING EXIT: the eng_hot OFF-arm ledger STREAM axis reaches 10/10, i.e. the four
venue-path consumer isolations remove the boot-varying draw counts from the shared
gRand stream so the post-anchor stream position becomes boot-invariant. This makes the
Wave-18 WHITE re-grade's VOID precondition satisfiable.

WHY A LANE SCRIPT, NOT `white_regrade --validate`: --validate caps N at 3
(white_regrade.py) and grades only cleanliness; EXIT needs N=10. This script REUSES
white_regrade.capture_arm (eng_hot + seam env, RB3_LOADDET_ATTRIB=1) and
loaddet_gate.grade_external_logs UNCHANGED (both imported as modules, the _load
pattern — NO edits to loaddet_gate.py, which is Lane F's file this wave).

PASS (PLAN_REVIEW AM-2a, BINDING): grade_external_logs(...)["summary"]["stream"] ==
"N/N" AND nParsed == N. The stream axis passes on boot-INVARIANCE vs the reference boot
(loaddet_gate.py:367, `s == ref["postAnchorDelta"]`), NOT on postAnchorDelta == 0. Each
boot's axes.stream.value is RECORDED; a nonzero but boot-INVARIANT constant is
informational (charter satisfied), NOT a gate failure and NOT an R-A trigger.

NAMING BOX (F9): this gate measures per-consumer stream-POSITION count invariance
(frame-assignment count variance). It does NOT touch R4's ledger `order` axis (10/10).

R-A (PLAN §3 + AM-3): on FAIL, --attrib re-attributes the failing boots' OWN logs over
the post-anchor window, SUBTRACTING the union of already-isolated site PCs (the attrib
tap fires BEFORE the redirect, Rand.cpp:186-212, so isolated draws are still fully
per-PC-counted). One bounded iteration; past that the residual is priced to Lane T1.
"""
import argparse
import importlib.util
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
NSCR = os.path.join(REPO, "scripts", "native")
R4M4 = os.path.join(REPO, "docs", "native", "engine-arch-review-2026-07-05",
                    "execution", "R4-M4")

# The union of all sites already redirected onto private per-tag streams. Their PCs
# still appear in the attrib table (tap fires BEFORE redirect) but no longer reach
# gRand, so R-A iteration 2 MUST subtract them before reading the residual (AM-3).
#   4 new (this lane): CharClipDriver ctor, WorldCrowd::OnIterateFrac,
#                      CharInterest::ComputeScore, LightPresetManager::PickRandomPreset
#   5 R4-M2:           Part.cpp x2, CameraShot.cpp, Sequence.cpp, CharEyes.cpp
ISOLATED_SYM_SUBSTR = [
    "CharClipDriver", "OnIterateFrac", "ComputeScore", "PickRandomPreset",  # 4 new
    "Part", "CameraShot", "Sequence", "CharEyes",                            # 5 R4-M2
]

K_FRAMES = 300


def _load(m, p):
    s = importlib.util.spec_from_file_location(m, p)
    x = importlib.util.module_from_spec(s)
    s.loader.exec_module(x)
    return x


wr = _load("white_regrade", os.path.join(R4M4, "white_regrade.py"))
lg = _load("loaddet_gate", os.path.join(NSCR, "loaddet_gate.py"))


def run_gate(binpath, n, songms, overshoot_ms, rawdir, tag):
    """Capture n OFF-arm eng_hot boots under the seam; grade their OWN logs."""
    os.makedirs(rawdir, exist_ok=True)
    overshoot = songms + overshoot_ms
    # capture_arm returns (rows, disclosure) after the W-ISO M2 wiring.
    rows, disclosure = wr.capture_arm(binpath, False, n, songms, overshoot, rawdir, tag)
    log_paths = [r["log"] for r in rows]
    return grade_logs(log_paths, n, binpath, songms, disclosure, tag)


def grade_logs(log_paths, n, binpath, songms, disclosure, tag):
    """Grade a set of already-produced boot logs (A2/F6: their OWN logs). Offline —
    no re-boot; used both live (from run_gate) and to re-grade captured logs."""
    led = lg.grade_external_logs(log_paths, K_FRAMES)
    if "error" in led or "boots" not in led:
        return {"tag": tag, "bin": binpath, "n": n, "songms": songms,
                "attempt_disclosure": disclosure, "PASS": False,
                "stream": "ERROR", "nParsed": led.get("nParsed", 0),
                "nLogs": len(log_paths), "per_boot_stream_value": [],
                "distinct_stream_values": [], "boot_invariant_nonzero_residual": False,
                "log_paths": log_paths, "ledger_error": led.get("error", "no boots"),
                "summary": led.get("summary", {})}
    # The ledger's "boots" rows are already ok-filtered inside ledger_for_arm.
    per_boot_stream = [b["axes"]["stream"]["value"] for b in led["boots"]]
    stream = led["summary"]["stream"]
    nParsed = led["nParsed"]
    # AM-2a: PASS = stream N/N AND nParsed N. NOT postAnchorDelta==0.
    passed = (stream == f"{n}/{n}") and (nParsed == n)
    distinct = sorted(set(per_boot_stream))
    invariant_nonzero = (len(distinct) == 1 and distinct[0] != 0)
    return {
        "tag": tag, "bin": binpath, "n": n, "songms": songms,
        "attempt_disclosure": disclosure,
        "nParsed": nParsed, "nLogs": led["nLogs"],
        "stream": stream,
        "per_boot_stream_value": per_boot_stream,
        "distinct_stream_values": distinct,
        "PASS": passed,
        "boot_invariant_nonzero_residual": invariant_nonzero,
        "log_paths": log_paths,
        "summary": led.get("summary", {}),
    }


def ra_attribution(binpath, log_paths, anchor_window=K_FRAMES):
    """R-A iteration 2 (AM-3): re-attribute the failing boots' OWN logs over the
    post-anchor window, then SUBTRACT the union of already-isolated site PCs. Returns
    the residual (non-isolated) consumer rows — the ONLY candidates for a further
    guard. If empty, the residual is not a per-consumer stream-position axis -> price
    to Lane T1 (STOP; no third guard round)."""
    boots = [lg.parse_boot_log(lp, K_FRAMES, attrib=True) for lp in log_paths]
    ok = [b for b in boots if b["ok"]]
    if not ok:
        return {"error": "no parseable boots", "residual_rows": []}
    # per-boot window totals over [anchor, anchor+K]
    off_boots = []
    for b in ok:
        lo = b["anchorFrame"]
        hi = lo + anchor_window
        off_boots.append(lg.window_totals(b, lo, hi))
    tbl = lg.attribution_table(binpath, off_boots, len(ok), 0, anchor_window,
                               "post-anchor")
    residual = []
    for r in tbl["rows"]:
        sym = r.get("sym", "")
        if any(sub in sym for sub in ISOLATED_SYM_SUBSTR):
            continue  # AM-3: already isolated; drop before reading the residual
        if r["variable"]:  # only boot-varying consumers matter for the stream axis
            residual.append(r)
    return {
        "nBoots": len(ok),
        "isolated_filtered": ISOLATED_SYM_SUBSTR,
        "residual_rows": residual,
        "verdict": ("CLEAN (no non-isolated variable consumer -> price to Lane T1)"
                    if not residual else
                    "RESIDUAL FOUND -> isolate in one bounded M3 round"),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--n", type=int, default=10)
    ap.add_argument("--songms", type=float, default=21000.0)
    ap.add_argument("--overshoot", type=float, default=4000.0)
    ap.add_argument("--raws", default="/tmp/wiso-iso-ledger")
    ap.add_argument("--tag", default="iso")
    ap.add_argument("--out", required=True, help="output JSON path")
    ap.add_argument("--attrib", action="store_true",
                    help="on FAIL, run the bounded R-A attribution (AM-3 subtraction)")
    ap.add_argument("--regrade-logs", nargs="+", default=None,
                    help="offline: grade these already-produced boot logs (no re-boot, "
                         "A2/F6 their-own-logs discipline). Skips capture.")
    a = ap.parse_args()

    if a.regrade_logs:
        disc = {"attempts": None, "captured": len(a.regrade_logs),
                "discarded": None, "discarded_reasons": ["offline-regrade"]}
        res = grade_logs(a.regrade_logs, a.n, a.bin, a.songms, disc, a.tag)
    else:
        res = run_gate(a.bin, a.n, a.songms, a.overshoot, a.raws, a.tag)

    if (not res["PASS"]) and a.attrib:
        res["ra_attribution"] = ra_attribution(a.bin, res["log_paths"])

    with open(a.out, "w") as f:
        json.dump(res, f, indent=2, allow_nan=False)

    print("==================== W-ISO ISO-LEDGER GATE ====================")
    print(f"bin={a.bin}")
    print(f"n={a.n} nParsed={res['nParsed']}/{res['nLogs']} stream={res['stream']}")
    print(f"per-boot stream values (postAnchorDelta): {res['per_boot_stream_value']}")
    print(f"distinct={res['distinct_stream_values']} "
          f"boot_invariant_nonzero={res['boot_invariant_nonzero_residual']}")
    print(f"attempt_disclosure={res['attempt_disclosure']}")
    print(f"PASS(stream {a.n}/{a.n} AND nParsed {a.n}): {res['PASS']}")
    if "ra_attribution" in res:
        print(f"R-A: {res['ra_attribution']['verdict']} "
              f"(residual_rows={len(res['ra_attribution']['residual_rows'])})")
    print(f"wrote {a.out}")
    return 0 if res["PASS"] else 1


if __name__ == "__main__":
    sys.exit(main())
