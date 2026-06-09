#!/usr/bin/env python3
"""
gen-synth-trace.py — emit a synthetic, schema-EXACT session-telemetry .jsonl.

This is the cross-agent fixture for the Session Telemetry pipeline (design:
docs/native/SESSION_TELEMETRY_DESIGN.md). It produces a single NDJSON session
that exercises every v1 event kind so the report tool (trace-report.py) and the
server-side ingest can be developed/tested without a real engine run. It MUST
stay faithful to §4 (wire schema) + the Locked v1 contract; do NOT invent fields.

Wire contract emitted here (Locked v1):
  - Line 1 is the `hdr` (k="hdr", no t/f/sm). Every line — incl. hdr — carries
    `cs` (client_seq): a monotonic per-session uint64 the recorder stamps on each
    emitted line (the idempotency + ordering key; D3 PK is (sid,cs)).
  - Common envelope on every NON-hdr line: t (monotonic ms, 1dp), f (frame idx),
    sm (song ms, 1dp — EMITTED ONLY WHEN >= 0; omitted entirely in menus), k.
  - Per-kind fields (beyond envelope):
      boot : ph
      fr   : dt, lp, lpu, scr, ld, st, pend          (decimated; see §4.7)
      in   : pad, b, dn, up, ax{wh,ti}               (edge-only; ax SPARSE: only
             non-zero axes, keys wh=whammy / ti=tilt, values = round(v*1000))
      nav  : from, to, focus, wb                     (wb = wentBack)
      song : ev(load/start/end), id, track, diff, score(end), pct(end)
      au   : under, frames
      log  : lvl(assert/warn/info), msg, src(optional)
      mark : tag, note(optional)
  - `#` comment lines are legal anywhere after the hdr (the parser skips them).

Usage:
  python3 scripts/telemetry/gen-synth-trace.py [-o OUT.jsonl] [--frames N]
          [--seed S] [--hdr-v V] [--comments]
  (default: write to stdout)
"""
import argparse, json, math, random, sys, time


# JoypadButton bit indices (mirror src/system/os/Joypad.h, kPad_* digital set).
# Kept here only so the synthetic `in` rows use real held-bitmask values; the
# report tool derives its bit->name table from the header directly.
BTN = {
    "L2": 0, "R2": 1, "L1": 2, "R1": 3, "Tri": 4, "Circle": 5, "X": 6,
    "Square": 7, "Select": 8, "Start": 11, "DUp": 12, "DRight": 13,
    "DDown": 14, "DLeft": 15,
}


def emit(rows, out):
    """Serialize a list of dict rows as NDJSON. None values are dropped so an
    omitted optional field never appears (matches the recorder's omit-when-absent
    rule, e.g. `sm` in menus)."""
    for r in rows:
        clean = {k: v for k, v in r.items() if v is not None}
        out.write(json.dumps(clean, separators=(",", ":"), sort_keys=False))
        out.write("\n")


