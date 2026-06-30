import sys, time, os, importlib.util, subprocess, signal
sys.path.insert(0, "scripts/native")
spec = importlib.util.spec_from_file_location("k2g", "scripts/native/keyboard-to-gameplay.py")
k2g = importlib.util.module_from_spec(spec); spec.loader.exec_module(k2g)

port = 8937
env = dict(os.environ)
env.update({"RB3_GAME":"1","RB3_HTTP":"1","RB3_HTTP_PORT":str(port),"MILO_HEADLESS":"1",
            "RB3_DATA":"/home/free/code/milohax/rb3/orig-assets/extracted",
            "RB3_DTA_OVERLAY":"native/dta","RB3_INPUT_DEBUG":"1","RB3_CROWD_DBG":"1"})
logf=open("/tmp/crowd-probe-engine.log","w")
proc=subprocess.Popen(["native/build-native/rb3-native"],env=env,stdout=logf,stderr=subprocess.STDOUT,
                      cwd=os.getcwd(),start_new_session=True)
print("launched pid",proc.pid)
try:
    k2g.wait_screen(port, lambda s: s and s!="", 60, proc)
    # splash
    for _ in range(8):
        if k2g.health(port)[2]=="main_hub_screen": break
        k2g.press(port,k2g.START); k2g.drain_pad(port); time.sleep(0.6)
    k2g.wait_screen(port,"main_hub_screen",30,proc)
    for _ in range(10):
        if k2g.health(port)[2]=="song_select_screen": break
        k2g.press(port,k2g.CONFIRM); k2g.drain_pad(port); time.sleep(0.7)
    k2g.wait_screen(port,"song_select_screen",40,proc); time.sleep(1.5)
    for _ in range(4): k2g.press(port,k2g.DDOWN); k2g.drain_pad(port); time.sleep(0.2)
    k2g.press(port,k2g.CONFIRM); k2g.drain_pad(port)
    k2g.wait_screen(port,"part_difficulty_screen",60,proc)
    ov=k2g.wait_view(port, lambda v: v.startswith("choose_part") or v=="choose_diff",30,proc,"part")
    if ov and ov[0].startswith("choose_part"):
        k2g.press(port,k2g.CONFIRM); k2g.drain_pad(port)
    ov=k2g.overshell(port); g=0
    while ov[0]=="confirm_action" and g<4:
        k2g.press(port,k2g.CONFIRM); k2g.drain_pad(port); time.sleep(0.5); ov=k2g.overshell(port); g+=1
    ov=k2g.wait_view(port, lambda v: v=="choose_diff",30,proc,"diff")
    if ov:
        for _ in range(2): k2g.press(port,k2g.DDOWN); k2g.drain_pad(port); time.sleep(0.25)
        k2g.press(port,k2g.CONFIRM); k2g.drain_pad(port)
    ov=k2g.overshell(port); g=0
    while ov[0]=="confirm_action" and g<4:
        k2g.press(port,k2g.CONFIRM); k2g.drain_pad(port); time.sleep(0.5); ov=k2g.overshell(port); g+=1
    k2g.wait_view(port, lambda v: v=="ready_to_play",30,proc,"ready")
    h=k2g.wait_screen(port,"game_screen",90,proc)
    print("reached game_screen:", h)
    k2g.verb(port,"nofail")
    # let the intro play a few seconds
    for _ in range(20):
        k2g.verb(port,"autohit"); time.sleep(0.5)
    print("FINAL health:", k2g.health(port))
    # Query live: is TheCrowdAudio non-null? via dta eval of crowd_audio var
    print("crowd_audio var:", k2g.dta(port, "{crowd_audio}"))
    print("crowd_audio type:", k2g.dta(port, "{type_of {crowd_audio}}"))
finally:
    time.sleep(1)
    try: os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=8)
    except Exception: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    print("game terminated")
