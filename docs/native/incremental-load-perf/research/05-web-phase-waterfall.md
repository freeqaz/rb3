# 05 — Web boot + menu-transition phase waterfall (measured)

**Date:** 2026-06-10
**Build:** deployed release `native/web/build/release/rb3-web.wasm` (6.0 MB, `-O2 -g0`,
brotli `.wasm.br` 1.5 MB), built 2026-06-10 05:28. Chromium 1223 (Playwright 1.60),
headless, ANGLE-Vulkan/SwiftShader, JSPI on. **No rebuilds were done.**

> **Headline caveat — localhost makes the network look free.** All fetches were
> served by `native/web/server.py` over loopback, where a 37 MB mogg transfers in
> ~20 ms. The server-side `ms` I log is *disk-read + on-the-fly compress only*; the
> socket write to localhost buffers instantly, so it does **not** include real
> wire-transfer time. Therefore every "network = X ms" number below is a **lower
> bound**. On a real network the same byte volumes (52 MB of boot/menu milos; a
> 32–37 MB cold preview mogg) become seconds of *blocking* sync-XHR stall added on
> top of the CPU costs measured here. The CPU and rAF-freeze numbers are real and
> transferable; the network numbers must be re-scaled by the user's bandwidth.

## Methodology (exact commands)

Server (instrumented wrapper — imports the real `server.py`, monkeypatches
`do_GET` to append per-request `{t0,t1,ms,method,path,status,wireBytes}` NDJSON;
`wireBytes` = bytes actually written to the socket, i.e. post-compression):

```bash
python3 /tmp/rb3perf/server_instr.py --port 8431 --log /tmp/rb3perf/netlog.ndjson
```

Client harness (Playwright, run from `scripts/web/` so `node_modules` resolves; a
`/tmp/rb3perf/node_modules` symlink lets the `/tmp` scripts import `playwright`).
Each script installs an `addInitScript` that records an **epoch-stamped** (`Date.now()`)
rAF-gap trace + boot milestones + `window.rb3BootPhaseLog`, so client events
align with the server's epoch-ms request log. A CDP `Profiler` CPU profile spans
each run.

```bash
cd scripts/web
# 3× cold boot -> main_hub (run1 with CPU profile):
node /tmp/rb3perf/waterfall.mjs --port 8431 --out /tmp/rb3perf/run1
node /tmp/rb3perf/waterfall.mjs --port 8431 --out /tmp/rb3perf/run2 --no-cpu
node /tmp/rb3perf/waterfall.mjs --port 8431 --out /tmp/rb3perf/run3 --no-cpu
# debug build (?debug=true, -O0 -g2) once, for CPU-profile SYMBOL NAMES only:
node /tmp/rb3perf/waterfall_debug.mjs --port 8431 --out /tmp/rb3perf/run_dbg
# song_select entry + 3 cold hovers + 3 warm re-hovers (one session):
node /tmp/rb3perf/preview-hover.mjs --port 8431 --out /tmp/rb3perf/preview
# correlation:
node /tmp/rb3perf/analyze.mjs /tmp/rb3perf/run1
```

Each Playwright `chromium.launch()` is a fresh ephemeral profile → **IndexedDB is
cold every run**, so `/api/bundle/boot` (the 13.75 MB boot-milo bundle) is
re-fetched each time. This matches the user's reported cold-load experience.

## (a) Boot waterfall — 3 cold runs, median (seconds from navigation)

| Phase | run1 | run2 | run3 | **median** | Δ from prev |
|---|---|---|---|---|---|
| `appBooted` (App ctor done) | 3.84 | 3.82 | 3.65 | **3.82** | **+3.82** |
| `intro_movie_screen` visible | 3.96 | 3.99 | 3.76 | **3.96** | +0.14 |
| `splash_screen` (title) | 5.42 | 5.61 | 5.11 | **5.42** | **+1.46** |
| `main_hub_screen` interactive | 7.61 | 7.55 | 8.82 | **7.61** | **+2.19** |

Fixed boot prelude (run1 `rb3BootPhaseLog`, all cheap):