def build(frames, seed, hdr_v, comments):
    rng = random.Random(seed)
    rows = []
    cs = 0                 # client_seq, stamped on EVERY line incl hdr
    t = 0.0                # monotonic ms
    f = 0                  # frame index

    def nextcs():
        nonlocal cs
        v = cs
        cs += 1
        return v

    # ---- hdr (line 1; no t/f/sm) ------------------------------------------
    started = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(1717900000))
    rows.append({
        "k": "hdr", "cs": nextcs(), "v": hdr_v,
        "sid": "synth%08x" % (seed & 0xFFFFFFFF),
        "platform": "native",
        "started": started,
        "build": {"wasm_sha": "0000000000000000", "git": "deadbee"},
        "asset_version": "synth-assets-1",
        "ua": "rb3-native/synthetic",
        "viewport": [1280, 720],
        "flags": {"trace": True, "synthetic": True},
    })

    if comments:
        # A real native tracer writes a leading '#' comment; the parser skips it.
        rows.append({"__comment__": "# rb3 synthetic session-telemetry fixture"})

    # ---- boot timeline (the 6 canonical BootMark phases) ------------------
    boot_phases = [
        ("fetch_start", 0.0), ("fetch_done", 120.0),
        ("engine_init_done", 480.0), ("gpu_ready", 760.0),
        ("appctor_start", 800.0), ("appctor_done", 5200.0),
    ]
    for ph, ms in boot_phases:
        t = ms
        rows.append({"t": round(t, 1), "f": 0, "k": "boot",
                     "cs": nextcs(), "ph": ph})

    # Boot done; the frame loop starts here.
    t = boot_phases[-1][1]

    # ---- initial nav into the main hub (menu => NO sm) --------------------
    def nav(frm, to, focus, wb):
        rows.append({"t": round(t, 1), "f": f, "k": "nav", "cs": nextcs(),
                     "from": frm, "to": to, "focus": focus, "wb": wb})

    def inputrow(b, dn, up, wh=0, ti=0):
        # ax is SPARSE: include only non-zero axes, fixed-point round(v*1000).
        ax = {}
        if wh:
            ax["wh"] = int(round(wh * 1000))
        if ti:
            ax["ti"] = int(round(ti * 1000))
        row = {"t": round(t, 1), "f": f, "k": "in", "cs": nextcs(),
               "pad": 0, "b": b, "dn": dn, "up": up}
        if ax:
            row["ax"] = ax
        rows.append(row)

    def logrow(lvl, msg, src=None):
        rows.append({"t": round(t, 1), "f": f, "k": "log", "cs": nextcs(),
                     "lvl": lvl, "msg": msg, "src": src})

    def frrow(dt, lp, lpu, scr, ld, st, pend, sm=None):
        row = {"t": round(t, 1), "f": f, "k": "fr", "cs": nextcs(),
               "dt": round(dt, 2), "lp": round(lp, 2), "lpu": round(lpu, 2),
               "scr": scr, "ld": ld, "st": st, "pend": pend}
        if sm is not None and sm >= 0:
            row["sm"] = round(sm, 1)
        rows.append(row)

    # State that drives the timeline.
    DECIMATE = 30        # baseline 1/N sample (§4.7 default)
    LONG_MS = 20.0       # long-frame "always emit" threshold (§4.7 default)
    screen = "main_hub"
    nav("", "main_hub", "play_now.btn", False)

    # Scripted phases: (screen, n_frames, in_song?, song_ms_start)
    # menus: sm omitted; gameplay: sm present and advancing.
    phases = [
        ("main_hub", 90, False, None),
        ("song_select_screen", 200, False, None),
        ("gameplay", frames, True, 0.0),
    ]
    # Scripted nav transitions + a few input edges + one song lifecycle.
    nav_at = {
        90: ("main_hub", "song_select_screen", "qp_quickplay.btn", False),
        290: ("song_select_screen", "gameplay", "", False),
    }
    held = 0
    song_started = False
    song_id = "spoonman"

    total_frames = sum(p[1] for p in phases)
    song_start_frame = phases[0][1] + phases[1][1]   # gameplay begins here
    song_ms = 0.0

    for p_scr, p_n, in_song, _ in phases:
        screen = p_scr
        for _ in range(p_n):
            # advance clock: mostly ~16.7ms (60fps) with occasional long frames
            base = 1000.0 / 60.0
            jitter = rng.uniform(-1.5, 1.5)
            # inject deterministic long frames on a few frames to exercise
            # attribution (lp / lpu / draw).
            ld = st = 0
            lp = lpu = 0.0
            dt = base + jitter

            if f in nav_at:
                frm, to, focus, wb = nav_at[f]
                # a transition frame is also a stutter, carrying loader work
                lpu = rng.uniform(40.0, 120.0)   # sync drain (PollUntil)
                ld = rng.randint(2, 6)
                st = 1 if to == "gameplay" else 0
                dt = base + lpu + rng.uniform(2.0, 8.0)
                nav(frm, to, focus, wb)

            # song lifecycle around the gameplay entry
            if in_song and not song_started:
                # song load -> start fired at gameplay entry frame
                rows.append({"t": round(t, 1), "f": f, "k": "song",
                             "cs": nextcs(), "ev": "load", "id": song_id,
                             "track": "guitar", "diff": "expert"})
                rows.append({"t": round(t, 1), "f": f, "k": "song",
                             "cs": nextcs(), "ev": "start", "id": song_id,
                             "track": "guitar", "diff": "expert"})
                song_started = True
                song_ms = 0.0

            # gameplay: occasional bg-load spike + a periodic in-song stutter
            if in_song:
                if f % 120 == 0 and f > song_start_frame:
                    lp = rng.uniform(8.0, 22.0)   # bg LoadMgr.Poll
                    ld = rng.randint(1, 3)
                    dt = base + lp + rng.uniform(0.5, 3.0)
                if f % 200 == 0 and f > song_start_frame:
                    # a draw/gpu stutter (no loader signal): dt-lp-lpu => draw
                    dt = base + rng.uniform(25.0, 45.0)

            sm = song_ms if (in_song and song_started) else None

            # input edges: a couple of menu presses + in-song whammy/strum
            if not in_song:
                if f == 30:
                    held |= 1 << BTN["Start"]
                    inputrow(held, 1 << BTN["Start"], 0)
                elif f == 33:
                    up = 1 << BTN["Start"]
                    held &= ~up
                    inputrow(held, 0, up)
                elif f == 120:
                    held |= 1 << BTN["DDown"]
                    inputrow(held, 1 << BTN["DDown"], 0)
                elif f == 123:
                    up = 1 << BTN["DDown"]
                    held &= ~up
                    inputrow(held, 0, up)
            else:
                # strum + whammy bursts during gameplay (edge-only)
                if (f - song_start_frame) % 45 == 0:
                    bit = 1 << BTN["R1"]
                    held |= bit
                    # whammy axis present (non-zero) -> sparse ax{wh}
                    inputrow(held, bit, 0, wh=rng.uniform(0.2, 0.95))
                elif (f - song_start_frame) % 45 == 3:
                    bit = 1 << BTN["R1"]
                    up = bit
                    held &= ~up
                    inputrow(held, 0, up, wh=rng.uniform(0.0, 0.3) or None)

            # a couple of log events (severity allowlist: warn/assert)
            if f == 150:
                logrow("warn", "preview stream underrun recovered",
                       "Synth.cpp:204")
            if in_song and f == song_start_frame + 60:
                logrow("warn", "loader budget exceeded this frame",
                       "Loader.cpp:144")

            # an audio underrun event during gameplay
            if in_song and f == song_start_frame + 90:
                rows.append({"t": round(t, 1), "f": f, "k": "au",
                             "cs": nextcs(), "under": 2, "frames": 7,
                             "sm": round(song_ms, 1)})

            # a user marker mid-song
            if in_song and f == song_start_frame + 30:
                rows.append({"t": round(t, 1), "f": f, "k": "mark",
                             "cs": nextcs(), "tag": "bug_repro",
                             "note": "highway flicker here",
                             "sm": round(song_ms, 1)})

            # ---- frame decimation (§4.7) ----
            #  1) always emit a long frame (dt > LONG_MS)
            #  2) always emit when ld>0 || st>0
            #  3) otherwise sample 1/N (f % DECIMATE == 0)
            if dt > LONG_MS or ld > 0 or st > 0 or (f % DECIMATE == 0):
                frrow(dt, lp, lpu, screen, ld, st,
                      pend=max(0, ld - rng.randint(0, ld) if ld else 0),
                      sm=sm)

            t += dt
            f += 1
            if in_song and song_started:
                song_ms += dt

        # song end at the close of the gameplay phase
        if in_song and song_started:
            rows.append({"t": round(t, 1), "f": f, "k": "song", "cs": nextcs(),
                         "ev": "end", "id": song_id, "track": "guitar",
                         "diff": "expert", "score": 184502, "pct": 0.97,
                         "sm": round(song_ms, 1)})
            # back out to results -> menu (wb=True on the back step)
            nav("gameplay", "song_select_screen", "qp_quickplay.btn", True)

    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", default="-",
                    help="output path (default '-' = stdout)")
    ap.add_argument("--frames", type=int, default=900,
                    help="number of gameplay frames (default 900 ~= 15s @60fps)")
    ap.add_argument("--seed", type=int, default=1337)
    ap.add_argument("--hdr-v", type=int, default=1, help="hdr.v schema version")
    ap.add_argument("--comments", action="store_true",
                    help="emit a leading '#' comment line (parser must skip it)")
    args = ap.parse_args()

    rows = build(args.frames, args.seed, args.hdr_v, args.comments)

    out = sys.stdout if args.out == "-" else open(args.out, "w")
    try:
        for r in rows:
            if "__comment__" in r:
                out.write(r["__comment__"] + "\n")
                continue
            clean = {k: v for k, v in r.items() if v is not None}
            out.write(json.dumps(clean, separators=(",", ":"), sort_keys=False))
            out.write("\n")
    finally:
        if out is not sys.stdout:
            out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
