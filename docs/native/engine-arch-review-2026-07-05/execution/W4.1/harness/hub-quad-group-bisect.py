import http.client, json, os, signal, socket, subprocess, time
REPO="/home/free/code/milohax/rb3"; BIN=REPO+"/native/build-native/rb3-native"; DATA=REPO+"/orig-assets/extracted"
OUT="/tmp/wave6-hub-probe4"; os.makedirs(OUT,exist_ok=True)
NAV="@10:start,@30:confirm"
def fp():
    s=socket.socket();s.bind(("127.0.0.1",0));p=s.getsockname()[1];s.close();return p
def hget(port,path,t=20):
    c=http.client.HTTPConnection("127.0.0.1",port,timeout=t)
    try:c.request("GET",path);r=c.getresponse();return r.status,r.read()
    finally:c.close()
def hpost(port,path,body,t=15):
    c=http.client.HTTPConnection("127.0.0.1",port,timeout=t)
    try:
        c.request("POST",path,body=body,headers={"Content-Type":"text/plain"});r=c.getresponse();return r.status,r.read().decode("utf-8","replace")
    finally:c.close()
def health(port):
    try:
        st,b=hget(port,"/api/health")
        if st!=200:return None
        d=json.loads(b.decode())["data"];return int(d["frame"]),float(d["songMs"]),str(d["currentScreen"])
    except:return None
def shot(port,path):
    st,data=hget(port,"/api/screenshot")
    if st!=200 or data[:8]!=b"\x89PNG\r\n\x1a\n":return False
    open(path,"wb").write(data);return True
def ev(port,expr):
    return hpost(port,"/api/dta/eval",expr,t=10)[1][:150]
port=fp();logp=f"/tmp/rb3-hubprobe4-{port}.log";logf=open(logp,"w")
env=dict(os.environ);env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),"MILO_HEADLESS":"1","RB3_DATA":DATA,"RB3_GAME_INPUT":NAV,"RB3_FIXED_CLOCK":"1"})
proc=subprocess.Popen([BIN],env=env,stdout=logf,stderr=subprocess.STDOUT,cwd=REPO,start_new_session=True)
try:
    dl=time.time()+40
    while time.time()<dl and health(port) is None:
        if proc.poll() is not None:print("died");raise SystemExit(1)
        time.sleep(0.4)
    dl=time.time()+90
    while time.time()<dl:
        h=health(port)
        if h and h[2]=="main_hub_screen":break
        time.sleep(0.3)
    print("hub:",health(port));time.sleep(1.5)
    grps=["all.grp","main_bg.grp","header_art_bg.grp","menu_buttons.grp","main_menu.grp","bar_all.grp","bar_header_song_bg.grp"]
    for g in grps:
        r=ev(port,'{{main_hub_panel find "%s" TRUE} set_showing FALSE}'%g)
        time.sleep(0.45);shot(port,OUT+f"/hide_{g.replace('.','_')}.png")
        print("hide",g,"->",r)
        ev(port,'{{main_hub_panel find "%s" TRUE} set_showing TRUE}'%g);time.sleep(0.25)
finally:
    try:os.killpg(os.getpgid(proc.pid),signal.SIGTERM);proc.wait(timeout=8)
    except:
        try:os.killpg(os.getpgid(proc.pid),signal.SIGKILL)
        except:pass
    logf.close();print("log",logp)
