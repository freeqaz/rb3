# R3 — Eager boot-bundle expansion: pre-pack the boot `.milo_xbox` working set into the async bundle

> Status: design / handoff. Author: investigation pass 2026-06-08.
> Revised 2026-06-08 to fold in adversarial-review must-fixes (W4b/IDB warm-boot
> regression; corrected gzip ratios; combined-vs-separate route). See
> "Review corrections applied" at the end.
> Verification tool: `scripts/web/netperf-suite.mjs` (CDP-throttled boot waterfall).
> Sibling items: depends on nothing (**fully independent of R1**); overlaps R2
> (per-screen prefetch — boot is "screen 0") and R5 (wire compression — the boot
> set gzips *better* than the venue milos, so the two compose strongly). R3 is the
> recommended early win after R5.

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
5. **IDB write-back (mandatory — closes the W4b warm-boot regression)**: as
   `onBundleSuccess` unpacks each file into MEMFS it must **also** persist it to the
   W4b IndexedDB cache, exactly as the sync path does. This is not optional polish —
   without it R3 is a **net regression for returning users** (see below).

### W4b / IndexedDB warm-boot regression — must-fix, not a nicety

Today the *sync* miss path is also the IDB write-through path: after a successful
`WebAssetsFetchSync`, `native_file.cpp` (L160-167) computes a cache key
(`cacheRelFromMemfsPath`) and calls `cachePutAfterFetch` (`native_file.cpp:100` →
`window.__rb3CachePut`), so a **second** cold boot serves all ~94 boot milos from
local IndexedDB with **zero network** (the boot opens hit `cacheTryHit`,
`native_file.cpp:66` → `window.__rb3IdbCache`, before they ever reach the XHR).

`onBundleSuccess` (`milo-native-engine/src/platform/WebAssets.cpp:156`) writes
straight to MEMFS (`fopen`/`fwrite`, L224-227) and **never** calls
`cachePutAfterFetch` / `window.__rb3CachePut` — those helpers live only in
`native/src/native_file.cpp`, on the sync miss path. So a naïve R3 that just routes
the boot set through the bundle would **bypass IDB entirely**: warm/repeat boots
would **re-download the entire ~54 MB boot bundle** every time, regressing the W4b
warm-cache win for exactly the returning-user case it was built for.

**R3 therefore must do one of (prefer the first):**
- **(a) Write each unpacked file back to IDB during unpack.** In `onBundleSuccess`,
  after the MEMFS `fwrite` succeeds, persist the file to the W4b cache using the same
  key derivation the sync path uses (`cacheRelFromMemfsPath(memfsPath)` →
  `window.__rb3CachePut(key, bytes)`). Since `onBundleSuccess` already holds the file
  bytes in `data`, it can hand them to `__rb3CachePut` directly — no extra
  `FS.readFile`. This is engine-shared code (DC3 also calls `WebAssetsFetchBundle`),
  so the write-back must be **HX_WEB/RB3-gated** or guarded on the presence of
  `window.__rb3CachePut` (DC3 may not define it) so it is a no-op where the hook is
  absent. **The key derivation must match the sync path's exactly** or warm boots
  miss the cache they just populated.
- **(b) Gate the bundle off when the boot set is already IDB-resident.** Before firing
  `/api/bundle/boot`, query `window.__rb3IdbCache` for the manifest entries; if all
  (or a high fraction) are present, skip the bundle and let the warm sync opens serve
  from IDB at ~zero cost. Simpler to reason about, but it re-introduces the per-file
  sync-open path on warm boots (cheap — IDB hits don't touch the network — but it
  forgoes the single-unpack efficiency). (a) is preferred; (b) is an acceptable
  fallback if writing through the shared engine path proves awkward.

Why this shape:
- **Reuses the entire async + unpack path** (`emscripten_fetch` → `onBundleSuccess`
  → MEMFS) that is already load-bearing for the config bundle. Zero new transport.
- **Overlaps the boot CPU work**: the bundle downloads while WASM compiles / the GPU
  device comes up (`BOOT_GPU_WAIT` is several RAF ticks of pure async wait), so on a
  fast link the 25 MB-compressed boot bundle is largely free.
- **Manifest, not extension**: the full extracted set is **4455 `.milo_xbox` / ~4 GB**
  — gating the bundle by extension is impossible. The boot set must be an explicit
  curated list (54 MB), which is exactly what a manifest gives.
