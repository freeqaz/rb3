#!/usr/bin/env python3
"""Offline brotli-q11 pre-warm of the web dev server's encode cache (W5-T1).

The dev server (native/web/server.py) compresses compressible assets ON DEMAND
at brotli q5 — cheap (paid inline on the first, freezing fetch) but leaves ~11 %
of wire bytes on the table vs q11 (research/08 §2: q11/q5 ≈ 0.89 on the journey
milo mix). q11 costs ~1.2–1.5 s/MB single-core, far too slow to pay inline, so it
must be PRE-warmed offline. This script walks the same asset roots the server
serves, pre-builds q11 artifacts into the SAME ENCODE_CACHE_DIR with the SAME
atomic temp+rename layout + 3-field `.meta` the server reads, and ALSO pre-builds
the boot/config/per-screen bundle artifacts by IMPORTING server.py's bundle-body
builders + fingerprint (one source of truth — no fingerprint drift).

Design:
  - imports native/web/server.py — reuses COMPRESSIBLE/INCOMPRESSIBLE_EXTS, the
    encode primitives (encode_file_to_cache / encode_bytes), the .meta parser
    (parse_meta_text / meta_is_valid_for), and the bundle builders
    (build_config_bundle_entries / build_manifest_bundle_entries /
    serialize_bundle / bundle_fingerprint). No logic is duplicated.
  - idempotent: skips an artifact whose `.meta` is FRESH for the current source
    AND already at >= the target level (a pre-existing q5 entry is upgraded to
    q11; a q11 entry is left alone). A 2nd run is a no-op.
  - parallel (--jobs, multiprocessing); nice-able (run under `nice` or with
    --nice). On-demand q5 stays the server's cold fallback for anything the
    pre-warm hasn't reached yet.

Full tree ≈ 90 CPU-min ≈ ~6 min wall on 16 cores; cache ≈ 2.4 GB (gitignored).

Usage:
  scripts/web/prewarm_encode_cache.py                 # auto-detect roots, q11
  scripts/web/prewarm_encode_cache.py --jobs 8 --level 11
  scripts/web/prewarm_encode_cache.py --roots /path/a --roots /path/b
  scripts/web/prewarm_encode_cache.py --include world/venue   # filter substring
  scripts/web/prewarm_encode_cache.py --dry-run               # plan only
"""

import argparse
import functools
import glob
import multiprocessing
import os
import sys
import time

# Import the dev server as the one source of truth for the cache layout, the
# encode primitives, the compressible-ext policy, and the bundle builders.
_HERE = os.path.dirname(os.path.abspath(__file__))
_SERVER_DIR = os.path.normpath(os.path.join(_HERE, "..", "..", "native", "web"))
sys.path.insert(0, _SERVER_DIR)
import server  # noqa: E402


# The encode level the server WRITES into a .meta is the level the artifact was
# built at. A legacy 2-field .meta (no level) is a pre-W5 q5 entry; treat its
# effective level as the server's on-demand brotli default (q5) for the
# upgrade decision.
LEGACY_LEVEL = server.ENCODE_LEVEL_BROTLI  # 5


def _configure_server_globals(args):
    """Resolve the server's module globals exactly as server.main() would, so the
    bundle builders + cache dir + encoder binaries match the running server."""
    server.ASSETS_DIR = args.assets_dir or server._find_assets_dir()
    server.OVERLAY_DIR = (
        os.path.realpath(args.overlay_dir)
        if args.overlay_dir and os.path.isdir(args.overlay_dir)
        else server._find_overlay_dir()
    )
    if args.assets_fallback:
        server.FALLBACK_ASSETS_DIRS = [
            os.path.realpath(p) for p in args.assets_fallback if os.path.isdir(p)
        ]
    else:
        server.FALLBACK_ASSETS_DIRS = server._find_fallback_dirs()
    server.SIDECAR_DIR = (
        os.path.realpath(args.sidecar_dir)
        if args.sidecar_dir and os.path.isdir(args.sidecar_dir)
        else server._find_sidecar_dir()
    )
    # A4 web downscale: mirror the server's resolution so a standalone prewarm run
    # (auto-detect roots) walks the stripped venue tree when the flag is on. The
    # CLI --downscale/--no-downscale overrides the RB3_WEB_DOWNSCALE env.
    server.DOWNSCALE_DIR = (
        os.path.realpath(args.downscale_dir)
        if args.downscale_dir and os.path.isdir(args.downscale_dir)
        else server._find_downscale_dir()
    )
    if args.downscale is not None:
        server.DOWNSCALE_ENABLED = args.downscale
    else:
        server.DOWNSCALE_ENABLED = server._downscale_enabled_from_env()
    server.ENCODE_CACHE_DIR = (
        os.path.abspath(args.encode_cache)
        if args.encode_cache
        else server._default_encode_cache_dir()
    )
    # Resolve encoder binaries exactly as the server does.
    import shutil
    server.BROTLI_BIN = shutil.which("brotli")
    server.GZIP_BIN = shutil.which("gzip")


