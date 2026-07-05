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

## W0.5.S2 — done

**Commit:** `0df1d073` (rb3) — `scripts/native/patch-lineup-capture.py` (new file only).

Patch-bearing WIDE lineup capture harness. Reuses `band-closeup-capture.py` /
`keyboard-to-gameplay.py`'s proven boot->gameplay nav (duplicated inline, since
it lives inside `band-closeup-capture.py:main()` and is not factored into a
standalone function — importing `bc` gives the pin primitives
(`force_shot`/`director_disable`/`cur_shot`/`advance_to_songms`/
`parse_shard_log`) which ARE reused directly, not re-derived).

- **WIDE shot candidates:** `coop_all/coop_wide/coop_band/coop_establish(ing)/
  coop_crowd/coop_full/coop_stage/coop_group` + `coop_g_n03/coop_g_b` fallback
  (wider guitarist framings that still catch a neighbour), `--shots` override.
  On the default boot venue (club-family, `--song-downs 4`) NONE of the
  generic wide names resolve (`force_shot not_found`) — only the two
  guitarist-framing fallbacks (`coop_g_n03.shot`, `coop_g_b.shot`) resolve.
  Recorded in `skipped` so S3/S4 know this venue's real wide-shot set is the
  fallback pair, not a genuine establishing shot. A different `--song-downs`
  (different venue) may resolve more of the generic names — worth trying in S3.
- **Layer C (`char_probe`):** `{rb3_char_probe <slot>}` parsed into
  `{slot,raw,meshes,skinned,verts,loading}` for slots 0..3 (default), recorded
  verbatim (including `null_char`/`no_charcache`) when the regex does not match.
- **Layer B (`shard_ratios`):** new `parse_shard_ratios()` (extends
  `parse_shard_log`'s regex) captures the FULL per-mesh
  `[SHARD_RATIO] mesh=... bindExt=... worldExt=... ratio=... band|other[ DROP]`
  tuple keyed by mesh name, plus `max_band_ratio`. One engine process/log per
  run -> the per-mesh table is shared across all frames in that run (S3 can
  refine to per-frame windows later if the log ever gets frame-scoped).
- **Artifacts:** `<tag>_<shot>_<i>.png` + `manifest.json` (adds `char_probe`,
  `shard_ratios`, `max_band_ratio`, `drops_*` alongside the existing
  shot/songMs/cur_shot/pinned/file fields). Last line:
  `PATCH_LINEUP out=... frames=... forced_shots=... max_band_ratio=...`.
  Exit 0 = nav ok + >=1 wide shot pinned; 1 = nav ok but no shot resolved/pin
  lost; 2 = ERROR (hook missing / never reached gameplay) — no baked-in
  `drops_band==0` verdict, per PLAN.md (S3 owns the gate).

**Verification (build/verify block from PLAN.md):**
- `cmake -B native/build-agent-W0.5 -S native` then `cmake --build ... --target
  rb3-native -j8` — required `CC=clang CXX=clang++` (bare `cmake` picked up GNU
  g++ by default in this shell, which rejects the MWCC-compat clang flags; the
  existing `native/build-native` CMakeCache already pins clang, this is only a
  fresh-build-dir gotcha, not a PLAN.md issue). One transient link failure
  (`undefined reference to __hmx_stub_first_hit`) self-resolved on rebuild —
  a race against a concurrent agent (W0.2.S2, "loud stub shim + census infra")
  editing `native/src/band3_link_stubs.s`/`rb3_stub_census.cpp` in the shared
  source tree at build time; not a bug in this harness or W0.2's work.
- `RB3_BIN=... python3 scripts/native/patch-lineup-capture.py --frames 2 --out
  /tmp/rb3-lineup/base --tag base` (also ran with `--bin` per PLAN.md's
  correction) -> exit 0, reaches `game_screen`, resolves 2 wide shots
  (`coop_g_n03.shot`, `coop_g_b.shot`), writes 4 PNGs + `manifest.json` with
  non-empty `char_probe` (4 slots/frame, real mesh/vert counts e.g.
  `player2 meshes=144 skinned=4 verts=15395`) and `shard_ratios` (~100 meshes/
  frame, e.g. `max_band_ratio=4.50`). Confirmed on 2 separate runs.

**ENVIRONMENT CAVEAT (recorded for S3/S4, not a harness bug):** this shared
machine's both GPUs are persistently near-saturated by long-running unrelated
`VLLM::EngineCore` processes (~23.9/24.6 GB used, ~200-300 MB free, steady-state
across repeated `nvidia-smi` samples — not concurrent-agent contention). Dawn/
Vulkan texture allocation for the render target intermittently OOMs
(`vkAllocateMemory ... VK_ERROR_OUT_OF_DEVICE_MEMORY` on `RB3Tex`), producing a
BLACK captured PNG even though the CPU-side sim (nav/pin/char_probe/shard_ratio
numerics) is unaffected and correct. One of two runs in this session captured
black frames for this reason; the harness itself does not detect or retry this
(out of scope for S2 — a capturer, not a gate). **S3 (golden generation) and S4
(fail-red visual proof, which needs the exploded frame to be VISUALLY confirmed
per its own step 1) should check GPU headroom (`nvidia-smi`) before generating/
capturing goldens, and may want to retry-on-black or wait for headroom**, or
the segmentation-numerics layer (S1) will see near-zero foreground on a
GPU-starved frame and that noise will leak into tolerance tuning.

**Deviations from PLAN.md:** none in behavior; only the `CC=clang CXX=clang++`
build-dir note above (informational, not a scope change).

**Remaining for downstream:** S3 owns golden generation + composite gate
tolerances; S4 owns the fail-red demonstration. Both should re-check
`nvidia-smi` headroom per the caveat above before trusting captured PNG content.