- **R5 composes strongly**: the boot milos are texture/mesh/scene-graph data that
  compresses well, so serving the boot bundle with `Content-Encoding: gzip` (or
  pre-gzipping it) meaningfully shrinks the ~54 MB on the wire. **Gzip ratio is
  milo-DEPENDENT** — measured: `colorpalettes.milo_xbox` (the 20.8 MB boot offender)
  gzips to **~52%**, `small_club_01.milo_xbox` (a venue, R2's territory) to **~68%**;
  a different, already-DXT-packed milo can be ~99% (the DXT block payload is
  near-incompressible — that residual is R6's job, GPU BC upload). So the earlier flat
  "boot set gzips ~46% / venues gzip ~98% — useless" claim was **wrong on both
  counts**: R5 *does* help the venue milos, and R3's boot set compresses *better* than
  the stated ~46% (closer to ~50% on the dominant entries). R3 (boot) and R2 (venues)
  remain separate for *prefetch-timing* reasons (boot vs. screen transition), **not**
  because venues are incompressible.

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
- Add `_serve_boot_bundle()` on a **separate route** `/api/bundle/boot` (decision:
  separate route, *not* appended to the existing `/api/bundle` — see Open questions):
  read `native/web/boot-assets.manifest`, resolve each path with the existing
  overlay/fallback/`(..)` logic from `_serve_asset_file`, and call `_emit_bundle`.
  Keeping it a distinct per-set route lets the config bundle stay byte-identical for
  non-graphical tools, and gives **R2 a reusable `/api/bundle/<set>` convention** to
  generalize per screen.
- Optional: gzip the boot-bundle body and set `Content-Encoding: gzip` when the
  client `Accept-Encoding` allows it (this is the R5 hook; can land later — the
  format inside is unchanged, the browser inflates before `onBundleSuccess` sees it).
  The boot set compresses ~50% (corrected ratio), so this is a real ~54 MB → ~27 MB win.

**Phase 2 — engine: parameterize the bundle fetch *and* add IDB write-back.**
- In `milo-native-engine/src/platform/WebAssets.{h,cpp}`, change
  `WebAssetsFetchBundle()` (`WebAssets.cpp:245`, currently hardcodes `/api/bundle` at
  L253) to `WebAssetsFetchBundle(const char *url = "/api/bundle")` (default keeps
  every existing caller, incl. DC3, working). The body is identical except the URL
  string. `onBundleError`/`sPending` are unchanged and already handle N concurrent
  bundles via the shared counter.
- **In `onBundleSuccess` (`WebAssets.cpp:156`), add the W4b IDB write-back** (the
  must-fix). After the successful MEMFS `fwrite` (L224-228), persist the file to the
  W4b cache so warm boots don't re-download the bundle. Two options for *where* the
  write-back lives, since `cachePutAfterFetch`/`__rb3CachePut` are defined in RB3's
  `native/src/native_file.cpp`, not in the engine:
  - **(preferred) an RB3-provided hook**: declare a weak/overridable
    `RB3BundleCacheWriteThrough(const char *memfsPath, const void *bytes, size_t n)`
    that `onBundleSuccess` calls per file; RB3 implements it in `native_file.cpp`
    (deriving the key via `cacheRelFromMemfsPath` and calling `__rb3CachePut`), and it
    is a **no-op / absent for DC3**. This keeps the engine free of RB3-specific cache
    semantics.
  - **(alt) inline, guarded on the JS hook**: have `onBundleSuccess` call
    `EM_ASM`-style `if (window.__rb3CachePut) window.__rb3CachePut(key, bytes)` directly,
    computing the same key the sync path uses. Simpler but bakes RB3's key scheme into
    shared code.
  Either way the **key derivation MUST match the sync path's exactly**
  (`cacheRelFromMemfsPath` over the resolved `/data/...` memfsPath) or warm boots miss
  the cache they just filled. Add this even if the bundle gzip lands later — the
  write-back is correctness, not perf.

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
**Crucially, add a WARM/persisted-IDB run** (boot once to populate IDB, then boot
again *with the network throttled and the HTTP cache cleared*) and assert the boot
bundle is **not** re-downloaded — this is the only run that surfaces the W4b
regression; the cold-only netperf matrix never does.

## Key files & call sites (verified)

- `milo-native-engine/src/platform/WebAssets.cpp`
  - `WebAssetsFetchBundle()` — L245: hardcodes the URL at `emscripten_fetch(&attr, "/api/bundle")` (L253); **parameterize the URL here** (default arg).
  - `onBundleSuccess()` — L156-236: parses `uint32 count` (L172) then per-file `pathLen/path/dataLen/data` (L176-190), `..`-resolves (L195-214), mkdir + `fopen`/`fwrite` into MEMFS (L224-227), bumps `sCompleted` (L233). **Reused for the boot bundle, but NOT unchanged — add the W4b IDB write-back per file** right after the `fwrite` succeeds (L228). It does **not** currently call `cachePutAfterFetch`/`__rb3CachePut` — that gap is the warm-boot regression.
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
  - `NativeStdioFile` ctor fopen-miss path — L137-168: `cacheTryHit` (L161) → `WebAssetsFetchSync` (L163) → retry, then `cachePutAfterFetch` (L166) on success. **The freeze this item removes; add a one-shot log/counter to prove boot-set files no longer reach it.**
  - **IDB write-through helpers R3 must reuse** — `cacheRelFromMemfsPath` (L55, key derivation from a `/data/...` path), `cacheTryHit` (L66 → `window.__rb3IdbCache`, L70-73), `cachePutAfterFetch` (L100 → `window.__rb3CachePut`, L103-108). These live **only here** (the sync path), which is exactly why `onBundleSuccess` bypasses IDB today. R3's bundle write-back must call the same key derivation + `__rb3CachePut`, either by exposing an RB3 hook the engine calls or by inlining the `__rb3CachePut` call in `onBundleSuccess` (DC3-safe-guarded).
- `native/CMakeLists.txt` — L642 (`WebAssets.cpp` in the web TU list), L874 (`rb3_pre.js` pre-js). No source-list change needed unless the manifest generator is added as a target.
- `scripts/web/netperf-suite.mjs` — `trackNetwork()` L125-, waterfall emit L338 (`network-waterfall.json`). The verification + manifest-source tool.

## Risks & tradeoffs

- **W4b warm-boot regression (the headline risk — mitigated by design, not optional).**
  If R3 ships *without* the IDB write-back, returning users re-download the ~54 MB
  boot bundle on every boot, regressing the shipped W4b warm cache (which today serves
  all ~94 boot milos from IndexedDB with zero network on a 2nd boot). The sync path
  writes through to IDB (`cachePutAfterFetch`); `onBundleSuccess` does not. Mitigation
  is in the design (Architecture step 5 + Phase 2): write each unpacked file to IDB
  during unpack, OR gate the bundle off when the boot set is already IDB-resident — and
  the warm-boot verification run above must pass before R3 lands. **This must be in the
  first cut, not a follow-up**; a returning-user regression is worse than the cold-boot
  win for the population that boots more than once.
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
- **Warm-boot / persisted-IDB run (mandatory — guards the W4b regression).** The
  cold-only matrix above CANNOT see the IDB regression. Add a two-boot run: (1) boot
  once with the bundle to populate IndexedDB; (2) clear the HTTP cache, throttle the
  link, and boot again. Assert the boot bundle (`/api/bundle/boot`) is **not**
  re-requested (network waterfall shows 0 bytes for it / served from IDB), and that
  warm-boot wall time is at/near the `local` floor. Without the IDB write-back this
  run fails (the full ~54 MB re-downloads), which is precisely the returning-user
  regression. The harness can also read `window.__rb3IdbCache` size via `engineState`
  to confirm the boot set was persisted.
- **Correctness / no-regression.** A normal cold boot must still reach the first
  interactive screen with the same screen name + song count
  (`window.rb3CurrentScreen`, `window.rb3SongCount`) — the existing boot smoke
  (`scripts/web/lib/core.mjs` + a boot scenario) confirms the bundle didn't drop a
  needed file. Manifest-completeness is exactly the sync-miss-count check above.
- **Bundle-format unit check (cheap, native-ish).** A Python test that round-trips
  `_serve_boot_bundle`'s output through the same parse `onBundleSuccess` does
  (count + per-file records) catches a format regression without a full web build.

## Effort, impact & dependencies

- **Effort: M.** Engine change is a URL parameterization + default arg, **plus the
  W4b IDB write-back in `onBundleSuccess`** (a per-file `__rb3CachePut` call reusing
  the sync path's key derivation, DC3-guarded) — small but correctness-critical, not
  the original "one-liner". Server change is a refactor of an existing method + one
  route. Client change is one extra call + one env knob. The manifest generator and
  the warm-boot verification run are the genuinely new pieces; the manifest reads an
  artifact the netperf suite already emits.
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
  - **Composes with R5** (wire compression): the boot set compresses well (~50% on
    the dominant entries, e.g. `colorpalettes` ~52%), so gzipping `/api/bundle/boot`
    roughly halves its transfer; the format inside is unchanged. (R5 *also* helps the
    venue milos R2 handles — the old "venues gzip ~98% — useless" claim was wrong; the
    near-incompressible residual is the DXT payload, which is R6's territory.)
  - **Unblocks** a clean per-set `/api/bundle/<set>` convention for R2 (one of the
    reasons R3 uses a separate `/api/bundle/boot` route rather than appending to the
    existing config bundle — see Open questions).

## Open questions

- **Manifest location & format.** `native/web/boot-assets.manifest` (committed,
  newline-delimited) vs an entry in an existing JSON manifest vs generated into the
  build dir. Committed text is simplest and diff-reviewable; confirm it doesn't
  collide with the `/api/manifest` (full asset list) endpoint semantics.
- **One combined bundle vs a separate `/api/bundle/boot` route — DECIDED: separate.**
  Appending the boot milos to the *existing* `/api/bundle` is one request / simplest
  gate, but a **separate `/api/bundle/boot`** is the recommendation: it lets R2 reuse
  a per-set `/api/bundle/<set>` convention, keeps the config bundle byte-identical for
  non-graphical tools (which don't want 54 MB of milos), and lets the boot bundle be
  gated/skipped independently on warm boots (the IDB-resident gate). The extra
  request's RTT is negligible vs the 54 MB body at any realistic link, and both fire
  concurrently in `BOOT_INIT` so there is no added serialization. Recorded as the lean
  decision; revisit only if the per-set route proves to cost a measurable RTT.
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

## Review corrections applied

(Adversarial-review pass 2026-06-08; all verified against the code before folding in.)

- **W4b/IDB warm-boot regression — now a first-class must-fix.** Added Architecture
  step 5 + a dedicated "W4b / IndexedDB warm-boot regression" subsection: `onBundleSuccess`
  (`WebAssets.cpp:156`) writes to MEMFS but never calls `cachePutAfterFetch`/`__rb3CachePut`
  (those live only in `native/src/native_file.cpp`, the sync miss path, L100/L160-167), so a
  naïve R3 re-downloads the 54 MB bundle on warm boots. R3 must write each unpacked file back
  to IDB (preferred) or gate the bundle when the set is already IDB-resident.
- **Phase 2 rewritten** from "one-line URL parameterization" to URL param **plus** the IDB
  write-back in `onBundleSuccess`, with the exact-key-match requirement and DC3-guard.
- **Verification + Phase 4 + Effort** updated to require a **warm/persisted-IDB run** (the
  cold-only matrix can't surface the regression) and to reflect the added correctness work.
- **Gzip claim corrected.** Replaced the flat "boot gzips ~46% / venues gzip ~98% — useless"
  with the measured, milo-dependent reality: `colorpalettes.milo_xbox` ~52%, `small_club_01`
  ~68%, some already-DXT-packed milos ~99% (R6's residual). R5 *does* help the venues; R3's
  boot set compresses *better* than the stated ~46%. Fixed in Architecture, Open questions,
  and Dependencies.
- **Combined-vs-separate route — recorded as DECIDED (separate `/api/bundle/boot`)** so R2
  reuses the per-set convention; noted in Phase 1, Open questions, and Dependencies.
- **R1 independence + early-win status** clarified in the header and Dependencies (R3 is the
  recommended early win after R5, fully independent of R1).
- **Line citations corrected** where the doc had drifted (`onBundleSuccess` MEMFS write
  L224-227, not L216-229; `WebAssetsFetchBundle` URL hardcode at L253; added the
  `native_file.cpp` IDB-helper call sites `cacheRelFromMemfsPath`/`cacheTryHit`/`cachePutAfterFetch`).
