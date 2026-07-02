#!/usr/bin/env python3
"""Range-stripping reverse proxy — B1 verification (research/14 Lane B fix).

Forwards every request to a backend server.py, but DROPS the Range request
header on `.sharpen` sidecar requests — emulating a Range-ignoring topology
(RFC 9110 §14.2 lets a server ignore Range; `python -m http.server` does;
some proxies/CDN edges strip it). The backend then replies 200 + whole body
instead of 206, which is exactly the condition review finding B1 describes
(unbounded append loop -> wasm OOM before the fix).

Only `.sharpen` paths are stripped so mogg Range streaming stays intact —
this isolates the sidecar pump's behavior.

  --after N   honor Range for the first N .sharpen Range requests, strip from
              request N on. N=0 (default) strips from chunk 0 (whole-body
              accept arm); N=3 exercises the mid-assembly betrayal arm
              (discard + legacy single-fetch fallback).

Usage:
  RB3_WEB_DOWNSCALE=1 python3 native/web/server.py --port 8797   # backend
  python3 scripts/web/_range_strip_proxy.py --port 8798 --backend 8797 [--after N]
  node scripts/web/_sharpen_chunk_smoke.mjs --port 8798
"""
import argparse
import http.client
import http.server
import threading

ARGS = None
LOCK = threading.Lock()
SHARPEN_RANGE_SEEN = 0

HOP = {
    "connection", "keep-alive", "proxy-authenticate", "proxy-authorization",
    "te", "trailers", "transfer-encoding", "upgrade", "host",
}


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *a):  # quiet
        pass

    def _proxy(self):
        global SHARPEN_RANGE_SEEN
        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length) if length else None

        hdrs = {}
        strip_note = None
        is_sharpen = ".sharpen" in self.path
        for k, v in self.headers.items():
            kl = k.lower()
            if kl in HOP:
                continue
            if kl in ("range", "if-range") and is_sharpen:
                with LOCK:
                    n = SHARPEN_RANGE_SEEN
                    SHARPEN_RANGE_SEEN += 1
                if n >= ARGS.after:
                    strip_note = f"STRIPPED {k}: {v} (sharpen req #{n})"
                    continue  # drop it — backend serves 200 whole body
                strip_note = f"honored {k}: {v} (sharpen req #{n} < --after {ARGS.after})"
            hdrs[k] = v
        if strip_note:
            print(f"[range-strip-proxy] {self.path}: {strip_note}", flush=True)

        conn = http.client.HTTPConnection("127.0.0.1", ARGS.backend, timeout=300)
        try:
            conn.request(self.command, self.path, body=body, headers=hdrs)
            resp = conn.getresponse()
            data = resp.read()
        except Exception as e:
            try:
                self.send_error(502, f"proxy error: {e}")
            except Exception:
                pass
            return
        finally:
            conn.close()

        self.send_response(resp.status, resp.reason)
        for k, v in resp.getheaders():
            if k.lower() in ("connection", "transfer-encoding", "keep-alive",
                             "content-length"):
                continue
            self.send_header(k, v)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)

    do_GET = _proxy
    do_POST = _proxy
    do_HEAD = _proxy


def main():
    global ARGS
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=8798)
    p.add_argument("--backend", type=int, default=8797)
    p.add_argument("--after", type=int, default=0,
                   help="honor Range on the first N .sharpen requests, strip after")
    ARGS = p.parse_args()
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", ARGS.port), Handler)
    print(f"[range-strip-proxy] :{ARGS.port} -> 127.0.0.1:{ARGS.backend} "
          f"(strip .sharpen Range from req #{ARGS.after})", flush=True)
    srv.serve_forever()


if __name__ == "__main__":
    main()
