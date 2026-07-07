#!/usr/bin/env python3
"""W2.8g B-S1 — Instrument B (per-vertex shell invariant) axis-discrimination harness.

Reproduces the A.S3 gameplay + partdiff hand-smear sightings with RB3_HANDS_ATTACH_PROBE=1
RB3_HANDS_INSTR_B=1 so the shell invariant ||s(v)-shat(v)|| is co-sampled ON the smear frames.
For each captured [INSTR_B] record it collects wext, shellMax/shellMean, and the task-4 rigidity
discriminator corr(radius,resid). Then it decides the pre-registered axis branch:
  - shellMax co-varies with wext (RED)  -> composition / SPACE axis
  - shellMax ~0 while wext RED (GREEN)   -> weights / indices / decode axis
and cross-checks against the clean body control (greaserjacket_resource).

Engine binary: native/build-agent-W2.8g/rb3-native (Instrument B, render-inert, getenv-gated).
"""
import argparse, http.client, json, math, os, re, signal, socket, subprocess, time

REPO = "/home/free/code/milohax/rb3"
BIN = os.path.join(REPO, "native", "build-agent-W2.8g", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")

NAV_GAMEPLAY = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
    "@220:select:qp_quickplay.btn,@320:down,"
    "@350:msg:music_library:select_highlighted_node,"
    "@380:part:guitar,@410:diff:expert,@440:nofail")
NAV_PARTDIFF = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,"
    "@220:select:qp_quickplay.btn,@320:down,"
    "@350:msg:music_library:select_highlighted_node")

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

def log(m): print(f"[bs1] {m}", flush=True)

def launch(nav, logpath):
    port = free_port(); logf = open(logpath, "w")
    env = dict(os.environ)
    env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),
        "MILO_HEADLESS":"1","RB3_DATA":DATA,"RB3_GAME_INPUT":nav,
        "RB3_FIXED_CLOCK":"1",
        # selector includes greaserjacket_resource so the clean-body CONTROL also runs Instrument B
        "RB3_HANDS_ATTACH_PROBE":"hands_naked,greaserjacket,finger,glove","RB3_HANDS_INSTR_B":"1"})
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

# [INSTR_B] two-line record (new format: owner + rest-free AXIS line)
HDR = re.compile(r"\[INSTR_B\] mesh='([^']+)' owner='([^']*)' frame=(\d+) wext=([\d.]+) shellMax=([\d.]+)u shellMean=([\d.]+)u worstDom='([^']*)' n=(\d+)")
AXIS = re.compile(r"AXIS\(rest-free\) worstBone='([^']*)' orthoResid=([-\d.]+) isoDistort=([-\d.]+) grpN=(\d+) rExact=([-\d.]+)u nearest='([^']*)' idxOK=(\d)")

def parse_records(logpath, mesh_filter):
    recs=[]; lines=open(logpath, errors="replace").read().splitlines(); i=0
    while i < len(lines):
        m = HDR.search(lines[i])
        if m and mesh_filter in m.group(1):
            rec=dict(mesh=m.group(1),owner=m.group(2),frame=int(m.group(3)),wext=float(m.group(4)),
                     smax=float(m.group(5)),smean=float(m.group(6)),wdom=m.group(7),n=int(m.group(8)),
                     worstBone=None,ortho=None,iso=None,grpN=None,rExact=None,nearest=None,idxOK=None)
            if i+1 < len(lines):
                r = AXIS.search(lines[i+1])
                if r: rec.update(worstBone=r.group(1),ortho=float(r.group(2)),
                                 iso=float(r.group(3)),grpN=int(r.group(4)),
                                 rExact=float(r.group(5)),nearest=r.group(6),idxOK=int(r.group(7)))
            recs.append(rec)
        i += 1
    return recs

def corr(pts):
    if len(pts) < 4: return float('nan')
    xs=[p[0] for p in pts]; ys=[p[1] for p in pts]
    mx=sum(xs)/len(xs); my=sum(ys)/len(ys)
    num=sum((x-mx)*(y-my) for x,y in pts)
    den=math.sqrt(sum((x-mx)**2 for x in xs)*sum((y-my)**2 for y in ys))
    return num/den if den>1e-9 else float('nan')

def stat(vs):
    vs=[v for v in vs if v is not None]
    return (min(vs),sum(vs)/len(vs),max(vs)) if vs else (0,0,0)

