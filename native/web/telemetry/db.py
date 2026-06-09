#!/usr/bin/env python3
"""Session-telemetry SQLite store + single-writer ingest (D3, SESSION_TELEMETRY_DESIGN.md §5).

This module is the *source of truth* for ingested traces. It owns:
  - the schema (§5.2) + a version-gated migration runner (§5.6),
  - a single dedicated writer thread fed by a queue (§5.4 — the chosen model),
    so writes are serialized under ``ThreadingHTTPServer`` while GET readers use
    their own short-lived read-only WAL connections,
  - NDJSON ingest with ``(sid, client_seq)`` idempotency (the recorder stamps a
    monotonic per-session ``client_seq`` on every line; a duplicated ``sendBeacon``
    tail re-POSTs the same lines → ``INSERT OR IGNORE`` makes re-ingest a no-op),
  - byte-faithful fetch-back reconstruction from ``events`` (§5.5 — no raw blob).

It is intentionally free of any HTTP concern so ``trace-report.py`` (D7) can import
it directly. ``server.py`` wires the HTTP routes around ``ingest_ndjson`` /
``fetch_*`` and starts/stops the writer thread.

Durability (Locked v1 contract): ``synchronous=NORMAL`` + WAL + ``busy_timeout`` —
correct for a dev/playtest tool; a host crash loses at most the last unflushed tail.
"""

import json
import os
import queue
import sqlite3
import threading

# DB lives next to this module: native/web/telemetry/sessions.db
TELEMETRY_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_DB_PATH = os.path.join(TELEMETRY_DIR, "sessions.db")

# §5.6 — the SQLite schema version is independent of the wire hdr.v.
CURRENT_SCHEMA_VERSION = 1

# Bound the writer queue so a slow disk applies backpressure (the POST handler
# turns a full queue into a 503) instead of growing memory without limit.
WRITER_QUEUE_MAXSIZE = 256

_SCHEMA_V1 = """
CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL);

CREATE TABLE IF NOT EXISTS sessions (
  sid            TEXT PRIMARY KEY,
  started_utc    TEXT, platform TEXT, build_sha TEXT, asset_version TEXT,
  ua TEXT, viewport_w INTEGER, viewport_h INTEGER, flags_json TEXT,
  ended_utc      TEXT, last_frame INTEGER,
  client_seq_hi  INTEGER DEFAULT -1,
  bytes_total    INTEGER DEFAULT 0,
  first_seen_utc TEXT, last_seen_utc TEXT
);

-- Canonical event log. (sid, client_seq) is the idempotency key.
CREATE TABLE IF NOT EXISTS events (
  sid TEXT NOT NULL, client_seq INTEGER NOT NULL,
  t_ms REAL, frame INTEGER, song_ms REAL, kind TEXT,
  payload TEXT,
  PRIMARY KEY (sid, client_seq)
) WITHOUT ROWID;

-- Denormalized hot tables, populated in the SAME txn from kind='fr'/'in'.
-- Keyed on client_seq too so they're equally idempotent (frame is NOT unique:
-- decimated frames can repeat, inputs are multi-per-frame).
CREATE TABLE IF NOT EXISTS frames (
  sid TEXT, client_seq INTEGER, frame INTEGER, dt REAL,
  lp REAL, lpu REAL, scr TEXT, pend INTEGER,
  ld INTEGER, st INTEGER, t_ms REAL, song_ms REAL,
  PRIMARY KEY (sid, client_seq)
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS inputs (
  sid TEXT, client_seq INTEGER, frame INTEGER, t_ms REAL,
  pad INTEGER, bits INTEGER, dn INTEGER, up INTEGER, axes TEXT,
  PRIMARY KEY (sid, client_seq)
) WITHOUT ROWID;

CREATE INDEX IF NOT EXISTS ix_frames_sf ON frames(sid, frame);
CREATE INDEX IF NOT EXISTS ix_inputs_sf ON inputs(sid, frame);
CREATE INDEX IF NOT EXISTS ix_events_sk ON events(sid, kind);
"""


def _connect(db_path, *, read_only=False):
    """Open a connection with the §5.4 PRAGMAs. Read-only connections still need
    WAL set on the file (a no-op if the writer already enabled it); they get a
    query_only guard so a stray write can't corrupt under the single-writer model."""
    conn = sqlite3.connect(db_path, timeout=5.0)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=NORMAL")
    conn.execute("PRAGMA busy_timeout=5000")
    if read_only:
        conn.execute("PRAGMA query_only=ON")
    return conn


