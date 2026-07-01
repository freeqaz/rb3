# Web Load/Transition Performance Roadmap

> ## ✅ SUPERSEDED / SHIPPED (verified 2026-07-01)
>
> **This roadmap's goal is achieved. Every item R1–R6 shipped** — landed by the
> parallel [`incremental-load-perf`](../incremental-load-perf/) effort (which is the
> canonical, more-complete record; it also adds network-matrix gating at 8/80 +
> 4/150 Mbps, A4 texture mip-strip that takes a 1.5 Mbps client from DNF → gameplay,
> first-frame 410→96 ms, and an L1 vertex-unpack cache). **Do not re-open R1–R6** —
> they are in `HEAD` and re-implementing any of them is pure duplication.
>
> | This roadmap | Shipped as | Commit |
> |---|---|---|
> | R1 async on-demand fetch | `WebPendingFile` async-open seam + `rb3_prefetch_native.cpp` | `79cb7a54` |
> | R2 per-screen prefetch | A2 per-screen dependency bundles (`/api/bundle/screen/<name>`) | `bc651674` |
> | R3 boot bundle | landed here, generalized by R2 | `150a33f8` |
> | R4 streaming song audio | Range-backed moggs (HTTP 206) + N2 read-ahead LRU chunk cache | `79cb7a54` |
> | R5 wire compression | landed here + W5 bytes (SFX PCM→ogg 10×, −36% wire) | `6e627201` / `81972d4c` |
> | R6 post-async CPU floor | Q4 GPU BC texture path (`RB3_BC_TEX_OFF`) | engine `6c45e96` |
>
> **Independent re-baseline (`scripts/web/netperf-suite.mjs`, nav, 2026-07-01) — the
> transition-freeze complaint is measured GONE.** `blockedMs` = tab frozen/unresponsive:
>
> | transition | 2026-06-08 blocked (50 Mbit) | **2026-07-01 blocked (50 Mbit)** |
> |---|---|---|
> | boot → main_hub | 15,500 ms | **35 ms** |
> | main_hub → song_select | 5,900 ms (20 reqs / 12 MB) | **0 ms (1 req / 0 MB — prefetched)** |
> | song_select → part_difficulty | 1,700 ms | **0 ms** |
> | part_difficulty → game | 11,400 ms (6.7 s dead frame) | **0 ms (67 ms worst hitch)** |
>
> Worst single hitch anywhere is now 167 ms (boot), ≤67 ms after. The residual is
> **non-blocking wall-clock transfer** at low bandwidth (throughput-bound: wall ≈
> bytes ÷ bandwidth) — a *bytes* problem, whose lever (A4 texture downscale,
> `RB3_WEB_DOWNSCALE` default-ON, `cc86c7c4`) is already shipped. The
> `part_difficulty → game` wall (~9.6 s unbounded) is the `songMs>0`/intro-cinematic
> reached-heuristic, **not** a transfer cost. The design docs below are kept as the
> historical design record; the live status lives in `incremental-load-perf/`.
>
> ---

> Master index for the web load/transition performance work. Six design docs
> (`R1`–`R6`), each adversarially reviewed. Companion data:
> [`web-netperf-findings-2026-06-08.md`](../web-netperf-findings-2026-06-08.md).
> Verification tool for every item: `scripts/web/netperf-suite.mjs`
> (CDP-throttled boot + per-menu-transition profiler).

## Executive summary

**Root cause:** every on-demand asset *miss* in the web build is fetched through a
**synchronous XHR** (`WebAssetsFetchSync`, `xhr.open(..., false)`) that **freezes the
wasm main thread for the entire transfer** (`freeze ≈ bytes ÷ throughput`). Invisible
on loopback, brutal on a real link: boot frozen 0.9 s (unbounded) → 10.0 s (50 Mbit)
for the same 71 MB / 98 sync XHRs; worst transition `part_difficulty → game` frozen
11.4 s / 6.7 s dead-frame at 50 Mbit. Named offenders are all large binaries:
`20thcenturyboy.mogg` 37 MB → 6.76 s, `colorpalettes.milo_xbox` 21 MB → 3.7 s,
`small_club_01.milo_xbox` 19 MB → 3.5 s, shell vignettes ~12 MB each. **Bandwidth is
fine; synchrony and just-in-timing are the bugs.** The strategy is, in order:
(1) get the on-demand fetch **off the main thread** (async + cooperative JSPI yield) so
no single frame freezes; (2) **warm the bytes ahead of need** (boot bundle + per-screen
prefetch + song-mogg prefetch) so the hot path does no network I/O at all; (3) **shrink
what crosses the wire** (server gzip/brotli) and **shrink the post-fetch CPU floor**
(GPU BC texture upload) once the network freeze is gone. The first three are the
headline wins; R5/R6 are secondary multipliers that compose with everything.

