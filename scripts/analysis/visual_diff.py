#!/usr/bin/env python3
"""visual_diff.py — the ONE canonical render-diff for the RB3 native/web port.

Mirrors the philosophy of westworld's `tools/pngdiff.py` + `cmd/renderdiff`
(per-pixel max-channel absolute delta, configurable LSB tolerance, %differing,
max delta, differing-region bbox), but vectorized with numpy so it's fast enough
to run in CI / agent loops on 1280x720 frames (the pure-Python double loop in
pngdiff.py is far too slow at that resolution).

Two modes, one tool:

  STRICT (default) — build-vs-build A/B. Both images MUST be the same size.
    A pixel "differs" when its max per-channel |Δ| exceeds --tol (so a <=N-LSB
    rounding wobble is tolerated). Reports max(R,G,B) delta, %differing, the
    largest single-channel |Δ|, and the bbox of the differing region. This is
    the gate you run to prove a GPU-backend change (e.g. the per-mesh buffer
    cache) did not regress rendering vs the current build.

  PERCEPTUAL (--perceptual) — reference-photo sanity. Different renderer / AA /
    resolution means exact pixels are impossible, so we downscale BOTH to a
    common grid and compute a resolution-agnostic 0..100 similarity score by
    blending three translation/AA-tolerant terms: translation-tolerant STRUCTURE
    (regional edge-energy + coarse gradient correlation), windowed grayscale
    SSIM, and block-mean colour delta — plus a content-presence guard that caps
    the score when one side is near-flat (the "screen never rendered" case). It
    answers "did the song list / characters / HUD render AT ALL" without
    false-failing on anti-aliasing. It is a "rendered-vs-blank" gate with an
    advisory similarity score, NOT a precise same-screen classifier: a low score
    (< ~35) means almost certainly blank/garbage/wrong-screen; a high score means
    structurally present and roughly laid out like the reference.

Usage:
    visual_diff.py A.png B.png [--tol N] [--threshold PCT] [--label NAME]
                                [--heatmap OUT.png] [--json]
    visual_diff.py A.png REF.png --perceptual [--min-score S] [--json]

Exit codes (so other scripts/agents can gate on it):
    0  PASS  — strict: %differing <= threshold (or byte-identical).
              perceptual: score >= --min-score.
    1  FAIL  — strict: too many differing pixels. perceptual: score below min.
    2  ERROR — bad args, missing/size-mismatch (strict) etc.

The LAST line of stdout is always a one-line machine-readable summary
(`VISUAL_DIFF mode=... verdict=PASS ...`); `--json` additionally prints a JSON
object on the line before it. Importable too: `from visual_diff import
diff_strict, diff_perceptual` return dataclasses.
"""
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass, asdict

import numpy as np
from PIL import Image


# ---------------------------------------------------------------------------
# Image loading
# ---------------------------------------------------------------------------

def load_rgb(path: str) -> np.ndarray:
    """Load a PNG/JPG as an HxWx3 uint8 RGB array (alpha flattened onto black)."""
    im = Image.open(path)
    if im.mode in ("RGBA", "LA", "P"):
        im = im.convert("RGBA")
        bg = Image.new("RGBA", im.size, (0, 0, 0, 255))
        im = Image.alpha_composite(bg, im).convert("RGB")
    else:
        im = im.convert("RGB")
    return np.asarray(im, dtype=np.uint8)


# ---------------------------------------------------------------------------
# Strict mode — build-vs-build (the canonical pixel diff)
# ---------------------------------------------------------------------------

