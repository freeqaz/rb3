#!/usr/bin/env python3
"""lineup-gate.py — composite non-blind visual lineup gate (W0.5.S3).

docs/native/engine-arch-review-2026-07-05/execution/W0.5/PLAN.md §W0.5.S3.

ONE driver that runs a WIDE band-lineup capture (via patch-lineup-capture.py,
W0.5.S2) and compares each captured frame against a COMMITTED golden across four
layers, returning a single PASS/FAIL with visible per-layer verdicts:

  image  (ADVISORY, does NOT gate) — visual_diff.py --perceptual candidate-vs-
         golden PNG. The doc shows this layer is fool-able by scattered slivers
         (a shard explosion keeps the translation-tolerant score above min), so
         it is REPORTED but never fails the gate on its own.
  segA   (NUMERIC, gates) — scripts/analysis/lineup_bbox_metrics.compare_to_golden
         on each WIDE PNG: n_slivers / n_components / mean_solidity / fg_fill /
         fg_bbox_diag vs golden. A compact character passes; an exploded one
         scatters thin low-solidity slivers and fails.
  ratioB (NUMERIC, gates) — per-mesh world-extent ratios from the engine's
         `[SHARD_RATIO]` log (SHARD_RATIO_DBG=1): every mesh's ratio <= the
         golden-derived per_mesh_ratio_cap AND max_band_ratio within golden bound.
  countC (NUMERIC, gates) — per-slot {rb3_char_probe} draw/geometry counts
         (meshes/skinned/verts) within golden tolerance, and NO slot flipped to
         null_char. A patch/shard explosion that adds/drops/re-tessellates draws
         moves these counts.

Overall verdict = AND of the NUMERIC layers (segA, ratioB, countC). The image
layer is printed but excluded from the AND (it is the blind one) so an
"image PASS but numeric FAIL" is visible — the whole point of W0.5.

Modes:
  --gen-golden   run a capture on the current KNOWN-GOOD build and write the
                 committed golden (golden.json + golden WIDE PNGs) under
                 scripts/native/goldens/w0.5-lineup/. Refuses to write a golden
                 whose frames are black/near-empty (GPU-OOM guard — see the
                 W0.5.S2 STATUS ENVIRONMENT CAVEAT).
  (default)      GATE: run a capture with the golden's params/shots and gate it.

Exit: 0 = PASS, 1 = FAIL (a numeric layer regressed), 2 = ERROR (capture/nav
failed, golden missing/black, hook missing).
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
ANALYSIS = os.path.join(REPO, "scripts", "analysis")
GOLDEN_DIR = os.path.join(HERE, "goldens", "w0.5-lineup")
GOLDEN_JSON = os.path.join(GOLDEN_DIR, "golden.json")
CAPTURE = os.path.join(HERE, "patch-lineup-capture.py")

# Import S1 (segmentation analyzer) + visual_diff (image layer) as modules.
for p in (ANALYSIS, HERE):
    if p not in sys.path:
        sys.path.insert(0, p)
import lineup_bbox_metrics as lbm  # noqa: E402
import visual_diff as vd           # noqa: E402


# ---------------------------------------------------------------------------
# Gate tolerances for the layers S1 does NOT own (B, C) + the image layer.
# Layer A (segmentation) tolerances live in lineup_bbox_metrics as GOLD_* and
# are tuned there; these three are module constants here so they are auditable.
# ---------------------------------------------------------------------------
# Layer B (per-mesh world-extent ratio). The golden's max band ratio is the
# largest bind->world extent blowup a KNOWN-GOOD animated band mesh reaches.
# A shard explosion flings verts, so the per-mesh ratio balloons far past this.
# Cap = golden_max * (1 + slack) with an absolute floor so a low golden max
# (e.g. an almost-static pose) still leaves headroom for normal animation.
RATIO_CAP_SLACK = 0.50          # per_mesh_ratio_cap = golden_max * 1.50 ...
RATIO_CAP_FLOOR = 8.0           # ... but never below this absolute cap
MAX_BAND_RATIO_SLACK = 0.35     # max_band_ratio <= golden_max * 1.35 (+floor)

# Layer C (draw/geometry counts). Topology (mesh/vert counts) is pose-invariant
# on a good build, so these are tight; a re-tessellation / dropped-draw moves
# them. verts gets a little slack for LOD selection jitter across boots.
COUNT_MESHES_SLACK = 0.10       # |meshes  / golden - 1| <= this
COUNT_SKINNED_SLACK = 0.10      # |skinned / golden - 1| <= this
COUNT_VERTS_SLACK = 0.08        # |verts   / golden - 1| <= this

# Image layer (advisory only).
IMAGE_MIN_SCORE = 35.0          # visual_diff --perceptual PASS threshold

# Segmentation (layer A) gating policy. MEASURED across 3 good-build runs (12
# WIDE venue frames), fill/solidity/component-count/bbox vary a lot run-to-run
# from legit crowd/lighting/particle nondeterminism (fg_fill 0.17-0.76,
# mean_solidity 0.34-0.57, n_components 5-36, fg_bbox_diag always ~frame diag),
# so those are ADVISORY (reported, not gated). The shard-specific, RUN-STABLE
# signal is the thin-sliver count (0-5 across all 12 good frames) plus gross
# fragmentation: a BandPatchMesh explosion scatters MANY thin slivers / fragments
# (S1 selftest: a modest shatter = 15+ slivers, 19 comps). So segA GATES on
# n_slivers + n_components only; fill/solidity/bbox stay advisory. These land in
# golden.json (per gen-golden) so the policy travels with the golden.
# Two gating signals:
#  - n_slivers: golden-relative (baseline is run-stable near 0-4). A shard
#    explosion scatters MANY thin slivers -> count spikes past golden + slack.
#  - n_components_abs: an ABSOLUTE explosion cap. Component count is too noisy
#    run-to-run (measured 5-51 on good frames) to gate golden-relative, but an
#    absolute ceiling well above the good max still catches a fragmentation
#    explosion (patch shards -> hundreds of components) without false-failing.
SEG_GATING = ["n_slivers", "n_components_abs"]
SEG_ABS_COMPONENT_CAP = 110     # good-build max observed ~51; explosion -> 100s
SEG_TOL = {
    "sliver_abs_slack": 8,      # n_slivers <= golden + 8   (good envelope max 5)
    # remaining checks are ADVISORY (excluded from the gating AND); kept loose so
    # their reported bound is sane, not accidentally red on a noisy-but-good frame.
    "component_factor": 3.0, "solidity_factor": 0.99,
    "fill_factor": 0.99, "bbox_extent_factor": 5.0,
}


def seg_verdict(metrics, golden_seg, tol=None, gating=None, abs_comp_cap=None):
    """Segmentation-layer verdict: compare_to_golden for the full advisory report,
    plus an absolute component-explosion check; derive PASS/FAIL from ONLY the
    gating signals (n_slivers golden-relative + n_components_abs). Returns
    (gate_pass, checks_dict)."""
    tol = tol or SEG_TOL
    gating = gating or SEG_GATING
    cap = SEG_ABS_COMPONENT_CAP if abs_comp_cap is None else abs_comp_cap
    checks = lbm.compare_to_golden(metrics, golden_seg, tol=tol)["checks"]
    ncomp = metrics.get("n_components", 0)
    checks["n_components_abs"] = {
        "pass": bool(ncomp <= cap), "observed": ncomp, "bound": cap,
        "rule": f"<= {cap} (absolute explosion cap)"}
    gate_pass = all(checks[n]["pass"] for n in gating if n in checks)
    return gate_pass, checks

# Golden-generation black-frame guard: reject a golden frame whose foreground is
# below this fraction of the frame (GPU-OOM captured a black PNG).
MIN_FG_FRAC = 0.01


# ---------------------------------------------------------------------------
# Capture driver — shell out to S2 (patch-lineup-capture.py) and read manifest.
# ---------------------------------------------------------------------------
def run_capture(binary, out_dir, tag, shots, frames, frame_dt, anchor_ms,
                song_downs, diff, slots, extra_env=None):
    """Run S2's capture as a subprocess (reuses its proven nav/pin verbatim) and
    return (rc, manifest_dict). The inherited environment flows through, so
    broken-skin toggles (RB3_NO_SKEL_REBIND / SHARD_GUARD_OFF / RB3_NO_SKIN_CLAMP)
    set by the caller reach the engine exactly as S4's fail-red needs."""
    cmd = [sys.executable, CAPTURE, "--bin", binary, "--out", out_dir,
           "--tag", tag, "--frames", str(frames), "--frame-dt", str(frame_dt),
           "--song-downs", str(song_downs), "--diff", diff, "--slots", str(slots)]
    if shots:
        cmd += ["--shots", ",".join(shots)]
    if anchor_ms is not None and anchor_ms >= 0:
        cmd += ["--anchor-ms", str(anchor_ms)]
    env = dict(os.environ)
    if extra_env:
        env.update(extra_env)
    print(f"[lineup-gate] capture: {' '.join(cmd)}")
    rc = subprocess.call(cmd, env=env, cwd=REPO)
    man_path = os.path.join(out_dir, "manifest.json")
    manifest = None
    if os.path.exists(man_path):
        with open(man_path) as f:
            manifest = json.load(f)
    return rc, manifest


