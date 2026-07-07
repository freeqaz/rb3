#!/usr/bin/env python3
"""bootrng_probe.py — Wave 11 Lane A (BOOTRNG) A.S1 prime-suspect instrumentation.

Reuses wash-measure.capture_pinned VERBATIM (same pinned coop_dir_crowd.shot,
songMs window, RB3_FIXED_CLOCK boot) so this measures the SAME confound the WHITE
gate hit. Boots N>=10 times with RB3_BOOTRNG_PROBE=1 + RB3_WASH_PROBE=1, parses
each boot's engine.log for the [BOOTRNG] streams (tail = captured frame), scores
the PNG (wash class / mid_sat / hi_frac), and correlates.

Emits per boot:
  - preset picks: the ordered tuple of [BOOTRNG] PRESET (cat,idx,preset,gdraw)
  - postproc source tuple (tail): [BOOTRNG] PPSRC (src,p1,p2,blend,gdraw)
  - light VALUE digest (tail): [BOOTRNG] LIGHTVAL (env,valhash,dl,pl,amb)
  - resolved ColorXfm (tail): [BOOTRNG] PPRESOLVED (pp,con,bri,sat,vig,levels)
  - engagement (tail): [WASHPROBE] SCENE (env,engaged,miss,dl,pl,greykey)
  - wash_score: class, mean_luma, hi_frac, mid_sat

Then partitions boots by (preset-pick tuple, valhash@capture, gdraw@capture) and
reports within-partition vs cross-partition mid_sat/hi_frac spread — the H-A test.

Usage:
  python3 bootrng_probe.py --bin native/build-agent-BOOTRNG/rb3-native --n 10 \
      --raws /tmp/bootrng-caps --out measure --tag as1
"""
import argparse, importlib.util, json, os, re, collections, statistics as st

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
NSCR = os.path.join(REPO, "scripts", "native")

