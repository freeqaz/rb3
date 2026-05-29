# W4 — Optimization & Polish

**Parent plan:** [`PLAN.md`](PLAN.md). Read it for context.
**Depends on:** [`W3_BOOT_TO_SONG.md`](W3_BOOT_TO_SONG.md) (a song plays in the browser).
**Blocks:** nothing — last phase.

---

## W4 Baseline (2026-05-29)

**Commit:** `6cfb0a7d` (master HEAD — W3c complete, one song e2e in browser)
**Engine pin:** `5fda7f0`
**Build tool:** emcc 5.0.2, default flags (debug `-O0 -g2`)
**Screenshots:** [`docs/sessions/web/screenshots/baseline-2026-05-29/`](../../sessions/web/screenshots/baseline-2026-05-29/README.md)

### WASM + JS artifact sizes

| Artifact | Raw | gzip -9 | brotli -11 |
|---|---|---|---|
| `rb3-web.wasm` | 28,433,918 B (27.1 MB) | 5,342,743 B (5.1 MB) | 3,265,585 B (3.1 MB) |
| `rb3-web.js` | 377,291 B | 83,819 B | 67,805 B |
| `index.html` | 7,836 B | — | — |
| `audio-worklet.js` | 2,340 B | — | — |
| **Total dist** | **28,821,385 B (27.5 MB)** | — | — |

The gzipped wasm (5.1 MB) is already under the 10 MB must-hit target. The `-Oz` / `-g0` strip
work in W4a can cut this further (DC3 achieves 5.4 MB gzip on a comparable build). Brotli
gets it to 3.1 MB, well inside the target.

### Memory

| Moment | WASM heap |
|---|---|
| Post-boot (before splash interaction) | 119.6 MB |
| During gameplay (frame ~1820, ~15s into song) | 206.8 MB |

Heap is within the 256 MB must-hit target even with debug symbols present. A release build
(`-Oz -g0`) will reduce this. Full per-frame profiling (W4d.0) still needed.

### Visual state (vs native refs)

Full analysis in the screenshot README. Key deficits ranked by impact:

1. **Song list text not rendering** — song names blank in `song_select_screen`. Font texture / text material gap.
2. **Main hub menu labels absent** — "PLAY NOW / CAREER / TRAINING" etc. not rendering over the 3D hub scene.
3. **Gameplay HUD missing** — score, streak, star power, progress bar all absent. UI overlay pipeline gap.
4. **Album art not loading** — placeholder `?` icon shown instead of cover art.
5. **Splash background black** — intro movie not playing (BinkVideo stubbed; expected).

The 3D scene geometry and gem highway render correctly. The deficit is entirely in the UI/HUD
overlay layer (text + 2D panels).

---

## Goal

The browser tab loads quickly, runs at stable 30+ fps during gameplay, and
the asset experience is good enough for casual play. WASM build size <15MB
gzipped.

This phase mirrors DC3's Phase 8 (`dc3-decomp/docs/plans/web-port/PLAN.md`
Phase 8). DC3 hasn't completed Phase 8 yet; W4 may end up driving DC3 in
the same direction.

## Targets

DC3's current `dc3-web.wasm` is ~5.4MB gzipped (27MB raw) — RB3 is
gfx-only so should come in similar or smaller. Heap/time-to-canvas
targets below are not measured; do **W4d.0** (one-time baseline)
before tuning.

| Metric | Target | Type |
|---|---|---|
| `rb3-web.wasm` size (gzipped) | <10MB | must-hit (W4a) |
| Initial asset bundle size (gzipped) | <30MB (boot + first song) | must-hit (W4b/f) |
| Time to canvas-clear (cached load) | <3s | stretch (W4b/c) |
| Time to song_select (cached load) | <8s | stretch (W4b) |
| Steady-state framerate during gameplay | ≥30fps on a 2020-era integrated GPU | must-hit (no specific task) |
| Steady-state WASM heap | ≤256MB target / ≤384MB acceptable | must-hit (W4d) |

**Target → task mapping.** Skipping a sub-task means the corresponding
"must-hit" metric is unmet:

- W4a (WASM size) → governs the gzipped-WASM target. Cannot skip.
- W4b (IndexedDB) → governs cached load times. If skipped, downgrade
  cold-load targets only.
- W4c (loading screen) → UX, not a metric. Skippable.
- W4d (memory tuning) → governs WASM heap target. Cannot skip.
- W4e (pthreads) → optional. Only pursue if W4d baseline shows
  main-thread blocking is the bottleneck.
- W4f (progressive song loading) → improves first-song load. Stretch.
- W4g (diagnostics overlay) → dev-mode only. Stretch.
- W4h (persistent settings) → user-facing nice-to-have. Stretch.

## Tasks (parallelisable; each can be a sub-agent)

### W4a — WASM size reduction

- Switch `rb3-web` build to `-Oz` (size-optimised) for release; keep
  `-O0 -g2` for dev (`scripts/web/build-debug.sh`).