def _probe_map(char_probe):
    """slot -> probe dict, keeping only slots that returned numeric counts."""
    m = {}
    for p in char_probe or []:
        m[p.get("slot")] = p
    return m


def _has_counts(p):
    return p is not None and all(k in p for k in ("meshes", "skinned", "verts"))


# ---------------------------------------------------------------------------
# Layer B — per-mesh world-extent ratio (run-level; one engine log per capture).
# ---------------------------------------------------------------------------
def gate_ratio(shard_ratios, max_band_ratio, golden):
    cap = golden["per_mesh_ratio_cap"]
    g_max = golden.get("shard_ratio_max", 0.0)
    max_band_bound = max(g_max * (1 + MAX_BAND_RATIO_SLACK), RATIO_CAP_FLOOR)
    offenders = []
    for mesh, d in (shard_ratios or {}).items():
        r = d.get("ratio", 0.0)
        if r > cap:
            offenders.append({"mesh": mesh, "ratio": r, "class": d.get("class")})
    ok_cap = len(offenders) == 0
    ok_max = max_band_ratio <= max_band_bound
    return {
        "pass": bool(ok_cap and ok_max),
        "per_mesh_ratio_cap": cap,
        "offenders": offenders[:20],
        "n_offenders": len(offenders),
        "max_band_ratio": max_band_ratio,
        "max_band_bound": round(max_band_bound, 3),
        "max_band_ok": bool(ok_max),
    }


