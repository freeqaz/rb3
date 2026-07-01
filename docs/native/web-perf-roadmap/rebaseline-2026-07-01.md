# Web perf re-baseline — 2026-07-01 (roadmap close-out verification)

Independent verification that the web-perf-roadmap (R1–R6) goal is achieved. Measured
with `scripts/web/netperf-suite.mjs` (CDP `Network.emulateNetworkConditions`, nav
scenario, cold IDB per run) against the fresh release build (`native/web/build/release`,
wasm mtime 2026-07-01 16:53). This suite's angle is the **transition main-thread
`blockedMs`** (tab frozen/unresponsive) — complementary to the incload effort's
bytes-focused `scripts/web/_netmatrix.mjs`.

## Headline: the synchronous-freeze complaint is GONE

`blockedMs` = summed long-task time the tab was frozen. Compared to the original
2026-06-08 baseline (`../web-netperf-findings-2026-06-08.md`):

| transition | 2026-06-08 blocked @50 Mbit | 2026-07-01 blocked @50 Mbit | reduction |
|---|---|---|---|
| boot → main_hub | 15,500 ms | 35 ms | −99.8% |
| main_hub → song_select | 5,900 ms (20 req / 12 MB) | 0 ms (1 req / 0 MB, prefetched) | −100% |
| song_select → part_difficulty | 1,700 ms | 0 ms | −100% |
| part_difficulty → game | 11,400 ms (6.7 s dead frame) | 0 ms (67 ms worst hitch) | −100% |

Worst single user-visible hitch anywhere: **167 ms** (during boot), ≤67 ms after boot.
The `main_hub → song_select` collapse to a single request is the R2/T9 per-screen
bundle prefetch working live (next-screen working set fetched during the prior screen).

## Full 3-profile matrix (nav, 1 run each)

### low — 50 Mbit/s, 30 ms RTT
| transition | wall | network | blocked | max RAF gap |
|---|---|---|---|---|
| boot→main_hub | 7.9 s | 31.9 MB / 363 req | 35 ms | 167 ms |
| main_hub→song_select | 2.1 s | 0.0 MB / 2 req | 0 ms | 50 ms |
| song_select→part_difficulty | 1.9 s | 3.3 MB / 4 req | 0 ms | 17 ms |
| part_difficulty→game | 6.4 s | 20.5 MB / 30 req | 0 ms | 67 ms |

### normal — 200 Mbit/s, 15 ms RTT
| transition | wall | network | blocked | max RAF gap |
|---|---|---|---|---|
| boot→main_hub | 5.5 s | 43.1 MB / 316 req | 33 ms | 133 ms |
| main_hub→song_select | 2.1 s | 0.1 MB / 2 req | 0 ms | 67 ms |
| song_select→part_difficulty | 1.9 s | 2.9 MB / 4 req | 0 ms | 50 ms |
| part_difficulty→game | 9.6 s | 17.5 MB / 23 req | 0 ms | 67 ms |

### local — unbounded, 0 ms RTT
| transition | wall | network | blocked | max RAF gap |
|---|---|---|---|---|
| boot→main_hub | 3.8 s | 42.3 MB / 314 req | 41 ms | 117 ms |
| main_hub→song_select | 2.0 s | 0.0 MB / 1 req | 0 ms | 17 ms |
| song_select→part_difficulty | 1.9 s | 2.9 MB / 4 req | 0 ms | 33 ms |
| part_difficulty→game | 9.6 s | 17.5 MB / 23 req | 0 ms | 67 ms |

## Reading the residual

- **All remaining cost is non-blocking wall-clock transfer**, not freezes. At low
  bandwidth the pipe is saturated: `wall ≈ bytes ÷ bandwidth`. The only lever left is
  **bytes** — which is the A4 texture-downscale path (`RB3_WEB_DOWNSCALE` default-ON,
  `cc86c7c4`) plus the W5 SFX-ogg / brotli work, all already shipped.
- The `part_difficulty → game` "wall" (~9.6 s even unbounded, 0 ms blocked) is **not a
  transfer cost** — it's the reached-heuristic waiting on `songMs>0`, which includes the
  intro cinematic. Do not read it as a load stall (the documented "don't screenshot at
  +4 s" gotcha).

Live/ongoing status for the load-perf frontier lives in `../incremental-load-perf/`.
Raw run artifacts for this baseline were written to `/tmp/netperf-rebaseline-2026-07-01/`
(transient) — re-run `node scripts/web/netperf-suite.mjs --scenario nav` to regenerate.
