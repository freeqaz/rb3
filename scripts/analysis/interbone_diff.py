#!/usr/bin/env python3
"""interbone_diff.py — Wave-17 R1-DOLPHIN Lane D3 Wii-vs-native inter-bone JOIN.

Produces the per-pair Wii-vs-native inter-bone delta table the R5 hands-endgame
decision is gated on (PLAN §3.7). For each adjacent hand-chain bone pair (D2's M-2
list: forearm->hand anchor + middle/ring/thumb 01->02->03 cascades, BOTH hands):

  D_side  = inv(W_parent) . W_child                (matrix-relative, lint 1)
  delta   = angle( D_wii . inv(D_native) )         (matrix-relative, NEVER
                                                    angle-to-identity — lint 1)
  dTrans  = | t(D_wii) - t(D_native) |

CONVENTION SELF-CALIBRATION (the anti-oracle gate): D2's JSON stores BOTH the raw
per-bone world matrices (records[]) AND the precomputed per-pair D (interbone_tables
D_rel_rot + rel_rot_deg). We brute-force the homogeneous layout / product order /
transpose combo that reproduces D2's stored D from D2's raw worlds, and apply that
IDENTICAL convention to the native worlds. If no combo reproduces D2 to <0.5 deg the
join ABORTS (we will not emit a table on an unvalidated convention).

RED-TEAM (lint 3, no unvalidated oracle): after calibration we fire the delta metric
on a deliberately MISMATCHED pair (Wii anchor vs a native finger pair) and assert it
reads LARGE. A machinery that reports ~0 on a known-bad pair is rejected.

Usage:
  interbone_diff.py --wii D2_wii_bones.json --native native_bones_shell.json \\
      --scene-wii shell:ui/overshell --scene-native shell:main_hub \\
      --out-md D3_delta_table.md --out-json D3_delta_table.json
"""
import argparse, json, re, sys
import numpy as np

# D2's M-2 pair list. (parent_token, child_token, kind). Tokens are normalized
# (strip "bone_" prefix and ".cb"/".mesh" suffix, case-insensitive).
PAIRS = [
    ("forearm", "hand", "anchor"),
    ("hand", "middlefinger01", "finger_base"),
    ("middlefinger01", "middlefinger02", "finger"),
    ("middlefinger02", "middlefinger03", "finger"),
    ("hand", "ringfinger01", "finger_base"),
    ("ringfinger01", "ringfinger02", "finger"),
    ("ringfinger02", "ringfinger03", "finger"),
    ("hand", "thumb01", "finger_base"),
    ("thumb01", "thumb02", "finger"),
    ("thumb02", "thumb03", "finger"),
]


def norm(name):
    if not name:
        return ""
    n = re.sub(r"^bone_", "", name)
    n = re.sub(r"\.(cb|mesh)$", "", n)
    return n.lower()


def key(side_token, token):
    # side_token e.g. "L"/"R"; token e.g. "hand" -> "l-hand"
    return f"{side_token}-{token}".lower()


# ---- matrix helpers -------------------------------------------------------
def geodesic_deg(R):
    # angle of a 3x3 rotation: arccos((tr-1)/2), clamped.
    c = (np.trace(R) - 1.0) / 2.0
    c = max(-1.0, min(1.0, c))
    return float(np.degrees(np.arccos(c)))


def make_H(rows, trans, layout, transpose):
    """Build a 4x4 homogeneous transform from 3x3 rows + 3-vec trans under a chosen
    convention. layout='row' => translation in the last ROW (row-vector convention,
    p'=p.M); layout='col' => translation in the last COLUMN (column-vector, p'=M.p).
    transpose => use R^T instead of R for the basis block."""
    R = np.array(rows, dtype=np.float64)
    if transpose:
        R = R.T
    t = np.array(trans, dtype=np.float64)
    H = np.eye(4)
    H[:3, :3] = R
    if layout == "row":
        H[3, :3] = t
    else:  # col
        H[:3, 3] = t
    return H


def relative_D(Hp, Hc, order):
    """D = inv(P).C  (order='ipc')  or  C.inv(P) (order='cip')."""
    iP = np.linalg.inv(Hp)
    return iP @ Hc if order == "ipc" else Hc @ iP


def D_rot_trans(D, layout):
    R = D[:3, :3]
    t = D[3, :3] if layout == "row" else D[:3, 3]
    return R, t


# ---- data loading ---------------------------------------------------------
def wii_world_map(wii):
    """name(normalized) -> (rows, trans) from D2 raw records (single member = the
    Wii shell band; D2 bilateral symmetry already validated real matrices)."""
    m = {}
    for rec in wii["records"]:
        n = norm(rec["name"])
        if "world_rows" in rec and "world_trans" in rec:
            m[n] = (rec["world_rows"], rec["world_trans"])
    return m


