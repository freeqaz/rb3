#!/usr/bin/env python3
"""band-closeup-capture.py — DETERMINISTIC force-band-closeup capture harness.

Boots rb3-native headless, navigates to gameplay (reusing keyboard-to-gameplay's
proven nav verbatim), then PINS a venue camera shot on a band member and captures
matched (shot, songMs) frames for A/B convergence diffing.

This is the tool the convergence batch was missing. The probe
(docs/native/converge-2026-06-20/probe-data.md §1) found that the editor verbs
`{band_director force_shot ...}` / `{$band_director set disabled 1}` are SILENT
no-ops in native (TheBandDirector is a C++ global, not a DTA object) — so every
A/B frame the old crowd-shot-capture.py grabbed was at a DIFFERENT auto-director
angle (the camera-desync false positive). This harness uses three NATIVE-ONLY DTA
accessors added in native/src/rb3_http_handlers.cpp:

  {rb3_director_disable <0|1>}  -> freeze/unfreeze the per-frame auto re-pick
  {rb3_force_shot "<name>"}     -> pin a BandCamShot by name; returns a status string
  {rb3_cur_shot}                -> the live mCurShot name (the determinism proof)

The pin needs BOTH director_disable(1) (FIRST) and force_shot (so OnSelectCamera
can't re-pick over it). After forcing, {rb3_cur_shot} == forced name on EVERY
frame, which is the hard determinism gate this harness asserts.

VENUE NUANCE (discovered on-device): shot names are venue-specific AND need the
`.shot` suffix to resolve via ObjectDir::Find (e.g. `coop_g_cg.shot`, NOT
`coop_g_cg`, and NOT the `coop_dir_g_cls00` dir-cut SELECTOR names the probe
listed for arena_01). The default boot song (--song-downs 4) loads a club-family
venue whose guitarist closeups are `coop_g_cg*` (close-guitar) + `coop_g_n0*`
(numbered framings). The harness sends each candidate WITH and WITHOUT a `.shot`
suffix and keeps whichever resolves (force_shot ok); not_found candidates are
skipped + warned, so a different --song/venue degrades gracefully.

Usage (single pass):
  SHARD_RATIO_DBG=1 SHARD_DBG=1 python3 scripts/native/band-closeup-capture.py \
      --member guitar --frames 3 --frame-dt 500 --out /tmp/bc/on --tag on

A/B pairing (two passes, same params, different env), then matched-frame diff:
  python3 scripts/native/band-closeup-capture.py --member guitar --out /tmp/bc/on --tag on
  SHARD_GUARD_OFF=1 python3 scripts/native/band-closeup-capture.py \
      --member guitar --out /tmp/bc/off --tag off
  # compare matched (shot, frame) pairs (manifest.json carries songMs for the guard):
  for s in coop_g_cg coop_g_n01; do for i in 0 1 2; do
    scripts/analysis/visual_diff.py /tmp/bc/on/on_${s}_${i}.png \
        /tmp/bc/off/off_${s}_${i}.png --json; done; done

Exit code: 0 = PASS (pinned N/N, no new band drops, no band-ratio regression);
1 = FAIL (lost the pin, or a band drop/ratio regression); 2 = ERROR (hook missing
-> force_shot returned 0/int; bad shot name; no venue; never reached gameplay).
"""
import argparse, json, os, re, signal, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import importlib.util
spec = importlib.util.spec_from_file_location(
    "kbd2game", os.path.join(HERE, "keyboard-to-gameplay.py"))
k = importlib.util.module_from_spec(spec)
spec.loader.exec_module(k)

REPO = k.REPO

# Per-member default closeup candidate lists. Names are sent both bare and with a
# `.shot` suffix; whichever resolves (force_shot ok) is used. These are the REAL
# runtime BandCamShot names found in the club-family boot venue (probe §2's
# `coop_dir_*_cls` are dir-cut SELECTOR objects that do NOT resolve via Find).
#   guitar: coop_g_cg* = close-on-guitar; coop_g_n0* = numbered guitarist framings
#   bass:   coop_b_cg* / coop_b_n0*
#   drums:  coop_d_n0* (kit/drummer framings — no `_cls`/`_cg`)
#   keys:   coop_k_n0*
#   vocals: coop_v_n0* / coop_front_n0*
MEMBER_SHOTS = {
    "guitar": ["coop_g_cg", "coop_g_cg01", "coop_g_n01", "coop_g_n03", "coop_g_b"],
    "bass":   ["coop_b_cg", "coop_b_cg01", "coop_b_n01", "coop_b_n03"],
    "drums":  ["coop_d_n01", "coop_d_n02", "coop_d_n03"],
    "keys":   ["coop_k_n01", "coop_k_n02"],
    "vocals": ["coop_v_n01", "coop_front_n01", "coop_front_n00"],
}


