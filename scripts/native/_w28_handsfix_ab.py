#!/usr/bin/env python3
"""
_w28_handsfix_ab.py  — W2.8 Stage B.S2: A/B the EXISTING RB3_HANDS_BIND_FIX flag
under the far-vertex rotation-basis oracle's real-band-path metrics.

One boot per arm (RB3_HANDS_BIND_FIX ON vs default OFF). Both boots run the SAME
real rb3 band load path (boot->song-select->gameplay->count-in->steady play) with
the in-tree read-only skinning diagnostics enabled, INCLUDING the far-vertex
IK_SHARD_VERT probe (the non-blind metric BL-A2 formalizes). We reduce, per arm,
the appendage (hand/finger/hair/head) shard signals:
  - SHARD_RATIO band-mesh DROPs   (guard judged the mesh degenerate: the shard that
                                    makes hands "missing")
  - REBIND_DRAW_FLING (>120u)     (a bone flung far)
  - REBIND_DRAW_SKINPOS worst-u   (origin metric — the BLIND one, recorded for
                                    contrast)
  - IK_SHARD_VERT wext / vertDev  (the FAR-vertex explosion, the non-blind metric)
  - HEAD_REBIND_PENDING appendage (un-completable meshes; the flag's whole job is to
                                    turn these into completed rebinds)

Verdict logic (per WAVE7_KICKOFF B.S2):
  (i)  flag ON turns the far-vertex shard GREEN / materially better -> BL-A1 = adopt
  (ii) no improvement -> pose-varying basis error suspected (static rebake can't fix)
  (iii)intermediate -> quantify.

Reuses hands_bind_characterize.py machinery. Read-only; runs the existing binary.
"""
import argparse, json, os, signal, socket, subprocess, sys, time, importlib.util

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
HC = os.path.join(os.path.dirname(__file__), "hands_bind_characterize.py")
spec = importlib.util.spec_from_file_location("hc", HC)
hc = importlib.util.module_from_spec(spec); spec.loader.exec_module(hc)

NAV_SCRIPT = hc.NAV_SCRIPT
IK_RE = None
import re
RE_IK = re.compile(
    r"\[IK_SHARD_VERT\] mesh='([^']*)' wext=([\d.]+) worstVtx=\d+ domBone\[\d+\]='([^']*)' "
    r"w=[\d.]+ vertR=([\d.]+) vertDevFromCentroid=([\d.]+)")


