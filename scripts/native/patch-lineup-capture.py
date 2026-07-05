#!/usr/bin/env python3
"""patch-lineup-capture.py — patch-bearing WIDE band lineup capture harness.

W0.5.S2 (docs/native/engine-arch-review-2026-07-05/execution/W0.5/PLAN.md).

Boots rb3-native headless, reuses band-closeup-capture.py's proven boot ->
gameplay nav + pin machinery (which itself reuses keyboard-to-gameplay.py's
proven nav), but pins WIDE/establishing band-camera shots that frame MULTIPLE
BandPatchMesh-wearing characters at once (not a single tight member closeup),
and records three numeric sources per captured frame into manifest.json:

  (A) segmentation numerics  -> computed downstream by
      scripts/analysis/lineup_bbox_metrics.py (W0.5.S1) on the PNG artifacts
      this harness writes. This harness does not compute layer A itself.
  (B) per-mesh world-extent ratios -> parsed from the engine's
      `[SHARD_RATIO] mesh='...' bindExt=... worldExt=... ratio=... band|other[ DROP]`
      log lines (Rnd_Wgpu_RB3.cpp), enabled via SHARD_RATIO_DBG=1 (default ON
      here).
  (C) per-frame draw/geometry counts -> `{rb3_char_probe <slot>}` ->
      "playerN meshes=M skinned=S verts=V loading=L" per band slot
      (native/src/rb3_http_handlers.cpp), parsed per captured frame for slots
      0..--slots-1 (default 0..3).

This harness is a CAPTURER, not a gate: it does not bake in the old
`drops_band==0` PASS/FAIL (band-closeup-capture.py's verdict). W0.5.S3 owns
composing these three numeric layers (plus the image layer) into a single
PASS/FAIL against a committed golden.

Usage:
  SHARD_RATIO_DBG=1 python3 scripts/native/patch-lineup-capture.py \
      --frames 2 --out /tmp/rb3-lineup/base --tag base

  # explicit wide shot override:
  python3 scripts/native/patch-lineup-capture.py --shots coop_all,coop_wide \
      --out /tmp/rb3-lineup/x --tag x

Exit code: 0 = nav + at least one wide shot pinned OK; 1 = nav ok but no shot
resolved / pin lost; 2 = ERROR (hook missing, bad binary, never reached
gameplay).
"""
import argparse, json, os, re, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import importlib.util

# Reuse band-closeup-capture.py's pin/nav machinery (which itself reuses
# keyboard-to-gameplay.py's proven nav) rather than re-deriving it. `bc` gives
# us: bc.k (the kbd2game module), bc.force_shot, bc.director_disable,
# bc.cur_shot, bc.advance_to_songms, bc.parse_shard_log, bc.REPO.
spec = importlib.util.spec_from_file_location(
    "bandcloseup", os.path.join(HERE, "band-closeup-capture.py"))
bc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bc)

k = bc.k
REPO = bc.REPO

# ---------------------------------------------------------------------------
# WIDE / establishing shot candidates — frame the WHOLE band (multiple
# BandPatchMesh-wearing members), not a single tight member closeup like
# band-closeup-capture.py's coop_g_cg* etc. Names are tried both bare and with
# a `.shot` suffix via bc.force_shot; whichever resolves is used. This is a
# candidate list, not a guarantee — venues vary. --shots overrides entirely.
# coop_g_n03 / coop_g_b are guitarist framings wide enough to typically also
# catch a neighbouring member (bass/drums), kept as a fallback when no
# genuinely-wide establishing shot resolves for a given venue.
WIDE_SHOTS = [
    "coop_all", "coop_wide", "coop_band", "coop_establish", "coop_establishing",
    "coop_crowd", "coop_full", "coop_stage", "coop_group",
    "coop_g_n03", "coop_g_b",
]


# ---------------------------------------------------------------------------
# Layer C: {rb3_char_probe <slot>} -> "playerN meshes=M skinned=S verts=V loading=L"
# (or "null_char (...)" / "no_charcache" when the slot/cache isn't resolvable).
# ---------------------------------------------------------------------------
_PROBE_RE = re.compile(
    r"player(\d+)\s+meshes=(\d+)\s+skinned=(\d+)\s+verts=(\d+)\s+loading=(\d+)")


def char_probe(port, slot):
    """Query {rb3_char_probe <slot>} and parse it. Returns a dict; on a
    non-matching response (null_char / no_charcache / hook missing) records
    the raw string verbatim under 'raw' with numeric fields absent, per
    PLAN.md's note that a null_char transition is a FAIL signal for S3, not a
    harness crash."""
    resp = k.dta(port, "{rb3_char_probe %d}" % slot)
    out = {"slot": slot, "raw": resp}
    if isinstance(resp, str):
        m = _PROBE_RE.match(resp.strip())
        if m:
            out.update({
                "meshes": int(m.group(2)), "skinned": int(m.group(3)),
                "verts": int(m.group(4)), "loading": int(m.group(5)),
            })
    return out


