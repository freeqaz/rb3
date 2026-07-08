#!/usr/bin/env python3
"""smoke_boot.py — single-boot plumbing check for Lane W (R4-M4).

Runs ONE eng_hot capture under the full measurement env
(RB3_FIXED_CLOCK + RB3_LOAD_DETERMINISM + RB3_LOADDET_ATTRIB + RB3_BOOTRNG_PROBE
+ RB3_WASH_PROBE, optional RB3_VENUE_WHITE_GUARD) and reports whether every marker
the WHITE re-grade + wash instrument depend on actually fires in the boot log:

  - [LOADDET] anchor / reseed / frame / attrib / complete   (ledger substrate)
  - [BOOTRNG] LIGHTVAL                                       (swept-light phase)
  - [WASH] ... engaged                                       (guard branch entry)
  - attrib InitParticle/CreateParticles present             (per-FX emission)
  - captured PNG + hi_frac                                  (wash score)

Usage: smoke_boot.py --bin <rb3-native> [--guard]
"""
import argparse, importlib.util, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
NSCR = os.path.join(REPO, "scripts", "native")
WF = os.path.join(REPO, "docs", "native", "engine-arch-review-2026-07-05",
                  "execution", "WHITE-fix")


def _load(m, p):
    s = importlib.util.spec_from_file_location(m, p)
    x = importlib.util.module_from_spec(s); s.loader.exec_module(x); return x


wm = _load("washmeasure", os.path.join(NSCR, "wash-measure.py"))
ws = _load("wash_score", os.path.join(NSCR, "wash_score.py"))
wd = _load("white_discriminate", os.path.join(WF, "white_discriminate.py"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--guard", action="store_true")
    ap.add_argument("--songms", type=float, default=21000.0)
    ap.add_argument("--tol", type=float, default=2000.0)
    ap.add_argument("--raws", default="/tmp/r4m4-smoke")
    a = ap.parse_args()
    os.makedirs(a.raws, exist_ok=True)

    env = dict(wd.ARMS["eng_hot"])
    env["RB3_FIXED_CLOCK"] = "1"
    env["RB3_LOAD_DETERMINISM"] = "1"
    env["RB3_LOADDET_ATTRIB"] = "1"
    env["RB3_LOADDET_JITTER"] = "200"
    env["RB3_BOOTRNG_PROBE"] = "1"
    env["RB3_WASH_PROBE"] = "1"
    if a.guard:
        env["RB3_VENUE_WHITE_GUARD"] = "1"

    lo, hi = a.songms - a.tol, a.songms + a.tol
    pref = os.path.join(a.raws, "smoke" + ("_guard" if a.guard else ""))
    print(f"booting eng_hot guard={'ON' if a.guard else 'OFF'} ...")
    png, info = wm.capture_pinned(a.bin, env, pref, lo, hi, hi + 6000,
                                  song_downs=4, verbose=True)
    if png is None:
        print(f"CAPTURE FAILED: {info}")
        return 1
    log = pref + ".engine.log"
    txt = open(log, errors="replace").read()

    def count(pat):
        return len(re.findall(pat, txt))

    m = ws.score_image(png)
    print("\n==== MARKER CHECK ====")
    checks = {
        "LOADDET anchor": count(r"\[LOADDET\] anchor "),
        "LOADDET reseed": count(r"\[LOADDET\] reseed "),
        "LOADDET frame": count(r"\[LOADDET\] frame="),
        "LOADDET attrib": count(r"\[LOADDET\] attrib "),
        "LOADDET complete": count(r"\[LOADDET\] complete "),
        "attrib InitParticle": count(r"attrib .*InitParticle"),
        "attrib CreateParticles": count(r"attrib .*CreateParticles"),
        "BOOTRNG LIGHTVAL": count(r"\[BOOTRNG\] LIGHTVAL "),
        "WASH engaged": count(r"\[WASH\].*engaged"),
    }
    for k, v in checks.items():
        print(f"  {'OK ' if v > 0 else 'MISS'} {k:26s} = {v}")
    print(f"\ncaptured songMs={info:.0f} class={m['wash_class']} "
          f"hi_frac={m['hi_frac']:.2f} mean_luma={m['mean_luma']:.3f}")
    print(f"log: {log}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