def _resolved_roots(args):
    """The directory roots to walk. Default = the server's auto-detected
    assets + fallback + sidecar dirs (the three trees /api/file serves)."""
    if args.roots:
        return [os.path.realpath(r) for r in args.roots if os.path.isdir(r)]
    roots = []
    # A4: the stripped venue tree goes FIRST so its milos win the first-root-wins
    # de-dupe (same cache key as the extracted milo, smaller stripped bytes) when
    # the downscale is enabled. Off ⇒ not prepended (the extracted milo warms).
    if server.DOWNSCALE_ENABLED and server.DOWNSCALE_DIR:
        roots.append(server.DOWNSCALE_DIR)
    if server.ASSETS_DIR:
        roots.append(server.ASSETS_DIR)
    roots.extend(server.FALLBACK_ASSETS_DIRS)
    if server.SIDECAR_DIR:
        roots.append(server.SIDECAR_DIR)
    # de-dupe preserving order
    seen, out = set(), []
    for r in roots:
        rr = os.path.realpath(r)
        if rr not in seen and os.path.isdir(rr):
            seen.add(rr)
            out.append(rr)
    return out


def _cache_key_for(src_path, root):
    """Map an on-disk source file under `root` to the request-relative cache key
    the server keys its file cache on (the normpath'd /api/file path the client
    asks for — `safe` in server._serve_asset_file).

    Two server-side resolution quirks are reversed here so the prewarmed cache
    key is the one the server actually looks up at request time:

    - Sidecars are flat <hex>.pcm under SIDECAR_DIR but the runtime requests them
      at sfx/gen/xma_pcm/<hex>.pcm — carry that virtual prefix.
    - Files under system/run/ live on disk at (..)/(..)/system/run/... but the
      client requests them as system/run/... (the server probes the (..)/(..)
      alt). Strip a leading (..)/(..)/ so the cache key matches the request.
    """
    if server.SIDECAR_DIR and os.path.realpath(root) == os.path.realpath(server.SIDECAR_DIR):
        return os.path.join("sfx", "gen", "xma_pcm", os.path.basename(src_path))
    rel = os.path.normpath(os.path.relpath(src_path, root))
    prefix = os.path.join("(..)", "(..)") + os.sep
    if rel.startswith(prefix):
        rel = rel[len(prefix):]
    return rel


def _is_compressible_ext(path):
    """Mirror the server's deny-over-allow ext policy."""
    _r, ext = os.path.splitext(path)
    ext = ext.lower()
    if ext in server.INCOMPRESSIBLE_EXTS:
        return False
    return ext in server.COMPRESSIBLE_EXTS


def _enumerate_files(roots, include):
    """Walk roots, yield (src_path, cache_key) for every compressible file.

    De-dupes by cache_key (a file present in both ASSETS_DIR and a fallback root
    resolves to ASSETS_DIR for the server — first root wins, matching the
    server's probe order). An --include substring filters by cache key."""
    seen = set()
    for root in roots:
        for dirpath, _dirs, filenames in os.walk(root):
            for fn in filenames:
                src = os.path.join(dirpath, fn)
                if not _is_compressible_ext(src):
                    continue
                key = _cache_key_for(src, root)
                if include and include not in key:
                    continue
                if key in seen:
                    continue
                seen.add(key)
                yield src, key


def _meta_level(cache_path):
    """Effective encode level recorded in `cache_path`'s `.meta`, or None if the
    artifact/.meta is absent or unreadable. A legacy 2-field entry → LEGACY_LEVEL."""
    meta_path = cache_path + ".meta"
    try:
        with open(meta_path, "r") as fh:
            parsed = server.parse_meta_text(fh.read())
    except OSError:
        return None
    if parsed is None:
        return None
    _size, _mtime, _enc, level = parsed
    return level if level is not None else LEGACY_LEVEL


def _needs_warm(src_path, cache_path, target_level):
    """True if `cache_path` is stale for `src_path`, missing, or below
    `target_level` — i.e. the artifact should be (re)built at target_level."""
    if not os.path.isfile(cache_path):
        return True
    meta_path = cache_path + ".meta"
    try:
        st = os.stat(src_path)
        with open(meta_path, "r") as fh:
            meta = fh.read().strip()
    except OSError:
        return True
    if not server.meta_is_valid_for(meta, st.st_size, st.st_mtime):
        return True  # source changed → rebuild
    have = _meta_level(cache_path)
    if have is None:
        return True
    return have < target_level  # upgrade q5 -> q11


