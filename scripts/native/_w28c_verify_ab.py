#!/usr/bin/env python3
"""
_w28c_verify_ab.py — W2.8c B.S3 HARD-EXIT VERIFY: A/B the RB3_HANDS_PERFRAME_CONJ
per-frame pose-aware conjugation under the far-vertex IK_SHARD_VERT metric, on the
real rb3 band path. Same-binary / same-member protocol (WAVE8_REVIEW A5).

conjOFF (default) vs conjON (RB3_HANDS_PERFRAME_CONJ=1). In BOTH arms:
  RB3_HANDS_POSEAWARE  UNSET (A5 — it writes the same meshes at the same seam)
  RB3_PP_LUMA_CEILING  UNSET (A7)
  RB3_PLACEMENT_CONTRACT default-ON (do NOT set _OFF)

Reuses hands_bind_characterize (hc) + _w28_handsfix_ab.reduce_arm verbatim. Read-only
w.r.t. source; runs the given binary. Hard exit bar: conjON worst appendage
IK_SHARD_VERT wext < 20u vs conjOFF ~106u; FLING=0; nothing in 200-460u; 0 added drops;
gloves not regressed (<=84u).
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


def run_arm(binpath, data, dwell, outdir, conj, verbose):
    label = "conjON" if conj else "conjOFF"
    port = hc.free_port()
    log_path = os.path.join(outdir, f"raw-{label}.log")
    logf = open(log_path, "wb")
    env = dict(os.environ)
    # A5/A7: guarantee confounding flags are UNSET in BOTH arms.
    for k in ("RB3_HANDS_POSEAWARE", "RB3_PP_LUMA_CEILING", "RB3_HANDS_PERFRAME_CONJ",
              "RB3_PLACEMENT_CONTRACT_OFF"):
        env.pop(k, None)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": data, "RB3_GAME_INPUT": hc.NAV_SCRIPT,
        "REBIND_DRAW_SKINPOS": "1", "REBIND_DRAW_FLING": "1", "SHARD_RATIO_DBG": "1",
        "HEAD_REBIND_PROBE": "1", "RELOAD_PROBE": "1",
        "IK_SHARD_VERT": "*",  # far-vertex non-blind probe, all shard meshes
    })
    if conj:
        env["RB3_HANDS_PERFRAME_CONJ"] = "1"
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
    ap.add_argument("--bin", default=os.path.join(REPO, "native", "build-agent-W2.8c-verify", "rb3-native"))
    ap.add_argument("--data", default=os.path.join(REPO, "orig-assets", "extracted"))
    ap.add_argument("--dwell", type=float, default=30.0)
    ap.add_argument("--outdir", default="/tmp/w28c-verify-ab")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    results = {}
    for conj in (False, True):
        key = "conjON" if conj else "conjOFF"
        lp = run_arm(a.bin, a.data, a.dwell, a.outdir, conj, a.verbose)
        results[key] = {"error": "arm failed"} if lp is None else ab.reduce_arm(lp)
    out = os.path.join(a.outdir, "ab-result.json")
    with open(out, "w") as f:
        json.dump(results, f, indent=2)
    print("\n===== W2.8c B.S3 HARD-EXIT A/B RESULT =====", flush=True)
    print(json.dumps(results, indent=2), flush=True)
    print(f"\nwritten -> {out}", flush=True)


if __name__ == "__main__":
    main()