def _load(mod, path):
    spec = importlib.util.spec_from_file_location(mod, path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m

wm = _load("wash_measure", os.path.join(NSCR, "wash-measure.py"))
ws = _load("wash_score", os.path.join(NSCR, "wash_score.py"))
tb = _load("tonal_band_sat", os.path.join(HERE, "..", "WASH-fix", "tonal_band_sat.py"))

PRESET_RE = re.compile(r"\[BOOTRNG\] PRESET cat=(\S+) idx=(\d+)/(\d+) preset=(\S+) gdraw=(\d+)")
PPSRC_RE  = re.compile(r"\[BOOTRNG\] PPSRC src=(.+?) p1=(\S+) p2=(\S+) blend=(\S+) gdraw=(\d+)")
LIGHTV_RE = re.compile(r"\[BOOTRNG\] LIGHTVAL env=(\S+) valhash=(\S+) dl=(\d+) pl=(\d+) amb=\(([^)]+)\)")
PPRES_RE  = re.compile(r"\[BOOTRNG\] PPRESOLVED pp=(\S+) con=(\S+) bri=(\S+) sat=(\S+) vig=(\S+) ")
SCENE_RE  = re.compile(r"\[WASHPROBE\] SCENE env=(\S+) engaged=(\d) miss=(\S+) dl=(\d+) pl=(\d+) greykey=(\d)")

def parse_log(path):
    presets, ppsrc, lightv, ppres, scenes = [], [], [], [], []
    try:
        with open(path, "r", errors="replace") as f:
            for ln in f:
                m = PRESET_RE.search(ln)
                if m: presets.append((m.group(1), int(m.group(2)), int(m.group(3)), m.group(4), int(m.group(5)))); continue
                m = PPSRC_RE.search(ln)
                if m: ppsrc.append((m.group(1).strip(), m.group(2), m.group(3), float(m.group(4)), int(m.group(5)))); continue
                m = LIGHTV_RE.search(ln)
                if m: lightv.append((m.group(1), m.group(2), int(m.group(3)), int(m.group(4)), m.group(5))); continue
                m = PPRES_RE.search(ln)
                if m: ppres.append((m.group(1), float(m.group(2)), float(m.group(3)), float(m.group(4)), float(m.group(5)))); continue
                m = SCENE_RE.search(ln)
                if m: scenes.append((m.group(1), int(m.group(2)), m.group(3), int(m.group(4)), int(m.group(5)), int(m.group(6)))); continue
    except FileNotFoundError:
        return {}
    # preset picks: full ordered tuple (of names) + distinct-by-category last picks
    preset_seq = tuple(p[3] for p in presets)                       # ordered names
    preset_final_by_cat = {}
    for cat, idx, cnt, name, gd in presets:
        preset_final_by_cat[cat] = name                              # last pick per category
    # tail (captured frame) states
    ppsrc_tail   = ppsrc[-1] if ppsrc else None
    lightv_tail  = lightv[-40:]                                      # env-scoped burst near capture
    # dominant valhash multiset over the tail world.cam writes
    valhash_tail = tuple(sorted(set(v[1] for v in lightv_tail)))
    ppres_tail   = ppres[-1] if ppres else None
    scene_tail   = scenes[-1] if scenes else None
    return {
        "n_preset": len(presets), "preset_seq": preset_seq,
        "preset_final_by_cat": preset_final_by_cat,
        "preset_gdraws": [p[4] for p in presets],
        "n_ppsrc": len(ppsrc), "ppsrc_tail": ppsrc_tail,
        "gdraw_capture": ppsrc_tail[4] if ppsrc_tail else None,
        "n_lightv": len(lightv), "valhash_tail": valhash_tail,
        "lightv_tail_envs": collections.Counter(v[0] for v in lightv_tail),
        "ppres_tail": ppres_tail,
        "scene_tail": scene_tail,
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--n", type=int, default=10)
    ap.add_argument("--songms", type=float, default=21000.0)
    ap.add_argument("--tol", type=float, default=2000.0)
    ap.add_argument("--out", default=os.path.join(HERE, "measure"))
    ap.add_argument("--raws", default="/tmp/bootrng-caps")
    ap.add_argument("--tag", default="as1")
    ap.add_argument("--song", type=int, default=4)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True); os.makedirs(args.raws, exist_ok=True)
    lo, hi = args.songms - args.tol, args.songms + args.tol
    overshoot = hi + 6000
    env = {"RB3_BOOTRNG_PROBE": "1", "RB3_WASH_PROBE": "1"}
    rows = []
    got = 0; attempts = 0
    while got < args.n and attempts < args.n * 4:
        attempts += 1
        pref = os.path.join(args.raws, f"{args.tag}_{attempts:02d}")
        png, info = wm.capture_pinned(args.bin, dict(env), pref, lo, hi, overshoot, song_downs=args.song)
        if png is None:
            print(f"  [#{attempts}] skip: {info}", flush=True); continue
        got += 1
        m = ws.score_image(png); bands = tb.bands(png)
        pl = parse_log(pref + ".engine.log")
        row = {"attempt": attempts, "png": png, "songms": float(info),
               "class": m["wash_class"], "mean_luma": m["mean_luma"],
               "hi_frac": m["hi_frac"], "mid_sat": bands["mid_sat"], **pl}
        rows.append(row)
        pt = row.get("ppsrc_tail"); pr = row.get("ppres_tail")
        print(f"  [#{attempts}] {m['wash_class']:9s} mid_sat={bands['mid_sat']:.3f} hi={m['hi_frac']:5.2f} "
              f"presets={row.get('preset_final_by_cat')} gdrawCap={row.get('gdraw_capture')} "
              f"valhash={row.get('valhash_tail')} ppsrc={(pt[0] if pt else None)} "
              f"sat={(pr[3] if pr else None)}", flush=True)
        json.dump(rows, open(os.path.join(args.out, f"{args.tag}.json"), "w"), indent=1, default=str)
    # ---- H-A partition analysis ----
    print("\n=== PER-BOOT TABLE ===")
    print(f"{'#':>3} {'class':9s} {'mid_sat':>7} {'hi_frac':>7} {'gdrawCap':>9} {'valhash_tail':<20} {'preset_final_by_cat'}")
    for r in rows:
        vh = ",".join(x[:8] for x in (r.get('valhash_tail') or ()))
        print(f"{r['attempt']:>3} {r['class']:9s} {r['mid_sat']:>7.3f} {r['hi_frac']:>7.2f} "
              f"{str(r.get('gdraw_capture')):>9} {vh:<20} {r.get('preset_final_by_cat')}")

    def partition(keyfn, label):
        groups = collections.defaultdict(list)
        for r in rows: groups[keyfn(r)].append(r)
        print(f"\n=== PARTITION BY {label} ({len(groups)} classes) ===")
        for k, rs in sorted(groups.items(), key=lambda kv: str(kv[0])):
            ms = [r["mid_sat"] for r in rs]; hf = [r["hi_frac"] for r in rs]
            spread = (max(ms) - min(ms)) if len(ms) > 1 else 0.0
            print(f"  key={str(k)[:60]:<60} n={len(rs)} mid_sat[{min(ms):.3f}-{max(ms):.3f}] "
                  f"spread={spread:.3f} mean={st.mean(ms):.3f} hi[{min(hf):.1f}-{max(hf):.1f}]")
        # within-partition mean spread
        within = [max([r['mid_sat'] for r in rs]) - min([r['mid_sat'] for r in rs]) for rs in groups.values() if len(rs) > 1]
        allms = [r['mid_sat'] for r in rows]
        print(f"  overall mid_sat spread={max(allms)-min(allms):.3f}; "
              f"mean WITHIN-partition spread={ (st.mean(within) if within else float('nan')):.3f} "
              f"(n multi-member partitions={len(within)})")

    if rows:
        partition(lambda r: tuple(sorted((r.get('preset_final_by_cat') or {}).items())), "PRESET-PICK (final-by-cat)")
        partition(lambda r: r.get('valhash_tail'), "LIGHTVAL valhash (tail)")
        partition(lambda r: r.get('gdraw_capture'), "gRand STREAM POSITION @capture")
    print("\n[bootrng] DONE", flush=True)

if __name__ == "__main__":
    main()