def ensure_schema(conn):
    """Idempotent schema create + version gate (§5.6). Runs only under the writer
    thread's single connection, so there is no migration race."""
    conn.executescript(_SCHEMA_V1)
    row = conn.execute("SELECT version FROM schema_version LIMIT 1").fetchone()
    if row is None:
        conn.execute(
            "INSERT INTO schema_version (version) VALUES (?)",
            (CURRENT_SCHEMA_VERSION,),
        )
        conn.commit()
        return
    version = row[0]
    if version < CURRENT_SCHEMA_VERSION:
        _run_migrations(conn, version, CURRENT_SCHEMA_VERSION)


def _run_migrations(conn, from_version, to_version):
    """Ordered migrations from_version -> to_version. v1 is the base schema; later
    column adds append a step here and bump CURRENT_SCHEMA_VERSION."""
    # _MIGRATIONS[v] upgrades a v-DB to v+1. None today (v1 is the base).
    _MIGRATIONS = {}
    for v in range(from_version, to_version):
        step = _MIGRATIONS.get(v)
        if step is not None:
            step(conn)
    conn.execute("UPDATE schema_version SET version = ?", (to_version,))
    conn.commit()


# --------------------------------------------------------------------------- #
# NDJSON parsing + row extraction (pure; no DB, no HTTP — unit-testable)
# --------------------------------------------------------------------------- #

def parse_ndjson_lines(text):
    """Yield parsed JSON objects from an NDJSON fragment.

    - '#'-comment lines and blank lines are skipped (matches the C++ tracer,
      rb3_frame_trace.cpp:68).
    - A malformed line is skipped (the caller counts it) — never abort the whole
      upload for one bad line.
    - The final line may be truncated (a chunk boundary mid-line); a trailing
      line that does not parse is simply dropped — the next chunk re-sends it
      because client_seq is contiguous.

    Returns (objects, malformed_count). Unknown keys are preserved verbatim in the
    object (callers tolerate them; the payload is stored as-is).
    """
    objects = []
    malformed = 0
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        try:
            obj = json.loads(line)
        except (ValueError, json.JSONDecodeError):
            malformed += 1
            continue
        if isinstance(obj, dict):
            objects.append(obj)
        else:
            malformed += 1
    return objects, malformed


def _session_row_from_hdr(sid, hdr):
    """Map a parsed hdr object → the sessions upsert tuple. Tolerant of missing
    keys (defaults to NULL/None). The route <sid> wins over any hdr.sid so the PK,
    the route, and the row always agree."""
    viewport = hdr.get("viewport") or [None, None]
    vw = viewport[0] if len(viewport) > 0 else None
    vh = viewport[1] if len(viewport) > 1 else None
    build = hdr.get("build") or {}
    build_sha = None
    if isinstance(build, dict):
        build_sha = build.get("wasm_sha") or build.get("git")
    elif isinstance(build, str):
        build_sha = build
    flags = hdr.get("flags")
    flags_json = json.dumps(flags) if flags is not None else None
    return {
        "sid": sid,
        "started_utc": hdr.get("started"),
        "platform": hdr.get("platform"),
        "build_sha": build_sha,
        "asset_version": hdr.get("asset_version"),
        "ua": hdr.get("ua"),
        "viewport_w": vw,
        "viewport_h": vh,
        "flags_json": flags_json,
    }


def _client_seq_of(obj):
    """The v1 monotonic per-session sequence number. The wire key is `cs` (the
    terse envelope convention matching t/f/sm/k, as emitted by the C++ recorder +
    gen-synth-trace.py); `client_seq` is accepted as a long-form fallback. The
    SQLite *column* is `client_seq` regardless (§5.2 — D7 queries it by name)."""
    cs = obj.get("cs")
    if cs is None:
        cs = obj.get("client_seq")
    return cs


# Envelope keys stripped from the stored per-kind payload (both the short `cs`
# wire key and the long-form fallback so neither leaks into payload).
_ENVELOPE_KEYS = ("t", "f", "sm", "k", "cs", "client_seq")


