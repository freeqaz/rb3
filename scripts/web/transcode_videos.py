#!/usr/bin/env python3
"""Transcode RB3 fullscreen cinematics (Bink .bik -> WebM VP9+Opus) for the web build.

The web intro/credits play through a browser <video> overlay (see
native/src/rb3_movie_native.cpp); browsers can't decode Bink, so we pre-transcode
the .bik to .webm (VP9 video + Opus audio). The engine rewrites the .bik filename
to .webm at runtime, and native/web/server.py serves the sidecar from the
extracted-xbox-full fallback root with HTTP range requests — so the .webm just
needs to live next to its .bik source.

This mirrors dc3-decomp/scripts/web/transcode_bink.py but is scoped to the two
fullscreen cinematics RB3 actually plays (the in-world TexMovie videos stay
no-op on web). Output is a gitignored derived artifact (orig-assets/), so this
script is how a fresh checkout reproduces it.

Usage:
    scripts/web/transcode_videos.py                 # transcode missing sidecars
    scripts/web/transcode_videos.py --force         # re-encode even if present
    scripts/web/transcode_videos.py --crf 28        # higher quality (bigger)
    scripts/web/transcode_videos.py --all-roots     # also wii-extracted source
"""
import argparse, os, shutil, subprocess, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# The fullscreen cinematics MoviePanel plays. Source is preferred from the Xbox
# extraction (720p) and falls back to the Wii extraction.
CINEMATICS = ["rb3_intro_cinematic.bik", "rb3_end_credits.bik"]
SOURCE_ROOTS = [
    os.path.join(REPO, "orig-assets", "extracted-xbox-full", "videos"),
    os.path.join(REPO, "orig-assets", "wii-extracted", "videos"),
]


def have_ffmpeg():
    return shutil.which("ffmpeg") is not None


def transcode(src, dst, crf, cpu_used):
    tmp = dst + ".partial.webm"
    cmd = [
        "ffmpeg", "-y", "-v", "warning", "-stats", "-i", src,
        "-map", "0:v:0", "-c:v", "libvpx-vp9",
        "-crf", str(crf), "-b:v", "0",
        "-deadline", "good", "-cpu-used", str(cpu_used),
        "-row-mt", "1", "-tile-columns", "2", "-threads", "8",
        "-pix_fmt", "yuv420p",
        "-map", "0:a:0?", "-c:a", "libopus", "-b:a", "128k",
        "-f", "webm", tmp,
    ]
    print(f"  ffmpeg: {os.path.basename(src)} -> {os.path.basename(dst)} (crf={crf})")
    r = subprocess.run(cmd, cwd=REPO)
    if r.returncode != 0:
        if os.path.exists(tmp):
            os.remove(tmp)
        return False
    os.replace(tmp, dst)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true", help="re-encode even if .webm exists")
    ap.add_argument("--crf", type=int, default=32, help="VP9 quality (lower=better/bigger; default 32)")
    ap.add_argument("--cpu-used", type=int, default=4, help="VP9 speed 0..8 (higher=faster/worse; default 4)")
    ap.add_argument("--all-roots", action="store_true",
                    help="also transcode the wii-extracted source root")
    args = ap.parse_args()

    if not have_ffmpeg():
        print("FAIL: ffmpeg not found on PATH"); return 1

    roots = SOURCE_ROOTS if args.all_roots else SOURCE_ROOTS[:1]
    did, skipped, failed = 0, 0, 0
    for root in roots:
        if not os.path.isdir(root):
            continue
        for bik in CINEMATICS:
            src = os.path.join(root, bik)
            if not os.path.isfile(src):
                continue
            dst = os.path.splitext(src)[0] + ".webm"
            if os.path.isfile(dst) and not args.force:
                print(f"  skip (exists): {os.path.relpath(dst, REPO)}")
                skipped += 1
                continue
            if transcode(src, dst, args.crf, args.cpu_used):
                sz = os.path.getsize(dst)
                print(f"  OK: {os.path.relpath(dst, REPO)} ({sz/1e6:.1f} MB)")
                did += 1
            else:
                print(f"  FAIL: {os.path.relpath(src, REPO)}")
                failed += 1

    print(f"\ntranscoded={did} skipped={skipped} failed={failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
