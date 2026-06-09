#!/usr/bin/env python3
"""End-to-end test for the session-telemetry SQLite ingest (D3 / §5).

Starts a REAL native/web/server.py on a random high port, POSTs a synthetic
schema-exact trace (hdr + frames + inputs + nav + a DUPLICATED tail), GETs it
back, and asserts the §5 contract:

  - row counts (events / frames / inputs) match the unique lines POSTed,
  - idempotency: the duplicated tail is NOT double-inserted ((sid,client_seq) PK),
  - the frames/inputs hot tables are populated incl. ld/st (frames) + axes (inputs),
  - the sessions row is seeded from the hdr line,
  - chunked uploads append (the trace is POSTed as two separate chunks),
  - fetch-back reconstructs hdr + every event in client_seq order,
  - localhost gate + body-size guard behave.

The server needs native/web/build/ to exist (its main() guards on it); since the
worktree has no web build, we point it at a throwaway empty dir and a temp DB —
nothing here touches the real sessions.db. No C++ build required.

Run: python3 scripts/telemetry/test-ingest.py   (exit 0 = pass)
"""

import json
import os
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
SERVER_PY = os.path.join(REPO, "native", "web", "server.py")


def _free_port():
    """Grab a free high port by binding :0 then releasing it. Not 8421."""
    while True:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind(("127.0.0.1", 0))
        port = s.getsockname()[1]
        s.close()
        if port != 8421 and port >= 1024:
            return port


def _wait_health(port, proc, timeout=20.0):
    url = f"http://127.0.0.1:{port}/api/health"
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            out = proc.stdout.read().decode("utf-8", "replace") if proc.stdout else ""
            raise RuntimeError(f"server exited early (rc={proc.returncode}):\n{out}")
        try:
            with urllib.request.urlopen(url, timeout=1.0) as r:
                if r.status == 200:
                    return
        except (urllib.error.URLError, ConnectionError, OSError):
            time.sleep(0.15)
    raise RuntimeError("server did not become healthy in time")


def _post(port, sid, body_text, expect_status=200, headers=None):
    url = f"http://127.0.0.1:{port}/api/telemetry/{sid}"
    data = body_text.encode("utf-8")
    req = urllib.request.Request(url, data=data, method="POST")
    req.add_header("Content-Type", "application/x-ndjson")
    for k, v in (headers or {}).items():
        req.add_header(k, v)
    try:
        with urllib.request.urlopen(req, timeout=10.0) as r:
            status, payload = r.status, r.read()
    except urllib.error.HTTPError as e:
        status, payload = e.code, e.read()
    assert status == expect_status, (
        f"POST {sid}: expected {expect_status}, got {status}: {payload[:200]!r}"
    )
    return json.loads(payload) if payload else {}


def _get(port, sid, query="", expect_status=200):
    return _get_url(port, f"/api/telemetry/{sid}{query}")


def _get_url(port, path):
    url = f"http://127.0.0.1:{port}{path}"
    try:
        with urllib.request.urlopen(url, timeout=10.0) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


# --------------------------------------------------------------------------- #
# Synthetic schema-exact trace (matches §4.2 envelope + §4.3 per-kind fields).
# Every line carries the v1 client_seq (the idempotency / ordering key).
# --------------------------------------------------------------------------- #

SID = "testsid-0123456789ab"


