# 02 — Boot sync-read fast path (P1.1): kill the ~7 s App-ctor idle

## Problem

Web cold boot ≈ 13–20 s; `new App()` alone ≈ 12.45 s, of which ~7 s is IDLE (not CPU).
Native boots the same code in 5.2 s with sync I/O. Proven GPU-independent.
Canonical findings: `docs/native/web-loadperf-findings-2026-06-03.md` — which attributes
the idle to the **web async-file-I/O / JSPI suspend-per-read** model, NOT loader-yield
overhead (the loader yield was already throttled and measured neutral; see corrected root
cause below).

## Current code (rb3 repo)

- **File layer** — `native/src/native_file.cpp`:
  - `NativeStdioFile` ctor `:121-190` — `fopen`; on web+read-mode first-open failure
    (`:137-168`, `if (!mFp && readMode)`): `cacheTryHit()` (`:161`, IDB warm cache →
    `FS.writeFile` → reopen) else `WebAssetsFetchSync()` (`:163`, engine
    `src/platform/WebAssets.cpp:309-377`, **sync XHR** via EM_ASM → `FS.writeFile` →
    reopen) followed by `cachePutAfterFetch()` (`:166`, IDB write-back).
  - `Read` `:196-200` = `std::fread` (MEMFS = memcpy). `ReadAsync` `:201-204` calls
    `Read` synchronously and stores `mLastReadBytes` — already sync under the async API.
    **VERIFIED.**
  - This is the ONLY File backend on the web build: `NewFile()` (`src/system/os/File.cpp:158`)
    returns early at `:168` via `HmxNativeOpenFile` under `HX_NATIVE` (the web build defines
    BOTH `HX_NATIVE=1` and `HX_WEB=1` — `native/CMakeLists.txt:371,820`). The Wii
    ArkFile / AsyncFile / FileCache paths (`:200-208`) are dead code on web. So **all** boot
    reads go through `NativeStdioFile` — no async loader class is missed. **VERIFIED.**
- **Loader drains** — `src/system/utl/Loader.cpp` (HX_WEB arms):
  - `PollUntilEmpty` `:257-279` — called from `App` ctor (`src/App.cpp:460`). **VERIFIED.**
    It just sets `mPeriod = unk1c = 1e30f` (drain-to-empty sentinel) and calls
    `TheLoadMgr.Poll()` ONCE — so its yield cadence is entirely `Poll()`'s, below.
  - `Poll` web arm `:305-411` — budgeted (`RB3_LOADER_BUDGET_MS`=8 default `:342`,
    `RB3_LOADER_YIELD_MS`=16 default `:363`). **`emscripten_sleep(0)` at `:405` is
    ALREADY THROTTLED** by a `sinceYield` timer to fire only once every `sYieldMs`
    (default 16 ms) of accumulated work (`:403-407`), NOT per slice. The
    drain-to-empty mode (`mPeriod >= 1e29f`, set by `PollUntilEmpty`) empties the whole
    queue with that same throttled yield; the normal per-frame mode breaks after
    `sBudgetMs` and returns (the frame return is the composite yield, no manual sleep —
    `:395-400`).
  - `PollUntilLoaded` web arm `:163-223` — `kSliceMs=8` (`:185`) cooperative slicing;
    **`emscripten_sleep(0)` at `:219` fires once PER while-iteration (per ~8 ms slice),
    UN-throttled** — distinct from `Poll()`'s throttled yield. This is the only loader
    path that still yields per-slice. **VERIFIED.**
- **JSPI link**: `native/CMakeLists.txt:891-895` — `-sJSPI_EXPORTS=...`, gated on
  `MILO_WEB_ASYNC`. **VERIFIED.**
- **Boot bundle**: `/api/bundle/boot` (native/web/server.py:355) + `gen-boot-manifest.mjs`
  manifest. Fetched async at `BOOT_INIT` (`native/src/main_web.cpp:541`,
  `WebAssetsFetchBundle("/api/bundle/boot")`); `BOOT_FETCHING` gates on
  `WebAssetsAllDone()` (`:554`) before `BOOT_ENGINE_INIT` → … → `BOOT_APP_CTOR`
  (`new App` at `:633`). So boot-critical assets ARE in MEMFS before the App ctor.
  **VERIFIED.**

