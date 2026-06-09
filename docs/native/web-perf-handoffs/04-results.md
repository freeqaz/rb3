# 04 — Mesh-cache v2: NATIVE confidence-check results (2026-06-09)

Measurement-only pass quantifying the already-validated + already-pinned v2 mesh
cache (engine `b5309b3`, rb3 pin commit `91468cd5`). Web crashrate sweep was
SKIPPED per instruction (web build dirs contended). No code was changed.

## Test setup (important caveats)

- **Binary**: a CLEAN isolated build — rb3 worktree at `4c6b8500` (master HEAD)
  + engine worktree at the pin `b5309b3`, built to
  `/tmp/meshcache-eval/build/rb3-native`. The shared
  `native/build-native/rb3-native` could NOT be used: it was rebuilt mid-session
  by a concurrent agent with uncommitted `Rnd_Wgpu_RB3.cpp`/`Tex.cpp` edits
  (size changed 113944112 → 113946400 under me), so every number below comes
  from the isolated pinned binary.
- **A/B arms**: same binary; cache-ON = default, cache-OFF = `RB3_NO_MESH_CACHE=1`
  (legacy per-draw upload path).
- **Box load**: load avg ~6–12 throughout (Ghidra job running). Treat all
  absolute ms as inflated; **relative ON↔OFF deltas are the result**. Arms were
  interleaved within minutes of each other.
- All runs serialized under `/tmp/rb3-native-run.lock`, headless
  (`MILO_HEADLESS=1`, RTX 3090).

## 1. Steady-state resource check — PASS

`RENDER_DBG=30` per-frame telemetry (`cached_meshes` / `buf_creates` /
`bg_creates`), run: boot → song_select → into a song, 60 s gameplay hold
(log `/tmp/rb3-frameprof-39661.log`).

| Phase | cached_meshes | buf_creates/frame | bg_creates/frame |
|---|---|---|---|
| song_select steady (f270–f600) | 183–218, flat | **0** | **0** |
| in-song steady (gameplay) | 342–346, flat | 0–6 typical (worst sampled 20) | **0** |

- Cache-OFF contrast, same scenario: **1,200–4,143 buffer creates + ~700–1,018
  bind-group creates per frame** — the per-draw churn v2 eliminates.
