#!/usr/bin/env python3
"""W27 close-out Q4: recalibrate the per-name fixedclock-residual eps for the
splash_screen drawlog golden from N clean-tree fixed-clock captures.

Ruling (WAVE27_CLOSEOUT_REVIEW.md Q4, option b): extend/raise PER-NAME eps for
field=world crowd-pose draws only — never widen the top-level eps, never
--update the golden. Count/structural/non-world checks stay strict.

Method: N captures via drawlog-golden.py's own capture_fixed_clock; each is
compared against the committed golden with compare_canonical under an EMPTY
residual so every world deviation surfaces; per golden-draw max |cand-golden|
is aggregated across runs by mesh-name-hash; new eps = ceil(2.0 x max observed)
per name (existing entries only ever raised, never lowered).
"""
import importlib.util, json, math, os, re, sys, argparse, collections

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "rb3"))
spec = importlib.util.spec_from_file_location(
    "drawlog_golden", os.path.join(os.path.dirname(os.path.abspath(__file__)), "drawlog-golden.py"))
dg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dg)

MSG_RE = re.compile(r"draw g(\d+)~c(\d+) field=world world\[(\d+)\] golden=([-\d.eE+]+) cand=([-\d.eE+]+)")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=6)
    ap.add_argument("--margin", type=float, default=2.0)
    ap.add_argument("--apply", action="store_true", help="write the updated sidecar")
    a = ap.parse_args()

    args = dg.__dict__  # namespace for capture: reuse the script's parser defaults
    p = __import__("argparse").Namespace(
        port=0, bin=dg.DEFAULT_BIN, data=dg.DEFAULT_DATA, scene="splash_screen",
        update=False, determinism_check=0, fail_red_audit=False, no_stabilize=False,
        fixed_clock=True, frames=dg.FIXED_CLOCK_FRAMES, no_aslr_off=False,
        canonical_order=True, verbose=False, keep_log=False)

    golden = json.load(open(os.path.join(dg.GOLDEN_DIR, "splash_screen.json")))
    gd = golden["draws"]
    sidecar_path = dg.residual_path("splash_screen")
    sidecar = json.load(open(sidecar_path))

    # per-name max deviation across all runs; also track count anomalies
    dev = collections.defaultdict(float)
    seen_idx = collections.defaultdict(set)
    count_bad = 0
    for r in range(a.runs):
        cand = dg.capture_fixed_clock(p, tag=f"[recal{r}]")
        if cand is None:
            print(f"run {r}: CAPTURE FAILED — aborting"); sys.exit(2)
        if cand.get("count") != golden.get("count"):
            print(f"run {r}: COUNT {cand.get('count')} != golden {golden.get('count')} — real regression, aborting")
            sys.exit(2)
        passed, allf, unexpected, expected = dg.compare_canonical(golden, cand, None)
        nworld = 0
        for m in allf:
            mm = MSG_RE.search(m)
            if not mm:
                print(f"run {r}: NON-WORLD failure (not eligible for residual): {m}")
                count_bad += 1
                continue
            gi = int(mm.group(1)); d = abs(float(mm.group(5)) - float(mm.group(4)))
            name = gd[gi].get("name")
            dev[name] = max(dev[name], d)
            seen_idx[name].add(gi)
            nworld += 1
        print(f"run {r}: {nworld} world-elem deviations across {len(set(MSG_RE.search(m).group(1) for m in allf if MSG_RE.search(m)))} draws")
    if count_bad:
        print(f"ABORT: {count_bad} non-world/structural failures observed — not a jitter class")
        sys.exit(2)

    # merge into sidecar: raise-only per-name eps
    by_name = {}
    for e in sidecar["draws"]:
        by_name.setdefault(e["name"], []).append(e)
    changed = []
    for name, mx in sorted(dev.items(), key=lambda t: -t[1]):
        want = math.ceil(mx * a.margin)
        if name in by_name:
            for e in by_name[name]:
                cur = e.get("eps", sidecar["eps"])
                if want > cur:
                    changed.append((name, cur, want)); e["eps"] = float(want)
        else:
            # new jitter name: add one entry per golden index observed deviating
            for gi in sorted(seen_idx[name]):
                sidecar["draws"].append({"index": gi, "name": name, "eps": float(want)})
            changed.append((name, None, want))
    print("\nper-name max deviation over", a.runs, "runs:")
    for name, mx in sorted(dev.items(), key=lambda t: -t[1]):
        print(f"  {name}  maxdev={mx:.3f}  -> eps {math.ceil(mx*a.margin)}  (draws {sorted(seen_idx[name])})")
    print("\nchanges:", changed if changed else "none needed")
    if a.apply and changed:
        sidecar["draws"].sort(key=lambda e: e["index"])
        with open(sidecar_path, "w") as f:
            json.dump(sidecar, f, indent=1); f.write("\n")
        print("wrote", sidecar_path)

if __name__ == "__main__":
    main()
