#!/usr/bin/env python3
"""W2.8f B.S2 — reproduce the A.S3 forearm sighting WITH the trustworthy instrument live.

Runs the SAME two A.S3 nav sequences (partdiff wipe + gameplay ceiling-forearm) but with
RB3_HANDS_ATTACH_PROBE=1 so the Tier-1/Tier-2 palette invariants are co-sampled ON the exact
sighting frames. Captures screenshots at the sighting window (visible-smear confirmation) and
the probe stderr, then correlates: on the high-wext (visible smear) frames, what does the
corrected Tier-2 EXACT joint-attachment read? (A7: does the trustworthy metric track the smear?)

Engine binary: build-agent-W2.8f (commit 4c93608 probe). Probe getenv-gated, render-inert.
"""
import argparse, http.client, json, os, re, signal, socket, subprocess, sys, time

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-agent-W2.8f", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")

NAV_PARTDIFF = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
    "@220:select:qp_quickplay.btn,@320:down,"
    "@350:msg:music_library:select_highlighted_node")
NAV_GAMEPLAY = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
    "@220:select:qp_quickplay.btn,@320:down,"
    "@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@410:diff:expert,@440:nofail")


def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p

def hget(port, path, t=20):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=t)
    try: c.request("GET", path); r = c.getresponse(); return r.status, r.read()
    finally: c.close()

def health(port):
    try:
        st, b = hget(port, "/api/health")
        if st != 200: return None
        d = json.loads(b.decode())["data"]
        return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception: return None

def shot(port, path):
    st, data = hget(port, "/api/screenshot", t=20)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n": return False
    open(path, "wb").write(data); return True

def log(m): print(f"[bs2] {m}", flush=True)

def launch(nav, logpath):
    port = free_port(); logf = open(logpath, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),
        "MILO_HEADLESS":"1","RB3_DATA":DATA,"RB3_GAME_INPUT":nav,
        "RB3_FIXED_CLOCK":"1","RB3_HANDS_ATTACH_PROBE":"1"})
    proc = subprocess.Popen([BIN], env=env, stdout=logf, stderr=subprocess.STDOUT,
        cwd=REPO, start_new_session=True)
    return proc, port, logf

def teardown(proc, logf):
    try:
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
        try: proc.wait(timeout=8)
        except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except Exception: pass
    logf.close()

def wait_screen(port, proc, screen, timeout):
    dl = time.time()+timeout
    while time.time()<dl:
        if proc.poll() is not None: return None
        h = health(port)
        if h and h[2]==screen: return h
        time.sleep(0.25)
    return None

def wait_songms(port, proc, ms, timeout):
    dl = time.time()+timeout
    while time.time()<dl:
        if proc.poll() is not None: return None
        h = health(port)
        if h and h[2]=="game_screen" and h[1]>=ms: return h
        time.sleep(0.15)
    return None

def wait_frame(port, proc, tgt, timeout):
    dl = time.time()+timeout
    while time.time()<dl:
        if proc.poll() is not None: return None
        h = health(port)
        if h and h[0]>=tgt: return h
        time.sleep(0.1)
    return None


# probe line parser: two-line record starting with mesh line then TIER1/TIER2 lines.
HDR = re.compile(r"\[HANDS_ATTACH\] mesh='([^']+)' owner='[^']+' frame=(\d+) nb=\d+ wext=([\d.]+)")
T1  = re.compile(r"TIER1 rest-coherence: worst=([\d.]+)deg .* count\(>5deg\)=(\d+)")
T2  = re.compile(r"TIER2 joint-attach\(PRIMARY\): worst=([\d.]+)u .* exactJoint=([\d.]+)u")

def parse_records(logpath, mesh_filter="hands_naked"):
    recs = []
    lines = open(logpath, errors="replace").read().splitlines()
    i = 0
    while i < len(lines):
        m = HDR.search(lines[i])
        if m and mesh_filter in m.group(1):
            mesh, frame, wext = m.group(1), int(m.group(2)), float(m.group(3))
            t1w=t2w=t2e=None; t1c=None
            for j in (i+1, i+2, i+3):
                if j>=len(lines): break
                a=T1.search(lines[j]);
                if a: t1w=float(a.group(1)); t1c=int(a.group(2))
                b=T2.search(lines[j])
                if b: t2w=float(b.group(1)); t2e=float(b.group(2))
            recs.append(dict(mesh=mesh,frame=frame,wext=wext,t1w=t1w,t1c=t1c,t2w=t2w,t2e=t2e))
        i += 1
    return recs


