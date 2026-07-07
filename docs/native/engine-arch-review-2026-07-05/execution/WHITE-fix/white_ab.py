#!/usr/bin/env python3
"""white_ab.py — Wave-10 Lane B: RB3_VENUE_WHITE_GUARD OFF vs ON paired A/B.

Reuses white_discriminate.run_arm + ARMS. For each base arm we run guard-OFF (unset)
and guard-ON (RB3_VENUE_WHITE_GUARD=1), N boots each, and dump per-boot rows +
arm-mean hi_frac / mid_sat / mean_luma for the pre-registered gates.
"""
import argparse, importlib.util, json, os, statistics as st, collections

HERE = os.path.dirname(os.path.abspath(__file__))
def _load(m,p):
    s=importlib.util.spec_from_file_location(m,p); x=importlib.util.module_from_spec(s); s.loader.exec_module(x); return x
wd = _load("white_discriminate", os.path.join(HERE,"white_discriminate.py"))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--bin",required=True)
    ap.add_argument("--n",type=int,default=6)
    ap.add_argument("--songms",type=float,default=21000.0)
    ap.add_argument("--tol",type=float,default=2000.0)
    ap.add_argument("--bases",default="eng_hot,venue_light_off")
    ap.add_argument("--out",default=os.path.join(HERE,"measure"))
    ap.add_argument("--raws",default="/tmp/whitefix-bs1-caps")
    ap.add_argument("--tag",default="bs1")
    ap.add_argument("--song",type=int,default=4)
    args=ap.parse_args()
    os.makedirs(args.out,exist_ok=True); os.makedirs(args.raws,exist_ok=True)
    lo,hi=args.songms-args.tol,args.songms+args.tol; overshoot=hi+6000
    allrows=[]
    for base in args.bases.split(","):
        base=base.strip()
        baseenv=dict(wd.ARMS[base])
        for guard in ("OFF","ON"):
            env=dict(baseenv)
            if guard=="ON": env["RB3_VENUE_WHITE_GUARD"]="1"
            armtag=f"{base}_{guard}"
            print(f"== {armtag} env={env} ==")
            rows=wd.run_arm(args.bin,armtag,env,args.n,lo,hi,overshoot,args.raws,args.tag,song=args.song)
            for r in rows: r["base"]=base; r["guard"]=guard
            allrows+=rows
    outp=os.path.join(args.out,f"{args.tag}.json")
    json.dump(allrows,open(outp,"w"),indent=2); print("wrote",outp)
    # summary + gate deltas
    by=collections.defaultdict(list)
    for r in allrows: by[(r["base"],r["guard"])].append(r)
    print("\n=== ARM SUMMARY ===")
    agg={}
    for (base,guard),rs in sorted(by.items()):
        hi_=[r["hi_frac"] for r in rs]; ms=[r["mid_sat"] for r in rs]; ml=[r["mean_luma"] for r in rs]
        nwhite=sum(1 for r in rs if r["class"]=="WHITE")
        agg[(base,guard)]=dict(n=len(rs),hi=st.mean(hi_),mid=st.mean(ms),lum=st.mean(ml),white=nwhite,
                               classes=[r["class"] for r in rs])
        print(f"{base:16s} {guard:3s} N={len(rs)} hi_frac={st.mean(hi_):6.2f} mid_sat={st.mean(ms):.3f} "
              f"mean_luma={st.mean(ml):.3f} WHITE={nwhite} {[r['class'] for r in rs]}")
    print("\n=== PRE-REGISTERED GATE DELTAS ===")
    for base in args.bases.split(","):
        base=base.strip()
        if (base,"OFF") in agg and (base,"ON") in agg:
            o=agg[(base,"OFF")]; n=agg[(base,"ON")]
            dhi=n["hi"]-o["hi"]; dmid=n["mid"]-o["mid"]; dlum=n["lum"]-o["lum"]
            print(f"{base:16s} d_hi_frac={dhi:+.2f}  d_mid_sat={dmid:+.3f}  d_mean_luma={dlum:+.3f}  "
                  f"(OFF hi={o['hi']:.2f} mid={o['mid']:.3f} lum={o['lum']:.3f} W={o['white']} | "
                  f"ON hi={n['hi']:.2f} mid={n['mid']:.3f} lum={n['lum']:.3f} W={n['white']})")
if __name__=="__main__": main()
