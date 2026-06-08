# R5 — Wire compression for `/api/file` (`.milo_xbox` gzip/brotli)

**Verdict up front: YES, worthwhile — but as a *secondary*, on-demand-cached server
change, not a build-time pre-generate-everything change.** Measured on the real nav
working set, brotli cuts the on-demand `.milo_xbox` transfer **79 MB → 36 MB (45 %)**,
which at 50 Mbit/s is roughly **12 s → 5.4 s** of synchronous milo freeze time. It does
*not* touch the single worst hitch (the 37 MB `.mogg`, which is already Ogg → ~0 % gain),
so it complements but does not replace R1/R2 (kill the sync XHR / per-screen prefetch).
It is transparent to the C++ engine — **server.py-only** — because the browser
decompresses `Content-Encoding` before the engine's XHR/fetch ever sees the bytes.

## Problem & data

`docs/native/web-netperf-findings-2026-06-08.md` shows every on-demand asset miss is a
**synchronous XHR (`WebAssetsFetchSync`, `xhr.open(..., false)`) that freezes the wasm
main thread for the whole transfer**. Freeze time = `bytes ÷ throughput`. So fewer bytes
on the wire = proportionally less freeze, *for free*, even before R1/R2 make the fetch
async. Today `/api/file` serves `.milo_xbox` **raw** — `_maybe_serve_precompressed`
short-circuits **only** `.wasm`/`.js` (which have `.br`/`.gz` siblings from `build.sh`);
everything under `/api/file/` falls straight through to `_serve_asset_file` with no
`Content-Encoding`.

**Measured compression ratios (this machine: brotli 1.2.0, gzip 1.14, zstd 1.5.7).**
The big milos vary enormously because they embed already-DXT-compressed textures
(incompressible) mixed with DTA/scene-graph/object data (very compressible):

| file | size | gzip -9 | **brotli q11** | brotli q5 | zstd-19 | character |
|---|---|---|---|---|---|---|
| `ui/main/gen/main_hub.milo_xbox`            | 5.27 MB  | 29.0 % | **6.7 %**  | 7.3 %  | 7.0 %  | almost all data → huge win |
| `world/vignette/transition/gen/tv6_a.milo_xbox` | 6.27 MB | — | **36.7 %** | — | — | mostly data |
| `char/main/shared/gen/colorpalettes.milo_xbox` | 20.84 MB | 51.9 % | **40.3 %** | 45.8 % | 42.9 % | mixed |
| `world/vignette/shell/gen/sv3_a.milo_xbox`  | 11.23 MB | 53.7 % | **43.9 %** | 49.0 % | 47.1 % | mixed |
| `world/vignette/shell/gen/sv8_a.milo_xbox`  | 11.84 MB | 62.4 % | **47.3 %** | — | 49.6 % | texture-heavy |
| `sfx/gen/kit03_bank.milo_xbox`              | 4.03 MB  | — | **65.9 %** | — | — | texture/audio-heavy |
| `world/venue/.../small_club_01.milo_xbox`   | 19.43 MB | 67.5 % | **60.0 %** | 66.2 % | — | DXT-texture-heavy → weak |
| `songs/.../20thcenturyboy.mogg`             | 37.4 MB  | **100.0 %** | (skip) | — | — | **Ogg → 0 gain, EXCLUDE** |

**Aggregate over the real nav working set** (the seven `.milo_xbox` the
`boot→main_hub→song_select→part_difficulty→game` path actually fetches, mogg excluded):
**78.91 MB → 35.94 MB with brotli q11 = 45 % (saves 41 MB / 43 M bytes).** That converts
the milo portion of the at-50 Mbit freeze budget from ~12.0 s to ~5.4 s. The mogg is the
single biggest hitch (6.76 s) and gains nothing here — that one needs R1/R3, not R5.

