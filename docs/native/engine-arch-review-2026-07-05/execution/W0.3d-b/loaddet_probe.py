#!/usr/bin/env python3
"""loaddet_probe.py — Wave-12 Lane A (W0.3d-b) A-S1 attribution harness.

Boots rb3-native N>=6 times headless under RB3_LOADDET_PROBE + RB3_FIXED_CLOCK,
each for a fixed MILO_MAX_FRAMES, parses the two co-registered [LOADDET] streams
from each boot's stderr, aligns the boots frame-by-frame, and reports:

  - per-frame gdraw table (cumulative gRand stream position at each frame start),
    one column per boot, so the FIRST divergent frame is visible directly;
  - the per-frame draw DELTA (draws consumed during a frame);
  - the completion events (kind=dir|data, frame, name) so a divergence can be
    named: did a loader COMPLETE on a different frame (H-TIMING) or did the same
    frame consume a different count with the same completions (something else)?

Boots run CONCURRENTLY so they mutually contend for the scheduler (the W0.3c/W0.3d
quiescent-machine trap: the async-completion flake only manifests under contention;
a quiescent run shows 1 variant and hides it). Optionally add extra busy-loops.

Usage:
  python3 loaddet_probe.py --bin native/build-agent-W0.3d-b/rb3-native \
      --n 8 --frames 120 --out /tmp/loaddet --tag as1
"""
import argparse, json, os, re, subprocess, sys, collections, statistics as st

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
DEFAULT_OVERLAY = os.path.join(REPO, "native", "dta")

FRAME_RE = re.compile(r"\[LOADDET\] frame=(\d+) gdraw=(\d+)")
COMP_RE = re.compile(r"\[LOADDET\] complete frame=(\d+) gdraw=(\d+) kind=(\S+) name=(\S+)")


def boot(binpath, frames, log_path):
    env = dict(os.environ)
    env.update({
        "RB3_LOADDET_PROBE": "1", "RB3_FIXED_CLOCK": "1", "MILO_HEADLESS": "1",
        "RB3_GAME": "1", "RB3_DATA": DEFAULT_DATA, "RB3_DTA_OVERLAY": DEFAULT_OVERLAY,
        "MILO_MAX_FRAMES": str(frames),
    })
    logf = open(log_path, "w")
    # setarch -R pins ASLR base (deterministic addresses) so any surviving
    # divergence is scheduler-timing, not address-layout.
    return subprocess.Popen(["setarch", "-R", binpath], env=env,
                            stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True), logf


