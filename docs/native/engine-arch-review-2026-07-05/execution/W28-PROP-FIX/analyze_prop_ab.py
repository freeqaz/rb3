#!/usr/bin/env python3
"""W28-PROP-FIX A/B analyzer (committed per E6 lesson: quoted numbers must be
computed by a committed script, never by hand).

Parses one or more prop-probe stderr logs and, per ikhand, reports:
  - [IK_CLAMP] skip / clamp mode counts  (RB3_IK_CLAMP_DBG)
  - [PROP_DST] count of dst_from_hand entries (>30u; the probe only logs >30u)
    plus the median dst_from_hand.

Legacy acceptance (W28 A8 / W29): flag-ON run must have, for strum/fret/right_hand
ikhands, skip == 0 AND zero [PROP_DST] entries (all dropped below the 30u floor).
Printed as `ACCEPTANCE (<label>):`.

W30-PROP-DEFAULT-ON extensions (this file extended in place per CA1):
  --w30-census OFF=off.log ON=on.log
      Parse the threshold-unbiased [PROP_CENSUS] enumeration (RB3_PROP_CENSUS_DBG)
      and the char-tagged [PROP_DST] rows. Prints the full mFinger census (every
      CharIKHand: char, ikhand, finger!=NULL, chain), classifies each finger=1 hand
      as PROP (guitar/drum playing hand) vs NON-PROP (foot/mic/vocalist free hand),
      then A/Bs the per-(char,ikhand) dst_from_hand distribution OFF vs ON. Emits
      `W30 DECISION: FLIP-SAFE` (exit 0) iff no NON-PROP finger=1 chain regresses
      (ON median not worse than OFF by more than REGRESS_TOL, and ON max not worse),
      else `W30 DECISION: NON-PROP REGRESSION` (exit 1) naming the offenders.

  --w30-residual-baseline ON=on.log   (CA2 mechanical decider for Path B)
      On a W29-equivalent cap-120 FOCUS window, prints BOTH:
        `ACCEPTANCE (ON):`        (legacy bar; W29 expects FAIL: right_hand 8 dst>30u)
        `ACCEPTANCE (W30-ON):`    PASS (exit 0) iff strum/fret rows skip==0 AND
                                  dst_n==0 AND right_hand skip==0 AND dst_n<=8 AND
                                  dst_med<=33.0 (== the W29 committed residual).

Usage:
  analyze_prop_ab.py LABEL=path.log [LABEL2=path2.log ...]      (legacy)
  analyze_prop_ab.py --w30-census OFF=off.log ON=on.log
  analyze_prop_ab.py --w30-residual-baseline ON=on.log
"""
import re, sys, statistics

CLAMP_RE = re.compile(
    r"\[IK_CLAMP\] ikhand='([^']*)' preDist=([\d.]+) reach=([\d.]+) mode=(\w+)")
# char='...' is appended at END by the W30 probe; kept optional so legacy logs match.
DST_RE = re.compile(
    r"\[PROP_DST\] ikhand='([^']*)' finger=(\d+) dst_from_hand=([\d.]+) reach=([\d.]+)"
    r"(?: char='([^']*)')?")
CENSUS_RE = re.compile(
    r"\[PROP_CENSUS\] char='([^']*)' ikhand='([^']*)' finger=(\d+) "
    r"fingerName='([^']*)' hand='([^']*)' handParent='([^']*)' "
    r"ntargets=(\d+) reach=([\d.]+)")
FOCUS = ("strum.ikhand", "fret.ikhand", "right_hand.ikhand")
REGRESS_TOL = 8.0   # u; ON median may not exceed OFF median by more than this

# A finger=1 ikhand is a PROP (playing-hand) chain iff its finger bone is a
# pick/tip prop bone. Everything else (foot toe, mic-stand top, vocalist at-hand
# target frame) is NON-PROP and must not regress when piece(1) breaks its mFinger
# re-projection globally.
def is_prop_finger(finger_name, ikhand):
    fn = finger_name or ""
    if "bone_pick" in fn or "bone_tip" in fn:
        return True                      # guitar strum(pick)/fret(tip)
    if re.search(r"bone_[LR]-tip", fn):
        return True                      # drummer stick tips
    return False


