# 08 — Bytes-reduction plan (Wave 5)

**Date:** 2026-06-11. **Status:** measured + designed (this doc); implementation
tasks W5-T1..T3 below.
**Driver:** Wave 4 (PLAN §Wave 4 + research/07) proved that at 4–8 Mbps the pipe
is saturated — wall-clock ≈ totalBytes/bandwidth regardless of concurrency, and
1.5 Mbps DNFs. Parallelism is done; **bytes on the wire are now the #1 lever.**

**Answer in one line:** the journey is NOT "85 MB" — it is **~181 MB of wire
bytes** (the 85 MB was only the milo subset, *already* brotli-q5-compressed);
the two big untapped levers are **59 MB of raw 16-bit PCM SFX sidecars served
uncompressed** (measured: re-encoding to vorbis = **8.9–11.1 % of PCM**) and a
**q5→q11 offline pre-compression upgrade** for everything else (measured:
q11/q5 ≈ 0.89 on the real milo mix). Together they take the cold 4 Mbps journey
from **181 → ~117 MB (−35 %)**; client needs **zero changes** for the
compression part (transparency is already production-proven by the existing
milo/bundle compression). 1.5 Mbps does NOT become playable from compression
alone — shown honestly below.

---

## 1. What the cold journey actually ships (measured)

Per-category **wire** bytes (CDP `encodedDataLength`, i.e. post-Content-Encoding)
from the wave-4 integration runs (`/tmp/rb3perf-w4-integ/*/net.ndjson`) and the
matrix baselines (`/tmp/rb3perf-netmatrix/*/net.ndjson`). Re-derivable: run one
`scripts/web/_netmatrix.mjs` journey and aggregate `net.ndjson` by extension.

| Category | c4-w4 (4 Mbps) | c3-w4 (8 Mbps) | c1-w4 (20 Mbps) | Compressed today? |
|---|--:|--:|--:|---|
| `/api/file` `.milo_xbox` | **85.4 MB** (n=103) | 87.8 | 92.6 | **YES — br q5** (R5; wire/disk = 53.7 %, disk 159 MB) |
| `/api/file` `.pcm` (XMA→PCM SFX sidecars) | **59.1 MB** (n=360) | 58.8 | 60.8 | **NO — served 100 % raw** (`.pcm` is in `INCOMPRESSIBLE_EXTS`, server.py:103) |
| `.mogg` Range chunks | 15.7 MB | 18.9 | 19.9 | n/a (vorbis; measured br = 100 % — correctly skipped) |
| Bundles (boot 13.75 + screens 2.8 + config 1.3) | 17.9 MB | 17.9 | 17.9 | YES — br q5 (boot bundle raw = 60.4 MB → 13.75 wire) |
| wasm + js | 1.6 MB | 1.6 | 1.6 | YES — pre-br by build.sh |
| `.png_xbox` album art | 0.5 MB | — | 0.7 | NO (deny list) |
| `/api/manifest` (JSON, boot critical path) | 0.46 MB | 0.46 | 0.46 | **NO — raw JSON** |
| `.mid` + `.txt` misc | 0.4 MB | — | 0.4 | NO (not in allowlist) |
| **Total journey wire** | **181.1 MB** | 186.6 | 188.8–194.4 | |

Key corrections to the Wave-4 framing:

- **"~85 MB journey" was wrong by 2×** — that was the milo subset only, and it
  is the *post-q5* wire number (disk is 159 MB). The matrix's wall-clock model
  (`wall ≈ bytes/bandwidth`) still holds, but the byte base is ~181 MB.
- **Bundle vs long-tail split:** bundles carry only 17.9 MB (~10 %) of the
  journey; **~146 MB (~80 %) is per-file `/api/file` long tail** (+15.7 MB mogg
  Range). So per-file treatment, not more bundling, is where the bytes are.
- **The PCM burst is positioned exactly where the user hurts.** 360 sidecar
  fetches (350 distinct, p50 = 91 KB, max 1.59 MB) cluster at the transition
  windows (hub ≈ 9.5 MB, hub→select ≈ 30 MB, gameplay-enter ≈ 16 MB) with
  **peak 132 concurrent requests** — they contend with the venue/vignette milos
  for the same throttled pipe during the very transitions that "hang".
  59 MB ≈ **470 seconds of mono 44.1 kHz audio** shipped per cold journey.

