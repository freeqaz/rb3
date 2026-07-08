import http.client, json, os, signal, socket, subprocess, sys, time
sys.path.insert(0, "scripts/native"); import uidump_query as U
REPO=os.getcwd(); BIN="native/build-native/rb3-native"; DATA="orig-assets/extracted"
def fp():
    s=socket.socket(); s.bind(("127.0.0.1",0)); p=s.getsockname()[1]; s.close(); return p
def get(port,path,t=25):
    c=http.client.HTTPConnection("127.0.0.1",port,timeout=t)
    try: c.request("GET",path); r=c.getresponse(); return r.status,r.read()
    finally: c.close()
def health(port):
    try:
        st,b=get(port,"/api/health")
        if st!=200: return None
        d=json.loads(b.decode())["data"]; return int(d["frame"]),str(d["currentScreen"])
    except: return None
port=fp(); log=open(f"/tmp/hubroi-{port}.log","w")
env=dict(os.environ); env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),
  "MILO_HEADLESS":"1","RB3_DATA":DATA,"RB3_FIXED_CLOCK":"1","RB3_DRAWLOG_PROV":"1",
  "RB3_GAME_INPUT":"@10:start,@30:confirm"})
proc=subprocess.Popen([BIN],env=env,stdout=log,stderr=subprocess.STDOUT,cwd=REPO,start_new_session=True)
try:
    dl=time.time()+120; ok=False
    while time.time()<dl:
        if proc.poll() is not None: print("exit",proc.returncode); break
        h=health(port)
        if h and h[1]=="main_hub_screen": ok=True; break
        time.sleep(0.5)
    if not ok: print("FAIL no hub"); sys.exit(1)
    time.sleep(4.0)
    authored=U.authored_index(U.fetch_uidump(port))
    # center-street pedestrian region (retail walkers ~x560-800,y470-620)
    for lbl,roi in [("center_walkers",(560,460,260,180)),("tiger_neon",(880,60,400,400))]:
        print(f"\n=== ROI[{lbl}]={roi} ===")
        doc=U.fetch_roi(port,list(map(float,roi)))
        # only skinned/char + neon-ish
        for d in doc.get("draws",[]):
            p=d.get("prov",{})
            print(f"  #{d.get('i')} skinned={d.get('skinned')} mesh={p.get('mesh')!r} mat={p.get('mat')!r} owner={p.get('owner')!r} rect={p.get('rect')}")
finally:
    try: os.killpg(os.getpgid(proc.pid),signal.SIGTERM); proc.wait(timeout=8)
    except: 
        try: os.killpg(os.getpgid(proc.pid),signal.SIGKILL)
        except: pass
    log.close()
