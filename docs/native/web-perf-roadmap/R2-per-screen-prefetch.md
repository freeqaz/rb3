# R2 — Per-screen asset manifests + predictive prefetch

Status: design (handoff to implementation agent)
Owner doc for roadmap item **R2**. Companion data: `docs/native/web-netperf-findings-2026-06-08.md`.
Verification tool: `scripts/web/netperf-suite.mjs`.

## Problem & data

The web port fetches every on-demand asset **miss** through a *synchronous* XHR
(`WebAssetsFetchSync`, `xhr.open(url, false)`) that freezes the wasm main thread
for the entire transfer. The freeze is `bytes ÷ throughput`, invisible on loopback,
brutal on a real link. Measured (`web-netperf-findings-2026-06-08.md`):

- Boot main-thread **blocked**: 0.9 s (unbounded) → 2.9 s (200 Mbit) → 10.0 s (50 Mbit),
  same 71 MB / 98 sync XHRs.
- Worst transition `part_difficulty → game`: **3.6 s frozen / 2.3 s dead-frame** at
  200 Mbit; **11.4 s / 6.7 s** at 50 Mbit.
- `boot → main_hub`: 6.2 s blocked at 200 Mbit, **15.5 s** at 50 Mbit.
- Named offenders per transition are a *small, fixed, knowable* set of large milos +
  the song mogg:
  - `songs/.../20thcenturyboy.mogg` — 37.4 MB → 6.76 s freeze (the single worst hitch).
  - `char/main/shared/gen/colorpalettes.milo_xbox` — 20.8 MB → 3.7 s.
  - `world/venue/small_club/.../small_club_01.milo_xbox` — 19.4 MB → 3.5 s.
  - `world/vignette/shell/gen/sv*_a.milo_xbox` — ~12 MB each.

Crucial property the data proves: **the per-screen working set is small and
deterministic** (the netperf waterfall *is* the manifest — 188 unique file requests
across the whole nav path, no redundant refetch, MEMFS dedups within a session). The
transfer time is also fine — 71 MB at 200 Mbit is ~3 s of actual bytes. The cost is
purely that the fetches are (a) **synchronous** and (b) **just-in-time** instead of
ahead-of-need.