### Client transparency — verified, zero client changes needed

The engine's fetch path (emscripten_fetch/XHR + the JSPI suspend path) receives
**browser-decoded** bytes for any `Content-Encoding` response. This is not a
projection: every `.milo_xbox` and every bundle in the measured runs was served
`Content-Encoding: br` and parsed fine in production (R5 landed Wave 0; the
R3↔R5 bundle fix is `6e627201`). The manifest `Size()` oracle reports the
*source* file size (server stats the original), so decoded-size bookkeeping is
already correct. CDP throttling counts **encoded** bytes (proven: milo wire
bytes < disk bytes and transfer durations match wire/bandwidth), so wire
compression converts 1:1 into wall-clock at the gated conditions.

## 2. Measured compressibility (real assets, this machine)

ratio = compressed/original; times are single-core (`brotli`/`gzip` CLI, same
binaries server.py uses). Top rows are the actual biggest journey fetches.

| Asset (real file) | Size | gzip -6 | br q5 | br q11 | q11 time |
|---|--:|--:|--:|--:|--:|
| `world/venue/small_club/.../small_club_01.milo_xbox` (biggest single fetch) | 19.43 MB | 67.7 % | 66.3 % | **60.1 %** | 29.4 s |
| `char/main/shared/gen/colorpalettes.milo_xbox` | 20.84 MB | 52.3 % | 45.9 % | **40.3 %** | 22.8 s |
| `world/vignette/shell/gen/sv4_d.milo_xbox` | 11.96 MB | 70.7 % | 70.1 % | **64.6 %** | 17.9 s |
| `world/vignette/transition/gen/tv3_c.milo_xbox` | 7.11 MB | 44.2 % | 36.9 % | **31.4 %** | 8.4 s |
| `world/shared/gen/chars.milo_xbox` | 3.48 MB | 68.9 % | 59.1 % | 54.5 % | 4.1 s |
| `sfx/gen/kit03_bank.milo_xbox` (99.6 % embedded SynthSample PCM) | 4.03 MB | 82.2 % | 78.0 % | 65.9 % | 7.0 s |
| `songs/20thcenturyboy/gen/*.milo_xbox` (song milo) | 0.32 MB | 64.9 % | 34.1 % | 31.2 % | 0.4 s |
| `.pcm` sidecar (1.59 MB crowd loop, mono 44.1 kHz) | 1.59 MB | 88.4 % | 88.7 % | 75.6 % | 2.4 s |
| `.pcm` sidecar (p50-size, 90 KB) | 0.09 MB | 85.1 % | 85.2 % | 69.2 % | 0.3 s |
| **same crowd loop → vorbis q2 / q4 (ffmpeg libvorbis)** | 1.59 MB | — | — | **8.9 % / 11.1 %** | <1 s |
| `.png_xbox` album art | 0.04 MB | 75.5 % | 76.0 % | 69.9 % | — |
| `.mid` (song midi) | 0.18 MB | 9.8 % | 9.1 % | 8.0 % | — |
| `.mogg` 1 MB chunk (negative control) | 1.05 MB | 99.9 % | 100 % | 100 % | — |

Readings:

- **q11 vs q5 on the journey milo mix ≈ ×0.89** (per-file q11/q5: 0.906, 0.878,
  0.921, 0.851, 0.922, 0.845, 0.915) → milo wire 85.4 → **~76 MB**. q11 costs
  ~1.2–1.5 s/MB single-core → **must be offline/pre-warmed**, never inline
  (q5 inline at 0.01–0.25 s stays the on-demand path).
- **Generic LZ is the wrong tool for PCM** (br q5 ≈ 88 %, q11 ≈ 70–76 %), but
  **vorbis re-encode is ~10×** (8.9–11.1 % measured on a real crowd loop).
  The sidecars are *already lossy-decoded XMA*, mono, SFX/crowd content — a
  q2–q4 vorbis re-encode is far above the source's own fidelity.
- gzip ≈ br q5 on this data; brotli is strictly better at q11. Keep br primary,
  gzip fallback (existing negotiation).

## 3. Milo internals + the A4 question (pre-converted assets)

Checked the container: extracted `.milo_xbox` files are **0xCABEDEAF =
uncompressed-block** containers (verified magic on venue/vignette/song milos) —
no internal zlib, so wire compression is the right lever and the 53.7 % q5
ratio is genuine. Object-type byte attribution (ADDEADDE-split + BE entry-list
parse; Tex chunks identified by their `0000000b 00000002` version signature
learned from a typed milo):