@dataclass
class StrictResult:
    mode: str             # "strict"
    width: int
    height: int
    total: int            # total pixels
    differing: int        # pixels whose max channel |Δ| > tol
    pct_differing: float  # 100 * differing / total
    tolerance: int        # the LSB tolerance applied
    max_r: int
    max_g: int
    max_b: int
    max_delta: int        # largest single-channel |Δ| anywhere
    # bbox of differing region (inclusive); all -1 when nothing differs
    bbox_min_x: int
    bbox_min_y: int
    bbox_max_x: int
    bbox_max_y: int
    threshold_pct: float  # the PASS/FAIL threshold on pct_differing
    verdict: str          # "PASS" | "FAIL"

    def summary(self) -> str:
        bbox = (
            "none" if self.bbox_min_x < 0
            else f"[{self.bbox_min_x}..{self.bbox_max_x},{self.bbox_min_y}..{self.bbox_max_y}]"
        )
        return (
            f"VISUAL_DIFF mode=strict verdict={self.verdict} "
            f"size={self.width}x{self.height} tol={self.tolerance} "
            f"maxRGB={self.max_r}/{self.max_g}/{self.max_b} maxd={self.max_delta} "
            f"ndiff={self.differing}/{self.total} pct={self.pct_differing:.4f} "
            f"thresh={self.threshold_pct:.4f} bbox={bbox}"
        )


def diff_strict(a: np.ndarray, b: np.ndarray, tol: int = 2,
                threshold_pct: float = 0.10) -> StrictResult:
    """Vectorized per-pixel max-channel |Δ| diff. `a`/`b` must be equal-size RGB."""
    if a.shape != b.shape:
        raise ValueError(f"size mismatch {a.shape[1]}x{a.shape[0]} vs {b.shape[1]}x{b.shape[0]}")
    h, w = a.shape[:2]
    ai = a.astype(np.int16)
    bi = b.astype(np.int16)
    absd = np.abs(ai - bi)                  # HxWx3
    per_channel_max = absd.max(axis=(0, 1)) # [maxR, maxG, maxB]
    pixel_max = absd.max(axis=2)            # HxW: max channel delta per pixel
    differ_mask = pixel_max > tol

    differing = int(differ_mask.sum())
    total = int(h * w)
    max_delta = int(pixel_max.max()) if total else 0

    if differing:
        ys, xs = np.nonzero(differ_mask)
        bbox = (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))
    else:
        bbox = (-1, -1, -1, -1)

    pct = (100.0 * differing / total) if total else 0.0
    verdict = "PASS" if pct <= threshold_pct else "FAIL"
    return StrictResult(
        mode="strict", width=w, height=h, total=total, differing=differing,
        pct_differing=pct, tolerance=tol,
        max_r=int(per_channel_max[0]), max_g=int(per_channel_max[1]),
        max_b=int(per_channel_max[2]), max_delta=max_delta,
        bbox_min_x=bbox[0], bbox_min_y=bbox[1], bbox_max_x=bbox[2], bbox_max_y=bbox[3],
        threshold_pct=threshold_pct, verdict=verdict,
    )


def write_heatmap(a: np.ndarray, b: np.ndarray, tol: int, out_path: str) -> None:
    """Diff heatmap: in-tolerance pixels = dim grayscale of A (context),
    out-of-tolerance = bright magenta scaled by delta magnitude. Mirrors
    renderdiff's Heatmap()."""
    h, w = a.shape[:2]
    ai = a.astype(np.int16)
    bi = b.astype(np.int16)
    pixel_max = np.abs(ai - bi).max(axis=2)        # HxW
    differ = pixel_max > tol

    # context: dimmed grayscale luminance of A
    lum = (a[..., 0] * 30 + a[..., 1] * 59 + a[..., 2] * 11) // 100 // 3
    out = np.zeros((h, w, 3), dtype=np.uint8)
    out[..., 0] = lum
    out[..., 1] = lum
    out[..., 2] = lum
    # highlight: magenta intensity = clamped delta
    mag = np.clip(pixel_max, 0, 255).astype(np.uint8)
    out[differ, 0] = mag[differ]
    out[differ, 1] = 0
    out[differ, 2] = mag[differ]
    Image.fromarray(out, "RGB").save(out_path)


