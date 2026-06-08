# R3 — Eager boot-bundle expansion: pre-pack the boot `.milo_xbox` working set into the async bundle

> Status: design / handoff. Author: investigation pass 2026-06-08.
> Verification tool: `scripts/web/netperf-suite.mjs` (CDP-throttled boot waterfall).
> Sibling items: depends on nothing (R1-independent); overlaps R2 (per-screen
> prefetch — boot is "screen 0") and R5 (wire compression — the boot set
> gzips ~46%, so the two compose).

## Problem & data

Cold boot is dominated by **synchronous, throughput-bound on-demand fetches**. The
async `/api/bundle` (7.6 MB of `.dta/.dtb`) downloads fine in `BOOT_FETCHING`, but
then the `App` ctor walks the UI/track/char milo dependency graph and every miss
goes through `WebAssetsFetchSync()` — a blocking `xhr.open(..., false)` that freezes
the wasm main thread for the whole transfer.

Measured boot working set (CDP waterfall, `/tmp/rb3-web/netperf-matrix/low-boot-run1/network-waterfall.json`,
50 Mbit/s cold boot to first interactive screen):

| class | count | bytes |
|---|---|---|
| `/api/bundle` (async, already done) | 1 | 7.6 MB |
| **`.milo_xbox` (sync XHR misses)** | **94** | **54.0 MB** |
| `.dta` on-demand (sync) | 2 | ~0 MB |
| `.webm` intro cinematic | 1 | 3.8 MB |

Those **94 sync milo fetches = 54 MB** are what R3 attacks. Each blocks the main
thread for `bytes ÷ throughput`; the top offenders in *this* phase are
`ui/track/gen/track_shared.milo_xbox` (10.2 MB → 1.9 s), `trackpanel` (5.7 MB),
`overshell_player_common` (4.6 MB), plus a long tail of UI/font/`char/*_resource`
milos. Aggregate boot main-thread-blocked time scales straight with the link:
**0.9 s (local) → 2.9 s (200 Mbit) → 10.0 s (50 Mbit)** for the same ~71 MB / 98
sync XHRs (`web-netperf-findings-2026-06-08.md` boot table).

**Scope boundary (important — the findings doc conflates two phases).** The giant
files the findings doc lists under "boot → main_hub" — `colorpalettes` (20 MB),
`small_club_01` (18 MB), the `sv*_a`/`tv*_a` vignettes (5–12 MB each), and the
37 MB song mogg — do **not** appear in the cold-boot-to-first-screen waterfall.
They are fetched in the **first menu transition** and are **R2's** territory
(per-screen prefetch). R3 is strictly the 94-file / 54 MB set the `App` ctor needs
*before the first interactive frame*. Keeping that boundary is what makes R3 a
focused, independently-shippable win.

The bundle delivery is the proven-good path: it is one async `emscripten_fetch`
(`EMSCRIPTEN_FETCH_LOAD_TO_MEMORY`) that never blocks the main thread, and
`onBundleSuccess` already unpacks `count · {pathLen, path, dataLen, data}` records
into MEMFS. R3 reuses that machinery verbatim and just feeds it the boot milo set.

## Architecture

A **second, curated "boot-assets" bundle** fetched async in `BOOT_FETCHING`,
*before* `new App()`, so every sync milo read the `App` ctor issues lands in warm
MEMFS and never touches the network.

Data/control flow:

1. **Build time** (`build.sh` / asset prep): write a committed **boot manifest**
   — a newline-delimited list of server-relative paths (the 94 milos, derived
   deterministically from the netperf boot waterfall; see Implementation §1).
   It lives next to the assets / in `native/web/` so the server can read it.
2. **Server**: a new endpoint `GET /api/bundle/boot` (or `/api/bundle?set=boot`)
   reads the manifest and emits the **same binary bundle format** `onBundleSuccess`
   already parses — `uint32 count`, then per file `uint32 pathLen, path, uint32
   dataLen, data` (little-endian). It is the existing `_serve_bundle` generalized
   from "walk tree, filter by `BUNDLE_EXTS`" to "iterate an explicit path list".
   Overlay resolution (`_overlay_path`) and the `(..)` → `..` restore stay.