def _warm_one(job, enc, target_level, dry_run):
    """Worker: warm a single (src_path, cache_key). Returns a status string:
    'warmed', 'skipped', or 'failed'. Runs in a child process (multiprocessing),
    so it re-reads the already-imported server module's globals via fork."""
    src_path, cache_key = job
    cache_ext = ".br" if enc == "br" else ".gz"
    cache_path = os.path.join(server.ENCODE_CACHE_DIR, cache_key + cache_ext)
    if not _needs_warm(src_path, cache_path, target_level):
        return "skipped"
    if dry_run:
        return "warmed"  # would warm
    if server.encode_file_to_cache(src_path, cache_path, enc, target_level):
        return "warmed"
    return "failed"


def _warm_bundles(enc, target_level, dry_run, log):
    """Pre-build the boot / config / per-screen bundle artifacts at target_level,
    using server.py's builders so the body + fingerprint (and thus the cache key)
    match the server's request-time lookup exactly. Returns (warmed, skipped,
    failed) counts."""
    cache_ext = ".br" if enc == "br" else ".gz"
    cache_dir = os.path.join(server.ENCODE_CACHE_DIR, "_bundles")
    try:
        os.makedirs(cache_dir, exist_ok=True)
    except OSError:
        return (0, 0, 1)

    jobs = []  # (cache_name, entries)
    # config bundle (.dta/.dtb)
    jobs.append(("config", server.build_config_bundle_entries()))
    # boot bundle
    if os.path.isfile(server.BOOT_MANIFEST_PATH):
        jobs.append(("boot",
                     server.build_manifest_bundle_entries(server.BOOT_MANIFEST_PATH)))
    # per-screen bundles
    for m in sorted(glob.glob(os.path.join(server.SCREEN_MANIFEST_DIR,
                                           "screen-*.manifest"))):
        name = os.path.basename(m)[len("screen-"):-len(".manifest")]
        jobs.append((f"screen-{name}",
                     server.build_manifest_bundle_entries(m)))

    warmed = skipped = failed = 0
    for cache_name, entries in jobs:
        if not entries:
            log(f"  bundle {cache_name}: empty (skipped)")
            continue
        fp = server.bundle_fingerprint(entries)
        cache_path = os.path.join(cache_dir, f"{cache_name}.{fp}{cache_ext}")
        # The bundle artifact filename has no embedded level. We DON'T blow away a
        # fresh artifact unconditionally — but since the level isn't recorded in
        # the name, we rebuild only if the artifact is missing (the server writes
        # q5; the pre-warm writes q11 over the same path so a q11 file persists).
        # To force a q11 upgrade of an existing q5 bundle, rebuild when present at
        # the wrong level — tracked via a sidecar `.lvl` marker.
        lvl_path = cache_path + ".lvl"
        have_level = None
        if os.path.isfile(cache_path):
            try:
                with open(lvl_path) as fh:
                    have_level = int(fh.read().strip())
            except (OSError, ValueError):
                have_level = LEGACY_LEVEL  # present but unmarked → q5
        if have_level is not None and have_level >= target_level:
            skipped += 1
            continue
        if dry_run:
            warmed += 1
            continue
        body = server.serialize_bundle(entries)
        comp = server.encode_bytes(body, enc, target_level)
        if comp is None:
            failed += 1
            log(f"  bundle {cache_name}: encode FAILED")
            continue
        if server._atomic_write(cache_path, comp):
            server._atomic_write(lvl_path, str(target_level).encode())
            mb = len(comp) / 1e6
            log(f"  bundle {cache_name}.{fp}{cache_ext}: {mb:.2f} MB (q{target_level})")
            warmed += 1
        else:
            failed += 1
    return (warmed, skipped, failed)


