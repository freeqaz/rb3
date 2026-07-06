#!/usr/bin/env python3
"""
songsel-sliver-probe.py — W4.1 subitem (b): identify the thin dark-red vertical
"sliver" noted in the 2026-07-02 visdiff / Wave-5 memory summary near the right
edge of the song_select list (approx x=882-895).

Verdict (see W4.1/STATUS.md C.S2-b): it is the list's proportional SCROLLBAR
THUMB, correctly rendered, matching retail — NOT a bug. No fix landed.

Method: capture native song_select at several scroll depths (reusing
scripts/native/song-select-capture.py), then scan a fixed screen column
(x=893) for the sliver's dark-red pixels and report its y-range at each depth.
A *scrollbar thumb* moves a small, roughly-linear amount per scroll step
(much slower than the ~22-26px per-row list scroll) because it represents
position within the *whole* list (~109 entries); a highlighted-row artifact
would instead track the yellow highlight bar 1:1. This probe checks both.

Usage (after `python3 scripts/native/song-select-capture.py --depths ... --out DIR`,
or standalone — it will run the capture itself if --out doesn't already have
the expected files):

    python3 songsel-sliver-probe.py --shots-dir /tmp/wave6-ss-sliver-check \
        --depths 0,10,20,30,50,70
"""
import argparse, os, subprocess, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "..", "..", ".."))
CAPTURE = os.path.join(REPO, "scripts", "native", "song-select-capture.py")

RED_COL_X = 893
YELLOW_COL_X = 50


def scan_column(png_path, x, is_red):
    from PIL import Image
    im = Image.open(png_path)
    hits = []
    for y in range(90, 600):
        px = im.getpixel((x, y))
        if is_red:
            if px[0] > 90 and px[1] < 60 and px[2] < 40:
                hits.append(y)
        else:
            if px[0] > 200 and px[1] > 180 and px[2] < 80:
                hits.append(y)
    return (min(hits), max(hits)) if hits else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--shots-dir", default="/tmp/wave6-ss-sliver-check")
    ap.add_argument("--depths", default="0,10,20,30,50,70")
    ap.add_argument("--skip-capture", action="store_true",
                     help="assume --shots-dir already has native_depth_NN.png files")
    args = ap.parse_args()

    depths = sorted(int(x) for x in args.depths.split(",") if x.strip())

    if not args.skip_capture:
        os.makedirs(args.shots_dir, exist_ok=True)
        rc = subprocess.call([sys.executable, CAPTURE, "--depths", args.depths,
                               "--out", args.shots_dir, "--verbose"])
        if rc != 0:
            print(f"[sliver-probe] FAIL: capture script exited {rc}")
            return 1

    print(f"{'depth':>6} {'sliver_y':>14} {'highlight_y':>14}  sliver_step  hl_step")
    prev_sliver = prev_hl = None
    for d in depths:
        path = os.path.join(args.shots_dir, f"native_depth_{d:02d}.png")
        if not os.path.exists(path):
            print(f"[sliver-probe] missing {path}, skipping"); continue
        sliver = scan_column(path, RED_COL_X, True)
        hl = scan_column(path, YELLOW_COL_X, False)
        sstep = (sliver[0] - prev_sliver[0]) if (sliver and prev_sliver) else None
        hstep = (hl[0] - prev_hl[0]) if (hl and prev_hl) else None
        print(f"{d:>6} {str(sliver):>14} {str(hl):>14}  {str(sstep):>11}  {str(hstep):>7}")
        prev_sliver, prev_hl = sliver or prev_sliver, hl or prev_hl

    print()
    print("Interpretation: if sliver_step is small (~2px) and roughly constant per")
    print("scroll step while highlight_y jumps/holds independently, the sliver is a")
    print("scrollbar thumb (position proportional to the FULL list), not tied to the")
    print("highlighted row or any single mesh slot. Cross-check against a retail")
    print("screenshot near the end of an alphabetized list (images/retail-screenshots/"
          "yt_qRagnZCIMzk_song_select_diff_ratings.png) — retail shows the same red")
    print("mark near the BOTTOM of the track when browsing late letters (V-Y), vs near")
    print("the TOP when browsing early letters (A-B) in the album-art capture — i.e.")
    print("retail has the identical element.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