3. **Client boot machine** (`main_web.cpp`): in `BOOT_INIT`, fire **both** bundles
   — the existing `.dta/.dtb` config bundle *and* the new boot-milo bundle — via a
   parameterized `WebAssetsFetchBundle(url)`. `BOOT_FETCHING` already polls
   `WebAssetsAllDone()` (a shared `sPending` counter), so two concurrent async
   bundles just bump `sPending` to 2 and the existing gate waits for both. No new
   state needed; `onBundleSuccess` runs once per bundle and unpacks into the same
   `/data/...` tree.
4. **App ctor**: its `NativeStdioFile` opens now hit MEMFS (the bytes are resident),
   so the `WebAssetsFetchSync` fallback in `native_file.cpp` is **never reached for
   boot-set files** — the freeze is gone. The sync path stays as the correctness
   backstop for anything not in the manifest (and for misses past boot).

Why this shape:
- **Reuses the entire async + unpack path** (`emscripten_fetch` → `onBundleSuccess`
  → MEMFS) that is already load-bearing for the config bundle. Zero new transport.
- **Overlaps the boot CPU work**: the bundle downloads while WASM compiles / the GPU
  device comes up (`BOOT_GPU_WAIT` is several RAF ticks of pure async wait), so on a
  fast link the 25 MB-compressed boot bundle is largely free.
- **Manifest, not extension**: the full extracted set is **4455 `.milo_xbox` / ~4 GB**
  — gating the bundle by extension is impossible. The boot set must be an explicit
  curated list (54 MB), which is exactly what a manifest gives.
- **R5 composes for free**: the boot milos gzip to ~46% (`colorpalettes` is in R2,
  but the boot UI/track milos are texture/mesh data that compresses), so serving the
  boot bundle with `Content-Encoding: gzip` (or pre-gzipping it) roughly halves the
  ~54 MB → ~25 MB on the wire. Venue milos (R2) gzip to ~98% — useless — which is
  another reason R3 (boot) and R2 (venues) are separate.

## Implementation plan (high-level, phased)

**Phase 0 — derive the boot manifest deterministically.**
- Add a small generator (`scripts/web/gen-boot-manifest.mjs` or a `netperf-suite`
  `--emit-manifest` flag) that reads a cold-boot `network-waterfall.json`, filters
  to `/api/file/*.milo_xbox` requests captured *before the `first-screen`
  milestone*, strips `/api/file/`, and writes the sorted unique path list to
  `native/web/boot-assets.manifest`. This is a build/CI artifact, regenerated when
  the boot set changes; it is **measured, not hand-authored**, so it can't drift
  silently. Cross-check count == 94 / 54 MB against the current waterfall.