**Build-time cost is the gating constraint.** brotli q11 on the 20.8 MB `colorpalettes`
takes **22.1 s** wall; the full corpus is **4,455 `.milo_xbox` / 3.9 GB**. Pre-compressing
*all* of them at q11 would be ~hours of build time and ~1.8 GB extra on disk for files
that mostly never get fetched in a session (a run touches ~98). So the design must
compress **only what is actually requested**, cached to disk. Faster levels for
reference on `colorpalettes`: brotli q9 = 3.9 s, **brotli q5 = 0.23 s**, gzip -9 = 1.6 s,
gzip -6 = 0.40 s, zstd-19 = 2.8 s, zstd-12 = 0.38 s. brotli **q5 keeps ~90 % of q11's
win** on the compressible files (main_hub 7.3 % vs 6.7 %; colorpalettes 45.8 % vs 40.3 %)
at 1/100th the time.

## Architecture

**Compress-on-demand with a persistent disk cache, served via standard
`Content-Encoding` negotiation. Zero engine/wasm changes.**

Data flow for a `GET /api/file/<rel>` request:

1. Server reads the client's `Accept-Encoding` (emscripten's XHR and `emscripten_fetch`
   both send `Accept-Encoding: gzip, deflate, br` automatically — the browser, not the
   wasm, adds it).
2. If `<rel>` is in a **compressible-extension allowlist** (`.milo_xbox`, `.dta`, `.dtb`,
   `.milo`, …) **and not** in a deny set (`.mogg`, `.ogg`, `.png_xbox`?, `.webm`, `.pcm`)
   **and** no `Range` header is present, the server looks for a cached compressed
   sibling in a **cache dir** (e.g. `native/web/.cache/encoded/<rel>.br`).
3. **Cache hit** → stream the cached `.br`/`.gz`, set `Content-Encoding: br|gzip`,
   `Content-Length` = compressed size, `Vary: Accept-Encoding`. Reuses the existing
   `_serve_encoded()` helper.
4. **Cache miss** → compress the source file once into the cache dir (brotli q5 by
   default — fast enough to do inline on the first request; the cost is paid once, then
   amortized across every later fetch + every page reload), then serve it as in (3).
5. **Anything not in the allowlist, or a `Range` request, or the client sent no
   `Accept-Encoding`** → the existing raw `_serve_asset_file` path, byte-identical to
   today. This is the safe fallback that guarantees no regression.

**Why the browser makes this transparent to the engine.** `Content-Encoding` is a
*transport* encoding. For both the synchronous XHR in `WebAssetsFetchSync`
(`xhr.open(...,false)`; reads `xhr.responseText` with
`overrideMimeType("text/plain; charset=x-user-defined")`) and the async
`emscripten_fetch` path, the browser **decompresses the body before exposing it** —
`responseText` / `fetch->data` already hold the original uncompressed milo bytes. The
`overrideMimeType` only reinterprets the *charset* of the already-decoded body, so the
byte-per-`charCodeAt` reconstruction still yields the exact original bytes. **Verified
the negotiation end-to-end** with a throwaway server: `Content-Encoding: gzip` response,
client decode == original file (bit-identical). No change needed in `WebAssets.cpp`,
`native_file.cpp`, or `Loader.cpp`.

**Cache invalidation.** The cache dir is keyed by `<rel>` + source mtime/size; if the
extracted asset changes, recompress. Simplest: encode the source `(size,mtime)` into the
cache filename or a sidecar `.meta`, and on a hit, `stat` the source and recompress if it
drifted. The cache dir is gitignored derived state (like `orig-assets/derived/`), so a
fresh checkout just starts cold and warms itself.

**Optional build-time pre-warm (later).** Once R2 has a per-screen manifest, `build.sh`
(or a small `scripts/web/precompress-assets.py`) can pre-populate the cache for the
*boot + first-screen* working set only (the ~dozen files in the boot/main_hub waterfall)
at brotli q11, so even the very first cold request is already-compressed and gets the
best ratio. This is strictly additive on top of the on-demand cache and not required for
the win.

## Implementation plan

Phased; high-level. All changes are in `native/web/server.py` plus one optional script.
No C++ / wasm rebuild.

