#!/usr/bin/env python3
"""RB3 Web Port — Development Server with Asset Streaming API

Serves WASM build artifacts + streams game assets via HTTP API on localhost:8421.
(DC3's server runs on 8420; the disjoint port lets both run side-by-side.)
Sends required COOP/COEP headers for SharedArrayBuffer (future threading).

API endpoints:
  GET /api/health           — liveness probe (used by smoke-test waitForServer)
  GET /api/version          — asset version tag for IDB cache invalidation
  GET /api/manifest         — JSON list of all available assets
  GET /api/bundle           — single binary bundle of all .dta/.dtb (boot path)
  GET /api/bundle/boot      — binary bundle of the boot-critical .milo_xbox set
                              (R3: native/web/boot-assets.manifest)
  GET /api/file/<path>      — raw bytes of an extracted asset file
  GET /                     — index.html (build artifacts)
  GET /rb3-web.{js,wasm}    — WASM build output
"""

import argparse
import gzip as gzip_mod
import http.server
import json
import os
import queue
import random
import shutil
import subprocess
import sys
import urllib.parse

# Session-telemetry SQLite store (D3 / SESSION_TELEMETRY_DESIGN.md §5). Importing
# the sibling package: server.py lives in native/web/, telemetry/ is next to it.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from telemetry import db as telemetry_db  # noqa: E402

PORT = 8421
BUILD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build")

# --- Session telemetry ingest config (§5.3) --------------------------------- #
# The shared store handle (writer thread + queue). Created + started in main().
TELEMETRY_STORE = None
# Per-request body cap (§5.3). sendBeacon / fetch-keepalive flushes are small
# (D5 batches ~64 KB coalesced); 8 MiB is generous and bounds memory.
TELEMETRY_MAX_BODY = 8 * 1024 * 1024
# Capture scope — local now, remote-ready (Locked v1 contract). The telemetry
# routes enforce a localhost client check by default even though the server
# binds 0.0.0.0; --telemetry-bind-any (default OFF) opens them to remote
# playtesters, at which point a shared secret header is required.
TELEMETRY_BIND_ANY = False
# Shared secret required ONLY when TELEMETRY_BIND_ANY is on (remote capture).
# Localhost requests never need it. Set via --telemetry-secret / env.
TELEMETRY_SECRET = None
_LOCALHOST_ADDRS = {"127.0.0.1", "::1", "::ffff:127.0.0.1", "localhost"}
# R3 — boot-assets bundle manifest. A newline-delimited, comment-tolerant list of
# server-relative .milo_xbox paths the App ctor reads before the first interactive
# screen (see docs/native/web-perf-roadmap/R3-boot-bundle-expansion.md). Served as
# one async binary bundle by /api/bundle/boot so those reads hit warm MEMFS instead
# of freezing the wasm thread on a synchronous XHR. Generated, not hand-authored —
# scripts/web/gen-boot-manifest.mjs derives it from a cold-boot netperf waterfall.
BOOT_MANIFEST_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "boot-assets.manifest"
)
ASSETS_DIR = None  # Set via --assets-dir, RB3_ASSETS env, or auto-detect
# DTA overlay dir (native/dta/). Mirrors the native disk overlay in
# native/src/native_file.cpp: when an asset path is shadowed by an overlay
# file (native/dta/<path>), serve THAT file instead of the extracted original
# in both /api/file and /api/bundle, so the web build's MEMFS receives the
# overlay copy (e.g. config/joypad.dta with its button_meanings block). The
# extracted assets stay pristine. See docs/native/DTA_OVERLAY_ENGINE.md.
OVERLAY_DIR = None  # Set via --overlay-dir, RB3_DTA_OVERLAY env, or auto-detect
# Optional fallback asset roots probed when a file is missing from ASSETS_DIR.
# W7-V4: album art (and other "long-tail" assets) live only in the full xbox
# extraction. The curated ASSETS_DIR is kept as the primary so on-disc smoke
# tests stay deterministic; the fallback fills the gap for art / chars / etc.
FALLBACK_ASSETS_DIRS = []
# Offline XMA->PCM sidecars (web-xma). These are content-hash-named flat
# <hex>.pcm files (~180MB total) that are NOT copied into the curated
# ASSETS_DIR or the wasm preload — they live in the gitignored derived tree
# and are served on demand here when the web build fetches one via
# /api/file/sfx/gen/xma_pcm/<hex>.pcm (lazily, one per distinct SFX that plays).
SIDECAR_DIR = None  # auto-detected (orig-assets/derived/sfx_pcm) or RB3_SFX_PCM_DIR

# R5 — on-demand wire compression for /api/file (see
# docs/native/web-perf-roadmap/R5-wire-compression.md). The big .milo_xbox
# assets are fetched via a *synchronous* XHR that freezes the wasm main thread
# for the whole transfer, so fewer wire bytes = proportionally less freeze.
# We compress compressible assets on demand with the brotli CLI (gzip fallback),
# cache the artifact to disk, and serve it via standard Content-Encoding
# negotiation — the browser decompresses before the engine's XHR sees the bytes,
# so this is fully transparent to the C++ engine (server.py-only change).
#
# Auto-detected at native/web/.cache/encoded/ (env RB3_ENCODE_CACHE,
# --encode-cache flag). Created on first use.
ENCODE_CACHE_DIR = None
ENCODE_ENABLED = True  # --no-encode disables (raw path, for A/B)
# Compressible asset extensions (scene-graph / DTA / object data). Conservative
# start with the measured offenders (.milo_xbox + the DTA family). Deny wins
# over allow. Already-compressed payloads (.mogg/.ogg/DXT textures/webm) gain
# ~0 and are explicitly denied — the mogg is R4's surface, never touched here.
COMPRESSIBLE_EXTS = {
    ".milo_xbox", ".milo", ".milo_ps3", ".milo_wii",
    ".dta", ".dtb", ".dtb_ps3",
}
INCOMPRESSIBLE_EXTS = {
    ".mogg", ".ogg", ".webm", ".pcm",
    ".png_xbox", ".bmp_xbox", ".jpg", ".jpeg", ".png", ".gz", ".br",
}
# brotli q5 keeps ~90% of q11's win at ~1/100th the CPU (colorpalettes 45.9% vs
# 40.3%, 0.23s vs 22.1s), and the cost is paid inline on the first (freezing)
# request — so q5 is the right on-demand default. q11 is reserved for the
# optional build-time pre-warm. gzip -6 is the fallback level.
ENCODE_LEVEL_BROTLI = 5
ENCODE_LEVEL_GZIP = 6
# Resolved encoder binaries (probed once in main()); None if absent.
BROTLI_BIN = None
GZIP_BIN = None


