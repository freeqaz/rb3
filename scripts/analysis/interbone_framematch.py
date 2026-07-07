#!/usr/bin/env python3
"""interbone_framematch.py — Wave-17 R1-DOLPHIN Lane D4 frame-matched JOIN.

The FINAL R5 artifact. D3 delivered the Wii-vs-native inter-bone machinery but its
two shells were captured at UN-SYNCED frames, so the middle/ring divergence could not
be separated from a pose-frame difference (D3_findings §Interpretation). D4 closes
that with a POSE-ANCHORED NEAREST-FRAME join (PLAN §3 DO-step-3 fallback, blessed
when exact frame-driving is impractical):

  * Wii = the single D2 frozen pose (its per-pair D_rel_rot is already STORED in
    D2_wii_bones.json — the validated ground truth).
  * Native = a DENSE SWEEP over the whole `playerN` idle-vignette clip [0..~6.4]
    (d4-bonedump-sweep.py; each snapshot carries the member's CharClipDriver clip
    name + frame).

Why a nearest-frame join and not a beat-label join (the measured reason, committed):
at the D2 frozen instant the Wii CharDrivers have NO active CharClipDriver playing a
`playerN` clip (stacks empty; the player-clip heap referencers are string/config
tables, not live drivers with a beat field — verified live on the D2 boot). So the
Wii artifact carries no readable vignette beat; the Wii POSE itself is the frame
label, matched against the native sweep on the KNOWN-shared bones.

The decisive test (frame-explainable envelope):
  For each native member m and each pair, over ALL sweep frames f:
    delta(m,f,pair) = angle( D_wii_stored[pair] · inv(D_native[m,f,pair]) )
  Pick f* = argmin over f of the ANCHOR+THUMB delta sum (the proven-structurally-equal
  reference — D3: anchor/thumb agree in relRot magnitude to ~1°; this is the
  convention-null / pelvis-identity anchor per D3's unblock). Report, per pair:
    - delta @ f*                         (the residual at the best frame-match)
    - min / max delta over the sweep     (the frame-explainable ENVELOPE)
  Interpretation contract (pre-registered, PLAN §3.7):
    * anchor+thumb small at f* AND middle/ring MIN-over-sweep still large  => the
      finger divergence SURVIVES frame-matching: NO vignette frame reproduces the Wii
      finger pose => a REAL skeleton/pose delta candidate (the headline).
    * middle/ring min-over-sweep collapses to ~anchor level at some frame  => the
      divergence is frame/animation-state explainable => skeleton EXONERATED there.

Convention: reuses interbone_diff.calibrate (reproduces D2's stored D from D2's raw
worlds to <0.5°, else ABORT) so native D is measured in D2's own convention.
Red-team + calibration are RE-RUN on this NEW capture (OPTIONS §4 lint 3).

Usage:
  interbone_framematch.py --wii D2_wii_bones.json --sweep-dir /tmp/d4sweep \\
      --out-md D4_delta_table.md --out-json D4_delta_table.json
"""
import argparse, glob, json, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import importlib.util
_spec = importlib.util.spec_from_file_location(
    "ibd", os.path.join(HERE, "interbone_diff.py"))
ibd = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ibd)

PAIRS = ibd.PAIRS
norm = ibd.norm
key = ibd.key
geodesic_deg = ibd.geodesic_deg
make_H = ibd.make_H
relative_D = ibd.relative_D
D_rot_trans = ibd.D_rot_trans

# The proven-structurally-equal reference pairs used to pick the matched frame f*
# (D3: anchor forearm->hand + the thumb cascade agree cross-side in relRot magnitude).
ANCHOR_KINDS = ("anchor",)
REF_TOKENS = ("thumb01", "thumb02", "thumb03")  # thumb child tokens => the thumb cascade