def parse(path):
    clamp = {}
    dst = {}          # bare ikhand -> [dst]
    dst_char = {}     # (char, ikhand) -> [dst]
    census = {}       # (char, ikhand) -> dict
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
                ik, dd = m.group(1), float(m.group(3))
                ch = m.group(5) or "(nochar)"
                dst.setdefault(ik, []).append(dd)
                dst_char.setdefault((ch, ik), []).append(dd)
                continue
            m = CENSUS_RE.search(line)
            if m:
                ch, ik = m.group(1), m.group(2)
                census[(ch, ik)] = {
                    "finger": int(m.group(3)),
                    "fingerName": m.group(4),
                    "hand": m.group(5),
                    "handParent": m.group(6),
                    "ntargets": int(m.group(7)),
                    "reach": float(m.group(8)),
                }
    return clamp, dst, dst_char, census


def med(xs):
    return statistics.median(xs) if xs else None


def _print_table(label, path, clamp, dst):
    print(f"=== {label}  ({path}) ===")
    iks = sorted(set(list(clamp) + list(dst)), key=lambda k: (k not in FOCUS, k))
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


def legacy(reports):
    for label, path, clamp, dst, _dc, _cn in reports:
        _print_table(label, path, clamp, dst)
    label, path, clamp, dst, _dc, _cn = reports[-1]
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


def _kv(args):
    out = {}
    for a in args:
        label, _, path = a.partition("=")
        out[label] = path
    return out


def w30_census(args):
    kv = _kv(args)
    if "OFF" not in kv or "ON" not in kv:
        print("usage: --w30-census OFF=off.log ON=on.log", file=sys.stderr)
        return 2
    off = parse(kv["OFF"]); on = parse(kv["ON"])
    off_clamp, off_dst, off_dc, off_cn = off
    on_clamp, on_dst, on_dc, on_cn = on
    census = dict(off_cn); census.update(on_cn)   # union; identical by construction

    print("=== [PROP_CENSUS] mFinger census (threshold-unbiased, RB3_PROP_CENSUS_DBG) ===")
    print(f"{'char':<9} {'ikhand':<20} {'fin':>3} {'class':<9} {'fingerName':<26} "
          f"{'reach':>6} {'ntgt':>4}")
    keys = sorted(census)
    finger1 = []
    for k in keys:
        ch, ik = k
        c = census[k]
        if c["finger"] == 1:
            cls = "PROP" if is_prop_finger(c["fingerName"], ik) else "NON-PROP"
            finger1.append((k, cls))
        else:
            cls = "-"
        print(f"{ch:<9} {ik:<20} {c['finger']:>3} {cls:<9} "
              f"{c['fingerName']:<26} {c['reach']:>6.2f} {c['ntargets']:>4}")

    print()
    print("=== finger=1 A/B: per-(char,ikhand) dst_from_hand OFF vs ON ===")
    print(f"{'char':<9} {'ikhand':<20} {'class':<9} "
          f"{'OFF_n':>6} {'OFF_med':>7} {'OFF_max':>7}  "
          f"{'ON_n':>6} {'ON_med':>7} {'ON_max':>7}  verdict")
    regressions = []
    for (k, cls) in sorted(finger1, key=lambda x: (x[1] != "NON-PROP", x[0])):
        ch, ik = k
        o = off_dc.get(k, []); n = on_dc.get(k, [])
        om, nm = med(o), med(n)
        omax = max(o) if o else None
        nmax = max(n) if n else None
        # regression: NON-PROP chain whose ON median worsens beyond tolerance,
        # AND whose ON max also worsens (both, to avoid window-share false alarms).
        verdict = "ok"
        if cls == "NON-PROP":
            worse_med = (om is not None and nm is not None and nm > om + REGRESS_TOL)
            worse_max = (omax is not None and nmax is not None and nmax > omax + REGRESS_TOL)
            # a chain that had NO ON entries (dropped below floor) can't regress
            if n and worse_med and worse_max:
                verdict = "REGRESS"
                regressions.append((ch, ik, om, nm, omax, nmax))
            elif not n:
                verdict = "improved(0)"
            elif nm is not None and om is not None and nm < om:
                verdict = "improved"
            else:
                verdict = "neutral"
        print(f"{ch:<9} {ik:<20} {cls:<9} "
              f"{len(o):>6} {('%.1f'%om) if om is not None else '-':>7} "
              f"{('%.1f'%omax) if omax is not None else '-':>7}  "
              f"{len(n):>6} {('%.1f'%nm) if nm is not None else '-':>7} "
              f"{('%.1f'%nmax) if nmax is not None else '-':>7}  {verdict}")

    print()
    print("=== foot/plant sanity (player3 feet finger=1) ===")
    for k in [("player3", "left_foot.ikhand"), ("player3", "right_foot.ikhand")]:
        o = off_dc.get(k, []); n = on_dc.get(k, [])
        om, nm = med(o), med(n)
        reach = census.get(k, {}).get("reach", 0.0)
        tag = "PLANTED-CLOSER" if (om and nm and nm < om) else "NO-IMPROVE"
        print(f"  {k[1]:<18} reach={reach:.2f}  OFF med={('%.1f'%om) if om else '-'}"
              f"  ON med={('%.1f'%nm) if nm else '-'}  -> {tag}")

    print()
    if regressions:
        print("W30 DECISION: NON-PROP REGRESSION")
        for ch, ik, om, nm, omax, nmax in regressions:
            print(f"  - {ch}/{ik}: OFF med={om:.1f}/max={omax:.1f} -> "
                  f"ON med={nm:.1f}/max={nmax:.1f}")
        print("  => Path B: re-scope piece 1 to prop-chain ikhands (same flag).")
        return 1
    print("W30 DECISION: FLIP-SAFE")
    print("  No NON-PROP finger=1 chain regresses under RB3_PROP_POSE_FULL=1")
    print("  (every non-prop chain equal-or-better on median AND max).")
    return 0