# ---------------------------------------------------------------------------
# Layer C — per-slot draw/geometry counts vs golden (per frame).
# ---------------------------------------------------------------------------
def gate_counts(cand_probe, golden_probe):
    gmap = _probe_map(golden_probe)
    cmap = _probe_map(cand_probe)
    checks = []
    ok = True
    for slot, g in gmap.items():
        if not _has_counts(g):
            continue  # golden slot had no counts -> nothing to assert
        c = cmap.get(slot)
        if not _has_counts(c):
            # golden had numeric counts, candidate flipped to null_char/missing.
            checks.append({"slot": slot, "pass": False,
                           "reason": "null_char/missing (golden had counts)",
                           "raw": None if c is None else c.get("raw")})
            ok = False
            continue
        slot_ok = True
        detail = {}
        for field, slack in (("meshes", COUNT_MESHES_SLACK),
                             ("skinned", COUNT_SKINNED_SLACK),
                             ("verts", COUNT_VERTS_SLACK)):
            gv = g[field]
            cv = c[field]
            if gv == 0:
                field_ok = (cv == 0)
                ratio = None
            else:
                ratio = cv / gv
                field_ok = abs(ratio - 1.0) <= slack
            detail[field] = {"golden": gv, "observed": cv,
                             "ratio": None if ratio is None else round(ratio, 3),
                             "pass": bool(field_ok)}
            slot_ok = slot_ok and field_ok
        checks.append({"slot": slot, "pass": bool(slot_ok), "detail": detail})
        ok = ok and slot_ok
    return {"pass": bool(ok), "slots": checks}


# ---------------------------------------------------------------------------
# Image layer — ADVISORY perceptual compare (reported, does not gate).
# ---------------------------------------------------------------------------
def image_layer(cand_png, golden_png):
    try:
        a = vd.load_rgb(cand_png)
        b = vd.load_rgb(golden_png)
        res = vd.diff_perceptual(a, b, min_score=IMAGE_MIN_SCORE)
        return {"verdict": res.verdict, "score": res.score,
                "min_score": IMAGE_MIN_SCORE}
    except Exception as e:
        return {"verdict": "ERROR", "error": str(e)}