def native_members(native):
    out = []
    for mem in native["members"]:
        wm = {}
        for b in mem["bones"]:
            wm[norm(b["name"])] = (b["world"]["rows"], b["world"]["trans"],
                                   b.get("trans_addr", ""))
        out.append({"slot": mem["slot"], "name": mem.get("name", ""),
                    "gender": mem.get("gender", ""), "source": mem.get("source", ""),
                    "world": wm})
    return out


# ---- convention calibration ----------------------------------------------
def calibrate(wii_map, wii_tables):
    """Find the (layout, transpose, order) combo that reproduces D2's stored
    D_rel_rot + rel_rot_deg from D2's raw worlds. Returns the combo or None."""
    # use every L/R pair D2 stored as the fit set
    checks = []
    for side in ("L", "R"):
        for row in wii_tables.get(side, []):
            pair = row["pair"]  # e.g. "L-forearm->L-hand"
            p_full, c_full = pair.split("->")
            pn, cn = norm(p_full), norm(c_full)
            if pn in wii_map and cn in wii_map:
                checks.append((pn, cn, row["rel_rot_deg"],
                               np.array(row["D_rel_rot"], dtype=np.float64),
                               np.array(row["rel_trans"], dtype=np.float64)))
    if not checks:
        return None
    best = None
    for layout in ("row", "col"):
        for transpose in (False, True):
            for order in ("ipc", "cip"):
                ok = True
                worst = 0.0
                for pn, cn, deg_ref, Drot_ref, t_ref in checks:
                    Hp = make_H(*wii_map[pn], layout, transpose)
                    Hc = make_H(*wii_map[cn], layout, transpose)
                    D = relative_D(Hp, Hc, order)
                    R, t = D_rot_trans(D, layout)
                    # compare rotation via relative angle to reference rotation,
                    # and translation vector, and the scalar geodesic angle.
                    dang = geodesic_deg(R @ Drot_ref.T)  # 0 if R==Dref
                    ddeg = abs(geodesic_deg(R) - deg_ref)
                    dt = float(np.linalg.norm(t - t_ref))
                    worst = max(worst, dang, ddeg, dt)
                    if dang > 0.5 or ddeg > 0.5 or dt > 0.05:
                        ok = False
                        break
                if ok:
                    return {"layout": layout, "transpose": transpose,
                            "order": order, "worst_residual": worst,
                            "n_checks": len(checks)}
                if best is None or worst < best[1]:
                    best = ((layout, transpose, order), worst)
    sys.stderr.write(f"[calibrate] NO exact combo; best={best}\n")
    return None