def _event_rows_from_obj(sid, obj):
    """Map one parsed non-hdr event object → (events_row, frames_row|None,
    inputs_row|None). Returns None if the object has no client_seq (the v1 PK /
    idempotency key — an event without it cannot be stored deterministically)."""
    client_seq = _client_seq_of(obj)
    if client_seq is None:
        return None
    kind = obj.get("k")
    t_ms = obj.get("t")
    frame = obj.get("f")
    song_ms = obj.get("sm")  # omitted in menus per §4.5

    # payload = the kind-specific object, verbatim (envelope keys stripped so the
    # stored payload is just the per-kind fields, mirroring §5.2's "payload" note).
    payload = {
        k: v for k, v in obj.items()
        if k not in _ENVELOPE_KEYS
    }
    events_row = (
        sid, client_seq, t_ms, frame, song_ms, kind, json.dumps(payload),
    )

    frames_row = None
    inputs_row = None
    if kind == "fr":
        frames_row = (
            sid, client_seq, frame,
            payload.get("dt"), payload.get("lp"), payload.get("lpu"),
            payload.get("scr"), payload.get("pend"),
            payload.get("ld"), payload.get("st"),
            t_ms, song_ms,
        )
    elif kind == "in":
        ax = payload.get("ax")
        axes_json = json.dumps(ax) if ax is not None else None
        inputs_row = (
            sid, client_seq, frame, t_ms,
            payload.get("pad"), payload.get("b"),
            payload.get("dn"), payload.get("up"), axes_json,
        )
    return events_row, frames_row, inputs_row


# --------------------------------------------------------------------------- #
# Writer thread — the single owner of the write connection (§5.4, chosen model)
# --------------------------------------------------------------------------- #

class _WriteJob:
    __slots__ = ("sid", "objects", "byte_len", "done", "result", "error")

    def __init__(self, sid, objects, byte_len):
        self.sid = sid
        self.objects = objects
        self.byte_len = byte_len
        self.done = threading.Event()
        self.result = None
        self.error = None