# ---------------------------------------------------------------------------
# Layer B: per-mesh [SHARD_RATIO] lines. band-closeup-capture.py's
# parse_shard_log() already reduces these to a max_band_ratio + drop
# classification; here we ALSO want the full per-mesh tuple (bindExt/worldExt/
# ratio/class/dropped) as PLAN.md §W0.5.S2 step 3 asks for, keyed by mesh name
# (last-seen value wins — the log line is throttled to ~once/60 frames per
# mesh pointer, so within one short capture window a mesh typically logs 0-1
# times; last-seen is the natural choice if it ever logs more than once).
# ---------------------------------------------------------------------------
_SHARD_RATIO_FULL_RE = re.compile(
    r"\[SHARD_RATIO\]\s+mesh='([^']*)'\s+bindExt=([\d.]+)\s+worldExt=([\d.]+)\s+"
    r"ratio=([\d.]+)\s+(band|other)(\s+DROP)?")


def parse_shard_ratios(log_path):
    """Return (per_mesh, max_band_ratio) where per_mesh is
    {mesh: {bindExt, worldExt, ratio, class, dropped}} from every
    [SHARD_RATIO] line in the log (binary-safe read, matching
    band-closeup-capture.py's parse_shard_log)."""
    per_mesh = {}
    max_band_ratio = 0.0
    try:
        raw = open(log_path, "rb").read()
    except Exception:
        return per_mesh, max_band_ratio
    txt = raw.decode("utf-8", "replace")
    for line in txt.splitlines():
        m = _SHARD_RATIO_FULL_RE.search(line)
        if not m:
            continue
        mesh, bind_ext, world_ext, ratio, cls, drop = m.groups()
        ratio_f = float(ratio)
        per_mesh[mesh] = {
            "bindExt": float(bind_ext), "worldExt": float(world_ext),
            "ratio": ratio_f, "class": cls, "dropped": bool(drop),
        }
        if cls == "band" and ratio_f > max_band_ratio:
            max_band_ratio = ratio_f
    return per_mesh, max_band_ratio


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=k.DEFAULT_BIN)
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--diff", default="hard", choices=list(k.DIFF_INDEX))
    ap.add_argument("--song-downs", type=int, default=4,
                    help="DDown presses in song_select (selects which song -> venue)")
    ap.add_argument("--shots", default="",
                    help="comma-separated explicit WIDE shot list (overrides WIDE_SHOTS)")
    ap.add_argument("--frames", type=int, default=2,
                    help="frames captured per shot for the numeric time series")
    ap.add_argument("--frame-dt", type=int, default=500,
                    help="deterministic song-clock advance (ms) between frames")
    ap.add_argument("--anchor-ms", type=float, default=-1.0,
                    help="capture ALL shots' frames at ABSOLUTE songMs targets "
                         "anchor-ms + frame_idx*frame_dt, for cross-run pose "
                         "determinism (see band-closeup-capture.py). Default -1 "
                         "= relative t0 (each shot starts at its own arrival clock).")
    ap.add_argument("--slots", type=int, default=4,
                    help="number of band char_probe slots to sample (0..N-1)")
    ap.add_argument("--out", default="")
    ap.add_argument("--tag", default="lineup",
                    help="filename prefix; distinguish base/candidate captures")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print(f"FAIL: binary not found: {args.bin}"); return 2
    out = args.out or os.path.join("/tmp", "rb3-lineup", args.tag)
    os.makedirs(out, exist_ok=True)

    cand_shots = ([s.strip() for s in args.shots.split(",") if s.strip()]
                  if args.shots else list(WIDE_SHOTS))

    port = args.port or k.free_port()
    log_path = os.path.join("/tmp", f"rb3-lineup-{args.tag}-{port}.log")
    logf = open(log_path, "w")
    # SHARD_RATIO_DBG=1 by DEFAULT (layer B depends on it); still overridable/
    # extendable via the inherited environment (A/B env toggles like
    # SHARD_GUARD_OFF=1, RB3_NO_SKEL_REBIND=1, RB3_NO_SKIN_CLAMP=1 flow through
    # os.environ exactly as band-closeup-capture.py does).
    env = dict(os.environ)
    env.setdefault("SHARD_RATIO_DBG", "1")
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay, "RB3_INPUT_DEBUG": "1"})
    print(f"[patchlineup] launching rb3-native (port {port}, tag={args.tag}); log -> {log_path}")
    print(f"[patchlineup] env toggles: " + " ".join(
        f"{e}={env[e]}" for e in ("SHARD_GUARD_OFF", "SHARD_RATIO_DBG", "SHARD_DBG",
                                  "RB3_NO_INST_REBIND", "RB3_NO_SKEL_REBIND",
                                  "RB3_NO_SKIN_CLAMP") if e in env) or "(none)")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    diff_idx = k.DIFF_INDEX[args.diff]
    rc = 2
    manifest = []        # per captured WIDE PNG: shot/songMs/cur_shot/char_probe/shard_ratios/file
    pinned_ok = 0
    pinned_total = 0
    forced_shots = []    # resolved (with-suffix) names actually pinned
    skipped = []         # not_found candidates
    try:
        # --- boot -> gameplay (reuse the SAME proven nav as
        # band-closeup-capture.py:283-336, which itself reuses
        # keyboard-to-gameplay.py verbatim) -----------------------------------
        if k.wait_screen(port, lambda s: True, 40, proc) is None:
            print("FAIL: HTTP server never came up"); return 2
        print("[patchlineup] HTTP up")
        k.wait_screen(port, lambda s: s and s != "", 60, proc, args.verbose)
        for _ in range(8):
            if k.health(port)[2] == "main_hub_screen": break
            k.press(port, k.START); k.drain_pad(port); time.sleep(0.6)
        if k.wait_screen(port, "main_hub_screen", 30, proc, args.verbose) is None:
            print("FAIL: no main_hub"); return 2
        for _ in range(10):
            if k.health(port)[2] == "song_select_screen": break
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.7)
        if k.wait_screen(port, "song_select_screen", 40, proc, args.verbose) is None:
            print("FAIL: no song_select"); return 2
        time.sleep(1.5)
        for _ in range(args.song_downs):
            k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.2)
        k.press(port, k.CONFIRM); k.drain_pad(port)
        if k.wait_screen(port, "part_difficulty_screen", 60, proc, args.verbose) is None:
            print("FAIL: no part_difficulty"); return 2
        ov = k.wait_view(port, lambda v: v.startswith("choose_part") or v == "choose_diff",
                         30, proc, "choose_part", args.verbose)
        if ov and ov[0].startswith("choose_part"):
            k.press(port, k.CONFIRM); k.drain_pad(port)
        ov = k.overshell(port); g = 0
        while ov[0] == "confirm_action" and g < 4:
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5); ov = k.overshell(port); g += 1
        ov = k.wait_view(port, lambda v: v == "choose_diff", 30, proc, "choose_diff", args.verbose)
        if ov:
            for _ in range(diff_idx):
                k.press(port, k.DDOWN); k.drain_pad(port); time.sleep(0.25)
            k.press(port, k.CONFIRM); k.drain_pad(port)
        ov = k.overshell(port); g = 0
        while ov[0] == "confirm_action" and g < 4:
            k.press(port, k.CONFIRM); k.drain_pad(port); time.sleep(0.5); ov = k.overshell(port); g += 1
        k.wait_view(port, lambda v: v == "ready_to_play", 30, proc, "ready", args.verbose)
        if k.wait_screen(port, "game_screen", 90, proc, args.verbose) is None:
            print("FAIL: no game_screen"); return 2
        print("[patchlineup] game_screen reached")
        k.verb(port, "nofail")
        is_playing = 0; start = -1; dl = time.time() + 60
        while time.time() < dl:
            ip = k.dta(port, "{game is_playing}"); h = k.health(port)
            if ip is not None and int(ip) == 1:
                is_playing = 1
                if start < 0 and h and h[1] >= 0: start = h[1]
            k.verb(port, "autohit")
            if is_playing and h and h[1] > start + 200 and start >= 0:
                break
            time.sleep(0.5)
        print(f"[patchlineup] is_playing={is_playing} songMs={k.health(port)[1]:.0f} "
              f"live_shot={bc.cur_shot(port)!r}")

        # --- pin + capture loop (WIDE shots + numeric layers B/C) -----------
        dz = bc.director_disable(port, 1)
        print(f"[patchlineup] director_disable(1) -> {dz!r}")
        if dz != 1:
            print("ERROR: rb3_director_disable did not echo 1 (hook missing?)"); return 2

        use_anchor = args.anchor_ms >= 0.0
        shot_span = args.frames * args.frame_dt
        if use_anchor:
            h_arr = k.health(port)
            arrival = h_arr[1] if h_arr else 0.0
            if args.anchor_ms < arrival + 100:
                print(f"[patchlineup] WARN: --anchor-ms {args.anchor_ms:.0f} <= "
                      f"arrival songMs {arrival:.0f}; the clock can't rewind. "
                      f"Pick an anchor a few seconds ahead (e.g. {arrival+3000:.0f}).")
        shot_idx = 0
        for shot in cand_shots:
            resolved, resp = bc.force_shot(port, shot)
            if resolved is None:
                if isinstance(resp, int):
                    print(f"ERROR: rb3_force_shot returned int {resp} — hook MISSING")
                    return 2
                print(f"[patchlineup]   skip {shot}: {resp}")
                skipped.append((shot, resp))
                continue
            forced_shots.append(resolved)
            base = resolved.replace(".shot", "")
            print(f"[patchlineup] force {resolved} -> {resp}")
            for _ in range(3):
                k.verb(port, "autohit"); time.sleep(0.15)
            cur = bc.cur_shot(port)
            if cur != resolved:
                print(f"[patchlineup]   WARN: cur_shot={cur!r} != forced {resolved!r}")
            if use_anchor:
                t0 = args.anchor_ms + shot_idx * shot_span
            else:
                h0 = k.health(port)
                t0 = h0[1] if h0 else 0.0
            for i in range(args.frames):
                target = t0 + i * args.frame_dt
                got_ms = bc.advance_to_songms(port, target)
                cur = bc.cur_shot(port)
                pinned_total += 1
                if cur == resolved:
                    pinned_ok += 1
                p = os.path.join(out, f"{args.tag}_{base}_{i}.png")
                ok = k.screenshot(port, p)
                probes = [char_probe(port, s) for s in range(args.slots)]
                manifest.append({
                    "shot": base, "shot_full": resolved, "frame_idx": i,
                    "songMs": round(got_ms, 1), "cur_shot": cur,
                    "pinned": (cur == resolved), "file": p, "ok": ok,
                    "char_probe": probes,
                })
                if args.verbose:
                    print(f"[patchlineup]     {base}[{i}] target={target:.0f} "
                          f"songMs={got_ms:.0f} cur_shot={cur!r} ok={ok} "
                          f"probes={[p.get('raw') for p in probes]}")
            shot_idx += 1

        # --- diagnostics + manifest (no PASS/FAIL verdict — S3 owns that) ---
        logf.flush()
        shard = bc.parse_shard_log(log_path)
        per_mesh_ratios, max_band_ratio = parse_shard_ratios(log_path)
        # fold the per-mesh shard_ratios into every frame's manifest entry — a
        # single capture run shares one engine process/log, so the per-mesh
        # table is frame-independent within this run (S3 can refine to
        # per-frame windows later if the log ever gets frame-scoped).
        for entry in manifest:
            entry["shard_ratios"] = per_mesh_ratios

        with open(os.path.join(out, "manifest.json"), "w") as mf:
            json.dump({
                "tag": args.tag, "diff": args.diff, "song_downs": args.song_downs,
                "frames": args.frames, "frame_dt": args.frame_dt,
                "anchor_ms": args.anchor_ms, "slots": args.slots,
                "forced_shots": forced_shots, "skipped": skipped,
                "frames_manifest": manifest,
                "shard_ratios": per_mesh_ratios,
                "max_band_ratio": round(max_band_ratio, 2),
                "drops_total": shard["drops_total"], "drops_band": shard["drops_band"],
                "drops_other": shard["drops_other"], "drop_meshes": shard["drop_meshes"],
                "env": {e: env[e] for e in ("SHARD_GUARD_OFF", "SHARD_RATIO_DBG",
                        "SHARD_DBG", "RB3_NO_INST_REBIND", "RB3_NO_SKEL_REBIND",
                        "RB3_NO_SKIN_CLAMP") if e in env},
                "log": log_path,
            }, mf, indent=2)

        # nav+pin ok if we reached gameplay and pinned at least one wide shot.
        if not forced_shots:
            rc = 1
            print("FAIL: no WIDE shot candidate resolved for this venue "
                  "(try --shots with venue-specific names)")
        elif pinned_total == 0 or pinned_ok != pinned_total:
            rc = 1
            print(f"FAIL: pin lost ({pinned_ok}/{pinned_total})")
        else:
            rc = 0

        print(
            f"PATCH_LINEUP out={out} frames={len(manifest)} "
            f"forced_shots={','.join(forced_shots) or 'none'} "
            f"max_band_ratio={max_band_ratio:.2f}"
        )
        return rc
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
            try: proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception: pass
        logf.close()
        print(f"[patchlineup] engine log: {log_path}")
        print(f"[patchlineup] out: {out}")


if __name__ == "__main__":
    sys.exit(main())
