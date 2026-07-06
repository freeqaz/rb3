#!/usr/bin/env python3
"""WASH-fix (Wave 8 A.S1) — two-hypothesis venue-wash instrumentation driver.

Reuses wash-measure.capture_pinned verbatim, boots with RB3_WASH_PROBE=1 so the
engine emits the SCENE engagement digest (H1) + PP unorm-intermediate fact (H2) +
env-staleness path into each boot's `<prefix>.engine.log`. For every boot it
records: wash_score class, the engagement outcome parsed from the TAIL of the log
(the state at capture time), the grey-key flag, the distinct envs seen, and the PP
unorm fact. Writes per-boot rows to measure/probe_<tag>.json.

A7 baseline arms (RB3_PP_LUMA_CEILING UNSET in every arm, per WAVE8_REVIEW A7):
  default          {}                              -> expect low wash rate
  venue_light_off  {RB3_VENUE_LIGHT_OFF:1}         -> expect PINK 8/8 (deterministic fail-red)

Usage:
  RB3_WASH_PROBE=1 python3 wash_probe_run.py --bin <rb3-native> --songms 21000 --n 8 \
      --arms default,venue_light_off --out measure --raws /tmp/washfix-caps
"""
import argparse, importlib.util, json, os, re, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
NSCR = os.path.join(REPO, "scripts", "native")

def _load(mod, path):
    spec = importlib.util.spec_from_file_location(mod, path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m

wm = _load("wash_measure", os.path.join(NSCR, "wash-measure.py"))
ws = _load("wash_score", os.path.join(NSCR, "wash_score.py"))

ARMS = {
    "default":          {},
    "venue_light_off":  {"RB3_VENUE_LIGHT_OFF": "1"},
    "pp_off":           {"RB3_PP_OFF": "1"},
}

SCENE_RE = re.compile(r"\[WASHPROBE\] SCENE env=(\S+) engaged=(\d) miss=(\S+) dl=(\d+) pl=(\d+) greykey=(\d)")
PP_RE    = re.compile(r"\[WASHPROBE\] PP intermediate \d+x\d+ fmt=(\S+) \(unorm=(\d)\)")
STALE_RE = re.compile(r"\[WASHPROBE\] STALE rewrite=env env=(\S+)")

def parse_log(path):
    """Return dict summarising the boot's WASHPROBE stream, weighted to the TAIL
    (last 40 SCENE lines ~ the captured frame)."""
    scenes, pp_unorm, stale_envs = [], None, collections.Counter()
    try:
        with open(path, "r", errors="replace") as f:
            for ln in f:
                m = SCENE_RE.search(ln)
                if m:
                    scenes.append((m.group(1), int(m.group(2)), m.group(3),
                                   int(m.group(4)), int(m.group(5)), int(m.group(6))))
                    continue
                m = PP_RE.search(ln)
                if m and pp_unorm is None:
                    pp_unorm = {"fmt": m.group(1), "unorm": int(m.group(2))}
                    continue
                m = STALE_RE.search(ln)
                if m:
                    stale_envs[m.group(1)] += 1
    except FileNotFoundError:
        return {"n_scene": 0}
    tail = scenes[-40:]
    engaged_tail = sum(1 for s in tail if s[1] == 1)
    missed_tail  = sum(1 for s in tail if s[1] == 0)
    miss_reasons = collections.Counter(s[2] for s in tail if s[1] == 0)
    envs_tail    = collections.Counter(s[0] for s in tail)
    greykey_tail = sum(1 for s in tail if s[5] == 1)
    # dominant final state
    final = tail[-1] if tail else None
    return {
        "n_scene": len(scenes),
        "tail_engaged": engaged_tail, "tail_missed": missed_tail,
        "tail_miss_reasons": dict(miss_reasons),
        "tail_envs": dict(envs_tail),
        "tail_greykey": greykey_tail,
        "final_env": final[0] if final else None,
        "final_engaged": final[1] if final else None,
        "final_miss": final[2] if final else None,
        "pp": pp_unorm,
        "stale_env_rewrites": dict(stale_envs),
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--songms", type=float, default=21000.0)
    ap.add_argument("--tol", type=float, default=250.0)
    ap.add_argument("--n", type=int, default=8)
    ap.add_argument("--arms", default="default,venue_light_off")
    ap.add_argument("--raws", default="/tmp/washfix-caps")
    ap.add_argument("--out", default=os.path.join(HERE, "measure"))
    ap.add_argument("--max-retries", type=int, default=6)
    ap.add_argument("--tag", default=None)
    args = ap.parse_args()

    os.makedirs(args.raws, exist_ok=True); os.makedirs(args.out, exist_ok=True)
    os.environ["RB3_WASH_PROBE"] = "1"      # propagates via capture_pinned's dict(os.environ)
    lo, hi = args.songms - args.tol, args.songms + args.tol
    arms = args.arms.split(",")
    tag = args.tag or f"ms{int(args.songms)}"
    rows = []
    for arm in arms:
        env = dict(ARMS[arm])
        got = 0; tries = 0
        while got < args.n and tries < args.n * args.max_retries:
            tries += 1
            prefix = os.path.join(args.raws, f"{tag}_{arm}_{got+1:02d}_t{tries}")
            print(f"[washfix] {tag} {arm} #{got+1} try{tries}", flush=True)
            png, info = wm.capture_pinned(args.bin, dict(env), prefix, lo, hi, hi)
            if png is None:
                print(f"  discard: {info}", flush=True)
                continue
            cls = ws.score_image(png)
            logsum = parse_log(prefix + ".engine.log")
            row = {"arm": arm, "songms_target": args.songms, "songms_actual": float(info),
                   "png": png, "class": cls.get("wash_class"), "is_wash": cls.get("is_wash"),
                   "mean_luma": cls.get("mean_luma"), "pink_frac": cls.get("pink_frac"), **logsum}
            rows.append(row); got += 1
            print(f"  CAP class={row['class']} luma={row.get('mean_luma')} "
                  f"engaged_final={logsum.get('final_engaged')} miss={logsum.get('final_miss')} "
                  f"tail_eng/miss={logsum.get('tail_engaged')}/{logsum.get('tail_missed')} "
                  f"pp={logsum.get('pp')}", flush=True)
        # crash-safe write after each arm
        outp = os.path.join(args.out, f"probe_{tag}.json")
        json.dump(rows, open(outp, "w"), indent=1)
        print(f"[washfix] wrote {outp} ({len(rows)} rows)", flush=True)
    print("[washfix] DONE", flush=True)

if __name__ == "__main__":
    sys.exit(main())