- Try `--closure 1` on the JS glue. Verify `_main` and
  `_rb3MainLoopTick` exports survive closure mangling. (DC3 has not
  enabled closure to date — no prior evidence of issues; if mangling
  breaks exports, fall back to `--closure 0`.)
- Strip debug info from release build: `-g0`.
- **Brotli compression: net-new work in `server.py`.** The DC3
  `server.py` does **not** do any `Content-Encoding` negotiation today;
  it serves `.wasm` raw. Add a `Content-Encoding: br` path that serves
  a pre-compressed `.wasm.br` when the client `Accept-Encoding` includes
  `br`. Generate the `.wasm.br` artifact in `build.sh` via the `brotli`
  CLI.
- Audit the source set: are any RB3 fork TUs in the web build that the
  web flow never executes? Filter them out.

### W4b — Asset caching (IndexedDB)

DC3 hasn't done this. RB3 W4 implements it; backport to DC3 in a
follow-up.

- Browser-side: IndexedDB store keyed by asset path + checksum.
- `WebAssets.cpp` (engine, lifted in W0): check IDB first, fall back to
  HTTP fetch on miss, write to IDB on successful fetch.
- Cache invalidation: server emits a `/api/version` endpoint; on
  startup, the browser nukes IDB if the version changed.

### W4c — Loading screen

- HTML/CSS spinner shown before `rb3-web.js` finishes downloading.
- Progress bar driven by the WASM-side `WebAssets.cpp` (it knows
  `Pending` / `Completed` counts).
- Replace the bare console-log status panel from W1 with a styled
  loading screen.

### W4d — Memory tuning

**W4d.0 (one-time baseline, do this first):** measure peak heap during a
full song playthrough on both native (`rb3-native` with RSS sampling)
and DC3 web (DevTools Memory tab during a song). Use the higher of the
two as the floor for the RB3 target. Until this number exists, the
"≤256MB" target above is a guess — treat it as a stretch goal.

- Profile WASM heap usage during a full song playthrough.
- Tune `-sMAXIMUM_MEMORY=`. DC3 uses 512MB
  (`dc3-decomp/native/CMakeLists.txt:1345`); RB3 may want lower since
  the gfx core is smaller and no NgRnd tier exists.
- Add `-sINITIAL_MEMORY=` to skip the first few growths.

### W4e — Optional: pthreads + PROXY_TO_PTHREAD

DC3 lists this as optional. RB3 should evaluate:

- Move asset downloading + decode off the main thread.
- Risk: pthreads requires `Cross-Origin-Isolated` (COOP/COEP headers
  are already set by DC3's `server.py:35-39` — RB3's copy inherits
  them). Test on production hosting.
- Risk: thread-unsafe statics in matched-fork code. No audit exists
  yet; one must be done before enabling pthreads. The matched-fork code
  is full of file-scope statics and lazy singletons that assume
  single-threaded access. Budget real time for this audit; do not treat
  pthreads as a quick win.

Recommendation: **defer unless W4d shows main-thread blocking is the
bottleneck.**

### W4f — Progressive song loading

- Loading screen between song select and gameplay can stream the
  `.mogg` while the venue loads.
- DC3 doesn't have this; novel work for RB3.

### W4g — Diagnostics overlay

- Reuse the engine's ImGui backend (`ImGuiBackend_Web.cpp` lifted in W0)
  for an opt-in DevTools-style overlay: FPS, heap, audio buffer status,
  asset fetch queue, current screen, RTT to server.
- Toggle via `?dev=1` query param + F2.

### W4h — Persistent settings

- `localStorage`-backed config (volume, keyboard mappings).
- Plug-in point: RB3's settings flow through `GameConfig`
  (`src/band3/game/GameConfig.cpp`); on Wii these persist to the memory
  card. The web shim intercepts that save/load path and routes it to
  `localStorage` under `HX_WEB`. Identify the exact `GameConfig`
  save/load entry points before wiring.
- A future feature, but cheap to add once the entry points are known.

## Acceptance test

Manual playthrough:

1. Cold load (cache empty): WASM downloads, assets fetch, song loads,
   gameplay completes. Stopwatch the milestones — meet the targets table.
2. Warm load (IDB cache populated): the same playthrough, sub-3s to
   canvas, sub-8s to song_select. Verify IDB cache hit rate via
   DevTools.
3. Bundle and serve from a static host (e.g. nginx with Brotli on) —
   verify production-shape works, not just `server.py`.
4. Run `node scripts/web/smoke-test.mjs --boot-to-song` 10 times in CI;
   expect 100% pass. (The `--boot-to-song` mode is added in W3.)

## Suggested subagent prompt

> Execute Phase W4 of the RB3 web port plan
> (`rb3/docs/plans/web-port/W4_POLISH.md`). W3 has landed: one song plays
> end-to-end in the browser. Drive the WASM size, asset caching, loading
> screen, and memory tuning sub-tasks (W4a-d are highest priority; W4e-h
> are optional). Hit the targets in the metrics table. Do not regress W3
> functionality. Where DC3 has the same problem (e.g. no Phase-8 work
> yet), pattern your solution so it can be backported to DC3 cheaply
> later.
