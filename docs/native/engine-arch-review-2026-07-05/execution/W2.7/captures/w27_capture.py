#!/usr/bin/env python3
"""W2.7 black-head capture harness. Boots rb3-native headless, navigates to
gameplay (same QUICKPLAY default song as the current-state manifest), waits for a
target songMs window, captures a screenshot + drains the engine log. Env flags are
passed through from the caller so a flag matrix can be run."""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3"
DEFAULT_BIN = os.path.join(REPO, "native", "build-agent-W2.7", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")

NAV_SCRIPT = (
    "@10:start,@30:confirm,"
    "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
    "@320:down,@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@400:diff:expert,"
    "@500:nofail,@520:autohit"
)

def log(m): print(f"[w27] {m}", flush=True)

def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p

def req(port, path, method="GET", body=None, timeout=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        if body is not None:
            b = json.dumps(body).encode()
            c.request(method, path, b, {"Content-Type": "application/json"})
        else:
            c.request(method, path)
        r = c.getresponse(); data = r.read()
        return r.status, data
    finally:
        c.close()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="screenshot output path")
    ap.add_argument("--logout", required=True, help="engine log output path")
    ap.add_argument("--target-ms", type=float, default=20153.0)
    ap.add_argument("--window-ms", type=float, default=1500.0)
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--boot-timeout", type=float, default=240.0)
    args = ap.parse_args()

    port = free_port()
    env = dict(os.environ)
    env["RB3_GAME"] = "1"
    env["RB3_HTTP"] = "1"
    env["RB3_HTTP_PORT"] = str(port)
    env["MILO_HEADLESS"] = "1"
    env["RB3_FIXED_CLOCK"] = "1"
    env["RB3_GAME_INPUT"] = NAV_SCRIPT
    env["RB3_DATA"] = args.data
    logf = open(args.logout, "wb")
    proc = subprocess.Popen([args.bin], env=env,
                            stdout=logf, stderr=subprocess.STDOUT,
                            preexec_fn=os.setsid)
    try:
        # wait for server
        t0 = time.time(); ready = False
        while time.time() - t0 < 40:
            try:
                st, _ = req(port, "/api/health"); ready = (st == 200);
                if ready: break
            except Exception: pass
            if proc.poll() is not None:
                log(f"process exited early rc={proc.returncode}"); return 3
            time.sleep(0.5)
        if not ready:
            log("server never ready"); return 3
        log(f"server ready on {port}")

        # poll for gameplay + target songMs
        t0 = time.time(); captured = False; last_state=""; last_ms=-1
        while time.time() - t0 < args.boot_timeout:
            if proc.poll() is not None:
                log(f"process exited rc={proc.returncode}"); break
            try:
                st, data = req(port, "/api/health")
                j = json.loads(data) if st == 200 else {}
                h = j.get("data", j)
            except Exception:
                time.sleep(0.3); continue
            state = h.get("currentScreen", h.get("state",""))
            ms = float(h.get("songMs", -1))
            if state != last_state or abs(ms-last_ms) > 3000:
                log(f"state={state} songMs={ms:.0f}"); last_state=state; last_ms=ms
            if ms >= args.target_ms and ms <= args.target_ms + args.window_ms:
                st, png = req(port, "/api/screenshot", timeout=20)
                if st == 200 and len(png) > 1000:
                    with open(args.out, "wb") as f: f.write(png)
                    log(f"captured {args.out} at songMs={ms:.0f} ({len(png)} bytes)")
                    captured = True
                    break
            time.sleep(0.15)
        if not captured:
            log("did not reach target window / capture")
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except Exception: pass
        try: proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()
    return 0 if captured else 1

if __name__ == "__main__":
    sys.exit(main())