def _frame_key(entry):
    return (entry.get("shot"), entry.get("frame_idx"))


def _is_black(seg_metrics):
    total = seg_metrics.get("width", 0) * seg_metrics.get("height", 0)
    if total <= 0:
        return True
    return seg_metrics.get("total_fg_px", 0) < total * MIN_FG_FRAC


# ---------------------------------------------------------------------------
# Golden generation
# ---------------------------------------------------------------------------
def gen_golden(args):
    out_dir = args.out or "/tmp/rb3-lineup/golden-gen"
    if os.path.isdir(out_dir):
        shutil.rmtree(out_dir, ignore_errors=True)
    rc, manifest = run_capture(
        args.bin, out_dir, "golden", args.shots or None, args.frames,
        args.frame_dt, args.anchor_ms, args.song_downs, args.diff, args.slots)
    if rc != 0 or not manifest:
        print(f"ERROR: capture failed (rc={rc}) — cannot generate golden")
        return 2
    frames = [e for e in manifest.get("frames_manifest", []) if e.get("ok")]
    if not frames:
        print("ERROR: capture produced no successful frames")
        return 2

    golden_frames = []
    black = []
    for e in frames:
        png = e["file"]
        try:
            metrics = lbm.analyze_path(png).to_dict()
        except Exception as ex:
            print(f"ERROR: analyze {png}: {ex}")
            return 2
        if _is_black(metrics):
            black.append((png, metrics.get("total_fg_px")))
            continue
        base = f"{e['shot']}_{e['frame_idx']}.png"
        golden_frames.append({
            "shot": e["shot"], "frame_idx": e["frame_idx"], "png": base,
            "seg_metrics": metrics,
            "char_probe": [p for p in e.get("char_probe", [])],
            "songMs": e.get("songMs"),
        })

    if black:
        print(f"ERROR: {len(black)} golden frame(s) are BLACK/near-empty "
              f"(GPU-OOM? free GPU mem and retry): {black}")
        return 2
    if not golden_frames:
        print("ERROR: no non-black golden frames")
        return 2

    shard_ratios = manifest.get("shard_ratios", {})
    g_max = manifest.get("max_band_ratio", 0.0)
    per_mesh_ratio_cap = round(max(g_max * (1 + RATIO_CAP_SLACK), RATIO_CAP_FLOOR), 3)

    golden = {
        "generated_from": os.path.abspath(args.bin),
        "capture_params": {
            "frames": args.frames, "frame_dt": args.frame_dt,
            "anchor_ms": args.anchor_ms, "song_downs": args.song_downs,
            "diff": args.diff, "slots": args.slots,
        },
        "shots": manifest.get("forced_shots", []),
        "shard_ratio_max": g_max,
        "per_mesh_ratio_cap": per_mesh_ratio_cap,
        "n_shard_meshes": len(shard_ratios),
        "seg_gating": SEG_GATING,
        "seg_tol": SEG_TOL,
        "seg_abs_component_cap": SEG_ABS_COMPONENT_CAP,
        "frames": golden_frames,
    }

    os.makedirs(GOLDEN_DIR, exist_ok=True)
    # Copy golden PNGs alongside golden.json.
    for e in frames:
        gf = next((g for g in golden_frames
                   if g["shot"] == e["shot"] and g["frame_idx"] == e["frame_idx"]),
                  None)
        if gf:
            shutil.copyfile(e["file"], os.path.join(GOLDEN_DIR, gf["png"]))
    with open(GOLDEN_JSON, "w") as f:
        json.dump(golden, f, indent=2)

    print(f"[lineup-gate] wrote golden: {GOLDEN_JSON}")
    print(f"[lineup-gate]   shots={golden['shots']} frames={len(golden_frames)} "
          f"per_mesh_ratio_cap={per_mesh_ratio_cap} shard_ratio_max={g_max}")
    for g in golden_frames:
        sm = g["seg_metrics"]
        print(f"[lineup-gate]   {g['shot']}[{g['frame_idx']}] "
              f"n_comp={sm['n_components']} n_sliv={sm['n_slivers']} "
              f"mean_sol={sm['mean_solidity']:.3f} fill={sm['fg_fill']:.3f} "
              f"diag={sm['fg_bbox_diag']:.0f}")
    return 0