class TelemetryStore:
    """Owns the writer thread + queue. Construct once per process; call
    ``start()`` before serving and ``stop()`` on shutdown. ``ingest_ndjson`` is
    called from request threads and blocks for the writer's ack (so the HTTP
    response can report ingested/dup counts)."""

    def __init__(self, db_path=DEFAULT_DB_PATH, queue_maxsize=WRITER_QUEUE_MAXSIZE):
        self.db_path = db_path
        self._queue = queue.Queue(maxsize=queue_maxsize)
        self._thread = None
        self._stop = object()  # sentinel

    # -- lifecycle ---------------------------------------------------------- #
    def start(self):
        if self._thread is not None:
            return
        os.makedirs(os.path.dirname(self.db_path) or ".", exist_ok=True)
        # Apply the schema synchronously on start (under no contention) so the
        # very first request can read a session list even before any write.
        conn = _connect(self.db_path)
        try:
            ensure_schema(conn)
        finally:
            conn.close()
        self._thread = threading.Thread(
            target=self._writer_loop, name="telemetry-writer", daemon=True
        )
        self._thread.start()

    def stop(self):
        if self._thread is None:
            return
        self._queue.put(self._stop)
        self._thread.join(timeout=10.0)
        self._thread = None

    # -- request-thread API ------------------------------------------------- #
    def ingest_ndjson(self, sid, body_text, byte_len=None):
        """Parse an NDJSON fragment + enqueue it for the writer. Blocks for the
        ack. Returns a dict: ingested, dup, malformed, client_seq_hi, is_hdr.
        Raises ``queue.Full`` if the writer is saturated (caller → 503)."""
        objects, malformed = parse_ndjson_lines(body_text)
        if byte_len is None:
            byte_len = len(body_text.encode("utf-8", "replace"))
        job = _WriteJob(sid, objects, byte_len)
        # Non-blocking put: a full queue means the writer is behind → surface
        # backpressure rather than stalling the request thread indefinitely.
        self._queue.put(job, timeout=5.0)
        job.done.wait()
        if job.error is not None:
            raise job.error
        result = dict(job.result)
        result["malformed"] = malformed
        return result

    # -- reader API (own short-lived read-only WAL connection per call) ------ #
    def session_summary(self, sid):
        conn = _connect(self.db_path, read_only=True)
        try:
            row = conn.execute(
                "SELECT * FROM sessions WHERE sid = ?", (sid,)
            ).fetchone()
            if row is None:
                return None
            counts = {}
            for kind, n in conn.execute(
                "SELECT kind, COUNT(*) FROM events WHERE sid = ? GROUP BY kind",
                (sid,),
            ):
                counts[kind] = n
            total = conn.execute(
                "SELECT COUNT(*) FROM events WHERE sid = ?", (sid,)
            ).fetchone()[0]
            summary = {k: row[k] for k in row.keys()}
            summary["event_count"] = total
            summary["kind_counts"] = counts
            return summary
        finally:
            conn.close()

    def list_sessions(self):
        conn = _connect(self.db_path, read_only=True)
        try:
            rows = conn.execute(
                "SELECT sid, started_utc, platform, last_frame, bytes_total, "
                "first_seen_utc, last_seen_utc FROM sessions "
                "ORDER BY first_seen_utc DESC"
            ).fetchall()
            return [{k: r[k] for k in r.keys()} for r in rows]
        finally:
            conn.close()

    def iter_ndjson(self, sid):
        """Reconstruct the byte-faithful JSONL stream (§5.5): hdr from sessions,
        then events ordered by client_seq. Yields str lines (no trailing newline).
        Returns/raises nothing if the session is absent — yields nothing."""
        conn = _connect(self.db_path, read_only=True)
        try:
            srow = conn.execute(
                "SELECT * FROM sessions WHERE sid = ?", (sid,)
            ).fetchone()
            if srow is None:
                return
            yield json.dumps(_hdr_from_session_row(srow))
            for crow in conn.execute(
                "SELECT client_seq, t_ms, frame, song_ms, kind, payload "
                "FROM events WHERE sid = ? ORDER BY client_seq", (sid,),
            ):
                obj = {}
                # Envelope first (matches the wire ordering t,f,sm,k), then the
                # per-kind payload fields, then client_seq.
                if crow["t_ms"] is not None:
                    obj["t"] = crow["t_ms"]
                if crow["frame"] is not None:
                    obj["f"] = crow["frame"]
                if crow["song_ms"] is not None:
                    obj["sm"] = crow["song_ms"]
                if crow["kind"] is not None:
                    obj["k"] = crow["kind"]
                payload = json.loads(crow["payload"]) if crow["payload"] else {}
                obj.update(payload)
                # Emit the terse wire key `cs` (matches the producer + the
                # gen-synth-trace fixture); the column is client_seq internally.
                obj["cs"] = crow["client_seq"]
                yield json.dumps(obj)
        finally:
            conn.close()

    # -- writer thread ------------------------------------------------------ #
    def _writer_loop(self):
        conn = _connect(self.db_path)
        try:
            ensure_schema(conn)
            while True:
                job = self._queue.get()
                if job is self._stop:
                    break
                try:
                    job.result = self._apply(conn, job)
                except Exception as exc:  # noqa: BLE001 — surface to request thread
                    job.error = exc
                    try:
                        conn.rollback()
                    except sqlite3.Error:
                        pass
                finally:
                    job.done.set()
        finally:
            conn.close()

    def _apply(self, conn, job):
        """One DB transaction per POST. INSERT OR IGNORE → idempotent on the PK."""
        sid = job.sid
        ingested = 0
        dup = 0
        max_seq = None
        max_frame = None
        ended_utc = None
        hdr_obj = None

        cur = conn.cursor()
        cur.execute("BEGIN")
        for obj in job.objects:
            kind = obj.get("k")
            if kind == "hdr":
                hdr_obj = obj
                continue
            rows = _event_rows_from_obj(sid, obj)
            if rows is None:
                continue  # no client_seq → not storable
            events_row, frames_row, inputs_row = rows
            cur.execute(
                "INSERT OR IGNORE INTO events "
                "(sid, client_seq, t_ms, frame, song_ms, kind, payload) "
                "VALUES (?,?,?,?,?,?,?)",
                events_row,
            )
            if cur.rowcount == 0:
                dup += 1
                continue
            ingested += 1
            client_seq = events_row[1]
            if max_seq is None or client_seq > max_seq:
                max_seq = client_seq
            frame = events_row[3]
            if frame is not None and (max_frame is None or frame > max_frame):
                max_frame = frame
            if frames_row is not None:
                cur.execute(
                    "INSERT OR IGNORE INTO frames "
                    "(sid, client_seq, frame, dt, lp, lpu, scr, pend, ld, st, "
                    "t_ms, song_ms) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)",
                    frames_row,
                )
            if inputs_row is not None:
                cur.execute(
                    "INSERT OR IGNORE INTO inputs "
                    "(sid, client_seq, frame, t_ms, pad, bits, dn, up, axes) "
                    "VALUES (?,?,?,?,?,?,?,?,?)",
                    inputs_row,
                )
            if kind == "song" and obj.get("ev") == "end":
                # Mark session end with a wall-clock UTC stamp (the column is
                # *_utc; the wire only has a monotonic t). Server receipt time is
                # close enough for a dev tool and keeps the column type honest.
                ended_utc = _utcnow_iso()

        # Upsert the sessions row. The hdr (first POST) seeds identity columns;
        # every POST advances the bookkeeping columns. The route <sid> is the PK.
        self._upsert_session(cur, sid, hdr_obj, job.byte_len,
                             max_seq, max_frame, ended_utc)
        conn.commit()

        # client_seq_hi reflects the stored high-water mark after this txn.
        hi = cur.execute(
            "SELECT client_seq_hi FROM sessions WHERE sid = ?", (sid,)
        ).fetchone()
        client_seq_hi = hi[0] if hi is not None else -1
        return {
            "sid": sid,
            "ingested": ingested,
            "dup": dup,
            "client_seq_hi": client_seq_hi,
            "is_hdr": hdr_obj is not None,
        }

    @staticmethod
    def _upsert_session(cur, sid, hdr_obj, byte_len, max_seq, max_frame, ended_utc):
        existing = cur.execute(
            "SELECT sid FROM sessions WHERE sid = ?", (sid,)
        ).fetchone()
        now = _utcnow_iso()
        if existing is None:
            base = (_session_row_from_hdr(sid, hdr_obj)
                    if hdr_obj is not None else {"sid": sid})
            cur.execute(
                "INSERT INTO sessions "
                "(sid, started_utc, platform, build_sha, asset_version, ua, "
                " viewport_w, viewport_h, flags_json, ended_utc, last_frame, "
                " client_seq_hi, bytes_total, first_seen_utc, last_seen_utc) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (
                    sid,
                    base.get("started_utc"), base.get("platform"),
                    base.get("build_sha"), base.get("asset_version"),
                    base.get("ua"), base.get("viewport_w"),
                    base.get("viewport_h"), base.get("flags_json"),
                    ended_utc, max_frame,
                    max_seq if max_seq is not None else -1,
                    byte_len, now, now,
                ),
            )
            return
        # Existing row: a later hdr re-POST refreshes identity columns; the
        # bookkeeping columns advance monotonically (max of old/new).
        sets = [
            "bytes_total = bytes_total + ?",
            "last_seen_utc = ?",
        ]
        params = [byte_len, now]
        if max_seq is not None:
            sets.append("client_seq_hi = MAX(client_seq_hi, ?)")
            params.append(max_seq)
        if max_frame is not None:
            sets.append("last_frame = MAX(COALESCE(last_frame, -1), ?)")
            params.append(max_frame)
        if ended_utc is not None:
            sets.append("ended_utc = ?")
            params.append(ended_utc)
        if hdr_obj is not None:
            base = _session_row_from_hdr(sid, hdr_obj)
            for col in ("started_utc", "platform", "build_sha", "asset_version",
                        "ua", "viewport_w", "viewport_h", "flags_json"):
                sets.append(f"{col} = COALESCE(?, {col})")
                params.append(base.get(col))
        params.append(sid)
        cur.execute(f"UPDATE sessions SET {', '.join(sets)} WHERE sid = ?", params)


