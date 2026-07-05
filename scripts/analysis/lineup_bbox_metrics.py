#!/usr/bin/env python3
"""Segmentation screen-space numeric analyzer for the W0.5 non-blind lineup gate.

Turns ONE lineup PNG into shard-sensitive screen-space numerics that a shard
explosion cannot pass, and a golden-compare gate function.

Why this exists
---------------
The old band-closeup gate (`scripts/native/band-closeup-capture.py`) and a plain
perceptual/SSIM image compare are both BLIND to shard explosions: scattered
slivers of roughly the right colours in roughly the right screen region keep the
translation-tolerant perceptual score above threshold, and the old gate's only
hard numeric (`drops_band == 0`) never moves for a BandPatchMesh corruption.

This module segments the foreground silhouette and measures its *shape*:
  - a compact character   -> few connected components, high solidity, no slivers,
                             a tight foreground bbox.
  - an exploded character -> many components, low solidity, many thin slivers,
                             and/or a foreground bbox that balloons across frame.

It is self-contained (numpy + Pillow only, matching `visual_diff.py`'s ethos)
and catches ANY on-screen shard, including BandPatchMesh which never emits a
`[SHARD_RATIO]` line and so is structurally outside the old gate's sensor.

CLI
---
  lineup_bbox_metrics.py IMG.png [--json]           # prints metrics dict (JSON last line)
  lineup_bbox_metrics.py IMG.png --golden G.json    # gate vs a committed golden -> exit 0/1
  lineup_bbox_metrics.py --selftest                 # correctness proof (compact vs shattered)

Exit codes: 0 = PASS / OK, 1 = FAIL (gate), 2 = ERROR.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from dataclasses import dataclass, asdict, field

import numpy as np

# Reuse visual_diff's RGB loader + gradient helper (it is importable).
_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)
from visual_diff import load_rgb, _grad_mag  # noqa: E402

# ---------------------------------------------------------------------------
# Segmentation tunables (module constants; documented so S3 can tune them).
# ---------------------------------------------------------------------------
# Foreground = pixels that are NOT background. Background colour is estimated as
# the modal colour of a border ring (venue backdrops fill the frame edges).
BG_BORDER_FRAC = 0.06          # border ring width as a fraction of min(H,W)
BG_DIST_THRESH = 42.0          # max-channel colour distance from bg to be "not bg"
EDGE_FLOOR = 34.0              # gradient-magnitude floor for the edge-energy backup
# A component must cover at least this fraction of the frame to be kept (drops
# JPEG/venue speckle). Small enough that a real thin shard sliver survives.
MIN_COMPONENT_AREA_FRAC = 1.8e-4
# "Sliver" classification: a thin, low-fill fragment. Real exploded shards
# rasterize to diagonal/irregular thin components whose pixel count is a small
# fraction of their (large) bbox -> low solidity AND high aspect.
SLIVER_ASPECT = 6.0           # bbox long/short >= this
SLIVER_SOLIDITY = 0.25        # AND area/bbox_area <= this

# ---------------------------------------------------------------------------
# Golden-compare tolerances (module constants; S3 tunes against the real golden).
# ---------------------------------------------------------------------------
GOLD_SLIVER_ABS_SLACK = 2      # n_slivers <= golden.n_slivers + this
GOLD_COMPONENT_FACTOR = 0.60   # n_components <= golden.n_components * (1 + this)
GOLD_SOLIDITY_FACTOR = 0.35    # mean_solidity >= golden.mean_solidity * (1 - this)
GOLD_BBOX_EXTENT_FACTOR = 0.30  # |fg_bbox_diag / golden - 1| <= this (both directions)
GOLD_FILL_FACTOR = 0.40        # fg_fill >= golden.fg_fill * (1 - this)


@dataclass
class Component:
    x0: int
    y0: int
    x1: int
    y1: int
    area: int
    solidity: float
    aspect: float       # elongation used for the sliver test (see below)
    bbox_aspect: float  # axis-aligned bbox long/short (blind to rotation)
    pca_elong: float    # orientation-independent PCA elongation (rotation-safe)

    @property
    def bbox(self):
        return [self.x0, self.y0, self.x1, self.y1]


@dataclass
class Metrics:
    width: int
    height: int
    total_fg_px: int
    n_components: int
    n_slivers: int
    max_solidity: float
    mean_solidity: float
    fg_fill: float                     # fg px / fg_bbox area
    fg_bbox: list                      # [x0,y0,x1,y1] union of kept components; [-1]*4 if empty
    fg_bbox_w: int
    fg_bbox_h: int
    fg_bbox_diag: float
    bg_mode: str
    components: list = field(default_factory=list)  # per-component dicts (omitted in --json summary unless --verbose)

    def to_dict(self, include_components: bool = False) -> dict:
        d = asdict(self)
        if not include_components:
            d.pop("components", None)
        return d


# ---------------------------------------------------------------------------
# Foreground segmentation
# ---------------------------------------------------------------------------
def _modal_border_color(rgb: np.ndarray) -> np.ndarray:
    """Dominant colour of the frame's border ring (robust bg estimate)."""
    h, w, _ = rgb.shape
    b = max(2, int(round(min(h, w) * BG_BORDER_FRAC)))
    ring = np.concatenate([
        rgb[:b, :, :].reshape(-1, 3),
        rgb[-b:, :, :].reshape(-1, 3),
        rgb[:, :b, :].reshape(-1, 3),
        rgb[:, -b:, :].reshape(-1, 3),
    ], axis=0)
    # Mode via coarse 3D histogram (16-level bins) to be robust to dithering.
    q = (ring.astype(np.int32) >> 4)  # 0..15 per channel
    codes = (q[:, 0] << 8) | (q[:, 1] << 4) | q[:, 2]
    vals, counts = np.unique(codes, return_counts=True)
    top = vals[int(np.argmax(counts))]
    sel = codes == top
    return ring[sel].mean(axis=0)