# ---------------------------------------------------------------------------
# Pin verbs (the §1 native hook). dta() returns the parsed JSON value: a STRING
# for force_shot/cur_shot, an INT for director_disable. A bare int 0 from
# force_shot means the hook is MISSING (func not registered).
# ---------------------------------------------------------------------------
def force_shot(port, name):
    """Try the name as given, then with a `.shot` suffix. Returns (resolved_name,
    response) where resolved_name is the suffix variant that returned
    'force_shot ok', or (None, last_response) if neither resolved."""
    candidates = [name]
    if not name.endswith(".shot"):
        candidates.append(name + ".shot")
    last = None
    for cand in candidates:
        r = k.dta(port, '{rb3_force_shot "%s"}' % cand)
        last = r
        if r == "force_shot ok":
            return cand, r
        # A bare int (0) means the hook is not registered -> hard error, stop.
        if isinstance(r, int):
            return None, r
    return None, last


def director_disable(port, val):
    return k.dta(port, "{rb3_director_disable %d}" % (1 if val else 0))


def cur_shot(port):
    return k.dta(port, "{rb3_cur_shot}")


def advance_to_songms(port, target_ms, max_wait=8.0):
    """Autohit + poll until songMs >= target_ms (deterministic clock gate, NOT a
    wall-clock sleep). Returns the songMs actually reached."""
    dl = time.time() + max_wait
    while time.time() < dl:
        h = k.health(port)
        if h and h[1] >= target_ms:
            return h[1]
        k.verb(port, "autohit")
        time.sleep(0.12)
    h = k.health(port)
    return h[1] if h else -1.0


# ---------------------------------------------------------------------------
# Binary engine-log parse: [SHARD_GUARD] drops + [SHARD_RATIO] ratios.
# The log is BINARY (NUL bytes) — read bytes + replace-decode, never plain grep.
# ---------------------------------------------------------------------------
# [SHARD_GUARD] drop line shape (Rnd_Wgpu_RB3.cpp): we tolerate field-order drift
# by pulling mesh name + the band/other class token + the ratio with loose regex.
_DROP_RE = re.compile(r"\[SHARD_GUARD\].*?(?:mesh[=:]\s*|drop[^\w]*)?([\w.]+\.mesh)?")
_RATIO_RE = re.compile(r"\[SHARD_RATIO\]")


