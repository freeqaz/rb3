#!/usr/bin/env python3
"""
_exit-trap-test.py — reproduce (on native) the web-release "continue past the
score screen traps unreachable" crash. Boots to gameplay, jumps to song end,
reaches coop_endgame, then injects Confirm presses to advance PAST the score
screen toward meta_loading_continue / song_select, watching for a crash.
"""
import argparse, http.client, json, os, signal, socket, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(REPO, "native", "build-native", "rb3-native")
DATA = os.path.join(REPO, "orig-assets", "extracted")
NAV = ("@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,"
       "@320:down,@350:msg:music_library:select_highlighted_node,"
       "@380:part:guitar,@400:diff:expert,@470:confirm,@540:confirm,@620:confirm")
JUMP_MS = 600000

def log(m): print(f"[exit-trap] {m}", flush=True)
def free_port():
    s=socket.socket(); s.bind(("127.0.0.1",0)); p=s.getsockname()[1]; s.close(); return p
def get(port,path,t=10):
    c=http.client.HTTPConnection("127.0.0.1",port,timeout=t)
    try: c.request("GET",path); r=c.getresponse(); return r.status,r.read()
    finally: c.close()
def post(port,path,body,t=15):
    c=http.client.HTTPConnection("127.0.0.1",port,timeout=t)
    try:
        c.request("POST",path,body=body,headers={"Content-Type":"text/plain"})
        r=c.getresponse(); return r.status,r.read().decode("utf-8","replace")
    finally: c.close()
def health(port):
    try:
        st,b=get(port,"/api/health")
        if st!=200: return None
        d=json.loads(b.decode())["data"]; return int(d["frame"]),float(d["songMs"]),str(d["currentScreen"])
    except Exception: return None
def wait_for(port,pred,timeout,label,proc):
    dl=time.time()+timeout; last=None
    while time.time()<dl:
        if proc.poll() is not None:
            log(f"*** CRASH: process exited (code {proc.returncode}) waiting for {label} ***"); return "CRASH"
        h=health(port)
        if h:
            f,m,s=h
            if s!=last: log(f"  ...{label}: frame={f} songMs={m:.0f} screen='{s}'"); last=s
            if pred(f,m,s): return h
        time.sleep(0.4)
    return None

def main():
    port=free_port(); log_path=f"/tmp/rb3-exit-{port}.log"; logf=open(log_path,"w")
    env=dict(os.environ); env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),
              "MILO_HEADLESS":"1","RB3_DATA":DATA,"RB3_GAME_INPUT":NAV})
    log(f"launching (port {port}), log -> {log_path}")
    proc=subprocess.Popen([BIN],env=env,stdout=logf,stderr=subprocess.STDOUT,cwd=REPO,start_new_session=True)
    rc=1
    try:
        if wait_for(port,lambda f,m,s:True,40,"server",proc) in (None,"CRASH"): return 1
        # wait for gameplay (song clock advancing)
        if wait_for(port,lambda f,m,s:s=="game_screen" and m>2000,180,"gameplay",proc) in (None,"CRASH"):
            log("FAIL: never reached gameplay"); return 1
        time.sleep(1.0)
        log(f"injecting jump {JUMP_MS}")
        post(port,"/api/input",f"msg:game:jump:{JUMP_MS}")
        h=wait_for(port,lambda f,m,s:"endgame" in s,90,"endgame",proc)
        if h in (None,"CRASH"):
            log("FAIL: never reached endgame (or crashed)"); return 1 if h is None else 2
        log(f"reached endgame: screen='{h[2]}'; now Confirm-ing PAST the score screen")
        time.sleep(3.0)
        # Inject Confirm presses to advance through/past the endgame screens.
        for i in range(20):
            if proc.poll() is not None:
                log(f"*** CRASH: process exited (code {proc.returncode}) during exit confirm #{i} ***"); return 2
            post(port,"/api/input","confirm")
            time.sleep(1.2)
            h2=health(port)
            if h2 is None:
                if proc.poll() is not None:
                    log(f"*** CRASH after confirm #{i} (code {proc.returncode}) ***"); return 2
            else:
                s=h2[2]
                if i==0 or s!=getattr(main,"_last",None):
                    log(f"  confirm #{i}: screen='{s}'"); main._last=s
                if any(k in s for k in ("song_select","meta_loading_continue","music_library","main_hub")):
                    log(f"EXIT OK: reached '{s}' — survived the past-score-screen exit"); rc=0; break
        if rc!=0 and proc.poll() is None:
            log(f"DONE (no crash, ended on screen='{health(port)[2] if health(port) else '?'}')"); rc=0
        elif proc.poll() is not None:
            log(f"*** CRASH at end (code {proc.returncode}) ***"); rc=2
        return rc
    finally:
        try:
            if proc.poll() is None:
                os.killpg(os.getpgid(proc.pid),signal.SIGTERM)
                try: proc.wait(timeout=8)
                except subprocess.TimeoutExpired: os.killpg(os.getpgid(proc.pid),signal.SIGKILL)
        except Exception: pass
        logf.close(); log(f"engine log: {log_path}")

if __name__=="__main__": sys.exit(main())
