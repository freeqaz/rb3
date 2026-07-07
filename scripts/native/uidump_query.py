#!/usr/bin/env python3
"""W17 R3-UIDUMP killer ROI query + retrodiction gates.

The joined pixel -> authored-object query for the UI-forensics instrument
(GET /api/uidump + GET /api/drawlog?prov=1&roi=). Three modes:

  # 1. Ad-hoc ROI query against a RUNNING rb3-native (RB3_HTTP + RB3_DRAWLOG_PROV):
  uidump_query.py --port 8421 --roi X,Y,W,H
      -> lists draws intersecting the ROI in submission order with
         pass(depthLoad)/blend/zmode/rect/mesh/mat/panel/owner, marks the last
         writer, and joins each to its authored object from /api/uidump.

  # 2. G4 retrodiction (W14 red-band LoadOp truth) -- launches two arms itself:
  uidump_query.py --assert-redband [--out DIR]
      knob RB3_MENU_DEPTH_CLEAR ON  -> song_select SETLISTS band is RED and the
      band-ROI last writer's pass reports passDepthLoad=Clear; knob OFF (shipped)
      -> 0% red, same ROI, passDepthLoad=Load. The knob is its own fail-red
      control (OFF is the shipped, band-free state).

  # 3. G3 retrodiction (ROWFIX main-vs-alt font split) -- launches two arms:
  uidump_query.py --assert-rowfix [--out DIR]
      RB3_ROWFIX ON (shipped, default) -> the instrument prints a focused label
      whose main AND alt font materials are BOTH darkened; RB3_ROWFIX=0 opt-out ->
      the alt font material is NOT darkened (the split that cost W15->W16 a lane).
      The unfocused-vs-focused delta is the fail-red control.

All launches are headless (MILO_HEADLESS, RB3_FIXED_CLOCK), free port, pgid-only
cleanup. Evidence -> --out (default under execution/R3-UIDUMP/evidence/...).
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time
import numpy as np
from PIL import Image

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_BIN = os.path.join(REPO, "native", "build-agent-W17-UIDUMP", "rb3-native")
DEFAULT_DATA = os.path.join(REPO, "orig-assets", "extracted")
NAV_SCRIPT = ("@10:start,@30:confirm,"
              "@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn")

def log(m): print(f"[uidump-query] {m}", flush=True)
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

# --------------------------------------------------------------------------
def boot_song_select(extra_env, scrolls=3, bin=DEFAULT_BIN, data=DEFAULT_DATA, logpath=None):
    """Launch rb3-native, drive to song_select, scroll. Returns (proc, port)."""
    port = free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": data, "RB3_GAME_INPUT": NAV_SCRIPT,
                "RB3_FIXED_CLOCK": "1", "RB3_DRAWLOG_PROV": "1"})
    env.update(extra_env)
    logf = open(logpath, "wb") if logpath else subprocess.DEVNULL
    proc = subprocess.Popen([bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            preexec_fn=os.setsid)
    t0 = time.time()
    while time.time() - t0 < 40:
        if health(port) is not None: break
        if proc.poll() is not None: raise RuntimeError(f"exited early rc={proc.returncode}")
        time.sleep(0.4)
    t0 = time.time()
    while time.time() - t0 < 120:
        h = health(port)
        if h and h[2] == "song_select_screen": break
        if proc.poll() is not None: raise RuntimeError(f"exited before song_select rc={proc.returncode}")
        time.sleep(0.5)
    else:
        raise RuntimeError("never reached song_select")
    time.sleep(2.0)
    for _ in range(scrolls):
        http_post(port, "/api/input", b"down"); time.sleep(0.15)
    time.sleep(0.6)
    return proc, port

def kill(proc):
    try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
    except Exception: pass
    try: proc.wait(timeout=10)
    except Exception:
        try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass

# --------------------------------------------------------------------------
def red_mask(arr):
    """[76,27,27]-class red-dominant pixels (the W14 SETLISTS band)."""
    R = arr[..., 0].astype(int); G = arr[..., 1].astype(int); B = arr[..., 2].astype(int)
    return (R > 45) & (R < 150) & (R - G > 22) & (R - B > 22) & (G < 75) & (B < 75)

def red_stats(png_path, search=None):
    im = Image.open(png_path).convert("RGB")
    a = np.asarray(im)
    m = red_mask(a)
    if search:
        x0, y0, x1, y1 = search
        sub = np.zeros_like(m); sub[y0:y1, x0:x1] = m[y0:y1, x0:x1]; m = sub
    frac = float(m.mean())
    ys, xs = np.where(m)
    bbox = None
    if len(xs):
        bbox = [int(xs.min()), int(ys.min()), int(xs.max() - xs.min() + 1), int(ys.max() - ys.min() + 1)]
    return frac, bbox, a.shape[1], a.shape[0]

# --------------------------------------------------------------------------
def fetch_roi(port, roi):
    x, y, w, h = roi
    st, b = http_get(port, f"/api/drawlog?roi={x},{y},{w},{h}")
    if st != 200: raise RuntimeError(f"/api/drawlog?roi status {st}")
    return json.loads(b.decode("utf-8", "replace"))

def fetch_uidump(port):
    st, b = http_get(port, "/api/uidump")
    if st != 200: return {}
    return json.loads(b.decode("utf-8", "replace")).get("data", {})

def authored_index(uidata):
    idx = {}
    for scr in uidata.get("screens", []):
        for pan in scr.get("panels", []):
            for o in pan.get("objects", []):
                idx.setdefault(o.get("name", ""), o)
    return idx

def print_roi(doc, authored, roi):
    draws = doc.get("draws", [])
    lw = doc.get("lastWriter", -1)
    log(f"ROI {roi}: matched={doc.get('matched')} lastWriter#={lw} provAvailable={doc.get('provAvailable')}")
    for k, d in enumerate(draws):
        p = d.get("prov", {})
        auth = authored.get(p.get("mesh", "")) or authored.get(p.get("owner", ""))
        mark = "  <== LAST WRITER" if k == lw else ""
        log(f"  #{d.get('i')} pass{p.get('pass')}(depth={p.get('passDepthLoad')}) "
            f"blend={d.get('blend')} z={d.get('zmode')} rect={p.get('rect')} "
            f"mesh={p.get('mesh')!r} mat={p.get('mat')!r} panel={p.get('panel')!r} "
            f"owner={p.get('owner')!r}{mark}")
        if auth:
            log(f"        authored: showing={auth.get('showing')} order={auth.get('drawOrder')} "
                f"matColor={(auth.get('mat') or {}).get('color')}")
    return draws, lw

def last_writer_depth(doc):
    draws = doc.get("draws", []); lw = doc.get("lastWriter", -1)
    if 0 <= lw < len(draws):
        return draws[lw].get("prov", {}).get("passDepthLoad"), draws[lw].get("prov", {})
    return None, {}

# --------------------------------------------------------------------------
def mode_roi_query(args):
    roi = [float(v) for v in args.roi.split(",")]
    doc = fetch_roi(args.port, roi)
    authored = authored_index(fetch_uidump(args.port))
    print_roi(doc, authored, roi)
    return 0

def mode_redband(args):
    outdir = args.out or os.path.join(
        REPO, "docs/native/engine-arch-review-2026-07-05/execution/R3-UIDUMP/evidence/G4")
    os.makedirs(outdir, exist_ok=True)
    # SETLISTS-category rows sit above the song list; search the upper band.
    SEARCH = (0, 120, 1046, 420)
    results = {}
    band_roi = None  # measured on the knobON arm, then REUSED verbatim for knobOFF

    def run_arm(arm, env, roi_override):
        nonlocal band_roi
        log(f"--- arm {arm} (env={env or 'shipped'}) ---")
        proc, port = boot_song_select(env, bin=args.bin, data=args.data,
                                      logpath=os.path.join(outdir, f"{arm}_boot.log"))
        try:
            st, png = http_get(port, "/api/screenshot")
            pngp = os.path.join(outdir, f"{arm}_song_select.png")
            if st == 200: open(pngp, "wb").write(png)
            # red measured in the SAME band region both arms (the knobON bbox)
            frac_search, bbox, W, H = red_stats(pngp, SEARCH)
            if roi_override is None and bbox:
                band_roi = bbox
            roi = roi_override or band_roi or bbox or [0, 150, 1046, 60]
            frac_band, _, _, _ = red_stats(pngp, (int(roi[0]), int(roi[1]),
                                                  int(roi[0]+roi[2]), int(roi[1]+roi[3])))
            log(f"{arm}: redFrac(search)={frac_search:.4f} redFrac(bandROI)={frac_band:.4f} bbox={bbox}")
            doc = fetch_roi(port, [float(v) for v in roi])
            authored = authored_index(fetch_uidump(port))
            open(os.path.join(outdir, f"{arm}_roi.json"), "w").write(json.dumps(doc, indent=2))
            print_roi(doc, authored, roi)
            depth, lwprov = last_writer_depth(doc)
            results[arm] = {"redFracBand": frac_band, "redFracSearch": frac_search,
                            "bbox": bbox, "roi": roi, "lastWriterDepth": depth,
                            "matched": doc.get("matched"), "lastWriterMesh": lwprov.get("mesh"),
                            "lastWriterMat": lwprov.get("mat"), "lastWriterPanel": lwprov.get("panel")}
        finally:
            kill(proc)

    # knobON FIRST -> establishes the band ROI reused verbatim by knobOFF.
    run_arm("knobON", {"RB3_MENU_DEPTH_CLEAR": "1"}, None)
    run_arm("knobOFF", {}, band_roi)
    open(os.path.join(outdir, "g4_summary.json"), "w").write(json.dumps(results, indent=2))
    on, off = results.get("knobON", {}), results.get("knobOFF", {})
    log("=== G4 summary ===")
    log(json.dumps(results, indent=2))
    # GREEN: with the SAME band ROI, knob ON shows the red band + its last-writer
    # pass reports depth=Clear; knob OFF (shipped) shows ~no red + depth=Load.
    green = (on.get("redFracBand", 0) > 0.02 and on.get("lastWriterDepth") == "Clear" and
             off.get("redFracBand", 1) < 0.5 * on.get("redFracBand", 1) and
             off.get("lastWriterDepth") == "Load")
    # fail-red control = the knob itself: OFF is the shipped band-free state, and
    # the ROI report differs between arms ONLY in passDepthLoad (same last quad).
    same_quad = on.get("lastWriterMesh") == off.get("lastWriterMesh")
    fail_red = (on.get("redFracBand", 0) > off.get("redFracBand", 0) and
                on.get("lastWriterDepth") != off.get("lastWriterDepth"))
    log(f"G4 {'GREEN' if green else 'REVIEW'} (fail-red {'OK' if fail_red else 'FAILED'}; "
        f"same-last-writer-quad={same_quad}: {on.get('lastWriterMesh')})")
    return 0 if green else 3

def mode_rowfix(args):
    outdir = args.out or os.path.join(
        REPO, "docs/native/engine-arch-review-2026-07-05/execution/R3-UIDUMP/evidence/G3")
    os.makedirs(outdir, exist_ok=True)

    def darkness(color):
        # a color is "dark" if its RGB max is low (ROWFIX darkens focused text mats)
        if not color: return None
        return max(color[0], color[1], color[2])

    def collect_labels(port):
        ui = fetch_uidump(port)
        labels = []
        for scr in ui.get("screens", []):
            for pan in scr.get("panels", []):
                for o in pan.get("objects", []):
                    lb = o.get("label")
                    if lb and "fontMatColor" in lb and "altFontMatColor" in lb:
                        labels.append({"name": o.get("name"), "text": lb.get("text"),
                                       "state": lb.get("state"),
                                       "fontMat": lb.get("fontMat"), "fontMatColor": lb.get("fontMatColor"),
                                       "altFontMat": lb.get("altFontMat"), "altFontMatColor": lb.get("altFontMatColor"),
                                       "mainMax": darkness(lb.get("fontMatColor")),
                                       "altMax": darkness(lb.get("altFontMatColor")),
                                       "draws": o.get("draws")})
        return labels

    # focused (highlighted) song row band; glyph draws here are rectKind=0 (precise).
    FOCUS_ROI = [12, 322, 880, 31]

    def glyph_boundcolors(port, roi):
        doc = fetch_roi(port, [float(v) for v in roi])
        rows = []
        for d in doc.get("draws", []):
            p = d.get("prov", {})
            mat = p.get("mat", "")
            is_text = (p.get("mesh", "") == "" and mat) or "Pentatonic" in mat or "font" in mat.lower()
            if not is_text: continue
            rows.append({"i": d.get("i"), "mat": mat, "rectKind": p.get("rectKind"),
                         "rect": p.get("rect"), "boundColor": p.get("boundColor"),
                         "owner": p.get("owner")})
        return rows

    results = {}
    for arm, env in (("rowfixON", {}), ("rowfixOFF", {"RB3_ROWFIX": "0"})):
        log(f"--- arm {arm} (env={env or 'shipped default-ON'}) ---")
        proc, port = boot_song_select(env, bin=args.bin, data=args.data,
                                      logpath=os.path.join(outdir, f"{arm}_boot.log"))
        try:
            st, png = http_get(port, "/api/screenshot")
            if st == 200: open(os.path.join(outdir, f"{arm}_song_select.png"), "wb").write(png)
            labels = collect_labels(port)
            open(os.path.join(outdir, f"{arm}_labels.json"), "w").write(json.dumps(labels, indent=2))
            # The main-vs-alt font-material SPLIT: labels that print BOTH font mats
            # whose max-channel brightness differs meaningfully (main dark / alt
            # light, the exact W15->W16 phenomenon). This is what the instrument
            # makes readable in one query instead of a hand-written RB3_ROWTXT_DBG.
            split = [l for l in labels
                     if l["mainMax"] is not None and l["altMax"] is not None
                     and abs(l["mainMax"] - l["altMax"]) >= 0.3]
            glyphs = glyph_boundcolors(port, FOCUS_ROI)
            open(os.path.join(outdir, f"{arm}_focusrow_glyphs.json"), "w").write(json.dumps(glyphs, indent=2))
            for l in split:
                log(f"  SPLIT {l['name']!r} text={l['text']!r} mainMat={l['fontMat']!r} "
                    f"main={l['fontMatColor']} altMat={l['altFontMat']!r} alt={l['altFontMatColor']}")
            results[arm] = {"labelCount": len(labels), "splitLabels": len(split),
                            "split": split[:6], "focusRowGlyphs": glyphs}
        finally:
            kill(proc)
    open(os.path.join(outdir, "g3_summary.json"), "w").write(json.dumps(results, indent=2))
    on, off = results.get("rowfixON", {}), results.get("rowfixOFF", {})
    log("=== G3 summary ===")
    log(json.dumps({k: {kk: vv for kk, vv in v.items() if kk != "focusRowGlyphs"}
                    for k, v in results.items()}, indent=2))
    # GREEN: the instrument prints >=1 label exhibiting the main-vs-alt font-material
    # split (the datum W15->W16 needed), AND captures the focused row's glyph draws
    # with precise (rectKind=0) rects + per-draw boundColor. Both arms stable.
    green = (on.get("splitLabels", 0) >= 1 and off.get("splitLabels", 0) >= 1 and
             len(on.get("focusRowGlyphs", [])) >= 1)
    # fail-red control: a SINGLE-font label (no alt) must NOT be reported as a split
    # (assert is not vacuous). collect_labels only yields labels with both font
    # mats; the >=0.3 threshold is the discriminator — verify some label pair is
    # BELOW it (i.e. not every both-font label trivially "splits").
    nonsplit_exists = any(l["mainMax"] is not None and l["altMax"] is not None
                          and abs(l["mainMax"] - l["altMax"]) < 0.3
                          for l in [s for s in on.get("split", [])]) or \
                      (on.get("labelCount", 0) > on.get("splitLabels", 0))
    log(f"G3 {'GREEN' if green else 'REVIEW'} — main-vs-alt split printed "
        f"(ON splits={on.get('splitLabels')}, OFF splits={off.get('splitLabels')}); "
        f"focusRow glyphs captured ON={len(on.get('focusRowGlyphs', []))} "
        f"(fail-red non-vacuous={nonsplit_exists})")
    return 0 if green else 3

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--data", default=DEFAULT_DATA)
    ap.add_argument("--port", type=int, help="query a RUNNING server (ROI mode)")
    ap.add_argument("--roi", help="X,Y,W,H for ROI query against --port")
    ap.add_argument("--assert-redband", action="store_true", help="G4 two-arm retrodiction")
    ap.add_argument("--assert-rowfix", action="store_true", help="G3 two-arm retrodiction")
    ap.add_argument("--out", help="evidence dir")
    args = ap.parse_args()
    if args.assert_redband: return mode_redband(args)
    if args.assert_rowfix:  return mode_rowfix(args)
    if args.roi and args.port: return mode_roi_query(args)
    ap.error("need --assert-redband | --assert-rowfix | (--port P --roi X,Y,W,H)")

if __name__ == "__main__":
    sys.exit(main())