def run_gameplay(out):
    logp = os.path.join(out, "gameplay.log")
    proc, port, logf = launch(NAV_GAMEPLAY, logp)
    caps = []
    try:
        h = wait_screen(port, proc, "game_screen", 150)
        if h is None: log("gameplay: FAIL never reached game_screen"); return
        log(f"game_screen arrived frame={h[0]} songMs={h[1]:.0f}")
        for ms in [2000, 6000, 12000, 20000, 30000, 45000]:
            hh = wait_songms(port, proc, ms, 60)
            if hh is None: log(f"gameplay ms={ms}: timeout"); continue
            path = os.path.join(out, f"gp_ms{ms:05d}.png")
            ok = shot(port, path)
            caps.append((ms, hh[0], ok))
            log(f"gameplay ms>={ms}: frame={hh[0]} songMs={hh[1]:.0f} shot={'OK' if ok else 'FAIL'}")
        time.sleep(1.0)
    finally:
        teardown(proc, logf)
    return logp

def run_partdiff(out):
    logp = os.path.join(out, "partdiff.log")
    proc, port, logf = launch(NAV_PARTDIFF, logp)
    try:
        h = wait_screen(port, proc, "part_difficulty_screen", 120)
        if h is None: log("partdiff: FAIL never reached screen"); return
        arrived = h[0]; log(f"partdiff arrived frame={arrived}")
        for tgt in [0, 60, 120, 180, 360]:
            hh = wait_frame(port, proc, arrived+tgt, 30)
            if hh is None: continue
            path = os.path.join(out, f"pd_{tgt:03d}.png"); ok = shot(port, path)
            log(f"partdiff +{tgt:>3}f: frame={hh[0]} shot={'OK' if ok else 'FAIL'}")
        time.sleep(0.5)
    finally:
        teardown(proc, logf)
    return logp


def summarize(logp, tag):
    recs = parse_records(logp, "hands_naked")
    if not recs:
        log(f"{tag}: no hands_naked records"); return
    # sort by wext desc; the highest-wext = the visible smear frames
    recs_by_w = sorted(recs, key=lambda r: r["wext"], reverse=True)
    wmax = recs_by_w[0]["wext"]; wmin = recs_by_w[-1]["wext"]
    # top-decile-wext (the visible-smear frames): report Tier-2 exact stats
    hi = [r for r in recs if r["wext"] >= wmin + 0.8*(wmax-wmin) and r["t2e"] is not None]
    lo = [r for r in recs if r["wext"] <= wmin + 0.2*(wmax-wmin) and r["t2e"] is not None]
    def stat(rs, k):
        vs=[r[k] for r in rs if r[k] is not None]
        return (min(vs),sum(vs)/len(vs),max(vs)) if vs else (0,0,0)
    print(f"\n===== {tag}: hands_naked  N={len(recs)}  wext[{wmin:.1f}..{wmax:.1f}] =====")
    print(f"  ALL     : t2exact {stat(recs,'t2e')}  t2approx {stat(recs,'t2w')}  t1 {stat(recs,'t1w')}")
    if hi: print(f"  HI-wext (visible smear, wext>={wmin+0.8*(wmax-wmin):.1f}, n={len(hi)}): "
                 f"t2exact {stat(hi,'t2e')}  t2approx {stat(hi,'t2w')}  t1 {stat(hi,'t1w')}")
    if lo: print(f"  LO-wext (near-rest,  wext<={wmin+0.2*(wmax-wmin):.1f}, n={len(lo)}): "
                 f"t2exact {stat(lo,'t2e')}  t2approx {stat(lo,'t2w')}  t1 {stat(lo,'t1w')}")
    # top-8 highest-wext frames verbatim
    print("  --- top-8 highest-wext frames (the SIGHTING frames) ---")
    print("   frame    wext  t2exact  t2approx   t1worst t1cnt")
    for r in recs_by_w[:8]:
        print(f"  {r['frame']:6d}  {r['wext']:6.1f}  {(r['t2e'] or -1):7.2f}  {(r['t2w'] or -1):8.2f}   "
              f"{(r['t1w'] or -1):7.1f} {r['t1c'] if r['t1c'] is not None else -1}")
    # co-variation on THIS run
    import math
    pts=[(r["wext"],r["t2e"]) for r in recs if r["t2e"] is not None]
    if len(pts)>3:
        xs=[p[0] for p in pts]; ys=[p[1] for p in pts]
        mx=sum(xs)/len(xs); my=sum(ys)/len(ys)
        num=sum((x-mx)*(y-my) for x,y in pts)
        den=math.sqrt(sum((x-mx)**2 for x in xs)*sum((y-my)**2 for y in ys))
        corr=num/den if den>1e-9 else float('nan')
        print(f"  A7 co-variation on this run: corr(wext, t2exact) = {corr:+.3f}   (|corr|>~0.6 => tracks)")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/tmp/wave11-bs2")
    ap.add_argument("--mode", choices=["gameplay","partdiff","both"], default="both")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    if a.mode in ("gameplay","both"):
        lp = run_gameplay(a.out)
        if lp: summarize(lp, "GAMEPLAY (ceiling-forearm sighting)")
    if a.mode in ("partdiff","both"):
        lp = run_partdiff(a.out)
        if lp: summarize(lp, "PARTDIFF (wipe-transition sighting)")
    log(f"done -> {a.out}")
