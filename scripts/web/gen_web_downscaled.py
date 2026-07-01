#!/usr/bin/env python3
"""Generate the web-downscaled venue milo tree (A4 texture mip-strip).

The 4 Mbps cold web journey is bytes-bound (~115 MB, milos ~75 MB). Inside a
venue `.milo_xbox`, the BC/DXT textures are a WIRE plurality (~44%) because BC
blocks barely brotli-compress; each texture carries a full mip chain whose TOP
level is ~3/4 of the chain's bytes. Dropping the top mip (promote the next mip to
base, half-resolution, near-lossless — the remaining mips are pre-authored)
removes exactly the brotli-incompressible bytes: it STACKS on Wave 5's q11 and
adds zero artifacts. Measured on the journey venue small_club_01:
11.67 -> 4.88 MB q11 wire (-58%).

This builds a WEB-ONLY served COPY at `orig-assets/web-downscaled/` mirroring the
request key (`world/venue/.../foo.milo_xbox`). The canonical
`orig-assets/extracted` tree that native/Wii/decomp read stays UNTOUCHED. The
server (native/web/server.py) resolves `/api/file/...` from this tree FIRST when
`RB3_WEB_DOWNSCALE` is enabled (default OFF until the visual gate flips it on);
the q11 prewarm walks this tree so the wire bytes are the stripped+q11 size.

SCOPE (initial exclusion): VENUES ONLY (`world/venue/**/*.milo_xbox`). Venues are
the texture-heavy target; char/UI/album-art/font milos are small + geometry-heavy
and are NOT stripped. This generator selects WHICH milos to process; the per-bitmap
exclusion logic lives in the strip tool. `mip_strip.strip_file` now applies the A4
T2 visual-gate exclusion list by default (`default_exclude`): BC5/DXN normal maps,
BC3-alpha detail/noise, and small textures (`max(w,h)<=256`) stay full-res, and
RndCubeTex face runs are never stripped (mixed face dims break the cube). The strip
also emits a SINGLE-block ChunkStream container — required, multi-block crashes the
gameplay venue loader (ChunkStream.cpp:458). See research/12 "A4 FIX WAVE".

The DO-NOT-COMMIT rule: the generated tree is large build/deploy output (like the
brotli cache). `orig-assets/` is already gitignored — never `git add` it.

Parallel + idempotent + atomic, like prewarm_encode_cache.py:
  - parallel (--jobs, multiprocessing)
  - idempotent: skips an output whose source mtime+size is unchanged (a `.src`
    sidecar records the source identity); a 2nd run is a no-op
  - atomic: strips into a unique temp file, validates it round-trips (the tool's
    own self-check + an optional dc3 validate_milo_entries), then renames into
    place — a reader never sees a half-written milo
  - HARD STOP on any milo that fails to round-trip (the tool raises) — better a
    big milo than one the engine rejects

Usage:
  scripts/web/gen_web_downscaled.py                 # venues -> web-downscaled/
  scripts/web/gen_web_downscaled.py --jobs 8
  scripts/web/gen_web_downscaled.py --include small_club   # filter substring
  scripts/web/gen_web_downscaled.py --validate             # + dc3 entry validator
  scripts/web/gen_web_downscaled.py --dry-run              # plan only
  scripts/web/gen_web_downscaled.py --force                # ignore the .src cache
  scripts/web/gen_web_downscaled.py --stats                # wire savings summary
"""

import argparse
import functools
import multiprocessing
import os
import random
import subprocess
import sys
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.normpath(os.path.join(_HERE, "..", ".."))
sys.path.insert(0, os.path.join(_REPO, "scripts", "milo"))
import mip_strip  # noqa: E402

# The canonical extracted tree (source) and the web-only downscaled tree (dest).
# The dest mirrors the request key, so `world/venue/.../foo.milo_xbox` in the
# source lands at the same relative path in the dest — which is exactly the
# /api/file key the server looks up.
DEFAULT_SRC = os.path.join(_REPO, "orig-assets", "extracted")
DEFAULT_DST = os.path.join(_REPO, "orig-assets", "web-downscaled")

# Initial scope: venues only. A milo's request-relative path must start with this
# to be eligible. (The strip tool's per-bitmap exclusion handles UI textures that
# might live inside a venue milo; this gate selects which FILES to touch.)
VENUE_PREFIX = os.path.join("world", "venue")


def _enumerate_venue_milos(src_root, include):
    """Yield (src_abs, rel_key) for every venue `.milo_xbox` under src_root.

    rel_key is the request-relative path (== the /api/file key and the dest path
    under the downscaled tree). An --include substring filters by rel_key."""
    venue_root = os.path.join(src_root, VENUE_PREFIX)
    if not os.path.isdir(venue_root):
        return
    for dirpath, _dirs, filenames in os.walk(venue_root):
        for fn in filenames:
            if not fn.endswith(".milo_xbox"):
                continue
            src = os.path.join(dirpath, fn)
            rel = os.path.relpath(src, src_root)
            if include and include not in rel:
                continue
            yield src, rel