| Milo | Tex (DXT) payload | Implication |
|---|--:|---|
| `colorpalettes.milo_xbox` | **90.5 %** (18.9 MB, 101 Tex) | texture-dominated |
| `small_club_01.milo_xbox` (venue) | **73.5 %** (14.3 MB, 77 Tex) | texture-dominated |
| `sv4_d.milo_xbox` (shell vignette) | 36.0 % | anim/mesh-heavy |
| `tv3_c.milo_xbox` (transition vignette) | 33.2 % | anim/mesh-heavy |
| `kit03_bank.milo_xbox` | 99.6 % SynthSample (embedded PCM) | audio, compresses 66 % @q11 |

**A4 verdicts (extraction-time pre-conversion):**

- **BC-ready textures: already the on-disk format.** The Tex payloads ARE DXT;
  the W2 BC-native upload consumes them directly. **Zero bytes to win.**
- **LE-pre-swap: zero bytes** (BE→LE swap is a load-time CPU lever — belongs to
  the first-frame-hitch workstream, not this wave; also diverges asset truth).
- **Texture downscale / top-mip strip: real but rejected this wave.** Journey
  milos are ~35–74 % DXT; stripping the top mip (−75 % of texture bytes) would
  take post-q11 milo wire ~76 → ~50 MB (journey ~117 → ~91 MB). It requires
  full Tex/RndBitmap parse+rewrite tooling, a visual-diff gate per screen
  (mesh-cache lesson), and per-format edge cases — M–L effort, HIGH visual
  risk — **and it still does not make 1.5 Mbps playable** (see §5 arithmetic).
  Revisit only if the post-wave 4 Mbps experience is still user-unacceptable.

## 4. Chosen design

Three tasks. T1+T2 are independent (disjoint files) and can run concurrently;
T3 gates the integrated result.

1. **W5-T1 — server: q11 offline pre-warm + close the coverage holes.**
   - Extend `COMPRESSIBLE_EXTS` with `.pcm`, `.png_xbox`, `.bmp_xbox`, `.mid`,
     `.txt` (remove `.pcm`/`.png_xbox`/`.bmp_xbox` from the deny set); keep
     `.mogg/.ogg/.webm` denied (measured incompressible).
   - Compress `/api/manifest` (462 KB JSON → ~50 KB; it's on the boot critical
     path, raced by rb3_pre.js against wasm compile).
   - `.meta` gains an encode-level field (`size:mtime:br11`), back-compat with
     the 2-field form so existing q5 cache entries stay valid until upgraded.
   - New `scripts/web/prewarm_encode_cache.py`: walks the asset roots (+ sidecar
     dir) and pre-builds **q11** artifacts into `ENCODE_CACHE_DIR` with the same
     atomic temp+rename layout; also pre-builds the bundle artifacts (boot,
     config, screen-*) by **importing server.py's** bundle-body builder +
     fingerprint (refactor those into module functions — one source of truth,
     no drift). Parallel (`--jobs`), idempotent (skips fresh artifacts at
     ≥ target level), `nice`-d. Full-tree cost ≈ 90 CPU-min ≈ ~6 min wall on
     16 cores; cache ≈ 2.4 GB disk (one-time, gitignored).
   - On-demand q5 stays the cold-start fallback for anything not pre-warmed.

2. **W5-T2 — vorbis SFX sidecars (the 10× lever).** The converter
   (`native/tools/xma_repack`, already links libav) additionally emits
   `<hexkey>.ogg` (libvorbis q4, same PayloadKey — the key hashes the raw XMA
   payload, not the container) next to each `.pcm`. The runtime loader
   (`native/src/rb3_xma_sidecar.h`, rb3-side, NOT engine) tries `.ogg` first
   and decodes with a vendored `stb_vorbis.h` into the same `SidecarPCM`
   (identical rate/channels/numSamples); on 404/decode-fail it falls back to
   `.pcm` exactly as today. server.py needs **no change** (the sidecar route
   already resolves any basename under `SIDECAR_DIR`; `.ogg` is already in the
   deny set for double-compression). 59.1 → **~6.6 MB** per journey.