# ---------------------------------------------------------------------------
# Perceptual mode — reference-photo sanity (resolution-agnostic)
# ---------------------------------------------------------------------------

@dataclass
class PerceptualResult:
    mode: str               # "perceptual"
    grid_w: int             # common downscale grid the score is computed on
    grid_h: int
    a_size: str             # original WxH of A
    b_size: str             # original WxH of B
    struct_score: float     # 0..100 from gradient-magnitude correlation (layout)
    block_score: float      # 0..100 from block-mean abs delta (colour)
    ssim_score: float       # 0..100 from grayscale SSIM (windowed)
    detail_ratio: float     # min/max edge-energy ratio (content-presence guard)
    score: float            # 0..100 blended similarity
    min_score: float        # PASS threshold
    verdict: str            # "PASS" | "FAIL"

    def summary(self) -> str:
        return (
            f"VISUAL_DIFF mode=perceptual verdict={self.verdict} "
            f"score={self.score:.1f} struct={self.struct_score:.1f} "
            f"ssim={self.ssim_score:.1f} block={self.block_score:.1f} "
            f"detail_ratio={self.detail_ratio:.2f} min={self.min_score:.1f} "
            f"grid={self.grid_w}x{self.grid_h} a={self.a_size} b={self.b_size}"
        )


def _downscale(a: np.ndarray, gw: int, gh: int) -> np.ndarray:
    """Area-average downscale to gw x gh, returning float64 RGB. PIL BOX gives a
    proper area filter (averages source pixels) so AA/resolution differences are
    absorbed into block means."""
    im = Image.fromarray(a, "RGB").resize((gw, gh), Image.BOX)
    return np.asarray(im, dtype=np.float64)


def _gray(a: np.ndarray) -> np.ndarray:
    return a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114


def _ssim(x: np.ndarray, y: np.ndarray, win: int = 7) -> float:
    """Mean windowed SSIM over two equal-size grayscale float images (0..255).
    Plain numpy (no scipy): uniform-box local stats via cumulative-sum, so it's
    fast and dependency-free. Returns mean SSIM in [-1, 1]."""
    h, w = x.shape
    win = max(3, min(win, h, w))
    if win % 2 == 0:
        win -= 1
    C1 = (0.01 * 255) ** 2
    C2 = (0.03 * 255) ** 2

    def boxsum(img: np.ndarray) -> np.ndarray:
        # sum over win x win windows via 2D prefix sums (valid region)
        cs = np.cumsum(np.cumsum(img, axis=0), axis=1)
        cs = np.pad(cs, ((1, 0), (1, 0)), mode="constant")
        out = (cs[win:, win:] - cs[:-win, win:] - cs[win:, :-win] + cs[:-win, :-win])
        return out

    n = win * win
    mux = boxsum(x) / n
    muy = boxsum(y) / n
    muxx = boxsum(x * x) / n
    muyy = boxsum(y * y) / n
    muxy = boxsum(x * y) / n
    vx = muxx - mux * mux
    vy = muyy - muy * muy
    cxy = muxy - mux * muy
    ssim_map = ((2 * mux * muy + C1) * (2 * cxy + C2)) / (
        (mux * mux + muy * muy + C1) * (vx + vy + C2))
    return float(np.clip(ssim_map.mean(), -1.0, 1.0))


def _grad_mag(g: np.ndarray) -> np.ndarray:
    """Gradient magnitude of a grayscale image (forward differences, same shape)."""
    gx = np.zeros_like(g)
    gy = np.zeros_like(g)
    gx[:, :-1] = np.diff(g, axis=1)
    gy[:-1, :] = np.diff(g, axis=0)
    return np.sqrt(gx * gx + gy * gy)