R1 (async miss path) removes the *synchronous* half: a miss no longer freezes the
thread, but the engine read still **stalls on first touch** until the bytes arrive
(it has to block *somewhere* — the data isn't there yet). **R2 removes the
just-in-time half**: warm MEMFS *before* the screen needs it, during the previous
screen's idle time, so the engine's reads hit a populated filesystem and the
transition does no network I/O at all. R1 and R2 together collapse the throughput
tiers toward the loopback numbers.

## Architecture

Two cooperating pieces — a **build-time manifest pipeline** and a **runtime prefetch
driver** — plus a small server endpoint.

### (a) Per-screen manifests (build-time, captured from real navigation)

A *manifest* is the ordered list of server-relative asset paths a given UI screen
touches when it loads, **minus** what was already resident before the screen (the
boot bundle's `.dta/.dtb`, plus assets warmed by earlier screens). One JSON file per
screen, keyed by the engine's screen name (`main_hub_screen`, `song_select_screen`,
`part_difficulty_screen`, `game_screen`, …) — the exact string already published to
`window.rb3CurrentScreen` via `PublishCurrentScreen()` (main_web.cpp:230).

Manifests are **captured**, not hand-written, by instrumenting a real headless boot+nav
run and recording the `/api/file/<rel>` requests issued in each screen's window — the
same windowing `netperf-suite.mjs` already does (`trackNetwork` + `inWindow`). A
generator script replays the canonical nav path, buckets the waterfall by the
screen-transition timestamps, diffs out the already-resident set, and writes
`native/web/manifests/<screen>.json`. Re-running the generator after an asset change
regenerates them; a CI/`build.sh` check flags drift.

Two refinements that make the manifests robust rather than brittle:

- **Song/venue parameterization.** The `game_screen` working set is *mostly* fixed
  (track milos, HUD, shared kit banks) but contains one variable-by-selection chunk:
  the chosen song's `.mogg` + chart + the venue milo for that song's venue. Split the
  manifest into a **static part** (always the same for that screen) and a **dynamic
  part** resolved at runtime from the current selection (song shortname → mogg path;
  song → venue → venue milo). The static part ships as the JSON; the dynamic part is
  computed by the driver from engine state (`TheMusicLibrary->GetHighlightedNode()` /
  `BandSongMgr`) at trigger time.
- **Size annotation.** Record each entry's byte size in the manifest so the driver can
  budget (fetch biggest-first, or cap concurrent bytes in flight) and so the loading
  UI can show real progress.

Manifest format (one file per screen):

```json
{
  "screen": "part_difficulty_screen",
  "generated": "2026-06-08T...",
  "assetVersion": "<token from /api/version>",
  "static": [
    { "path": "ui/overshell/gen/part_difficulty.milo_xbox", "bytes": 1234567 },
    { "path": "world/vignette/transition/gen/tv6_a.milo_xbox", "bytes": 6300000 }
  ],
  "dynamic": [
    { "kind": "song_mogg" },        // resolved to songs/<short>/<short>.mogg at runtime
    { "kind": "song_venue_milo" }   // resolved via song->venue->world/venue/.../*.milo_xbox
  ]
}
```

### (b) Prefetch driver (runtime)

A new translation unit, `native/src/rb3_prefetch_web.cpp` (`__EMSCRIPTEN__`/`HX_WEB`
only), owns:

- **A static screen-flow graph**: `current screen → likely-next screen(s)`, encoding
  the canonical path `main_hub → song_select → part_difficulty → game` (plus a couple
  of obvious back-edges). Small hand-authored table; the manifests are the heavy data.
- **A transition observer.** The boot loop already publishes the current screen every
  8 frames (`PublishCurrentScreen`, main_web.cpp:602). The driver hooks the same
  `TheUI.CurrentScreen()->Name()` read: on a *change* of screen name, look up the
  next-screen(s) for the new current screen and **kick off async prefetch of their
  manifests** — but only after the current screen has finished its own loading (so we
  don't contend with the in-progress transition's bytes).
- **An idle-gated async fetch pump.** For each manifest entry not already resident in
  MEMFS (`access(path, F_OK)` / `stat`), call the engine's existing **async**
  `WebAssetsFetch(serverPath)` (WebAssets.cpp:118) — which downloads via
  `emscripten_fetch` and writes to `/data/<serverPath>`, **the exact path the sync
  read path anchors to** (native_file.cpp:154 prepends `/data/`). So a prefetched file
  is indistinguishable from one that was synchronously fetched: the later `fopen`
  inside `NativeStdioFile` simply succeeds with no network round-trip. The pump bounds
  concurrency (e.g. ≤2 in flight, biggest-first by the manifest's `bytes`) so prefetch
  never saturates the link the active screen might still need, and it only runs while
  the current screen is interactive/quiescent.
- **A tick hook** called once per frame from `BOOT_RUNNING` in `mainLoop`
  (main_web.cpp:553), after `PublishCurrentScreen`: `RB3PrefetchPoll()`. It (1)
  detects screen change → enqueues the next screen's manifest, (2) advances the async
  fetch pump (start next when a slot frees), (3) reconciles with the IDB cache.

Control flow per transition (the win):

```
on screen change to S:
   nextScreens = flow[S]
   for N in nextScreens:
       manifest = load("native/web/manifests/N.json")   // tiny JSON, prefetched too
       for entry in manifest.static + resolveDynamic(manifest, engineState):
           if not resident(/data/entry.path):
               enqueue async WebAssetsFetch(entry.path)   // background, non-blocking
   ... user dwells on S; pump drains in the background ...
on user advances to N:
   engine reads entry.path -> fopen hits warm MEMFS -> no sync XHR, no freeze
```

### Server change

`_serve_bundle` (server.py:272) bundles `.dta/.dtb` only. R2 does **not** need a new
bundle format — it reuses per-file `/api/file/<rel>` (already async-capable, supports
range). Add one tiny endpoint or static route so the driver can fetch manifests:
serve `native/web/manifests/*.json` (either as a static route under `build/` deployed
by `build.sh`, or a `/api/manifest/<screen>` reader). Either is trivial; static-deploy
is simplest and lets manifests be IDB/HTTP-cached like any asset.

## Implementation plan (high-level, phased)

**Phase 0 — manifest capture tooling (no engine change).**
- Add `scripts/web/gen-screen-manifests.mjs`. It reuses `scripts/web/lib/core.mjs`
  (Playwright/CDP harness) and the `trackNetwork` + screen-window bucketing already in
  `netperf-suite.mjs` (`instrument()`/`screenLog`, `inWindow(a,b)`). Drive the
  canonical nav path, bucket `/api/file/<rel>` requests by screen-transition
  timestamps, diff out the already-resident set (boot bundle + earlier screens), and
  emit `native/web/manifests/<screen>.json` with per-entry `bytes` + the current
  `/api/version` token. Tag the song/venue-variable entries as `dynamic` rather than
  baking the specific song.
- Add an `npm`/make wrapper + a drift check (re-generate, `git diff --exit-code` on the
  manifests dir) so stale manifests are caught.

**Phase 1 — prefetch driver scaffold (engine-side, behind an env flag).**
- New TU `native/src/rb3_prefetch_web.cpp` exporting `RB3PrefetchInit()` and
  `RB3PrefetchPoll()` (declared `extern` in main_web.cpp, like the other web hooks).
  Add it to the web target source list in `native/CMakeLists.txt` (near the other
  `rb3_*_web.cpp`).
- Manifest loading: fetch `manifests/<screen>.json` via async `WebAssetsFetch` (it's
  tiny — a few KB), parse with a minimal JSON reader (or reuse the DTA/JSON facility
  already linked; a hand-rolled parser is fine for this fixed shape).
- Screen-flow table (`static const` map of screen-name → next-screen-name[]).
- Resident check: `stat("/data/<rel>")` before enqueuing (skip warm files).
- Async pump: wrap `WebAssetsFetch` with a bounded in-flight count; track ids via the
  existing `WebAssetsFetchDone(id)` / `WebAssetsPendingCount()` counters. Biggest-first
  by `bytes`.
- Gate everything behind `RB3_PREFETCH` env (URL param `?prefetch=1` via the existing
  `ApplyUrlLoaderEnv` mechanism, main_web.cpp:169) so it can be A/B'd off for
  regression measurement. Default off in Phase 1, default on once verified.

**Phase 2 — wire the trigger + dynamic resolution.**
- Call `RB3PrefetchInit()` once at `BOOT_APP_CTOR` (after `new App`) and
  `RB3PrefetchPoll()` each frame in `BOOT_RUNNING` (main_web.cpp:553), right after the
  `PublishCurrentScreen()` block (which already reads `TheUI.CurrentScreen()`).
- Implement `resolveDynamic`: for `game_screen`, read the highlighted/selected song
  (`TheMusicLibrary->GetHighlightedNode()->GetToken()` — already used by
  `PublishHighlightedSong`, main_web.cpp:293) → `songs/<short>/<short>.mogg`, and the
  song's venue → venue milo path (via `BandSongMgr`/song metadata). Prefetch these as
  soon as the song is *highlighted* on `song_select`/`part_difficulty` (earliest point
  the selection is known), maximizing dwell-time overlap for the 37 MB mogg.

**Phase 3 — manifest static-deploy + server route.**
- Have `scripts/web/build.sh` copy `native/web/manifests/` into the deploy dir
  (`native/web/build/<release|debug>/manifests/`) so they're served + cacheable. No new
  endpoint strictly required; optionally add `/api/manifest/<screen>` to server.py for
  the dev-server convenience path.

**Phase 4 — default on + tune.**
- Flip `RB3_PREFETCH` default on, tune in-flight concurrency + the trigger lead (how
  early to start, e.g. prefetch `game` assets the moment a song is highlighted vs. on
  `part_difficulty` entry).

## Key files & call sites (verified)

Engine / fetch primitives:
- `milo-native-engine/src/platform/WebAssets.cpp:118` `WebAssetsFetch(const char*)` —
  **the async prefetch engine.** Queues `emscripten_fetch` → `onFetchSuccess`
  (`:60`) writes bytes to `/data/<serverPath>`. **Currently has zero callers** (only
  `WebAssetsFetchSync` + `WebAssetsFetchBundle` are wired) — R2 is its first user.
- `WebAssets.cpp:145` `WebAssetsFetchDone(int id)`, `:331` `WebAssetsAllDone()`,
  `:332` `WebAssetsPendingCount()` — pump bookkeeping.
- `WebAssets.cpp:261` `WebAssetsFetchSync(const char*)` — the *blocking* path R2
  bypasses by warming MEMFS first. (Verifies bytes land at the same `/data/...` path.)
- `WebAssets.h:16-37` — async/sync/bundle API surface.

RB3 read path (proves warm-MEMFS interception works):
- `native/src/native_file.cpp:128-168` — the `__EMSCRIPTEN__` on-demand block inside
  `NativeStdioFile::NativeStdioFile`. Order is: `fopen` → on miss, IDB `cacheTryHit`
  → else `WebAssetsFetchSync`. A prefetched file makes the **first `fopen` succeed**,
  short-circuiting all of it. `:154` prepends `/data/` to relative paths — the same
  anchor `WebAssetsFetch` uses, so prefetch + read agree on one absolute path.

Boot loop / trigger points:
- `native/src/main_web.cpp:230` `PublishCurrentScreen()` — already reads
  `TheUI.CurrentScreen()->Name()`; the screen-change signal R2 hooks.
- `main_web.cpp:293` `PublishHighlightedSong()` — already resolves the selected song
  token via `TheMusicLibrary->GetHighlightedNode()->GetToken()`; reuse for the dynamic
  mogg/venue resolution.
- `main_web.cpp:553` `case BOOT_RUNNING` — per-frame tick; add `RB3PrefetchPoll()` after
  the `(sFrameCount & 7)` publish block (`:602`).
- `main_web.cpp:521` `case BOOT_APP_CTOR` — add `RB3PrefetchInit()` after `new App`.
- `main_web.cpp:169` `ApplyUrlLoaderEnv()` — pattern to add `?prefetch=` → `RB3_PREFETCH`.

UI screen-transition chokepoint (alternative/stronger hook than polling the name):
- `src/system/ui/UI.cpp:665` `UIManager::GotoScreenImpl(UIScreen*, bool, bool)` — the
  single funnel all screen changes pass through (`GotoScreen`/`PushScreen`/`PopScreen`
  all reach it). `:682` constructs `UIScreenChangeMsg(scr, mCurrentScreen, …)`. A
  precise prefetch trigger could subscribe to `UIScreenChangeMsg` or add an
  `#ifdef HX_WEB` notify here — gives the **incoming** screen name *before* it loads,
  earlier than the published-name poll. (Polling the published name in Phase 1 is
  simpler; promote to this hook if lead-time matters.)

Loader (why warm MEMFS is sufficient, no loader change needed):
- `src/system/utl/Loader.cpp:163` `PollUntilLoaded`, `:257` `PollUntilEmpty`,
  `:311+` the `HX_WEB` budgeted-drain `Poll` (`RB3_LOADER_BUDGET_MS` /
  `RB3_LOADER_YIELD_MS`). The `FileLoader` state machine reads via
  `ReadAsync`/`ReadDone` → `NativeStdioFile::Read` → `fread` from MEMFS
  (native_file.cpp:196-204, synchronous). Once a file is resident, these reads are
  pure memory copies. R2 changes *when bytes arrive in MEMFS*, not the loader.

Server / manifest deploy:
- `native/web/server.py:272` `_serve_bundle` (`BUNDLE_EXTS={'.dta','.dtb'}`) — unchanged.
- `server.py:322` `_serve_asset_file` — already async-capable + range-capable; serves
  the prefetched milos/moggs. Add manifest static route / `/api/manifest/<screen>` here.
- `native/web/rb3_pre.js` (`window.__rb3IdbCache`, `__rb3CachePut`, `__rb3IdbReady`) —
  the W4b IDB warm cache. R2 prefetch should **also** write through to IDB (call the
  existing `cachePutAfterFetch`/`window.__rb3CachePut` after a successful async fetch)
  so prefetched assets persist for the next session, same as sync-fetched ones.

Tooling to reuse:
- `scripts/web/netperf-suite.mjs:125` `trackNetwork(cdp)` + `:143` `inWindow(a,b)` +
  `:77` `instrument()` (`screenLog`) — the exact capture+bucketing the manifest
  generator needs.
- `scripts/web/lib/core.mjs` — shared Playwright/CDP harness.

CMake:
- `native/CMakeLists.txt:219`/`:642` — `WebAssets.cpp` is compiled into the web target
  (confirmed). Add `rb3_prefetch_web.cpp` to the web source list. `MILO_WEB_ASYNC`
  (JSPI, `:876`) is on, so async fetch + yields work.

## Risks & tradeoffs

- **Bandwidth contention.** Eager prefetch can compete with assets the *current* screen
  still needs, making the active transition worse. Mitigation: only pump while the
  current screen is quiescent (loader queue near-empty), bound in-flight count, and
  prefer the next screen only after the current one's manifest is resident.
- **Wrong prediction = wasted bytes.** If the user backs out, prefetched assets were
  downloaded for nothing. Cost is bounded (MEMFS dedups; the bytes are also written to
  IDB so a later visit still benefits) and the flow graph is small/accurate for the
  canonical path. Keep predictions conservative (1 hop ahead).
- **Manifest staleness.** If assets change and manifests aren't regenerated, prefetch
  fetches the wrong/old set; the sync fallback still works (correctness preserved, perf
  reverts to baseline for the drifted entries). Mitigation: the `assetVersion` token in
  each manifest + the `build.sh`/CI drift check.
- **MEMFS memory pressure.** Prefetching venue/palette/mogg into MEMFS holds tens of MB
  resident sooner. Same bytes the screen would load anyway, just earlier; bounded by the
  1-hop horizon. Watch heap if multiple large dynamic assets (mogg + venue) prefetch
  concurrently — the concurrency cap addresses this.
- **The 37 MB mogg is special.** Even fully async + prefetched, it's a lot to hold and
  the *first play* still needs it resident. R2 overlaps its download with song-select
  dwell time (start at *highlight*), which usually hides it — but the deeper fix is
  progressive streaming of the mogg (out of R2 scope; note for a dedicated audio-stream
  item). R2 makes it non-blocking and ahead-of-need, which already removes the 6.7 s
  dead frame.
- **Resident check race.** `stat` then async fetch — if the engine sync-fetches the
  same file between the check and our enqueue, we double-download. Harmless (MEMFS
  write idempotent, second write overwrites identical bytes); optionally track an
  in-flight set to skip.

## Verification

Primary: `scripts/web/netperf-suite.mjs --scenario nav` A/B, prefetch off vs on
(`?prefetch=1`), at `low` (50 Mbit) and `normal` (200 Mbit). Success = the
per-transition **blocked ms** and **maxGap** for `main_hub→song_select`,
`song_select→part_difficulty`, and especially `part_difficulty→game` drop sharply
(target: the on-demand sync XHRs for the prefetched milos disappear from the
transition window's waterfall — verify in `network-waterfall.json` that the
offenders' requests now land *during the previous screen*, not the transition). The
3.6 s / 11.4 s `part_difficulty→game` freeze should fall toward the loopback ~0.8 s.

Secondary:
- Manifest-generator self-check: regenerate manifests, confirm the captured set
  matches the netperf waterfall offenders (colorpalettes, venue milos, vignettes,
  mogg) and that the drift check is green.
- IDB write-through: confirm a prefetched asset is in IDB on the next (warm) run
  (existing `window.__rb3CacheStats`).
- Correctness: with prefetch on, the game still loads + plays the right song (reuse
  `scripts/web/web-song-preview-audio.mjs` / `audio-verify`); prefetch must not change
  *what* loads, only *when*.
- Negative control: `?prefetch=0` reproduces the baseline numbers exactly (proves the
  win is the prefetch, not measurement drift).

## Effort, impact & dependencies

- **Effort: L.** Two real components (capture tooling + runtime driver) plus dynamic
  song/venue resolution. The fetch primitive (`WebAssetsFetch`) and the
  capture/bucketing harness already exist, which caps the size; the manifest pipeline +
  driver state machine + dynamic resolution are the work.
- **Impact: high.** Directly attacks the dominant mid-session jank — the per-transition
  freezes (3.6 s / 11.4 s into a song; 6.2 s / 15.5 s into the hub). Converts N blocking
  XHRs per transition into overlapped background downloads that complete during idle.
- **Dependencies / interactions:**
  - **R1 (async on-demand miss path)** — *complementary, mostly independent.* R1 makes
    a *missed* read non-blocking (no freeze even on a prefetch miss/misprediction); R2
    makes the read *not miss at all*. R2 can land before R1 (it eliminates the misses
    on the predicted path), but the two together are the full fix: R1 covers the
    unpredicted long tail, R2 covers the hot path. R2's async pump and R1 likely share
    the same `WebAssetsFetch` infrastructure — coordinate so there's one async-fetch
    bookkeeping path, not two.
  - **R3 (wire compression for `/api/file`)** — *compounds.* Measured gzip ratios:
    `main_hub.milo_xbox` 0.29, `colorpalettes` 0.52, `small_club_01` 0.68. Prefetching
    **gzipped** milos shrinks both the freeze (R1/R2) and the bytes (R3); R2's prefetch
    pump benefits directly when R3 ships `.gz` siblings. The mogg is already-compressed
    Ogg (~0 gain), so R3 does nothing for the worst single file — which is exactly why
    R2 must handle the mogg via async-prefetch (and, longer term, streaming), not
    compression. Land order is independent; do both.
  - **W4b IDB cache (shipped)** — R2 write-through to IDB makes prefetched assets warm
    for the *next* session too. Reuse `cachePutAfterFetch` / `window.__rb3CachePut`.
- **Unblocks:** a credible "production-feeling" web demo (no multi-second tab freezes on
  normal connections) and a foundation (manifests) for any future progressive/streaming
  asset work.

## Open questions

1. **Trigger lead time.** Start prefetching the next screen on *entry* to the current
   screen, or on a dwell timer, or on the first input that implies advancing? Earliest
   safe point for the 37 MB mogg is song *highlight* on song_select — confirm that's
   reachable/stable enough to key on.
2. **Manifest granularity for `game_screen`.** How much of the venue/track set is truly
   static vs. song-dependent? Need to capture across a few songs/venues to separate the
   fixed core from the per-selection dynamic part (Phase 0 should sample >1 song).
3. **Concurrency + ordering policy.** Biggest-first (kill the worst freeze soonest) vs.
   manifest-order (match engine read order)? And the right in-flight cap given MEMFS
   memory + link contention — tune empirically in Phase 4.
4. **Hook choice.** Poll `window.rb3CurrentScreen`/`TheUI.CurrentScreen()->Name()`
   (simple, Phase 1) vs. a `UIScreenChangeMsg` subscription / `GotoScreenImpl`
   `#ifdef HX_WEB` notify (earlier incoming-screen signal). Start with polling; measure
   whether the lead time matters before adding the engine hook.
5. **Manifest delivery.** Static-deploy under `build/<build>/manifests/` (cacheable,
   no server change) vs. a `/api/manifest/<screen>` endpoint (dynamic, dev-friendly).
   Static is simpler; confirm `build.sh` can copy them and the IDB/HTTP cache treats
   them correctly.
6. **Back-edge / cancellation.** If the user backs out mid-prefetch, do we cancel
   in-flight fetches (emscripten_fetch close) or let them finish (bytes still useful,
   already in IDB)? Likely let finish; confirm no leak in the request bookkeeping.