def main():
    parser = argparse.ArgumentParser(
        description="Offline brotli-q11 pre-warm of the web encode cache (W5-T1).")
    parser.add_argument("--roots", action="append", default=None,
                        help="Asset root to walk (repeatable; default: the "
                             "server's auto-detected assets+fallback+sidecar dirs).")
    parser.add_argument("--assets-dir", default=None,
                        help="Override the primary assets dir (server auto-detect default).")
    parser.add_argument("--assets-fallback", action="append", default=None,
                        help="Override fallback asset roots.")
    parser.add_argument("--sidecar-dir", default=None,
                        help="Override the XMA->PCM sidecar dir.")
    parser.add_argument("--overlay-dir", default=None,
                        help="Override the DTA overlay dir.")
    parser.add_argument("--downscale-dir", default=None,
                        help="Override the A4 web-downscaled venue tree.")
    parser.add_argument("--downscale", dest="downscale", action="store_true",
                        default=None,
                        help="Walk the A4 downscaled venue tree first (stripped "
                             "milos win; default OFF, also RB3_WEB_DOWNSCALE=1).")
    parser.add_argument("--no-downscale", dest="downscale", action="store_false",
                        help="Force the A4 downscale OFF (overrides the env).")
    parser.add_argument("--encode-cache", default=None,
                        help="Encode cache dir (default: RB3_ENCODE_CACHE env or "
                             "native/web/.cache/encoded — same as the server).")
    parser.add_argument("--level", type=int, default=11,
                        help="brotli quality (default 11 — the offline pre-warm level).")
    parser.add_argument("--encoder", choices=["br", "gzip"], default="br",
                        help="Encoder (default br; gzip is the fallback arm).")
    parser.add_argument("--jobs", type=int, default=None,
                        help="Parallel worker processes (default: nproc).")
    parser.add_argument("--include", default=None,
                        help="Only warm cache keys containing this substring.")
    parser.add_argument("--nice", type=int, default=None,
                        help="Re-nice this process (and children) to N before warming.")
    parser.add_argument("--no-bundles", action="store_true",
                        help="Skip the boot/config/screen bundle artifacts.")
    parser.add_argument("--dry-run", action="store_true",
                        help="Plan only: count what would be warmed, write nothing.")
    args = parser.parse_args()

    if args.nice is not None:
        try:
            os.nice(args.nice)
        except OSError:
            pass

    _configure_server_globals(args)

    enc = args.encoder
    if enc == "br" and not server.BROTLI_BIN:
        print("error: brotli CLI not found (and --encoder br requested)", file=sys.stderr)
        return 2
    if enc == "gzip" and not server.GZIP_BIN:
        print("error: gzip CLI not found", file=sys.stderr)
        return 2

    if not server.ASSETS_DIR and not args.roots:
        print("error: no assets dir configured (set --assets-dir/--roots or "
              "RB3_ASSETS)", file=sys.stderr)
        return 2

    roots = _resolved_roots(args)
    if not roots:
        print("error: no valid asset roots to walk", file=sys.stderr)
        return 2

    jobs_n = args.jobs or os.cpu_count() or 4
    target_level = args.level

    print(f"Prewarm encode cache (W5-T1)")
    print(f"  Cache:    {server.ENCODE_CACHE_DIR}")
    print(f"  Encoder:  {enc} q{target_level}  (jobs={jobs_n})")
    for r in roots:
        print(f"  Root:     {r}")
    if args.include:
        print(f"  Include:  '{args.include}'")
    if args.dry_run:
        print("  DRY RUN — nothing written")
    print()

    files = list(_enumerate_files(roots, args.include))
    print(f"  {len(files)} compressible files to consider")

    t0 = time.time()
    warmed = skipped = failed = 0
    worker = functools.partial(_warm_one, enc=enc, target_level=target_level,
                               dry_run=args.dry_run)
    if jobs_n > 1 and len(files) > 1:
        with multiprocessing.Pool(jobs_n) as pool:
            for i, status in enumerate(pool.imap_unordered(worker, files, chunksize=8)):
                if status == "warmed":
                    warmed += 1
                elif status == "skipped":
                    skipped += 1
                else:
                    failed += 1
                if (i + 1) % 200 == 0:
                    print(f"  ... {i+1}/{len(files)} "
                          f"(warmed={warmed} skipped={skipped} failed={failed})",
                          flush=True)
    else:
        for f in files:
            status = worker(f)
            if status == "warmed":
                warmed += 1
            elif status == "skipped":
                skipped += 1
            else:
                failed += 1

    b_warmed = b_skipped = b_failed = 0
    if not args.no_bundles:
        print("  bundles:")
        b_warmed, b_skipped, b_failed = _warm_bundles(
            enc, target_level, args.dry_run, lambda m: print(m, flush=True))

    dt = time.time() - t0
    print()
    print(f"  files:   warmed={warmed} skipped={skipped} failed={failed}")
    print(f"  bundles: warmed={b_warmed} skipped={b_skipped} failed={b_failed}")
    print(f"  elapsed: {dt:.1f}s")
    # Non-zero exit only on a hard failure (so CI/loops can gate); a fully-skipped
    # idempotent run is success (exit 0).
    return 1 if (failed or b_failed) else 0


if __name__ == "__main__":
    sys.exit(main())
