# XMA Audio Gap — Implementation (offline XMA→PCM sidecars, RB3 + DC3)

Date: 2026-06-02. Closes the silent-SFX gap from
[xma-decode-investigation.md](xma-decode-investigation.md). The remaining silent
SFX are Xbox-360 **XMA** samples (`SampleData::Format == 3 = kXMA`) embedded in
`.milo_xbox` containers. ffmpeg is linked on native but **not** on web, so the
fix is **offline** conversion: decode kXMA→PCM once, so the existing PCM playback
path Just Works on native AND web for both repos.

## Chosen approach: **B — sidecar + runtime glue hook** (NOT A, in-place repack)

Why B over A (in-place milo rewrite):

- **A requires error-prone container surgery.** XMA→PCM is ~5–10× larger, so an
  in-place rewrite shifts every downstream object offset and must recompute the
  milo `ChunkInfo` header (chunk sizes / `mMaxChunkSize`) and the object stream's
  internal framing. A malformed rewrite silently corrupts the whole bank.
  `arkhelper` (Mackiloha) is **not present** at `tools/mackiloha/arkhelper`
  (only a zip in `/home/free/code/milohax/tools/mackiloha/`), so there is no
  validated round-trip repacker to lean on.
- **B touches zero container bytes.** The offline tool only *reads* each kXMA
  blob; it writes a separate PCM sidecar. A tiny runtime hook loads the sidecar
  when a kXMA sample plays. Originals stay pristine; the loader is unchanged.
- B is bank-independent: sidecars are keyed by a **content hash of the raw XMA
  payload** (FNV-1a over the bytes + size + sample rate), so the runtime
  `SampleInst`, which only has the loaded `SampleData`, finds the right sidecar
  without knowing which bank/object it came from. Identical L/R mono pairs
  de-dup naturally (677 unique sidecars for 1706 RB3 blobs).

The codec is **reused verbatim** from DC3's validated `DecodeXMAToPCM`
(`dc3-decomp/native/src/platform/XmaSampleDecoder.cpp`, ffmpeg `xma2`).

## Key milo-container facts (verified against all RB3 banks)

- RB3 SFX `.milo_xbox` are **uncompressed** milo containers: a 0x810-byte
  little-endian `ChunkStream::ChunkInfo` header (id `0xCABEDEAF`) then the milo
  object stream (**big-endian**). Single or few chunks; payload = concatenated.
- Each bank holds N `SynthSample` objects. `SynthSample::PreLoad` reads
  `mFile`(milostring) `mIsLooped`(bool) `mLoopStartSamp`(int) `mLoopEndSamp`(int)
  then **inline** `SampleData::Load`: `rev fmt numSamples sampleRate sizeBytes`
  (all big-endian int), `hasData`(bool, rev≥0xB), `[payload]`. RB3 `SampleData`
  has **no** `mNumChannels` (banks are mono). XMA payloads are 2048-aligned.
- **kXMA is NOT only in `sfx/gen/`.** It is spread across many containers: UI
  (endgame/tour/trainers), all venue crowd loops (`world/venue/*`), closet, and
  dozens of `world/vignette/*` cutscenes. The converter therefore scans the
  **entire** extracted tree recursively.

## RB3 — what was added/changed (branch `wt-xma`, worktree `.claude/worktrees/xma`)

Offline converter (standalone, links libav only — NOT the milo engine):

- `native/tools/xma_repack/milo_sample_scan.h` — read-only RB3 milo bank parser
  (locates SampleData blobs) + `payload_key` (the content hash).
- `native/tools/xma_repack/xma_convert_main.cpp` — CLI `rb3-xma-convert`: scans
  banks, decodes kXMA via `DecodeXMAToPCM`, validates **RMS>0** + decoded sample
  count, writes `<hexkey>.pcm` sidecars + `manifest.txt`. Originals untouched.