- The in-song ON-arm residual (usually exactly 2, i.e. one mesh's VB+IB) is a
  small set of dynamic meshes being destroyed/recreated per frame
  (`cached_meshes` oscillates ±2 around a flat mean — create+destroy balance,
  not growth). `bg_creates=0` means no per-draw bind-group churn remains.
- Bounded over the whole run: cached_meshes never exceeded ~378 (menus) /
  ~356 (in-song); drops when scenes unload (CleanupGpuMesh works).
- **worst `slots.size()`: not directly observable** — the pinned build logs no
  per-slot counter and this was a no-code-edit pass. Indirect bound:
  steady-state `bg_creates=0` proves the per-instance slot pools stop growing
  after warm-up (slot growth would create bind groups).

## 2. Gameplay multi-draw correctness — PASS (at the determinism floor)

Gems are multi-drawn meshes — the exact v1 failure class (per-mesh uniforms
collapsed to the LAST instance: darkened rows, mean Δ ≈ (−7,−11,−12)).

**song_select framepin strict** (`_framepin_capture.py --screen song_select
--pin-frame 600`, 2 captures per arm):

| Pair | diff px | mean signed RGB | mean abs |
|---|---|---|---|
| ON↔ON (floor) | 32.0 % | (+0.05,+0.04,+0.03) | 0.72 |
| OFF↔OFF (floor) | 45.9 % | (+2.02,+1.54,+1.08) | 6.37 |
| ON↔OFF | 35.0–46.1 % | (−1.84..+0.18, −1.27..+0.27, −0.76..+0.32) | 1.19–6.22 |

Cross-arm diffs sit INSIDE the same-arm floor envelope (the floor is highlight
pulse + ±8-frame capture jitter), and the signed means flip sign across
pairings → **no systematic darkening; no v1 signature**.

**Gameplay highway** (`gameplay-depth-capture.py`, songMs-pinned 3500/6000/9000
ms, ON vs OFF): both arms render the highway with individually colored gems,
correct now-bar and HUD (visual inspection of all six captures — no uniform
collapse, no single-color gems, no darkening). The best-aligned pair (9000 ms)
has mean signed Δ = (−0.43,−0.39,−0.44) ≈ zero systematic offset; the residual
mean_abs ~17–27 across pairs is camera-cut/scroll-phase noise from the ±0.5 s
songMs capture granularity (same magnitude as the ON↔ON floor measured on the
plain frame-pinned game screen: mean_abs 22.2). Frame-pinned `game` captures
are NOT phase-stable (capture overshoots the pin by a load-dependent amount:
frames 902–1166 across runs), so gameplay can only be gated at this floor
without new tooling.

## 3. Perf numbers — large, unambiguous win

`frame_profiler.py`, RENDER_DBG=30 on both arms. **CAVEAT: box load avg ~6–12;
absolute ms inflated, read the deltas.**

Run A — `--scroll 10` song_select hover/scroll (per-screen, song_select only):

| Arm | n | p50 | p95 | p99 | max | mean |
|---|---|---|---|---|---|---|
| cache ON | 2415 | **9.96** | 17.60 | 19.73 | 95.4 | 10.9 |
| cache OFF | 1120 | **23.25** | 27.60 | 32.72 | 69.3 | 23.6 |

Run B — `--scroll 1 --into-song`, 60 s gameplay (game_screen only):

| Arm | n | p50 | p95 | p99 | max | mean |
|---|---|---|---|---|---|---|
| cache ON | 5506 | **9.57** | 15.88 | 17.33 | 233.6 | 10.5 |
| cache OFF | 2357 | **21.97** | 38.42 | 47.85 | 230.9 | 24.4 |

- **p50 ≈ 2.3× faster with the cache, on BOTH song_select and gameplay; p99
  2.8× on gameplay.** The ON arm renders ~2.3× the frames in the same 60 s
  hold (5506 vs 2357).
- Worst frames are the SAME class in both arms (asset syncDrain ~160–180 ms,
  splash-phase loads) — load stutters, not render, and not cache-affected.
- Whole-run overall: ON p50 9.4 / p95 16.1 / p99 34.8 ms vs OFF p50 21.4 /
  p95 40.8 / p99 98.4 ms.

## Verdict — does this plausibly fix web 25 fps?

**Yes — this is the strongest single candidate so far for the web frame-rate
problem.** The cache halves-plus native frame time at p50 (≈10 ms vs ≈22–23 ms)
in exactly the scenes the web build runs (song_select, gameplay), by removing
~1.2k–4k buffer creates + ~0.7k–1k bind-group creates per frame. On web those
creates are far more expensive than native (JS↔wasm boundary + Dawn-in-browser
validation per call), so the relative win should be at least as large there:
25 fps (40 ms) with the OFF-class per-draw path maps to ≈50+ fps if the web
build's bottleneck matches the native render profile. For the P0.2/P2 decision:
the mesh cache (already pinned) should be treated as the primary render-perf
lever; measure web rAF p50 + crashrate once the web build dirs free up
(handoff step 5/6, still open).

## Incidental findings (not mesh-cache related)

1. **Pre-existing into-song crash on some songs**: `frame_profiler.py
   --scroll 10/12 --into-song` aborts at gameplay start —
   `std::vector<SongData::TrackInfo*>::operator[]` assert OOB in
   `NewTrackWatcherImpl` (src/system/beatmatch/TrackWatcher.cpp:35) ←
   `TrackWatcher::SetImpl` ← `BeatMatcher::SetTrack` ← `GemPlayer::SetTrack`
   (GemPlayer.cpp:1311) ← `Game::PostLoad` (Game.cpp:342). Reproduces on the
   clean pinned binary with songs `beencaughtstealing` (10 down) and
   `breakonthrough` (12 down); the 1-down song plays fine. Deterministic;
   identical with cache ON (first seen) — i.e. NOT introduced by the mesh
   cache. Worth its own ticket.
2. **Shared build-dir contamination hazard confirmed**: `native/build-native/
   rb3-native` was rebuilt under me mid-session by a concurrent agent with
   uncommitted engine/rb3 edits. Any A/B measurement must copy or rebuild to an
   isolated path first.

## Artifacts

- Captures + traces + per-run profiler reports: `/tmp/meshcache-eval/`
  (`ss-{on,off}-{1,2}.png`, `game-{on,off}-{1,2}.png`, `gp-{on,off}/gameplay_*.png`,
  `trace-{scroll,song}-{on,off}.jsonl`, `prof-*.txt`, `pixdiff.py`)
- Engine logs: `/tmp/rb3-frameprof-39661.log` (ON in-song telemetry),
  `/tmp/rb3-frameprof-44227.log` (OFF contrast)
- Isolated pinned build: `/tmp/meshcache-eval/build/` (rb3 worktree
  `.claude/worktrees/meshcache-eval` @ 4c6b8500, engine worktree
  `/tmp/engine-b5309b3` @ b5309b3)