def _ncc(x: np.ndarray, y: np.ndarray) -> float:
    """Normalized cross-correlation of two equal-size arrays, in [-1, 1].
    Returns 0 if either side is (near-)constant — the key property that makes a
    flat/blank frame score 0 against a textured one."""
    xf = x.ravel().astype(np.float64)
    yf = y.ravel().astype(np.float64)
    xf = xf - xf.mean()
    yf = yf - yf.mean()
    nx = np.sqrt((xf * xf).sum())
    ny = np.sqrt((yf * yf).sum())
    if nx < 1e-6 or ny < 1e-6:
        return 0.0
    return float(np.clip((xf * yf).sum() / (nx * ny), -1.0, 1.0))


def _region_energy(g: np.ndarray, ny: int = 6, nx: int = 8) -> np.ndarray:
    """Per-region edge-energy map: split grayscale `g` into an ny x nx grid and
    take each region's local detail (std of gradient magnitude). This is
    TRANSLATION-TOLERANT — it asks "is the list region busy / is the art panel
    textured / does the header have content", not "is this exact edge here". So
    two different song lists (text at different rows) still correlate, while a
    blank region reads as zero energy."""
    gm = _grad_mag(g)
    h, w = gm.shape
    out = np.zeros((ny, nx), dtype=np.float64)
    ys = np.linspace(0, h, ny + 1).astype(int)
    xs = np.linspace(0, w, nx + 1).astype(int)
    for j in range(ny):
        for i in range(nx):
            block = gm[ys[j]:ys[j + 1], xs[i]:xs[i + 1]]
            out[j, i] = block.mean() if block.size else 0.0
    return out


