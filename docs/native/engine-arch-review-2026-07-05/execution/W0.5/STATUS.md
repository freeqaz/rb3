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

## W0.5.S3 — partial (driver done + proven; committed golden BLOCKED on persistent GPU-OOM)

**Commit:** `d8c8e477` (rb3) — `scripts/native/lineup-gate.py` (new file only).
Golden fixtures (`scripts/native/goldens/w0.5-lineup/`) NOT yet committed — see
blocker below; an autonomous waiter self-commits them under flock the moment GPU
headroom appears.

**Driver (`lineup-gate.py`) — COMPLETE.** One driver that runs S2's capture
(shells out to `patch-lineup-capture.py`, reusing its proven nav/pin verbatim)
and composes four layers vs a committed golden, matched per `(shot, frame_idx)`:
- **image (ADVISORY, does NOT gate):** `visual_diff.diff_perceptual` candidate
  vs golden PNG. Reported but excluded from the AND — it is the fool-able layer.
- **segA (NUMERIC):** `lineup_bbox_metrics.compare_to_golden` (S1) on each WIDE PNG.
- **ratioB (NUMERIC, run-level):** every `[SHARD_RATIO]` mesh ratio `<=
  per_mesh_ratio_cap` (golden max * 1.50, floor 8.0) AND `max_band_ratio` within
  golden bound.
- **countC (NUMERIC, per-frame):** `{rb3_char_probe}` meshes/skinned/verts per
  slot within tolerance (10/10/8%), and NO slot flipped to `null_char`.
- **Overall = AND(segA, ratioB, countC) [+ pin resolved]**; image is advisory so
  "image PASS but numeric FAIL" is visible. Emits `verdict.json` + one-line
  `LINEUP_GATE verdict=... img=... segA=... ratioB=... countC=...`.
- **`--gen-golden`** captures the good build, runs S1 on each WIDE PNG, and writes
  `golden.json` (per-frame seg_metrics + char_probe, per_mesh_ratio_cap,
  shard_ratio_max, resolved shots) + copies golden PNGs. Has a **black-frame
  guard** (`MIN_FG_FRAC`): refuses to write a golden whose frames are
  black/near-empty (the GPU-OOM failure mode), exit 2.
- Tolerances are auditable module constants (ratio/count here; segmentation GOLD_*
  in S1). `--anchor-ms` threads through gen -> golden.capture_params -> gate for
  cross-run pose determinism.

**`--selftest` — PASSES (GPU-INDEPENDENT correctness proof).** Proves each numeric
layer separates a clean lineup from an exploded one WITHOUT a rendered frame
(segA via S1 compact/shattered synth frames; ratioB via a 41.7-ratio flung mesh
vs cap 8.0; countC via a null_char-flipped slot + a 15395->61234 vert re-tessellation):
```
SELFTEST segA:  clean=PASS exploded=FAIL
SELFTEST ratioB: clean_pass=True exploded_pass=False (cap=8.0)
SELFTEST countC: clean_pass=True exploded_pass=False
SELFTEST: PASS — all three numeric layers separate clean vs exploded
```
This is the equivalent of S1's `--selftest`: it guarantees the composite gate is
not itself blind, independent of the GPU blocker below.

**Layers B & C proven LIVE under GPU-OOM.** A real capture on this machine
(`patch-lineup-capture.py`) returned, even with black rendered frames, valid
`shard_ratios` (94 meshes, `max_band_ratio=4.48`) and `char_probe`
(`player2 meshes=144 skinned=4 verts=15395`, all 4 slots numeric). So the
CPU-side numeric layers the gate depends on are capturable regardless of GPU
state; only segA + the image layer need a rendered frame.

**BLOCKER (environment, confirmed root cause) — committed golden + PASS x2.**
Exactly the ENVIRONMENT CAVEAT S2 flagged. Both GPUs are pinned by two persistent
unrelated `VLLM::EngineCore` processes (23.8-23.9 GB used each, **~300 MB free,
rock-steady over 90 s of sampling — NOT concurrent-agent contention**). rb3-native
renders via Dawn/Vulkan and there is **no software Vulkan ICD** installed
(`/usr/share/vulkan/icd.d/` has only `nvidia_icd.json`; no lavapipe/llvmpipe), so
no CPU fallback. Every capture attempt this session produced **pure-black frames**
(`min=max=0`, identical 36970-byte PNGs); engine log confirms the cause is
per-texture upload OOM, not a capture/nav bug:
```
GpuDevice: WebGPU error (type 3): vkAllocateMemory failed with VK_ERROR_OUT_OF_DEVICE_MEMORY
 - While calling [Device].CreateTexture([TextureDescriptor "RB3Tex"]).
```
`RB3Tex` is the aggregate per-RndTex GPU texture working set (not a resizable
render target), so no resolution knob shrinks it below ~300 MB. Consequently
`--gen-golden` correctly REFUSES (black-frame guard) and the golden + the
"PASS across two consecutive runs" verification (exit-criteria #3) cannot be
produced right now. This is an environment condition, not a code defect.

**Autonomous completion path (running now).** A managed background waiter
(`/tmp/w05-golden-waiter.sh`, started 20:49 UTC, 90-min budget) polls
`nvidia-smi`; the instant either GPU frees >=3000 MB it runs
`lineup-gate.py --gen-golden ... --anchor-ms 24000`, then 2 gate passes, then
**commits the golden fixtures under `flock /tmp/rb3-git.lock`** (stages ONLY
`scripts/native/goldens/w0.5-lineup/`). Progress/verdicts logged to
`/tmp/w05-golden-result.txt`, `/tmp/w05-gen-golden.log`, `/tmp/w05-gate{1,2}.log`.
So the render-dependent deliverable lands automatically if headroom appears,
even after this agent returns. If it TIMEOUTs, a human/coordinator finishes it
with one command when a GPU is free:
```
python3 scripts/native/lineup-gate.py --gen-golden --bin native/build-agent-W0.5/rb3-native --anchor-ms 24000
python3 scripts/native/lineup-gate.py            --bin native/build-agent-W0.5/rb3-native   # -> PASS x2
git add scripts/native/goldens/w0.5-lineup && git commit -m "W0.5: committed golden ..."
```

**Deviations from PLAN.md:** (1) driver shells out to S2's capturer as a
subprocess (rather than importing its `main`) — cleanest reuse, keeps process
isolation; reads `manifest.json`. (2) Added `--selftest` (not in the plan) to
prove the composite logic without a GPU — necessary because the render is
environment-blocked; mirrors S1's selftest ethos. No scope expansion (still
`scripts/` + this `W0.5/` doc dir only; no engine/native/src edits).

