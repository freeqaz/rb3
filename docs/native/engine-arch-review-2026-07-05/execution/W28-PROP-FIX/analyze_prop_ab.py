#!/usr/bin/env python3
"""W28-PROP-FIX A/B analyzer (committed per E6 lesson: quoted numbers must be
computed by a committed script, never by hand).

Parses one or more prop-probe stderr logs and, per ikhand, reports:
  - [IK_CLAMP] skip / clamp mode counts  (RB3_IK_CLAMP_DBG)
  - [PROP_DST] count of dst_from_hand entries (>30u; the probe only logs >30u)
    plus the median dst_from_hand.

Acceptance (A8): flag-ON run must have, for strum/fret/right_hand ikhands,
skip == 0 AND zero [PROP_DST] entries (all dropped below the 30u probe floor).

Usage: analyze_prop_ab.py LABEL=path.log [LABEL2=path2.log ...]
"""
import re, sys, statistics

CLAMP_RE = re.compile(
    r"\[IK_CLAMP\] ikhand='([^']*)' preDist=([\d.]+) reach=([\d.]+) mode=(\w+)")
DST_RE = re.compile(
    r"\[PROP_DST\] ikhand='([^']*)' finger=\d+ dst_from_hand=([\d.]+) reach=([\d.]+)")
FOCUS = ("strum.ikhand", "fret.ikhand", "right_hand.ikhand")


def parse(path):
    clamp = {}   # ikhand -> {"skip": n, "clamp": n, "preDist": [..]}
    dst = {}     # ikhand -> [dst_from_hand, ..]
    with open(path, "r", errors="replace") as f:
        for line in f:
            m = CLAMP_RE.search(line)
            if m:
                ik, pre, _reach, mode = m.group(1), float(m.group(2)), m.group(3), m.group(4)
                d = clamp.setdefault(ik, {"skip": 0, "clamp": 0, "preDist": []})
                d[mode] = d.get(mode, 0) + 1
                d["preDist"].append(pre)
                continue
            m = DST_RE.search(line)
            if m:
                ik, dd = m.group(1), float(m.group(2))
                dst.setdefault(ik, []).append(dd)
    return clamp, dst


def med(xs):
    return statistics.median(xs) if xs else None


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    reports = []
    for arg in sys.argv[1:]:
        label, _, path = arg.partition("=")
        clamp, dst = parse(path)
        reports.append((label, path, clamp, dst))

    for label, path, clamp, dst in reports:
        print(f"=== {label}  ({path}) ===")
        iks = sorted(set(list(clamp) + list(dst)),
                     key=lambda k: (k not in FOCUS, k))
        print(f"{'ikhand':<22} {'skip':>5} {'clamp':>6} {'preDist_med':>12} "
              f"{'dst_n':>6} {'dst_med':>8}")
        for ik in iks:
            c = clamp.get(ik, {})
            skip = c.get("skip", 0)
            clmp = c.get("clamp", 0)
            pm = med(c.get("preDist", []))
            dl = dst.get(ik, [])
            dm = med(dl)
            focus = "*" if ik in FOCUS else " "
            print(f"{focus}{ik:<21} {skip:>5} {clmp:>6} "
                  f"{('%.1f' % pm) if pm is not None else '-':>12} "
                  f"{len(dl):>6} {('%.1f' % dm) if dm is not None else '-':>8}")

    # Acceptance verdict on the LAST report (the flag-ON one by convention).
    label, path, clamp, dst = reports[-1]
    ok = True
    reasons = []
    for ik in FOCUS:
        skip = clamp.get(ik, {}).get("skip", 0)
        ndst = len(dst.get(ik, []))
        if skip != 0:
            ok = False; reasons.append(f"{ik}: skip={skip} (want 0)")
        if ndst != 0:
            ok = False; reasons.append(f"{ik}: {ndst} dst>30u entries (want 0)")
    print()
    print(f"ACCEPTANCE ({label}): {'PASS' if ok else 'FAIL'}")
    for r in reasons:
        print(f"  - {r}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