# ---------------------------------------------------------------------------
# Gate mode
# ---------------------------------------------------------------------------
def gate(args):
    if not os.path.exists(GOLDEN_JSON):
        print(f"ERROR: no committed golden at {GOLDEN_JSON} — run --gen-golden first")
        return 2
    with open(GOLDEN_JSON) as f:
        golden = json.load(f)
    cp = golden.get("capture_params", {})
    # Use the golden's own capture params + resolved shots so frames align by
    # (shot, frame_idx) and (with anchor_ms) pose.
    shots = args.shots or golden.get("shots") or None
    frames = args.frames or cp.get("frames", 2)
    frame_dt = cp.get("frame_dt", 500)
    anchor_ms = cp.get("anchor_ms", -1.0) if args.anchor_ms is None else args.anchor_ms
    song_downs = cp.get("song_downs", 4)
    diff = cp.get("diff", "hard")
    slots = cp.get("slots", 4)

    out_dir = args.out or "/tmp/rb3-lineup/gate"
    if os.path.isdir(out_dir):
        shutil.rmtree(out_dir, ignore_errors=True)
    rc, manifest = run_capture(args.bin, out_dir, "cand", shots, frames,
                               frame_dt, anchor_ms, song_downs, diff, slots)
    if not manifest:
        print(f"ERROR: capture produced no manifest (rc={rc})")
        return 2
    # rc==1 (pin lost / no shot) is a legitimate FAIL signal, not an ERROR — a
    # broken build can lose the pin; fold it into the gate verdict below.

    golden_by_key = {(g["shot"], g["frame_idx"]): g for g in golden["frames"]}
    cand_frames = [e for e in manifest.get("frames_manifest", []) if e.get("ok")]

    # Run-level layer B (one engine log per capture).
    ratioB = gate_ratio(manifest.get("shard_ratios", {}),
                        manifest.get("max_band_ratio", 0.0), golden)

    frame_verdicts = []
    any_black = False
    for e in cand_frames:
        key = _frame_key(e)
        g = golden_by_key.get(key)
        if g is None:
            continue  # candidate shot/frame not in golden (extra) — skip
        png = e["file"]
        try:
            metrics = lbm.analyze_path(png).to_dict()
        except Exception as ex:
            frame_verdicts.append({"key": list(key), "error": f"analyze: {ex}"})
            continue
        black = _is_black(metrics)
        any_black = any_black or black
        seg_tol = golden.get("seg_tol", SEG_TOL)
        seg_gating = golden.get("seg_gating", SEG_GATING)
        seg_cap = golden.get("seg_abs_component_cap", SEG_ABS_COMPONENT_CAP)
        segA_pass, segA_checks = seg_verdict(metrics, g["seg_metrics"], seg_tol,
                                             seg_gating, seg_cap)
        countC = gate_counts(e.get("char_probe"), g.get("char_probe"))
        img = image_layer(png, os.path.join(GOLDEN_DIR, g["png"]))
        frame_verdicts.append({
            "key": list(key), "file": png, "black": black,
            "image": img, "segA": "PASS" if segA_pass else "FAIL",
            "segA_gating": seg_gating, "segA_checks": segA_checks,
            "countC": countC["pass"], "countC_slots": countC["slots"],
        })

    matched = [fv for fv in frame_verdicts if "error" not in fv]
    if not matched:
        print("ERROR: no candidate frame matched the golden by (shot, frame_idx)")
        return 2

    segA_ok = all(fv["segA"] == "PASS" for fv in matched)
    countC_ok = all(fv["countC"] for fv in matched)
    ratioB_ok = ratioB["pass"]
    pin_ok = (rc == 0)

    # Overall = AND of numeric layers. Image is advisory. A lost pin (rc==1)
    # is also a FAIL (the shot the golden pinned no longer resolves/holds).
    overall = "PASS" if (segA_ok and ratioB_ok and countC_ok and pin_ok) else "FAIL"

    img_verdicts = [fv["image"].get("verdict") for fv in matched]
    img_summary = ("PASS" if all(v == "PASS" for v in img_verdicts)
                   else "FAIL" if any(v == "FAIL" for v in img_verdicts)
                   else "MIXED")

    verdict = {
        "verdict": overall,
        "layers": {
            "image_advisory": img_summary,
            "segA": "PASS" if segA_ok else "FAIL",
            "ratioB": "PASS" if ratioB_ok else "FAIL",
            "countC": "PASS" if countC_ok else "FAIL",
            "pin": "PASS" if pin_ok else "FAIL",
        },
        "any_black_frame": any_black,
        "capture_rc": rc,
        "ratioB_detail": ratioB,
        "frames": frame_verdicts,
        "golden": GOLDEN_JSON,
        "shots": manifest.get("forced_shots", []),
    }
    vpath = os.path.join(out_dir, "verdict.json")
    with open(vpath, "w") as f:
        json.dump(verdict, f, indent=2)

    # Human-readable per-layer verdicts.
    print("\n=== LINEUP GATE (per-layer) ===")
    for fv in matched:
        gating = fv.get("segA_gating", SEG_GATING)
        ck = fv["segA_checks"]
        sa_fail = [n for n in gating if n in ck and not ck[n]["pass"]]
        adv = ck.get("mean_solidity", {}).get("observed"), ck.get("fg_fill", {}).get("observed")
        print(f"  frame {fv['key']} black={fv['black']} "
              f"img={fv['image'].get('verdict')}({fv['image'].get('score')}) "
              f"segA={fv['segA']}{'/'+','.join(sa_fail) if sa_fail else ''} "
              f"[sliv={ck.get('n_slivers',{}).get('observed')} "
              f"ncomp={ck.get('n_components',{}).get('observed')} "
              f"adv:sol={adv[0]} fill={adv[1]}] "
              f"countC={'PASS' if fv['countC'] else 'FAIL'}")
    if not ratioB_ok:
        print(f"  ratioB FAIL: cap={ratioB['per_mesh_ratio_cap']} "
              f"n_offenders={ratioB['n_offenders']} "
              f"max_band={ratioB['max_band_ratio']}(<= {ratioB['max_band_bound']}? "
              f"{ratioB['max_band_ok']})")
        for o in ratioB["offenders"][:5]:
            print(f"      offender mesh={o['mesh']!r} ratio={o['ratio']} ({o['class']})")
    if any_black:
        print("  NOTE: >=1 candidate frame is BLACK/near-empty (possible GPU-OOM, "
              "not necessarily a shard regression — check nvidia-smi headroom)")
    print(f"[lineup-gate] verdict.json -> {vpath}")
    print(f"LINEUP_GATE verdict={overall} img={img_summary} "
          f"segA={'PASS' if segA_ok else 'FAIL'} "
          f"ratioB={'PASS' if ratioB_ok else 'FAIL'} "
          f"countC={'PASS' if countC_ok else 'FAIL'} pin={'PASS' if pin_ok else 'FAIL'}")
    return 0 if overall == "PASS" else 1


