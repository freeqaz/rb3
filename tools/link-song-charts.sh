#!/bin/bash
#
# link-song-charts.sh — wire the full on-disc song set into the native loader root.
#
# The native loader reads songs from `orig-assets/extracted/songs`, but a dev
# extract often ships only a few songs' charts there while the FULL 83-song
# library lives in `orig-assets/extracted-xbox-full/songs` (identical layout:
# songs/<id>/{<id>.mid, <id>.mogg, gen/<id>.milo_xbox}). This script symlinks the
# missing per-song assets from the full extract into the loader root so every
# song LOADS AND PLAYS. Symlinks, never copies — the moggs are multi-GB.
#
# `orig-assets/*` is gitignored (machine-local), so this can't be committed as
# data and must run per-machine. tools/setup-worktree.sh calls it automatically
# (worktrees share the main tree's `extracted/` via a symlink, so populating the
# main extract benefits every worktree); run it standalone after a fresh checkout
# or a re-extract:
#
#   tools/link-song-charts.sh              # default: this repo's orig-assets
#   tools/link-song-charts.sh /path/orig-assets
#
# Idempotent: only creates a symlink where the target is missing; safe to re-run.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"
ASSETS="${1:-$REPO/orig-assets}"

SRC="$ASSETS/extracted-xbox-full/songs"   # the full 83-song set (xbox flavor; native resolves .milo_xbox)
DST="$ASSETS/extracted/songs"             # the loader root

if [ ! -d "$SRC" ]; then
    echo "link-song-charts: full extract not found at $SRC — skipping (only the dev-slice songs will be available)."
    exit 0
fi
if [ ! -d "$DST" ]; then
    echo "link-song-charts: loader song dir not found at $DST — skipping."
    exit 0
fi

# Album-art cover suffix. Native forces kPlatformXBox, so the cooked texture is
# gen/<id>_keep.png_xbox; BandSongMgr::GetAlbumArtPath stats that exact path.
ART_SUFFIX="_keep.png_xbox"

linked_dirs=0
linked_files=0
linked_art=0
for sd in "$SRC"/*/; do
    s="$(basename "$sd")"
    [ "$s" = gen ] && continue
    [ -e "$sd" ] || continue

    if [ ! -e "$DST/$s" ] && [ ! -L "$DST/$s" ]; then
        # Whole song absent from the loader root → symlink the entire song dir
        # (brings .mid + .mogg + gen/.milo_xbox + album art in one shot).
        ln -s "$sd%/" "$DST/$s" 2>/dev/null || ln -s "${sd%/}" "$DST/$s"
        linked_dirs=$((linked_dirs + 1))
        continue
    fi

    # Dir exists (real, with gen/milo) → ensure each per-song asset is present.
    # .mid is the usual gap (charts stripped from the dev slice); .mogg is a
    # safety net (audio is sometimes wired separately).
    for ext in mid mogg; do
        t="$DST/$s/$s.$ext"
        src="$SRC/$s/$s.$ext"
        if [ ! -e "$t" ] && [ ! -L "$t" ] && [ -e "$src" ]; then
            ln -s "$src" "$t"
            linked_files=$((linked_files + 1))
        fi
    done

    # Per-song album-art cover. The cooked texture lives at
    # gen/<id>_keep.png_xbox; the dev slice ships the .milo_xbox + audio but not
    # this, so the song-select grid falls back to the blank "?" placeholder.
    # ~81/86 songs have an xbox cover; the rest (e.g. ifeelgoodalt, radarlove)
    # have none and correctly stay blank. Symlink it into the loader root.
    art="$DST/$s/gen/$s$ART_SUFFIX"
    art_src="$SRC/$s/gen/$s$ART_SUFFIX"
    if [ ! -e "$art" ] && [ ! -L "$art" ] && [ -e "$art_src" ]; then
        mkdir -p "$DST/$s/gen"
        ln -s "$art_src" "$art"
        linked_art=$((linked_art + 1))
    fi
done

total_mid="$(find -L "$DST" -maxdepth 2 -name '*.mid' 2>/dev/null | wc -l | tr -d ' ')"
total_art="$(find -L "$DST" -maxdepth 3 -name "*$ART_SUFFIX" 2>/dev/null | wc -l | tr -d ' ')"
echo "link-song-charts: ${DST#$ASSETS/} now has ${total_mid} charts + ${total_art} covers (linked ${linked_dirs} dirs + ${linked_files} files + ${linked_art} covers this run)."
