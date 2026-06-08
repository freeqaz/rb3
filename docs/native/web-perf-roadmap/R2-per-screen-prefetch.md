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

- **Song/venue parameterization — and what is *not* prefetchable early.** The
  `game_screen` working set splits into three classes, not two:
  - **Static** (always the same for that screen): track milos, HUD, shared kit banks.
    Ships as JSON.
  - **Song-dynamic, known at *highlight*:** the chosen song's `.mogg` + chart, resolved
    from the highlighted selection (`song shortname → songs/<short>/<short>.mogg`). This
    is the *only* dynamic asset whose identity is known during song-select dwell, so it
    is the only one whose download overlaps the dwell window. The driver resolves it from
    engine state (`TheMusicLibrary->GetHighlightedNode()->GetToken()`) at highlight time.
  - **Venue-dynamic, NOT known until the performance is configured:** the venue milo is
    **performance-selected at random**, not derived from the song. `game_screen` entry
    runs `MetaPerformer::SelectRandomVenue()` (MetaPerformer.cpp:947 — a probability-/
    profile-driven random pick over `SystemConfig(video_venues)`, not a song→venue map).
    So the venue working set is **unknown during song-select dwell** and cannot be
    prefetched there. The earliest point the venue is known is *after* the song is
    confirmed and the performer commits a venue — i.e. the venue milo prefetch
    realistically overlaps the `part_difficulty → game` **transition** (the brief window
    between venue selection and the venue milo's first read), not the song-select dwell.
    Treat the venue as a third manifest class, resolved by reading
    `MetaPerformer::Current()->GetVenue()` (MetaPerformer.cpp:191) *after* venue
    selection has run, and prefetch it on the transition rather than the dwell. Do **not**
    claim the venue is overlappable with dwell time — only the mogg is.
- **Size annotation (raw, not compressed).** Record each entry's **raw (uncompressed)
  byte size** in the manifest so the driver can budget (fetch biggest-first, cap
  concurrent bytes in flight, and size the MEMFS-memory headroom) and the loading UI can
  show real progress. **Caveat once R5 (wire compression) ships:** the capture harness
  reads `encodedDataLength`, which is the *compressed* transfer size — wrong for
  MEMFS-memory budgeting, which needs the post-decode resident size. The generator must
  record the **raw** size (from the response `Content-Length` of the *uncompressed*
  asset, or by reading the on-disk asset size, not `encodedDataLength`). Optionally
  record `bytesWire` alongside `bytes` if the pump also wants to budget link time, but
  `bytes` must be raw.

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
    { "kind": "song_mogg", "when": "highlight" }    // resolved to songs/<short>/<short>.mogg at highlight time
  ],
  "lateDynamic": [
    { "kind": "venue_milo", "when": "transition" }  // venue is RANDOM (SelectRandomVenue); resolved AFTER venue selection, prefetched on the part_difficulty->game transition
  ]
}
```

`bytes` is the **raw** resident size (see Size annotation above), not the gzipped/brotli
wire size — required so the concurrency cap can budget MEMFS memory correctly under R5.

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
- **An idle-gated async fetch pump with its OWN fetch-id bookkeeping.** For each
  manifest entry not already **resident-or-warm** (see resident check below), call the
  engine's existing **async** `WebAssetsFetch(serverPath)` (WebAssets.cpp:118) — which
  downloads via `emscripten_fetch` and writes to `/data/<serverPath>`, **the exact path
  the sync read path anchors to** (native_file.cpp:155 prepends `/data/`). So a
  prefetched file is indistinguishable from one that was synchronously fetched: the later
  `fopen` inside `NativeStdioFile` simply succeeds with no network round-trip. The pump
  bounds concurrency (e.g. ≤2 in flight, biggest-first by the manifest's raw `bytes`) so
  prefetch never saturates the link the active screen might still need, and it only runs
  while the current screen is interactive/quiescent.

  **Completion tracking — do NOT key on the process-global gate.** `sPending` /
  `WebAssetsAllDone()` (WebAssets.cpp:331) and `WebAssetsPendingCount()` (:332) are the
  **boot gate**, shared with every other async fetch in the process — the pump must
  **not** read them to decide its own slot availability or "drained" state (a concurrent
  boot/bundle fetch would corrupt the count, and vice-versa). Instead the pump keeps its
  **own** set of in-flight fetch ids returned by `WebAssetsFetch`, polls each with
  `WebAssetsFetchDone(id)` (WebAssets.cpp:145), and frees a concurrency slot when *that
  id* reports done. The pump's "is this screen prefetched?" is `all of my enqueued ids
  are done`, never `WebAssetsAllDone()`.

  **Bookkeeping leak — fix BEFORE R2 scales it (R2 is its first real user).** Today
  `WebAssetsFetch` does `new FetchRequest()` (WebAssets.cpp:119), pushes it into the
  module-global `sFetchRequests` vector (:125), and **never frees it**;
  `WebAssetsFetchDone` linear-scans that ever-growing vector (:146). That is fine for the
  bundle's single fetch but **leaks unboundedly** at per-screen scale (hundreds of
  fetches per session) and makes every `WebAssetsFetchDone` O(n). Before wiring R2's pump,
  fix the engine bookkeeping: reap completed `FetchRequest`s (delete + erase from
  `sFetchRequests` once the caller has observed `done`, e.g. an explicit
  `WebAssetsFetchReap(id)` the pump calls after it reads a done id), or replace the vector
  with an id→request map and remove on reap. The pump owns the reap call for its own ids.
- **An IDB-aware resident/skip check.** Before enqueuing, the entry must be skipped if it
  is already in MEMFS **or** already in the W4b IDB warm cache — otherwise it re-downloads
  IDB-warm assets and defeats W4b. The check is two-stage, mirroring the read path
  (native_file.cpp): (1) `stat("/data/<rel>")` for MEMFS residency; (2) on a MEMFS miss,
  query the IDB warm map via the same sync `window.__rb3IdbCache.has(key)` lookup that
  `cacheTryHit` uses (native_file.cpp:66) — the key is the relative path
  (`StripDataPrefix`, native_file.cpp:57). If the IDB cache has it, the engine's own read
  path will fault it in from IDB with no network, so the pump skips it (no prefetch
  needed). Only entries missing from *both* MEMFS and IDB are enqueued for
  `WebAssetsFetch`.
- **A tick hook** called once per frame from `BOOT_RUNNING` in `mainLoop`
  (main_web.cpp:553), after `PublishCurrentScreen`: `RB3PrefetchPoll()`. It (1)
  detects screen change → enqueues the next screen's manifest (skipping MEMFS-resident
  and IDB-warm entries), (2) advances the async fetch pump using its **own** in-flight id
  set (start next when a slot frees, reap done ids), (3) on each successful async fetch,
  writes the bytes through to IDB so they persist for the next session.

Control flow per transition (the win):

```
on screen change to S:
   nextScreens = flow[S]
   for N in nextScreens:
       manifest = load(manifest(N))                       // tiny JSON, fetched via /api/file
       entries = manifest.static
                 + resolveSongDynamic(manifest, engineState)   // mogg, known at highlight
       // NOTE: manifest.lateDynamic (venue milo) is NOT resolved here — the venue is
       //       random (SelectRandomVenue) and unknown until venue selection runs.
       for entry in entries:
           if resident_in_MEMFS(/data/entry.path):  continue   // already warm
           if in_IDB_warm_cache(entry.path):         continue   // read path will fault from IDB, no net
           id = WebAssetsFetch(entry.path)            // background, non-blocking
           myInflight.add(id)                         // OWN id set, NOT sPending
   ... user dwells on S; pump drains myInflight in the background (≤K in flight) ...
   ... on each WebAssetsFetchDone(id): reap(id) (free slot + delete FetchRequest), IDB write-through ...