3. **W5-T3 — integrated byte gates** at the wave-4 policy conditions
   (8 Mbps/80 ms + 4 Mbps/150 ms, 20 Mbps regression backstop) plus the
   1.5 Mbps/300 ms extended-cap retry, with a committed per-category byte
   aggregator so the wire-byte census in §1 is reproducible.

### Rejected alternatives

| Option | Why rejected |
|---|---|
| Re-pack milos as zlib-block (0xCCBEDEAF) | wire brotli already beats zlib, transparently; repack adds an engine inflate path + asset-truth drift for zero net win |
| On-the-fly q11 | 1.2–1.5 s/MB inline = 20–30 s first-request stalls on big venue milos; prewarm gets the same bytes for free |
| PCM wire-compression as the *only* PCM fix | br q11 = 70–76 % vs vorbis ≈ 10 % — leaves 36 MB on the table; kept only as the `RB3_SFX_OGG_OFF` fallback arm (T1 covers it) |
| SFX/pcm bundles (request collapse) | regime is bandwidth-bound, not RTT-bound (browser already runs 6-wide); needed set is runtime-dynamic (content-hash keys); bytes unchanged |
| HTTP/2 server | dev server scope; a production deploy fronts it with a real proxy; changes RTT behavior, not bytes |
| Texture downscale / mip-strip (A4) | §3 — M–L effort + HIGH visual risk; doesn't rescue 1.5 Mbps; revisit only if post-wave 4 Mbps UX still unacceptable |
| LE-pre-swap / BC pre-convert (A4) | zero wire bytes (BC already on-disk; swap is CPU) — belongs to the first-frame-hitch workstream |
| Lower-bitrate moggs | lossy re-encode of the *music* (the product); multitrack+CTR-encrypted re-pipeline; out of bounds |
| Wider screen bundles | collapses requests, not bytes; loses per-file IDB granularity |
| Fetch-less orchestration (skip vignettes etc.) | real lever but belongs to the load-orchestration plan, not bytes; engine/Loader files are owned elsewhere |

## 5. Projection arithmetic

Per-category, c4 (4 Mbps/150 ms) cold journey, wave-4 baseline **181.1 MB**:

| Category | Today | After W5 | Δ | Basis |
|---|--:|--:|--:|---|
| milo per-file | 85.4 | 76.0 | −9.4 | measured q11/q5 ≈ 0.89 weighted on the journey file mix |
| PCM sidecars | 59.1 | 6.6 | −52.5 | measured vorbis q4 = 11.1 % of PCM |
| bundles | 17.9 | 15.8 | −2.1 | same ×0.885 q11 upgrade (boot raw 60.4 MB) |
| manifest JSON | 0.46 | 0.06 | −0.4 | JSON ≈ 12 % under br |
| png_xbox/mid/txt | 0.86 | 0.43 | −0.4 | measured ratios |
| mogg Range | 15.7 | 15.7 | 0 | incompressible (control) |
| wasm/js + misc | 1.7 | 1.7 | 0 | already br |
| **Total** | **181.1** | **≈116.6** | **−64.5 (−35.6 %)** | |

- **4 Mbps/150 ms:** transfer floor 181.1 MB × 8 / 4 Mbps = 362 s → **233 s
  (−36 %)**. `maxSingleMilo` (small_club 12.88 MB wire → 11.67) 35.5 s → ~32 s.
  Gameplay-enter PCM burst ~16 → ~1.8 MB ⇒ ~28 s less contention exactly at
  song start. Boot set ~39 → ~34.5 MB ⇒ appBooted 84.3 → ~76 s.
- **8 Mbps/80 ms:** same bytes at 2× bandwidth — transfer floor 186.6 →
  ~120 MB ⇒ ~133 s of transfer vs 207 s.
- **1.5 Mbps/300 ms (honest):** journey 116.6 MB at 187.5 KB/s ≈ **622 s of
  pure transfer (~11–12 min to gameplay)** vs ≥16 min baseline (measured DNF).
  Boot ~34.5 MB ⇒ appBooted ~190 s (vs 215.5 measured). splash→hub still needs
  ~55–60 MB of venue/char milos ≈ **~5 min** — the harness's 180 s main_hub
  wait still expires unless extended. **Compression alone does NOT make
  1.5 Mbps playable**; it moves it from "DNF" to "completes under extended
  caps". Also structural: gameplay mogg streaming is ~1.2 Mbps ≈ 80 % of a
  1.5 Mbps link — playable-at-3G needs content reduction (deferred A4
  downscale / lite-asset mode / orchestration), not compression. The 1.5 Mbps
  arm is therefore a **measured retry, not a pass/fail gate**.
