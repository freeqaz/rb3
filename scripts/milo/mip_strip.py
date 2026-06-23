#!/usr/bin/env python3
"""Milo texture mip-strip tool — census + top-mip strip for web downscale (A4).

Background
----------
The 4 Mbps cold web journey is bytes-bound (~115 MB, milos ~75 MB). Inside a
venue `.milo_xbox`, the BC/DXT textures are a WIRE plurality (~44%) because BC
blocks barely brotli-compress (ratio ~0.94) while geometry halves. Each texture
carries a full mip chain whose TOP level is ~3/4 of the chain's bytes. Dropping
the top mip (promote the next mip to base, half-resolution) removes exactly the
brotli-incompressible bytes — it STACKS on Wave 5's q11 and adds zero artifacts
(the remaining mips are pre-authored; no BC decode/encode).

This is a WEB-ONLY served copy. The canonical `orig-assets/extracted` tree that
native/Wii/decomp read stays UNTOUCHED.

On-disk format (decoded + validated against the 0xADDEADDE object separator)
----------------------------------------------------------------------------
Container: milo Version A (0xCABEDEAF), UNCOMPRESSED for venues. Inflated payload
= concat of the container blocks (see dc3 inflate_milo.py).

Cached RndTex bitmap (`.milo_xbox`, big-endian / Xbox360): the bitmap is embedded
INLINE in the milo stream right after the RndTex tail fields, with NO length
prefix (the cached venue passes the milo BinStream itself to the bitmap loader).
The on-disk RndBitmap is, per src/system/rndobj/Bitmap.cpp LoadHeader/SaveHeader:

    rev      u8     (0 or 1; BITMAP_REV=1)
    mBpp     u8
    mOrder   u32    (rev!=0; & 0x38 => DXT)
    numMips  u8     (count of ADDITIONAL mips after the base level)
    mWidth   u16
    mHeight  u16
    mRowBytes u16   (== mWidth * mBpp / 8, treating DXT as linear 4/8 bpp)
    pad      [0x13] (zeros, rev!=0)
    [palette PaletteBytes()]   (only when bpp<=8 and not DXT and not "white")
    base pixels  (mRowBytes * mHeight)
    mip[1] pixels (rowBytes(w/2)*(h/2)), mip[2] ...   x numMips

We do NOT need to walk the ObjectDir sequentially: we scan the inflated payload
for valid bitmap headers and accept ONLY chains whose computed end lands exactly
on an 0xADDEADDE object separator (strong validator → zero false positives).

Strip = promote mip[1] to base:
    remove base pixel block (sizes[0]); width/=2; height/=2; rowBytes/=2;
    numMips-=1. Palette + downstream mips are byte-identical.

Usage
-----
    mip_strip.py census  <file.milo_xbox> [--json]
    mip_strip.py strip   <in.milo_xbox> <out.milo_xbox> [--levels N] [--quiet]
    mip_strip.py brotli  <file.milo_xbox>        # q11 wire size of a milo

Exit nonzero on round-trip self-check failure.
"""

import argparse
import json
import os
import struct
import sys
from collections import Counter

MAGIC_A = 0xCABEDEAF
ADDE = b"\xad\xde\xad\xde"  # milo object separator (0xADDEADDE, stored as LE-ish marker)


# ---------------------------------------------------------------------------
# Container (Version A) read / write
# ---------------------------------------------------------------------------
def read_container(path):
    """Return (payload_bytes, meta) for a Version A milo. meta keeps offset+max_block."""
    with open(path, "rb") as f:
        raw = f.read()
    if len(raw) < 0x10:
        raise ValueError("file too small")
    magic, offset, num_blocks, max_block = struct.unpack_from("<IIII", raw, 0)
    if magic != MAGIC_A:
        raise ValueError(
            f"{os.path.basename(path)}: not an uncompressed (Version A) milo "
            f"(magic 0x{magic:08X}); this tool only strips uncompressed venues"
        )
    block_sizes = list(struct.unpack_from(f"<{num_blocks}I", raw, 0x10))
    o = offset
    chunks = []
    for sz in block_sizes:
        chunks.append(raw[o : o + sz])
        o += sz
    payload = b"".join(chunks)
    return payload, {"offset": offset, "max_block": max_block}