def _build_trace():
    """Return (chunk1_lines, chunk2_lines, dup_tail_lines). `cs` (client_seq) is a
    single monotonic counter across the whole session (chunks + dup). The wire key
    is the terse `cs`, matching the C++ recorder + gen-synth-trace.py (§4.2)."""
    hdr = {
        "k": "hdr", "v": 1, "cs": 0, "sid": SID,
        "platform": "native", "started": "2026-06-09T12:00:00+00:00",
        "build": {"wasm_sha": "deadbeef", "git": "abc123"},
        "asset_version": "av-42", "ua": "test-harness/1.0",
        "viewport": [1280, 720], "flags": {"trace": True},
    }
    # A '#' comment line (parser must skip) + a blank line.
    comment = "# rb3 session trace v1"

    fr1 = {"t": 16.7, "f": 1, "k": "fr", "cs": 1,
           "dt": 16.7, "lp": 0.2, "lpu": 0.1, "scr": "main_hub",
           "ld": 0, "st": 0, "pend": 0}
    # A long frame with a loader add + stream open → exercises ld/st in frames.
    fr2 = {"t": 50.0, "f": 2, "k": "fr", "cs": 2,
           "dt": 33.3, "lp": 5.0, "lpu": 4.0, "scr": "song_select",
           "ld": 2, "st": 1, "pend": 3}
    nav = {"t": 60.0, "f": 3, "k": "nav", "cs": 3,
           "from": "main_hub", "to": "song_select", "wb": False,
           "focus": "play_button"}
    in1 = {"t": 70.0, "f": 4, "k": "in", "cs": 4,
           "pad": 0, "b": 16, "dn": 16, "up": 0}
    # Input with analog axes (whammy in the sparse ax{} object).
    in2 = {"t": 90.0, "f": 5, "k": "in", "cs": 5,
           "pad": 0, "b": 0, "dn": 0, "up": 16, "ax": {"wh": 500}}
    # Song lifecycle + an in-song frame carrying sm (song ms).
    song = {"t": 100.0, "f": 6, "k": "song", "cs": 6,
            "ev": "start", "id": "song123", "track": "guitar", "diff": "expert"}
    fr3 = {"t": 116.0, "f": 7, "sm": 1234.5, "k": "fr", "cs": 7,
           "dt": 16.0, "lp": 0.0, "lpu": 0.0, "scr": "gameplay",
           "ld": 0, "st": 0, "pend": 0}

    chunk1 = [json.dumps(hdr), comment, "", json.dumps(fr1), json.dumps(fr2)]
    chunk2 = [json.dumps(nav), json.dumps(in1), json.dumps(in2),
              json.dumps(song), json.dumps(fr3)]
    # Duplicated sendBeacon tail: re-send the last two lines of chunk2.
    dup_tail = [json.dumps(song), json.dumps(fr3)]
    return chunk1, chunk2, dup_tail