- Warm visits are unaffected (IDB serves decoded bytes); this wave is about
  **cold** journeys.

## 6. Implementation tasks

### W5-T1 — server q11 pre-warm + compression coverage (model: opus)

**Owns:** `native/web/server.py`, `scripts/web/prewarm_encode_cache.py` (new).

Brief: §4.1. Mechanics: extend allow/deny sets; manifest compression; 3-field
`.meta` (`size:mtime:<enc><level>`) parsed back-compat (2-field ⇒ legacy q5);
`_serve_manifest` goes through the same negotiation; refactor bundle body
construction + fingerprint into importable module functions; prewarm script
imports server.py (no logic duplication), `--jobs N` multiprocessing, `--roots`
defaulting to the auto-detected assets/fallback/sidecar dirs, skips artifacts
whose `.meta` is fresh AND at ≥ target level, writes via the same atomic
temp+rename. Optional `--include` filter. Server gains `--prewarm` (spawn the
script nice-d in the background at startup) but the script must be runnable
standalone. Flags: server `--no-encode` (existing, off-switch), default
behavior ON; prewarm default level q11.

Risks: q11 CPU contention while measuring (run prewarm to completion before
gates; `nice -19`); `.meta` back-compat (legacy entries must stay valid);
bundle fingerprint drift (mitigated by importing server.py); server.py edits
need a server restart (known gotcha).