on user advances to N (part_difficulty -> game):
   SelectRandomVenue() runs -> venue now known
   resolveVenueDynamic(GetVenue()) -> enqueue venue milo (overlaps the transition, not the dwell)
   engine reads static/song entry.path -> fopen hits warm MEMFS -> no sync XHR, no freeze
```

### Server change & manifest delivery (ONE mechanism)

`_serve_bundle` (server.py:272) bundles `.dta/.dtb` only. R2 does **not** need a new
bundle format — it reuses per-file `/api/file/<rel>` (already async-capable, supports
range).

**Decision (pick one, no contradiction): manifests are static-deployed AND served
through the same `/api/file/<rel>` path the prefetch driver already uses.** Concretely:

- `build.sh` copies `native/web/manifests/<screen>.json` into the deploy tree at a
  server-relative path the existing asset route already serves — i.e. under the same root
  `_serve_asset_file` (server.py:322) resolves `/api/file/<rel>` against, at
  `manifests/<screen>.json`.
- The driver fetches a manifest with **`WebAssetsFetch("manifests/<screen>.json")`** —
  the *same* async primitive and the *same* `/api/file/<rel>` URL it uses for every other
  asset (WebAssets.cpp:129 builds `/api/file/%s`). The fetched JSON lands at
  `/data/manifests/<screen>.json` in MEMFS and is read back with a plain `fopen`.

This is internally consistent: there is **no** separate `/api/manifest/<screen>`
endpoint and **no** out-of-band manifest path. Manifests are just assets — static-deployed
into the asset tree, fetched over `/api/file/`, and therefore HTTP/IDB-cached exactly like
any other asset, with no new server route. (A `/api/manifest/<screen>` reader was
considered and **rejected** to avoid two delivery paths; if a dev-server convenience route
is ever wanted it must be an alias that resolves to the same file, not a second source of
truth.)

## Implementation plan (high-level, phased)

**Phase 0 — manifest capture tooling (no engine change).**
- Add `scripts/web/gen-screen-manifests.mjs`. It reuses `scripts/web/lib/core.mjs`
  (Playwright/CDP harness) and the `trackNetwork` + screen-window bucketing already in
  `netperf-suite.mjs` (`instrument()`/`screenLog`, `inWindow(a,b)`). Drive the
  canonical nav path, bucket `/api/file/<rel>` requests by screen-transition
  timestamps, diff out the already-resident set (boot bundle + earlier screens), and
  emit `manifests/<screen>.json` (in `native/web/manifests/`, see Phase 3 for deploy)
  with per-entry **raw** `bytes` + the current `/api/version` token.
- **Record raw size, not `encodedDataLength`.** Per-entry `bytes` must be the
  uncompressed resident size (for MEMFS-memory budgeting). Once R5 ships,
  `encodedDataLength` is the *compressed* wire size and is wrong here — read the raw size
  from the response `Content-Length` of the uncompressed asset, or stat the on-disk
  asset, not `encodedDataLength`.
- **Classify dynamic entries by *when they are knowable*, not just "variable":**
  - The song mogg/chart → `dynamic` with `"when":"highlight"` (known at song highlight).
  - The venue milo → `lateDynamic` with `"when":"transition"` — it is **random**
    (`SelectRandomVenue`), unknown until venue selection runs, so it must NOT be tagged
    as dwell-overlappable. Capture it (so the path/size is known) but mark that it
    resolves and prefetches only on the `part_difficulty → game` transition.
  - Because the venue is random across runs, the generator must capture across **several
    runs** to enumerate the candidate venue milos, or simply record that this entry's
    concrete path is resolved at runtime from `MetaPerformer::GetVenue()` and only its
    typical size is recorded for budgeting.
- Add an `npm`/make wrapper + a drift check (re-generate, `git diff --exit-code` on the
  manifests dir) so stale manifests are caught.

**Phase 1a — fix `WebAssetsFetch` bookkeeping (engine-side, prerequisite).**
- Before R2 issues hundreds of fetches, fix the leak in
  `milo-native-engine/src/platform/WebAssets.cpp`: `FetchRequest`s are `new`'d (:119),
  pushed to `sFetchRequests` (:125), and never freed; `WebAssetsFetchDone` linear-scans
  the growing vector (:146). Add a reap path (e.g. `WebAssetsFetchReap(int id)` that
  deletes the request and erases it from `sFetchRequests` once the caller has observed
  `done`), or convert `sFetchRequests` to an id→request map removed on reap. This is an
  engine change shared with R4 (the single async-prefetch driver) — do it once.
- Keep `sPending`/`WebAssetsAllDone()` as the **boot gate** only. Do not overload it for
  per-screen completion (the pump uses its own ids; see Phase 1b).

**Phase 1b — prefetch driver scaffold (engine-side, behind an env flag).**
- New TU `native/src/rb3_prefetch_web.cpp` exporting `RB3PrefetchInit()` and
  `RB3PrefetchPoll()` (declared `extern` in main_web.cpp, like the other web hooks).
  Add it to the web target source list in `native/CMakeLists.txt` (near the other
  `rb3_*_web.cpp`).
- Manifest loading: fetch `manifests/<screen>.json` via `WebAssetsFetch` over the same
  `/api/file/` path (it's tiny — a few KB), read back from `/data/manifests/<screen>.json`,
  parse with a minimal JSON reader (or reuse the DTA/JSON facility already linked; a
  hand-rolled parser is fine for this fixed shape).
- Screen-flow table (`static const` map of screen-name → next-screen-name[]).
- **IDB-aware resident check:** `stat("/data/<rel>")` for MEMFS; on a MEMFS miss, query
  the IDB warm map via the same sync `window.__rb3IdbCache.has(key)` lookup
  `cacheTryHit` uses (native_file.cpp:66), key = relative path (native_file.cpp:57). Skip
  the entry if it is in *either* — do not re-download IDB-warm assets.
- **Async pump with its OWN id set:** wrap `WebAssetsFetch` with a bounded in-flight
  count; track the returned ids in a pump-local set, poll each with
  `WebAssetsFetchDone(id)` (WebAssets.cpp:145), reap on done (`WebAssetsFetchReap`), free
  a slot. **Do NOT** consult `WebAssetsPendingCount()`/`WebAssetsAllDone()` (the boot
  gate) for pump state. Biggest-first by raw `bytes`.
- Gate everything behind `RB3_PREFETCH` env (URL param `?prefetch=1` via the existing
  `ApplyUrlLoaderEnv` mechanism, main_web.cpp:169) so it can be A/B'd off for
  regression measurement. Default off in Phase 1, default on once verified.

**Phase 2 — wire the trigger + dynamic resolution.**
- Call `RB3PrefetchInit()` once at `BOOT_APP_CTOR` (after `new App`) and
  `RB3PrefetchPoll()` each frame in `BOOT_RUNNING` (main_web.cpp:553), right after the
  `PublishCurrentScreen()` block (which already reads `TheUI.CurrentScreen()`).
- Implement `resolveSongDynamic` (mogg, **known at highlight**): read the
  highlighted/selected song (`TheMusicLibrary->GetHighlightedNode()->GetToken()` —
  already used by `PublishHighlightedSong`, main_web.cpp:293) → `songs/<short>/<short>.mogg`.
  Prefetch this as soon as the song is *highlighted* on `song_select`/`part_difficulty`
  (earliest point the selection is known), maximizing dwell-time overlap for the 37 MB
  mogg. **This is the only asset whose download overlaps the dwell.**
- Implement `resolveVenueDynamic` (venue milo, **late**): the venue is **random**
  (`MetaPerformer::SelectRandomVenue()`, MetaPerformer.cpp:947), so it is *not* resolvable
  at highlight. Resolve it only **after** venue selection has run, by reading
  `MetaPerformer::Current()->GetVenue()` (MetaPerformer.cpp:191) and mapping the venue
  symbol → `world/venue/.../*.milo_xbox`. Enqueue it on the `part_difficulty → game`
  transition (where it overlaps the transition's other reads), **not** during song-select
  dwell. Do not depend on the venue being known any earlier.

**Phase 3 — manifest static-deploy (single delivery path).**
- Have `scripts/web/build.sh` copy `native/web/manifests/` into the deploy dir at the
  asset-relative `manifests/` path that `_serve_asset_file` (server.py:322) already
  resolves under `/api/file/<rel>` — i.e. served at `/api/file/manifests/<screen>.json`.
  This is the **same and only** delivery path the driver fetches over (Phase 1b). No new
  endpoint, no `/api/manifest/<screen>` route — manifests are ordinary assets, so they are
  HTTP/IDB-cached like any other asset with zero server change.

**Phase 4 — default on + tune.**
- Flip `RB3_PREFETCH` default on, tune in-flight concurrency + the trigger lead (how
  early to start, e.g. prefetch `game` assets the moment a song is highlighted vs. on
  `part_difficulty` entry).

## Key files & call sites (verified)

Engine / fetch primitives:
- `milo-native-engine/src/platform/WebAssets.cpp:118` `WebAssetsFetch(const char*)` —
  **the async prefetch engine.** Builds `/api/file/%s` (`:129`), queues
  `emscripten_fetch` → `onFetchSuccess` (`:60`) writes bytes to `/data/<serverPath>`.
  **Currently has zero callers** (only `WebAssetsFetchSync` + the bundle path are wired)
  — R2 is its first real user. **It `new`s a `FetchRequest` (`:119`), pushes to the
  module-global `sFetchRequests` (`:125`), and NEVER frees it — fix this leak before R2
  scales it (Phase 1a).**
- `WebAssets.cpp:145` `WebAssetsFetchDone(int id)` — **linear-scans `sFetchRequests`
  (`:146`)**, O(n) and unbounded as fetches accumulate. The pump polls this per-id but
  must reap done ids (Phase 1a) to keep it bounded.
- `WebAssets.cpp:331` `WebAssetsAllDone()` (`sPending==0`), `:332`
  `WebAssetsPendingCount()` — the process-global **boot gate**. The pump must **NOT**
  key its own slot/completion logic on these (shared with boot/bundle fetches); it tracks
  its own in-flight id set instead.
- `WebAssets.cpp:261` `WebAssetsFetchSync(const char*)` — the *blocking* path R2
  bypasses by warming MEMFS first. (Verifies bytes land at the same `/data/...` path.)
- Note: `onFetchSuccess` (`:60`) does **not** resolve `..` path components (only the
  bundle's `onBundleSuccess`, `:195`, does). Manifest/mogg paths have no `..`; if any
  prefetched milo path does, the shared driver must normalize first (see ROADMAP
  dependency notes).
- `WebAssets.h:16-37` — async/sync/bundle API surface (extend with a reap entry point
  in Phase 1a).

RB3 read path (proves warm-MEMFS interception works + the IDB resident-check mechanism):
- `native/src/native_file.cpp:128-168` — the `__EMSCRIPTEN__` on-demand block inside
  `NativeStdioFile::NativeStdioFile`. Order is: `fopen` → on miss, IDB `cacheTryHit`
  → else `WebAssetsFetchSync`. A prefetched file makes the **first `fopen` succeed**,
  short-circuiting all of it. `:155` prepends `/data/` to relative paths — the same
  anchor `WebAssetsFetch` uses, so prefetch + read agree on one absolute path.
- `native/src/native_file.cpp:66` `cacheTryHit` — the sync `window.__rb3IdbCache.get(key)`
  lookup; `:57` `StripDataPrefix` derives the relative key. **R2's resident check reuses
  this exact IDB query** (`.has(key)`) so it skips IDB-warm assets and does not defeat W4b.

Boot loop / trigger points:
- `native/src/main_web.cpp:230` `PublishCurrentScreen()` — already reads
  `TheUI.CurrentScreen()->Name()`; the screen-change signal R2 hooks.
- `main_web.cpp:293` `PublishHighlightedSong()` — already resolves the selected song
  token via `TheMusicLibrary->GetHighlightedNode()->GetToken()`; reuse for the **song**
  dynamic mogg resolution (known at highlight).

Venue resolution (late, runtime-random — NOT a song→venue map):
- `src/band3/meta_band/MetaPerformer.cpp:947` `MetaPerformer::SelectRandomVenue()` —
  probability-/profile-driven **random** venue pick over `SystemConfig(video_venues)`.
  Proves the venue is performance-selected, **not** derivable from the song at
  song-select. So the venue milo is not prefetchable during dwell.
- `MetaPerformer.cpp:191` `MetaPerformer::GetVenue()` (`Symbol`) — read the *committed*
  venue here, *after* selection, to resolve the venue milo path on the
  `part_difficulty → game` transition.
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

Server / manifest deploy (single delivery path):
- `native/web/server.py:272` `_serve_bundle` (`BUNDLE_EXTS={'.dta','.dtb'}`) — unchanged.
- `server.py:322` `_serve_asset_file` — already async-capable + range-capable; serves
  the prefetched milos/moggs **and the manifests**. Manifests are static-deployed into
  the asset tree at `manifests/<screen>.json` and served over the existing
  `/api/file/<rel>` route — **no new endpoint, no `/api/manifest/<screen>`** (one
  delivery path; see Server change section).
- `native/web/rb3_pre.js` (`window.__rb3IdbCache` :139, `window.__rb3CachePut` :225,
  `__rb3IdbReady` :140) — the W4b IDB warm cache. R2's resident check **reads**
  `__rb3IdbCache` (skip IDB-warm assets), and after a successful async prefetch the pump
  **writes through** via `window.__rb3CachePut(path, bytes)` so prefetched assets persist
  for the next session, same as sync-fetched ones.

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
- **MEMFS memory pressure (budget on RAW size).** Prefetching venue/palette/mogg into
  MEMFS holds tens of MB resident sooner. Same bytes the screen would load anyway, just
  earlier; bounded by the 1-hop horizon. Watch heap if the mogg (dwell prefetch) and the
  venue milo (transition prefetch) end up resident at once near the transition — the
  concurrency cap addresses this. The cap must budget on the manifest's **raw** `bytes`,
  not the R5-compressed wire size, or it under-counts resident MEMFS and over-commits.
- **The 37 MB mogg is the only dwell-overlappable big asset.** Even fully async +
  prefetched, it's a lot to hold and the *first play* still needs it resident. R2
  overlaps its download with song-select dwell (start at *highlight*), which usually
  hides it — the deeper fix is progressive streaming of the mogg (R4 Layer B / a
  dedicated audio-stream item, out of R2 scope). **The venue milo does NOT overlap the
  dwell** (it's random-selected at performance config, unknown until the transition), so
  the mogg is the sole large asset R2 can hide behind song-select idle; the venue's
  ~3.5 s freeze is only reduced to the extent the transition window itself offers idle.
  R2 makes the mogg non-blocking and ahead-of-need, which already removes its 6.7 s dead
  frame.
- **Resident check race + double-fetch.** The resident check is two-stage (MEMFS `stat`
  then IDB `__rb3IdbCache.has`); if the engine sync-fetches the same file between the
  check and our enqueue, we could double-download. The pump's **own in-flight id set**
  (Phase 1b) is the primary guard — it never re-enqueues a path already in flight; and
  the underlying MEMFS write is idempotent (second write overwrites identical bytes), so
  even a lost race is harmless. This own-id set is *also* why the pump must not consult
  the global `sPending`/`WebAssetsAllDone` (those count unrelated fetches).

## Verification

Primary: `scripts/web/netperf-suite.mjs --scenario nav` A/B, prefetch off vs on
(`?prefetch=1`), at `low` (50 Mbit) and `normal` (200 Mbit). Success = the
per-transition **blocked ms** and **maxGap** for `main_hub→song_select`,
`song_select→part_difficulty`, and especially `part_difficulty→game` drop sharply.
Verify in `network-waterfall.json`:
- **Static + song-mogg** offenders (colorpalettes, vignettes, the highlighted song's
  mogg) now land *during the previous screen's dwell*, not the transition — these are
  the dwell-overlappable set.
- **Venue milo** offenders land *during the `part_difficulty→game` transition window*
  (right after `SelectRandomVenue` runs), **not** the previous screen — the venue is
  random-selected, so it is correctly resolved late. Do **not** expect the venue request
  during song-select dwell; that would be a misclassified manifest entry.
The 3.6 s / 11.4 s `part_difficulty→game` freeze should fall toward the loopback ~0.8 s
for the static/mogg portion; the venue's residual is bounded by how much idle the
transition itself offers.

Secondary:
- Manifest-generator self-check: regenerate manifests, confirm the captured set
  matches the netperf waterfall offenders (colorpalettes, vignettes, mogg as static/
  song-dynamic; venue milos as `lateDynamic`) and that the drift check is green. Confirm
  `bytes` are **raw** sizes (sanity-check one entry's `bytes` against the on-disk asset,
  not the gzipped transfer size — matters once R5 ships).
- IDB write-through: confirm a prefetched asset is in IDB on the next (warm) run
  (existing `window.__rb3CacheStats`). And confirm the resident check **skips** an
  IDB-warm asset on the warm run (no second download in the waterfall) — proves the IDB
  consult, not just `stat`.
- Bookkeeping/leak check: after a full nav (hundreds of prefetches), `sFetchRequests`
  must not grow unbounded (the pump reaps its ids); assert the pump never blocks on
  `WebAssetsAllDone()` while an unrelated boot/bundle fetch is pending (own-id set, not
  global gate).
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
- **Dependencies (R2 depends on R1 + R3; shares ONE driver with R4):**
  - **R1 (async on-demand miss path) — dependency.** R1 provides the async primitive
    and (per the ROADMAP re-scope) the re-entrant pending-aware open the engine needs;
    R2 is built on top of it. R1 makes a *missed* read non-blocking (no freeze on a
    misprediction/back-edge); R2 makes the predicted read *not miss at all*. Together
    they are the full fix: R1 covers the unpredicted long tail, R2 the hot path. **Single-
    threaded build caveat (from ROADMAP):** while a *sync* XHR blocks the main thread the
    browser can't dispatch in-flight `emscripten_fetch` callbacks, so until R1 removes the
    sync path, an R2 prefetch can still be stalled behind a sync miss — another reason R2
    lands after R1.
  - **R3 (boot-bundle expansion) — dependency / pattern source.** R3 is "screen 0" of R2:
    it establishes the per-set manifest + async-bundle plumbing and the IDB write-back
    convention that R2 generalizes to every subsequent screen. Build R3's manifest
    generator + bundle plumbing to be reusable by R2; R3 leads (lower risk, independent
    of R1, proves the pattern).
  - **R4 (streaming song audio) — SHARED async-prefetch driver.** R4 Layer A (song-mogg
    prefetch) and R2 (per-screen milo pump) are the **same mechanism**: enqueue a
    server-relative path → `WebAssetsFetch` → reap via `WebAssetsFetchDone(id)` (own id
    set) → IDB write-through. **Build it once** as the generic
    `RB3PrefetchAsset`/`RB3PrefetchPoll` driver (including the Phase 1a bookkeeping fix and
    the `..`-normalization caveat from the ROADMAP); R4's mogg hook and R2's manifest pump
    are thin callers. Do **not** write two async-fetch bookkeeping paths.
  - **R5 (wire compression for `/api/file`)** — *compounds.* Measured gzip ratios:
    `main_hub.milo_xbox` 0.29, `colorpalettes` 0.52, `small_club_01` 0.68. Prefetching
    **gzipped** milos shrinks both the freeze and the bytes; R2's prefetch pump benefits
    directly when R5 ships compressed siblings. The mogg is already-compressed Ogg (~0
    gain), so R5 does nothing for the worst single file — which is exactly why R2/R4 must
    handle the mogg via async-prefetch (and, longer term, streaming), not compression.
    **R5↔R2 bytes caveat:** once R5 ships, the capture harness's `encodedDataLength` is
    the *compressed* size — wrong for MEMFS budgeting. The manifest must record **raw**
    size (see Size annotation / Phase 0). Land order independent; do both.
  - **W4b IDB cache (shipped)** — R2 **reads** `__rb3IdbCache` in the resident check (skip
    IDB-warm assets, don't defeat W4b) and **writes through** via `window.__rb3CachePut`
    after each prefetch so assets are warm for the *next* session too.
- **Unblocks:** a credible "production-feeling" web demo (no multi-second tab freezes on
  normal connections) and a foundation (manifests) for any future progressive/streaming
  asset work.

## Open questions

1. **Trigger lead time (for the mogg only).** Start prefetching the next screen on
   *entry*, on a dwell timer, or on the first advancing input? The mogg is the only
   dwell-overlappable big asset; its earliest safe point is song *highlight* on
   song_select — confirm that's reachable/stable enough to key on. (The venue is
   *resolved late by definition*, so its lead time is fixed at the transition, not an
   open question.)
2. **Manifest granularity for `game_screen`.** How much of the track set is truly static
   vs. song-dependent? Capture across a few songs to separate the fixed core from the
   per-selection mogg/chart. The **venue** is random per run (`SelectRandomVenue`), so
   sample multiple runs to enumerate the candidate venue milos for sizing — but classify
   the venue as `lateDynamic` regardless (resolved at runtime from `GetVenue()`).
3. **Concurrency + ordering policy.** Biggest-first (kill the worst freeze soonest) vs.
   manifest-order (match engine read order)? And the right in-flight cap given **raw**
   MEMFS memory + link contention — tune empirically in Phase 4.
4. **Hook choice.** Poll `window.rb3CurrentScreen`/`TheUI.CurrentScreen()->Name()`
   (simple, Phase 1) vs. a `UIScreenChangeMsg` subscription / `GotoScreenImpl`
   `#ifdef HX_WEB` notify (earlier incoming-screen signal). Start with polling; measure
   whether the lead time matters before adding the engine hook.
5. *(Resolved — manifest delivery.)* Manifests are static-deployed into the asset tree
   and served over the existing `/api/file/<rel>` route; **no** `/api/manifest/<screen>`
   endpoint. One delivery path (see Server change). No longer open.
6. **Back-edge / cancellation.** If the user backs out mid-prefetch, do we cancel
   in-flight fetches (`emscripten_fetch_close`) or let them finish (bytes still useful,
   already in IDB)? Likely let finish; **the pump must still reap the ids** (Phase 1a)
   so a back-out doesn't leak `FetchRequest`s.

## Review corrections applied

Folded in the verified adversarial-review must-fixes (2026-06-08):

1. **Venue is performance-selected, not song-derived.** `MetaPerformer::SelectRandomVenue()`
   (MetaPerformer.cpp:947) random-picks the venue, so the venue milo is **not** prefetchable
   during song-select dwell — only the mogg is. Reclassified the venue as a `lateDynamic`
   manifest entry resolved on the `part_difficulty→game` transition via
   `GetVenue()` (MetaPerformer.cpp:191); corrected Architecture, control-flow, Phase 2,
   Risks, Verification, and the impact claim. The mogg is now the *only* dwell-overlappable
   large asset.
2. **One manifest-delivery mechanism.** Removed the self-contradiction; manifests are
   static-deployed into the asset tree and fetched over the existing `/api/file/<rel>`
   route (no `/api/manifest/<screen>` endpoint). Updated Server change, Phase 1b/Phase 3,
   Key files, and open question 5 (resolved).
3. **IDB-aware resident check.** The skip check now consults the W4b IDB warm cache
   (`window.__rb3IdbCache`, via the `cacheTryHit` mechanism at native_file.cpp:66), not
   just `stat('/data/...')`, so it doesn't re-download IDB-warm assets and defeat W4b.
4. **Own fetch-id bookkeeping + leak fix.** The pump tracks its own in-flight ids (polled
   via `WebAssetsFetchDone`) and must **not** key completion on the global
   `sPending`/`WebAssetsAllDone()` boot gate. Added Phase 1a to fix the
   `WebAssetsFetch` leak (`FetchRequest` `new`'d at WebAssets.cpp:119, never freed;
   `WebAssetsFetchDone` linear-scans the growing `sFetchRequests` at :146) **before** R2
   scales it — R2 is its first real user.
5. **Raw size in the manifest (R5 interaction).** Manifest `bytes` is the **raw**
   resident size, not `encodedDataLength` (which becomes the compressed wire size once R5
   ships) — required for correct MEMFS-memory budgeting. Corrected Architecture, Phase 0,
   Risks, Verification, and the R5 dependency note.
6. **Dependencies.** Marked R2 as depending on **R1** (async primitive / re-entrant open)
   and **R3** (manifest/bundle pattern + IDB write-back convention), and sharing **one**
   async-prefetch driver with **R4** (build the generic driver once; R2 + R4-mogg are thin
   callers).
