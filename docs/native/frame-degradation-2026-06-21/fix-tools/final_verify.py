import os, sys, time, subprocess, signal, re, json
sys.path.insert(0, "/home/free/code/milohax/rb3/scripts/native")
import importlib.util
spec = importlib.util.spec_from_file_location("k2g", "/home/free/code/milohax/rb3/scripts/native/keyboard-to-gameplay.py")
k2g = importlib.util.module_from_spec(spec); spec.loader.exec_module(k2g)
pd = importlib.util.spec_from_file_location("pd", "/home/free/code/milohax/rb3/.claude/worktrees/memleak-audiofix/docs/native/frame-degradation-2026-06-21/profile_degradation.py")
pdm = importlib.util.module_from_spec(pd); pd.loader.exec_module(pdm)
REPO="/home/free/code/milohax/rb3/.claude/worktrees/memleak-audiofix"
BIN=sys.argv[1]; LABEL=sys.argv[2]; secs=int(sys.argv[3]); TRACE=f"/tmp/fv_{LABEL}.jsonl"; extra=sys.argv[4:]
def maps(pid):
    heap=anon=rss=0; cur=None
    try:
        with open(f"/proc/{pid}/smaps") as f:
            for line in f:
                if re.match(r'^[0-9a-f]+-[0-9a-f]+ ', line):
                    p=line.split(); cur=p[5] if len(p)>=6 else "[anon]"
                elif line.startswith("Rss:"):
                    kb=int(line.split()[1]); rss+=kb
                    if cur=="[heap]": heap+=kb
                    elif not cur or cur=="[anon]": anon+=kb
    except: pass
    return rss,heap,anon
port=k2g.free_port()
env=dict(os.environ)
env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),"MILO_HEADLESS":"1",
  "RB3_DATA":os.path.join(REPO,"orig-assets/extracted"),"RB3_DTA_OVERLAY":os.path.join(REPO,"native/dta"),
  "RB3_INPUT_DEBUG":"1","RB3_FRAME_TRACE":TRACE})
for kv in extra:
    k,v=kv.split("=",1); env[k]=v
if os.path.exists(TRACE): os.remove(TRACE)
logf=open(f"/tmp/fv_{LABEL}.log","w")
proc=subprocess.Popen([BIN],env=env,stdout=logf,stderr=subprocess.STDOUT,cwd=REPO,start_new_session=True)
if not pdm.drive_to_gameplay(port,proc,"hard",4,LABEL):
    print(f"{LABEL}: DRIVE FAILED"); os.killpg(os.getpgid(proc.pid),signal.SIGKILL); sys.exit(2)
t0=time.time(); S=[]
while time.time()-t0<secs:
    k2g.verb(port,"autohit")
    rss,heap,anon=maps(proc.pid)
    tail=pdm.read_trace_tail(TRACE, last_n=120)
    if tail:
        recent=tail[-60:]; dt=sum(f["dt"] for f in recent)/len(recent); mx=max(f["dt"] for f in tail)
    else: dt=-1; mx=-1
    S.append((round(time.time()-t0,1),rss,heap,anon,round(dt,1),round(mx,1)))
    time.sleep(5)
os.killpg(os.getpgid(proc.pid),signal.SIGTERM)
try: proc.wait(timeout=8)
except: os.killpg(os.getpgid(proc.pid),signal.SIGKILL)
json.dump(S,open(f"/tmp/fv_{LABEL}.json","w"))
print(f"=== {LABEL} ({secs}s) ===")
for r in S[::2]:
    fps=round(1000/r[4],0) if r[4]>0 else -1
    print(f"  t={r[0]:5.0f}s RSS={r[1]/1024:6.1f} heap={r[2]/1024:6.1f} anon={r[3]/1024:6.1f}MB  dt={r[4]:5.1f}ms fps={fps:5.0f} maxdt={r[5]:.1f}")
def fit(idx,frm=0):
    pts=[(r[0],r[idx]/1024) for r in S if r[idx]>0 and r[0]>=frm]; xs=[p[0] for p in pts]; ys=[p[1] for p in pts]
    n=len(xs); sx=sum(xs); sy=sum(ys); sxx=sum(x*x for x in xs); sxy=sum(x*y for x,y in zip(xs,ys))
    return (n*sxy-sx*sy)/(n*sxx-sx*sx)*1000, ys[0], ys[-1]
for nm,idx in [("RSS",1),("heap",2),("anon",3)]:
    sl,a,b=fit(idx); print(f"  {nm}: full slope={sl:.1f}KB/s")