Verification (pass/fail):
1. `curl -H 'Accept-Encoding: br' -sD- -o/dev/null http://127.0.0.1:<port>/api/file/world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox`
   → `Content-Encoding: br`; after prewarm, body size ≈ 11.7 MB (q11, −9 % vs
   q5's 12.88 MB) and `.meta` records level 11. Same check shows a `.pcm`
   sidecar served `br` and `/api/manifest` ≤ 80 KB on the wire.
2. Range request on a mogg still returns 206 raw; HEAD still correct;
   identity-only client (no Accept-Encoding) gets raw bytes; `--no-encode`
   serves everything raw (A/B arm).
3. Prewarm: first run populates; immediate second run is a no-op (idempotency
   log line count == skipped count); a pre-existing q5 artifact is upgraded.
4. One `scripts/web/_netmatrix.mjs` journey at 4 Mbps/150 ms against the
   restarted server (cold IDB): journey completes, `pageerror`/trap = 0, milo
   wire ≤ 78 MB and bundle wire ≤ 16.5 MB in `net.ndjson` (vs 85.4/17.9
   baseline), frozenΣ = 0 maintained.
5. 20 Mbps/40 ms backstop: appBooted within ±5 % of the wave-4 value.

### W5-T2 — vorbis SFX sidecars (model: opus)

**Owns:** `native/tools/xma_repack/xma_convert_main.cpp`,
`native/tools/xma_repack/milo_sample_scan.h` (only if needed),
`native/tools/xma_repack/CMakeLists.txt`, `scripts/assets/convert_xma_banks.sh`,
`native/src/rb3_xma_sidecar.h`, `native/src/rb3_sampleinst_native.cpp` (only if
the call-site needs it), `native/src/stb_vorbis.h` (vendored, public domain).

Brief: §4.2. Converter: after the existing PCM decode, also encode
`<hexkey>.ogg` (libvorbis q4 via the already-linked libav; same `PayloadKey`,
same per-channel sample count/rate/channels as the `RB3PCM01` header) into
`OUT_DIR`; keep emitting `.pcm` (fallback + native A/B). Runtime
(`rb3_xma_sidecar.h::TryLoad`): try `<hex>.ogg` first (same MEMFS/fetch path —
server's sidecar basename resolution already serves any ext under
`SIDECAR_DIR`), decode via stb_vorbis into the identical `SidecarPCM` contract
(16-bit interleaved malloc'd buffer); fall back to `.pcm` on missing/failed.
Engine files untouched; Wii TUs untouched (header is already HX_NATIVE-only).
Flag: **default ON**, `RB3_SFX_OGG_OFF=1` opt-out (getenv-once static),
`?env=RB3_SFX_OGG_OFF=1` URL bridge works for web A/B.

Risks: lossy→lossy quality on crowd loops (q4 = ~128 kbps-class on mono
sources whose XMA original is far below that; spot-listen + RMS check);
decode-time CPU (stb_vorbis ~ms per 100 KB file, off the render path);
contract drift between converter header and runtime parse (single shared
constant); sidecar regeneration must be re-runnable (`convert_xma_banks.sh`
stays the one entry point).

Verification (pass/fail):
1. Re-run `scripts/assets/convert_xma_banks.sh`: every existing `.pcm` gains a
   sibling `.ogg` (677 → 677), total `.ogg` bytes ≤ 25 MB (vs 186 MB PCM).
2. Decode-equivalence harness (small script or gtest): for ≥10 random sidecars,
   stb_vorbis decode yields exactly the header's numSamples/channels/rate and
   per-file RMS within ±20 % of the PCM original.
3. `rb3-tests` green; native headless gameplay run (existing harness) with SFX
   enabled: no console errors, SFX/crowd audible (capture non-silent), and with
   `RB3_SFX_OGG_OFF=1` byte-identical behavior to today (.pcm path).
4. Web smoke at 4 Mbps/150 ms (cold IDB): `net.ndjson` shows `.ogg` fetches and
   ZERO `.pcm` fetches (except the OFF arm); total sidecar wire ≤ 10 MB vs
   59.1 baseline; `AudioDevice latency GROW` count not worse than baseline;
   journey completes.
5. `audio_verify` gameplay capture still verdict MATCH (music path untouched —
   regression guard only).

### W5-T3 — integrated gates + byte accounting (model: sonnet)

**Owns:** `scripts/web/_netbytes.py` (new), this doc's results section.

Brief: after T1+T2 land and the integration dual build deploys (with the
prewarm completed), run the wave-4 gate policy and write the measured
before/after table back into this doc. Commit the per-category aggregator used
in §1 as `scripts/web/_netbytes.py <net.ndjson>` (wire bytes by
milo/pcm-ogg/mogg/bundle/misc + top-10 largest fetches + duplicate-fetch
count) so the census is one command.

Verification (pass/fail gates):
1. **8 Mbps/80 ms (c3):** journey completes; total wire ≤ 130 MB (baseline
   186.6); frozenΣ = 0 on hovers/transitions (canvas property preserved).
2. **4 Mbps/150 ms (c4):** journey completes; total wire ≤ 125 MB (target
   ~117, baseline 181.1); milo wire ≤ 78 MB; sidecar wire ≤ 10 MB;
   maxSingleMilo ≤ 33 s (baseline 35.5); chunkReDownloads = 0 preserved.
3. **20 Mbps/40 ms backstop:** appBooted within ±5 % of wave-4 (19.6 s);
   hovers frozenΣ = 0.
4. **1.5 Mbps/300 ms extended-cap RETRY (measured, not pass/fail):** report
   appBooted / splash / main_hub / gameplay timestamps with the
   `_netmatrix_slow.sh`-style extended waits; expectation ≈ appBooted ≤ 200 s
   and gameplay reachable in ~11–13 min (vs DNF baseline). Record verdict
   honestly — this arm only graduates to a gate after a content-reduction
   wave.
5. Flag A/B: `RB3_SFX_OGG_OFF=1` and server `--no-encode` arms still reach
   song_select clean.

## 7. Repro commands

```bash
# byte census from any netmatrix run
python3 scripts/web/_netbytes.py /tmp/rb3perf-w4-integ/c4-w4-run1/net.ndjson  # (T3)

# compressibility spot-check (matches §2 method)
brotli -q 11 -c orig-assets/extracted/world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox | wc -c

# sidecar vorbis ratio spot-check (matches §2: 8.9 % at q2 / 11.1 % at q4)
python3 -c "d=open('orig-assets/derived/sfx_pcm/e0e745ba50227954.pcm','rb').read(); open('/tmp/sc.raw','wb').write(d[20:])"
ffmpeg -f s16le -ar 44100 -ac 1 -i /tmp/sc.raw -c:a libvorbis -q:a 4 /tmp/sc.ogg

# gates (wave-4 policy): own server on 8446-8459, cold-IDB netmatrix per arm
python3 native/web/server.py --port 8446   # restart after server.py edits!
cd scripts/web && node _netmatrix.mjs --port 8446 --out /tmp/w5-c4 --mbps 4 --rtt 150 --label c4 --run 1
```
