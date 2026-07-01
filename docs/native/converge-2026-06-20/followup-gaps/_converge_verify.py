#!/usr/bin/env python3
"""Verify driver for GAP A (highway watermark) + GAP B (big_club white crowd).

Boots the WORKTREE rb3-native (REPO derived from this script's location), optionally
sets a venue override, drives to a deep gameplay anchor, captures screenshots, then
measures:
  GAP A (highway): mid-track watermark band — stroke(top-25%-luma)-bg(bottom-50%)
    delta, stroke teal (g+b-2r), stroke G/B. crop x[0.38,0.62] y[0.50,0.62].
  GAP B (crowd):   left/right crowd strips — luma + white% (all chans >200) + sat.
    crop left [0,0.22w], right [0.78w,1.0w], mid-height [0.30h,0.75h].
  BAND region (control): center-top [0.35,0.65]x[0.18,0.55] luma + sat (must NOT drop).

Env A/B passes via the environment (RB3_CROWD_DIM_OFF, RB3_HIGHWAY_WATERMARK_OFF,
RB3_CROWD_DIM, RB3_HIGHWAY_WATERMARK_DIM, etc.). Prints a JSON metrics blob.
"""
import argparse, importlib.util, json, os, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
# REPO = the worktree root (4 levels up: followup-gaps/converge-2026-06-20/native/docs)
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
spec = importlib.util.spec_from_file_location(
    "kg", os.path.join(REPO, "scripts", "native", "keyboard-to-gameplay.py"))
kg = importlib.util.module_from_spec(spec); spec.loader.exec_module(kg)

from PIL import Image


def measure_surface(path):
    """Pure highway-surface patch just below the now-bar (x[0.40,0.60] y[0.64,0.80]):
    only surface + watermark filigree, no gems/lanes/venue. Reports mean RGB, luma,
    teal (g+b-2r), G/B — the clean watermark isolation."""
    im = Image.open(path).convert("RGB"); W, H = im.size
    box = im.crop((int(0.40 * W), int(0.64 * H), int(0.60 * W), int(0.80 * H)))
    px = list(box.getdata()); n = max(1, len(px))
    r = sum(p[0] for p in px) / n; g = sum(p[1] for p in px) / n; b = sum(p[2] for p in px) / n
    lum = 0.299 * r + 0.587 * g + 0.114 * b
    return {"rgb": [round(r, 1), round(g, 1), round(b, 1)], "luma": round(lum, 1),
            "teal_g_b_2r": round(g + b - 2 * r, 1), "G_over_B": round(g / (b + 1e-6), 3)}


