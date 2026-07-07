#!/usr/bin/env python3
"""W2.8g B-S2 — A/B the untried SPACE-axis cell RB3_HANDS_SHELL_FIX (own-live + bound-rest).

Reuses the A.S3 gameplay ceiling-forearm sighting with RB3_HANDS_ATTACH_PROBE=1 so wext +
Tier-2 joint-attach are co-sampled. Runs OFF-arm and ON-arm (RB3_HANDS_SHELL_FIX=1) on the
build-agent-W2.8g binary. Gate: wext 95-106u -> <=60u WITHOUT freezing (wext must still VARY);
Tier-2 exact stays <=1u.  Also captures band screenshots for the E1 review.
"""
import argparse, http.client, json, os, signal, socket, subprocess, time, re, math

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-agent-W2.8g", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
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
        d = json.loads(b.decode())["data"]; return int(d["frame"]), float(d["songMs"]), str(d["currentScreen"])
    except Exception: return None
def shot(port, path):
    st, data = hget(port, "/api/screenshot", t=20)
    if st != 200 or not data or data[:8] != b"\x89PNG\r\n\x1a\n": return False
    open(path, "wb").write(data); return True
def log(m): print(f"[ab] {m}", flush=True)
def launch(extra):
    port = free_port(); logf = open(extra["_log"], "w")
    env = dict(os.environ)
    env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),
        "MILO_HEADLESS":"1","RB3_DATA":DATA,"RB3_GAME_INPUT":NAV_GAMEPLAY,
        "RB3_FIXED_CLOCK":"1","RB3_HANDS_ATTACH_PROBE":"1"})
    for k,v in extra.items():
        if not k.startswith("_"): env[k]=v
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
    dl=time.time()+timeout
    while time.time()<dl:
        if proc.poll() is not None: return None
        h=health(port)
        if h and h[2]==screen: return h
        time.sleep(0.25)
    return None
def wait_songms(port, proc, ms, timeout):
    dl=time.time()+timeout
    while time.time()<dl:
        if proc.poll() is not None: return None
        h=health(port)
        if h and h[2]=="game_screen" and h[1]>=ms: return h
        time.sleep(0.15)
    return None

HDR = re.compile(r"\[HANDS_ATTACH\] mesh='([^']+)' owner='[^']+' frame=(\d+) nb=\d+ wext=([\d.]+)")
T1  = re.compile(r"TIER1 rest-coherence: worst=([\d.]+)deg .* count\(>5deg\)=(\d+)")
T2  = re.compile(r"TIER2 joint-attach\(PRIMARY\): worst=([\d.]+)u .* exactJoint=([\d.]+)u")
def parse_records(logpath, mf="hands_naked"):
    recs=[]; lines=open(logpath,errors="replace").read().splitlines(); i=0
    while i<len(lines):
        m=HDR.search(lines[i])
        if m and mf in m.group(1):
            frame,wext=int(m.group(2)),float(m.group(3)); t1w=t2w=t2e=t1c=None
            for j in (i+1,i+2,i+3):
                if j>=len(lines): break
                a=T1.search(lines[j])
                if a: t1w=float(a.group(1)); t1c=int(a.group(2))
                b=T2.search(lines[j])
                if b: t2w=float(b.group(1)); t2e=float(b.group(2))
            recs.append(dict(frame=frame,wext=wext,t1w=t1w,t1c=t1c,t2w=t2w,t2e=t2e))
        i+=1
    return recs
def run(out, tag, extra):
    lp=os.path.join(out,f"{tag}.log"); extra["_log"]=lp
    proc,port,logf=launch(extra)
    try:
        h=wait_screen(port,proc,"game_screen",150)
        if h is None: log(f"{tag}: FAIL never reached game_screen"); return lp
        log(f"{tag}: game_screen frame={h[0]} songMs={h[1]:.0f}")
        for ms in [2000,6000,12000,20000,30000]:
            hh=wait_songms(port,proc,ms,60)
            if hh is None: log(f"{tag} ms={ms}: timeout"); continue
            ok=shot(port,os.path.join(out,f"{tag}_ms{ms:05d}.png"))
            log(f"{tag} ms>={ms}: frame={hh[0]} shot={'OK' if ok else 'FAIL'}")
        time.sleep(1.0)
    finally:
        teardown(proc,logf)
    return lp
def summarize(lp, tag):
    recs=parse_records(lp)
    if not recs: log(f"{tag}: NO hands_naked records (mesh may not draw)"); return None
    ws=[r["wext"] for r in recs]
    uniq=sorted(set(round(w,1) for w in ws))
    t2e=[r["t2e"] for r in recs if r["t2e"] is not None]
    def stat(v): return (min(v),sum(v)/len(v),max(v)) if v else (0,0,0)
    print(f"\n===== {tag}: N={len(recs)} =====")
    print(f"  wext   min/mean/max = {stat(ws)[0]:.1f}/{stat(ws)[1]:.1f}/{stat(ws)[2]:.1f}")
    print(f"  wext   distinct rounded values (n={len(uniq)}): {uniq[:16]}{' ...' if len(uniq)>16 else ''}")
    print(f"  t2exact min/mean/max = {stat(t2e)[0]:.2f}/{stat(t2e)[1]:.2f}/{stat(t2e)[2]:.2f}")
    # freeze detector: a frozen arm pins wext to <=3 discrete values
    frozen = len(uniq) <= 3
    print(f"  FREEZE-CHECK: {'FROZEN (<=3 discrete wext values)' if frozen else 'animating (wext varies)'}")
    return dict(N=len(recs), wmin=stat(ws)[0], wmean=stat(ws)[1], wmax=stat(ws)[2],
                nuniq=len(uniq), t2max=stat(t2e)[2] if t2e else -1, frozen=frozen)

if __name__=="__main__":
    ap=argparse.ArgumentParser(); ap.add_argument("--out",default="/tmp/wave12-bs2ab"); a=ap.parse_args()
    os.makedirs(a.out,exist_ok=True)
    off=summarize(run(a.out,"OFF",{}),"OFF (default)")
    on =summarize(run(a.out,"ON",{"RB3_HANDS_SHELL_FIX":"1"}),"ON (RB3_HANDS_SHELL_FIX)")
    print("\n===== VERDICT =====")
    if off and on:
        print(f"  wext max: OFF {off['wmax']:.1f}u -> ON {on['wmax']:.1f}u  (gate: <=60u)")
        print(f"  wext distinct: OFF {off['nuniq']} -> ON {on['nuniq']}  (freeze if ON<=3)")
        print(f"  t2exact max: OFF {off['t2max']:.2f} -> ON {on['t2max']:.2f}  (gate: <=1u)")
        passed = (on['wmax']<=60.0 and not on['frozen'] and on['t2max']<=1.0)
        print(f"  RESULT: {'FIXED (gate PASS)' if passed else 'NOT-FIXED (gate FAIL)'}")
    json.dump({"off":off,"on":on}, open(os.path.join(a.out,"summary.json"),"w"), indent=2)