## Item table

| Item | Title | Impact | Effort | Risk | Depends on | Review verdict | Doc |
|---|---|---|---|---|---|---|---|
| **R1** | Async on-demand fetch — remove `WebAssetsFetchSync` blocking | critical | M | medium | — | **rethink** (re-scope fix point) | [R1](R1-async-ondemand-fetch.md) |
| **R3** | Eager boot-bundle expansion (boot `.milo_xbox` → async bundle) | high | M | low | — | **revise** (IDB regression, gzip claim) | [R3](R3-boot-bundle-expansion.md) |
| **R2** | Per-screen manifests + predictive prefetch | high | L | medium | R1, R3 | **revise** (venue timing, delivery, IDB) | [R2](R2-per-screen-prefetch.md) |
| **R4** | Streaming song audio — kill the 37 MB mogg fetch | critical | L | medium | R3 (shared driver) | **revise** (hook timing, sync/async race) | [R4](R4-streaming-song-audio.md) |
| **R5** | Wire compression for `/api/file` (.milo_xbox gzip/brotli) | medium | S | low | — | **ship-as-is** (2 pre-merge fixes) | [R5](R5-wire-compression.md) |
| **R6** | Post-async CPU floor — GPU BC (DXT) decode | medium | M | medium | R1, R2, R3, R4 | **revise** (BC bit-exactness/CI) | [R6](R6-post-async-cpu-floor.md) |

## Recommended sequence

Ordered by **measured impact per unit of risk**, honoring the dependency graph and the
review findings. The two **independent, low-risk** items lead so we bank wall-clock wins
while the harder re-scoping work (R1) is done carefully.

### Wave 0 — land immediately, fully independent (parallel)

- **R5 (wire compression).** S effort, low risk, `server.py`-only, zero C++/wasm/Wii-asm
  risk. Cuts the milo nav working set ~45 % (79 MB → 36 MB), halving milo freeze time
  (~12 s → 5.4 s at 50 Mbit) **on today's synchronous path** — a real win before any of
  the async work lands. Ship with its two pre-merge fixes (use the brotli **CLI** as the
  primary encoder since the Python `brotli` module is absent here; make the cache write
  atomic for the `ThreadingHTTPServer` parallel-miss race).
- **R3 (boot-bundle expansion).** M effort, low risk, attacks the **cold-boot** freeze
  (94 sync milo XHRs / 54 MB / up to 10 s blocked at 50 Mbit) by reusing the proven async
  `/api/bundle` + `onBundleSuccess` MEMFS-unpack path. It is genuinely independent of R1
  and self-contained, so it can run alongside R5. **Must fix the unacknowledged W4b/IDB
  regression first** (see Dependency notes) — onBundleSuccess must write each unpacked
  file back to IndexedDB, else warm/repeat boots regress vs the shipped cache.

These two together collapse the **boot** wall (R3) and shrink **every** transfer (R5)
without touching the loader spine.

### Wave 1 — the foundational async fix (the keystone)