def summarize(logp, tag, mesh):
    recs = parse_records(logp, mesh)
    if not recs:
        print(f"\n===== {tag}: {mesh} — NO records ====="); return None
    # per-owner co-variation (band members captured separately; do NOT lump owners)
    owners = sorted(set(r["owner"] for r in recs))
    print(f"\n===== {tag}: {mesh}  N={len(recs)}  owners={len(owners)} =====")
    covs=[]
    for ow in owners:
        rs=[r for r in recs if r["owner"]==ow]
        if len(rs)<6: continue
        wmin=min(r["wext"] for r in rs); wmax=max(r["wext"] for r in rs)
        cov=corr([(r["wext"],r["smax"]) for r in rs]); covs.append(cov)
        print(f"  owner='{ow}' N={len(rs)} wext[{wmin:.0f}..{wmax:.0f}] shellMax{stat([r['smax'] for r in rs])} "
              f"corr(wext,shellMax)={cov:+.2f}")
    ortho=stat([r["ortho"] for r in recs if r["ortho"] is not None and r["ortho"]>=0])
    iso=stat([r["iso"] for r in recs if r["iso"] is not None and r["iso"]>=0])
    print(f"  AXIS(rest-free) orthoResid min/mean/max = {ortho[0]:.4f}/{ortho[1]:.4f}/{ortho[2]:.4f}   (~0 => palette rigid rotation)")
    print(f"  AXIS(rest-free) isoDistort min/mean/max = {iso[0]:.4f}/{iso[1]:.4f}/{iso[2]:.4f}   (~0 => SPACE rigid; large => DECODE tear)")
    rex=stat([r["rExact"] for r in recs if r["rExact"] is not None and r["rExact"]>=0])
    idxs=[r["idxOK"] for r in recs if r["idxOK"] is not None]
    idxok_frac = (sum(idxs)/len(idxs)) if idxs else float('nan')
    print(f"  AXIS(rest-free) rExact(worst-vert to its dom-bone origin) min/mean/max = {rex[0]:.1f}/{rex[1]:.1f}/{rex[2]:.1f}u")
    print(f"  AXIS(rest-free) idxOK (dom bone IS spatially-nearest bone) fraction = {idxok_frac:.2f}  (1.0 => index correct => SPACE; <1 => index/decode)")
    if covs:
        print(f"  A7 per-owner corr(wext,shellMax): {['%+.2f'%c for c in covs]}")
    print("  --- top-8 highest-wext frames ---")
    print("   frame    wext  shellMax shellMean  ortho   iso  grpN rExact idxOK worstBone / nearest")
    for r in sorted(recs,key=lambda r:r["wext"],reverse=True)[:8]:
        print(f"  {r['frame']:6d}  {r['wext']:6.1f}  {r['smax']:7.1f}  {r['smean']:8.1f}  "
              f"{(r['ortho'] if r['ortho'] is not None else -9):6.3f} {(r['iso'] if r['iso'] is not None else -9):6.3f}  "
              f"{(r['grpN'] if r['grpN'] is not None else -1):3d} {(r['rExact'] if r['rExact'] is not None else -9):5.1f} "
              f"{(r['idxOK'] if r['idxOK'] is not None else -1):3d}  {r['worstBone']} / {r['nearest']}")
    return dict(mesh=mesh,N=len(recs),ortho=ortho,iso=iso,rExact=rex,idxok=idxok_frac,covs=covs)

def run(out, nav, screen, tag, songms=True):
    logp=os.path.join(out, tag+".log")
    proc,port,logf=launch(nav, logp)
    try:
        h=wait_screen(port,proc,screen,150)
        if h is None: log(f"{tag}: never reached {screen}"); return None
        log(f"{tag}: {screen} frame={h[0]} songMs={h[1]:.0f}")
        if songms:
            for ms in [2000,6000,12000,20000,30000,45000]:
                hh=wait_songms(port,proc,ms,60)
                if hh: shot(port,os.path.join(out,f"{tag}_ms{ms:05d}.png")); log(f"{tag} ms>={ms}: frame={hh[0]}")
        else:
            for tgt in [0,60,120,180,360]:
                hh=wait_frame(port,proc,h[0]+tgt,30)
                if hh: shot(port,os.path.join(out,f"{tag}_{tgt:03d}.png"))
        time.sleep(1.0)
    finally:
        teardown(proc,logf)
    return logp

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--out",default="/tmp/wave12-bs1")
    ap.add_argument("--mode",choices=["gameplay","partdiff","both"],default="both")
    a=ap.parse_args(); os.makedirs(a.out,exist_ok=True)
    results=[]
    if a.mode in ("gameplay","both"):
        lp=run(a.out,NAV_GAMEPLAY,"game_screen","gameplay",songms=True)
        if lp:
            for mesh in ("hands_naked","greaserjacket_resource"):
                r=summarize(lp,"GAMEPLAY",mesh);
                if r: results.append(("GAMEPLAY",r))
    if a.mode in ("partdiff","both"):
        lp=run(a.out,NAV_PARTDIFF,"part_difficulty_screen","partdiff",songms=False)
        if lp:
            for mesh in ("hands_naked","greaserjacket_resource"):
                r=summarize(lp,"PARTDIFF",mesh)
                if r: results.append(("PARTDIFF",r))
    log(f"done -> {a.out}")