- Commit the manifest (it is tiny text). Handle the one fallback-only entry
  (`patchcreator/og/gen/patch_warpmesh.milo_xbox` lives in `extracted-xbox-full`,
  not the curated `extracted/`) — the server already probes `FALLBACK_ASSETS_DIRS`,
  so the manifest entry resolves; just don't fail the bundle if a manifest path is
  missing (log + skip, matching `onBundleSuccess`'s skip-on-write-fail tolerance).

**Phase 1 — server: emit a boot bundle.**
- Refactor `_serve_bundle` in `native/web/server.py` into a helper
  `_emit_bundle(entries)` that takes an explicit `[(rel, bytes)]` list and writes
  the existing binary format. Keep `/api/bundle` calling it with the
  `BUNDLE_EXTS`-filtered tree walk (unchanged behavior).
- Add `_serve_boot_bundle()` (route `/api/bundle/boot`): read
  `native/web/boot-assets.manifest`, resolve each path with the existing
  overlay/fallback/`(..)` logic from `_serve_asset_file`, and call `_emit_bundle`.
- Optional: gzip the boot-bundle body and set `Content-Encoding: gzip` when the
  client `Accept-Encoding` allows it (this is the R5 hook; can land later — the
  format inside is unchanged, the browser inflates before `onBundleSuccess` sees it).

**Phase 2 — engine: parameterize the bundle fetch.**
- In `milo-native-engine/src/platform/WebAssets.{h,cpp}`, change
  `WebAssetsFetchBundle()` to `WebAssetsFetchBundle(const char *url = "/api/bundle")`
  (default keeps every existing caller, incl. DC3, working). The body is identical
  except the URL string. `onBundleSuccess`/`onBundleError`/`sPending` are unchanged
  and already handle N concurrent bundles via the shared counter.

**Phase 3 — client: fire the boot bundle in BOOT_INIT.**
- In `native/src/main_web.cpp` `BOOT_INIT`, after `WebAssetsFetchBundle()` (config),
  add `WebAssetsFetchBundle("/api/bundle/boot")`. Both are async; `BOOT_FETCHING`'s
  `WebAssetsAllDone()` gate already waits for both. Gate behind an opt-out env knob
  via `ApplyUrlLoaderEnv` (e.g. `?bootBundle=0` → `RB3_BOOT_BUNDLE_OFF`) so the
  netperf A/B can measure with/without, and the sync path stays reachable for the
  control run.
- Progress UX is already wired: `BOOT_FETCHING` publishes
  `window.rb3AssetsLoaded/Total` from `WebAssetsCompletedCount()` (the unpack loop
  bumps `sCompleted` per file), so the loading bar reflects the boot-bundle unpack
  with no index.html change.

**Phase 4 — verify & tune (see Verification).** Confirm the 94 sync XHRs collapse
to one async bundle, boot-blocked ms drops toward the `local` floor, and no
boot-set file still hits `WebAssetsFetchSync` (instrument `native_file.cpp`).

## Key files & call sites (verified)

- `milo-native-engine/src/platform/WebAssets.cpp`
  - `WebAssetsFetchBundle()` — L245-255: hardcodes `emscripten_fetch(&attr, "/api/bundle")`; **parameterize the URL here**.
  - `onBundleSuccess()` — L156-236: parses `uint32 count` (L172) then per-file `pathLen/path/dataLen/data` (L176-191), `..`-resolves (L197-214), mkdir + `fopen`/`fwrite` into MEMFS (L216-229), bumps `sCompleted` (L233). **Reused unchanged** for the boot bundle.
  - `WebAssetsFetchSync()` — L261-329: the blocking `xhr.open(...,false)` (L292) this item eliminates for boot-set files. Stays as the backstop.
  - `sPending`/`WebAssetsAllDone()` — L32, L331: shared counter; supports 2 concurrent bundles with no change.
- `native/web/server.py`
  - `_serve_bundle()` — L272-320: tree-walk filtered by `BUNDLE_EXTS = {".dta",".dtb"}` (L285); emits the binary format (L306-314). **Refactor to `_emit_bundle(entries)` + add `_serve_boot_bundle()`.**
  - `_handle_api()` — L170-186: route table; **add `/api/bundle/boot`**.
  - `_serve_asset_file()` — L322-397: the path-resolution logic (overlay L336, `(..)` system L340-343, fallback roots L349-359) to **lift into the boot-bundle path resolver**.
  - `_overlay_path()` — L259-270: overlay shadowing; keep for boot DTAs if any join the set.
- `native/src/main_web.cpp`
  - `BOOT_INIT` case — L437-447: `WebAssetsInit()` + `WebAssetsFetchBundle()`; **add the second `WebAssetsFetchBundle("/api/bundle/boot")` call here.**
  - `BOOT_FETCHING` case — L449-471: `WebAssetsAllDone()` gate + `rb3AssetsLoaded/Total` publish — **already handles N bundles**, no change.
  - `BOOT_APP_CTOR` case — L521-551: `new App(0,nullptr)` — the consumer whose sync milo reads now hit warm MEMFS.
  - `ApplyUrlLoaderEnv()` — L169-193: URL→env knob plumbing; **add `bootBundle`→`RB3_BOOT_BUNDLE_OFF`**.
- `native/src/native_file.cpp`
  - `NativeStdioFile` ctor fopen-miss path — L137-168: `cacheTryHit` → `WebAssetsFetchSync` → retry. **The freeze this item removes; add a one-shot log/counter to prove boot-set files no longer reach it.**
- `native/CMakeLists.txt` — L642 (`WebAssets.cpp` in the web TU list), L874 (`rb3_pre.js` pre-js). No source-list change needed unless the manifest generator is added as a target.
- `scripts/web/netperf-suite.mjs` — `trackNetwork()` L125-, waterfall emit L338 (`network-waterfall.json`). The verification + manifest-source tool.

## Risks & tradeoffs

- **Manifest staleness.** A hand-authored list rots as the UI/boot graph changes,
  silently re-introducing sync misses. Mitigation: **generate from the netperf
  waterfall** (Phase 0), regenerate in CI, and add a netperf assertion that
  *zero* boot-set files hit `WebAssetsFetchSync` (Phase 4). The generator makes the
  manifest a measured artifact, not a guess.
- **Over- vs under-inclusion.** Too few → residual sync freezes (partial win). Too
  many → larger boot bundle that delays first frame for files the first screen
  doesn't need. The waterfall-derived set is by construction exactly "what boot
  fetched", so it is neither — but venue/song milos must stay **out** (that's R2);
  a path-prefix denylist (`world/venue/`, `songs/`) in the generator is a cheap guard.
- **Bundle blocks first frame until fully resident.** The current sync path is
  pay-as-you-go; the bundle is all-or-nothing — the App ctor can't start until the
  boot bundle finishes unpacking. Net win because the bundle is *one overlapped
  async transfer during GPU bring-up* vs *94 serialized blocking XHRs*, but on a
  pathologically slow link the user stares at the loading bar a moment longer before
  the first paint. Acceptable (and honest — the bar is already shown). If it matters,
  a future split could prioritize the first-screen subset.
- **Memory.** 54 MB raw lands in MEMFS in one unpack pass (transient: bundle buffer
  + MEMFS copy coexist briefly, ~108 MB peak during `onBundleSuccess`). The sync
  path already resident-copies each file into MEMFS, so steady-state RAM is the
  same; only the transient doubles. Within wasm heap budget (boot already pulls
  71 MB), but worth watching on the unpack of the combined config+boot bundles.
- **Double-maintenance with DC3.** Parameterizing `WebAssetsFetchBundle` is
  engine-shared (touches DC3's build). The default-arg keeps DC3 source-compatible;
  DC3 opting into a boot bundle is independent future work.
- **Server CPU.** `/api/bundle/boot` re-reads + concatenates 54 MB per cold boot
  (no caching). Fine for the dev server; a production CDN would pre-bake the bundle
  to a static file. Note now; don't block R3 on it.

## Verification

- **Primary (netperf A/B).** Build, serve, then:
  ```bash
  python3 native/web/server.py &
  node scripts/web/netperf-suite.mjs --scenario boot --profiles low,normal,local --runs 3   # baseline
  # apply R3, rebuild
  node scripts/web/netperf-suite.mjs --scenario boot --profiles low,normal,local --runs 3   # after
  ```
  Win criteria, from `summary.json` / `REPORT.md`:
  - **boot main-thread-blocked** at 50 Mbit drops from ~10.0 s toward the ~0.9 s
    `local` floor (the sync-fetch component is eliminated; CPU floor remains).
  - **xhr request count** in the boot waterfall drops by ~94 (the boot milos move
    into the single `/api/bundle/boot` request); total bytes ≈ unchanged (or lower
    with R5 gzip).
  - **first-screen wall time** at `normal`/`low` falls (the 2.9 s / 10.0 s freeze
    overlaps GPU bring-up instead of serializing).
- **Negative-control assertion.** Instrument `native_file.cpp`'s
  `WebAssetsFetchSync` branch with a counter published to
  `window.__rb3BootSyncMisses`; after R3 it must read **0 for boot-set paths**
  (anything > 0 means a manifest gap). The netperf harness can read it via
  `engineState`.
- **A/B knob.** `?bootBundle=0` (→ `RB3_BOOT_BUNDLE_OFF`) restores the sync path for
  the control run, so the same build proves the delta.
- **Correctness / no-regression.** A normal cold boot must still reach the first
  interactive screen with the same screen name + song count
  (`window.rb3CurrentScreen`, `window.rb3SongCount`) — the existing boot smoke
  (`scripts/web/lib/core.mjs` + a boot scenario) confirms the bundle didn't drop a
  needed file. Manifest-completeness is exactly the sync-miss-count check above.
- **Bundle-format unit check (cheap, native-ish).** A Python test that round-trips
  `_serve_boot_bundle`'s output through the same parse `onBundleSuccess` does
  (count + per-file records) catches a format regression without a full web build.

## Effort, impact & dependencies

- **Effort: M.** Engine change is a one-line URL parameterization + default arg.
  Server change is a refactor of an existing method + one route. Client change is
  one extra call + one env knob. The manifest generator is the only genuinely new
  piece, and it reads an artifact the netperf suite already emits.
- **Impact: high.** Removes the dominant cold-boot freeze (the 94 sync XHRs / 54 MB
  / up to 10 s blocked at 50 Mbit) and converts it to one overlapped async download
  that hides behind GPU bring-up. This is the single biggest first-load win that
  does not require the larger per-screen-prefetch machinery.
- **Dependencies / ordering:**
  - **Independent of R1** (the async-read rework) — R3 stands alone on the existing
    bundle path.
  - **Overlaps R2** (per-screen prefetch): R3 *is* R2 specialized to "screen 0".
    If R2's prefetch framework lands first, the boot bundle can be re-expressed as
    "prefetch the screen-0 manifest"; if R3 lands first, it proves the
    manifest-driven-async-bundle pattern R2 then generalizes per screen. They share
    the manifest-generator and the parameterized-bundle plumbing — build those in
    R3 to be reusable by R2.
  - **Composes with R5** (wire compression): the boot set gzips ~46%, so gzipping
    `/api/bundle/boot` roughly halves its transfer; the format inside is unchanged.
  - **Unblocks** a clean per-screen-bundle convention for R2.

## Open questions

- **Manifest location & format.** `native/web/boot-assets.manifest` (committed,
  newline-delimited) vs an entry in an existing JSON manifest vs generated into the
  build dir. Committed text is simplest and diff-reviewable; confirm it doesn't
  collide with the `/api/manifest` (full asset list) endpoint semantics.
- **One combined bundle vs two concurrent.** Should the boot milos be appended to
  the *existing* `/api/bundle` (one request, simplest gate) or kept a separate
  `/api/bundle/boot` (lets R2 reuse the per-set route, and lets the config bundle
  stay the same for non-graphical tools)? Leaning two; verify the extra request's
  RTT cost is negligible vs the 54 MB body (it is, at any realistic link).
- **Gzip now or in R5.** Pre-gzip the boot bundle to a static `.gz` in `build.sh`
  (cacheable, no per-request CPU) vs on-the-fly gzip in the server vs defer entirely
  to R5. Pre-baking is the production-shaped answer but adds a build step.
- **First-screen subset.** Is there a smaller "absolute first paint" subset (fonts +
  splash + overshell) worth fetching as a priority bundle before the rest of the 94,
  to paint sooner? Probably premature; the netperf first-screen-wall number after
  Phase 4 decides whether to split.
- **Boot `.dta` on-demand (2 files).** `ui/dev_only/selvenue.dta` +
  `ui/framerate/frame_rate.dta` are sync-fetched at boot but tiny; fold them into
  the config bundle's tree walk (they're `.dta`, so already eligible — confirm why
  they miss the bundle: likely outside the walked `ASSETS_DIR` subtree or
  dev-only-gated) rather than the boot-milo bundle.
- **Manifest determinism across runs.** Does the boot fetch set vary run-to-run
  (timing-dependent lazy loads)? Generate from a `--runs 3` union to be safe, and
  diff successive generations in CI to catch drift.