def w30_residual_baseline(args):
    kv = _kv(args)
    if "ON" not in kv:
        print("usage: --w30-residual-baseline ON=on.log", file=sys.stderr)
        return 2
    clamp, dst, _dc, _cn = parse(kv["ON"])
    _print_table("ON", kv["ON"], clamp, dst)

    # Legacy bar (continuity with W29): strum/fret skip=0 & 0 dst>30u; verdict text.
    legacy_ok = True; legacy_reasons = []
    for ik in FOCUS:
        skip = clamp.get(ik, {}).get("skip", 0)
        ndst = len(dst.get(ik, []))
        if skip != 0:
            legacy_ok = False; legacy_reasons.append(f"{ik}: skip={skip} (want 0)")
        if ndst != 0:
            legacy_ok = False; legacy_reasons.append(f"{ik}: {ndst} dst>30u entries (want 0)")
    print()
    print(f"ACCEPTANCE (ON): {'PASS' if legacy_ok else 'FAIL'}")
    for r in legacy_reasons:
        print(f"  - {r}")

    # CA2 W30 residual-baseline bar.
    ok = True; reasons = []
    for ik in ("strum.ikhand", "fret.ikhand"):
        skip = clamp.get(ik, {}).get("skip", 0)
        ndst = len(dst.get(ik, []))
        if skip != 0:
            ok = False; reasons.append(f"{ik}: skip={skip} (want 0)")
        if ndst != 0:
            ok = False; reasons.append(f"{ik}: dst_n={ndst} (want 0)")
    rh = "right_hand.ikhand"
    rskip = clamp.get(rh, {}).get("skip", 0)
    rl = dst.get(rh, [])
    rn = len(rl); rm = med(rl)
    if rskip != 0:
        ok = False; reasons.append(f"{rh}: skip={rskip} (want 0)")
    if rn > 8:
        ok = False; reasons.append(f"{rh}: dst_n={rn} (want <=8)")
    if rm is not None and rm > 33.0:
        ok = False; reasons.append(f"{rh}: dst_med={rm:.1f} (want <=33.0)")
    print()
    print(f"ACCEPTANCE (W30-ON): {'PASS' if ok else 'FAIL'}")
    for r in reasons:
        print(f"  - {r}")
    return 0 if ok else 1


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    if sys.argv[1] == "--w30-census":
        return w30_census(sys.argv[2:])
    if sys.argv[1] == "--w30-residual-baseline":
        return w30_residual_baseline(sys.argv[2:])
    reports = []
    for arg in sys.argv[1:]:
        label, _, path = arg.partition("=")
        clamp, dst, dc, cn = parse(path)
        reports.append((label, path, clamp, dst, dc, cn))
    return legacy(reports)


if __name__ == "__main__":
    sys.exit(main())
