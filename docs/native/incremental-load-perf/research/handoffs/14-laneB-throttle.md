# Lane B handoff — chunked, mogg-yielding sharpen-sidecar fetch + reupload retry

**Status: IMPLEMENTED + smoke-passed (unthrottled web, debug build).** Spec:
research/14 §Lane B — every design decision there was followed; the two open
verification questions in the spec are now ANSWERED empirically (below).
The throttled 1.5/4 Mbps A/B against the T2 baselines (635/861 events, 3 events)
is the INTEGRATOR's gate, not run here.

## Commits (NO pin bump — integrator's job)

- engine (`../milo-native-engine`, main): `3e02cea` —
  `WebAssetsRangeInFlightCount()` (WebAssets.cpp/.h) + `RB3SharpenStep`
  retry-on-not-ready (RB3TexSharpen.cpp). Pin bump to `3e02cea` (or later) is
  required for the rb3 driver commit to build against a pinned engine.
- rb3 (master, this commit) — chunk pump driver
  (`native/src/rb3_texsharpen_native.cpp`), retry gtests
  (`native/tests/test_texsharpen_manager.cpp`), smoke harness
  (`scripts/web/_sharpen_chunk_smoke.mjs`), this doc.

## Empirical findings (live server, 2026-07-02)

1. **The manifest does NOT list `.sharpen` sidecars.** `_serve_manifest`
   (native/web/server.py:951) walks only `ASSETS_DIR`; the downscale tree
   (where the 52 sidecars live) is a per-request shadow in `resolve_asset_path`.
   Verified: `RB3_WEB_DOWNSCALE=1` server, `/api/manifest` → 0 of 4986 entries
   end in `.sharpen`. ⇒ `WebAssetsManifestSize(sidecarRel)` returns -1 and
   **short-read EOF detection is the shipping terminator** (the manifest is
   still queried and honored when ≥ 0, future-proofing a server-side union).
2. **Server Range semantics** (small_club_01 sidecar, 5,374,546 B):
   mid-chunk `bytes=0-262143` → 206/262144; final clamped
   `bytes=5242880-5505023` → 206/131666 (**short read ⇒ EOF**); past-EOF
   `bytes=5374546-…` → **416** (surfaces as fetch error → the
   error-after-progress arm treats it as EOF after `kChunkErrMax`=5 fresh
   retries one frame apart).

## Mechanism (exact)

### Engine — `WebAssetsRangeInFlightCount()` (WebAssets.cpp, + header decl)
Counts `sRangeRequests` entries with `!done && !abandoned`. This is the yield
signal: the sharpen driver's own chunk is provably never in flight when it
checks (it only checks when `chunkReqId == 0`), so any live Range fetch is the
mogg's.

### Driver — chunk pump (`rb3_texsharpen_native.cpp`, `__EMSCRIPTEN__` arm)
Replaces the single `WebAssetsEnsureResidentAsync` whole-file fetch. Per
`RB3TexSharpenPoll` frame (gameplay only, phase 1, until MEMFS-resident):

- **Flag:** `RB3_SHARPEN_CHUNK_KB` (default **256**; `0` = legacy single fetch
  — the A/B lever; floor 16, cap 8192).
- At most ONE chunk in flight. If none: **strict yield** — skip the frame when
  `WebAssetsRangeInFlightCount() > 0`; else `WebAssetsRangeFetch(rel, offset,
  chunkBytes)`. Bounded interference = one 256 KB chunk (~1.4 s at 1.5 Mbps) if
  the mogg starts mid-chunk.
- Landing: `WebAssetsRangeDone` → `WebAssetsRangeTake` into the in-memory
  assembly (Take frees the registry entry — the native_file.cpp:996 collect
  pattern). One chunk per frame max (landing and the next kick are separate
  frames — paced, ~21 frames minimum for 5.4 MB unthrottled).
- **Terminators:** short read (`taken < requested`) ⇒ EOF; manifest size
  reached (when known); persistent error after progress (5 fresh retries) ⇒
  EOF — covers the exact-chunk-multiple 416. Truncated-assembly safety: the
  SHRP parser's structural bounds checks reject it → matched 0 → clean
  cosmetic no-op, never a corrupt upload.
