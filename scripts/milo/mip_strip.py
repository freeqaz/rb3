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
    mip_strip.py sharpen <in.milo_xbox> <out.sharpen>  # progressive-sharpen sidecar
                                                       # (the discarded high-res
                                                       # top-mip delta; see below)

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
    """Write a Version A milo container around `payload` as a SINGLE block.

    CRITICAL — single-block only. A venue `.milo_xbox` is a ChunkStream container
    (`src/system/utl/ChunkStream.cpp`); its reader (`ReadImpl`, line 168) asserts
    `mCurBufOffset + bytes <= (*mCurChunk & kChunkSizeMask)` — every object read
    must fit inside one block. The original is chunked into ~131 KB blocks whose
    boundaries align to object reads; after the strip the payload is smaller, so
    re-chunking into fresh `max_block`-sized blocks puts boundaries mid-object →
    `ReadImpl` desync → abort at `ChunkStream.cpp:458` on the GAMEPLAY venue load.
    Emitting ONE block (`num_blocks=1`, `max_block=len(payload)`) makes that assert
    unreachable for ANY venue by construction (no read can cross a boundary). The
    wire cost vs multi-block is ±1 KB on a multi-MB brotli wire (negligible).

    `dc3 validate_milo_entries` and the `RB3_BOOT`/`DirLoader::LoadObjects` path
    read the payload as a plain concat and never exercise ChunkStream, which is
    why the multi-block tree validated yet crashed in-game.
    """
    nb = 1
    hdr_min = 0x10 + nb * 4
    offset = meta["offset"]
    if offset < hdr_min:
        # round up to a 0x800 boundary, like the original
        offset = ((hdr_min + 0x7FF) // 0x800) * 0x800
    out = bytearray()
    out += struct.pack("<IIII", MAGIC_A, offset, nb, len(payload))
    out += struct.pack("<I", len(payload))
    out += b"\x00" * (offset - len(out))
    out += payload
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
    land exactly on an 0xADDEADDE object separator.

    WARNING — this only sees the LAST bitmap of a multi-face run (an RndCubeTex
    serializes 6 face bitmaps back-to-back from the SHARED milo stream, and only
    the 6th lands on ADDE — see CubeTex.cpp:147-149 PostLoad). Stripping a bitmap
    this returns will corrupt a cube (one face half-res, five full-res). Use
    `find_bitmap_runs` for cube-aware stripping; this is kept for the census /
    self-check counting.
    """
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


def find_bitmap_runs(payload):
    """Scan the inflated payload for bitmap RUNS, each terminated by an 0xADDEADDE
    object separator.

    A *run* is one or more bitmaps serialized back-to-back into the same milo
    stream with no intervening separator, the run's last bitmap landing exactly on
    ADDE. A standalone RndTex bitmap is a run of length 1; an RndCubeTex serializes
    its 6 face bitmaps as one run of length 6 (CubeTex.cpp PostLoad loops
    `mBitmap[i].Load(bs)` from the shared stream, only the 6th followed by ADDE).

    Returns a list of runs, each a list of bitmap dicts. Stripping any member of a
    run with len > 1 would give the cube faces mismatched dimensions (the engine's
    RndCubeTex::ValidateBitmapProperties then fails → Reset() drops the cube; and a
    WebGPU cube texture requires all 6 faces equal-dimension), so callers MUST skip
    multi-member runs. The greedy chain is backtracked on any non-ADDE terminator,
    so it never over-groups (validated to match find_bitmaps on the standalone
    bitmaps of every venue).
    """
    n = len(payload)
    o = 0
    runs = []
    cur = []
    while o < n - 4:
        bm = parse_bitmap(payload, o)
        if bm:
            cur.append(bm)
            o = bm["end"]
            if payload[o : o + 4] == ADDE:
                runs.append(cur)
                cur = []
                o += 4
        else:
            if cur:
                # the open chain did not terminate on ADDE → it was a false chain;
                # restart the scan one byte past where it began.
                o = cur[0]["base"] + 1
                cur = []
            else:
                o += 1
    return runs


# ---------------------------------------------------------------------------
# Default per-bitmap exclusion (visual-gate exclusion list, A4 T2)
# ---------------------------------------------------------------------------
# The visual gate (research/12 T2) keeps these texture classes FULL-RES because
# the top-mip strip degrades them perceptibly for little byte win:
#   - BC5/DXN NORMAL MAPS (mOrder & 0x38 == 0x20): stripping the top mip softens
#     lighting/specular detail and risks shimmer.
#   - SMALL textures (max(w,h) <= threshold): the byte win lives in the 512^2-
#     1024^2 surfaces; small textures take the biggest SSIM hit for ~nothing.
#   - BC3-ALPHA detail/noise (mOrder & 0x38 == 0x18): high-frequency alpha content
#     (e.g. fine grain/noise) degrades most under SSIM.
# Cube faces are handled structurally (multi-member runs are never stripped), not
# by this predicate.
SMALL_MAX_DIM = 256  # keep max(w,h) <= this full-res (T2 recommends <=256)
ORDER_FMT_MASK = 0x38
FMT_BC3 = 0x18  # BC3 / DXT5 (alpha)
FMT_BC5 = 0x20  # BC5 / DXN (normal maps)


def default_exclude(bm):
    """Return True to KEEP this bitmap full-res (skip stripping), per the T2
    visual-gate exclusion list. `bm` is a parse_bitmap dict."""
    fmt = bm["order"] & ORDER_FMT_MASK
    if fmt == FMT_BC5:  # normal map
        return True
    if fmt == FMT_BC3:  # BC3-alpha detail/noise
        return True
    if max(bm["w"], bm["h"]) <= SMALL_MAX_DIM:
        return True
    return False


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
# Progressive-sharpen sidecar (A4 option C, research/13 T0)
# ---------------------------------------------------------------------------
# A4 ships venues with the TOP mip stripped (half-res base) so the venue reaches
# gameplay on a slow link. The sharpen sidecar carries the discarded high-res
# delta — the original top-mip BC bytes — so the engine can, in-session and in
# the background, restore each texture to full resolution: swap the RndBitmap's
# base level back to the full-res top-mip + recreate the GPU texture at the new
# (larger) size (see Rnd_Wgpu_RB3.cpp UploadRndTexIfNeeded). One `.sharpen` file
# per stripped venue (`<venue>.milo_xbox.sharpen`).
#
# On-disk format (all multi-byte fields LITTLE-endian to match the container
# header tooling; the embedded top-mip PIXEL bytes are copied VERBATIM from the
# milo, i.e. they keep the on-disk big-endian DXT word order the engine expects):
#
#   magic        4s   b"SHRP"
#   version      u32  == SHARPEN_VERSION (1)
#   levels       u32  top mip levels carried per entry (== the strip's --levels)
#   entry_count  u32
#   then entry_count records, each:
#     index            u32   stream-order ordinal among this venue's strippable
#                            standalone bitmaps (stable for a given milo)
#     full_w           u16   ORIGINAL (full-res) base level width
#     full_h           u16   ORIGINAL base height
#     full_rowbytes    u16   ORIGINAL base rowBytes
#     stripped_w       u16   the loaded (half-res) base width  == full_w>>levels
#     stripped_h       u16
#     stripped_rowbytes u16
#     bpp              u8
#     _pad             u8    (0)
#     order            u32   RndBitmap mOrder (format bits)
#     stripped_fp      u32   engine TexFingerprint() of the STRIPPED base bytes —
#                            the robust runtime match key: the engine recomputes
#                            it over each loaded RndTex's current pixels and looks
#                            up this entry (no name/order assumptions needed)
#     topmip_len       u32   bytes of the carried top-mip(s) (the high-res delta)
#     name_len         u32   length of the (best-effort) RndTex object name
#     name             name_len bytes (latin-1; "" when the dir-entry correlation
#                            is ambiguous — fingerprint is then authoritative)
#     topmip           topmip_len bytes — the ORIGINAL top-mip BC pixels, verbatim
#
# The sidecar is the ~75% of texture bytes A4 stripped; reported size vs the
# stripped venue confirms it carries the high-res delta. DO NOT COMMIT it (it is
# build output under the already-gitignored orig-assets/ tree).
SHARPEN_MAGIC = b"SHRP"
SHARPEN_VERSION = 1


def tex_fingerprint(data, off, size):
    """Replicate Rnd_Wgpu_RB3.cpp TexFingerprint() byte-for-byte so the engine
    can match a sidecar entry to a loaded RndTex by recomputing this over the
    live (stripped) bitmap pixels. Returns 0 for <16 bytes (engine does too)."""
    if size < 16:
        return 0
    h = 0
    step = size // 8
    if step < 1:
        step = 1
    i = 0
    while i < size:
        h = (h * 31 + data[off + i]) & 0xFFFFFFFF
        i += step
    return h


def _read_be_str(d, o):
    n = be32(d, o)
    if n > 512:
        raise ValueError("string too long")
    return d[o + 4 : o + 4 + n].decode("latin-1", "replace"), o + 4 + n


def parse_dir_entries(payload):
    """Best-effort parse of the milo ObjectDir entry table → ordered list of
    (className, objName). Mirrors dc3 validate_milo_entries.parse_directory_meta.
    Returns None on any structural surprise (caller then emits name="")."""
    try:
        o = 0
        rev = be32(payload, o); o += 4
        if rev > 50:
            return None
        if rev > 10:
            _t, o = _read_be_str(payload, o)  # dir type
            _n, o = _read_be_str(payload, o)  # dir name
            o += 4  # hash_count
            o += 4  # hash_size
            if rev >= 32:
                o += 1  # unknown bool
        ec = be32(payload, o); o += 4
        if ec > 100000:
            return None
        entries = []
        for _ in range(ec):
            ct, o = _read_be_str(payload, o)
            cn, o = _read_be_str(payload, o)
            entries.append((ct, cn))
        return entries
    except (ValueError, struct.error, IndexError, UnicodeError):
        return None


def _correlate_tex_names(payload, runs):
    """Map each STANDALONE bitmap run (in stream order) to an RndTex object name
    from the dir entry table, IF the correlation is unambiguous. Returns a dict
    {standalone_ordinal -> name}, empty when the counts don't line up (in which
    case the fingerprint is the only match key — safer than guessing a name)."""
    entries = parse_dir_entries(payload)
    if entries is None:
        return {}
    tex_names = [name for (cls, name) in entries if cls == "Tex"]
    n_standalone = sum(1 for r in runs if len(r) == 1)
    # Only trust the order-correlation when the count of standalone Tex bodies
    # exactly equals the count of "Tex" dir entries (cube faces are CubeTex
    # entries, handled structurally and never standalone).
    if len(tex_names) != n_standalone:
        return {}
    out = {}
    ordinal = 0
    for r in runs:
        if len(r) == 1:
            out[ordinal] = tex_names[ordinal]
            ordinal += 1
    return out


def build_sharpen_entries(payload, runs, levels=1, exclude=None,
                          apply_default_exclude=True):
    """Build the sidecar entry list for the SAME strippable set strip_file uses:
    single-member runs, numMips>=levels-eligible, not excluded. Each entry carries
    the full-res top-mip bytes + identity. `levels` mirrors strip_file's strip."""
    names = _correlate_tex_names(payload, runs)
    entries = []
    sidecar_index = 0
    standalone_ordinal = 0
    for r in sorted(runs, key=lambda rr: rr[0]["base"]):
        if len(r) != 1:
            continue
        ord_here = standalone_ordinal
        standalone_ordinal += 1
        bm = r[0]
        if bm["numMips"] < 1:
            continue
        if apply_default_exclude and default_exclude(bm):
            continue
        if exclude and exclude(bm):
            continue
        lv = min(levels, bm["numMips"])
        if lv <= 0:
            continue
        base = bm["base"]
        pb = bm["pb"]
        removed = sum(bm["sizes"][:lv])
        topmip_start = base + 32 + pb
        topmip = payload[topmip_start : topmip_start + removed]
        # the surviving base after the strip == mip[lv] — fingerprint it the way
        # the engine will fingerprint the loaded (stripped) bitmap's pixels.
        stripped_base_start = topmip_start + removed
        stripped_base_len = bm["sizes"][lv]
        stripped_fp = tex_fingerprint(payload, stripped_base_start, stripped_base_len)
        entries.append(dict(
            index=sidecar_index,
            full_w=bm["w"], full_h=bm["h"], full_rb=bm["rb"],
            stripped_w=bm["w"] >> lv, stripped_h=bm["h"] >> lv,
            stripped_rb=row_bytes(bm["w"] >> lv, bm["bpp"]),
            bpp=bm["bpp"], order=bm["order"], levels=lv,
            stripped_fp=stripped_fp, topmip=topmip,
            name=names.get(ord_here, ""),
        ))
        sidecar_index += 1
    return entries