- **R1 (async on-demand fetch), re-scoped.** This is *the* root-cause fix and every
  remaining item leans on it — but **its design as written targets the wrong layer** (it
  rewires `FileLoader::OpenFile`, but none of the named offenders load through
  `FileLoader`). Before implementation, R1 must be re-scoped to the **three real
  blocking-open surfaces** the reviews identified:
  1. **`.milo_xbox`** opens via `DirLoader::OpenFile` → `new ChunkStream(...)` → `NewFile`
     + synchronous `ReadAsync(header)` + synchronous `Fail()` check. Making milos async
     requires re-entrant `DirLoader::OpenFile` **and** a pending-aware `ChunkStream`
     (its `Fail()`/header-read contract), not just `FileLoader`.
  2. **The 37 MB mogg** opens via a direct `NewFile(path, 2)` in `Synth::NewStreamFile`,
     **outside any loader/LoadMgr state machine** — there is no `OpenFile` to re-enter.
     R4's prefetch (below) is the practical fix for this surface; R1's re-entrant-open
     model does not reach it.
  3. **`FileLoader`** (dta/dtx + the no-factory raw-buffer fallback) — the path R1 *did*
     design for; keep it, but recognize it is the minority of the measured freeze.

  The infra premises R1 relies on are all **verified correct** (single-threaded web build
  so `emscripten_fetch`/`-sFETCH=1` is genuinely async; `MILO_WEB_ASYNC`/JSPI on;
  `emscripten_sleep(0)` yield spine present; `WebAssetsFetch`/`WebAssetsFetchDone` async
  primitives exist and are uncalled). The work is in plumbing them through DirLoader +
  ChunkStream, which the doc must add before coding.

R1 is sequenced **after** Wave 0 because R3 already removes the boot freeze through a
simpler, lower-risk mechanism (bundle, no loader-spine surgery), and R5 shrinks the bytes
R1 will later fetch — so R1 lands into a smaller, better-understood problem.

### Wave 2 — warm-ahead-of-need (builds on R1's async primitive + R3's pattern)

- **R4 Layer A (song-mogg async prefetch).** The single worst hitch (6.76 s). Layer A is
  M, low-medium risk: prefetch the highlighted song's mogg during song-select dwell so
  the gameplay open hits warm MEMFS. **Fix the hook** (the doc's `CheckSongPreview` fires
  *at* the preview-delay boundary, giving zero pre-overlap — hook `StartSongPreview` /
  `SetHighlightIx` instead) and **add sync-vs-async coordination** so a fast song-pick
  doesn't issue a *second* 37 MB sync XHR while the async prefetch is still in flight.
- **R2 (per-screen manifests + prefetch).** L effort. Generalizes R3's
  manifest-driven-async-bundle pattern to every screen, warming each screen's working set
  during the prior screen's idle. **Fix three things first:** (a) the venue is
  *performance-selected* (`MetaPerformer::SelectRandomVenue`), not song-derived, so it is
  **not** prefetchable during song-select dwell — only the mogg is; the venue milo
  prefetch realistically overlaps the `part_difficulty → game` *transition*, not the
  dwell. (b) The manifest-delivery wiring is internally contradictory (`WebAssetsFetch`
  serves from `/api/file/` but the plan static-deploys to `build/.../manifests/`) —
  pick one. (c) The resident check must consult the IDB warm cache (`__rb3IdbCache`),
  not just `stat('/data/...')`, or it re-downloads IDB-warm assets.

R4 Layer A and R2 **share one async-prefetch driver** — build it once (see Dependency
notes). R4 Layer A is the higher-value, lower-risk slice (it kills the worst single
freeze) so it leads within the wave; R2 follows and reuses the driver.

### Wave 3 — post-async polish (only meaningful after the freeze is gone)

- **R4 Layer B (range-streamed mogg open).** L, with genuine audio-correctness risk.
  Bounds the *cold* worst case (song picked with no dwell / very slow link) to header
  latency instead of whole-file latency, via 206 range reads. Must pick a real async-read
  contract (an async-completing `ReadDone`, not `emscripten_sleep` inside the read),
  guard the audio-ring producer-starvation (the VorbisReader refill runs on the
  main/JSPI-suspended thread), and verify with `/audio-verify`.
- **R6 (GPU BC texture upload).** M, medium risk. Uploads DXT blocks directly as BC1/2/3
  instead of CPU-expanding to RGBA8 — a per-transition CPU + peak-memory + VRAM win on
  the texture-heavy venue/palette loads, **not** a boot-wall win. Explicitly depends on
  R1–R4 (until the network freeze is gone, the A/B is noise). **Fix the verification
  claim:** BC decode is *visually* but **not pixel-exact** vs RB3's truncating CPU
  decoder, and the native screenshot harness here has a **real BC-capable Vulkan
  adapter** (so BC is live, not inert) — the screenshot diff needs a per-channel
  tolerance (±2 LSB), not a byte-identical compare.