def _src_meta_text(src_path):
    st = os.stat(src_path)
    return f"{st.st_size}:{int(st.st_mtime)}"


def _is_fresh(src_path, dst_path):
    """True if dst_path exists and its `.src` sidecar matches the current source
    identity (size+mtime) — the idempotency check."""
    if not os.path.isfile(dst_path):
        return False
    sidecar = dst_path + ".src"
    try:
        with open(sidecar) as fh:
            recorded = fh.read().strip()
    except OSError:
        return False
    return recorded == _src_meta_text(src_path)


def _atomic_rename(tmp, dst):
    os.makedirs(os.path.dirname(dst) or ".", exist_ok=True)
    os.rename(tmp, dst)


def _strip_one(job, dst_root, validate, force, dry_run):
    """Worker: strip one venue milo (src_abs, rel_key) into dst_root/rel_key.

    Atomic (unique temp + validate + rename) and idempotent (the `.src` sidecar).
    Returns a status string: 'stripped', 'skipped', or 'failed'. A round-trip
    failure inside the strip tool raises SystemExit in the worker; we convert it
    to 'failed' so the parent can HARD-STOP after the pool drains."""
    src_abs, rel = job
    dst_abs = os.path.join(dst_root, rel)

    if not force and _is_fresh(src_abs, dst_abs):
        return ("skipped", rel, None)
    if dry_run:
        return ("stripped", rel, None)

    tmp = dst_abs + f".{os.getpid()}.{random.randrange(1 << 32):08x}.tmp"
    # The progressive-sharpen sidecar (the discarded high-res top-mip delta) is
    # written from the SAME strip pass so it carries the exact bytes the strip
    # removes (same payload, same exclusion selection). It lands next to the
    # stripped venue as `<venue>.milo_xbox.sharpen`; the server serves it from
    # the downscaled tree, and the in-session sharpen manager (research/13 T1)
    # fetches it to restore each texture to full-res. Build output — gitignored,
    # never committed.
    sharpen_dst = dst_abs + ".sharpen"
    sharpen_tmp = tmp + ".sharpen"
    os.makedirs(os.path.dirname(dst_abs) or ".", exist_ok=True)
    try:
        # strip_file does the byte surgery AND a re-parse self-check: every
        # surviving bitmap must still land on the 0xADDEADDE object separator,
        # else it raises SystemExit. That is the primary round-trip guard. Passing
        # sidecar_path emits the sharpen delta from the still-full-res payload
        # before the strip, atomically tied to this same strip.
        mip_strip.strip_file(src_abs, tmp, levels=1, quiet=True,
                             sidecar_path=sharpen_tmp)
        # Optional second guard: the dc3 entry-table validator parses the whole
        # ObjectDir (rev/type/name/entries) and confirms it's a loadable milo.
        if validate:
            ok = _dc3_validate(tmp)
            if not ok:
                _unlink_quiet(tmp)
                _unlink_quiet(sharpen_tmp)
                return ("failed", rel, "dc3 validate failed")
    except SystemExit as e:
        _unlink_quiet(tmp)
        _unlink_quiet(sharpen_tmp)
        return ("failed", rel, f"round-trip: {e}")
    except Exception as e:  # noqa: BLE001 — any tool error is a hard stop
        _unlink_quiet(tmp)
        _unlink_quiet(sharpen_tmp)
        return ("failed", rel, f"{type(e).__name__}: {e}")

    try:
        _atomic_rename(tmp, dst_abs)
        # The sharpen sidecar may not exist if the venue had nothing strippable
        # (strip_file still writes an empty-entry SHRP blob via build_sharpen_
        # entries, so it normally does); rename it into place when present.
        if os.path.isfile(sharpen_tmp):
            _atomic_rename(sharpen_tmp, sharpen_dst)
        # Record source identity AFTER the rename so the sidecar describes the
        # bytes actually written.
        with open(dst_abs + ".src", "w") as fh:
            fh.write(_src_meta_text(src_abs))
    except OSError as e:
        _unlink_quiet(tmp)
        _unlink_quiet(sharpen_tmp)
        return ("failed", rel, f"rename: {e}")
    return ("stripped", rel, None)


def _unlink_quiet(path):
    try:
        os.unlink(path)
    except OSError:
        pass


_DC3_VALIDATOR = os.path.normpath(
    os.path.join(_REPO, "..", "dc3-decomp", "scripts", "milo",
                 "validate_milo_entries.py"))