**Remaining:** golden generation + PASS-x2 (blocked on GPU headroom; waiter will
complete + commit autonomously, or the one-command path above). S4 (fail-red)
likewise needs a non-black exploded frame and inherits the same GPU dependency.

## W0.5.S3 — done (UPDATE — supersedes the "partial" section above)

GPU headroom appeared ~4 min after the partial write (a VLLM process freed
~24 GB), the managed waiter caught it and generated a first golden, and I then
finished the render-dependent work interactively. **S3 is now COMPLETE.**

**Commits (rb3):**
- `d8c8e477` — `scripts/native/lineup-gate.py` (driver, initial).
- `79c06406` — segA gating retune + committed golden
  (`scripts/native/goldens/w0.5-lineup/` : `golden.json` + 4 WIDE PNGs).

**Golden committed** (`scripts/native/goldens/w0.5-lineup/golden.json` + PNGs):
WIDE band lineup, shots `coop_g_n03.shot` + `coop_g_b.shot`, anchor 24000 ms,
4 frames, each with per-frame seg_metrics + char_probe(4 slots);
`per_mesh_ratio_cap=8.0`, `seg_abs_component_cap=110`,
`seg_gating=[n_slivers, n_components_abs]`, `seg_tol` (sliver_abs_slack=8).

**Verified — stable PASS x2 (exit-criteria #3).** Two consecutive gate runs on
the clean build both `LINEUP_GATE verdict=PASS img=PASS segA=PASS ratioB=PASS
countC=PASS pin=PASS` (exit 0):
```
run1: coop_g_n03[0] sliv=0 ncomp=25 | [1] sliv=2 ncomp=26 | coop_g_b[0] sliv=1 ncomp=20 | [1] sliv=0 ncomp=25  -> PASS
run2: coop_g_n03[0] sliv=1 ncomp=30 | [1] sliv=1 ncomp=23 | coop_g_b[0] sliv=0 ncomp=27 | [1] sliv=0 ncomp=22  -> PASS
```

**TUNING FINDING (important for S4 + future maintainers).** The FIRST golden
(single run, `n_slivers=0` everywhere) flaked: a fresh gate run FAILed segA
because `fg_fill`/`mean_solidity`/`n_components` swing 2-3x run-to-run on these
busy WIDE venue frames — MEASURED across 3 good runs (12 frames): `fg_fill
0.17-0.76`, `mean_solidity 0.34-0.57`, `n_components 5-51`. Those are dominated
by legit crowd/lighting/particle nondeterminism (NOT geometry), so gating them
false-fails a clean build. Retuned segA to gate ONLY the run-stable,
shard-specific signals:
- **`n_slivers`** golden-relative (`<= golden + 8`; good envelope max was 5) —
  the direct "thin teal/green/yellow sliver" detector.
- **`n_components_abs`** absolute explosion cap (`<= 110`; good max ~51;
  a fragmentation explosion -> hundreds) — golden-relative was too tight given
  the 5-51 component swing.
`fg_fill` / `mean_solidity` / `fg_bbox_diag` are now **ADVISORY** (printed, not
gated). `ratioB` (per-mesh SHARD_RATIO) and `countC` (char_probe counts) stay
DETERMINISTIC (CPU-side, anchor-pinned; passed every run). The non-blind property
for BandPatchMesh (which emits no SHARD_RATIO and moves no char_probe count) is
preserved by `n_slivers` + `n_components_abs`. `--selftest` still cleanly
separates clean vs exploded (shattered synth = 15 slivers / 19 comps -> FAIL).

**For S4:** golden is committed and the gate is stable-green. To prove fail-red,
induce shards (`RB3_NO_SKEL_REBIND=1 SHARD_GUARD_OFF=1` [+`RB3_NO_SKIN_CLAMP=1`])
and run `python3 scripts/native/lineup-gate.py --bin native/build-agent-W0.5/rb3-native`;
segA should FAIL on `n_slivers` (and/or `n_components_abs`). NOTE the GPU-OOM
caveat still applies — S4 must capture a NON-BLACK exploded frame (check
`nvidia-smi` headroom first; a black frame has ~0 foreground and would false-FAIL
segA for the wrong reason — the driver flags `any_black_frame` in verdict.json to
distinguish this). GPU on this box oscillates between fully saturated (~300 MB
free, VLLM steady-state) and fully free (~24 GB) — wait for a free window.

**Env caveat carried forward:** no software Vulkan ICD installed (nvidia only),
so there is no CPU render fallback when both GPUs are pinned.