## Dependency notes

**The async-fetch primitive is the shared spine.** R1, R2, R3, R4 all use the engine's
existing async path (`WebAssetsFetch`/`WebAssetsFetchDone` for per-file; the bundle for
R3). Three coordination requirements fall out:

- **One async-prefetch driver, not two.** R4 Layer A (song mogg) and R2 (per-screen
  milos) are the same mechanism (enqueue server-relative path → `WebAssetsFetch` → reap
  via `WebAssetsFetchDone(id)` → IDB write-through). Build it **once** as the generic
  `RB3PrefetchAsset`/`RB3PrefetchPoll` driver; R4's mogg hook and R2's manifest pump are
  thin callers. R4's doc proposes building it generically "so R3 reuses it" — but note
  the generic driver has a **path-normalization caveat**: the async `WebAssetsFetch`
  (`onFetchSuccess`) does **not** resolve `..` components (only `onBundleSuccess` does).
  Song moggs and most milos have no `..`, but some milo paths do — the generic driver
  must normalize, or it writes those to the wrong MEMFS path and the later fopen misses.
- **WebAssetsFetch bookkeeping must be fixed before R2 scales it.** R2 is its first real
  user; today `FetchRequest` objects are `new`'d and never freed, and `WebAssetsFetchDone`
  linear-scans an ever-growing vector. Fine for the bundle's single fetch; a per-screen
  pump issuing hundreds of fetches leaks them all. The pump must track its **own** fetch-id
  set and must **not** key completion on the process-global `sPending`/`WebAssetsAllDone`
  (those are the boot gate, shared with any other async fetch).

**R1 vs R3 vs R2 — overlapping but distinct slices of the same problem:**

- **R3 = "screen 0" of R2.** R3 is the boot working set; R2 is every subsequent screen.
  They share the manifest-generator and the parameterized-bundle plumbing — build R3's to
  be reusable by R2. R3 leads (lower risk, independent of R1, proves the pattern).
- **R1 covers the unpredicted long tail; R2/R3 cover the hot path.** R2/R3 make the read
  *not miss*; R1 makes a *missed* read non-blocking (mispredictions, back-edges, the long
  tail R2's 1-hop horizon doesn't cover). They compose: R2/R3 remove most misses, R1
  removes the freeze on the misses that remain.
- **A subtle contention point (single-threaded build):** while a *synchronous* XHR blocks
  the main thread, the browser cannot dispatch in-flight `emscripten_fetch` `onsuccess`
  callbacks — so a sync miss freezes **and** stalls any concurrent R2 prefetch. This is a
  distinct failure mode that **only R1 fully removes**; until R1 lands, R2's prefetch can
  still be stalled behind a sync miss.

**The W4b IndexedDB warm cache touches R2, R3, R4 — handle it explicitly:**

- **R3 regression (must fix):** today every boot milo fetched via `WebAssetsFetchSync` is
  written back to IDB (`cachePutAfterFetch`), so a *second* cold boot serves all ~94
  milos from local IDB with zero network. R3's bundle path (`onBundleSuccess`) writes
  straight to MEMFS and **never** calls `cachePutAfterFetch` — so warm boots would
  re-download the 54 MB bundle. R3 must write unpacked files back to IDB (or gate the
  bundle off when the boot set is already IDB-cached). The cold-only netperf matrix never
  surfaces this — **add a warm/persisted-IDB run** to the verification.
- **R2 resident check:** must consult `__rb3IdbCache`, not just `stat('/data/...')`, or it
  re-downloads IDB-warm assets, defeating W4b for prefetched files.
- **R4 IDB write-through cost:** `cachePutAfterFetch` does a whole-file `FS.readFile` + JS
  copy + IDB put — for a 37 MB mogg that is a *new* (smaller) main-thread stall on every
  prefetch, plus transient ~2× heap residency (fetch holds the body, then fwrites to
  MEMFS). Account for it; don't reintroduce a freeze the item exists to remove.

**R5 and R6 are complementary, non-overlapping byte-shrinkers — and the docs contradict
each other on venue gzip ratios. The correct numbers (measured):**