def foreground_mask(rgb: np.ndarray, bg_mode: str = "border") -> tuple[np.ndarray, np.ndarray]:
    """Boolean HxW foreground mask + the estimated bg colour.

    bg_mode:
      border  - bg = modal colour of the border ring (default; venue backdrops).
      black   - bg = pure black (alpha flattened onto black; near-black is bg).
    Foreground = (colour distance from bg > BG_DIST_THRESH) OR (edge energy floor).
    """
    rgbf = rgb.astype(np.float32)
    if bg_mode == "black":
        bg = np.zeros(3, dtype=np.float32)
    else:
        bg = _modal_border_color(rgb)
    # Max-channel colour distance from background.
    dist = np.abs(rgbf - bg[None, None, :]).max(axis=2)
    color_fg = dist > BG_DIST_THRESH
    # Edge-energy backup: catches textured regions close to bg colour.
    luma = rgbf @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
    edge_fg = _grad_mag(luma) > EDGE_FLOOR
    return (color_fg | edge_fg), bg


# ---------------------------------------------------------------------------
# Connected components (8-connectivity). scipy if present, else numpy union-find.
# ---------------------------------------------------------------------------
def _label(mask: np.ndarray) -> tuple[np.ndarray, int]:
    try:
        from scipy import ndimage  # type: ignore
        structure = np.ones((3, 3), dtype=np.int32)  # 8-connectivity
        labels, num = ndimage.label(mask, structure=structure)
        return labels.astype(np.int64), int(num)
    except Exception:
        return _label_numpy(mask)


