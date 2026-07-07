#!/usr/bin/env python3
"""
_w28d_probe.py — W2.8d (Wave 9 Lane A / A.S1) BONE-LEVEL FACTOR ATTRIBUTION driver.

Boots rb3-native headless through the real band gameplay path (reusing
hands_bind_characterize's boot/nav machinery) with:
  IK_SHARD_VERT=*          -> A7 RED-baseline reproduce (worst-appendage wext)
  RB3_DUALSKIN_PROBE=hands_naked
                           -> A3 dual-skin engine probe: per-bone factor table
                              (offset/liveWorld/liveLocal/weight/composed-skin +
                              det/orthonormality + rest-basis DeltaR + R*sin(theta)
                              prediction) and goldens/w2.8-farvert/live_pose.txt
Per A7, RB3_HANDS_POSEAWARE / RB3_HANDS_PERFRAME_CONJ / RB3_PP_LUMA_CEILING are
UNSET; placement contract stays default-ON. Read-only w.r.t. source.
"""
import argparse, os, signal, subprocess, sys, time, importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

hc = _load("hc", os.path.join(HERE, "hands_bind_characterize.py"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(REPO, "native", "build-agent-W2.8d", "rb3-native"))
    ap.add_argument("--data", default=os.path.join(REPO, "orig-assets", "extracted"))
    ap.add_argument("--dwell", type=float, default=45.0)
    ap.add_argument("--mesh", default="hands_naked")
    ap.add_argument("--outdir", default="/tmp/w28d-probe")
    ap.add_argument("--fixture", default=os.path.join(REPO, "native", "tests", "goldens", "w2.8-farvert", "live_pose.txt"))
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    os.makedirs(os.path.dirname(a.fixture), exist_ok=True)
    port = hc.free_port()
    log_path = os.path.join(a.outdir, "raw-probe.log")
    logf = open(log_path, "wb")
    env = dict(os.environ)
    for k in ("RB3_HANDS_POSEAWARE", "RB3_HANDS_PERFRAME_CONJ", "RB3_PP_LUMA_CEILING",
              "RB3_PLACEMENT_CONTRACT_OFF"):
        env.pop(k, None)
    env.update({
        "RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
        "MILO_HEADLESS": "1", "RB3_DATA": a.data, "RB3_GAME_INPUT": hc.NAV_SCRIPT,
        "IK_SHARD_VERT": "*",
        "RB3_DUALSKIN_PROBE": a.mesh,
        "RB3_DUALSKIN_FIXTURE": a.fixture,
    })
    print(f"[w28d] launching port {port} -> {log_path} (fixture {a.fixture})", flush=True)
    proc = subprocess.Popen([a.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    reached = False
    try:
        if hc.wait_for(port, lambda f, m, s: True, hc.SERVER_READY_TIMEOUT,
                       "server", a.verbose, proc) is None:
            print("[w28d] FAIL server never up", flush=True); return 2
        h = hc.wait_for(port, lambda f, m, s: hc.is_game_screen(s) or m > 500.0,
                        hc.GAMEPLAY_TIMEOUT, "game", a.verbose, proc)
        if h is None:
            print("[w28d] FAIL never reached game", flush=True); return 3
        reached = True
        print(f"[w28d] game reached frame={h[0]} songMs={h[1]:.0f} dwell {a.dwell}s", flush=True)
        dl = time.time() + a.dwell
        while time.time() < dl:
            if proc.poll() is not None:
                print(f"[w28d] proc exited {proc.returncode} during dwell", flush=True); break
            hc.health(port); time.sleep(0.5)
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except Exception: pass
        try: proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()
    if not reached:
        return 4
    # Summaries
    print("\n===== IK_SHARD_VERT worst appendage (A7 baseline) =====", flush=True)
    os.system(f"grep -a 'IK_SHARD_VERT' {log_path} | tail -6")
    print("\n===== [DUALSKIN] factor table (last = worst frame) =====", flush=True)
    os.system(f"grep -a -A8 '\\[DUALSKIN\\] mesh' {log_path} | tail -40")
    print(f"\n[w28d] fixture written? {os.path.exists(a.fixture)}", flush=True)
    if os.path.exists(a.fixture):
        os.system(f"echo '--- live_pose.txt head ---'; head -6 {a.fixture}; echo '... lines:'; wc -l {a.fixture}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