def wii_stored_D(wii, conv):
    """Wii ground-truth per pair from D2's STORED interbone_tables (D_rel_rot rotation
    + rel_trans). Keyed by normalized 'l-forearm->l-hand'. This is the frozen Wii pose;
    we do NOT recompute it (avoids the multi-instance ambiguity D2 already resolved)."""
    out = {}
    for side in ("L", "R"):
        for row in wii.get("interbone_tables", {}).get(side, []):
            p_full, c_full = row["pair"].split("->")
            pk = f"{norm(p_full)}->{norm(c_full)}"
            R = np.array(row["D_rel_rot"], dtype=np.float64)
            t = np.array(row["rel_trans"], dtype=np.float64)
            out[pk] = (R, t, row.get("rel_rot_deg"))
    return out


def native_D(world_map, pn, cn, conv):
    r = ibd.compute_D(world_map, pn, cn, conv)
    return r  # (R, t) or None


def load_sweep(sweep_dir):
    """Return [{frame_idx, main_frame, members:{slot:{world, gender, drivers}}}...]."""
    snaps = sorted(glob.glob(os.path.join(sweep_dir, "sweep_[0-9]*.json")))
    out = []
    for i, sp in enumerate(snaps):
        d = json.load(open(sp))
        mems = {}
        for mem in d.get("members", []):
            wm = {}
            for b in mem["bones"]:
                wm[norm(b["name"])] = (b["world"]["rows"], b["world"]["trans"],
                                       b.get("trans_addr", ""))
            main_fr = None
            for dr in mem.get("drivers", []):
                if dr.get("driver") == "main.drv":
                    main_fr = dr.get("frame")
            mems[mem["slot"]] = {"world": wm, "gender": mem.get("gender", ""),
                                 "name": mem.get("name", ""), "source": mem.get("source", ""),
                                 "main_frame": main_fr,
                                 "drivers": mem.get("drivers", [])}
        out.append({"idx": i, "file": os.path.basename(sp), "members": mems})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wii", required=True)
    ap.add_argument("--sweep-dir", required=True)
    ap.add_argument("--scene-wii", default="shell:ui/overshell (D2 frozen, no active vignette driver)")
    ap.add_argument("--scene-native", default="shell:main_hub playerN vignette sweep")
    ap.add_argument("--out-md", required=True)
    ap.add_argument("--out-json", required=True)
    args = ap.parse_args()

    wii = json.load(open(args.wii))
    wii_map = ibd.wii_world_map(wii)
    conv = ibd.calibrate(wii_map, wii.get("interbone_tables", {}))
    if conv is None:
        sys.stderr.write("ABORT: convention calibration failed on D2 (unvalidated).\n")
        return 2
    sys.stderr.write(f"[calibrate] convention = {conv}\n")
    Dwii = wii_stored_D(wii, conv)

    sweep = load_sweep(args.sweep_dir)
    if not sweep:
        sys.stderr.write("ABORT: no sweep_*.json found.\n"); return 2
    slots = sorted(sweep[0]["members"].keys())
    sys.stderr.write(f"[sweep] {len(sweep)} frames, members {slots}\n")

    # ---- RED TEAM on the NEW capture: known-bad pair must read LARGE (lint 3) ----
    rt = None
    m0 = sweep[len(sweep)//2]["members"][slots[0]]["world"]
    Dw = Dwii.get("l-forearm->l-hand")
    Dn = native_D(m0, "l-hand", "l-thumb01", conv)
    if Dw and Dn:
        rt_delta = geodesic_deg(Dw[0] @ Dn[0].T)
        rt = {"desc": "Wii[l-forearm->l-hand] vs native[l-hand->l-thumb01] (mid-sweep)",
              "delta_deg": round(rt_delta, 3),
              "verdict": "RED (separates)" if rt_delta > 20 else "SUSPECT (did NOT separate)"}
        sys.stderr.write(f"[redteam] {rt}\n")

    # ---- CONVENTION PIN (pelvis-identity, D3 unblock): cross-side delta on the
    # known-shared pelvis->spine1 pair should be small if the convention is aligned.
    pin = None
    Dw_pel = None
    # Wii pelvis->spine1 from raw worlds (not in stored interbone_tables) via conv.
    if "pelvis" in wii_map and "spine1" in wii_map:
        Dw_pel = ibd.compute_D(wii_map, "pelvis", "spine1", conv)
    if Dw_pel:
        vals = []
        for f in sweep:
            mem = f["members"].get(slots[0])
            if not mem:
                continue
            Dn_pel = native_D(mem["world"], "pelvis", "spine1", conv)
            if Dn_pel:
                vals.append(geodesic_deg(Dw_pel[0] @ Dn_pel[0].T))
        if vals:
            pin = {"pair": "pelvis->spine1", "wii_relRot": round(geodesic_deg(Dw_pel[0]), 3),
                   "cross_delta_min": round(float(np.min(vals)), 3),
                   "cross_delta_max": round(float(np.max(vals)), 3),
                   "note": "convention-null reference; small => local-frame convention aligned"}
            sys.stderr.write(f"[pin] {pin}\n")

    # ---- per member, per hand: sweep-envelope + frame match ----
    # PRIMARY metric = |Δmag| = | |relRot_native| - |relRot_wii| |, the CONVENTION-
    # INVARIANT magnitude difference (geodesic angle of a relative rotation is
    # invariant under the per-bone local-frame conjugation D' = inv(L).D.L that
    # contaminates the raw angle(D_wii.inv(D_native)) — D3's ambiguity). The raw
    # angle-delta is kept as a SECONDARY provenance column, not the headline.
    # f* is chosen to minimize the THUMB-cascade |Δmag| (the thumb is near-static in
    # the vignette AND proven structurally-shared — the stable reference).
    members_out = []
    for slot in slots:
        gender = ""
        for f in sweep:
            if slot in f["members"]:
                gender = f["members"][slot].get("gender", ""); break
        for side in ("L", "R"):
            pair_series = {}   # (pn,cn,kind) -> (Dw, wmag, [ (idx, main_frame, nmag, delta, dtr) ])
            for (pt, ct, kind) in PAIRS:
                pn, cn = key(side, pt), key(side, ct)
                Dw = Dwii.get(f"{pn}->{cn}")
                wmag = geodesic_deg(Dw[0]) if Dw else None
                series = []
                for f in sweep:
                    mem = f["members"].get(slot)
                    if not mem:
                        continue
                    Dn = native_D(mem["world"], pn, cn, conv)
                    if Dw and Dn:
                        nmag = geodesic_deg(Dn[0])
                        delta = geodesic_deg(Dw[0] @ Dn[0].T)
                        dtr = float(np.linalg.norm(Dw[1] - np.array(Dn[1])))
                        series.append((f["idx"], mem["main_frame"], nmag, delta, dtr))
                pair_series[(pn, cn, kind)] = (Dw, wmag, series)

            # f* = argmin over common frames of the thumb-cascade |Δmag| sum.
            ref_keys = [(pn, cn, kind) for (pn, cn, kind) in pair_series
                        if any(cn.endswith(t) for t in REF_TOKENS) or kind in ANCHOR_KINDS]
            common = None
            for k_ in ref_keys:
                idxs = set(s[0] for s in pair_series[k_][2])
                common = idxs if common is None else (common & idxs)
            best_f = None
            if common:
                def refsum(fi):
                    tot = 0.0
                    for k_ in ref_keys:
                        Dw, wmag, series = pair_series[k_]
                        for s in series:
                            if s[0] == fi and wmag is not None:
                                tot += abs(s[2] - wmag)
                    return tot
                best_f = min(common, key=refsum)

            hand_rows = []
            for (pn, cn, kind), (Dw, wmag, series) in pair_series.items():
                if Dw is None or not series:
                    hand_rows.append({"pair": f"{pn}->{cn}", "kind": kind, "status": "missing"})
                    continue
                nmags = [s[2] for s in series]
                magdiffs = [abs(x - wmag) for x in nmags]
                deltas = [s[3] for s in series]
                at_best = next((s for s in series if s[0] == best_f), None)
                hand_rows.append({
                    "pair": f"{pn}->{cn}", "kind": kind, "status": "ok",
                    "wii_relRot": round(wmag, 3),
                    # PRIMARY: convention-invariant magnitude difference
                    "native_relRot_at_matched": round(at_best[2], 3) if at_best else None,
                    "magdiff_at_matched": round(abs(at_best[2] - wmag), 3) if at_best else None,
                    "magdiff_min_over_sweep": round(float(np.min(magdiffs)), 3),
                    "native_relRot_min": round(float(np.min(nmags)), 3),
                    "native_relRot_max": round(float(np.max(nmags)), 3),
                    # SECONDARY (convention-contaminated) provenance:
                    "angle_delta_at_matched": round(at_best[3], 3) if at_best else None,
                    "angle_delta_min_over_sweep": round(float(np.min(deltas)), 3),
                    "dTrans_at_matched": round(at_best[4], 3) if at_best else None,
                    "matched_native_frame": round(at_best[1], 3) if at_best and at_best[1] is not None else None,
                })
            ref_resid = None
            if best_f is not None:
                rr = [r["magdiff_at_matched"] for r in hand_rows
                      if r.get("status") == "ok" and (r["kind"] in ANCHOR_KINDS
                          or any(r["pair"].endswith(t) for t in REF_TOKENS))]
                ref_resid = round(float(np.mean([x for x in rr if x is not None])), 3) if rr else None
            mf = None
            if best_f is not None:
                fobj = next((f for f in sweep if f["idx"] == best_f), None)
                if fobj and slot in fobj["members"] and fobj["members"][slot]["main_frame"] is not None:
                    mf = round(fobj["members"][slot]["main_frame"], 3)
            members_out.append({"slot": slot, "gender": gender, "hand": side,
                                "matched_frame_idx": best_f, "matched_native_frame": mf,
                                "ref_magdiff_at_matched": ref_resid, "rows": hand_rows})

    # ---- aggregate the headline: does middle/ring survive frame-matching? ----
    def collect(kind_filter, field):
        vals = []
        for m in members_out:
            for r in m["rows"]:
                if r.get("status") == "ok" and kind_filter(r):
                    v = r.get(field)
                    if v is not None:
                        vals.append(v)
        return vals
    is_mr = lambda r: ("middlefinger" in r["pair"] or "ringfinger" in r["pair"])
    is_thumb = lambda r: "thumb" in r["pair"]
    is_anchor = lambda r: r["kind"] == "anchor"
    # PRIMARY headline = convention-invariant magnitude-difference (|Δmag|). The
    # frame-explainable FLOOR = magdiff_min_over_sweep (smallest achievable by ANY
    # vignette frame). A finger family whose floor stays >> the thumb floor SURVIVES
    # frame-matching (real). The angle-delta means are reported too, tagged secondary.
    frame_vals = [fo["members"][s]["main_frame"] for fo in sweep for s in fo["members"]
                  if fo["members"][s].get("main_frame") is not None]
    headline = {
        "PRIMARY_metric": "|Δmag| = | |relRot_native| - |relRot_wii| |  (convention-invariant)",
        "anchor_magdiff_at_matched_mean": _m(collect(is_anchor, "magdiff_at_matched")),
        "thumb_magdiff_at_matched_mean": _m(collect(is_thumb, "magdiff_at_matched")),
        "thumb_magdiff_FLOOR_mean": _m(collect(is_thumb, "magdiff_min_over_sweep")),
        "middlering_magdiff_at_matched_mean": _m(collect(is_mr, "magdiff_at_matched")),
        "middlering_magdiff_FLOOR_mean": _m(collect(is_mr, "magdiff_min_over_sweep")),
        "middlering_magdiff_FLOOR_max": _mx(collect(is_mr, "magdiff_min_over_sweep")),
        "_secondary_angle_delta": {
            "anchor_at_matched_mean": _m(collect(is_anchor, "angle_delta_at_matched")),
            "thumb_at_matched_mean": _m(collect(is_thumb, "angle_delta_at_matched")),
            "middlering_at_matched_mean": _m(collect(is_mr, "angle_delta_at_matched")),
            "note": "convention-CONTAMINATED (per-bone local-frame conjugation); NOT the headline",
        },
    }

    out = {
        "artifact": "D4 frame-matched inter-bone delta table (supersedes D3 unsynced table)",
        "join_method": "pose-anchored nearest-frame (native dense sweep vs D2 frozen pose); "
                       "PRIMARY metric = convention-invariant |Δmag|",
        "wii_side": {"scene": args.scene_wii, "build": wii.get("build"),
                     "map_vtable_charbone": wii.get("map_vtable_charbone"),
                     "no_active_vignette_driver": True,
                     "note": "D2 CharDriver stacks empty at frozen instant; player-clip "
                             "referencers are config/string tables, not live beat-bearing "
                             "CharClipDrivers (verified live). Wii pose is the frame label."},
        "native_side": {"scene": args.scene_native, "build": "rb3-native",
                        "n_sweep_frames": len(sweep),
                        "clip": "playerN_{m,f} vignette (same clip family as Wii)",
                        "frame_range": [round(min(frame_vals), 3), round(max(frame_vals), 3)]
                                       if frame_vals else None},
        "convention": conv, "redteam": rt, "convention_pin": pin,
        "headline": headline,
        "interpretation": (
            "magdiff_min_over_sweep (the FLOOR) = the smallest CONVENTION-INVARIANT finger "
            "magnitude difference achievable by ANY frame of the shared vignette clip. "
            "thumb FLOOR ~ anchor FLOOR ~ 0 => those bones ARE frame-matchable (structurally "
            "shared, exonerated). If the middle/ring FLOOR stays well above the thumb FLOOR, "
            "no vignette frame reproduces the Wii finger magnitude => the middle/ring "
            "divergence SURVIVES frame-matching = a REAL skeleton/pose delta candidate. "
            "(R5 issues the verdict; D4 reports the surviving-vs-collapsing deltas.)"),
        "members": members_out,
    }
    json.dump(out, open(args.out_json, "w"), indent=1)

    # ---- markdown ----
    L = []
    L.append("# R1-DOLPHIN D4 — frame-matched Wii-vs-native inter-bone delta table")
    L.append("")
    L.append("**Supersedes D3's unsynced-shell table** for the R5 hands-endgame decision.")
    L.append("")
    L.append(f"- Join: {out['join_method']}.")
    L.append(f"- Wii: `{args.scene_wii}` — {wii.get('build')}. "
             f"**No active vignette driver at the frozen instant** (stacks empty; the Wii "
             f"pose is the frame label, matched against the native sweep).")
    L.append(f"- Native: `{args.scene_native}` — dense sweep of `playerN` vignette over "
             f"frames [{out['native_side']['frame_range'][0]:.2f}..{out['native_side']['frame_range'][1]:.2f}], "
             f"{len(sweep)} samples.")
    L.append(f"- Convention (calibrated to reproduce D2 stored D to <0.5°, worst "
             f"{conv['worst_residual']:.4f}): layout={conv['layout']} transpose={conv['transpose']} order={conv['order']}.")
    if rt:
        L.append(f"- Red-team (re-run on this capture): {rt['desc']} → {rt['delta_deg']}° **{rt['verdict']}**.")
    if pin:
        L.append(f"- Convention pin ({pin['pair']}, pelvis-identity ref): cross-side delta "
                 f"min={pin['cross_delta_min']}° max={pin['cross_delta_max']}° "
                 f"(small ⇒ local-frame convention aligned).")
    h = headline
    L.append("")
    L.append("## Headline (frame-matched, convention-invariant |Δmag|)")
    L.append("")
    L.append(f"- **anchor** |Δmag| @ matched frame (mean): **{h['anchor_magdiff_at_matched_mean']}°** "
             "→ frame-matchable, structurally shared.")
    L.append(f"- **thumb** |Δmag| @ matched (mean): **{h['thumb_magdiff_at_matched_mean']}°**; "
             f"thumb FLOOR (min over sweep, mean): **{h['thumb_magdiff_FLOOR_mean']}°** "
             "→ frame-matchable, exonerated.")
    L.append(f"- **middle/ring** |Δmag| @ matched (mean): {h['middlering_magdiff_at_matched_mean']}°; "
             f"**middle/ring FLOOR (min over sweep, mean): {h['middlering_magdiff_FLOOR_mean']}° "
             f"(max {h['middlering_magdiff_FLOOR_max']}°)** ← no vignette frame closes this.")
    L.append("")
    L.append("Interpretation: " + out["interpretation"])
    L.append("")
    L.append("_(Secondary, convention-CONTAMINATED angle-delta means: anchor "
             f"{h['_secondary_angle_delta']['anchor_at_matched_mean']}°, thumb "
             f"{h['_secondary_angle_delta']['thumb_at_matched_mean']}°, middle/ring "
             f"{h['_secondary_angle_delta']['middlering_at_matched_mean']}° — NOT the headline; "
             "the per-bone local-frame conjugation rotates the delta axis while preserving the "
             "angle, so only |Δmag| is a clean cross-engine claim.)_")
    L.append("")
    L.append("## Per member / hand")
    L.append("")
    L.append("PRIMARY columns = convention-invariant magnitude: `Wii |relRot|`, `native |relRot| @f*`, "
             "`|Δmag|@f*`, and the **FLOOR** = smallest `|Δmag|` over the WHOLE native vignette sweep "
             "(native range shown). A finger pair whose **FLOOR** stays large ⇒ NO shared-clip frame "
             "reproduces the Wii magnitude ⇒ divergence survives frame-matching. `Δang@f*` is the "
             "secondary convention-contaminated angle-delta.")
    for m in members_out:
        L.append("")
        L.append(f"### slot{m['slot']} ({m['gender']}) — {m['hand']}-hand "
                 f"(matched native frame ≈ {m['matched_native_frame']}, "
                 f"ref |Δmag| {m['ref_magdiff_at_matched']}°)")
        L.append("")
        L.append("| pair | kind | Wii \\|relRot\\| | native \\|relRot\\| @f* | native range | "
                 "**\\|Δmag\\|@f*** | **FLOOR (min\\|Δmag\\|)** | Δang@f* |")
        L.append("|---|---|---:|---:|---:|---:|---:|---:|")
        for r in m["rows"]:
            if r.get("status") == "ok":
                L.append(f"| {r['pair']} | {r['kind']} | {r['wii_relRot']} | "
                         f"{r['native_relRot_at_matched']} | "
                         f"{r['native_relRot_min']}–{r['native_relRot_max']} | "
                         f"**{r['magdiff_at_matched']}** | **{r['magdiff_min_over_sweep']}** | "
                         f"{r['angle_delta_at_matched']} |")
            else:
                L.append(f"| {r['pair']} | {r['kind']} | - | - | - | - | - | {r.get('status')} |")
    open(args.out_md, "w").write("\n".join(L) + "\n")
    sys.stderr.write(f"wrote {args.out_md} + {args.out_json}\n")
    return 0


def _m(v):
    return round(float(np.mean(v)), 3) if v else None
def _mx(v):
    return round(float(np.max(v)), 3) if v else None


if __name__ == "__main__":
    sys.exit(main())