def diff_perceptual(a: np.ndarray, b: np.ndarray, grid: int = 64,
                    min_score: float = 35.0) -> PerceptualResult:
    """Resolution-agnostic similarity in [0,100]. Downscales both to a common
    grid (preserving aspect), then scores STRUCTURE first so the dominant signal
    is "did comparable content render at all" — a flat/blank frame against a
    textured one scores near 0, even though both may be mostly dark.

    Components:
      - structure: normalized cross-correlation of the two gradient-magnitude
        maps (edges/layout). Zero for a blank frame. This is the heavy term.
      - ssim: windowed grayscale SSIM (luminance+contrast+structure).
      - block: mean per-channel abs delta (colour honesty), minor.
    A content-presence guard hard-caps the score when one side has detail and
    the other is near-flat (the classic "screen never rendered" failure)."""
    ah, aw = a.shape[:2]
    bh, bw = b.shape[:2]
    # common grid: longest side = `grid`, aspect from A (both forced to it)
    aspect = aw / ah
    if aspect >= 1.0:
        gw, gh = grid, max(1, round(grid / aspect))
    else:
        gw, gh = max(1, round(grid * aspect)), grid

    da = _downscale(a, gw, gh)
    db = _downscale(b, gw, gh)
    ga, gb = _gray(da), _gray(db)

    # structure: the "is comparable content laid out in roughly the right places"
    # term — the dominant signal for "did this render at all". We blend two
    # translation-tolerant layout measures so the score isn't brittle to either:
    #   - REGIONAL edge-energy NCC: split into a 6x8 region grid, correlate
    #     per-region detail. Two different song lists still correlate (the list
    #     region is busy in both, the art panel textured in both); a blank region
    #     reads zero energy.
    #   - COARSE gradient-magnitude NCC at ~24px: gross edge layout, jitter washed
    #     out.
    # Either alone is brittle on one screen class or another; the mean is steady.
    cside = 24
    if aspect >= 1.0:
        cw, ch = cside, max(1, round(cside / aspect))
    else:
        cw, ch = max(1, round(cside * aspect)), cside
    ca = _gray(_downscale(a, cw, ch))
    cb = _gray(_downscale(b, cw, ch))
    region_ncc = _ncc(_region_energy(ga), _region_energy(gb))
    grad_ncc = _ncc(_grad_mag(ca), _grad_mag(cb))
    struct_score = max(0.0, 100.0 * 0.5 * (region_ncc + grad_ncc))

    # ssim (luminance + contrast + structure).
    ssim = _ssim(ga, gb)
    ssim_score = max(0.0, 50.0 * (ssim + 1.0))   # [-1,1] -> [0,100]

    # block-mean colour delta.
    block_delta = float(np.abs(da - db).mean())
    block_score = max(0.0, 100.0 * (1.0 - block_delta / 255.0))

    # blend: structure dominates, ssim supports, block keeps colour honest.
    score = 0.55 * struct_score + 0.30 * ssim_score + 0.15 * block_score

    # content-presence guard: if one frame has real detail (gradient energy) and
    # the other is near-flat, this is the "didn't render" case — cap the score.
    ea = float(_grad_mag(ga).mean())
    eb = float(_grad_mag(gb).mean())
    lo, hi = min(ea, eb), max(ea, eb)
    detail_ratio = (lo / hi) if hi > 1e-6 else 1.0
    if detail_ratio < 0.25:        # one side has <25% the edge energy of the other
        score = min(score, 28.0)   # below the default 35 PASS bar → "didn't render"

    verdict = "PASS" if score >= min_score else "FAIL"
    return PerceptualResult(
        mode="perceptual", grid_w=gw, grid_h=gh,
        a_size=f"{aw}x{ah}", b_size=f"{bw}x{bh}",
        block_score=round(block_score, 2), ssim_score=round(ssim_score, 2),
        struct_score=round(struct_score, 2), detail_ratio=round(detail_ratio, 3),
        score=round(score, 2), min_score=min_score, verdict=verdict,
    )


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Canonical RB3 render diff (strict A/B + perceptual reference).")
    ap.add_argument("a", help="first image (PNG/JPG)")
    ap.add_argument("b", help="second image (build B, or reference photo)")
    ap.add_argument("--perceptual", action="store_true",
                    help="resolution-agnostic similarity score (for reference photos)")
    # strict knobs
    ap.add_argument("--tol", type=int, default=2,
                    help="strict: per-channel LSB tolerance (default 2)")
    ap.add_argument("--threshold", type=float, default=0.10,
                    help="strict: max %%differing for PASS (default 0.10)")
    ap.add_argument("--heatmap", metavar="OUT.png",
                    help="strict: write a diff heatmap PNG")
    # perceptual knobs
    ap.add_argument("--grid", type=int, default=64,
                    help="perceptual: common downscale grid longest-side (default 64)")
    ap.add_argument("--min-score", type=float, default=35.0,
                    help="perceptual: min similarity score for PASS (default 35)")
    # common
    ap.add_argument("--label", default="", help="prefix tag for the summary line")
    ap.add_argument("--json", action="store_true",
                    help="also print a JSON result object")
    args = ap.parse_args(argv)

    try:
        a = load_rgb(args.a)
        b = load_rgb(args.b)
    except Exception as e:  # noqa: BLE001
        print(f"VISUAL_DIFF error: {e}", file=sys.stderr)
        return 2

    tag = f"[{args.label}] " if args.label else ""

    if args.perceptual:
        res = diff_perceptual(a, b, grid=args.grid, min_score=args.min_score)
        if args.json:
            print(json.dumps(asdict(res)))
        print(tag + res.summary())
        return 0 if res.verdict == "PASS" else 1

    # strict
    try:
        res = diff_strict(a, b, tol=args.tol, threshold_pct=args.threshold)
    except ValueError as e:
        print(f"{tag}VISUAL_DIFF mode=strict verdict=ERROR {e}")
        return 2
    if args.heatmap:
        try:
            write_heatmap(a, b, args.tol, args.heatmap)
        except Exception as e:  # noqa: BLE001
            print(f"VISUAL_DIFF warning: heatmap failed: {e}", file=sys.stderr)
    if args.json:
        print(json.dumps(asdict(res)))
    print(tag + res.summary())
    return 0 if res.verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