| marker | t | Δ |
|---|---|---|
| fetch_start | 0.52s | wasm fetch+instantiate before this (wasm: 21 ms wire, decoded 6.3 MB) |
| fetch_done | 0.90s | +0.38s — `/api/bundle` (config dta/dtb) 47 ms + `/api/bundle/boot` 181 ms |
| engine_init_done | 0.91s | +0.01s |
| gpu_ready | 1.01s | +0.11s (async WebGPU device) |
| appctor_start | 1.01s | |
| **appctor_done** | **3.84s** | **+2.83s ← the App() constructor body** |

So of the 3.82 s to `appBooted`, ~1.0 s is wasm+bundles+GPU and **~2.83 s is the App
constructor body** (engine init, milo loads, first scene build).

## (b) Per-phase attribution: network vs CPU vs idle

Network is logged per request and bucketed to the phase active at its start. The
**rAF "frozen" column** = Σ(gap−33 ms) of animation-frame gaps inside the phase
window = user-visible main-thread block. (`(program)`/`(idle)` dominate the V8
profile because JSPI attributes suspend/unwind time to the embedder.)

| Phase | wall | sync-XHR (server ms, **localhost LOWER BOUND**) | bytes fetched | rAF frozen | dominant cost |
|---|---|---|---|---|---|
| nav→appBooted | 3.82s | 40 files, **70 ms** + bundles 228 ms | 22 MB files + 15 MB bundles | 1.24s | **wasm CPU** (milo parse + DXT decode + 1st mesh upload) |
| appBooted→intro | 0.14s | ~0 | — | 0.00s | instant |
| intro→splash | 1.46s | 32 files, **72 ms** | 19.5 MB milos | 0.40s | CPU (char/venue milo build) |
| splash→main_hub | 2.19s | 18 files, **27 ms** | 11.8 MB milos | 0.99s | CPU (venue `sv*`/`tv*` shell milos: sv8_a 6.2 MB, sv3_a 5.5 MB) |

**Network is NOT the local bottleneck.** Across the whole boot, ~52 MB of
`.milo_xbox` is served in **169 ms of total server time** (1–58 ms each). The wall
time is CPU + JSPI overhead. Re-scale the byte columns by real bandwidth to get
the on-network stall: e.g. at 20 Mbps the 37 MB boot-bundle + per-phase milos add
~20 s of blocking sync XHR that localhost hides.

### CPU attribution (debug build `?debug=true`, `-O0 -g2`, **shape only** — release
`-g0` wasm has no name section so its profile is unsymbolized; absolute ms differ,
the *ranking* is the signal). App-ctor self-time leaders:

| self ms (-O0) | function | meaning |
|---|---|---|
| 1033 | `vector<uint8>::__construct` | milo asset byte-buffer alloc |
| 816 | **`BandRnd::DrawMesh`** | first-frame mesh GPU upload |
| 814 | `vector<uint8>::__base_dest` | buffer teardown |
| ~600 (sum) | `DecompressDXT1Block`/`DecompressDXT5`/`DXT5AlphaBlock`/`ByteSwapDXT16` | **software DXT texture decode** of `.milo_xbox` textures |
| ~550 (sum) | `BeDec4n`/`BeColor`/`BeFloat`/`BeUDec4n`/`Half2Float` | **big-endian→LE vertex/color swap** (Xbox 360 milos) |
| 354 | `flushPendingWrites`+`saveResponseAndStatus` | **sync-XHR + MEMFS-write JS glue** (even on localhost) |
| 299 | `yylex`/`yy_get_previous_state` | DTA text-lexer (config parse) |
| — | `BinStream::Read`/`ChunkStream::ReadImpl`/`DataInput` | milo chunk parse |

→ The boot/menu cost is **milo parse (endian-swap) + DXT decode + first-draw mesh
upload**, on the main thread, with sync-XHR JS glue a secondary ~350 ms.

## (c) Frame-rate trace (rAF gaps), whole boot→main_hub (run1)

`samples=385  max=654 ms  >100ms:9  >250ms:3  >500ms:1  >1000ms:0`
Worst gaps (ms): 654, 312, 257, 204, 178, 155, 142, 127, 106…

