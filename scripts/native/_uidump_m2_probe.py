#!/usr/bin/env python3
"""W17 R3-UIDUMP M2 verification probe.

Boots rb3-native headless with RB3_DRAWLOG_PROV=1, navigates to song_select,
scrolls a few rows, then GETs BOTH /api/uidump (authored scene-graph, joined) and
/api/drawlog?prov=1 (the sidecar). Verifies the M2 exit criteria:
  - the authored dump enumerates the song_select panel's objects
  - a focused/named fill highlight quad (ml_highlight_glasstopp) is present and
    joins to >=1 draw with a rect
  - at least one UILabel dumps text + main AND alt font materials with live colors
  - highlight_yellow.mesh present in the authored set with draws.count == 0
  - unattributed UI-cam draw fraction (draws whose scopeOwner+meshName both fail
    to appear in any authored object) is reported (<30% ideal per the plan)

Writes uidump.json + drawlog_prov.json + m2_summary.json + song_select.png to --out.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-agent-W17-UIDUMP", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
NAV_SCRIPT = ("@10:start,@30:confirm,"
              "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn")

def log(m): print(f"[uidump-m2] {m}", flush=True)
def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]; s.close(); return p

def http_get(port, path, timeout=25):
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
    ap.add_argument("--out", default="/tmp/uidump_m2")
    ap.add_argument("--scrolls", type=int, default=3)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    port = free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_GAME_INPUT": NAV_SCRIPT, "RB3_FIXED_CLOCK": "1",
                "RB3_DRAWLOG_PROV": "1"})
    logf = open(os.path.join(args.out, "boot.log"), "wb")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            preexec_fn=os.setsid)
    uidump = drawdoc = None
    try:
        t0 = time.time()
        while time.time() - t0 < 40:
            if health(port) is not None: break
            if proc.poll() is not None:
                log(f"FAIL: process exited early rc={proc.returncode}"); return 2
            time.sleep(0.4)
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
        for _ in range(args.scrolls):
            http_post(port, "/api/input", b"down"); time.sleep(0.15)
        time.sleep(0.6)
        st, png = http_get(port, "/api/screenshot")
        if st == 200:
            with open(os.path.join(args.out, "song_select.png"), "wb") as f: f.write(png)
        st, ub = http_get(port, "/api/uidump")
        if st != 200:
            log(f"FAIL: /api/uidump status {st}: {ub[:200]!r}"); return 1
        with open(os.path.join(args.out, "uidump.json"), "wb") as f: f.write(ub)
        uidump = json.loads(ub.decode("utf-8", "replace"))
        st, db = http_get(port, "/api/drawlog?prov=1")
        if st == 200:
            with open(os.path.join(args.out, "drawlog_prov.json"), "wb") as f: f.write(db)
            drawdoc = json.loads(db.decode("utf-8", "replace"))
    finally:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        except Exception: pass
        try: proc.wait(timeout=10)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass

    data = uidump.get("data", {})
    # flatten authored objects across all panels
    authored = {}   # name -> obj
    labels_with_both_fonts = []
    glass = []
    yellow = []
    for scr in data.get("screens", []):
        for pan in scr.get("panels", []):
            for o in pan.get("objects", []):
                nm = o.get("name", "")
                authored[nm] = o
                lb = o.get("label")
                if lb and "fontMatColor" in lb and "altFontMatColor" in lb:
                    labels_with_both_fonts.append({"name": nm, "text": lb.get("text"),
                        "state": lb.get("state"), "fontMat": lb.get("fontMat"),
                        "fontMatColor": lb.get("fontMatColor"),
                        "altFontMat": lb.get("altFontMat"),
                        "altFontMatColor": lb.get("altFontMatColor"),
                        "draws": o.get("draws")})
                if "highlight_glasstop" in nm:
                    glass.append({"name": nm, "showing": o.get("showing"),
                                  "mat": o.get("mat"), "draws": o.get("draws")})
                if "highlight_yellow" in nm:
                    yellow.append({"name": nm, "showing": o.get("showing"),
                                   "draws": o.get("draws")})

    # unattributed fraction over UI-cam prov draws
    ui_draws = 0; unattributed = 0
    if drawdoc:
        for d in drawdoc.get("draws", []):
            pv = d.get("prov")
            if not pv: continue
            cam = pv.get("cam", "")
            if cam in ("game.cam", "world.cam"): continue
            ui_draws += 1
            mesh = pv.get("mesh", ""); owner = pv.get("owner", "")
            if (mesh and mesh in authored) or (owner and owner in authored):
                continue
            unattributed += 1
    unattr_frac = (unattributed / ui_draws) if ui_draws else 0.0

    glass_joined = any(g.get("draws") and g["draws"].get("count", 0) > 0 for g in glass)
    yellow_zero = any(y.get("draws") and y["draws"].get("count", 1) == 0 for y in yellow)

    summary = {
        "joinEnabled": data.get("joinEnabled"),
        "screens": [s.get("name") for s in data.get("screens", [])],
        "authoredObjectCount": len(authored),
        "labelsWithBothFonts": len(labels_with_both_fonts),
        "sampleLabels": labels_with_both_fonts[:5],
        "glass": glass[:4], "glassJoined": glass_joined,
        "yellowPresent": len(yellow) > 0, "yellowZeroDraws": yellow_zero, "yellow": yellow[:4],
        "uiCamProvDraws": ui_draws, "unattributed": unattributed,
        "unattributedFraction": round(unattr_frac, 4),
    }
    with open(os.path.join(args.out, "m2_summary.json"), "w") as f:
        json.dump(summary, f, indent=2)
    log(json.dumps({k: v for k, v in summary.items() if k not in ("sampleLabels","glass","yellow")}, indent=2))
    ok = (data.get("joinEnabled") and len(authored) > 0 and glass_joined
          and len(labels_with_both_fonts) > 0 and unattr_frac < 0.30)
    log("M2 GO" if ok else "M2 REVIEW (see m2_summary.json)")
    return 0 if ok else 3

if __name__ == "__main__":
    sys.exit(main())