def _label_numpy(mask: np.ndarray) -> tuple[np.ndarray, int]:
    """Pure-numpy 8-connectivity labeling via union-find over vectorized edges."""
    h, w = mask.shape
    flat = np.arange(h * w, dtype=np.int64).reshape(h, w)
    m = mask

    edges = []
    # right, down, down-right, down-left
    both = m[:, :-1] & m[:, 1:]
    edges.append((flat[:, :-1][both], flat[:, 1:][both]))
    both = m[:-1, :] & m[1:, :]
    edges.append((flat[:-1, :][both], flat[1:, :][both]))
    both = m[:-1, :-1] & m[1:, 1:]
    edges.append((flat[:-1, :-1][both], flat[1:, 1:][both]))
    both = m[:-1, 1:] & m[1:, :-1]
    edges.append((flat[:-1, 1:][both], flat[1:, :-1][both]))

    parent = list(range(h * w))

    def find(x):
        root = x
        while parent[root] != root:
            root = parent[root]
        while parent[x] != root:
            parent[x], x = root, parent[x]
        return root

    for a_arr, b_arr in edges:
        for a, b in zip(a_arr.tolist(), b_arr.tolist()):
            ra, rb = find(a), find(b)
            if ra != rb:
                parent[rb] = ra

    labels = np.zeros(h * w, dtype=np.int64)
    fg_flat = np.nonzero(mask.ravel())[0]
    roots = {}
    nxt = 1
    for p in fg_flat.tolist():
        r = find(p)
        lab = roots.get(r)
        if lab is None:
            lab = nxt
            roots[r] = lab
            nxt += 1
        labels[p] = lab
    return labels.reshape(h, w), nxt - 1


# ---------------------------------------------------------------------------
# Metric computation
# ---------------------------------------------------------------------------
def _pca_elongation(ys: np.ndarray, xs: np.ndarray) -> float:
    """Orientation-independent elongation of a pixel cloud = sqrt(major/minor)
    eigenvalue ratio of its covariance. A thin fragment at ANY angle scores high;
    a compact blob scores ~1. Robust to rotation, unlike an axis-aligned bbox."""
    n = xs.size
    if n < 2:
        return 1.0
    xf = xs.astype(np.float64)
    yf = ys.astype(np.float64)
    xf -= xf.mean()
    yf -= yf.mean()
    cxx = float((xf * xf).mean())
    cyy = float((yf * yf).mean())
    cxy = float((xf * yf).mean())
    tr = cxx + cyy
    det = cxx * cyy - cxy * cxy
    disc = max(0.0, tr * tr / 4.0 - det)
    root = disc ** 0.5
    lam_major = tr / 2.0 + root
    lam_minor = tr / 2.0 - root
    if lam_minor <= 1e-6:
        lam_minor = 1e-6
    return float((lam_major / lam_minor) ** 0.5)