Per-phase longest gap / frozen time:
`navStart 654 ms / 1.24 s` · `intro 257 ms / 0.40 s` · `splash 155 ms / 0.99 s` ·
`main_hub 52 ms / 0.02 s` (smooth once interactive).
No single >1 s freeze; the stall is **many 100–650 ms blocks** stacked across the
boot/menu transitions (the JSPI per-`emscripten_sleep(0)` 4–16 ms suspends amortise
these but cannot hide a ~650 ms synchronous milo-build task).

## (d) Song-preview hover — cold vs warm (one session)

Hover = `ArrowDown` onto a new song row + 3.8 s settle. The preview path has a
fixed **~1017 ms Request→Prepare debounce** (constant cold AND warm — it's a timer,
not load time); the *load freeze* lands right after "Preparing".

| event | song | longest rAF gap | frozen | mogg fetched (size / server ms) |
|---|---|---|---|---|
| song_select entry | — | 197 ms | 0.19 s | none (33 UI/venue milos, 12 MB, served 46 ms) |
| **cold** hover 1 | 123 | 23 ms | 0.00 s | none (no preview mogg) |
| **cold** hover 2 | 20thcenturyboy | **509 ms** | **0.48 s** | **37.4 MB / 20 ms** |
| **cold** hover 3 | 25or6to4 | **443 ms** | **0.41 s** | **32.0 MB / 19 ms** |
| **warm** hover 1 | 123 | 18 ms | 0.00 s | none |
| **warm** hover 2 | 20thcenturyboy | 46 ms | 0.01 s | **none (MEMFS-resident)** |
| **warm** hover 3 | 25or6to4 | 17 ms | 0.00 s | **none (MEMFS-resident)** |

**Cold preview-hover attribution:** the freeze is ~0.4–0.5 s. On localhost the 32–37
MB mogg transfers in ~20 ms, so the freeze is **CPU**: mogg HvDecrypt/setupCypher/
GrindArray crypto (visible in the `MOGG_DBG` trace) + vorbis decode prime. **On a
real network the full 32–37 MB is a *blocking sync XHR*** (`MoggClip::EnsureLoaded`
→ `PollUntilLoaded`, MoggClip.cpp:200) — at 20 Mbps that alone is ~13–15 s of frozen
canvas added to the ~0.45 s CPU freeze.

**Is the second hover fast? YES.** Warm re-hover of the same song issues **zero mogg
fetch** (the first fetch left it MEMFS-resident; `native_file.cpp` `memfsResident()`
short-circuits the sync XHR) and the freeze collapses to **17–46 ms (≈ 0 frozen)** —
a ~10× improvement. Note: the in-session warm path is **MEMFS residency**, not the
IDB cache — `__rb3CachePut` deliberately does *not* repopulate the in-memory
`__rb3IdbCache` (OOM fix, `rb3-web.js:330`), so IDB only helps on the *next page
load* (boot pre-warm), while same-session re-hovers ride MEMFS.

## Takeaways (for the fix roadmap, not done here)

1. **Cold preview hover is the worst single stall** and is a textbook sync-drain:
   `MoggClip::EnsureLoaded` → `PollUntilLoaded` of a 30–40 MB mogg. On a real
   network this is multi-second. Candidate: async `kLoadBack` the preview mogg and
   keep the canvas live; or Range-fetch only the preview window, not the whole song.
2. **Boot/menu stalls are CPU**, not local network: ~2.8 s App-ctor + ~0.4 s/0.99 s
   intro/splash freezes are DXT decode + BE→LE milo swap + first-draw mesh upload.
   Candidates: pre-swap/pre-decode assets at extraction time (ship LE + raw-format
   textures), or move DXT decode off the main thread.
3. **Warm same-session hovers are already smooth** via MEMFS residency — the
   problem is purely the *first* touch of each asset.

### Files
- Wrapper server: `/tmp/rb3perf/server_instr.py`
- Harness: `/tmp/rb3perf/waterfall.mjs`, `preview-hover.mjs`, `analyze.mjs`
- Raw data: `/tmp/rb3perf/run{1,2,3,_dbg}/`, `/tmp/rb3perf/preview/`
  (`wf.json`, `netlog.ndjson`, `console.ndjson`, `boot.cpuprofile`, `events.json`).