def parse_shard_log(log_path):
    """Return a dict: drops_total/drops_band/drops_other, per-mesh drop counts,
    max_band_ratio, closest_band_to_cap. Resilient to field-order drift."""
    out = {"drops_total": 0, "drops_band": 0, "drops_other": 0,
           "drop_meshes": {}, "max_band_ratio": 0.0,
           "closest_band_to_cap": "", "ratio_lines": 0}
    try:
        raw = open(log_path, "rb").read()
    except Exception:
        return out
    txt = raw.decode("utf-8", "replace")
    band_cap = 4.0  # relaxed band ratio cap (Rnd_Wgpu_RB3.cpp); for margin reporting
    closest_margin = 1e9
    lines = txt.splitlines()

    # IMPORTANT: the [SHARD_GUARD] DROP line (Rnd_Wgpu_RB3.cpp:5134) does NOT carry
    # a band/other class token — only the [SHARD_RATIO] line does (the trailing
    # `band`/`other` word, format at :5116). So a drop's class cannot be read off
    # the drop line itself. PASS 1: harvest the set of mesh names the engine ever
    # classified `band` from the RATIO lines; PASS 2: classify each drop by name
    # lookup in that set. Without this, drops_band is ALWAYS 0 even when a real
    # band mesh (e.g. lowtopsneaks_skin.2.mesh) is dropped — a false PASS on the
    # `drops_band==0` convergence gate.  (harness-verify fix)
    band_meshes = set()
    for line in lines:
        if "[SHARD_RATIO]" not in line:
            continue
        is_band = re.search(r"class[=:]\s*band", line) or (
            re.search(r"\bband\b", line) and not re.search(r"\bother\b", line))
        if is_band:
            nm = re.search(r"([\w.]+\.mesh)", line)
            if nm:
                band_meshes.add(nm.group(1))

    for line in lines:
        if "[SHARD_GUARD]" in line:
            out["drops_total"] += 1
            mm = re.search(r"([\w.]+\.mesh)", line)
            mesh = mm.group(1) if mm else None
            # class field: prefer an explicit `class=band`/`class=other` token if
            # the drop line ever grows one; else a bare band/other word; else fall
            # back to the RATIO-derived band-mesh set (the real case today).
            cm = re.search(r"class[=:]\s*(\w+)", line)
            if cm:
                cls = cm.group(1)
            elif re.search(r"\bband\b", line) and not re.search(r"\bother\b", line):
                cls = "band"
            elif re.search(r"\bother\b", line):
                cls = "other"
            elif mesh and mesh in band_meshes:
                cls = "band"
            else:
                cls = "other"
            if cls == "band":
                out["drops_band"] += 1
            else:
                out["drops_other"] += 1
            if mesh:
                out["drop_meshes"][mesh] = out["drop_meshes"].get(mesh, 0) + 1
        elif "[SHARD_RATIO]" in line:
            out["ratio_lines"] += 1
            # band-classified ratio lines only, for the safety-margin metric.
            if re.search(r"class[=:]\s*band", line) or (re.search(r"\bband\b", line)
                                                        and not re.search(r"\bother\b", line)):
                rm = re.search(r"ratio[=:]\s*([\d.]+)", line)
                if rm:
                    r = float(rm.group(1))
                    if r > out["max_band_ratio"]:
                        out["max_band_ratio"] = r
                    nm = re.search(r"([\w.]+\.mesh)", line)
                    margin = band_cap - r
                    if nm and 0 <= margin < closest_margin:
                        closest_margin = margin
                        out["closest_band_to_cap"] = "%s:%.2f" % (nm.group(1), r)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--bin", default=k.DEFAULT_BIN)
    ap.add_argument("--data", default=k.DEFAULT_DATA)
    ap.add_argument("--overlay", default=os.path.join(REPO, "native", "dta"))
    ap.add_argument("--diff", default="hard", choices=list(k.DIFF_INDEX))
    ap.add_argument("--song-downs", type=int, default=4,
                    help="DDown presses in song_select (selects which song -> venue)")
    ap.add_argument("--member", default="guitar",
                    choices=["guitar", "bass", "drums", "keys", "vocals", "all"],
                    help="which band member to frame (default guitar — most _cg/_cls shots)")
    ap.add_argument("--shots", default="",
                    help="comma-separated explicit shot list (overrides --member default)")
    ap.add_argument("--frames", type=int, default=2,
                    help="frames captured per shot for the A/B time series")
    ap.add_argument("--frame-dt", type=int, default=500,
                    help="deterministic song-clock advance (ms) between frames")
    ap.add_argument("--anchor-ms", type=float, default=-1.0,
                    help="capture ALL shots' frames at ABSOLUTE songMs targets "
                         "anchor-ms + frame_idx*frame_dt (NOT a per-boot-relative "
                         "t0). Required for cross-run/A-B pixel diffing: two passes "
                         "with the same --anchor-ms hit the same (shot, songMs) pair "
                         "-> same camera AND same char pose. Default -1 = relative "
                         "t0 (each shot starts at its own arrival clock).")
    ap.add_argument("--out", default="")
    ap.add_argument("--tag", default="cap",
                    help="filename prefix; A/B passes use --tag on / off etc.")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print(f"FAIL: binary not found: {args.bin}"); return 2
    out = args.out or os.path.join("/tmp", "rb3-bandcloseup", args.tag)
    os.makedirs(out, exist_ok=True)

    # candidate shot list
    if args.shots:
        cand_shots = [s.strip() for s in args.shots.split(",") if s.strip()]
    elif args.member == "all":
        cand_shots = []
        for m in ("guitar", "bass", "drums", "keys", "vocals"):
            cand_shots += MEMBER_SHOTS[m]
    else:
        cand_shots = MEMBER_SHOTS[args.member]

    port = args.port or k.free_port()
    log_path = os.path.join("/tmp", f"rb3-bandcloseup-{args.tag}-{port}.log")
    logf = open(log_path, "w")
    # Inherit + propagate os.environ (A/B env toggles like SHARD_GUARD_OFF=1 are
    # passed in the ENVIRONMENT, not as flags) — exactly as crowd-shot-capture.py.
    env = dict(os.environ)
    env.update({"RB3_GAME": "1", "RB3_HTTP": "1", "RB3_HTTP_PORT": str(port),
                "MILO_HEADLESS": "1", "RB3_DATA": args.data,
                "RB3_DTA_OVERLAY": args.overlay, "RB3_INPUT_DEBUG": "1"})
    print(f"[bandcloseup] launching rb3-native (port {port}, tag={args.tag}); log -> {log_path}")
    print(f"[bandcloseup] env toggles: " + " ".join(
        f"{e}={env[e]}" for e in ("SHARD_GUARD_OFF", "SHARD_RATIO_DBG", "SHARD_DBG",
                                  "RB3_NO_INST_REBIND", "RB3_NO_SKEL_REBIND") if e in env) or "(none)")
    proc = subprocess.Popen([args.bin], env=env, stdout=logf, stderr=subprocess.STDOUT,
                            cwd=REPO, start_new_session=True)
    diff_idx = k.DIFF_INDEX[args.diff]
    rc = 2
    manifest = []        # per captured PNG: {shot, frame_idx, songMs, cur_shot, file}
    pinned_ok = 0
    pinned_total = 0
    forced_shots = []    # resolved (with-suffix) names actually pinned
    skipped = []         # not_found candidates
    try:
        # --- boot -> gameplay (reuse k's proven nav verbatim) ---------------
        if k.wait_screen(port, lambda s: True, 40, proc) is None:
            print("FAIL: HTTP server never came up"); return 2
        print("[bandcloseup] HTTP up")
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
        print("[bandcloseup] game_screen reached")
        k.verb(port, "nofail")
        # wait for the song clock to advance (proves it's truly playing)
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
        print(f"[bandcloseup] is_playing={is_playing} songMs={k.health(port)[1]:.0f} "
              f"live_shot={cur_shot(port)!r}")

        # --- pin + capture loop (§2) ----------------------------------------
        # Freeze the auto-pick FIRST (assert echo == 1), so no frame re-picks
        # over a forced shot.
        dz = director_disable(port, 1)
        print(f"[bandcloseup] director_disable(1) -> {dz!r}")
        if dz != 1:
            print("ERROR: rb3_director_disable did not echo 1 (hook missing?)"); return 2

        # With --anchor-ms, each (shot, frame) captures at an ABSOLUTE songMs
        # target so two independent boots hit the same (shot, songMs) pair (same
        # camera AND same char pose). Each shot reserves a contiguous clock window
        # of frames*frame_dt; spans are laid back-to-back from anchor_ms. The
        # clock never goes backwards, so anchor_ms must be ahead of the arrival
        # clock — we sanity-check that below.
        use_anchor = args.anchor_ms >= 0.0
        shot_span = args.frames * args.frame_dt
        if use_anchor:
            h_arr = k.health(port)
            arrival = h_arr[1] if h_arr else 0.0
            n_resolvable = len([s for s in cand_shots])  # upper bound
            if args.anchor_ms < arrival + 100:
                print(f"[bandcloseup] WARN: --anchor-ms {args.anchor_ms:.0f} <= "
                      f"arrival songMs {arrival:.0f}; the clock can't rewind. "
                      f"Pick an anchor a few seconds ahead (e.g. {arrival+3000:.0f}).")
        shot_idx = 0
        for shot in cand_shots:
            resolved, resp = force_shot(port, shot)
            if resolved is None:
                if isinstance(resp, int):
                    print(f"ERROR: rb3_force_shot returned int {resp} — hook MISSING")
                    return 2
                print(f"[bandcloseup]   skip {shot}: {resp}")
                skipped.append((shot, resp))
                continue
            forced_shots.append(resolved)
            base = resolved.replace(".shot", "")
            print(f"[bandcloseup] force {resolved} -> {resp}")
            # let the forced cam apply: ONE OnSelectCamera tick is enough, settle 3
            for _ in range(3):
                k.verb(port, "autohit"); time.sleep(0.15)
            cur = cur_shot(port)
            if cur != resolved:
                # determinism gate failed for this shot — the pin didn't take.
                print(f"[bandcloseup]   WARN: cur_shot={cur!r} != forced {resolved!r}")
            # capture frames at deterministic songMs offsets (§3)
            if use_anchor:
                t0 = args.anchor_ms + shot_idx * shot_span
            else:
                h0 = k.health(port)
                t0 = h0[1] if h0 else 0.0
            for i in range(args.frames):
                target = t0 + i * args.frame_dt
                got_ms = advance_to_songms(port, target)
                cur = cur_shot(port)
                pinned_total += 1
                if cur == resolved:
                    pinned_ok += 1
                p = os.path.join(out, f"{args.tag}_{base}_{i}.png")
                ok = k.screenshot(port, p)
                manifest.append({"shot": base, "shot_full": resolved, "frame_idx": i,
                                 "songMs": round(got_ms, 1), "cur_shot": cur,
                                 "pinned": (cur == resolved), "file": p, "ok": ok})
                if args.verbose:
                    print(f"[bandcloseup]     {base}[{i}] target={target:.0f} "
                          f"songMs={got_ms:.0f} cur_shot={cur!r} ok={ok}")
            shot_idx += 1

        # --- diagnostics + verdict (§5) -------------------------------------
        logf.flush()
        shard = parse_shard_log(log_path)

        # write manifest.json (§3)
        with open(os.path.join(out, "manifest.json"), "w") as mf:
            json.dump({"tag": args.tag, "member": args.member, "diff": args.diff,
                       "song_downs": args.song_downs, "frames": args.frames,
                       "frame_dt": args.frame_dt, "anchor_ms": args.anchor_ms,
                       "forced_shots": forced_shots,
                       "skipped": skipped, "frames_manifest": manifest,
                       "env": {e: env[e] for e in ("SHARD_GUARD_OFF", "SHARD_RATIO_DBG",
                               "SHARD_DBG", "RB3_NO_INST_REBIND", "RB3_NO_SKEL_REBIND")
                               if e in env}}, mf, indent=2)

        verdict = "PASS"
        # hard determinism gate: every captured frame must be pinned
        if pinned_total == 0 or pinned_ok != pinned_total:
            verdict = "FAIL"
        # band drop / band-ratio regression -> FAIL (convergence goal: drops_band==0)
        if shard["drops_band"] > 0:
            verdict = "FAIL"

        rc = 0 if verdict == "PASS" else 1

        # write verdict.json
        vj = {"verdict": verdict, "member": args.member, "shots": len(forced_shots),
              "frames": args.frames, "pinned": f"{pinned_ok}/{pinned_total}",
              "drops_total": shard["drops_total"], "drops_band": shard["drops_band"],
              "drops_other": shard["drops_other"], "drop_meshes": shard["drop_meshes"],
              "max_band_ratio": round(shard["max_band_ratio"], 2),
              "closest_band_to_cap": shard["closest_band_to_cap"],
              "ratio_lines": shard["ratio_lines"], "skipped": skipped,
              "log": log_path, "out": out}
        with open(os.path.join(out, "verdict.json"), "w") as vf:
            json.dump(vj, vf, indent=2)

        # one-line machine-readable verdict (LAST stdout line, like visual_diff.py)
        print(json.dumps(vj))
        print(
            f"BAND_CLOSEUP verdict={verdict} member={args.member} "
            f"shots={len(forced_shots)} frames={args.frames} "
            f"pinned={pinned_ok}/{pinned_total} "
            f"drops_total={shard['drops_total']} drops_band={shard['drops_band']} "
            f"drops_other={shard['drops_other']} "
            f"max_band_ratio={shard['max_band_ratio']:.2f} "
            f"closest_band_to_cap={shard['closest_band_to_cap'] or 'none'}"
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
        print(f"[bandcloseup] engine log: {log_path}")
        print(f"[bandcloseup] out: {out}")


if __name__ == "__main__":
    sys.exit(main())