def _dc3_validate(milo_path):
    """Run the dc3 entry-table validator on one milo. Returns True on OK, False
    on a parse failure or a missing validator (treated as a soft skip → True so
    a fresh checkout without dc3 doesn't block; the tool's own self-check is the
    hard guard)."""
    if not os.path.isfile(_DC3_VALIDATOR):
        return True  # validator unavailable → rely on the strip self-check
    try:
        proc = subprocess.run(
            [sys.executable, _DC3_VALIDATOR, milo_path],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    except (OSError, subprocess.TimeoutExpired):
        return False
    return proc.returncode == 0


def main():
    ap = argparse.ArgumentParser(
        description="Generate the web-downscaled venue milo tree (A4).",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", default=DEFAULT_SRC,
                    help="Source extracted tree (default: orig-assets/extracted).")
    ap.add_argument("--dst", default=DEFAULT_DST,
                    help="Output downscaled tree (default: orig-assets/web-downscaled).")
    ap.add_argument("--include", default=None,
                    help="Only process milos whose rel path contains this substring.")
    ap.add_argument("--jobs", type=int, default=None,
                    help="Parallel worker processes (default: nproc).")
    ap.add_argument("--validate", action="store_true",
                    help="Also run the dc3 validate_milo_entries.py on each output "
                         "(the strip tool always self-checks the 0xADDEADDE chain).")
    ap.add_argument("--force", action="store_true",
                    help="Re-strip even if the .src sidecar says the output is fresh.")
    ap.add_argument("--dry-run", action="store_true",
                    help="Plan only: count what would be stripped, write nothing.")
    ap.add_argument("--stats", action="store_true",
                    help="After generating, print a brotli-q11 wire-savings summary "
                         "(orig vs stripped) — slower, encodes both trees.")
    args = ap.parse_args()

    src_root = os.path.realpath(args.src)
    dst_root = os.path.realpath(args.dst) if os.path.isdir(args.dst) else os.path.abspath(args.dst)
    if not os.path.isdir(src_root):
        print(f"error: source tree not found: {src_root}", file=sys.stderr)
        return 2

    jobs_n = args.jobs or os.cpu_count() or 4
    files = list(_enumerate_venue_milos(src_root, args.include))

    print("gen_web_downscaled (A4 venue mip-strip)")
    print(f"  Src:   {src_root}")
    print(f"  Dst:   {dst_root}")
    print(f"  Scope: {VENUE_PREFIX}/**/*.milo_xbox  ({len(files)} venue milos)")
    if args.include:
        print(f"  Include: '{args.include}'")
    if args.validate:
        print(f"  Validate: dc3 validate_milo_entries "
              f"({'found' if os.path.isfile(_DC3_VALIDATOR) else 'MISSING — self-check only'})")
    if args.dry_run:
        print("  DRY RUN — nothing written")
    print(f"  Jobs:  {jobs_n}")
    print()

    if not files:
        print("  no venue milos to process")
        return 0

    t0 = time.time()
    worker = functools.partial(_strip_one, dst_root=dst_root, validate=args.validate,
                               force=args.force, dry_run=args.dry_run)
    stripped = skipped = failed = 0
    failures = []
    if jobs_n > 1 and len(files) > 1:
        with multiprocessing.Pool(jobs_n) as pool:
            for status, rel, err in pool.imap_unordered(worker, files, chunksize=1):
                if status == "stripped":
                    stripped += 1
                elif status == "skipped":
                    skipped += 1
                else:
                    failed += 1
                    failures.append((rel, err))
    else:
        for f in files:
            status, rel, err = worker(f)
            if status == "stripped":
                stripped += 1
            elif status == "skipped":
                skipped += 1
            else:
                failed += 1
                failures.append((rel, err))

    dt = time.time() - t0
    print(f"  stripped={stripped} skipped={skipped} failed={failed}  ({dt:.1f}s)")
    for rel, err in failures:
        print(f"  FAILED: {rel}  ({err})", file=sys.stderr)

    if failed:
        # HARD STOP: a milo the engine would reject is worse than a big one. Leave
        # a nonzero exit so the deploy loop gates on it.
        print(f"\nHARD STOP: {failed} milo(s) failed to round-trip — fix the tool "
              f"or exclude them; the downscaled tree is INCOMPLETE.", file=sys.stderr)
        return 1

    if args.stats and not args.dry_run:
        _print_stats(files, src_root, dst_root)
    return 0


def _print_stats(files, src_root, dst_root):
    """brotli-q11 wire savings summary (orig vs stripped). Encodes both trees, so
    it's slow — opt-in via --stats."""
    print("\n  q11 wire savings (brotli -q 11):")
    tot_orig = tot_strip = 0
    rows = []
    for src_abs, rel in sorted(files):
        dst_abs = os.path.join(dst_root, rel)
        if not os.path.isfile(dst_abs):
            continue
        o = mip_strip.brotli_size(src_abs)
        s = mip_strip.brotli_size(dst_abs)
        tot_orig += o
        tot_strip += s
        rows.append((rel, o, s))
    for rel, o, s in rows:
        pct = (100 * (o - s) / o) if o else 0.0
        print(f"    {rel:<60s} {o/1e6:6.2f} -> {s/1e6:6.2f} MB  (-{pct:4.1f}%)")
    if tot_orig:
        pct = 100 * (tot_orig - tot_strip) / tot_orig
        print(f"  TOTAL venue q11 wire: {tot_orig/1e6:.2f} -> {tot_strip/1e6:.2f} MB "
              f"(-{pct:.1f}%, saved {(tot_orig-tot_strip)/1e6:.2f} MB)")


if __name__ == "__main__":
    sys.exit(main())
