#!/usr/bin/env python3
"""W17 R3-UIDUMP M1 go/no-go probe.

Boots rb3-native headless with RB3_DRAWLOG_PROV=1, navigates to song_select,
GETs /api/drawlog?prov=1, and reports the M1 exit criteria:
  - ml_highlight_glasstopp(.mesh) present with a sane (non-degenerate) rect
  - highlight_yellow.mesh absent from the drawn set ("0 draws for free")
  - degenerate-rect fraction over named UI-cam draws (go/no-go: <20% ideal)

Writes the raw ?prov=1 JSON + a summary to the path given by --out.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-agent-W17-UIDUMP", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
NAV_SCRIPT = ("@10:start,@30:confirm,"
              "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn")

def log(m): print(f"[uidump-m1] {m}", flush=True)
def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get(port, path, timeout=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()

def http_post(port, path, body, timeout=15):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        c.request("POST", path, body=body, headers={"Content-Type": "text/plain"})
        r = c.getresponse(); return r.status, r.read()
    finally: c.close()

def health(port):
    try:
        st, b = http_get(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode("utf-8", "replace"))["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception:
        return None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--out", default="/tmp/uidump_m1")
    ap.add_argument("--scrolls", type=int, default=3)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    port = free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_GAME_INPUT": NAV_SCRIPT,
                "RB3_DRAWLOG_PROV": "1"})
    logf = open(os.path.join(args.out, "boot.log"), "wb")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            preexec_fn=os.setsid)
    try:
        # wait for server
        t0 = time.time()
        while time.time() - t0 < 40:
            if health(port) is not None: break
            if proc.poll() is not None:
                log(f"FAIL: process exited early rc={proc.returncode}"); return 2
            time.sleep(0.4)
        # wait for song_select
        t0 = time.time(); reached = None
        while time.time() - t0 < 120:
            h = health(port)
            if h and h[2] == "song_select_screen": reached = h; break
            if proc.poll() is not None:
                log(f"FAIL: exited before song_select rc={proc.returncode}"); return 2
            time.sleep(0.5)
        if not reached:
            log("FAIL: never reached song_select_screen"); return 1
        log(f"song_select reached frame={reached[0]}")
        time.sleep(2.0)
        # scroll a couple rows so a focused row highlight is live
        for _ in range(args.scrolls):
            http_post(port, "/api/input", b"down"); time.sleep(0.15)
        time.sleep(0.6)
        # screenshot for visual reference
        st, png = http_get(port, "/api/screenshot", timeout=25)
        if st == 200:
            with open(os.path.join(args.out, "song_select.png"), "wb") as f: f.write(png)
        # the drawlog for the just-completed frame with prov
        st, body = http_get(port, "/api/drawlog?prov=1", timeout=25)
        if st != 200:
            log(f"FAIL: /api/drawlog?prov=1 status {st}"); return 1
        with open(os.path.join(args.out, "drawlog_prov.json"), "wb") as f: f.write(body)
        doc = json.loads(body.decode("utf-8", "replace"))
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except Exception: pass
        try: proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass

    draws = doc.get("draws", [])
    prov_avail = doc.get("provAvailable")
    names = {}
    ui_named, ui_degen = 0, 0
    glass = []
    yellow = []
    for d in draws:
        pv = d.get("prov")
        if not pv: continue
        nm = pv.get("mesh", "")
        names[nm] = names.get(nm, 0) + 1
        # UI-cam heuristic: a UI/HUD/panel cam name (not game.cam/world.cam)
        cam = pv.get("cam", "")
        is_ui = cam not in ("game.cam", "world.cam") and nm != ""
        if is_ui:
            ui_named += 1
            if pv.get("rectKind") == 2 or (pv.get("rect", [0,0,-1])[2] < 0):
                ui_degen += 1
        if "highlight_glasstop" in nm:
            glass.append({"mesh": nm, "rect": pv.get("rect"), "rectKind": pv.get("rectKind"),
                          "cam": cam, "matColor": pv.get("matColor"),
                          "boundColor": pv.get("boundColor"), "pass": pv.get("pass"),
                          "panel": pv.get("panel"), "owner": pv.get("owner")})
        if "highlight_yellow" in nm:
            yellow.append({"mesh": nm, "rect": pv.get("rect")})

    degen_frac = (ui_degen / ui_named) if ui_named else 0.0
    summary = {
        "provAvailable": prov_avail,
        "totalDraws": len(draws),
        "uiNamedDraws": ui_named,
        "uiDegenerateDraws": ui_degen,
        "uiDegenerateFraction": round(degen_frac, 4),
        "glasstop_present": len(glass) > 0,
        "glasstop": glass[:4],
        "highlight_yellow_present": len(yellow) > 0,
        "highlight_yellow": yellow[:4],
        "distinct_named_meshes": len(names),
    }
    with open(os.path.join(args.out, "m1_summary.json"), "w") as f:
        json.dump(summary, f, indent=2)
    log(json.dumps(summary, indent=2))
    # go/no-go
    ok = prov_avail and summary["glasstop_present"] and not summary["highlight_yellow_present"] \
         and degen_frac < 0.20
    log("M1 GO" if ok else "M1 REVIEW (see summary)")
    return 0 if ok else 3

if __name__ == "__main__":
    sys.exit(main())
