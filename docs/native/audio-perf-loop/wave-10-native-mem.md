# Wave 10 — native-mem probe (control + shared-engine leak finder)

**Author:** wave-10 native-mem investigator. **Date:** 2026-06-09.
**Question:** does the shared Milo engine leak memory during steady song playback?
(Native has NO JS GC, so it isolates an ENGINE leak from a web-JS-glue leak.)

**VERDICT: REFUTED — the shared engine does NOT leak during steady song playback.**
RSS is FLAT (bounded oscillation, no monotonic growth) across 2 min and 3.1 min of
continuous gameplay audio. This localizes any remaining web jitter/leak to the
JS glue (or to a non-leak cause), NOT the shared C++ engine.

---

## Method

- **Build:** clean native binary in a fresh worktree (`tools/setup-worktree.sh
  wt-w10-nativemem`, branch `wt-wt-w10-nativemem` from HEAD `437987c1`). The MAIN
  repo is currently UNBUILDABLE for native (a concurrent agent's uncommitted edit
  to `native/src/rndobj_synth_link_stubs.s` removed the `CleanupGpuMesh` weak stub);
  the worktree from HEAD has the stub intact (`grep -c CleanupGpuMesh` = 2). Did NOT
  build in main, did NOT touch that file anywhere.
  - Configure: `cmake -B native/build-native native -DMILO_ENGINE_PATH=.../milo-native-engine
    -DDawn_DIR=.../dc3-decomp-deps/dawn/lib/cmake/Dawn -DCMAKE_BUILD_TYPE=Debug
    -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++`
    (matched the main repo's CMakeCache: clang LP64, Dawn from dc3-decomp-deps).
  - Engine pin matches: `MILO_ENGINE_PIN 58901477` == milo-native-engine HEAD `5890147`
    (this is the adaptive-output-latency-buffer engine, the current canonical state).
  - Build: `cmake --build native/build-native --target rb3-native -j16` → green, 42s,
    113 MB binary.
- **Run:** headless with real audio pumping + HTTP debug API:
  `MILO_AUDIO=1 MILO_AUDIO_BACKEND=null MILO_HEADLESS=1 RB3_GAME=1 RB3_HTTP=1
   RB3_HTTP_PORT=<free> RB3_DATA=<extracted> RB3_GAME_INPUT=<canonical boot->song nav>`.
  Nav = the standard quickplay→guitar/expert→nofail→autohit sequence (song self-plays).
- **Audio path is genuinely exercised:** the miniaudio **null backend runs its
  data-callback thread at real-time pacing**, which calls `AudioDevice::MixSources`
  (decode → mix → limiter → resample) and advances the song clock. Confirmed by
  `/api/health` `songMs` advancing **~0.93–0.95x realtime** (5193ms → 191614ms over
  200s). This is the SAME decode→mix→resample→ring pump path that web uses (web swaps
  the miniaudio sink for the SAB ring; the upstream decode/mix/resample is shared).
- **Measurement:** `scripts/native/w10_rss_steady.py` — launches its OWN instance
  (own pid + own free port), waits until `songMs > 5000` (genuine steady playback,
  past count-in/intro), then samples `VmRSS` from `/proc/<pid>/status` every **0.5s**
  for the requested window. Kills ONLY its own process group (never a sibling agent's).
  Linear-regression slope (MB/min) over the full window AND the 2nd half; peak/min/band.

## Raw numbers

### Run A — 120s steady window (`/tmp/w10_rss_steady.csv`, 240 samples)
- screen = `game_screen` throughout; songMs **5144 → 118979** (113.8s of song, 0.95x RT)
- RSS first **447.7MB**, last **437.4MB**, **net delta −10.26MB**
- peak **467.3MB @ 45s**, min **431.2MB**, band **36.0MB**
- slope full-window **+4.9 MB/min**, slope 2nd-half **−1.5 MB/min**
- 10s-interval trajectory (MB): 447.7 / 440.5 / 447.1 / 435.2 / 435.2 / 439.3 / 439.4 /
  448.9 / 456.9 / 449.0 / 449.2 / 449.2  → flat, oscillating; last samples pinned 447940 kB.

### Run B — 200s steady window (`/tmp/w10_rss_steady_long.csv`, 399 samples) — CONFIRMATION
- screen = `game_screen` throughout; songMs **5193 → 191614** (186.4s of song = 3.1 min, 0.93x RT)
- RSS first **459.5MB**, last **448.0MB**, **net delta −11.52MB**
- peak **467.8MB**, min **431.5MB**, band **36.3MB**, mean **442.1MB** (n=399)
- slope full-window **+1.3 MB/min**, slope 2nd-half **+3.1 MB/min**
- 10s-interval trajectory (MB): 459.5 / 448.4 / 445.4 / 437.4 / 437.5 / 445.5 / 445.7 /
  437.7 / 445.8 / 447.0 / 435.1 / 439.2 / 435.3 / 439.3 / 439.4 / 463.6 / 435.6 / 443.7 /
  431.8 / 452.0  → no monotonic trend; t=180s (431.8MB) ≈ t=10s (448.4MB) ≈ t=190s (452MB).

### Independent corroboration
- A concurrent sibling worktree (`domino2-fix`) running rb3-native gameplay showed
  **RSS 442868 kB (~432MB)** — same ~440MB working set as my measurements, confirming
  ~440MB is the normal RB3-native gameplay working set, not a per-session-growing leak.

## Interpretation

- **RSS is FLAT.** Across 2 min and 3.1 min of continuous song playback the RSS sits
  in a tight **~36MB oscillation band (431–468MB, mean ~442MB)** with **zero upward
  trend**; the **net delta is NEGATIVE** in both runs (−10.3MB / −11.5MB).
- The positive full-window slopes (+1.3 / +4.9 MB/min) are **least-squares artifacts
  of fitting a line through oscillating data**, not real growth — refuted by (a) the
  negative net deltas, (b) the flat 10s-interval trajectories, (c) the 2nd-half slope
  flipping sign between runs (−1.5 vs +3.1 MB/min = noise), and (d) the last samples
  pinning to a constant. A real leak at even +5 MB/min would have added ~16MB over
  Run B's window and shown a monotone climb; instead it ended 11.5MB BELOW where it
  started. Bounded oscillation = an engine that allocates a working set and reuses it.
- **massif/heaptrack NOT run:** per the wave plan, RSS slope is the decisive cheap
  signal and only escalate to massif if RSS shows a clear leak. RSS is flat → no
  escalation warranted (massif would be ~50x slower for zero added signal here).

## Localization (the value of this control)

A FLAT native RSS during the exact same decode→mix→resample pump path that web uses
means **the shared C++ engine is not the source of a per-session memory leak during
steady playback.** Any remaining web heap growth / GC-pause jitter after the IDB
warm-cache fix (`b79cbafa`) is therefore in the **web JS glue** (Emscripten MEMFS,
fetch buffers, JS-side Maps/closures/ArrayBuffers, or the SAB/worklet message path) —
or is not a leak at all (e.g. a long render/parse frame stalling the main-thread pump).
The other wave-10 probes (web-capture with CDP v8.gc tracing; web-leak-audit) own that
half.

## Artifacts
- Build: `/home/free/code/milohax/rb3/.claude/worktrees/wt-w10-nativemem/native/build-native/rb3-native`
- Scripts (in the worktree):
  - `scripts/native/w10_rss_steady.py` — dense steady-playback RSS sampler (the decisive tool)
  - `scripts/native/w10_rss_continuous.py` — continuous-from-boot sampler (crash-robust)
  - `scripts/native/w10_rss_measure.py` — first cut (superseded by w10_rss_steady.py)
- Data: `/tmp/w10_rss_steady.csv` (120s, 240 samples), `/tmp/w10_rss_steady_long.csv` (200s, 399 samples)
- Native logs: `/tmp/w10-steady-<port>.log`

## Notes / gotchas
- `MILO_AUDIO_BACKEND=null` DOES exercise the full audio pipeline + advance the song
  clock at real time (null backend = miniaudio's real-time-paced silent sink). songMs
  advancing ~0.95x realtime is the proof the song is genuinely playing, not frozen.
- The headless frame loop is UNPACED (free-runs to thousands of fps); do NOT use frame
  count as a clock. Use `songMs` (real-time audio clock) to gate "steady playback."
- An earlier harness iteration logged process exit code -9: that was MY script's own
  `killpg` in its finally-block racing the poll, NOT a crash — a directly-launched
  detached instance survived indefinitely (verified 65s+, songMs advancing). The
  STATE.md "native frame-242 __dynamic_cast SIGSEGV at main_hub->song_select" did NOT
  reproduce on HEAD `437987c1` (every run reached `game_screen` cleanly).