def measure_highway(path):
    im = Image.open(path).convert("RGB"); W, H = im.size
    box = im.crop((int(0.38 * W), int(0.50 * H), int(0.62 * W), int(0.62 * H)))
    px = list(box.getdata())
    lum = [0.299 * r + 0.587 * g + 0.114 * b for (r, g, b) in px]
    order = sorted(range(len(px)), key=lambda i: lum[i])
    n = len(px)
    bg_idx = order[: n // 2]                 # bottom 50% luma = background
    stroke_idx = order[int(n * 0.75):]       # top 25% luma = watermark stroke
    def mean(idxs, ch):
        return sum(px[i][ch] for i in idxs) / max(1, len(idxs))
    sr, sg, sb = mean(stroke_idx, 0), mean(stroke_idx, 1), mean(stroke_idx, 2)
    bgl = sum(lum[i] for i in bg_idx) / max(1, len(bg_idx))
    strl = sum(lum[i] for i in stroke_idx) / max(1, len(stroke_idx))
    return {"stroke_rgb": [round(sr, 1), round(sg, 1), round(sb, 1)],
            "bg_luma": round(bgl, 1), "stroke_luma": round(strl, 1),
            "stroke_minus_bg": round(strl - bgl, 1),
            "teal_g_b_2r": round(sg + sb - 2 * sr, 1),
            "stroke_G_over_B": round(sg / (sb + 1e-6), 3)}


def _region_stats(im, x0, y0, x1, y1):
    W, H = im.size
    box = im.crop((int(x0 * W), int(y0 * H), int(x1 * W), int(y1 * H)))
    px = list(box.getdata()); n = max(1, len(px))
    lum = sum(0.299 * r + 0.587 * g + 0.114 * b for (r, g, b) in px) / n
    white = sum(1 for (r, g, b) in px if r > 200 and g > 200 and b > 200) / n
    # mean saturation (max-min)/max
    sat = 0.0
    for (r, g, b) in px:
        mx = max(r, g, b); mn = min(r, g, b)
        sat += (mx - mn) / (mx + 1e-6)
    sat /= n
    return {"luma": round(lum, 1), "white_pct": round(white * 100, 2),
            "sat": round(sat, 3)}


def measure_crowd(path):
    im = Image.open(path).convert("RGB")
    return {
        # foreground white-crowd strips (the raised-hands billboards line the lower
        # edges) — the dominant visible white is here, NOT mid-height.
        "crowdL": _region_stats(im, 0.00, 0.45, 0.26, 0.95),
        "crowdR": _region_stats(im, 0.74, 0.45, 1.00, 0.95),
        # band performers (center-top) — control, must NOT dim.
        "band":   _region_stats(im, 0.35, 0.18, 0.65, 0.55),
        "frame":  _region_stats(im, 0.00, 0.00, 1.00, 1.00),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--override", default="")
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--diff", default="hard")
    ap.add_argument("--mode", default="highway", choices=["highway", "crowd"])
    ap.add_argument("--anchor-ms", type=float, default=30000.0)
    ap.add_argument("--shot", default="")  # crowd: force a crowd shot
    ap.add_argument("--hold-s", type=float, default=30.0)
    ap.add_argument("--nframes", type=int, default=3)
    ap.add_argument("--log", default="/tmp/converge_verify.log")
    ap.add_argument("--out", default="/tmp/converge_verify")
    ap.add_argument("--tag", default="cap")
    args = ap.parse_args()

    binp = os.path.join(REPO, "native", "build-native", "rb3-native")
    data = os.path.join(REPO, "orig-assets", "extracted")
    overlay = os.path.join(REPO, "native", "dta")
    os.makedirs(args.out, exist_ok=True)
    port = kg.free_port()
    logf = open(args.log, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": data, "RB3_DTA_OVERLAY": overlay})
    print(f"[verify] launch port={port} mode={args.mode} override='{args.override}' "
          f"tag={args.tag} log={args.log}")
    proc = subprocess.Popen([binp], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    P = lambda b: kg.press(port, b)
    metrics = []
    try:
        if kg.wait_screen(port, lambda s: True, 40, proc) is None:
            print("[verify] FAIL: no http"); return 1
        kg.wait_screen(port, lambda s: s and s != "", 60, proc)
        def setov():
            if args.override:
                r = kg.dta(port, '{meta_performer set_venue_override %s}' % args.override)
        setov()
        for _ in range(8):
            if kg.health(port)[2] == "main_hub_screen": break
            P(kg.START); time.sleep(0.6)
        kg.wait_screen(port, "main_hub_screen", 30, proc); setov()
        for _ in range(10):
            if kg.health(port)[2] == "song_select_screen": break
            P(kg.CONFIRM); time.sleep(0.7)
        if kg.wait_screen(port, "song_select_screen", 40, proc) is None:
            print("[verify] FAIL: no song_select"); return 1
        time.sleep(1.5); setov()
        for _ in range(args.song_downs):
            P(kg.DDOWN); time.sleep(0.2)
        P(kg.CONFIRM)
        if kg.wait_screen(port, "part_difficulty_screen", 60, proc) is None:
            print("[verify] FAIL: no part_difficulty"); return 1
        setov()
        ov = kg.wait_view(port, lambda v: v.startswith("choose_part") or v == "choose_diff", 30, proc, "part")
        if ov and ov[0].startswith("choose_part"):
            P(kg.CONFIRM)
        o2 = kg.overshell(port); g = 0
        while o2[0] == "confirm_action" and g < 4:
            P(kg.CONFIRM); time.sleep(0.5); o2 = kg.overshell(port); g += 1
        ov = kg.wait_view(port, lambda v: v == "choose_diff", 30, proc, "diff")
        if ov:
            for _ in range(kg.DIFF_INDEX[args.diff]):
                P(kg.DDOWN); time.sleep(0.25)
            P(kg.CONFIRM)
        o2 = kg.overshell(port); g = 0
        while o2[0] == "confirm_action" and g < 4:
            P(kg.CONFIRM); time.sleep(0.5); o2 = kg.overshell(port); g += 1
        kg.wait_view(port, lambda v: v == "ready_to_play", 30, proc, "ready")
        h = kg.wait_screen(port, "game_screen", 90, proc)
        if h is None:
            print("[verify] FAIL: no game_screen"); return 1
        kg.verb(port, "nofail")
        print(f"[verify] game_screen reached songMs={h[1]:.0f}")

        # crowd mode: pin a crowd shot so the audience is framed
        if args.mode == "crowd" and args.shot:
            kg.dta(port, "{rb3_director_disable 1}")
            for cand in (args.shot, args.shot + ".shot"):
                r = kg.dta(port, '{rb3_force_shot "%s"}' % cand)
                if r == "force_shot ok":
                    print(f"[verify] forced shot {cand}")
                    break

        # advance to anchor — capture at ABSOLUTE songMs targets so two A/B passes
        # hit the SAME (songMs) pose (crowd characters animate clap/lighter; an
        # unmatched pose swamps the dim signal).
        dl = time.time() + args.hold_s
        shot_i = 0
        anchor = args.anchor_ms
        nf = args.nframes
        targets = [anchor + i * 700.0 for i in range(nf)]
        while time.time() < dl and shot_i < nf:
            kg.verb(port, "autohit")
            hh = kg.health(port)
            if hh is None: break
            if hh[1] >= targets[shot_i]:
                p = os.path.join(args.out, f"{args.tag}_anchor_{shot_i}_ms{int(targets[shot_i])}.png")
                if kg.screenshot(port, p):
                    if args.mode == "highway":
                        m = measure_highway(p)
                        m["surface"] = measure_surface(p)
                    else:
                        m = measure_crowd(p)
                    m["file"] = p; m["songMs"] = round(hh[1], 1)
                    metrics.append(m)
                    print(f"[verify] shot {shot_i} songMs={hh[1]:.0f} -> {p}")
                    shot_i += 1
            time.sleep(0.6)
        result = {"tag": args.tag, "mode": args.mode, "override": args.override,
                  "env": {k: env[k] for k in (
                      "RB3_CROWD_DIM_OFF", "RB3_CROWD_DIM",
                      "RB3_HIGHWAY_WATERMARK_OFF", "RB3_HIGHWAY_WATERMARK_DIM",
                      "RB3_TRACK_LIGHT_OFF", "RB3_VENUE_LIGHT_OFF") if k in env},
                  "metrics": metrics}
        with open(os.path.join(args.out, f"{args.tag}_metrics.json"), "w") as f:
            json.dump(result, f, indent=2)
        print("METRICS " + json.dumps(result))
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()


if __name__ == "__main__":
    sys.exit(main())