def analyze(rgb: np.ndarray, bg_mode: str = "border") -> Metrics:
    h, w, _ = rgb.shape
    total = h * w
    mask, _bg = foreground_mask(rgb, bg_mode=bg_mode)
    total_fg_px = int(mask.sum())

    labels, num = _label(mask)
    min_area = max(8, int(round(total * MIN_COMPONENT_AREA_FRAC)))

    comps: list[Component] = []
    if num > 0 and total_fg_px > 0:
        ys, xs = np.nonzero(mask)
        labs = labels[ys, xs]
        order = np.argsort(labs, kind="stable")
        labs_s = labs[order]
        ys_s = ys[order]
        xs_s = xs[order]
        uniq, starts = np.unique(labs_s, return_index=True)
        ends = np.append(starts[1:], labs_s.size)
        for s, e in zip(starts.tolist(), ends.tolist()):
            area = e - s
            if area < min_area:
                continue
            yy = ys_s[s:e]
            xx = xs_s[s:e]
            x0 = int(xx.min()); x1 = int(xx.max())
            y0 = int(yy.min()); y1 = int(yy.max())
            bw = x1 - x0 + 1
            bh = y1 - y0 + 1
            bbox_area = bw * bh
            solidity = area / bbox_area if bbox_area > 0 else 0.0
            longs = max(bw, bh)
            shorts = max(1, min(bw, bh))
            bbox_aspect = longs / shorts
            pca_elong = _pca_elongation(yy, xx)
            # Use the rotation-safe elongation as the effective aspect: a thin
            # DIAGONAL shard has a near-square bbox (bbox_aspect ~ 1) but a large
            # PCA elongation, so the bbox metric alone is blind to it.
            aspect = max(bbox_aspect, pca_elong)
            comps.append(Component(
                x0, y0, x1, y1, int(area), float(solidity), float(aspect),
                float(bbox_aspect), float(pca_elong)))

    n_components = len(comps)
    if n_components == 0:
        return Metrics(
            width=w, height=h, total_fg_px=total_fg_px, n_components=0, n_slivers=0,
            max_solidity=0.0, mean_solidity=0.0, fg_fill=0.0,
            fg_bbox=[-1, -1, -1, -1], fg_bbox_w=0, fg_bbox_h=0, fg_bbox_diag=0.0,
            bg_mode=bg_mode, components=[],
        )

    n_slivers = sum(
        1 for c in comps if c.aspect >= SLIVER_ASPECT and c.solidity <= SLIVER_SOLIDITY
    )
    solidities = [c.solidity for c in comps]
    max_solidity = float(max(solidities))
    mean_solidity = float(sum(solidities) / len(solidities))

    ux0 = min(c.x0 for c in comps)
    uy0 = min(c.y0 for c in comps)
    ux1 = max(c.x1 for c in comps)
    uy1 = max(c.y1 for c in comps)
    fg_bbox_w = ux1 - ux0 + 1
    fg_bbox_h = uy1 - uy0 + 1
    fg_bbox_diag = float((fg_bbox_w ** 2 + fg_bbox_h ** 2) ** 0.5)
    kept_area = sum(c.area for c in comps)
    fg_fill = kept_area / (fg_bbox_w * fg_bbox_h) if fg_bbox_w * fg_bbox_h > 0 else 0.0

    return Metrics(
        width=w, height=h, total_fg_px=total_fg_px, n_components=n_components,
        n_slivers=n_slivers, max_solidity=max_solidity, mean_solidity=mean_solidity,
        fg_fill=float(fg_fill), fg_bbox=[ux0, uy0, ux1, uy1],
        fg_bbox_w=fg_bbox_w, fg_bbox_h=fg_bbox_h, fg_bbox_diag=fg_bbox_diag,
        bg_mode=bg_mode, components=[asdict(c) for c in comps],
    )


def analyze_path(path: str, bg_mode: str = "border") -> Metrics:
    return analyze(load_rgb(path), bg_mode=bg_mode)


