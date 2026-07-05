# W0.5 — Non-blind visual lineup gate — STATUS

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask,
under `flock /tmp/rb3-docs.lock`, with commit SHAs and blockers.

## W0.5.S1 — done

**Commit:** `961889c6` (rb3) — `scripts/analysis/lineup_bbox_metrics.py` (new file only).

Segmentation screen-space numeric analyzer + golden-compare gate + selftest.

- **Foreground segmentation:** modal-border bg colour distance (`BG_DIST_THRESH`)
  OR gradient edge-energy backup (`EDGE_FLOOR`), reusing `visual_diff.load_rgb`
  and `_grad_mag`. `--bg-mode {border,black}`.
- **Connected components:** 8-connectivity. `scipy.ndimage.label` when present
  (confirmed available), pure-numpy union-find fallback otherwise (no hard scipy
  dep). Min-area filter `MIN_COMPONENT_AREA_FRAC`.
- **Frame metrics:** `n_components`, `n_slivers`, `max/mean_solidity`, `fg_fill`,
  `fg_bbox` + `fg_bbox_w/h/diag`, `total_fg_px`.
- **compare_to_golden(metrics, golden, tol):** 5 rules — n_slivers, n_components,
  mean_solidity, fg_fill, fg_bbox_diag — all as module constants (`GOLD_*`) for
  S3 to tune. Returns per-metric pass/fail + verdict.
- **CLI:** `IMG.png [--json]` (JSON metrics last line), `IMG.png --golden G.json`
  (gate, exit 0/1/2), `--selftest`.

**Verification:**
- `python3 scripts/analysis/lineup_bbox_metrics.py --selftest` exits 0. Compact
  blob = 1 comp / 0 slivers / mean_solidity 0.78; shattered = 19 comps / 15
  slivers / mean_solidity 0.178. Compact PASSes a golden derived from itself;
  shattered FAILs all 5 numeric layers. Clean separation.
- Runs on real frames (retail `fandom_gameplay_guitar.png`, `/tmp/render_t3.png`)
  and emits sane one-line JSON.

**DEVIATION from PLAN.md (recorded):** the plan's `n_slivers` uses only the
axis-aligned bbox aspect (max(w,h)/min(w,h)). That is blind to diagonal shards —
a thin sliver at 45 degrees has a near-square bbox (aspect ~1) despite being
thin, so it would never be flagged. Added an orientation-independent PCA
elongation (sqrt(major/minor) eigenvalue ratio of the pixel cloud) and use
`aspect = max(bbox_aspect, pca_elong)` for the sliver test. Both `bbox_aspect`
and `pca_elong` are also emitted per-component. Minimal fix that keeps the plan's
aspect+solidity semantics but is rotation-safe. No other scope change.

**Remaining for downstream:** S3 tunes the `GOLD_*` tolerance constants against
the committed real golden; S1 leaves them as documented placeholders.