def run_arm(binpath, data, dwell, outdir, hands_fix, verbose):
    label = "fixON" if hands_fix else "fixOFF"
    port = hc.free_port()
    log_path = os.path.join(outdir, f"raw-{label}.log")
    logf = open(log_path, "wb")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": data, "RB3_GAME_INPUT": NAV_SCRIPT,
        "REBIND_DRAW_SKINPOS": "1", "REBIND_DRAW_FLING": "1", "SHARD_RATIO_DBG": "1",
        "HEAD_REBIND_PROBE": "1", "RELOAD_PROBE": "1",
        "IK_SHARD_VERT": "*",  # far-vertex non-blind probe, all shard meshes
    })
    if hands_fix:
        env["RB3_HANDS_BIND_FIX"] = "1"
    print(f"[ab] [{label}] launching (port {port}) -> {log_path}", flush=True)
    proc = subprocess.Popen([binpath], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    reached = False
    try:
        if hc.wait_for(port, lambda f, m, s: True, hc.SERVER_READY_TIMEOUT,
                       "server", verbose, proc) is None:
            print(f"[ab] [{label}] FAIL server never up", flush=True)
            return None
        h = hc.wait_for(port, lambda f, m, s: hc.is_game_screen(s) or m > 500.0,
                        hc.GAMEPLAY_TIMEOUT, "game", verbose, proc)
        if h is None:
            print(f"[ab] [{label}] FAIL never reached game", flush=True)
            return None
        reached = True
        print(f"[ab] [{label}] game reached frame={h[0]} songMs={h[1]:.0f} "
              f"screen='{h[2]}' dwell {dwell}s", flush=True)
        dl = time.time() + dwell
        saw_zero = saw_adv = False
        while time.time() < dl:
            if proc.poll() is not None:
                print(f"[ab] [{label}] proc exited {proc.returncode} during dwell", flush=True)
                break
            cur = hc.health(port)
            if cur:
                _, m, _ = cur
                if m <= 1.0: saw_zero = True
                if m > 1500.0: saw_adv = True
            time.sleep(0.5)
        print(f"[ab] [{label}] countin_covered={saw_zero and saw_adv}", flush=True)
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except Exception: pass
        try: proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()
    if not reached:
        return None
    return log_path


def reduce_arm(log_path):
    parsed = hc.parse_log(log_path)
    # IK_SHARD_VERT far-vertex reduction (not in hc.parse_log)
    ik = {}
    for line in hc.read_lines(log_path):
        m = RE_IK.search(line)
        if m:
            mesh, wext, bone, vr, dev = m.group(1), float(m.group(2)), m.group(3), \
                float(m.group(4)), float(m.group(5))
            e = ik.setdefault(mesh, {"wext": 0.0, "dev": 0.0, "bone": bone, "vertR": vr})
            if wext > e["wext"]:
                e["wext"] = wext; e["dev"] = dev; e["bone"] = bone; e["vertR"] = vr
    # Appendage-only summaries
    def app(d): return {k: v for k, v in d.items() if hc.is_appendage(k)}
    ratio = parsed["ratio"]
    band_drops = {k: v for k, v in ratio.items() if v.get("band") and v.get("drop")}
    app_drops = app(band_drops)
    app_fling = app(parsed["fling"])
    app_skinpos = app(parsed["skinpos"])
    app_ik = app(ik)
    app_pending = {k: v for k, v in parsed["pending"].items()
                   if hc.is_appendage(k.split("|")[-1])}
    worst_ik = max((v["wext"] for v in app_ik.values()), default=0.0)
    worst_dev = max((v["dev"] for v in app_ik.values()), default=0.0)
    worst_skinpos = max((v["max"] for v in app_skinpos.values()), default=0.0)
    return {
        "appendage_band_drops": {k: v["max_ratio"] for k, v in app_drops.items()},
        "n_appendage_drops": len(app_drops),
        "appendage_fling": {k: {"count": v["count"], "max": v["max"]} for k, v in app_fling.items()},
        "n_appendage_fling": len(app_fling),
        "worst_appendage_skinpos_u": round(worst_skinpos, 1),
        "appendage_ik_shard": {k: {"wext": round(v["wext"], 1), "dev": round(v["dev"], 1),
                                    "bone": v["bone"], "vertR": round(v["vertR"], 1)}
                                for k, v in app_ik.items()},
        "worst_appendage_ik_wext_u": round(worst_ik, 1),
        "worst_appendage_ik_dev_u": round(worst_dev, 1),
        "appendage_pending": app_pending,
        "n_appendage_pending": len(app_pending),
        "all_band_drops": {k: round(v["max_ratio"], 2) for k, v in band_drops.items()},
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(REPO, "native", "build-native", "rb3-native"))
    ap.add_argument("--data", default=os.path.join(REPO, "orig-assets", "extracted"))
    ap.add_argument("--dwell", type=float, default=30.0)
    ap.add_argument("--outdir", default="/tmp/w28-handsfix-ab")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    results = {}
    for fix in (False, True):
        key = "fixON" if fix else "fixOFF"
        lp = run_arm(a.bin, a.data, a.dwell, a.outdir, fix, a.verbose)
        if lp is None:
            results[key] = {"error": "arm failed to reach gameplay"}
        else:
            results[key] = reduce_arm(lp)
    out = os.path.join(a.outdir, "ab-result.json")
    with open(out, "w") as f:
        json.dump(results, f, indent=2)
    print("\n===== W2.8 B.S2 A/B RESULT =====", flush=True)
    print(json.dumps(results, indent=2), flush=True)
    print(f"\nwritten -> {out}", flush=True)


if __name__ == "__main__":
    main()
