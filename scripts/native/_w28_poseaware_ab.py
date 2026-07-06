#!/usr/bin/env python3
"""
_w28_poseaware_ab.py — W2.8 BL-A1 (S3): A/B the NEW RB3_HANDS_POSEAWARE rigid-anchor
hands fix under the far-vertex IK_SHARD_VERT metric, on the real rb3 band path.

One boot per arm (RB3_HANDS_POSEAWARE ON vs default OFF). Reuses the step-0 harness
machinery (_w28_handsfix_ab.reduce_arm + hands_bind_characterize) verbatim; the only
difference is which flag the ON arm sets. The metric that matters is the appendage
far-vertex shard `IK_SHARD_VERT wext` (should collapse toward the mesh's own bind
extent when the rigid anchor removes the multi-bone basis divergence).

Read-only w.r.t. source; runs the given binary.
"""
import argparse, json, os, signal, subprocess, sys, time, importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

hc = _load("hc", os.path.join(HERE, "hands_bind_characterize.py"))
ab = _load("ab", os.path.join(HERE, "_w28_handsfix_ab.py"))


def run_arm(binpath, data, dwell, outdir, pose_aware, verbose):
    label = "poseON" if pose_aware else "poseOFF"
    port = hc.free_port()
    log_path = os.path.join(outdir, f"raw-{label}.log")
    logf = open(log_path, "wb")
    env = dict(os.environ)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": data, "RB3_GAME_INPUT": hc.NAV_SCRIPT,
        "REBIND_DRAW_SKINPOS": "1", "REBIND_DRAW_FLING": "1", "SHARD_RATIO_DBG": "1",
        "HEAD_REBIND_PROBE": "1", "RELOAD_PROBE": "1",
        "IK_SHARD_VERT": "*",
        "HANDS_RIGID_PROBE": "1",
    })
    if pose_aware:
        env["RB3_HANDS_POSEAWARE"] = "1"
    print(f"[ab] [{label}] launching (port {port}) -> {log_path}", flush=True)
    proc = subprocess.Popen([binpath], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    reached = False
    try:
        if hc.wait_for(port, lambda f, m, s: True, hc.SERVER_READY_TIMEOUT,
                       "server", verbose, proc) is None:
            print(f"[ab] [{label}] FAIL server never up", flush=True); return None
        h = hc.wait_for(port, lambda f, m, s: hc.is_game_screen(s) or m > 500.0,
                        hc.GAMEPLAY_TIMEOUT, "game", verbose, proc)
        if h is None:
            print(f"[ab] [{label}] FAIL never reached game", flush=True); return None
        reached = True
        print(f"[ab] [{label}] game reached frame={h[0]} songMs={h[1]:.0f} dwell {dwell}s",
              flush=True)
        dl = time.time() + dwell
        while time.time() < dl:
            if proc.poll() is not None:
                print(f"[ab] [{label}] proc exited {proc.returncode} during dwell", flush=True)
                break
            hc.health(port); time.sleep(0.5)
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except Exception: pass
        try: proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()
    return log_path if reached else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(REPO, "native", "build-agent-W2.8", "rb3-native"))
    ap.add_argument("--data", default=os.path.join(REPO, "orig-assets", "extracted"))
    ap.add_argument("--dwell", type=float, default=30.0)
    ap.add_argument("--outdir", default="/tmp/w28-poseaware-ab")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    results = {}
    for pa in (False, True):
        key = "poseON" if pa else "poseOFF"
        lp = run_arm(a.bin, a.data, a.dwell, a.outdir, pa, a.verbose)
        results[key] = {"error": "arm failed"} if lp is None else ab.reduce_arm(lp)
    out = os.path.join(a.outdir, "ab-result.json")
    with open(out, "w") as f:
        json.dump(results, f, indent=2)
    print("\n===== W2.8 BL-A1 S3 A/B RESULT =====", flush=True)
    print(json.dumps(results, indent=2), flush=True)
    print(f"\nwritten -> {out}", flush=True)


if __name__ == "__main__":
    main()