def pack_sharpen_sidecar(entries, levels=1):
    """Serialize sidecar entries to the on-disk SHRP byte format (see header)."""
    out = bytearray()
    out += SHARPEN_MAGIC
    out += struct.pack("<III", SHARPEN_VERSION, levels, len(entries))
    for e in entries:
        name = e["name"].encode("latin-1", "replace")
        out += struct.pack(
            "<I HHH HHH BB I I I I",
            e["index"],
            e["full_w"], e["full_h"], e["full_rb"],
            e["stripped_w"], e["stripped_h"], e["stripped_rb"],
            e["bpp"], 0,
            e["order"], e["stripped_fp"],
            len(e["topmip"]), len(name),
        )
        out += name
        out += e["topmip"]
    return bytes(out)


def write_sharpen_sidecar(in_path, out_path, levels=1, quiet=False,
                          exclude=None, apply_default_exclude=True):
    """Read a full-res venue milo and write its `.sharpen` sidecar (the high-res
    top-mip delta for every bitmap strip_file would strip). Returns a stats dict."""
    payload, _meta = read_container(in_path)
    runs = find_bitmap_runs(payload)
    entries = build_sharpen_entries(payload, runs, levels=levels, exclude=exclude,
                                    apply_default_exclude=apply_default_exclude)
    blob = pack_sharpen_sidecar(entries, levels=levels)
    os.makedirs(os.path.dirname(os.path.abspath(out_path)) or ".", exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(blob)
    topmip_total = sum(len(e["topmip"]) for e in entries)
    named = sum(1 for e in entries if e["name"])
    if not quiet:
        print(
            f"  {os.path.basename(in_path)}: sharpen sidecar — {len(entries)} "
            f"textures, {topmip_total:,} top-mip bytes, {named} named; "
            f"sidecar {len(blob):,} bytes -> {os.path.basename(out_path)}"
        )
    return {
        "in": in_path, "out": out_path, "levels": levels,
        "n_entries": len(entries), "n_named": named,
        "topmip_bytes": topmip_total, "sidecar_bytes": len(blob),
    }


# ---------------------------------------------------------------------------
# Census
# ---------------------------------------------------------------------------
def census(path):
    payload, meta = read_container(path)
    runs = find_bitmap_runs(payload)
    bitmaps = [b for r in runs for b in r]
    # The STRIPPABLE set excludes cube faces (multi-member runs) AND the default
    # visual-gate exclusions (normal maps / BC3-alpha / small textures).
    cube_runs = [r for r in runs if len(r) > 1]
    standalone = [r[0] for r in runs if len(r) == 1]
    strippable = [b for b in standalone
                  if b["numMips"] >= 1 and not default_exclude(b)]
    total_tex = sum(32 + b["pb"] + sum(b["sizes"]) for b in bitmaps)
    strip_one = sum(b["sizes"][0] for b in strippable)
    n_strip = len(strippable)
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
        "num_cube_runs": len(cube_runs),
        "num_cube_faces": sum(len(r) for r in cube_runs),
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
def strip_file(in_path, out_path, levels=1, quiet=False, exclude=None,
               apply_default_exclude=True, sidecar_path=None):
    """Strip the top mip from eligible STANDALONE bitmaps and write a single-block
    container copy.

    Cube-safe + exclusion-aware:
      * `find_bitmap_runs` groups the bitmaps; a run with len > 1 is an RndCubeTex
        (6 face bitmaps) — its members are NEVER stripped (mismatched face dims
        break ValidateBitmapProperties + the WebGPU cube upload).
      * `default_exclude` (T2 visual-gate list: normal maps, BC3-alpha, small
        textures) keeps perceptually-fragile textures full-res. Pass
        `apply_default_exclude=False` to strip everything (census / A-B).
      * an extra caller `exclude(bm)` predicate is ANDed on top.
    """
    payload, meta = read_container(in_path)
    runs = find_bitmap_runs(payload)
    n_bitmaps = sum(len(r) for r in runs)
    n_cube_runs = sum(1 for r in runs if len(r) > 1)
    # Emit the progressive-sharpen sidecar (the discarded top-mip delta) BEFORE
    # the strip, from the still-full-res payload, so it carries the exact bytes
    # that get removed below. Same strippable selection as the strip itself.
    if sidecar_path is not None:
        entries = build_sharpen_entries(payload, runs, levels=levels, exclude=exclude,
                                        apply_default_exclude=apply_default_exclude)
        blob = pack_sharpen_sidecar(entries, levels=levels)
        os.makedirs(os.path.dirname(os.path.abspath(sidecar_path)) or ".", exist_ok=True)
        with open(sidecar_path, "wb") as fh:
            fh.write(blob)
    # Flatten the STRIPPABLE candidates: only single-member runs (standalone Tex),
    # never cube faces. Strip from the END backwards so earlier offsets stay valid.
    candidates = [r[0] for r in runs if len(r) == 1]
    total_removed = 0
    n_stripped = 0
    n_excluded = 0
    for bm in sorted(candidates, key=lambda b: -b["base"]):
        if bm["numMips"] < 1:
            continue
        if apply_default_exclude and default_exclude(bm):
            n_excluded += 1
            continue
        if exclude and exclude(bm):
            n_excluded += 1
            continue
        payload, removed = strip_bitmap_bytes(payload, bm, levels)
        total_removed += removed
        n_stripped += 1
    write_container(out_path, payload, meta)
    if not quiet:
        before = os.path.getsize(in_path)
        after = os.path.getsize(out_path)
        print(
            f"  {os.path.basename(in_path)}: stripped {n_stripped}/{n_bitmaps} "
            f"bitmaps ({n_excluded} excluded, {n_cube_runs} cube runs kept), "
            f"removed {total_removed:,} payload bytes; "
            f"file {before:,} -> {after:,} ({100*after/before:.1f}%)"
        )
    # round-trip self-check: re-parse the output. The run STRUCTURE must be
    # preserved (same number of runs, same cube-run face counts + uniform dims),
    # and the standalone bitmap count must match.
    out_payload, _ = read_container(out_path)
    out_runs = find_bitmap_runs(out_payload)
    if len(out_runs) != len(runs):
        raise SystemExit(
            f"ROUND-TRIP FAIL: re-parsed {len(out_runs)} runs, expected {len(runs)}"
        )
    for r in out_runs:
        if len(r) > 1:
            dims = set((b["w"], b["h"]) for b in r)
            if len(dims) != 1:
                raise SystemExit(
                    f"ROUND-TRIP FAIL: cube run has mixed face dims {sorted(dims)} "
                    f"(a cube face was stripped) in {os.path.basename(out_path)}"
                )
    return {
        "in": in_path, "out": out_path, "levels": levels,
        "n_bitmaps": n_bitmaps, "n_stripped": n_stripped,
        "n_excluded": n_excluded, "n_cube_runs": n_cube_runs,
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
    s.add_argument("--no-exclude", action="store_true",
                   help="strip EVERY standalone bitmap (ignore the T2 visual-gate "
                        "exclusion list); cube faces are still never stripped.")

    b = sub.add_parser("brotli", help="brotli-q11 wire size of a milo")
    b.add_argument("file")

    sh = sub.add_parser("sharpen",
                        help="emit the progressive-sharpen sidecar (high-res "
                             "top-mip delta) for a full-res venue milo")
    sh.add_argument("input", help="full-res .milo_xbox (the strip SOURCE)")
    sh.add_argument("output", help="output .sharpen sidecar path")
    sh.add_argument("--levels", type=int, default=1)
    sh.add_argument("--quiet", action="store_true")
    sh.add_argument("--no-exclude", action="store_true",
                    help="carry a delta for EVERY standalone bitmap (ignore the "
                         "T2 visual-gate exclusion list).")

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
            print(f"  cube runs / faces    : {rep['num_cube_runs']} / {rep['num_cube_faces']} "
                  f"(kept full-res, never stripped)")
            print(f"  strippable (post-exc): {rep['num_strippable']}")
            print(f"  texture bytes        : {rep['total_texture_bytes']:,}")
            print(f"  strippable top-mip   : {rep['strippable_top_mip_bytes']:,} "
                  f"({100*rep['strippable_fraction_of_texture']:.1f}% of texture bytes)")
            print(f"  numMips distribution : {rep['numMips_distribution']}")
            print(f"  format counts        : {rep['format_counts']}")
    elif args.cmd == "strip":
        strip_file(args.input, args.output, levels=args.levels, quiet=args.quiet,
                   apply_default_exclude=not args.no_exclude)
    elif args.cmd == "brotli":
        print(f"{os.path.basename(args.file)}: brotli-q11 = {brotli_size(args.file):,} bytes")
    elif args.cmd == "sharpen":
        write_sharpen_sidecar(args.input, args.output, levels=args.levels,
                              quiet=args.quiet,
                              apply_default_exclude=not args.no_exclude)


if __name__ == "__main__":
    main()