- `native/tools/xma_repack/CMakeLists.txt` — builds `rb3-xma-convert`
  (pkg-config libav + DC3's `XmaSampleDecoder.cpp`, `HX_FFMPEG=1`). No engine.
- `scripts/assets/convert_xma_banks.sh` — reproducible wrapper: builds the tool
  if needed, recursively finds every `*.milo_xbox` under `orig-assets/extracted`,
  converts to `orig-assets/derived/sfx_pcm/`.

Runtime glue (gated `#ifdef HX_NATIVE` → native + web):

- `native/src/rb3_xma_sidecar.h` — `rb3_xma::TryLoad` (content-key lookup +
  sidecar read) + `PayloadKey` (must match the converter, verified).
- `native/src/rb3_sampleinst_native.cpp` — `StartImpl` now handles `kXMA`: load
  the sidecar, play it as little-endian mono PCM (owned buffer, freed in dtor).
  Un-converted kXMA warns once and is skipped (no regression). The
  `unsupported sample format` line no longer covers XMA.
- `native/src/main_native.cpp` — `RB3_XMA_VALIDATE=1 <bank>` mode: harness-
  independent validator that reads a bank directly and confirms every kXMA blob
  resolves to a sidecar (no engine boot — the headless `SystemInit` boot spine
  is fragile outside the full App path).

**No matched-fork `src/**` file was edited.** All runtime changes are in
`native/src/` glue or new `native/tools/`.

## DC3 — what was added/changed (branch `wt-xma-dc3` in `dc3-decomp`)

DC3 already decodes kXMA at load time on native (`SampleData::Load`,
`#ifdef HX_FFMPEG`). The web gap is purely that `HX_FFMPEG` is off on web. The
DC3 fix is **native-writes / web-reads**, so DC3's own validated loader generates
the web sidecars — no separate DC3 milo parser needed:

- `native/src/platform/XmaPcmSidecar.h` (new) — `dc3_xma::{PayloadKey,
  WriteSidecar,TryLoad}`. Same key + file format as RB3 (one shared dir serves
  both). Round-trip tested standalone (0 mismatches).
- `src/system/synth/SampleData.cpp` — inside the existing `#ifdef HX_FFMPEG`
  decode block, after a successful decode it now **also** `WriteSidecar`s the
  PCM (keyed off the raw XMA, captured before `Dealloc`). A new `#elif
  defined(HX_NATIVE)` branch (the ffmpeg-less web build) **reads** the sidecar
  into `mData` and flips `mFormat=kPCM`. Both are invisible to the matched Wii
  build (`HX_NATIVE`/`HX_FFMPEG` undefined there), exactly like the pre-existing
  HX_FFMPEG block — so the asm match is preserved.

Note: DC3's `milo2gltf` (the natural sidecar generator — loads any `.milo_xbox`
through DC3's loader) currently fails to rebuild on an **unrelated**, pre-existing
engine-state mismatch in `dc3-decomp/native/build` (`GameRenderHook.h` not found
when recompiling `Rnd_Wgpu.cpp`). `dc3-native` itself builds clean with the
change. Generating the DC3 sidecars (a native asset-prep run with
`DC3_SFX_PCM_DIR` set) + the web audibility check is the orchestrator's
post-merge step once that build env is healthy.

## How to run the converter (reproducible)

```bash
# RB3 (from the worktree). Builds rb3-xma-convert if needed, then converts ALL
# kXMA samples under orig-assets/extracted into orig-assets/derived/sfx_pcm/.
scripts/assets/convert_xma_banks.sh
#   arg1 = scan dir (default orig-assets/extracted, recursive)
#   arg2 = out dir  (default orig-assets/derived/sfx_pcm)

# Validate a specific bank through the runtime loader (no engine boot):
RB3_SFX_PCM_DIR=$PWD/orig-assets/derived/sfx_pcm \
  native/build-native/rb3-native  # build RB3_XMA_VALIDATE=1 <bank.milo_xbox>

# Runtime: point the game at the sidecars (absolute, since the game chdir's to
# RB3_DATA). Or place the *.pcm under <RB3_DATA>/sfx/gen/xma_pcm/ (the default).
RB3_GAME=1 RB3_SFX_PCM_DIR=$PWD/orig-assets/derived/sfx_pcm \
  RB3_DATA=/.../orig-assets/extracted  native/build-native/rb3-native
```

Web bundle: ship `orig-assets/derived/sfx_pcm/*.pcm` alongside the extracted
assets so the MEMFS-mounted data tree contains them (mirrors
`dc3/scripts/web/transcode_bik.sh` — decode offline, bundle host-friendly
format). The web `SampleInst` (RB3) / `SampleData::Load` (DC3) reads them at play
time. Converted output is **not committed** (180 MB); regenerate with the script.

## Validation results

Offline conversion (RB3, full recursive scan):

- **4455 banks scanned, 1706 kXMA blobs found, 1706 converted, 0 failed, 0
  silent.** 677 unique sidecars after content de-dup. 180 MB total.
- **1706/1706 decoded sample count == stored `mNumSamples`** (exact). RMS in
  [0.0144, 0.4043] — every sample is real audio, none decoded to silence.

Runtime resolution (RB3, `RB3_XMA_VALIDATE`, harness-independent, via the real
runtime `TryLoad`):

- 11 banks across every category (SFX, UI, all 4 venue types, vignette, closet):
  **508 kXMA, 508 resolved, 0 missing.** PCM-only banks (cowbell) correctly show
  0 XMA / N PCM.

End-to-end runtime (RB3, real `RB3_GAME` App boot, this box is headless so
audibility is the orchestrator's check):

- A previously-silent kXMA sample (`sv8_a` train, 132740 @ 28000 Hz) now logs
  `rb3 SFX: playing XMA->PCM sidecar (132740 samples @ 28000 Hz)`. The
  `kXMA ... no PCM sidecar` line is gone. Big-endian PCM still plays.
- Cross-check: runtime `rb3_xma::PayloadKey` == converter `milo_scan::payload_key`
  for real on-disk blobs (and matches the manifest hex), so the lookup contract
  is exact.

DC3: helper write/read round-trips (0/1000 mismatches), key is byte-identical to
RB3's; `dc3-native` builds clean with the `SampleData.cpp` change. Sidecar
generation + web audibility deferred to the orchestrator (DC3 build-env fix).

## Caveats / open items

- **Audibility is unverified here** — this box has no audio output
  (`MILO_HEADLESS` → "AudioDevice: skipped"). Verified: source registers,
  format flips kXMA→PCM, sidecar resolves + loads. Real-output / in-browser
  audibility is a manual post-merge check.
- **DC3 sidecars not yet generated** (milo2gltf rebuild blocked by an unrelated
  engine-state mismatch in dc3's build dir). DC3 code path is in place + unit-
  tested; an orchestrator native asset-prep run generates them.
- `chartest.milo_xbox` (a test artifact with a malformed `../../` path) is
  scanned harmlessly; its 1 kXMA blob converts like any other.
- DC3 stereo: `WriteSidecar`/`TryLoad` carry `numChannels`, so DC3's stereo XMA
  (where present) round-trips; RB3 is mono throughout.
- Web build deliberately NOT rebuilt (slow; per task) — the runtime glue is
  shared `HX_NATIVE` code already exercised by rb3-native.