def parse_log(path):
    frames = {}          # frame -> gdraw (last seen)
    comps = []           # (frame, gdraw, kind, name)
    try:
        with open(path, "r", errors="replace") as f:
            for ln in f:
                m = FRAME_RE.search(ln)
                if m:
                    frames[int(m.group(1))] = int(m.group(2))
                    continue
                m = COMP_RE.search(ln)
                if m:
                    comps.append((int(m.group(1)), int(m.group(2)),
                                  m.group(3), m.group(4)))
    except FileNotFoundError:
        pass
    return {"frames": frames, "comps": comps}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--n", type=int, default=8)
    ap.add_argument("--frames", type=int, default=120)
    ap.add_argument("--out", default="/tmp/loaddet")
    ap.add_argument("--tag", default="as1")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    # Launch all N boots concurrently (mutual scheduler contention).
    procs = []
    for i in range(args.n):
        lp = os.path.join(args.out, f"{args.tag}-boot{i}.log")
        p, lf = boot(args.bin, args.frames, lp)
        procs.append((i, p, lf, lp))
        print(f"[loaddet] launched boot {i} -> {lp}", flush=True)
    for i, p, lf, lp in procs:
        try:
            p.wait(timeout=300)
        except subprocess.TimeoutExpired:
            os.killpg(os.getpgid(p.pid), 9)
        lf.close()
        print(f"[loaddet] boot {i} exit={p.returncode}", flush=True)

    boots = [parse_log(lp) for _, _, _, lp in procs]

    # Per-frame gdraw table across boots. Restrict to frames present in ALL boots.
    common = None
    for b in boots:
        fs = set(b["frames"].keys())
        common = fs if common is None else (common & fs)
    common = sorted(common or [])

    # Find first divergent frame (gdraw differs across boots).
    first_div = None
    rows = []
    for fr in common:
        vals = [b["frames"][fr] for b in boots]
        diverged = len(set(vals)) > 1
        rows.append((fr, vals, diverged))
        if diverged and first_div is None:
            first_div = fr

    # Completion-frame map per boot: name -> set of frames it completed on.
    comp_frame_by_name = []   # per boot: {name: sorted frames}
    for b in boots:
        d = collections.defaultdict(list)
        for (fr, gd, kind, name) in b["comps"]:
            d[(kind, name)].append(fr)
        comp_frame_by_name.append(d)

    # For each (kind,name), does its completion frame vary across boots?
    all_keys = set()
    for d in comp_frame_by_name:
        all_keys |= set(d.keys())
    comp_frame_variance = {}
    for key in all_keys:
        per_boot_first = []
        for d in comp_frame_by_name:
            fr = min(d[key]) if key in d and d[key] else None
            per_boot_first.append(fr)
        distinct = set(x for x in per_boot_first if x is not None)
        comp_frame_variance[key] = {
            "per_boot_first_frame": per_boot_first,
            "distinct_frames": sorted(distinct),
            "varies": len(distinct) > 1,
        }

    # Summary numbers.
    final_gdraws = [b["frames"].get(max(common), None) for b in boots] if common else []
    gdraw_spread = (max(final_gdraws) - min(final_gdraws)) if final_gdraws and None not in final_gdraws else None

    report = {
        "tag": args.tag, "n": args.n, "frames": args.frames,
        "common_frames": [min(common), max(common)] if common else None,
        "first_divergent_frame": first_div,
        "final_gdraws": final_gdraws,
        "final_gdraw_spread": gdraw_spread,
        "n_completions_per_boot": [len(b["comps"]) for b in boots],
        "comp_frame_varies": {
            f"{k[0]}:{k[1]}": v for k, v in comp_frame_variance.items() if v["varies"]
        },
    }

    # Console report.
    print("\n================ LOADDET ATTRIBUTION ================", flush=True)
    print(f"boots={args.n} frames={args.frames} common=[{report['common_frames']}]")
    print(f"final gdraws per boot: {final_gdraws}")
    print(f"final gdraw spread: {gdraw_spread}")
    print(f"completions per boot: {report['n_completions_per_boot']}")
    print(f"FIRST DIVERGENT FRAME: {first_div}")
    if first_div is not None:
        idx = common.index(first_div)
        lo = max(0, idx - 2)
        print("\n  frame | " + " | ".join(f"boot{i}" for i in range(args.n)))
        for fr, vals, dv in rows[lo:idx + 4]:
            mark = " <== DIVERGES" if dv else ""
            print(f"  {fr:5d} | " + " | ".join(f"{v:8d}" for v in vals) + mark)
        # What completed on the first divergent frame (and the frame before) per boot?
        print("\n  completions on frames [{}..{}]:".format(first_div - 1, first_div + 1))
        for i, b in enumerate(boots):
            evs = [(fr, kind, name) for (fr, gd, kind, name) in b["comps"]
                   if first_div - 1 <= fr <= first_div + 1]
            print(f"   boot{i}: {evs}")
    print("\n  completion-frame VARIANCE (loaders that finished on different frames):")
    varies = report["comp_frame_varies"]
    if not varies:
        print("   (none — all loaders completed on identical frames across boots)")
    else:
        for name, v in sorted(varies.items(), key=lambda kv: min(f for f in kv[1]['per_boot_first_frame'] if f is not None)):
            print(f"   {name}: first-frames {v['per_boot_first_frame']} distinct={v['distinct_frames']}")

    outp = os.path.join(args.out, f"{args.tag}.json")
    with open(outp, "w") as f:
        json.dump(report, f, indent=2)
    # Also dump the full per-frame table for the STATUS evidence table.
    tblp = os.path.join(args.out, f"{args.tag}-table.json")
    with open(tblp, "w") as f:
        json.dump({"common": common,
                   "gdraw_by_frame": {fr: [b["frames"][fr] for b in boots] for fr in common}},
                  f)
    print(f"\nwrote {outp} and {tblp}", flush=True)


if __name__ == "__main__":
    main()
