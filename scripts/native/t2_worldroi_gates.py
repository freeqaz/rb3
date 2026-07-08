#!/usr/bin/env python3
"""
t2_worldroi_gates.py — T2-WORLDROI (Wave 19) gates G1/G2/G3 runner.

Boots the SAME binary TWICE (RB3_PROV_SKIN_SPHERE is env-cached, so two boots A/B it
per review B3), dumps the full band-gameplay drawlog each arm, and evaluates:

  G1 (RED baseline / known-answer localization contrast, review B3): on a band ROI,
     the RED arm (sphere-forced) FAILS to localize skinned draws (rectKind==1, no bone
     names), while the GREEN arm returns a bounded set naming mesh + bone(s).
     Cardinalities are reported as evidence, not the pass criterion.
  G2 (disjoint-ROI negative control): a background-corner ROI returns a SKINNED-owner
     set DISJOINT from the band ROI's (no band-member owner leaks into the corner).
  G3 (known-answer positive control): a hand ROI names a hand mesh + finger/wrist bones
     (the engine's own hand-bone taxonomy is the oracle). B5: query a coherent frame or
     accept wrist-level naming — a mitten-triggered frame is not instrument failure.

SPATIAL provenance axis only (which mesh/bone/owner drew a pixel), never the T1
frame-assignment timing axis nor R4's ledger `order` axis.
"""
import argparse, json, os, signal, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import t2_worldroi_probe as t2

BAND_ROI = [540.0, 310.0, 60.0, 160.0]   # player0 guitar-neck region (upper-center band)
CORNER_ROI = [0.0, 0.0, 80.0, 80.0]      # top-left backdrop (no band structure)
HAND_ROI = [560.0, 300.0, 220.0, 80.0]   # visible fretting/instrument hands band


def rect_isect(r, roi):
    return (r and len(r) == 4 and r[2] >= 0 and
            r[0] < roi[0] + roi[2] and r[0] + r[2] > roi[0] and
            r[1] < roi[1] + roi[3] and r[1] + r[3] > roi[1])


def boot_dump(bin_, data, extra_env, tag, outdir):
    port = t2.free_port()
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": data,
                "RB3_DTA_OVERLAY": os.path.join(t2.REPO, "native", "dta"),
                "RB3_DRAWLOG_PROV": "1", "RB3_FIXED_CLOCK": "1"})
    env.update(extra_env)
    logf = open(os.path.join(outdir, f"{tag}_boot.log"), "w")
    proc = subprocess.Popen([bin_], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=t2.REPO, start_new_session=True)
    try:
        if not t2.nav_to_gameplay(port, proc):
            return None
        for _ in range(8):
            t2.verb(port, "autohit"); time.sleep(0.25)
        t2.screenshot(port, os.path.join(outdir, f"{tag}_frame.png"))
        dl = t2.fetch_drawlog(port, roi=None)
        with open(os.path.join(outdir, f"{tag}_drawlog.json"), "w") as f:
            json.dump(dl, f)
        return dl
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM); proc.wait(timeout=8)
        except Exception:
            try: os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except Exception: pass
        logf.close()


def skinned_in_roi(dl, roi):
    out = []
    for d in dl.get("draws", []):
        if not d.get("skinned"): continue
        p = d.get("prov", {})
        if not rect_isect(p.get("rect"), roi): continue
        bones = [br["bone"] for br in p.get("boneRects", []) if rect_isect(br["rect"], roi)]
        out.append({"mesh": p.get("mesh"), "owner": p.get("owner"),
                    "rectKind": p.get("rectKind"), "bones": bones,
                    "boneFallback": p.get("boneFallback"), "rect": p.get("rect")})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=t2.DEFAULT_BIN)
    ap.add_argument("--data", default=t2.DEFAULT_DATA)
    ap.add_argument("--out", default="/tmp/t2-gates")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    print("[gates] GREEN arm (default, RB3_PROV_SKIN_SPHERE unset) ...", flush=True)
    green = boot_dump(args.bin, args.data, {}, "green", args.out)
    print("[gates] RED arm (RB3_PROV_SKIN_SPHERE=1, legacy sphere) ...", flush=True)
    red = boot_dump(args.bin, args.data, {"RB3_PROV_SKIN_SPHERE": "1"}, "red", args.out)
    if green is None or red is None:
        print("[gates] BOOT FAIL"); return 1

    res = {}

    # ---- G1: RED fails to localize; GREEN names mesh+bone on the band ROI ----
    g_band = skinned_in_roi(green, BAND_ROI)
    r_band = skinned_in_roi(red, BAND_ROI)
    g_named = [h for h in g_band if h["rectKind"] == 3 and h["bones"]]
    r_named = [h for h in r_band if h["rectKind"] == 3 and h["bones"]]
    g1_pass = len(g_named) >= 1 and len(r_named) == 0
    res["G1"] = {"pass": g1_pass,
                 "green_skinned_in_roi": len(g_band), "red_skinned_in_roi": len(r_band),
                 "green_named_mesh+bone": len(g_named), "red_named_mesh+bone": len(r_named),
                 "green_example": g_named[0] if g_named else None,
                 "red_rectKinds": sorted({h["rectKind"] for h in r_band})}

    # ---- G2: disjoint-ROI negative control (skinned owner sets disjoint) ----
    g_band_owners = {h["owner"] for h in g_band if h["owner"]}
    g_corner = skinned_in_roi(green, CORNER_ROI)
    g_corner_owners = {h["owner"] for h in g_corner if h["owner"]}
    leaked = g_band_owners & g_corner_owners
    g2_pass = len(leaked) == 0
    res["G2"] = {"pass": g2_pass, "band_owners": sorted(g_band_owners),
                 "corner_skinned_owners": sorted(g_corner_owners),
                 "leaked_band_owners_into_corner": sorted(leaked)}

    # ---- G3: hand ROI names hand mesh + finger/wrist bones ----
    g_hand = skinned_in_roi(green, HAND_ROI)
    def is_handish(m): return m and ("hand" in m or "glove" in m or "finger" in m)
    def is_fingerwrist(b): return b and any(t in b for t in
        ("finger", "thumb", "index", "middle", "ring", "pinky", "hand", "wrist", "fore"))
    hand_hits = [h for h in g_hand if h["rectKind"] == 3 and
                 (is_handish(h["mesh"]) or any(is_fingerwrist(b) for b in h["bones"]))]
    g3_pass = len(hand_hits) >= 1 and any(any(is_fingerwrist(b) for b in h["bones"]) for h in hand_hits)
    res["G3"] = {"pass": g3_pass, "hand_hits": hand_hits[:5],
                 "note": "B5: hand meshes/finger bones on a coherent frame; wrist-level accepted"}

    with open(os.path.join(args.out, "gates_result.json"), "w") as f:
        json.dump(res, f, indent=1)
    for g in ("G1", "G2", "G3"):
        print(f"[gates] {g}: {'GREEN' if res[g]['pass'] else 'RED'}  {json.dumps(res[g])[:220]}")
    ok = all(res[g]["pass"] for g in ("G1", "G2", "G3"))
    print(f"[gates] OVERALL: {'ALL GREEN' if ok else 'SOME RED'}")
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
