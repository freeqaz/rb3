#!/usr/bin/env python3
"""
_filter-crash-test.py — reproduce the "space/tab on song select crashes" bug on
NATIVE. Boots headless to song_select, then injects a raw pad press to open the
sort/filter panel (kPad_Select bit 8 -> kAction_ViewModify -> song_select_filter_panel
filter_enter). Also optionally exercises kPad_Start (space on web) and follow-up
nav inside the filter panel. Reports whether the process crashes.

    python3 scripts/native/_filter-crash-test.py [--verbose] [--seq pad:8,down,confirm]
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
NAV = "@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn"

def log(m): print(f"[filter-crash] {m}", flush=True)
def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p
def http_get(port, path, t=10):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try: c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()
def http_post(port, path, body, t=15):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read().decode("utf-8","replace")
    finally: c.close()
def health(port):
    try:
        st, b = http_get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode())["data"]; return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception: return None
def dta(port, expr):
    try:
        st, b = http_post(port, "/api/dta/eval", expr, t=10)
        if st != 200: return None
        d = json.loads(b); return d["data"].get("value") if d.get("ok") else None
    except Exception: return None
def wait_for(port, pred, timeout, label, verbose, proc):
    dl = time.time()+timeout; last=None
    while time.time() < dl:
        if proc.poll() is not None:
            log(f"FAIL: process exited (code {proc.returncode}) waiting for {label}"); return "CRASH"
        h = health(port)
        if h:
            f,m,s = h
            if verbose and (f,s)!=last: log(f"  ...{label}: frame={f} screen='{s}'"); last=(f,s)
            if pred(f,m,s): return h
        time.sleep(0.4)
    return None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--seq", default="pad:8")  # verbs to inject on song_select, comma-sep
    ap.add_argument("--out", default="/tmp/rb3-filter-crash")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    port = free_port()
    log_path = f"/tmp/rb3-filter-{port}.log"; logf = open(log_path, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),
                "MILO_HEADLESS":"1","RB3_DATA":DATA,"RB3_GAME_INPUT":NAV})
    log(f"launching (port {port}), log -> {log_path}")
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    rc = 1
    try:
        if wait_for(port, lambda f,m,s: True, 40, "server", args.verbose, proc) in (None,"CRASH"):
            log("FAIL: server never up"); return 1
        h = wait_for(port, lambda f,m,s: s=="song_select_screen", 120, "song_select", args.verbose, proc)
        if h in (None, "CRASH"):
            log("FAIL: never reached song_select (or crashed)"); return 1
        log(f"song_select reached: frame={h[0]}")
        time.sleep(2.0)
        for verb in args.seq.split(","):
            verb = verb.strip()
            if not verb: continue
            log(f"injecting verb: {verb}")
            st, resp = http_post(port, "/api/input", verb)
            log(f"  -> HTTP {st}: {resp[:120]}")
            # let it process a few frames
            for _ in range(15):
                if proc.poll() is not None:
                    log(f"*** CRASH: process exited (code {proc.returncode}) after '{verb}' ***"); return 2
                time.sleep(0.15)
            h2 = health(port)
            if h2 is None:
                log(f"*** HANG/CRASH: health unreachable after '{verb}' ***")
                if proc.poll() is not None:
                    log(f"    process exited code {proc.returncode}"); return 2
            else:
                log(f"  after '{verb}': frame={h2[0]} screen='{h2[2]}'")
                fm = dta(port, "{song_select_filter_panel filter_mode}")
                log(f"    filter_mode={fm}")
        # capture a screenshot
        st, data = http_get(port, "/api/screenshot", t=20)
        if st==200 and data[:8]==b"\x89PNG\r\n\x1a\n":
            with open(os.path.join(args.out,"filter.png"),"wb") as f: f.write(data)
            log(f"screenshot OK -> {args.out}/filter.png")
        if proc.poll() is None:
            log("PASS: no crash; filter sequence completed"); rc = 0
        else:
            log(f"*** CRASH at end (code {proc.returncode}) ***"); rc = 2
        return rc
    finally:
        try:
            if proc.poll() is None:
                os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                try: proc.wait(timeout=8)
                except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        log(f"engine log: {log_path}")

if __name__ == "__main__":
    sys.exit(main())