- **Finalize:** write assembly to MEMFS `/data/<rel>` (mkdir -p via stdio —
  WebAssets' dir-ensure is a TU-static, not exposed) so `WebAssetsIsResident`
  flips true and the existing `ReadWholeFile → RB3SharpenLoadSidecar` path runs
  UNCHANGED.
- **Fallbacks:** chunk 0 fails 5× (404 / Range-stripping proxy) or MEMFS write
  fails → permanent fallback to the legacy single fetch; legacy lane now also
  detects a dead fetch (`WebAssetsEnsureStatus == 2` → clean no-op instead of
  polling forever — covers venues that legitimately have no sidecar).
- **Reset:** `RB3TexSharpenReset` drops an in-flight chunk via
  `WebAssetsRangeDrop` (the WebAssets.cpp:~820 abandon/detach pattern — no
  leak, no UAF on song unload mid-transfer).
- Native (non-web) path unchanged (local `FileExists` gate).

### Engine — `RB3SharpenStep` retry-on-not-ready (RB3TexSharpen.cpp)
Previously `e.sharpened = true` even when `RB3SharpenReuploadTex` returned
false (`!mGpuReady`) — the entry was lost. Now: entry gains `swapped`/`retries`
state; on `recreated == false` with `retries < 120` the cursor is rewound and
the loop breaks — **not marked done, budget not consumed** (the swap itself is
latched in `swapped` so the retry never re-swaps the already-freed topmip; a
not-ready GPU is global so yielding the rest of the frame's budget is correct).
At the 120-frame cap (~2 s) the entry is marked done with an
`RB3_SHARPEN_DBG` note (the bitmap stays full-res CPU-side; an organic
draw-path upload still picks it up if the GPU returns later).

## How to A/B it (integrator)

Reuse `scripts/web/_sharpen_audio_throttle.mjs` / `_sharpen_flow_gate.mjs`
unchanged — the flag rides the same `window.__rb3ExtraEnv` mechanism:

- **Chunked ON (new default):** no extra env needed (256 KB).
- **Legacy single-fetch arm:** `RB3_SHARPEN_CHUNK_KB: '0'` in `__rb3ExtraEnv`.
- Gate per research/14: 1.5 Mbps chunked-ON underruns ≤ 635 (T2 single-fetch ON
  baseline; OFF was 861) AND sharpen reaches COMPLETE 15/15 (window may be
  longer — cosmetic; log it). 4 Mbps ON sanity ≈ 3 events. `rb3-tests` green.
- Observability: `RB3_SHARPEN_DBG=1` emits `chunk pump start` (manifest size,
  chunk KB), `chunk landed N B @ off (total T)`, `chunk assembly COMPLETE — N
  bytes -> /data/<rel>`, and the existing session lines.
- NOTE on 1.5 Mbps completion: the strict yield means the sidecar only
  transfers in the gaps between mogg fetches (mogg duty cycle at 1.5 Mbps is
  high). COMPLETE will take substantially longer than the ~29 s single fetch —
  that is the intended trade; if it fails to complete within the measured
  window, extend the window before concluding regression (spec: "completion
  may take longer — log the sharpen-window length, it is cosmetic").

## Verification run here (smoke, not the gate)

- `rb3-tests` **53/53 green** (incl. 2 new: `RetriesWhenGpuNotReady` — 0
  progress/0 budget over 5 not-ready frames then 2/2 complete on recovery;
  `RetryCapMarksDoneEventually` — bounded 101..130 steps, no GPU touch).
  `rb3-native` + `rb3-web --debug` build clean.
- Web smoke (unthrottled localhost, debug build, `RB3_WEB_DOWNSCALE=1`,
  20thcenturyboy/small_club_01): see §RESULTS-smoke below.

## RESULTS-smoke

**PASS (exit 0).** Unthrottled localhost, debug build, `RB3_WEB_DOWNSCALE=1`,
20thcenturyboy / small_club_01, chunk 256 KB:

- `chunk pump start … (manifest size -1, chunk 256 KB)` — the in-engine
  manifest miss confirmed live; short-read EOF was the terminator that fired.
- **21 chunks landed** (20 × 262,144 + final clamped 131,666 =
  **5,374,546 B exactly**, one per frame max, in order).
- `chunk assembly COMPLETE — 5374546 bytes ->
  /data/world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox.sharpen`
  (MEMFS write → residency flip → UNCHANGED load path).
- `[sharpen] loaded 17 entries, matched 15` → all 15 `recreate=1` →
  `session COMPLETE: 15 textures, 5177344 bytes upgraded` →
  `RB3_SHARPEN: COMPLETE — 15/15` — byte-identical to the shipped T2/flow-gate
  numbers (research/13 §RESULTS).

Repro: `RB3_WEB_DOWNSCALE=1 python3 native/web/server.py --port 8797` +
`node scripts/web/_sharpen_chunk_smoke.mjs --port 8797` (debug build deployed).