## Root cause — CORRECTED after verification; MEASURE FIRST

> **Verifier correction (2026-06-09).** The original draft claimed
> `emscripten_sleep(0)` fires "per loader slice/iteration" and proposed throttling it
> as "Fix B (the likely 7 s)". **That throttle ALREADY EXISTS for the dominant boot
> path.** In `Poll()` (which is what `PollUntilEmpty` — the App-ctor drain — calls), the
> yield at `Loader.cpp:405` is gated by a `sinceYield` timer to fire only every
> `RB3_LOADER_YIELD_MS` (default 16 ms) of accumulated work, not per slice
> (`:403-407`). An in-code NOTE there (`:356-362`) records that this throttle was
> **measured NEUTRAL for total boot time** — see
> `docs/native/web-loadperf-findings-2026-06-03.md`: the App-ctor ~7 s is the web
> async-file-I/O / **JSPI suspend-per-read** model, and is GPU-independent (real RTX
> 3090 == SwiftShader == 12.3 s App ctor). The yield-cadence refactor was already a
> "clarity + spin-overhead" change, not a boot-time fix. **Fix B as originally framed
> is largely redundant for the App-ctor drain; do not expect ~7 s from it.**

Where the 7 s of idle most plausibly comes from, given the above:

- **(a) JSPI suspend-per-read overhead** *(most likely)*: under JSPI/ASYNCIFY each
  point at which the wasm stack can suspend (and each `emscripten_sleep(0)` that does
  yield) costs a full event-loop round trip (~4–16 ms). Even one yield per 16 ms across
  a ~12 s App ctor is ~750 yields ≈ several seconds of pure event-loop latency that
  shows as IDLE in the CPU profile. The async file model (sync-XHR-into-MEMFS, then
  resume) inherently spends wall-time in the browser, not in wasm CPU.
- **(b) Redundant blocking fetches per open**: files NOT in the eager boot bundle (or
  opened before any write-through) take the sync-XHR path per open (`native_file.cpp:163`).
  Each sync XHR blocks the wasm thread for a network round trip even on localhost. The
  boot bundle covers the boot-critical `.milo_xbox` set, so this should be a small
  residual — Step 0 quantifies it.
- **(c) `PollUntilLoaded` un-throttled per-slice yield** (`Loader.cpp:219`): the one
  loader path that still yields once per ~8 ms slice. If the App-ctor load chain routes
  through `PollUntilLoaded` (not just `PollUntilEmpty`/`Poll`), this path's yields are
  un-throttled and a legitimate Fix-B target. Step 0's per-path counts decide whether it
  fires during boot at all.

**Step 0 (mandatory):** instrument and count, one boot:
- In `native_file.cpp`: count opens that (i) succeed first-try, (ii) hit `cacheTryHit`,
  (iii) hit `WebAssetsFetchSync`. Log totals at appctor_done.
- In `Loader.cpp` web arms: count `emscripten_sleep(0)` calls **separately for the
  `Poll()` path (`:405`, throttled) vs the `PollUntilLoaded` path (`:219`, per-slice)**
  + cumulative wall time around each during App-ctor. Confirm which path the App-ctor
  load chain actually drives.
Decide from the numbers. The corrected expectation: (a) JSPI/async-I/O dominant; (b) a
small residual fetch tax; (c) only material if `PollUntilLoaded` is on the boot path.

## Fix spec

**Fix A (cheap, do regardless)** — residency check before the blocking fetch, in the
`if (!mFp && readMode)` block (`native_file.cpp:137-168`): before
`cacheTryHit`/`WebAssetsFetchSync`, EM_ASM `FS.analyzePath(path).exists` (try/catch → 0);
if resident, just `fopen` and skip both. Zero risk: falls through to the existing paths
when not resident.
- **Warm-cache check (verifier note):** Fix A does **NOT** regress warm-boot IDB
  caching. The boot bundle is the warm-cache source for bundle-resident files: its
  `onBundleSuccess` (engine `WebAssets.cpp:156-192`) already writes every bundled file
  through to IDB via `window.__rb3CachePut`, using the SAME cache key the sync path
  derives. The per-file `cachePutAfterFetch` (`native_file.cpp:166`) only runs on the
  `WebAssetsFetchSync` miss path. For a bundle-resident file the first `fopen` at `:127`
  already succeeds, so the `if (!mFp && readMode)` block never fires for it today —
  Fix A only short-circuits the (already non-firing) block more cheaply. The bundle's
  own IDB write-through is independent of this block and unaffected. So warm boots still
  serve bundle files from IDB; the bundle re-arrives only on a cold (empty-IDB) boot,
  which is the intended behaviour (`main_web.cpp:531-538`).

