#!/usr/bin/env python3
"""
t2_worldroi_burst.py — T2-WORLDROI (Wave 19) FOREARM-FLOAT triage burst.

Boots rb3-native to band gameplay (RB3_DRAWLOG_PROV=1) and captures a burst of
paired (screenshot, skinned-prov drawlog) frames — the production-smoke harness the
plan §6 asks for (adapted from char-burst-capture.py). Use it to catch a frame where
the top-center flesh-colored FOREARM-FLOAT structure is present, then query its ROI
with `uidump_query.py --roi` (live) or offline against the saved dl_NN.json to NAME
its mesh / bone / owner. Triage only — no fix.

SPATIAL provenance axis (which mesh/bone/owner drew a pixel), never the T1 timing axis.
"""
import argparse, json, os, signal, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import t2_worldroi_probe as t2


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=t2.DEFAULT_BIN)
    ap.add_argument("--data", default=t2.DEFAULT_DATA)
    ap.add_argument("--out", default="/tmp/t2-m5-burst")
    ap.add_argument("--shots", type=int, default=16)
    ap.add_argument("--interval", type=float, default=1.2)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    port = t2.free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": os.path.join(t2.REPO, "native", "dta"),
                "RB3_DRAWLOG_PROV": "1", "RB3_FIXED_CLOCK": "1"})
    logf = open(os.path.join(args.out, "boot.log"), "w")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=t2.REPO, start_new_session=True)
    try:
        if not t2.nav_to_gameplay(port, proc):
            print("NAV FAIL"); return 1
        for i in range(args.shots):
            t2.verb(port, "autohit")
            h = t2.health(port)
            t2.screenshot(port, os.path.join(args.out, f"shot_{i:02d}.png"))
            dl = t2.fetch_drawlog(port, roi=None)
            rows = [{"i": d.get("i"), "mesh": d.get("prov", {}).get("mesh"),
                     "owner": d.get("prov", {}).get("owner"),
                     "rectKind": d.get("prov", {}).get("rectKind"),
                     "rect": d.get("prov", {}).get("rect"),
                     "boneFallback": d.get("prov", {}).get("boneFallback"),
                     "boneRects": d.get("prov", {}).get("boneRects", [])}
                    for d in dl.get("draws", []) if d.get("skinned")]
            with open(os.path.join(args.out, f"dl_{i:02d}.json"), "w") as f:
                json.dump({"frame": dl.get("frame"), "rows": rows}, f)
            print(f"shot {i:02d} frame={h[0] if h else '?'} skinned={len(rows)}", flush=True)
            time.sleep(args.interval)
        print(f"done -> {args.out}")
        return 0
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=8)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()


if __name__ == "__main__":
    sys.exit(main())