def write_container(path, payload, meta):
    """Write a Version A milo container around `payload`, mirroring the original
    block size + data offset so the loader sees a familiar shape."""
    max_block = meta["max_block"]
    offset = meta["offset"]
    blocks = [payload[i : i + max_block] for i in range(0, len(payload), max_block)]
    if not blocks:
        blocks = [b""]
    nb = len(blocks)
    hdr_min = 0x10 + nb * 4
    if offset < hdr_min:
        # round up to a 0x800 boundary, like the original
        offset = ((hdr_min + 0x7FF) // 0x800) * 0x800
    out = bytearray()
    out += struct.pack("<IIII", MAGIC_A, offset, nb, max(len(b) for b in blocks))
    for b in blocks:
        out += struct.pack("<I", len(b))
    out += b"\x00" * (offset - len(out))
    for b in blocks:
        out += b
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    with open(path, "wb") as f:
        f.write(out)


# ---------------------------------------------------------------------------
# Bitmap header decode / re-encode
# ---------------------------------------------------------------------------
def be16(d, o):
    return struct.unpack_from(">H", d, o)[0]


def be32(d, o):
    return struct.unpack_from(">I", d, o)[0]


def palette_bytes(bpp, order):
    # mirrors RndBitmap::PaletteBytes()
    if bpp <= 8 and (order & 0x38) == 0 and (order & 0x80) == 0:
        return (1 << bpp) * 4
    return 0


def row_bytes(w, bpp):
    # mirrors the on-disk mRowBytes (== width*bpp/8; DXT treated as linear 4/8 bpp)
    return (w * bpp) // 8


def parse_bitmap(d, base):
    """Try to parse a bitmap mip chain at `base`. Returns a dict or None.

    The returned dict has: base, rev, bpp, order, numMips, w, h, rb, pb (palette
    bytes), sizes (list of per-level pixel byte counts, [0]=base), end (one past
    the last mip byte).
    """
    n = len(d)
    if base + 32 > n:
        return None
    rev = d[base]
    if rev not in (0, 1):
        return None
    bpp = d[base + 1]
    if bpp not in (4, 8, 16, 24, 32):
        return None
    order = be32(d, base + 2)
    if order > 0xFF:
        return None
    numMips = d[base + 6]
    if numMips > 15:
        return None
    w = be16(d, base + 7)
    h = be16(d, base + 9)
    rb = be16(d, base + 11)
    if w == 0 or h == 0 or rb == 0 or w > 4096 or h > 4096:
        return None
    if rb != row_bytes(w, bpp):
        return None
    if any(d[base + 13 : base + 13 + 0x13]):
        return None
    pb = palette_bytes(bpp, order)
    sizes = [rb * h]
    ww, hh = w, h
    for _ in range(numMips):
        ww >>= 1
        hh >>= 1
        if ww == 0 or hh == 0:
            return None
        sizes.append(row_bytes(ww, bpp) * hh)
    end = base + 32 + pb + sum(sizes)
    if end > n:
        return None
    return dict(
        base=base, rev=rev, bpp=bpp, order=order, numMips=numMips,
        w=w, h=h, rb=rb, pb=pb, sizes=sizes, end=end,
    )


def find_bitmaps(payload):
    """Scan the inflated payload for valid bitmaps, accepting only chains that
    land exactly on an 0xADDEADDE object separator."""
    n = len(payload)
    out = []
    o = 0
    while o < n - 32:
        bm = parse_bitmap(payload, o)
        if bm and payload[bm["end"] : bm["end"] + 4] == ADDE:
            out.append(bm)
            o = bm["end"] + 4
        else:
            o += 1
    return out


def strip_bitmap_bytes(payload, bm, levels):
    """Return (new_payload, removed_bytes) for stripping `levels` top mip levels
    from the bitmap `bm`. `levels` is clamped to numMips (keep at least the
    smallest level)."""
    levels = min(levels, bm["numMips"])
    if levels <= 0:
        return payload, 0
    removed = sum(bm["sizes"][:levels])
    new_w = bm["w"] >> levels
    new_h = bm["h"] >> levels
    new_rb = row_bytes(new_w, bm["bpp"])
    new_mips = bm["numMips"] - levels
    # Rebuild the header (offsets per the layout above). All big-endian.
    base = bm["base"]
    hdr = bytearray(payload[base : base + 32])
    hdr[6] = new_mips & 0xFF
    struct.pack_into(">H", hdr, 7, new_w)
    struct.pack_into(">H", hdr, 9, new_h)
    struct.pack_into(">H", hdr, 11, new_rb)
    # remaining bytes after the dropped base levels: palette + surviving mips
    palette = payload[base + 32 : base + 32 + bm["pb"]]
    kept_start = base + 32 + bm["pb"] + removed
    kept = payload[kept_start : bm["end"]]
    new_chunk = bytes(hdr) + palette + kept
    new_payload = payload[:base] + new_chunk + payload[bm["end"] :]
    return new_payload, removed


# ---------------------------------------------------------------------------
# Census
# ---------------------------------------------------------------------------
def census(path):
    payload, meta = read_container(path)
    bitmaps = find_bitmaps(payload)
    total_tex = sum(32 + b["pb"] + sum(b["sizes"]) for b in bitmaps)
    # strippable bytes for a single-level strip = the base (top) level of each
    # bitmap that has at least one additional mip.
    strip_one = sum(b["sizes"][0] for b in bitmaps if b["numMips"] >= 1)
    n_strip = sum(1 for b in bitmaps if b["numMips"] >= 1)
    dist = Counter(b["numMips"] for b in bitmaps)
    dxt = Counter()
    for b in bitmaps:
        if b["order"] & 0x38:
            dxt["dxt"] += 1
        else:
            dxt["linear"] += 1
    return {
        "path": path,
        "payload_bytes": len(payload),
        "num_bitmaps": len(bitmaps),
        "num_strippable": n_strip,
        "total_texture_bytes": total_tex,
        "strippable_top_mip_bytes": strip_one,
        "strippable_fraction_of_texture": (strip_one / total_tex) if total_tex else 0.0,
        "numMips_distribution": dict(sorted(dist.items())),
        "format_counts": dict(dxt),
        "bitmaps": bitmaps,
        "meta": meta,
    }


# ---------------------------------------------------------------------------
# Strip whole file
# ---------------------------------------------------------------------------
def strip_file(in_path, out_path, levels=1, quiet=False, exclude=None):
    payload, meta = read_container(in_path)
    bitmaps = find_bitmaps(payload)
    # strip from the END of the payload backwards so earlier offsets stay valid.
    total_removed = 0
    n_stripped = 0
    for bm in sorted(bitmaps, key=lambda b: -b["base"]):
        if bm["numMips"] < 1:
            continue
        if exclude and exclude(bm):
            continue
        payload, removed = strip_bitmap_bytes(payload, bm, levels)
        total_removed += removed
        n_stripped += 1
    write_container(out_path, payload, meta)
    if not quiet:
        before = os.path.getsize(in_path)
        after = os.path.getsize(out_path)
        print(
            f"  {os.path.basename(in_path)}: stripped {n_stripped}/{len(bitmaps)} "
            f"bitmaps, removed {total_removed:,} payload bytes; "
            f"file {before:,} -> {after:,} ({100*after/before:.1f}%)"
        )
    # round-trip self-check: re-parse the output, every surviving bitmap must
    # still land on ADDEADDE.
    out_payload, _ = read_container(out_path)
    out_bms = find_bitmaps(out_payload)
    if len(out_bms) != len(bitmaps):
        raise SystemExit(
            f"ROUND-TRIP FAIL: re-parsed {len(out_bms)} bitmaps, expected {len(bitmaps)}"
        )
    return {
        "in": in_path, "out": out_path, "levels": levels,
        "n_bitmaps": len(bitmaps), "n_stripped": n_stripped,
        "removed_bytes": total_removed,
        "size_before": os.path.getsize(in_path),
        "size_after": os.path.getsize(out_path),
    }


def brotli_size(path):
    """brotli-q11 wire size, matching the web prewarm (scripts/web/prewarm_encode_cache.py).
    Prefers the python module; falls back to the `brotli` CLI (what prewarm uses)."""
    with open(path, "rb") as f:
        data = f.read()
    try:
        import brotli
        return len(brotli.compress(data, quality=11))
    except ImportError:
        pass
    import shutil
    import subprocess
    exe = shutil.which("brotli")
    if not exe:
        raise SystemExit("no brotli (python module or CLI)")
    out = subprocess.run([exe, "-q", "11", "-c"], input=data, stdout=subprocess.PIPE,
                         check=True).stdout
    return len(out)


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    c = sub.add_parser("census", help="report per-texture numMips + strippable bytes")
    c.add_argument("file")
    c.add_argument("--json", action="store_true")

    s = sub.add_parser("strip", help="write a top-mip-stripped copy")
    s.add_argument("input")
    s.add_argument("output")
    s.add_argument("--levels", type=int, default=1)
    s.add_argument("--quiet", action="store_true")

    b = sub.add_parser("brotli", help="brotli-q11 wire size of a milo")
    b.add_argument("file")

    args = ap.parse_args()

    if args.cmd == "census":
        rep = census(args.file)
        if args.json:
            rep = {k: v for k, v in rep.items() if k != "bitmaps"}
            json.dump(rep, sys.stdout, indent=2)
            print()
        else:
            print(f"{os.path.basename(args.file)}")
            print(f"  payload bytes        : {rep['payload_bytes']:,}")
            print(f"  bitmaps              : {rep['num_bitmaps']}")
            print(f"  strippable (mips>=1) : {rep['num_strippable']}")
            print(f"  texture bytes        : {rep['total_texture_bytes']:,}")
            print(f"  strippable top-mip   : {rep['strippable_top_mip_bytes']:,} "
                  f"({100*rep['strippable_fraction_of_texture']:.1f}% of texture bytes)")
            print(f"  numMips distribution : {rep['numMips_distribution']}")
            print(f"  format counts        : {rep['format_counts']}")
    elif args.cmd == "strip":
        strip_file(args.input, args.output, levels=args.levels, quiet=args.quiet)
    elif args.cmd == "brotli":
        print(f"{os.path.basename(args.file)}: brotli-q11 = {brotli_size(args.file):,} bytes")


if __name__ == "__main__":
    main()