- R3 claims venue milos "gzip to ~98 % — useless"; R5/R6 measure `small_club_01` gzips to
  **67.7 %** and `colorpalettes` to **52.3 %** (gzip) / 40.3 % (brotli q11). **R5/R6's
  numbers are correct; R3's 98 % is wrong.** So R5 *does* help the venue milos meaningfully
  (and R3's boot set compresses *better* than its stated ~46 %). R6 then covers the
  residual texture (DXT) payload that even brotli can't shrink. R5 = text/DTA/scene-graph
  bytes; R6 = the DXT block payload on the GPU. They stack; neither replaces the other.
- The **mogg gains nothing** from R5 (already-Ogg) — that is squarely R4's job. R5/R6
  never touch it.
- **R5↔R2 bytes-field caveat:** once R5 ships, a manifest's captured `bytes` (from
  `encodedDataLength`) becomes the *compressed* size — wrong for MEMFS-memory budgeting,
  which needs the raw size. R2's manifest must record raw size for the memory budget.

## Per-item summary

- **[R1 — Async on-demand fetch](R1-async-ondemand-fetch.md)** *(critical / M / medium —
  verdict: **rethink**)*. The root-cause fix: replace the blocking sync-XHR miss path with
  the engine's existing async `WebAssetsFetch` + JSPI yield so only the requesting loader
  suspends. **Must-resolve before coding:** the design rewires `FileLoader::OpenFile`, but
  *none of the named offenders load through FileLoader* — milos go through
  `DirLoader`+`ChunkStream` and the mogg through a direct `Synth::NewStreamFile` `NewFile`.
  Re-scope to those surfaces (DirLoader/ChunkStream re-entrant pending-aware open; mogg via
  R4 prefetch) or the change compiles, passes its checks, and **moves none of the measured
  numbers**. Top open question: `File::Pending()` as a default-false virtual (vtable touch,
  needs an objdiff spot-check) vs. a HX_WEB free-function downcast.

- **[R2 — Per-screen prefetch](R2-per-screen-prefetch.md)** *(high / L / medium — verdict:
  **revise**)*. Captures each screen's deterministic working set into per-screen manifests
  and async-prefetches the next screen during idle, warming MEMFS so the transition does no
  network I/O. Core mechanism is sound and the code map is accurate. **Must-resolve:**
  (1) the venue is performance-selected, not song-derived — only the mogg overlaps
  song-select dwell, the venue prefetch realistically overlaps the transition; (2) the
  manifest-delivery path is self-contradictory (`WebAssetsFetch` `/api/file/` vs.
  `build/`-static-deploy) — pick one; (3) the resident check must consult `__rb3IdbCache`,
  and the pump must track its own fetch-ids (not global `sPending`) and not leak
  `FetchRequest`s. Top open question: trigger lead time (entry vs. dwell timer vs. first
  advancing input).

