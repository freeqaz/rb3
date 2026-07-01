#!/usr/bin/env python3
"""Transcode the Wii crowd-audio Bink streams (.bik) -> unencrypted .mogg so the
native/web port has venue/crowd intro audio during the song-start window.

WHY
---
On the real RB3, the ~negative-clock song-intro window (venue/band cinematic,
several seconds before the song stems start at clock 0) is filled by CrowdAudio:
`CrowdAudio::OnIntro` -> `PlayLoop("crowd_intro.mogg")` -> `BinkClip::Play` ->
`Synth::NewStream` (the native VorbisReader mogg path). On native that intro is
SILENT because two asset gaps stack up (see
docs/native/songstart-2sec-2026-06-21/CROWDAUDIO_DIAGNOSIS.md):
  1. the Xbox venue milo the native port loads has ZERO `BinkClip` objects, and
  2. the crowd stream payloads (`world/venue/<class>/streams/crowd_*.mogg`) are
     not in the native `extracted` tree.

The ONLY crowd stream payloads that exist in any extracted asset tree are the
three Wii `crowd_<class>_intro.bik` files (small_club, big_club, arena) — the
excitement loops (norm/good/peak/...) were never extracted. So this script
transcodes those three intro biks into the format the native VorbisReader can
play, and drops them where the engine's file resolution expects them:
`world/venue/<class>/streams/crowd_<class>_intro.mogg`.

A native HX_NATIVE bridge in CrowdAudio::PlayLoop synthesizes the missing
`crowd_intro.mogg` BinkClip on the fly (its mFile -> `crowd_<class>_intro.bik`,
which BinkClip::Play strips to `crowd_<class>_intro` + Synth appends `.mogg`),
so the engine streams the file this script produces.

FORMAT
------
RB3 "mogg" = a tiny little-endian header followed by the raw Ogg/Vorbis stream:
    u32 version        (10 = UNENCRYPTED — no AES-CTR, no HMXA scan)
    u32 hdrSize        (byte offset to the start of the Ogg data)
    --- OggMap (VorbisReader's mMap.Read) ---
    u32 mapVersion     (>= 0x0B)
    u32 mGran          (samples per lookup bucket; 1000)
    u32 lookupCount    (std::vector length)
    (i32 bytePos, i32 sampPos) * lookupCount
    --- raw Ogg/Vorbis at offset hdrSize ---
Version 10 means VorbisReader::CheckHmxHeader skips ALL crypto (mCtrState stays
null, magicHash stays 0), so ::Decrypt is a no-op and the Ogg pages pass through
untouched. This is the simplest mogg the native path accepts.

Output is a gitignored derived artifact (orig-assets/), so this script is how a
fresh checkout reproduces the crowd-intro moggs.

Usage:
    scripts/native/transcode_crowd_audio.py            # produce missing moggs
    scripts/native/transcode_crowd_audio.py --force    # re-encode even if present
    scripts/native/transcode_crowd_audio.py --quality 6
"""
import argparse, os, struct, subprocess, sys, shutil, tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# Source biks (Wii) -> destination moggs (native extracted tree). The destination
# basename is what CrowdAudio's native bridge points the synthesized BinkClip at.
VENUES = [
    ("small_club", "crowd_small_intro"),
    ("big_club", "crowd_big_intro"),
    ("arena", "crowd_arena_intro"),
]
SRC_ROOT = os.path.join(REPO, "orig-assets", "wii-extracted", "world", "venue")
DST_ROOT = os.path.join(REPO, "orig-assets", "extracted", "world", "venue")


def have(tool):
    return shutil.which(tool) is not None


def bik_to_ogg(src, ogg_path, quality):
    """Decode the bik's binkaudio stream and re-encode as Ogg/Vorbis."""
    cmd = [
        "ffmpeg", "-y", "-v", "error", "-nostdin",
        "-i", src,
        "-vn",                       # drop the dummy 4x4 bink video
        "-c:a", "libvorbis", "-q:a", str(quality),
        "-f", "ogg", ogg_path,
    ]
    r = subprocess.run(cmd, cwd=REPO)
    return r.returncode == 0 and os.path.isfile(ogg_path) and os.path.getsize(ogg_path) > 0


def wrap_mogg(ogg_bytes):
    """Wrap raw Ogg bytes in a minimal version-10 (unencrypted) RB3 mogg."""
    # OggMap: version 0x0B, gran 1000, one lookup entry (0,0).
    oggmap = struct.pack("<iii", 0x0B, 1000, 1)      # version, gran, lookupCount
    oggmap += struct.pack("<ii", 0, 0)               # lookup[0] = (bytePos, sampPos)
    # mogg header: version 10, hdrSize = 8 (the two leading u32s) + len(oggmap).
    hdr_size = 8 + len(oggmap)
    header = struct.pack("<ii", 10, hdr_size) + oggmap
    assert len(header) == hdr_size, (len(header), hdr_size)
    return header + ogg_bytes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true", help="re-encode even if .mogg exists")
    ap.add_argument("--quality", type=int, default=5, help="libvorbis -q:a (default 5)")
    args = ap.parse_args()

    if not have("ffmpeg"):
        print("FAIL: ffmpeg not found on PATH"); return 1

    did = skipped = failed = 0
    for venue, base in VENUES:
        src = os.path.join(SRC_ROOT, venue, "streams", base + ".bik")
        dst = os.path.join(DST_ROOT, venue, "streams", base + ".mogg")
        rel_dst = os.path.relpath(dst, REPO)
        if not os.path.isfile(src):
            print(f"  skip (no source bik): {os.path.relpath(src, REPO)}")
            skipped += 1
            continue
        if os.path.isfile(dst) and not args.force:
            print(f"  skip (exists): {rel_dst}")
            skipped += 1
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with tempfile.NamedTemporaryFile(suffix=".ogg", delete=False) as tf:
            ogg_tmp = tf.name
        try:
            if not bik_to_ogg(src, ogg_tmp, args.quality):
                print(f"  FAIL: ffmpeg decode {os.path.relpath(src, REPO)}")
                failed += 1
                continue
            mogg = wrap_mogg(open(ogg_tmp, "rb").read())
            tmp_dst = dst + ".partial"
            with open(tmp_dst, "wb") as f:
                f.write(mogg)
            os.replace(tmp_dst, dst)
            print(f"  OK: {rel_dst} ({len(mogg)/1e3:.1f} KB)")
            did += 1
        finally:
            if os.path.exists(ogg_tmp):
                os.remove(ogg_tmp)

    print(f"\ndone: {did} written, {skipped} skipped, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