def _hdr_from_session_row(srow):
    """Reconstruct a hdr line object from a stored sessions row (fetch-back).
    The producer stamps the hdr with cs=0 (first line of the session)."""
    hdr = {"k": "hdr", "cs": 0, "v": CURRENT_SCHEMA_VERSION, "sid": srow["sid"]}
    if srow["platform"] is not None:
        hdr["platform"] = srow["platform"]
    if srow["started_utc"] is not None:
        hdr["started"] = srow["started_utc"]
    build = {}
    if srow["build_sha"] is not None:
        build["wasm_sha"] = srow["build_sha"]
    if build:
        hdr["build"] = build
    if srow["asset_version"] is not None:
        hdr["asset_version"] = srow["asset_version"]
    if srow["ua"] is not None:
        hdr["ua"] = srow["ua"]
    if srow["viewport_w"] is not None or srow["viewport_h"] is not None:
        hdr["viewport"] = [srow["viewport_w"], srow["viewport_h"]]
    if srow["flags_json"]:
        try:
            hdr["flags"] = json.loads(srow["flags_json"])
        except (ValueError, json.JSONDecodeError):
            pass
    return hdr


def _utcnow_iso():
    # Imported lazily so importing the module stays dependency-light.
    from datetime import datetime, timezone
    return datetime.now(timezone.utc).isoformat()