# ---------------------------------------------------------------------------
# Golden compare
# ---------------------------------------------------------------------------
def compare_to_golden(metrics: dict, golden: dict, tol: dict | None = None) -> dict:
    """Gate `metrics` against a committed `golden` metrics dict.

    Rules (a shard explosion fails at least one):
      - n_slivers    <= golden.n_slivers + GOLD_SLIVER_ABS_SLACK
      - n_components  <= golden.n_components * (1 + GOLD_COMPONENT_FACTOR)
      - mean_solidity >= golden.mean_solidity * (1 - GOLD_SOLIDITY_FACTOR)
      - fg_fill       >= golden.fg_fill      * (1 - GOLD_FILL_FACTOR)
      - fg_bbox_diag  within +/- GOLD_BBOX_EXTENT_FACTOR of golden.fg_bbox_diag

    `tol` may override any of the *_FACTOR / *_SLACK constants by name.
    Returns {verdict, checks:{name:{pass,observed,bound,rule}}}.
    """
    t = {
        "sliver_abs_slack": GOLD_SLIVER_ABS_SLACK,
        "component_factor": GOLD_COMPONENT_FACTOR,
        "solidity_factor": GOLD_SOLIDITY_FACTOR,
        "fill_factor": GOLD_FILL_FACTOR,
        "bbox_extent_factor": GOLD_BBOX_EXTENT_FACTOR,
    }
    if tol:
        t.update(tol)

    checks: dict = {}

    def add(name, ok, observed, bound, rule):
        checks[name] = {
            "pass": bool(ok),
            "observed": observed,
            "bound": bound,
            "rule": rule,
        }

    g_sliv = golden.get("n_slivers", 0)
    bound = g_sliv + t["sliver_abs_slack"]
    add("n_slivers", metrics["n_slivers"] <= bound, metrics["n_slivers"], bound,
        f"<= golden({g_sliv}) + {t['sliver_abs_slack']}")

    g_comp = golden.get("n_components", 0)
    bound = g_comp * (1 + t["component_factor"])
    add("n_components", metrics["n_components"] <= bound, metrics["n_components"], bound,
        f"<= golden({g_comp}) * {1 + t['component_factor']:.2f}")

    g_sol = golden.get("mean_solidity", 0.0)
    bound = g_sol * (1 - t["solidity_factor"])
    add("mean_solidity", metrics["mean_solidity"] >= bound, metrics["mean_solidity"], bound,
        f">= golden({g_sol:.3f}) * {1 - t['solidity_factor']:.2f}")

    g_fill = golden.get("fg_fill", 0.0)
    bound = g_fill * (1 - t["fill_factor"])
    add("fg_fill", metrics["fg_fill"] >= bound, metrics["fg_fill"], bound,
        f">= golden({g_fill:.3f}) * {1 - t['fill_factor']:.2f}")

    g_diag = golden.get("fg_bbox_diag", 0.0)
    if g_diag > 0:
        ratio = metrics["fg_bbox_diag"] / g_diag
        ok = abs(ratio - 1.0) <= t["bbox_extent_factor"]
    else:
        ratio = 0.0
        ok = True
    add("fg_bbox_diag", ok, metrics["fg_bbox_diag"],
        [g_diag * (1 - t["bbox_extent_factor"]), g_diag * (1 + t["bbox_extent_factor"])],
        f"|obs/golden({g_diag:.1f}) - 1| <= {t['bbox_extent_factor']:.2f} (ratio={ratio:.3f})")

    verdict = "PASS" if all(c["pass"] for c in checks.values()) else "FAIL"
    return {"verdict": verdict, "checks": checks}


# ---------------------------------------------------------------------------
# Selftest — the correctness proof (compact blob vs shattered slivers).
# ---------------------------------------------------------------------------
def _draw_diag_sliver(img, cx, cy, angle_deg, length, thickness, color):
    """Draw one thin DIAGONAL sliver (rotated line) so it has high aspect AND
    low solidity (pixels are a small fraction of the diagonal bbox)."""
    h, w, _ = img.shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    a = np.deg2rad(angle_deg)
    ca, sa = np.cos(a), np.sin(a)
    dx = xx - cx
    dy = yy - cy
    along = dx * ca + dy * sa       # coordinate along the sliver
    perp = -dx * sa + dy * ca       # perpendicular distance
    m = (np.abs(perp) <= thickness / 2.0) & (np.abs(along) <= length / 2.0)
    img[m] = color