- **[R3 — Boot-bundle expansion](R3-boot-bundle-expansion.md)** *(high / M / low — verdict:
  **revise**)*. Ships a second curated "boot-assets" async bundle fetched before
  `new App()`, reusing `/api/bundle` + `onBundleSuccess`, so every App-ctor sync milo read
  hits warm MEMFS. Sound core, verified data, the cleanest first big win. **Must-resolve:**
  the unacknowledged **W4b/IndexedDB warm-boot regression** (onBundleSuccess bypasses
  `cachePutAfterFetch` → repeat boots re-download the bundle) — write unpacked files back
  to IDB, and add a warm-boot verification run. Also fix the wrong "venues gzip to ~98 %"
  claim (it's ~52–68 %). Top open question: one combined bundle vs. a separate
  `/api/bundle/boot` route (leaning separate so R2 reuses the per-set convention).

- **[R4 — Streaming song audio](R4-streaming-song-audio.md)** *(critical / L / medium —
  verdict: **revise**)*. Kills the 37 MB mogg freeze: Layer A async-prefetches the mogg on
  song highlight; Layer B range-streams the open so play starts after header + first window.
  Diagnosis and Layer-A architecture are sound. **Must-resolve:** (1) the prefetch **hook is
  wrong** — `CheckSongPreview` fires *at* the preview-delay boundary (zero pre-overlap);
  hook `StartSongPreview`/`SetHighlightIx`; (2) add **sync-vs-async coordination** so a fast
  pick doesn't double-fetch the 37 MB; (3) the `_prev`-mogg / `unk71` claim is conditional
  (true for on-disc songs, diverges for DLC); (4) Layer B's audio-underrun cause is
  producer-starvation (main-thread VorbisReader refill), not the output thread. Top open
  question: prefetch trigger granularity (highlight-with-debounce vs. part_difficulty enter).

- **[R5 — Wire compression](R5-wire-compression.md)** *(medium / S / low — verdict:
  **ship-as-is**, 2 pre-merge fixes)*. Compress `.milo_xbox` on demand in `server.py` with a
  persistent disk cache, served via standard `Content-Encoding` negotiation — zero
  C++/wasm/Wii-asm change (the browser decodes before the engine's XHR sees the bytes).
  Cuts the milo nav set ~45 %. Verified transparent, verified ratios, can land
  **immediately** and helps the *current* synchronous path. **Pre-merge:** the headline
  brotli number requires the Python `brotli` module which is **absent here** — use the
  brotli **CLI** (which build.sh already uses) as the primary encoder; and make the cache
  write **atomic** (temp+rename) to survive the `ThreadingHTTPServer` parallel-miss race.
  Top open question: q5 vs. q9 default on demand (q5 = 0.23 s keeps ~90 % of the win).

- **[R6 — Post-async CPU floor](R6-post-async-cpu-floor.md)** *(medium / M / medium —
  verdict: **revise**)*. Teaches RB3's `Rnd_Wgpu_RB3.cpp::UploadRndTexIfNeeded` to upload
  DXT blocks directly as BC1/2/3 (the path DC3's `TextureConvert.cpp` already proves)
  instead of always CPU-decoding to RGBA8 — a per-transition CPU + peak-memory + VRAM win,
  not a boot-wall win. Plumbing is correct and Wii-safe. **Must-resolve:** the
  "byte-identical" verification claim is **false** — BC hardware decode is visually but not
  pixel-exact vs RB3's truncating integer CPU decoder, and the native screenshot harness has
  a **real BC-capable adapter** (BC is live, not inert), so the existing baselines shift
  sub-LSB; the screenshot diff needs a **±2-LSB tolerance**, not an exact compare. Top open
  question: do the production target browsers/GPUs (esp. mobile/low-end) advertise
  `texture-compression-bc`?

## Recommended first implementation task

**Ship R5 (wire compression) first**, with its two pre-merge fixes (brotli CLI as primary
encoder; atomic cache write).

Why R5 leads:

- **Lowest risk, smallest blast radius, zero decomp exposure.** `server.py`-only, no
  C++/wasm rebuild, no loader-spine surgery, no Wii-asm risk, no engine/vtable change. It
  is the only **ship-as-is** verdict in the set.
- **Immediate, standalone win on today's path.** It shrinks every milo transfer ~45 %
  (~12 s → 5.4 s of milo freeze at 50 Mbit) **before** any async work — it does not wait on
  R1/R2/R3 and benefits the current synchronous build right now.
- **It de-risks and compounds with everything downstream.** Once R3's bundle and R2's
  prefetch land, the bytes they move are already smaller; R5 stacks with all of them and
  needs no rework to do so. (And it usefully forces the R3-vs-R5 venue-gzip contradiction
  to be resolved with real numbers — R5's, which are correct.)
- **Fast feedback loop to validate the whole roadmap's tooling.** R5's A/B is the cleanest
  exercise of `netperf-suite.mjs` (the bytes-drop is the proof), warming up the
  measurement harness every later item depends on.

If the next run wants to attack a **freeze** rather than bytes, the second-best first task
is **R3 (boot-bundle expansion)** — the biggest single first-load freeze win and
independent of R1 — but only with the **IDB write-back fix and a warm-boot verification
run** included from the start (without them, R3 is a net regression for returning users).

> Note on R1: although R1 is the conceptual keystone (critical impact), it is **not** the
> right first task — its design currently targets the wrong layer (`FileLoader` instead of
> `DirLoader`/`ChunkStream`/`Synth`) and must be re-scoped before any code is written.
> Hand R1 to an implementation run *after* R5 + R3 have banked the low-risk wins and the
> re-scope (DirLoader + ChunkStream re-entrant pending-open) has been pinned down.