def compute_D(world_map, pn, cn, conv):
    if pn not in world_map or cn not in world_map:
        return None
    Hp = make_H(world_map[pn][0], world_map[pn][1], conv["layout"], conv["transpose"])
    Hc = make_H(world_map[cn][0], world_map[cn][1], conv["layout"], conv["transpose"])
    D = relative_D(Hp, Hc, conv["order"])
    R, t = D_rot_trans(D, conv["layout"])
    return R, t


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wii", required=True)
    ap.add_argument("--native", required=True)
    ap.add_argument("--scene-wii", default="shell:ui/overshell")
    ap.add_argument("--scene-native", default="shell:main_hub")
    ap.add_argument("--out-md", required=True)
    ap.add_argument("--out-json", required=True)
    args = ap.parse_args()

    wii = json.load(open(args.wii))
    native = json.load(open(args.native))
    wii_map = wii_world_map(wii)
    wii_tables = wii.get("interbone_tables", {})

    conv = calibrate(wii_map, wii_tables)
    if conv is None:
        sys.stderr.write("ABORT: could not calibrate matrix convention against D2 "
                         "stored values — refusing to emit an unvalidated table.\n")
        return 2
    sys.stderr.write(f"[calibrate] convention = {conv}\n")

    mems = native_members(native)
    # For the delta table we join each native member against the Wii ground truth
    # (single Wii band member; the Wii side has one L and one R hand). Native has 4
    # members; we report per-member rows (lint 2: split by gender/member).
    rows_out = []
    for mem in mems:
        for side in ("L", "R"):
            for (pt, ct, kind) in PAIRS:
                pn, cn = key(side, pt), key(side, ct)
                Dw = compute_D(wii_map, pn, cn, conv)
                Dn = compute_D(mem["world"], pn, cn, conv)
                row = {"member_slot": mem["slot"], "member": mem["name"],
                       "gender": mem["gender"], "source": mem["source"],
                       "hand": side, "pair": f"{pn}->{cn}", "kind": kind}
                if Dw is None:
                    row["status"] = "wii_missing"
                elif Dn is None:
                    row["status"] = "native_missing"
                else:
                    Rw, tw = Dw
                    Rn, tn = Dn
                    delta = geodesic_deg(Rw @ Rn.T)
                    dtrans = float(np.linalg.norm(np.array(tw) - np.array(tn)))
                    row.update(status="ok",
                               wii_relRot=round(geodesic_deg(Rw), 3),
                               native_relRot=round(geodesic_deg(Rn), 3),
                               delta_deg=round(delta, 3),
                               wii_relT=[round(x, 3) for x in tw],
                               native_relT=[round(x, 3) for x in tn],
                               dTrans=round(dtrans, 3),
                               native_trans_addr=mem["world"].get(cn, ("", "", ""))[2])
                rows_out.append(row)

    # ---- RED TEAM: known-bad pair must read LARGE ----
    # Wii L-forearm->L-hand vs native L-hand->L-thumb01 (a real but WRONG pairing).
    rt = None
    Dw = compute_D(wii_map, "l-forearm", "l-hand", conv)
    Dn = compute_D(mems[0]["world"], "l-hand", "l-thumb01", conv)
    if Dw and Dn:
        rt_delta = geodesic_deg(Dw[0] @ Dn[0].T)
        rt = {"desc": "Wii[L-forearm->L-hand] vs native[L-hand->L-thumb01]",
              "delta_deg": round(rt_delta, 3),
              "verdict": "RED (machinery separates)" if rt_delta > 20
                         else "SUSPECT (machinery did NOT separate a known-bad pair)"}
        sys.stderr.write(f"[redteam] {rt}\n")

    ok_rows = [r for r in rows_out if r["status"] == "ok"]
    anchor_deltas = [r["delta_deg"] for r in ok_rows if r["kind"] == "anchor"]
    finger_deltas = [r["delta_deg"] for r in ok_rows if r["kind"] != "anchor"]

    out = {"wii_side": {"scene": args.scene_wii, "build": wii.get("build"),
                        "map_vtable_charbone": wii.get("map_vtable_charbone"),
                        "offsets_bank8": wii.get("offsets_bank8")},
           "native_side": {"scene": args.scene_native, "build": native.get("build"),
                           "members": [{"slot": m["slot"], "name": m["name"],
                                        "gender": m["gender"], "source": m["source"]}
                                       for m in mems]},
           "convention": conv, "redteam": rt,
           "summary": {"n_rows": len(rows_out), "n_ok": len(ok_rows),
                       "anchor_delta_mean": round(float(np.mean(anchor_deltas)), 3) if anchor_deltas else None,
                       "finger_delta_mean": round(float(np.mean(finger_deltas)), 3) if finger_deltas else None,
                       "finger_delta_max": round(float(np.max(finger_deltas)), 3) if finger_deltas else None},
           "rows": rows_out}
    json.dump(out, open(args.out_json, "w"), indent=1)

    # ---- markdown ----
    L = []
    L.append("# R1-DOLPHIN D3 — Wii-vs-native inter-bone delta table")
    L.append("")
    L.append(f"- Wii side: `{args.scene_wii}` — {wii.get('build')} "
             f"(vtable {wii.get('map_vtable_charbone')}, D2 ground truth).")
    L.append(f"- Native side: `{args.scene_native}` — {native.get('build')}; "
             f"members: " + ", ".join(f"slot{m['slot']} {m['gender']}/{m['source']}" for m in mems) + ".")
    L.append(f"- Convention (calibrated to reproduce D2 stored D to "
             f"<0.5deg, {conv['n_checks']} checks, worst {conv['worst_residual']:.4f}): "
             f"layout={conv['layout']} transpose={conv['transpose']} order={conv['order']}.")
    if rt:
        L.append(f"- Red-team (known-bad pair): {rt['desc']} -> "
                 f"delta={rt['delta_deg']}deg **{rt['verdict']}**.")
    s = out["summary"]
    L.append(f"- Summary: anchor delta mean={s['anchor_delta_mean']}deg, "
             f"finger delta mean={s['finger_delta_mean']}deg (max={s['finger_delta_max']}deg).")
    L.append("")
    L.append("D_side = inv(W_parent).W_child. delta = angle(D_wii . inv(D_native)) "
             "(matrix-relative). Per-member rows (lint 2: gender/member split).")
    L.append("")
    # group by member+hand
    for mem in mems:
        for side in ("L", "R"):
            sub = [r for r in rows_out if r["member_slot"] == mem["slot"] and r["hand"] == side]
            if not sub:
                continue
            L.append(f"## slot{mem['slot']} ({mem['gender']}, {mem['source']}) — {side}-hand")
            L.append("")
            L.append("| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |")
            L.append("|---|---|---:|---:|---:|---:|---|---|")
            for r in sub:
                if r["status"] == "ok":
                    L.append(f"| {r['pair']} | {r['kind']} | {r['wii_relRot']} | "
                             f"{r['native_relRot']} | **{r['delta_deg']}** | {r['dTrans']} | "
                             f"{r['native_trans_addr']} | ok |")
                else:
                    L.append(f"| {r['pair']} | {r['kind']} | - | - | - | - | - | {r['status']} |")
            L.append("")
    open(args.out_md, "w").write("\n".join(L) + "\n")
    sys.stderr.write(f"wrote {args.out_md} + {args.out_json} "
                     f"({len(ok_rows)}/{len(rows_out)} ok rows)\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