def _synth_compact(h=360, w=480):
    """A compact filled character-like blob on a venue-ish background."""
    bg = np.full((h, w, 3), (46, 70, 92), dtype=np.uint8)  # dim teal-blue venue
    img = bg.copy()
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    cx, cy = w * 0.5, h * 0.52
    rx, ry = w * 0.14, h * 0.34
    ell = ((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2 <= 1.0
    img[ell] = (210, 180, 150)  # skin/costume tone, clearly not bg
    return img


def _synth_shattered(h=360, w=480, cols=5, rows=4, seed=1234):
    """The same silhouette blown into thin scattered diagonal slivers, placed on
    a jittered grid so they stay distinct (mirrors an exploded mesh's fan-out)."""
    rng = np.random.default_rng(seed)
    bg = np.full((h, w, 3), (46, 70, 92), dtype=np.uint8)
    img = bg.copy()
    cell_w = w / cols
    cell_h = h / rows
    for r in range(rows):
        for c in range(cols):
            cx = (c + 0.5) * cell_w + float(rng.uniform(-cell_w * 0.15, cell_w * 0.15))
            cy = (r + 0.5) * cell_h + float(rng.uniform(-cell_h * 0.15, cell_h * 0.15))
            ang = float(rng.uniform(0, 180))
            length = float(rng.uniform(min(cell_w, cell_h) * 0.9, min(cell_w, cell_h) * 1.2))
            thick = float(rng.uniform(2.0, 3.0))
            _draw_diag_sliver(img, cx, cy, ang, length, thick, (210, 180, 150))
    return img


def selftest() -> int:
    compact = _synth_compact()
    shattered = _synth_shattered()

    mc = analyze(compact)
    ms = analyze(shattered)

    print("SELFTEST compact  :", json.dumps(mc.to_dict()))
    print("SELFTEST shattered:", json.dumps(ms.to_dict()))

    # Derive a golden from the compact frame; it must PASS itself and the
    # shattered frame must FAIL it. This proves the metric is not itself blind.
    golden = mc.to_dict()
    v_compact = compare_to_golden(mc.to_dict(), golden)
    v_shatter = compare_to_golden(ms.to_dict(), golden)
    print("SELFTEST gate compact  :", json.dumps(v_compact))
    print("SELFTEST gate shattered:", json.dumps(v_shatter))

    ok = True
    reasons = []

    # 1) Direct metric separation (independent of the gate tolerances).
    if not (ms.n_components >= mc.n_components + 5):
        ok = False
        reasons.append(f"component separation weak: compact={mc.n_components} shattered={ms.n_components}")
    if not (ms.n_slivers >= 5 and mc.n_slivers <= 1):
        ok = False
        reasons.append(f"sliver separation weak: compact={mc.n_slivers} shattered={ms.n_slivers}")
    if not (mc.mean_solidity >= ms.mean_solidity + 0.15):
        ok = False
        reasons.append(f"solidity separation weak: compact={mc.mean_solidity:.3f} shattered={ms.mean_solidity:.3f}")

    # 2) Gate separation: compact PASSes its own golden, shattered FAILs it.
    if v_compact["verdict"] != "PASS":
        ok = False
        reasons.append("compact frame did not PASS a golden derived from itself")
    if v_shatter["verdict"] != "FAIL":
        ok = False
        reasons.append("shattered frame did not FAIL the compact golden (metric is BLIND)")

    if ok:
        print("SELFTEST: PASS — compact and shattered separate cleanly")
        return 0
    print("SELFTEST: FAIL")
    for r in reasons:
        print("  -", r)
    return 1


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", nargs="?", help="lineup PNG to analyze")
    ap.add_argument("--golden", help="golden metrics JSON to gate against")
    ap.add_argument("--bg-mode", choices=["border", "black"], default="border",
                    help="background estimation mode (default: border modal colour)")
    ap.add_argument("--json", action="store_true", help="print metrics dict as JSON")
    ap.add_argument("--verbose", action="store_true", help="include per-component list in JSON")
    ap.add_argument("--selftest", action="store_true",
                    help="synthesize compact vs shattered frames and prove separation")
    args = ap.parse_args(argv)

    if args.selftest:
        return selftest()

    if not args.image:
        ap.error("image path required (or use --selftest)")

    try:
        metrics = analyze_path(args.image, bg_mode=args.bg_mode)
    except Exception as e:  # pragma: no cover
        print(f"ERROR: {e}", file=sys.stderr)
        return 2

    md = metrics.to_dict(include_components=args.verbose)

    if args.golden:
        try:
            with open(args.golden) as f:
                golden = json.load(f)
        except Exception as e:
            print(f"ERROR: cannot read golden {args.golden}: {e}", file=sys.stderr)
            return 2
        result = compare_to_golden(metrics.to_dict(), golden)
        for name, c in result["checks"].items():
            flag = "ok " if c["pass"] else "FAIL"
            print(f"  [{flag}] {name}: observed={c['observed']} rule={c['rule']}")
        print("LINEUP_BBOX verdict=%s file=%s" % (result["verdict"], args.image))
        print(json.dumps({"metrics": md, "gate": result}))
        return 0 if result["verdict"] == "PASS" else 1

    # Plain analyze: one-line JSON summary contract (mirror visual_diff).
    print(json.dumps(md))
    return 0


if __name__ == "__main__":
    sys.exit(main())