class RB3Handler(http.server.SimpleHTTPRequestHandler):
    """Serves static files from build/ with correct MIME types, security headers,
    and an asset streaming API backed by a pre-extracted game data directory."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=BUILD_DIR, **kwargs)

    def end_headers(self):
        # Required for SharedArrayBuffer (future pthreads support)
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        self.send_header("Cache-Control", self._cache_control_for_path())
        super().end_headers()

    def _cache_control_for_path(self):
        """Per-path HTTP cache policy. Centralized here so every response path
        (static, precompressed, asset API) gets a consistent header.

        The two deployable builds are served from distinct subdirs so caching can
        be decided purely from the request path (the client picks the subdir via
        the ?debug=true URL param — see native/web/index.html):

          /release/*  → long-lived immutable cache. index.html version-busts the
                        URL with ?v=<asset+wasm mtime> from /api/version, so a
                        rebuilt release wasm gets a new URL and the browser reuses
                        the cached + already-compiled wasm on every reload in
                        between. THIS is what makes reloads fast.
          /debug/*    → never cached. Fast-iteration build; always fresh.
          /api/version→ never cached, or the cache-bust token itself goes stale.
          everything else (index.html, audio-worklet.js, /api/bundle, manifest)
                      → revalidate each load so edits/rebuilds propagate.
        """
        path = urllib.parse.urlparse(self.path).path
        if path.startswith("/release/"):
            # Safe to cache hard: the URL is version-stamped, so a new build is a
            # new URL. 1 year + immutable = zero revalidation round-trips.
            return "public, max-age=31536000, immutable"
        if path.startswith("/debug/") or path == "/api/version":
            return "no-cache, no-store, must-revalidate"
        return "no-cache"

    def guess_type(self, path):
        if path.endswith(".wasm"):
            return "application/wasm"
        if path.endswith(".js"):
            return "application/javascript"
        if path.endswith(".dta"):
            return "text/plain"
        if path.endswith(".webm"):
            return "video/webm"
        return super().guess_type(path)

    def do_GET(self):
        if self.path.startswith("/api/"):
            self._handle_api()
            return
        if self.path == "/":
            self.path = "/index.html"
        if self._maybe_serve_precompressed():
            return
        super().do_GET()

    def do_HEAD(self):
        if self.path.startswith("/api/"):
            self._handle_api()
            return
        if self.path == "/":
            self.path = "/index.html"
        if self._maybe_serve_precompressed(head_only=True):
            return
        super().do_HEAD()

    def do_POST(self):
        # The first non-GET route on this server (§5.3): telemetry ingest. Any
        # other POST 404s (mirrors do_GET's /api/ short-circuit).
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        if path == "/api/telemetry/prune":
            self._handle_telemetry_prune()
            return
        if path.startswith("/api/telemetry/"):
            sid = path[len("/api/telemetry/"):]
            self._handle_telemetry_post(sid)
            return
        self._json_error(404, "Unknown API endpoint")

    def _maybe_serve_precompressed(self, head_only=False):
        """W4a — serve a pre-compressed .br or .gz next to the requested
        .wasm / .js when the client advertises support. Returns True if a
        response was sent (caller should bail out). The .br/.gz artifacts are
        generated by scripts/web/build.sh; if missing we fall through to the
        raw file (no on-the-fly compression — too slow at q11 per request)."""
        # Only short-circuit for the two big payloads. Everything else (API,
        # assets, etc.) keeps the existing identity path. Strip the ?query FIRST
        # — release URLs carry a ?v=<version> cache-bust token, so the suffix
        # check has to run against the path without it (else br/gz is skipped).
        url_path = urllib.parse.urlparse(self.path).path
        if not (url_path.endswith(".wasm") or url_path.endswith(".js")):
            return False
        rel = url_path.lstrip("/")
        base_path = os.path.join(BUILD_DIR, rel)
        accept = (self.headers.get("Accept-Encoding") or "").lower()
        candidates = []
        if "br" in accept:
            candidates.append((base_path + ".br", "br"))
        if "gzip" in accept:
            candidates.append((base_path + ".gz", "gzip"))
        for disk_path, enc in candidates:
            if os.path.isfile(disk_path):
                self._serve_encoded(disk_path, base_path, enc, head_only)
                return True
        return False

    def _serve_encoded(self, disk_path, base_path, encoding, head_only):
        """Serve a pre-compressed file with the matching Content-Encoding.
        Content-Type follows the *base* (.wasm / .js) so the browser parses
        it correctly after decompression."""
        size = os.path.getsize(disk_path)
        ctype = self.guess_type(base_path)
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Encoding", encoding)
        self.send_header("Content-Length", str(size))
        self.send_header("Vary", "Accept-Encoding")
        self.end_headers()
        if head_only:
            return
        with open(disk_path, "rb") as f:
            while True:
                chunk = f.read(65536)
                if not chunk:
                    break
                self.wfile.write(chunk)

    def _maybe_compressed_asset(self, full_path, safe_rel, head_only):
        """R5 — on-demand wire compression for /api/file. If `full_path` is a
        compressible asset and the client advertises br/gzip, compress it once
        into ENCODE_CACHE_DIR (atomic temp+rename) and serve the cached artifact
        with the matching Content-Encoding. Returns True if a response was sent
        (caller bails), False to fall through to the raw path.

        Transparent to the engine: the browser decompresses Content-Encoding
        before the wasm XHR/fetch sees the bytes, so the C++ side still receives
        the original uncompressed milo. See R5-wire-compression.md.

        Bails (→ raw path) for: compression disabled, a Range request (Range ⊕
        Content-Encoding is invalid), no encoder available, an ext not in the
        allowlist or in the deny set, or a client that advertised neither br nor
        gzip. The raw fallback guarantees no regression."""
        if not (ENCODE_ENABLED and ENCODE_CACHE_DIR):
            return False
        if self.headers.get("Range"):
            return False
        # Deny wins over allow. Lowercase the extension for the membership test.
        _root, ext = os.path.splitext(safe_rel)
        ext = ext.lower()
        if ext in INCOMPRESSIBLE_EXTS or ext not in COMPRESSIBLE_EXTS:
            return False

        accept = (self.headers.get("Accept-Encoding") or "").lower()
        # Prefer brotli over gzip; intersect preference with the client's
        # Accept-Encoding and with which encoder binaries actually resolved.
        if "br" in accept and BROTLI_BIN:
            enc, cache_ext = "br", ".br"
        elif "gzip" in accept and GZIP_BIN:
            enc, cache_ext = "gzip", ".gz"
        else:
            return False

        cache_path = os.path.join(ENCODE_CACHE_DIR, safe_rel + cache_ext)
        # mtime/size-keyed staleness: if the cached artifact predates the source
        # or the source changed, recompress. A sidecar .meta records (size,mtime)
        # of the source the artifact was built from; cheap stat-and-compare.
        if not self._encoded_cache_valid(cache_path, full_path):
            if not self._encode_to_cache(full_path, cache_path, enc):
                return False  # encode failed → fall through to raw
        # Final safety: a cache entry must exist and be non-stale now. If a
        # concurrent purge/race left it missing, fall through to raw rather than
        # 500.
        if not os.path.isfile(cache_path):
            return False

        self._serve_encoded(cache_path, full_path, enc, head_only)
        return True

    @staticmethod
    def _encoded_cache_valid(cache_path, src_path):
        """True if `cache_path` exists and was built from the current `src_path`
        (size + mtime match the sidecar .meta). Recompress on any drift."""
        if not os.path.isfile(cache_path):
            return False
        meta_path = cache_path + ".meta"
        try:
            st = os.stat(src_path)
            with open(meta_path, "r") as fh:
                meta = fh.read().strip()
            return meta == f"{st.st_size}:{int(st.st_mtime)}"
        except OSError:
            return False

    def _encode_to_cache(self, src_path, cache_path, enc):
        """Compress `src_path` into `cache_path` using the brotli/gzip CLI.

        The write is ATOMIC: compress to a unique temp file in the cache dir and
        os.rename() it onto the final path (atomic on the same filesystem). The
        server is a ThreadingHTTPServer, so two requests can miss the same asset
        at once; the rename guarantees a reader never sees a half-written body,
        and a duplicate compress just loses the rename race harmlessly (both
        temp files are complete; last writer wins). A sidecar .meta records the
        source (size,mtime) for staleness checks; it is written the same way.

        Returns True on success, False on any failure (caller falls through to
        the raw path — no regression)."""
        cache_dir = os.path.dirname(cache_path)
        try:
            os.makedirs(cache_dir, exist_ok=True)
        except OSError:
            return False

        tmp_suffix = f".{os.getpid()}.{random.randrange(1 << 32):08x}.tmp"
        tmp_path = cache_path + tmp_suffix
        try:
            if enc == "br":
                cmd = [BROTLI_BIN, "-q", str(ENCODE_LEVEL_BROTLI),
                       "-c", src_path]
            else:  # gzip
                cmd = [GZIP_BIN, f"-{ENCODE_LEVEL_GZIP}", "-c", src_path]
            with open(tmp_path, "wb") as out:
                proc = subprocess.run(cmd, stdout=out, stderr=subprocess.PIPE)
            if proc.returncode != 0:
                self._unlink_quiet(tmp_path)
                return False
            # Capture source identity BEFORE the rename, so the .meta describes
            # the exact bytes we compressed (not a racing rewrite of the source).
            st = os.stat(src_path)
            os.rename(tmp_path, cache_path)
        except OSError:
            self._unlink_quiet(tmp_path)
            return False

        # Best-effort sidecar .meta (also atomic). A missing/failed .meta just
        # forces a recompress next time — correct, only mildly wasteful.
        meta_path = cache_path + ".meta"
        meta_tmp = meta_path + tmp_suffix
        try:
            with open(meta_tmp, "w") as fh:
                fh.write(f"{st.st_size}:{int(st.st_mtime)}")
            os.rename(meta_tmp, meta_path)
        except OSError:
            self._unlink_quiet(meta_tmp)
        return True

    @staticmethod
    def _unlink_quiet(path):
        try:
            os.unlink(path)
        except OSError:
            pass

    def _handle_api(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path == "/api/health":
            self._serve_health()
        elif path == "/api/version":
            self._serve_version()
        elif path == "/api/manifest":
            self._serve_manifest()
        elif path == "/api/bundle/boot":
            self._serve_boot_bundle()
        elif path == "/api/bundle":
            self._serve_bundle()
        elif path == "/api/telemetry":
            self._handle_telemetry_list()
        elif path.startswith("/api/telemetry/"):
            sid = path[len("/api/telemetry/"):]
            self._handle_telemetry_get(sid, parsed)
        elif path.startswith("/api/file/"):
            rel = path[len("/api/file/"):]
            self._serve_asset_file(rel)
        else:
            self._json_error(404, "Unknown API endpoint")

    # --- Session telemetry ingest (§5.3) ---------------------------------- #

    def _drain_body(self, length):
        """Read and discard `length` bytes from the request body in chunks. Used
        when rejecting a POST (413) so the socket is left in a clean state and the
        client receives the status line instead of a reset connection."""
        remaining = length
        while remaining > 0:
            chunk = self.rfile.read(min(65536, remaining))
            if not chunk:
                break
            remaining -= len(chunk)

    def _telemetry_gate(self):
        """Enforce the §5.3 access policy: localhost-only by default; when
        --telemetry-bind-any is on, allow remote clients but require the shared
        secret header. Returns True if the request may proceed; on rejection it
        has already written the error response and returns False."""
        client = self.client_address[0] if self.client_address else ""
        is_local = client in _LOCALHOST_ADDRS
        if not TELEMETRY_BIND_ANY:
            if not is_local:
                self._json_error(403, "Telemetry is localhost-only")
                return False
            return True
        # Remote capture enabled. Localhost still goes through without a secret
        # (so local dev never needs the header); remote must present it.
        if is_local:
            return True
        if not TELEMETRY_SECRET:
            self._json_error(403, "Remote telemetry requires a configured secret")
            return False
        presented = self.headers.get("X-Telemetry-Secret")
        if not presented or presented != TELEMETRY_SECRET:
            self._json_error(403, "Bad or missing X-Telemetry-Secret")
            return False
        return True

    def _telemetry_sid_ok(self, sid):
        """Validate the route <sid>. The recorder mints it (clock + pid/counter)
        — keep it to a sane charset/length so it can't be a path-traversal or an
        unbounded key. Writes the error + returns False on rejection."""
        sid = urllib.parse.unquote(sid)
        if not sid or len(sid) > 128:
            self._json_error(400, "Missing or oversized sid")
            return None
        # alnum + the few separators a UUID-slice / native id uses.
        if not all(c.isalnum() or c in "-_.:" for c in sid):
            self._json_error(400, "Invalid sid")
            return None
        return sid

    def _handle_telemetry_post(self, sid):
        if TELEMETRY_STORE is None:
            self._json_error(503, "Telemetry store not initialized")
            return
        if not self._telemetry_gate():
            return
        sid = self._telemetry_sid_ok(sid)
        if sid is None:
            return

        # Body read + size guard (§5.3). Require Content-Length; reject over the
        # cap before reading the body so an oversized POST can't exhaust memory.
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except (TypeError, ValueError):
            self._json_error(400, "Bad Content-Length")
            return
        if length <= 0:
            self._json_error(400, "Empty body")
            return
        if length > TELEMETRY_MAX_BODY:
            # Drain (and discard) the oversized body before replying so the
            # client gets a clean 413 instead of a connection reset mid-write.
            self._drain_body(length)
            self._json_error(413, "Body exceeds telemetry max size")
            return
        raw = self.rfile.read(length)
        body_text = raw.decode("utf-8", "replace")

        try:
            result = TELEMETRY_STORE.ingest_ndjson(sid, body_text, byte_len=len(raw))
        except queue.Full:
            # Writer is saturated → backpressure (D5 client retries the chunk).
            self._json_error(503, "Telemetry writer busy; retry")
            return
        except Exception as exc:  # noqa: BLE001
            self._json_error(500, f"Ingest failed: {exc}")
            return

        if result["ingested"] == 0 and result["dup"] == 0 \
                and not result["is_hdr"] and result["malformed"] > 0:
            # Body had only unparseable lines → 400 (§5.3 last bullet).
            self._json_error(400, "No parseable telemetry lines")
            return

        body = json.dumps({
            "ok": True,
            "sid": sid,
            "ingested": result["ingested"],
            "dup": result["dup"],
            "malformed": result["malformed"],
            "client_seq_hi": result["client_seq_hi"],
        }).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _handle_telemetry_get(self, sid, parsed):
        if TELEMETRY_STORE is None:
            self._json_error(503, "Telemetry store not initialized")
            return
        if not self._telemetry_gate():
            return
        sid = self._telemetry_sid_ok(sid)
        if sid is None:
            return
        q = urllib.parse.parse_qs(parsed.query)
        # ?format=json → a single summary object (sessions row + counts) for D7.
        if q.get("format", [""])[0] == "json":
            summary = TELEMETRY_STORE.session_summary(sid)
            if summary is None:
                self._json_error(404, f"No such session: {sid}")
                return
            body = json.dumps(summary).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(body)
            return

        # Default: reconstruct the byte-faithful NDJSON stream (hdr + events in
        # client_seq order) for replay (D6) / trace-report.py (D7).
        summary = TELEMETRY_STORE.session_summary(sid)
        if summary is None:
            self._json_error(404, f"No such session: {sid}")
            return
        body = "\n".join(TELEMETRY_STORE.iter_ndjson(sid))
        if body:
            body += "\n"
        encoded = body.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/x-ndjson")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(encoded)

    def _handle_telemetry_list(self):
        if TELEMETRY_STORE is None:
            self._json_error(503, "Telemetry store not initialized")
            return
        if not self._telemetry_gate():
            return
        sessions = TELEMETRY_STORE.list_sessions()
        body = json.dumps({"sessions": sessions, "count": len(sessions)}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _handle_telemetry_prune(self):
        # Pruning is a write-side maintenance op; localhost-gated like the rest.
        if TELEMETRY_STORE is None:
            self._json_error(503, "Telemetry store not initialized")
            return
        if not self._telemetry_gate():
            return
        # prune is out of v1 scope for this handler; acknowledge as a no-op so the
        # route exists (the retention helper lands with the report tooling, D7).
        body = json.dumps({"ok": True, "pruned": 0, "note": "prune is a no-op in v1"}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _serve_health(self):
        body = json.dumps({"status": "ok"}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _serve_version(self):
        """Asset version tag for client-side IDB cache invalidation.

        Combines the assets-dir mtime and the WASM build mtime so the cache
        is dropped when either the asset set or the engine changes. The tag
        is opaque — clients only compare for equality.
        """
        parts = []
        if ASSETS_DIR and os.path.isdir(ASSETS_DIR):
            try:
                parts.append(str(int(os.path.getmtime(ASSETS_DIR))))
            except OSError:
                parts.append("0")
        else:
            parts.append("noassets")
        # Prefer the release wasm mtime (that's the cached, version-busted build);
        # fall back to debug, then the legacy flat path. Bumping the release wasm
        # changes this token → index.html appends a fresh ?v= → cache is busted.
        wasm_path = None
        for cand in ("release/rb3-web.wasm", "debug/rb3-web.wasm", "rb3-web.wasm"):
            p = os.path.join(BUILD_DIR, cand)
            if os.path.isfile(p):
                wasm_path = p
                break
        if wasm_path:
            try:
                parts.append(str(int(os.path.getmtime(wasm_path))))
            except OSError:
                parts.append("0")
        else:
            parts.append("nowasm")
        version = "-".join(parts)
        body = json.dumps({"version": version}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _serve_manifest(self):
        if not ASSETS_DIR:
            self._json_error(503, "No assets directory configured")
            return

        files = []
        for root, _dirs, filenames in os.walk(ASSETS_DIR):
            for f in filenames:
                full = os.path.join(root, f)
                rel = os.path.relpath(full, ASSETS_DIR)
                files.append({"path": rel, "size": os.path.getsize(full)})

        files.sort(key=lambda x: x["path"])
        body = json.dumps({"files": files, "count": len(files)}, indent=1).encode()

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    @staticmethod
    def _overlay_path(safe_rel):
        """Return the overlay file path for a relative asset path if an overlay
        shadows it, else None. The overlay is keyed on the archive-relative
        layout (e.g. "config/joypad.dta"); a key containing ".." would escape
        the overlay tree (the bundle restores "(..)" -> ".." for system/run
        files), so reject those to mirror the native ResolveOverlay() guard and
        keep the overlay strictly inside native/dta/."""
        if not OVERLAY_DIR or ".." in safe_rel.replace("\\", "/").split("/"):
            return None
        cand = os.path.join(OVERLAY_DIR, safe_rel)
        return cand if os.path.isfile(cand) else None

    def _emit_bundle(self, entries, cache_name=None):
        """Write `entries` ([(rel, bytes), ...]) as the binary bundle the engine's
        onBundleSuccess (milo-native-engine/src/platform/WebAssets.cpp) parses.

        Format: uint32 count, then for each file:
          uint32 path_len, path (UTF-8), uint32 data_len, data (bytes)
        Integers are little-endian (Emscripten host order).

        Shared by /api/bundle (the .dta/.dtb config bundle) and /api/bundle/boot
        (the R3 boot-milo bundle). Entries are emitted sorted by path for a stable
        body. HEAD sends headers only.

        R3<->R5 fix: when `cache_name` is given and the client accepts br/gzip, the
        bundle body is compressed on the wire (Content-Encoding) with a persistent
        disk cache. Without this the boot bundle shipped ~60 MB RAW — bypassing
        R5's per-file compression and putting the full uncompressed transfer on the
        cold-boot critical path. The browser decodes Content-Encoding before the
        engine's fetch sees the bytes, so onBundleSuccess still parses the raw
        bundle (transparent, same as /api/file)."""
        import struct
        import hashlib

        entries = sorted(entries, key=lambda x: x[0])
        chunks = [struct.pack("<I", len(entries))]
        for path, data in entries:
            path_bytes = path.encode("utf-8")
            chunks.append(struct.pack("<I", len(path_bytes)))
            chunks.append(path_bytes)
            chunks.append(struct.pack("<I", len(data)))
            chunks.append(data)

        body = b"".join(chunks)

        enc, cache_ext = self._pick_encoding()
        if enc and cache_name and self.command != "HEAD":
            # Fingerprint the entry set+sizes (cheap — no 60 MB hash); a changed
            # asset set / size rebuilds the cached artifact.
            fp = hashlib.sha1(
                (f"{len(entries)}|" + "|".join(
                    f"{r}:{len(d)}" for r, d in entries)).encode()
            ).hexdigest()[:16]
            comp = self._bundle_encoded(cache_name, fp, body, enc, cache_ext)
            if comp is not None:
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Encoding", enc)
                self.send_header("Content-Length", str(len(comp)))
                self.send_header("Vary", "Accept-Encoding")
                self.end_headers()
                self.wfile.write(comp)
                return

        # Raw fallback (HEAD, no encoder advertised, or encode failed).
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Vary", "Accept-Encoding")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _pick_encoding(self):
        """('br'|'gzip', ext) per Accept-Encoding + available encoders + the
        ENCODE_ENABLED toggle, else (None, None). Prefers brotli. Mirrors the
        negotiation in _maybe_compressed_asset for the in-memory bundle path."""
        if not (ENCODE_ENABLED and ENCODE_CACHE_DIR):
            return None, None
        accept = (self.headers.get("Accept-Encoding") or "").lower()
        if "br" in accept and BROTLI_BIN:
            return "br", ".br"
        if "gzip" in accept and GZIP_BIN:
            return "gzip", ".gz"
        return None, None

    def _bundle_encoded(self, cache_name, fp, body, enc, cache_ext):
        """Get-or-build the compressed bundle artifact, cached under
        ENCODE_CACHE_DIR/_bundles/<name>.<fp><ext> (the fingerprint is in the
        filename, so a changed asset set rebuilds). Compresses the in-memory body
        via the brotli/gzip CLI (stdin), atomic temp+rename. Returns the compressed
        bytes, or None on failure (caller serves raw — no regression)."""
        try:
            cache_dir = os.path.join(ENCODE_CACHE_DIR, "_bundles")
            os.makedirs(cache_dir, exist_ok=True)
        except OSError:
            return None
        cache_path = os.path.join(cache_dir, f"{cache_name}.{fp}{cache_ext}")
        if os.path.isfile(cache_path):
            try:
                with open(cache_path, "rb") as fh:
                    return fh.read()
            except OSError:
                pass  # fall through and rebuild
        tmp = cache_path + f".{os.getpid()}.{random.randrange(1 << 32):08x}.tmp"
        try:
            if enc == "br":
                cmd = [BROTLI_BIN, "-q", str(ENCODE_LEVEL_BROTLI), "-c"]
            else:  # gzip
                cmd = [GZIP_BIN, f"-{ENCODE_LEVEL_GZIP}", "-c"]
            proc = subprocess.run(cmd, input=body, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE)
            if proc.returncode != 0:
                return None
            comp = proc.stdout
            with open(tmp, "wb") as out:
                out.write(comp)
            os.rename(tmp, cache_path)  # atomic; duplicate builders race harmlessly
            return comp
        except OSError:
            self._unlink_quiet(tmp)
            return None

    def _serve_bundle(self):
        """All boot-path DTA/DTB files as a single binary bundle (see
        _emit_bundle for the format)."""
        if not ASSETS_DIR:
            self._json_error(503, "No assets directory configured")
            return

        # Ark extraction stores ".." as "(..)" in directory names — restore them
        # so the matched-fork's #include paths resolve.
        BUNDLE_EXTS = {".dta", ".dtb"}
        entries = []
        for root, _dirs, filenames in os.walk(ASSETS_DIR):
            for f in filenames:
                _name, ext = os.path.splitext(f)
                if ext.lower() not in BUNDLE_EXTS:
                    continue
                full = os.path.join(root, f)
                rel = os.path.relpath(full, ASSETS_DIR)
                rel = rel.replace("(..)", "..")
                # DTA overlay: if native/dta/<rel> exists, bundle THAT copy in
                # place of the extracted original (same relative key). This is
                # what gets the joypad.dta button_meanings block into the web
                # boot bundle (config/joypad.dta is a boot-path DTA).
                ov = self._overlay_path(rel)
                src = ov if ov else full
                with open(src, "rb") as fh:
                    data = fh.read()
                entries.append((rel, data))

        self._emit_bundle(entries, cache_name="config")

    def _resolve_asset_path(self, rel):
        """Resolve a server-relative asset path to an on-disk file, mirroring
        _serve_asset_file's resolution order: DTA overlay → curated ASSETS_DIR →
        the "(..)/(..)" system/run layout → the fallback asset roots. Returns the
        absolute path, or None if the asset is missing everywhere.

        Lifted out of _serve_asset_file so the boot-bundle path resolver shares
        the exact same logic (overlay shadowing, the ark "(..)" restore, the
        long-tail fallback roots) as the on-demand /api/file path."""
        # DTA overlay shadows the extracted original (boot manifest is milos, not
        # DTAs, but keep the parity so a future manifest entry resolves the same).
        full_path = self._overlay_path(rel) or os.path.join(ASSETS_DIR, rel)

        # Files under system/run/ live at (..)/(..)/system/run/ on disk.
        if not os.path.isfile(full_path) and rel.startswith("system/"):
            alt = os.path.join(ASSETS_DIR, "(..)", "(..)", rel)
            if os.path.isfile(alt):
                full_path = alt

        # Long-tail fallback roots (album art / chars / patchcreator milos that
        # ship only in the full xbox extraction, not the curated set).
        if not os.path.isfile(full_path):
            for fb_root in FALLBACK_ASSETS_DIRS:
                fb_path = os.path.join(fb_root, rel)
                if os.path.isfile(fb_path):
                    full_path = fb_path
                    break
                if rel.startswith("system/"):
                    fb_alt = os.path.join(fb_root, "(..)", "(..)", rel)
                    if os.path.isfile(fb_alt):
                        full_path = fb_alt
                        break

        return full_path if os.path.isfile(full_path) else None

    @staticmethod
    def _read_boot_manifest():
        """Read native/web/boot-assets.manifest → list of server-relative paths.
        Blank lines and '#'-comment lines are ignored. Returns [] if the manifest
        is absent (the boot bundle then emits an empty bundle — the client's sync
        path still serves every boot milo, just without the R3 prefetch win)."""
        paths = []
        try:
            with open(BOOT_MANIFEST_PATH, "r") as fh:
                for line in fh:
                    line = line.strip()
                    if not line or line.startswith("#"):
                        continue
                    paths.append(line)
        except OSError:
            pass
        return paths

    def _serve_boot_bundle(self):
        """R3 — the boot-critical .milo_xbox working set as one binary bundle.

        Reads native/web/boot-assets.manifest, resolves each path with the same
        overlay/fallback/"(..)" logic as /api/file, and emits the shared bundle
        format. A manifest entry that resolves to nothing is logged and SKIPPED
        (mirroring onBundleSuccess's skip-on-write-fail tolerance) so one missing
        asset never fails the whole boot bundle — the client's sync path is the
        backstop for anything not delivered here."""
        if not ASSETS_DIR:
            self._json_error(503, "No assets directory configured")
            return

        entries = []
        missing = 0
        for rel in self._read_boot_manifest():
            full = self._resolve_asset_path(rel)
            if not full:
                missing += 1
                self.log_message("boot-bundle: missing %s (skipped)", rel)
                continue
            with open(full, "rb") as fh:
                data = fh.read()
            entries.append((rel, data))

        total = sum(len(d) for _r, d in entries)
        self.log_message(
            "boot-bundle: %d files, %.1f MB%s",
            len(entries), total / 1e6,
            (f", {missing} missing" if missing else ""),
        )
        self._emit_bundle(entries, cache_name="boot")

    def _serve_asset_file(self, relpath):
        if not ASSETS_DIR:
            self._json_error(503, "No assets directory configured")
            return

        relpath = urllib.parse.unquote(relpath)
        safe = os.path.normpath(relpath)
        if safe.startswith("..") or os.path.isabs(safe):
            self._json_error(403, "Path traversal denied")
            return

        # DTA overlay: a native/dta/<path> file shadows the extracted original
        # for on-demand /api/file fetches too (the boot bundle covers the boot
        # path; this covers any lazily-fetched config DTA). Checked first.
        full_path = self._overlay_path(safe) or os.path.join(ASSETS_DIR, safe)

        # Ark extraction stores ".." as "(..)" in directory names.
        # Files under system/run/ live at (..)/(..)/system/run/ on disk.
        if not os.path.isfile(full_path) and safe.startswith("system/"):
            alt = os.path.join(ASSETS_DIR, "(..)", "(..)", safe)
            if os.path.isfile(alt):
                full_path = alt

        # W7-V4: long-tail asset fallback (album art, character textures, etc.)
        # The curated ASSETS_DIR doesn't ship per-song _keep.png_xbox files —
        # only the full xbox extraction does. Probe the configured fallback
        # roots before 404-ing so song_select can render real cover art.
        if not os.path.isfile(full_path):
            for fb_root in FALLBACK_ASSETS_DIRS:
                fb_path = os.path.join(fb_root, safe)
                if os.path.isfile(fb_path):
                    full_path = fb_path
                    break
                if safe.startswith("system/"):
                    fb_alt = os.path.join(fb_root, "(..)", "(..)", safe)
                    if os.path.isfile(fb_alt):
                        full_path = fb_alt
                        break

        # web-xma: offline XMA->PCM sidecars. They are served from the gitignored
        # derived tree (orig-assets/derived/sfx_pcm) on demand rather than copied
        # into ASSETS_DIR or the wasm preload. The runtime asks for them at
        # sfx/gen/xma_pcm/<hex>.pcm; the on-disk files are flat <hex>.pcm, so the
        # basename is the lookup key. normpath/`..` guard above already ran, so the
        # basename is safe. A fresh checkout without sidecars just falls through to
        # the 404 below (no regression).
        if not os.path.isfile(full_path) and SIDECAR_DIR and "xma_pcm/" in safe:
            sc_path = os.path.join(SIDECAR_DIR, os.path.basename(safe))
            if os.path.isfile(sc_path):
                full_path = sc_path

        if not os.path.isfile(full_path):
            self._json_error(404, f"Not found: {relpath}")
            return

        size = os.path.getsize(full_path)
        content_type = self.guess_type(full_path)
        head_only = self.command == "HEAD"

        range_hdr = self.headers.get("Range")
        if range_hdr and not head_only:
            self._serve_range(full_path, size, range_hdr, content_type)
            return

        # R5 — on-demand wire compression. Compress + cache compressible assets
        # (.milo_xbox etc.) and serve them via Content-Encoding negotiation when
        # the client supports it. Transparent to the engine (browser decodes
        # before the wasm XHR sees the bytes). Bails to the raw path below for
        # non-compressible exts, Range requests, identity-only clients, or if no
        # encoder is available — so there is no regression. `safe` is the
        # normpath'd archive-relative key (the cache is keyed on it).
        if self._maybe_compressed_asset(full_path, safe, head_only):
            return

        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(size))
        self.send_header("Accept-Ranges", "bytes")
        # Vary so shared/proxy caches don't hand a compressed body (served above
        # for br/gzip clients) to an identity-only client, and vice versa.
        self.send_header("Vary", "Accept-Encoding")
        self.end_headers()
        if not head_only:
            with open(full_path, "rb") as f:
                while True:
                    chunk = f.read(65536)
                    if not chunk:
                        break
                    self.wfile.write(chunk)

    def _serve_range(self, full_path, total_size, range_hdr, content_type="application/octet-stream"):
        try:
            ranges = range_hdr.replace("bytes=", "")
            start_str, end_str = ranges.split("-")
            start = int(start_str) if start_str else 0
            end = int(end_str) if end_str else total_size - 1
        except (ValueError, IndexError):
            self._json_error(416, "Invalid range")
            return

        if start >= total_size:
            self._json_error(416, "Range not satisfiable")
            return

        end = min(end, total_size - 1)
        length = end - start + 1

        self.send_response(206)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(length))
        self.send_header("Content-Range", f"bytes {start}-{end}/{total_size}")
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()
        with open(full_path, "rb") as f:
            f.seek(start)
            remaining = length
            while remaining > 0:
                chunk = f.read(min(65536, remaining))
                if not chunk:
                    break
                self.wfile.write(chunk)
                remaining -= len(chunk)

    def _json_error(self, code, msg):
        body = json.dumps({"error": msg}).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        # Quieter logging: skip 200s for static assets
        if len(args) >= 2 and str(args[1]) == "200" and not str(args[0]).startswith("GET /api"):
            return
        super().log_message(format, *args)


def _find_assets_dir():
    """Auto-detect extracted assets directory."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(script_dir, "../../orig-assets/extracted"),
        os.path.join(script_dir, "../../../orig-assets/extracted"),
    ]
    env = os.environ.get("RB3_ASSETS")
    if env:
        candidates.insert(0, env)
    for c in candidates:
        if os.path.isdir(c):
            return os.path.realpath(c)
    return None


def _find_fallback_dirs():
    """Auto-detect fallback asset directories (W7-V4 album art etc.).

    The full xbox extraction has per-song _keep.png_xbox cover art that the
    curated default `extracted/` dir strips. We probe it as a secondary root
    so song_select can render real album art without disturbing the primary
    dataset used by smoke tests."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(script_dir, "../../orig-assets/extracted-xbox-full"),
        os.path.join(script_dir, "../../../orig-assets/extracted-xbox-full"),
    ]
    env = os.environ.get("RB3_ASSETS_FALLBACK")
    if env:
        for p in env.split(os.pathsep):
            if p:
                candidates.insert(0, p)
    found = []
    seen = set()
    for c in candidates:
        if os.path.isdir(c):
            real = os.path.realpath(c)
            if real not in seen:
                seen.add(real)
                found.append(real)
    return found


def _find_overlay_dir():
    """Auto-detect the DTA overlay directory (native/dta/).

    The overlay holds small git-tracked DTA patches (e.g. config/joypad.dta
    with its button_meanings block) that shadow the extracted originals. The
    server prefers overlay copies in /api/bundle and /api/file. Overridable via
    RB3_DTA_OVERLAY; returns None gracefully if absent (no overlay = plain
    extracted assets, identical to pre-overlay behavior)."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        # server.py lives at native/web/, so the overlay is one dir up: native/dta
        os.path.join(script_dir, "..", "dta"),
        os.path.join(script_dir, "../../native/dta"),
    ]
    env = os.environ.get("RB3_DTA_OVERLAY")
    if env:
        candidates.insert(0, env)
    for c in candidates:
        if os.path.isdir(c):
            return os.path.realpath(c)
    return None


def _find_sidecar_dir():
    """Auto-detect the offline XMA->PCM sidecar directory (web-xma).

    The 180MB content-hash sidecars live in the gitignored derived tree
    (regenerated by scripts/assets/convert_xma_banks.sh). We serve them on
    demand rather than copying them into the asset set. Overridable via
    RB3_SFX_PCM_DIR; returns None gracefully if absent (then sidecar requests
    just 404, same as before the feature)."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(script_dir, "../../orig-assets/derived/sfx_pcm"),
        os.path.join(script_dir, "../../../orig-assets/derived/sfx_pcm"),
    ]
    env = os.environ.get("RB3_SFX_PCM_DIR")
    if env:
        candidates.insert(0, env)
    for c in candidates:
        if os.path.isdir(c):
            return os.path.realpath(c)
    return None


def _default_encode_cache_dir():
    """Default on-demand compression cache dir: native/web/.cache/encoded/.

    Gitignored derived state (like orig-assets/derived/); a fresh checkout
    starts cold and warms itself. Overridable via RB3_ENCODE_CACHE or
    --encode-cache. Returns the path (not created here — made on first use)."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    env = os.environ.get("RB3_ENCODE_CACHE")
    if env:
        return os.path.abspath(env)
    return os.path.join(script_dir, ".cache", "encoded")


def main():
    global ASSETS_DIR, FALLBACK_ASSETS_DIRS, SIDECAR_DIR, OVERLAY_DIR
    global ENCODE_CACHE_DIR, ENCODE_ENABLED, ENCODE_LEVEL_BROTLI
    global BROTLI_BIN, GZIP_BIN
    global TELEMETRY_STORE, TELEMETRY_BIND_ANY, TELEMETRY_SECRET

    parser = argparse.ArgumentParser(description="RB3 Web Dev Server")
    parser.add_argument(
        "--assets-dir",
        default=None,
        help="Path to extracted game assets (default: RB3_ASSETS env or auto-detect)",
    )
    parser.add_argument(
        "--assets-fallback",
        action="append",
        default=None,
        help=(
            "Additional asset directory probed when a file is missing from "
            "--assets-dir (repeatable; default: RB3_ASSETS_FALLBACK env or "
            "orig-assets/extracted-xbox-full for album art / long-tail assets)."
        ),
    )
    parser.add_argument(
        "--sidecar-dir",
        default=None,
        help=(
            "Directory of offline XMA->PCM sidecars served on demand for the web "
            "build (default: RB3_SFX_PCM_DIR env or "
            "orig-assets/derived/sfx_pcm). Served at /api/file/sfx/gen/xma_pcm/."
        ),
    )
    parser.add_argument(
        "--overlay-dir",
        default=None,
        help=(
            "DTA overlay directory whose files shadow the extracted assets in "
            "/api/bundle and /api/file (default: RB3_DTA_OVERLAY env or "
            "native/dta). Holds small git-tracked DTA patches (e.g. "
            "config/joypad.dta button_meanings)."
        ),
    )
    parser.add_argument(
        "--encode-cache",
        default=None,
        help=(
            "Cache dir for on-demand-compressed assets (R5 wire compression; "
            "default: RB3_ENCODE_CACHE env or native/web/.cache/encoded). "
            ".milo_xbox etc. are brotli/gzip-compressed once and served via "
            "Content-Encoding negotiation."
        ),
    )
    parser.add_argument(
        "--encode-level",
        type=int,
        default=None,
        help=(
            "brotli quality for on-demand compression (default 5 — keeps ~90%% "
            "of q11's win at ~1/100th the CPU, paid inline on the first fetch)."
        ),
    )
    parser.add_argument(
        "--no-encode",
        action="store_true",
        help="Disable on-demand wire compression (serve assets raw; for A/B).",
    )
    parser.add_argument(
        "--telemetry-db",
        default=None,
        help=(
            "Path to the session-telemetry SQLite store (default: "
            "RB3_TELEMETRY_DB env or native/web/telemetry/sessions.db)."
        ),
    )
    parser.add_argument(
        "--telemetry-bind-any",
        action="store_true",
        help=(
            "Open the /api/telemetry routes to non-localhost clients (remote "
            "playtest capture). OFF by default — telemetry is localhost-only. "
            "When on, a shared secret (--telemetry-secret) is required for "
            "remote requests."
        ),
    )
    parser.add_argument(
        "--telemetry-secret",
        default=None,
        help=(
            "Shared secret required in the X-Telemetry-Secret header for REMOTE "
            "telemetry requests when --telemetry-bind-any is set (default: "
            "RB3_TELEMETRY_SECRET env). Localhost never needs it."
        ),
    )
    parser.add_argument("--port", type=int, default=PORT)
    args = parser.parse_args()

    ASSETS_DIR = args.assets_dir or _find_assets_dir()
    OVERLAY_DIR = (
        os.path.realpath(args.overlay_dir)
        if args.overlay_dir and os.path.isdir(args.overlay_dir)
        else _find_overlay_dir()
    )
    if args.assets_fallback:
        FALLBACK_ASSETS_DIRS = [
            os.path.realpath(p) for p in args.assets_fallback if os.path.isdir(p)
        ]
    else:
        FALLBACK_ASSETS_DIRS = _find_fallback_dirs()
    SIDECAR_DIR = (
        os.path.realpath(args.sidecar_dir)
        if args.sidecar_dir and os.path.isdir(args.sidecar_dir)
        else _find_sidecar_dir()
    )

    # R5 — on-demand wire compression config + encoder probe.
    ENCODE_ENABLED = not args.no_encode
    ENCODE_CACHE_DIR = (
        os.path.abspath(args.encode_cache)
        if args.encode_cache
        else _default_encode_cache_dir()
    )
    if args.encode_level is not None:
        ENCODE_LEVEL_BROTLI = args.encode_level
    # Probe the encoder binaries once. brotli (primary) reuses build.sh's exact
    # CLI; gzip (CLI) is the always-available fallback. Python `brotli` module is
    # absent on this machine, so the CLI is the only brotli path. If neither
    # brotli is found we degrade to gzip-only (mirrors build.sh's fallback).
    BROTLI_BIN = shutil.which("brotli")
    GZIP_BIN = shutil.which("gzip")
    if ENCODE_ENABLED and not (BROTLI_BIN or GZIP_BIN):
        ENCODE_ENABLED = False  # no encoder → raw path everywhere

    # Session-telemetry ingest (§5). Open the store + start its single writer
    # thread before serving. The DB path defaults to native/web/telemetry/
    # sessions.db (gitignored runtime state).
    TELEMETRY_BIND_ANY = args.telemetry_bind_any
    TELEMETRY_SECRET = args.telemetry_secret or os.environ.get(
        "RB3_TELEMETRY_SECRET"
    )
    telemetry_db_path = (
        args.telemetry_db
        or os.environ.get("RB3_TELEMETRY_DB")
        or telemetry_db.DEFAULT_DB_PATH
    )
    TELEMETRY_STORE = telemetry_db.TelemetryStore(db_path=telemetry_db_path)
    TELEMETRY_STORE.start()

    if not os.path.isdir(BUILD_DIR):
        print(f"Build directory not found: {BUILD_DIR}")
        print("Run scripts/web/build.sh first.")
        sys.exit(1)

    print("RB3 Web Dev Server")
    print(f"  Build:   {BUILD_DIR}")
    if ASSETS_DIR:
        print(f"  Assets:  {ASSETS_DIR}")
    else:
        print("  Assets:  NOT CONFIGURED (set --assets-dir or RB3_ASSETS)")
    if FALLBACK_ASSETS_DIRS:
        for fb in FALLBACK_ASSETS_DIRS:
            print(f"  Fallback: {fb}")
    if OVERLAY_DIR:
        print(f"  Overlay: {OVERLAY_DIR} (DTA overlay; shadows extracted assets)")
    if SIDECAR_DIR:
        print(f"  Sidecars: {SIDECAR_DIR} (XMA->PCM, served on demand)")
    if not ENCODE_ENABLED:
        print("  Encode:  DISABLED (assets served raw)")
    elif BROTLI_BIN:
        print(
            f"  Encode:  brotli q{ENCODE_LEVEL_BROTLI} (CLI {BROTLI_BIN})"
            f" + gzip fallback -> {ENCODE_CACHE_DIR}"
        )
    else:
        print(
            f"  Encode:  gzip-only -{ENCODE_LEVEL_GZIP} (brotli CLI NOT found)"
            f" -> {ENCODE_CACHE_DIR}"
        )
    scope = ("REMOTE+localhost (shared secret for remote)"
             if TELEMETRY_BIND_ANY else "localhost-only")
    print(f"  Telemetry: {telemetry_db_path} ({scope})")
    print(f"  URL:     http://0.0.0.0:{args.port} (accessible remotely)")
    print(f"  API:     http://0.0.0.0:{args.port}/api/manifest")
    print(f"  COOP/COEP headers enabled")
    print()

    server = http.server.ThreadingHTTPServer(("0.0.0.0", args.port), RB3Handler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down.")
        server.shutdown()
    finally:
        if TELEMETRY_STORE is not None:
            TELEMETRY_STORE.stop()


if __name__ == "__main__":
    main()