def main():
    tmpdir = tempfile.mkdtemp(prefix="rb3-telemetry-test-")
    build_dir = os.path.join(tmpdir, "build")
    os.makedirs(build_dir, exist_ok=True)  # server main() guards on BUILD_DIR
    db_path = os.path.join(tmpdir, "sessions.db")

    env = dict(os.environ)
    env["RB3_TELEMETRY_DB"] = db_path
    # server.py's main() guards on BUILD_DIR (native/web/build) existing, but the
    # worktree has no web build (gitignored). Seed an empty native/web/build only
    # if it is absent, and remove it again on teardown so nothing is left behind.
    web_build = os.path.join(REPO, "native", "web", "build")
    created_web_build = False
    if not os.path.isdir(web_build):
        os.makedirs(web_build, exist_ok=True)
        created_web_build = True

    # Launch with a fresh random high port; retry on the bind race (the
    # bind-:0-then-release pattern leaves a tiny window where another process can
    # grab the port). Not 8421.
    proc = None
    port = None
    last_err = None
    for _attempt in range(8):
        port = _free_port()
        proc = subprocess.Popen(
            [sys.executable, SERVER_PY, "--port", str(port),
             "--telemetry-db", db_path, "--no-encode"],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env,
        )
        try:
            _wait_health(port, proc)
            break
        except RuntimeError as e:
            last_err = e
            try:
                proc.terminate()
                proc.communicate(timeout=3)
            except Exception:
                proc.kill()
            proc = None
    if proc is None:
        if created_web_build:
            try:
                os.rmdir(web_build)
            except OSError:
                pass
        print(f"FAIL: server never came up: {last_err}")
        sys.exit(1)

    failures = []
    try:
        chunk1, chunk2, dup_tail = _build_trace()

        # --- chunked upload: two independent fragments append ---------------- #
        r1 = _post(port, SID, "\n".join(chunk1) + "\n")
        assert r1["ok"] is True, r1
        # chunk1 has hdr + 2 fr = 2 storable events (hdr is not an event row).
        assert r1["ingested"] == 2, f"chunk1 ingested={r1['ingested']} (want 2)"
        assert r1["dup"] == 0, f"chunk1 dup={r1['dup']}"

        r2 = _post(port, SID, "\n".join(chunk2) + "\n")
        # chunk2 = nav + in + in + song + fr = 5 events.
        assert r2["ingested"] == 5, f"chunk2 ingested={r2['ingested']} (want 5)"
        assert r2["dup"] == 0, f"chunk2 dup={r2['dup']}"

        # --- idempotency: the duplicated tail must NOT double-insert ---------- #
        r3 = _post(port, SID, "\n".join(dup_tail) + "\n")
        assert r3["ingested"] == 0, f"dup tail ingested={r3['ingested']} (want 0)"
        assert r3["dup"] == 2, f"dup tail dup={r3['dup']} (want 2)"

        # client_seq_hi should be 7 (highest client_seq stored).
        assert r3["client_seq_hi"] == 7, f"client_seq_hi={r3['client_seq_hi']}"

        # --- verify the DB directly (counts + hot tables + sessions) --------- #
        conn = sqlite3.connect(db_path)
        conn.row_factory = sqlite3.Row

        ev_count = conn.execute(
            "SELECT COUNT(*) FROM events WHERE sid=?", (SID,)).fetchone()[0]
        assert ev_count == 7, f"events count={ev_count} (want 7 unique)"

        fr_count = conn.execute(
            "SELECT COUNT(*) FROM frames WHERE sid=?", (SID,)).fetchone()[0]
        assert fr_count == 3, f"frames count={fr_count} (want 3)"

        in_count = conn.execute(
            "SELECT COUNT(*) FROM inputs WHERE sid=?", (SID,)).fetchone()[0]
        assert in_count == 2, f"inputs count={in_count} (want 2)"

        # frames hot table carries ld/st (the asset-correlation columns).
        frame2 = conn.execute(
            "SELECT * FROM frames WHERE sid=? AND frame=2", (SID,)).fetchone()
        assert frame2 is not None, "frame 2 missing from frames hot table"
        assert frame2["ld"] == 2, f"frame2 ld={frame2['ld']} (want 2)"
        assert frame2["st"] == 1, f"frame2 st={frame2['st']} (want 1)"
        assert frame2["dt"] == 33.3, f"frame2 dt={frame2['dt']}"
        assert frame2["scr"] == "song_select", f"frame2 scr={frame2['scr']}"
        assert frame2["pend"] == 3, f"frame2 pend={frame2['pend']}"

        # in-song frame carries song_ms.
        frame7 = conn.execute(
            "SELECT * FROM frames WHERE sid=? AND frame=7", (SID,)).fetchone()
        assert frame7["song_ms"] == 1234.5, f"frame7 song_ms={frame7['song_ms']}"

        # inputs hot table carries the parsed bits + the axes JSON.
        in_axes = conn.execute(
            "SELECT axes FROM inputs WHERE sid=? AND frame=5", (SID,)).fetchone()
        assert in_axes is not None and in_axes["axes"], "input axes not stored"
        assert json.loads(in_axes["axes"]) == {"wh": 500}, in_axes["axes"]
        in_nofocus = conn.execute(
            "SELECT axes FROM inputs WHERE sid=? AND frame=4", (SID,)).fetchone()
        assert in_nofocus["axes"] is None, "axes should be NULL when absent"

        # sessions row seeded from the hdr line.
        srow = conn.execute(
            "SELECT * FROM sessions WHERE sid=?", (SID,)).fetchone()
        assert srow is not None, "sessions row missing"
        assert srow["platform"] == "native", srow["platform"]
        assert srow["build_sha"] == "deadbeef", srow["build_sha"]
        assert srow["asset_version"] == "av-42", srow["asset_version"]
        assert srow["ua"] == "test-harness/1.0", srow["ua"]
        assert srow["viewport_w"] == 1280 and srow["viewport_h"] == 720
        assert srow["started_utc"] == "2026-06-09T12:00:00+00:00"
        assert json.loads(srow["flags_json"]) == {"trace": True}
        assert srow["client_seq_hi"] == 7, srow["client_seq_hi"]
        assert srow["last_frame"] == 7, srow["last_frame"]
        # bytes_total accumulates across the three POSTs (chunked append).
        assert srow["bytes_total"] > 0, "bytes_total not accumulated"

        # schema_version present.
        ver = conn.execute("SELECT version FROM schema_version").fetchone()[0]
        assert ver == 1, f"schema_version={ver}"
        conn.close()

        # --- fetch-back: hdr + 7 events in client_seq order ------------------ #
        status, body = _get(port, SID)
        assert status == 200, f"GET status={status}"
        lines = [ln for ln in body.decode("utf-8").splitlines() if ln.strip()]
        assert len(lines) == 8, f"fetch-back lines={len(lines)} (want 1 hdr + 7)"
        first = json.loads(lines[0])
        assert first["k"] == "hdr" and first["sid"] == SID, first
        assert first["platform"] == "native", first
        seqs = [json.loads(ln)["cs"] for ln in lines[1:]]
        assert seqs == [1, 2, 3, 4, 5, 6, 7], f"order={seqs}"
        # an in-song fr line round-trips sm + ld/st via payload.
        fr7 = json.loads(lines[-1])
        assert fr7["k"] == "fr" and fr7["sm"] == 1234.5 and fr7["scr"] == "gameplay"

        # --- summary view (?format=json) ------------------------------------ #
        status, body = _get(port, SID, "?format=json")
        assert status == 200, f"summary status={status}"
        summary = json.loads(body)
        assert summary["event_count"] == 7, summary
        assert summary["kind_counts"].get("fr") == 3, summary["kind_counts"]

        # --- body-size guard ------------------------------------------------- #
        big = "x" * (9 * 1024 * 1024)  # > 8 MiB cap
        _post(port, SID, big, expect_status=413)

        # --- empty body → 400 ----------------------------------------------- #
        _post(port, SID, "", expect_status=400)

        # --- malformed-only body → 400 (no parseable lines) ----------------- #
        _post(port, SID, "{not json\nalso bad\n", expect_status=400)

        # --- invalid sid (path-traversal charset) → 400 --------------------- #
        status, _ = _get(port, "..%2Fetc", expect_status=400)
        assert status == 400, f"invalid sid GET status={status}"

        # --- unknown session GET → 404 -------------------------------------- #
        status, _ = _get(port, "neverseen-sid")
        assert status == 404, f"unknown sid GET status={status}"

        # --- session list includes our sid ---------------------------------- #
        status, body = _get_url(port, "/api/telemetry")
        assert status == 200, f"list status={status}"
        listing = json.loads(body)
        assert any(s["sid"] == SID for s in listing["sessions"]), listing

        # --- CROSS-AGENT CHECK: ingest the real gen-synth-trace.py output ----- #
        # gen-synth-trace.py is the sibling agent's schema-exact fixture; ingest
        # its emitted bytes verbatim to prove the wire contract (cs/wb/sparse-ax,
        # boot/song/log/mark kinds) round-trips through this server, not just our
        # hand-built trace. If the generator's hdr sid differs, we POST to it.
        gen = os.path.join(REPO, "scripts", "telemetry", "gen-synth-trace.py")
        if os.path.isfile(gen):
            out = subprocess.run(
                [sys.executable, gen, "--frames", "60", "--comments"],
                capture_output=True, text=True,
            )
            assert out.returncode == 0, f"gen-synth-trace failed: {out.stderr[:300]}"
            synth = out.stdout
            hdr0 = next(json.loads(ln) for ln in synth.splitlines()
                        if ln.strip() and not ln.startswith("#"))
            gen_sid = hdr0["sid"]
            non_comment = [ln for ln in synth.splitlines()
                           if ln.strip() and not ln.startswith("#")]
            r = _post(port, gen_sid, synth)
            assert r["ok"] is True, r
            # every non-comment line carries a unique cs → all ingest, hdr excluded.
            assert r["ingested"] == len(non_comment) - 1, (
                f"synth ingested={r['ingested']} of {len(non_comment) - 1} events"
            )
            assert r["dup"] == 0, f"synth dup={r['dup']}"
            # re-POST the whole thing → fully idempotent (0 new, all dup).
            r2 = _post(port, gen_sid, synth)
            assert r2["ingested"] == 0, f"synth re-POST ingested={r2['ingested']}"
            assert r2["dup"] == len(non_comment) - 1, f"synth re-POST dup={r2['dup']}"
            # fetch-back the synth session: hdr + every event, in cs order.
            status, gbody = _get(port, gen_sid)
            assert status == 200, f"synth GET status={status}"
            glines = [ln for ln in gbody.decode().splitlines() if ln.strip()]
            assert len(glines) == len(non_comment), (
                f"synth fetch-back lines={len(glines)} (want {len(non_comment)})"
            )
            gseqs = [json.loads(ln).get("cs") for ln in glines[1:]]
            assert gseqs == sorted(gseqs), "synth fetch-back not in cs order"
            print(f"  cross-agent: ingested gen-synth-trace ({len(non_comment)} "
                  f"lines incl hdr) idempotently + round-tripped")

        print("PASS: all telemetry ingest assertions held")
        print(f"  events=7 frames=3 inputs=2; idempotent dup tail; "
              f"hot tables incl ld/st/axes; sessions row from hdr")
    except AssertionError as e:
        failures.append(str(e))
    except Exception as e:  # noqa: BLE001
        failures.append(f"{type(e).__name__}: {e}")
    finally:
        proc.terminate()
        try:
            out, _ = proc.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            out, _ = proc.communicate()
        if created_web_build:
            try:
                os.rmdir(web_build)
            except OSError:
                pass
        if failures:
            print("FAIL:")
            for f in failures:
                print(f"  - {f}")
            if out:
                print("--- server output (tail) ---")
                print(out.decode("utf-8", "replace")[-2000:])

    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
