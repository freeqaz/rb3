#!/usr/bin/env python3
"""_netbytes.py — per-category wire-byte aggregator for RB3 netmatrix runs.

Usage:
    python3 scripts/web/_netbytes.py <net.ndjson> [<net.ndjson> ...]

Reads one or more net.ndjson files from _netmatrix.mjs (or _netmatrix_slow.mjs)
runs and prints:

  - Per-category wire totals: milo / pcm+ogg / mogg / bundle / misc
  - Per-category request counts
  - Top-10 largest individual fetches
  - Duplicate-fetch count (same URL fetched more than once)
  - Grand total wire bytes

Category definitions (by URL pattern):
  milo    — /api/file/**/*.milo_xbox  (and .milo .milo_ps3 .milo_wii)
  pcm     — /api/file/songs/xma_pcm/*.pcm  (raw sidecar)
  ogg     — /api/file/songs/xma_pcm/*.ogg  (vorbis sidecar, T2 arm)
  mogg    — /api/file/songs/*.mogg  (Range chunks — bytes field is per-chunk)
  bundle  — /api/bundle  and  /api/bundle/*
  misc    — everything else (wasm, js, manifest, png_xbox, mid, txt, etc.)

Wire bytes = `bytes` field in net.ndjson = CDP encodedDataLength (post-Content-
Encoding), same as what the browser actually receives over the wire.  Duplicate
URLs are counted + listed but their bytes are included in the totals (they were
real wire traffic).
"""

import sys
import json
import os
import collections


# ---------------------------------------------------------------------------
# Category classifier
# ---------------------------------------------------------------------------

_MILO_EXTS = {".milo_xbox", ".milo", ".milo_ps3", ".milo_wii"}

def classify(url: str) -> str:
    """Return the category string for a URL."""
    path = url.split("?")[0]  # strip query string
    # strip http://host/  prefix
    idx = path.find("/api/")
    if idx >= 0:
        path = path[idx:]
    else:
        # static assets (wasm, js, index.html)
        return "misc"

    if path.startswith("/api/bundle"):
        return "bundle"
    if path.startswith("/api/manifest"):
        return "misc"

    # /api/file/<rel>
    if path.startswith("/api/file/"):
        rel = path[len("/api/file/"):]
        # extension
        _, ext = os.path.splitext(rel)
        ext = ext.lower()
        if ext in _MILO_EXTS:
            return "milo"
        if "xma_pcm" in rel or "sfx_pcm" in rel:
            if ext == ".pcm":
                return "pcm"
            if ext == ".ogg":
                return "ogg"
        if ext == ".mogg":
            return "mogg"
        return "misc"

    # /api/version, /api/health, etc.
    return "misc"


# ---------------------------------------------------------------------------
# Aggregation
# ---------------------------------------------------------------------------

def aggregate(records):
    """Aggregate a list of {url, range, bytes, dur} dicts.

    Returns a dict with keys:
        categories  — OrderedDict {cat: {bytes, reqs}}
        top10       — list of (url, bytes) largest 10 unique URL totals
        duplicates  — count of URLs fetched more than once
        total_bytes — int
        total_reqs  — int
    """
    by_url = collections.defaultdict(lambda: {"bytes": 0, "reqs": 0})
    by_cat = collections.defaultdict(lambda: {"bytes": 0, "reqs": 0})

    for r in records:
        url = r.get("url", "")
        b = int(r.get("bytes", 0))
        cat = classify(url)
        by_url[url]["bytes"] += b
        by_url[url]["reqs"] += 1
        by_cat[cat]["bytes"] += b
        by_cat[cat]["reqs"] += 1

    total_bytes = sum(v["bytes"] for v in by_cat.values())
    total_reqs = sum(v["reqs"] for v in by_cat.values())

    # Duplicates = URLs fetched more than once
    duplicates = sum(1 for v in by_url.values() if v["reqs"] > 1)

    # Top-10 largest by total bytes across all fetches of that URL
    top10 = sorted(by_url.items(), key=lambda kv: kv[1]["bytes"], reverse=True)[:10]

    # Ordered categories for display
    order = ["milo", "pcm", "ogg", "mogg", "bundle", "misc"]
    categories = collections.OrderedDict()
    for cat in order:
        if cat in by_cat:
            categories[cat] = by_cat[cat]
    # Any unexpected categories
    for cat in sorted(by_cat):
        if cat not in categories:
            categories[cat] = by_cat[cat]

    return {
        "categories": categories,
        "top10": [(url, info["bytes"], info["reqs"]) for url, info in top10],
        "duplicates": duplicates,
        "total_bytes": total_bytes,
        "total_reqs": total_reqs,
    }


# ---------------------------------------------------------------------------
# Formatting helpers
# ---------------------------------------------------------------------------

def _mb(b: int) -> str:
    return f"{b / 1_000_000:.2f} MB"


def _fmt_url(url: str, max_len: int = 80) -> str:
    # Strip http://host prefix for display
    idx = url.find("/api/")
    if idx < 0:
        idx = url.find("/", 8)  # after http://
    if idx >= 0:
        url = url[idx:]
    if len(url) > max_len:
        url = "..." + url[-(max_len - 3):]
    return url


def print_report(agg, label: str = ""):
    cats = agg["categories"]
    top10 = agg["top10"]
    total_bytes = agg["total_bytes"]
    total_reqs = agg["total_reqs"]
    dupes = agg["duplicates"]

    if label:
        print(f"\n=== {label} ===")
    print()
    print(f"{'Category':<12}  {'Wire bytes':>12}  {'Reqs':>6}  {'% of total':>10}")
    print("-" * 48)
    for cat, info in cats.items():
        pct = 100.0 * info["bytes"] / total_bytes if total_bytes else 0
        print(f"  {cat:<10}  {_mb(info['bytes']):>12}  {info['reqs']:>6}  {pct:>9.1f}%")
    print("-" * 48)
    print(f"  {'TOTAL':<10}  {_mb(total_bytes):>12}  {total_reqs:>6}  {'100.0%':>10}")
    print()
    print(f"Duplicate URLs (fetched >1 time): {dupes}")
    print()
    print("Top-10 largest fetches (by total bytes per URL):")
    for i, (url, b, reqs) in enumerate(top10, 1):
        reqs_str = f" x{reqs}" if reqs > 1 else ""
        print(f"  {i:2d}.  {_mb(b):>10}  {_fmt_url(url)}{reqs_str}")
    print()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def load_ndjson(path: str):
    records = []
    with open(path, "r") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                records.append(json.loads(line))
            except json.JSONDecodeError:
                pass
    return records


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__)
        sys.exit(0)

    paths = sys.argv[1:]

    if len(paths) == 1:
        records = load_ndjson(paths[0])
        agg = aggregate(records)
        print_report(agg, label=paths[0])
    else:
        # Multiple files: individual + combined
        all_records = []
        for path in paths:
            records = load_ndjson(path)
            agg = aggregate(records)
            print_report(agg, label=path)
            all_records.extend(records)
        print("=" * 60)
        agg_all = aggregate(all_records)
        print_report(agg_all, label=f"COMBINED ({len(paths)} runs)")


if __name__ == "__main__":
    main()