# ---------------------------------------------------------------------------
# Selftest — GPU-INDEPENDENT correctness proof of the composite gate logic.
# Proves each NUMERIC layer (segA, ratioB, countC) PASSes a clean synthetic
# lineup and FAILs an exploded one, WITHOUT needing a rendered frame (so it
# runs even when the GPU is saturated / captures return black). The segmentation
# frames reuse S1's synthetic compact/shattered generators; the ratio/count
# layers use synthetic golden vs clean/exploded structures shaped like a real
# manifest (per the W0.5.S2 capture: meshes~140, verts~15395, max_band_ratio~4.5).
# ---------------------------------------------------------------------------
def selftest():
    ok = True
    reasons = []

    # --- Layer A (segmentation) via S1's synthetic frames ------------------
    compact = lbm.analyze(lbm._synth_compact()).to_dict()
    shattered = lbm.analyze(lbm._synth_shattered()).to_dict()
    # Exercise the SAME gating (n_slivers + n_components) the real gate uses.
    a_clean, _ = seg_verdict(compact, compact)
    a_boom, cks = seg_verdict(shattered, compact)
    print(f"SELFTEST segA: clean_pass={a_clean} exploded_pass={a_boom} "
          f"(shattered n_sliv={cks['n_slivers']['observed']} "
          f"n_comp={cks['n_components']['observed']})")
    if not a_clean:
        ok = False; reasons.append("segA: clean frame did not PASS its own golden")
    if a_boom:
        ok = False; reasons.append("segA: exploded frame did not FAIL (BLIND)")

    # --- Synthetic golden shaped like a real capture -----------------------
    golden = {
        "shard_ratio_max": 4.5,
        "per_mesh_ratio_cap": round(max(4.5 * (1 + RATIO_CAP_SLACK), RATIO_CAP_FLOOR), 3),
        "frames": [{
            "shot": "coop_g_n03", "frame_idx": 0, "png": "coop_g_n03_0.png",
            "seg_metrics": compact,
            "char_probe": [
                {"slot": 0, "meshes": 140, "skinned": 0, "verts": 15395, "loading": 0},
                {"slot": 2, "meshes": 144, "skinned": 4, "verts": 15395, "loading": 0},
            ],
        }],
    }

    # --- Layer B (per-mesh ratio) ------------------------------------------
    clean_ratios = {"head.mesh": {"ratio": 3.9, "class": "band"},
                    "torso.mesh": {"ratio": 4.4, "class": "band"}}
    boom_ratios = {"head.mesh": {"ratio": 3.9, "class": "band"},
                   "lowtopsneaks_skin.2.mesh": {"ratio": 41.7, "class": "band"}}
    b_clean = gate_ratio(clean_ratios, 4.4, golden)["pass"]
    b_boom = gate_ratio(boom_ratios, 41.7, golden)["pass"]
    print(f"SELFTEST ratioB: clean_pass={b_clean} exploded_pass={b_boom} "
          f"(cap={golden['per_mesh_ratio_cap']})")
    if not b_clean:
        ok = False; reasons.append("ratioB: clean ratios did not PASS")
    if b_boom:
        ok = False; reasons.append("ratioB: exploded mesh ratio did not FAIL (BLIND)")

    # --- Layer C (draw/geometry counts) ------------------------------------
    clean_probe = [dict(p) for p in golden["frames"][0]["char_probe"]]
    # exploded: a slot re-tessellates (verts blow up) AND another flips null_char.
    boom_probe = [
        {"slot": 0, "raw": "null_char (slot 0)"},                        # flipped
        {"slot": 2, "meshes": 210, "skinned": 4, "verts": 61234, "loading": 0},  # verts blown
    ]
    c_clean = gate_counts(clean_probe, golden["frames"][0]["char_probe"])["pass"]
    c_boom = gate_counts(boom_probe, golden["frames"][0]["char_probe"])["pass"]
    print(f"SELFTEST countC: clean_pass={c_clean} exploded_pass={c_boom}")
    if not c_clean:
        ok = False; reasons.append("countC: clean counts did not PASS")
    if c_boom:
        ok = False; reasons.append("countC: exploded counts/null_char did not FAIL (BLIND)")

    if ok:
        print("SELFTEST: PASS — all three numeric layers separate clean vs exploded")
        return 0
    print("SELFTEST: FAIL")
    for r in reasons:
        print("  -", r)
    return 1


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--gen-golden", action="store_true",
                    help="capture the current build and write the committed golden")
    ap.add_argument("--selftest", action="store_true",
                    help="GPU-independent proof the composite gate separates "
                         "clean vs exploded (segA/ratioB/countC)")
    ap.add_argument("--bin", default=os.path.join(
        REPO, "native", "build-agent-W0.5", "rb3-native"),
        help="rb3-native binary to drive (default: build-agent-W0.5)")
    ap.add_argument("--out", default="",
                    help="capture output dir (default: /tmp/rb3-lineup/{golden-gen,gate})")
    ap.add_argument("--shots", default="",
                    help="comma-separated WIDE shot override (default: golden's shots)")
    ap.add_argument("--frames", type=int, default=0,
                    help="frames per shot (default: golden's / 2 for gen)")
    ap.add_argument("--frame-dt", type=int, default=500)
    ap.add_argument("--anchor-ms", type=float, default=None,
                    help="absolute songMs anchor for cross-run pose determinism")
    ap.add_argument("--song-downs", type=int, default=4)
    ap.add_argument("--diff", default="hard")
    ap.add_argument("--slots", type=int, default=4)
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()

    args.shots = [s.strip() for s in args.shots.split(",") if s.strip()]
    if args.gen_golden:
        if not args.frames:
            args.frames = 2
        return gen_golden(args)
    return gate(args)


if __name__ == "__main__":
    sys.exit(main())