**Phase 1 — on-demand compress + cache in the server (the whole win).**
- Add module-level config near the other dir globals (`ASSETS_DIR`, `SIDECAR_DIR`):
  - `ENCODE_CACHE_DIR` (auto-detected `native/web/.cache/encoded/`, env
    `RB3_ENCODE_CACHE`, `--encode-cache` flag; created on first use).
  - `COMPRESSIBLE_EXTS` allowlist: `{".milo_xbox", ".milo", ".dta", ".dtb", ".dtb_ps3",
    ...}` — text/scene-graph/object data. Start conservative with `.milo_xbox` (the
    measured offenders) and widen after measuring.
  - `INCOMPRESSIBLE_EXTS` deny set as a guard: `{".mogg", ".ogg", ".webm", ".pcm",
    ".png_xbox", ".bmp_xbox", ...}` (already-compressed / DXT). Deny wins over allow.
  - `ENCODE_LEVEL_BROTLI` (default `5`), `ENCODE_LEVEL_GZIP` (default `6`) — tunable.
- New helper `_maybe_compressed_asset(full_path, safe_rel, head_only)` called from
  `_serve_asset_file()` **before** the raw-send block (after the file is resolved to
  `full_path`, before `_serve_range`):
  - Bail (return `False`) if: `Range` header present, ext not allowlisted or in deny set,
    or client `Accept-Encoding` advertises neither `br` nor `gzip`.
  - Pick encoding by preference (`br` > `gzip`) intersected with `Accept-Encoding`.
  - Compute cache path `ENCODE_CACHE_DIR/<safe_rel>.<ext>`; if stale/missing, compress
    `full_path` → cache (use the `brotli`/`gzip` Python modules; brotli is `import
    brotli` — add a graceful fallback to gzip-only if the module is absent, mirroring
    `build.sh`'s "brotli not installed" branch).
  - Serve via the **existing `_serve_encoded(cache_path, full_path, enc, head_only)`**
    (already sets `Content-Type` from base, `Content-Encoding`, `Content-Length`,
    `Vary`). Return `True`.
- In `_serve_asset_file()`, between the `Range` check and the raw `send_response(200)`
  block, insert: `if self._maybe_compressed_asset(...): return`.
- Add `Vary: Accept-Encoding` to the raw asset path too (so shared caches don't serve a
  compressed body to an `identity`-only client).

**Phase 2 — cache management + CLI.**
- `--encode-cache DIR`, `--no-encode` (disable, for A/B), `--encode-level N` args in
  `main()`, mirroring the existing `--assets-dir` / `--sidecar-dir` wiring.
- Startup banner line printing the cache dir + brotli-availability, like the
  `Sidecars:` / `Overlay:` lines.
- mtime/size-keyed staleness check in the cache lookup; optional `--encode-purge`.

**Phase 3 (optional, later, depends on R2) — build-time pre-warm of the boot set.**
- `scripts/web/precompress-assets.py <manifest>`: brotli **q11** the boot + first-screen
  working set into the same cache dir, so the first cold request is best-ratio. Wire an
  opt-in call from `build.sh` (guard behind a flag so the default fast build stays fast).
- Reuses the same cache layout, so the server transparently picks up the q11 siblings.

## Key files & call sites (verified)

- `native/web/server.py`
  - `_maybe_serve_precompressed` — `server.py:122` (today: `.wasm`/`.js` only; the
    template for negotiation; do **not** extend this one — it's the static-build path).
  - `_serve_encoded` — `server.py:149` (**reuse as-is** for the asset path; sets
    `Content-Encoding`/`Content-Length`/`Vary`, `Content-Type` from base).
  - `_serve_asset_file` — `server.py:322` (insert the compressed-asset hook here, after
    `full_path` is resolved, before the `Range` branch at `server.py:383` and the raw
    `200` at `server.py:386`).
  - `_serve_range` — `server.py:399` (the path that must stay raw — Range ⊕ compression).
  - `guess_type` — `server.py:91` (Content-Type stays the base type; unaffected).
  - `main()` arg wiring — `server.py:536`–`593` (add `--encode-cache` / `--encode-level`
    / `--no-encode` next to `--sidecar-dir`).
- `scripts/web/build.sh:186`–`196` — existing `.wasm`/`.js` `brotli -q 11` + `gzip -9`
  block; the Phase 3 pre-warm hook lives near here (opt-in).
- `milo-native-engine/src/platform/WebAssets.cpp`
  - `WebAssetsFetchSync` — `WebAssets.cpp:261` (sync XHR; `xhr.open(...,false)` at `:292`,
    `overrideMimeType` at `:293`, `responseText` at `:301`) — **no Range header → safe to
    compress; no change needed** (browser decodes transparently).
  - `onBundleSuccess`/`WebAssetsFetchBundle` — `WebAssets.cpp:156`/`:245` (async bundle;
    `/api/bundle` is `.dta/.dtb` only today — out of R5 scope, but the same allowlist
    would let the bundle be compressed later).
- `native/src/native_file.cpp:127`–`166` — fopen-miss path (IDB try → `WebAssetsFetchSync`
  → MEMFS write-back). Sees decompressed bytes; **no change**.
- DC3 reference: `dc3-decomp/native/web/server.py` has the identical
  `_maybe_serve_precompressed` (wasm/js only) — **no milo-compression mechanism to reuse**;
  R5 is net-new and should be ported back to DC3's server afterward (shared shape).

## Risks & tradeoffs

- **Range ⊕ Content-Encoding is invalid.** Compressing a body breaks byte-offset range
  requests. *Mitigation:* the hook bails whenever a `Range` header is present (falls to
  `_serve_range` raw). The milo sync-fetch path sends **no** Range header (verified — no
  `setRequestHeader`/Range in `WebAssetsFetchSync`); the only Range consumer is the
  `<video>` movie shim on `.webm` (already excluded). Low risk.
- **First-request latency on a cold cache.** brotli q5 ≈ 0.23 s for a 20 MB file — small
  vs the multi-second transfer it saves, and paid once. q11 inline would add 22 s on the
  first miss (unacceptable) → **default to q5 on demand**, reserve q11 for the optional
  Phase-3 build-time pre-warm. Tunable via `--encode-level`.
- **CPU on the dev server.** This is a single-user dev/demo server; one brotli-q5 per
  uncached file is negligible. Not a production CDN.
- **Disk for the cache.** Only fetched files get compressed; a full session adds ~36 MB
  of cache, not the 1.8 GB a whole-corpus pre-compress would. Gitignored derived state.
- **Cache staleness if assets change.** mtime/size key + recompress-on-drift; cheap.
- **Weak gain on DXT-heavy venues** (`small_club` 60 %, `kit03_bank` 66 %): still a real
  ~35 % cut, just not the headline 6.7 % some files hit. Allowlist by extension, not by
  expected ratio — measuring per-file isn't worth it, and even 60 % helps the freeze.
- **`brotli` Python module may be absent.** Graceful fallback to gzip-only (same pattern
  `build.sh` already uses for the CLI). gzip alone still gets colorpalettes to 52 %,
  main_hub to 29 % — meaningful, just below brotli.
- **Does NOT fix the worst hitch.** The 37 MB mogg (6.76 s freeze) is incompressible; R5
  is explicitly secondary to R1/R2/R3. Set expectations accordingly.

## Verification

1. **A/B with the existing tool — the bytes drop is the proof.** `netperf-suite.mjs`
   records per-transition **bytes** from CDP `encodedDataLength` (the *wire* bytes — line
   `~138`), so compression shows up directly as fewer MB/transition, and **`blockedMs`**
   (sync-XHR freeze) drops with it.
   ```bash
   # baseline (compression off)
   python3 native/web/server.py --no-encode &
   node scripts/web/netperf-suite.mjs --scenario nav --profiles low --runs 3   # -> baseline summary.json
   # treatment (compression on)
   kill %1; python3 native/web/server.py &
   node scripts/web/netperf-suite.mjs --scenario nav --profiles low --runs 3   # -> treatment summary.json
   ```
   Expect: `part_difficulty→game` and `boot→main_hub` **bytes** down ~40–45 % on the milo
   portion (mogg bytes unchanged), and `blockedMs`/`maxGap` down proportionally on the
   milo freezes. The mogg freeze (~6.76 s at 50 Mbit) is unchanged — that's the
   expected, documented limitation.
2. **Negotiation correctness (already prototyped, reproduce in-repo).** `curl` the asset
   API both ways and assert byte-identity after decode:
   ```bash
   curl -s -H 'Accept-Encoding: br'   --output - 'http://localhost:8421/api/file/ui/main/gen/main_hub.milo_xbox' -D /tmp/h.txt | brotli -d | sha256sum
   grep -i content-encoding /tmp/h.txt   # -> br
   curl -s --output - 'http://localhost:8421/api/file/ui/main/gen/main_hub.milo_xbox' | sha256sum   # identity, must match
   curl -s -I -H 'Range: bytes=0-99' 'http://localhost:8421/api/file/ui/main/gen/main_hub.milo_xbox'   # -> 206, NO Content-Encoding
   ```
3. **Functional smoke (no engine regression).** Boot the web build to gameplay
   (`scripts/web/smoke-test.mjs` / the nav path) and confirm the milo scenes still load
   and render — the engine must see identical decompressed bytes. A milo parse failure
   would surface as a missing/garbled scene.
4. **Cache behavior.** Hit a file twice; confirm second request is served from the cache
   dir (no recompress) and that touching the source asset triggers a recompress.

## Effort, impact & dependencies

- **Effort: S.** ~one helper + a few config globals + arg wiring in `server.py`, plus an
  optional small pre-warm script. Reuses `_serve_encoded` wholesale. No C++/wasm rebuild,
  no engine change. A day or less.
- **Impact: medium.** Cuts the on-demand milo transfer ~45 % (79 MB → 36 MB on the nav
  set), shaving the milo freeze budget roughly in half (~12 s → 5.4 s at 50 Mbit). Real,
  measurable, and it stacks with everything else. But it is **secondary**: it does not
  fix the synchrony (R1) or the single worst hitch (the mogg). Bandwidth is fine;
  synchrony is the bug — R5 just makes each frozen transfer shorter.
- **Risk: low.** Pure additive server path with a raw-byte fallback; transparent to the
  engine; verified negotiation.
- **Dependencies / sequencing:**
  - **Independent of R1/R2/R3** — can land immediately and benefits the *current*
    synchronous path right now (shorter freezes today).
  - **Stacks with / helps R1, R2, R3:** when fetches go async and/or per-screen-prefetched,
    smaller bodies mean shorter background downloads and faster warm-up too.
  - **Phase 3 (build-time pre-warm) depends on R2's per-screen manifest** for the
    boot/first-screen working-set list; Phases 1–2 do not.
  - **Should be ported back to DC3** (`dc3-decomp/native/web/server.py`) since the servers
    are near-forks.

## Open questions

- **Extend `/api/bundle` too?** The boot bundle (`.dta/.dtb`, `_serve_bundle` at
  `server.py:272`) is sent raw and is small-ish; compressing it (or letting the same
  allowlist apply) is a trivial follow-on but out of R5's stated `/api/file` scope.
  Worth a one-line check of its measured size first.
- **Which extensions beyond `.milo_xbox`?** Start with `.milo_xbox` (the measured
  offenders). Are `.png_xbox`/texture sidecars DXT (incompressible, deny) or sometimes
  raw (compressible)? Spot-measure a few before widening the allowlist.
- **q5 vs q9 default on demand?** q5 is 0.23 s and keeps ~90 % of the win; q9 is 3.9 s for
  ~5 pp more on the mixed files. q5 is the right default for the first-request path; q9/q11
  belong in the optional pre-warm. Confirm with one A/B if disk/CPU allows.
- **Interaction with the W4b IndexedDB warm cache.** The IDB cache stores the
  *decompressed* MEMFS bytes (post-decode), so warm boots already skip the network and are
  unaffected by R5. R5 strictly improves the *cold* path — confirm no double-work (it
  doesn't: IDB hit short-circuits before the XHR in `native_file.cpp`).
- **Should the cache dir live under `orig-assets/derived/` instead of `native/web/.cache/`?**
  Both are gitignored derived trees; pick whichever the asset-prep tooling already cleans.
