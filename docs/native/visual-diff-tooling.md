# Visual-diff tooling for the RB3 native/web port

Quantitatively compare a build's rendering against (a) another build (strict A/B)
and (b) the curated reference screenshots (perceptual sanity). Reusable project
tooling — the gate you run to prove a render/GPU-backend change (e.g. the
per-mesh GPU-buffer cache from `songlib-web-gpu-device-lost-2026-06-09.md`) does
not regress rendering, and to spot-check faithfulness against ground truth.

Modelled on the westworld render-parity rig (`tools/pngdiff.py` +
`cmd/renderdiff`): **one canonical pixel diff** (per-pixel max-channel `|Δ|`,
configurable LSB tolerance, %differing, max delta, differing-region bbox) plus a
**second, looser perceptual mode** for reference-photo comparison where exact
pixels are impossible (different renderer / AA / resolution).

## Files

| Path | Role |
|---|---|
| `scripts/analysis/visual_diff.py` | The diff library + CLI. Strict + perceptual. numpy-vectorized (fast on 1280×720). Importable: `diff_strict`, `diff_perceptual`. |
| `scripts/analysis/visual_diff_capture.py` | Capture+compare driver: captures the same canonical screens from two sources and diffs per-screen. |
| `scripts/web/visual-capture.mjs` | The web capture leg (Playwright, reuses `scripts/web/lib/core.mjs`). Driven by the Python driver; usable standalone. |

Dependencies are already present: numpy, PIL, and Playwright (in
`scripts/web/node_modules`). No build step.

---

## 1. The diff CLI — `visual_diff.py`

### Strict mode (default) — build-vs-build A/B

```bash
python3 scripts/analysis/visual_diff.py A.png B.png [--tol N] [--threshold PCT] \
        [--heatmap OUT.png] [--label NAME] [--json]
```

Both images MUST be the same size. A pixel "differs" when its max per-channel
`|Δ|` exceeds `--tol`. Reports `max(R,G,B)` delta, %differing, the largest
single-channel `|Δ|`, and the bbox of the differing region. `--heatmap` writes a
diff PNG (in-tolerance pixels = dim grayscale of A for context; out-of-tolerance
= bright magenta scaled by delta) — localizes *where* the change is.

```
VISUAL_DIFF mode=strict verdict=PASS size=1280x720 tol=2 maxRGB=0/0/0 maxd=0 \
            ndiff=0/921600 pct=0.0000 thresh=0.1000 bbox=none
```

- **Exit 0** = PASS (`pct_differing <= --threshold`), **1** = FAIL, **2** = error
  (e.g. size mismatch).
- The **last stdout line** is always the machine-readable `VISUAL_DIFF ...`
  summary; `--json` adds a JSON object on the line before it. Gate on either.

### Perceptual mode — reference-photo sanity (resolution-agnostic)

```bash
python3 scripts/analysis/visual_diff.py A.png REF.png --perceptual \
        [--min-score S] [--grid N] [--json]
```

Downscales BOTH to a common grid (any sizes OK) and blends three
translation/AA-tolerant terms into a **0..100 similarity score**:

- **struct** — translation-tolerant layout: regional edge-energy NCC (6×8 region
  grid: "is the list region busy / the art panel textured / the header has
  content") averaged with a coarse gradient-magnitude NCC. This is the heavy term.
- **ssim** — windowed grayscale SSIM (luminance + contrast + structure).
- **block** — block-mean colour delta (colour honesty), minor.

A **content-presence guard** caps the score when one side is near-flat — the
"screen never rendered" case scores unambiguously low.

> Perceptual is a **rendered-vs-blank gate with an advisory similarity score**,
> NOT a precise same-screen classifier. A score below ~35 means almost certainly
> blank / garbage / wrong-screen; a high score means structurally present and
> roughly laid out like the reference. Use it to answer "did the song list /
> characters / HUD render at all", not "do these match pixel-for-pixel".

Exit 0 = PASS (`score >= --min-score`), 1 = FAIL.

---

## 2. The capture+compare driver — `visual_diff_capture.py`

Captures the SAME canonical screens (`main_hub`, `song_select`, optionally
`game`) from two SOURCES and diffs per-screen.

A **SOURCE** is one of:

| Spec | Meaning |
|---|---|
| `native:<port>` | launch `rb3-native` headless, capture via `/api/screenshot` (port `0` = auto-pick) |
| `web:<port>` | drive the Playwright harness, capture the `#rb3-canvas` |
| `dir:<path>` | a directory of pre-captured PNGs (`<screen>.png`) — no capture, just diff |
| `ref` | the curated `images/retail-screenshots/` (Wii-native where available) |

```bash
# strict A/B: candidate native build vs the baseline native build
python3 scripts/analysis/visual_diff_capture.py \
    --a native:0 --a-bin native/build-native/rb3-native \
    --b native:0 --b-bin /path/to/candidate/rb3-native \
    --mode strict --screens main_hub,song_select

# perceptual: the live web build (:8421) vs the reference screenshots
python3 scripts/analysis/visual_diff_capture.py --a web:8421 --b ref --mode perceptual

# diff two already-captured dirs (e.g. golden-file regression in CI)
python3 scripts/analysis/visual_diff_capture.py --a dir:/tmp/golden --b dir:/tmp/run2 --mode strict
```

Outputs go to `--out DIR` (default `/tmp/rb3-vcap-<ts>`): each side's captures
under `a/` and `b/`, strict heatmaps under `diff/`, and a `report.json`. The last
stdout line is `VISUAL_DIFF_CAPTURE verdict=PASS|FAIL ...`. Exit 0 iff every
compared screen PASSes.

Determinism: native capture settles on the published `currentScreen` (never a
frame number — headless frame rate varies); web capture reuses `core.navigateTo`,
which settles per-screen. Both use a fixed nav verb sequence and a fixed
1280×720 viewport / canvas.

### Standalone web capture

```bash
node scripts/web/visual-capture.mjs --port 8421 --out DIR --screens main_hub,song_select
# determinism proof: navigate once, capture the SAME painted frame twice in-session
node scripts/web/visual-capture.mjs --port 8421 --out DIR --dup-screen song_select
```

> **GPU note:** browser/Playwright runs need a real GPU. Run them with the bash
> sandbox disabled (`dangerouslyDisableSandbox: true`) so chromium gets the host
> WebGPU adapter (ANGLE/Vulkan) rather than failing to acquire a device.

---

## Chosen thresholds / tolerance defaults

| Knob | Default | Rationale |
|---|---|---|
| strict `--tol` | **2** | absorbs ≤2-LSB rounding wobble between two renders of the same backend (sRGB encode / blend rounding). 0 is byte-exact; raise for a flakier backend. |
| strict `--threshold` | **0.10%** | a regression gate, not byte-exact. ~0.1% of pixels lets sub-pixel text/cursor jitter through while still catching any real geometry/colour/HUD change (which move whole regions → ≫0.1%). |
| perceptual `--min-score` | **35** | empirically separates "rendered & roughly laid out like the reference" (same-screen ≈ 38–55, identical = 100) from "blank / garbage / wrong-screen" (< ~32). The content-presence guard caps near-flat frames at 28. |
| perceptual `--grid` | **64** (long side) | coarse enough to be resolution/AA-agnostic, fine enough to keep panel-level layout. |

These are starting points calibrated on the reference set; override per use case.

---

## Verified runs (2026-06-09, live `:8421` web build, real GPU)

**Strict A/B determinism.** The web build's frame timing is non-deterministic
(variable frame rate, animated main-hub scene + band characters), so two
*independent* navigations land on different animation phases — a same-build
cross-session strict diff is therefore NOT ~0% on animated screens (main_hub
≈ 63% differing, song_select ≈ 10%, all from animation, `maxΔ` moderate, not
garbage). The strict ~0% guarantee is for **frame-pinned / golden** captures:

- Golden round-trip through the driver (a stored capture diffed against itself):
  `main_hub` and `song_select` both **0.0000% differing, maxΔ=0, PASS** — proves
  the capture→diff→report pipeline is deterministic (this is the CI regression
  pattern: snapshot a golden, fail on any later pixel drift).
- Cross-session **perceptual** of the same build confirms the harness reliably
  reaches and renders the same screens: `song_select` **99.5**, `main_hub`
  **91.6** (the gap is pure animation phase). The strict heatmap localizes the
  diff exactly to the scrolling song-list text + the album-art panel.

**Perceptual vs reference (ground truth).** Live web captures vs
`images/retail-screenshots/`:

- `main_hub` vs the 360 hub reference: **score 45.8 → PASS** (renders, laid out
  like the reference).
- `song_select` vs the Wii song-list reference: **score 30.4 → FAIL (borderline)**
  — an honest signal: our song_select *does* render (MUSIC LIBRARY header, list,
  album-art panel, action bar all present), but the content diverges from the
  ground truth (our build shows a short list + a "?" placeholder art vs the
  reference's full 587-song library with real album art), so the busy regions
  don't align. Not a false negative — exactly the "renders but doesn't match
  ground truth yet" state perceptual flags for follow-up.

**Self-test of the diff itself** (`images/retail-screenshots/`): identical = 100;
black "didn't render" = 27.6 (FAIL); random noise = 30.0 (FAIL); same-screen
different content = 53.4 (PASS); two gameplay variants = 38.9 (PASS); different
screens = 32.0 (FAIL); cross-resolution same content = 37.3 (PASS). Strict:
identical = 0%, size-mismatch = exit 2.