**Fix B (REDUCED priority — see corrected root cause)** — the `Poll()`/`PollUntilEmpty`
path is ALREADY throttled to one yield per `RB3_LOADER_YIELD_MS` (16 ms), and that was
measured neutral for boot time. So Fix B is **only** worthwhile if Step 0 shows the
App-ctor load chain drives `PollUntilLoaded` (`Loader.cpp:219`), whose per-slice yield is
the one remaining un-throttled path. If so:
- Throttle `PollUntilLoaded`'s yield the same way `Poll()` does: add a `sinceYield`
  timer and only `emscripten_sleep(0)` every `RB3_LOADER_YIELD_MS` of accumulated work
  (mirror `Loader.cpp:403-407` into the `:188-220` web loop), OR skip the yield when the
  just-completed slice performed only MEMFS-resident reads (plumb a "did any blocking
  fetch happen" flag from `native_file.cpp` / `WebAssets.cpp`).
- Keep ≥1 yield per ~16 ms wall so the browser still paints the loading overlay
  (web-polish memory: loader responsiveness was a real shipped issue — verify with
  `scripts/web/loadperf-responsiveness.mjs`).
- Env-gate the new behavior so A/B is a URL/env flip (`?loaderYieldMs=` → `RB3_LOADER_YIELD_MS`
  plumbing already exists — `native/src/main_web.cpp:167-172`).
- **Do NOT expect ~7 s from this.** The dominant cost is JSPI async-I/O latency, not
  loader yield spam; if Step 0 confirms `Poll()`-path-only on boot, Fix B is a no-op and
  the real lever is reducing the number of blocking opens (Fix A + a fatter eager bundle)
  and/or the JSPI suspend count itself.

## Risks

- Sync completion is already the reality (`ReadAsync` is sync) — no new reentrancy.
- Under-yielding starves the browser compositor + any in-flight fetch progress: keep
  the ≥1-per-16 ms floor; check the boot overlay still animates.
- Don't touch the non-web arms; all edits inside existing `#ifdef HX_WEB` blocks
  (Loader.cpp is a matched TU — web arm is already ifdef'd; verify Wii bytes unchanged).

## Acceptance

- `node scripts/web/loadperf-profile.mjs` (cold + warm): `appctor_start→appctor_done`
  improves from ~12.45 s. **Realistic target depends on Step 0** — if the idle is
  JSPI-suspend-dominated (the corrected hypothesis), Fix A + a fatter eager bundle may
  only shave the fetch residual; the bigger win needs fewer blocking opens / fewer JSPI
  suspends, not loader-yield throttling. Aim for ≤ ~6 s only if Step 0 shows a large
  `WebAssetsFetchSync` count or a `PollUntilLoaded`-path per-slice yield count;
  otherwise set the target from the measured breakdown.
- `scripts/web/loadperf-responsiveness.mjs`: overlay animation still passes.
- `scripts/web/smoke-test.mjs` green; one full nav to gameplay (lib/core.mjs harness).
- Record before/after Step 0 counts (per-path yield counts + fetch/cache-hit/first-try
  open counts) in the commit message.

## IMPLEMENTED (2026-06-09)

**Status: DONE.** The ~7 s App-ctor idle was actually ~11 s, and Step-0 measurement
pinned the dominant cause to root-cause option **(c)**: `PollUntilLoaded`'s
un-throttled per-slice `emscripten_sleep(0)` (`Loader.cpp:219`). The App-ctor load
chain drives `PollUntilLoaded` almost exclusively (≈1198 calls), NOT
`Poll()`/`PollUntilEmpty` (whose yield counter stayed 0). Throttling that path's
yield to one per 16 ms (Fix B) cut the App-ctor by ~10 s.

### Step-0 measurement (debug build, loaded box, RB3_BOOT_IO_STATS=1)

Harness: `scripts/web/_bootio-measure.mjs --port 8432` (captures the `[boot-io-stats]`
stderr line + the `appctor_start→appctor_done` delta from `window.rb3BootPhaseLog`).
A/B driven by URL params plumbed through `ApplyUrlLoaderEnv` (no rebuild):
`?bootIoStats=1`, `?bootNoResidencySkip=1`, `?loaderMinYieldMs=N`.

| Config | appctor_start→done | pullSliceYields (PollUntilLoaded) | poll-path yields | fetchSync opens |
|---|---|---|---|---|
| Original (`loaderMinYieldMs=0`) | **15.46–16.35 s** | 2568–2628 (~10.6–11.0 s) | 0 | 37 |
| Fix B on (default, `=16`) | **5.33–6.48 s** | 154–178 (~0.7–0.8 s) | 0 | 37 |

Open distribution (identical across configs): `reads=433 firstTry=392 residentSkip=0
cacheHit=0 fetchSync=37 stillFail=4`. So **Fix A residentSkip never fired** — every
fetchSync open is a genuine MEMFS miss (file not in the boot bundle), and resident
files already succeed on the first `fopen` (`firstTry=392`), so the
`if (!mFp && readMode)` block they'd short-circuit never runs for them. Fix A is a
zero-risk no-op-improvement on this boot, exactly as the verifier predicted; the
~37 sync XHRs are a small residual, not the bottleneck.

### loadperf-profile (release build, Fix B default-on)

`appctor_start→appctor_done = 5.36 s` (was ~12.45 s in the original handoff). Boot
reaches `intro_movie_screen`/`splash_screen` cleanly.

### Acceptance gates

- `loadperf-profile.mjs` (release, port 8432): appctor **12.45 s → 5.36 s**.
- `loadperf-responsiveness.mjs`: **PASS** (exit 0) — max RAF gap 983–1000 ms < 1500 ms
  threshold; the single ~1 s gap is the pre-loader wasm instantiate/compile blocking
  task, not the loader. Overlay disappears fast because boot now completes in ~6 s.
- `smoke-test.mjs`: **PASS** — `main_hub_screen` reached, song DB populated (83 songs),
  no pageerror.

### Changes

- `src/system/utl/Loader.cpp` (HX_WEB arm only — Wii bytes unchanged, utl/Loader fuzzy
  99.61162 % unchanged): Fix B yield throttle on `PollUntilLoaded` (`sinceYield` Timer
  mirroring `Poll()`'s gate), default `RB3_LOADER_MIN_YIELD_MS=16`, opt-out `=0`. Plus
  per-path yield counters (`gLoaderPollYields`/`gLoaderPullSliceYields` + wall-ms +
  `gLoaderPullCalls`).
- `native/src/native_file.cpp` (`__EMSCRIPTEN__` arm): Fix A residency short-circuit
  (`FS.analyzePath` before `cacheTryHit`/`WebAssetsFetchSync`, env-disable
  `RB3_BOOT_NO_RESIDENCY_SKIP`), open-outcome counters, and `RB3BootIoStatsDump()`
  dumped at appctor_done (gated on `RB3_BOOT_IO_STATS`).
- `native/src/main_web.cpp`: call `RB3BootIoStatsDump("appctor_done")` at appctor_done;
  plumb `?bootIoStats` / `?bootNoResidencySkip` / `?loaderMinYieldMs` URL params.
- `scripts/web/_bootio-measure.mjs`: the Step-0 measurement harness (kept as acceptance
  evidence).

**Deviation from the handoff:** the doc REDUCED Fix B's priority ("do NOT expect ~7 s
from this") on the assumption that the App-ctor drain runs `Poll()`/`PollUntilEmpty`
(already throttled). Step-0 data overturned that: the boot runs `PollUntilLoaded`, whose
per-slice yield was the un-throttled path, so Fix B WAS the ~10 s lever. Fix A is correct
and shipped but contributed 0 on this boot (no resident-but-missed opens). Instrumentation
left in, env-gated.
