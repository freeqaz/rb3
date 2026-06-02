# DC3 XMA→PCM Sidecar Generation (the DC3 half of the XMA audio fix)

Date: 2026-06-02. Completes the DC3 side of the offline XMA→PCM sidecar work
described in [xma-conversion-impl.md](xma-conversion-impl.md). The DC3 *runtime*
code was already committed (dc3 branch `wt-xma-dc3`, commit `ad2b0118`):
`SampleData::Load` WriteSidecar's decoded PCM on native (`HX_FFMPEG`) and a new
`#elif HX_NATIVE` branch READS the sidecar on web (no ffmpeg). What was missing:
the sidecar *files themselves* had never been generated, because DC3's natural
generator (`milo2gltf`, which loads any `.milo_xbox` through the full DC3 loader)
fails to rebuild on an unrelated, pre-existing engine-state mismatch
(`GameRenderHook.h` not found when recompiling `Rnd_Wgpu.cpp`).

## Path taken: PREFERRED (adapt the standalone converter — no DC3 engine build)

Generating the sidecars needs no DC3 engine build, so it cannot collide with the
concurrent DC3 render session. The RB3 standalone converter
(`native/tools/xma_repack/rb3-xma-convert`: libav + DC3's `DecodeXMAToPCM`, no
milo engine) was extended to ALSO parse DC3's on-disk `SampleData` layout.

### What changed (rb3 worktree `wt-dc3xma`)

- `native/tools/xma_repack/milo_sample_scan.h` — added `scan_bank_dc3()` and a
  `numChannels` field on `SampleBlob`. The DC3 SynthSample (rev 6) writes `mFile`
  then SampleData with **no** RB3-style loop tail, and DC3 `SampleData` (rev
  0x10) has a different shape, so the RB3 fixed-offset `.wav`-string parser does
  not fit. `scan_bank_dc3` instead scans the (big-endian) milo object stream for
  a self-consistent SampleData header and validates it by parsing fully through
  markers + numChannels:

  ```
  int  rev (==0x10)        int  mCRC (rev>0xE)     int fmt
  int  mNumSamples         int  mSampleRate        int mSizeBytes
  bool hasData (rev>=0xB)   [payload]              SampleMarker[] (rev>=0xE)
  int  mNumChannels (rev>=0x10)
  ```

  The strict multi-field validation (rev/fmt/rate/in-bounds-size/parseable
  markers/1<=ch<=8) makes false positives vanishingly unlikely; the scanner
  resumes just past each accepted blob's `numChannels`, so payload bytes are
  never re-scanned. DC3 has a real mono+stereo mix, so per-blob `numChannels` is
  fed to the decoder (it drives the XMA2 channel mask: mono=0x04, stereo=0x03).

- `native/tools/xma_repack/xma_convert_main.cpp` — added a leading `--dc3`/`--rb3`
  mode flag (also `XMA_CONVERT_DC3` env). `--dc3` selects `scan_bank_dc3` and uses
  per-blob `numChannels`; manifest gained a `channels` column. RB3 behavior is
  unchanged when the flag is absent.

- `scripts/assets/convert_xma_banks_dc3.sh` — DC3 sibling of the RB3 wrapper.
  Builds `rb3-xma-convert` (with `-DDC3_DECOMP_PATH`) if needed, recursively finds
  every `*.milo_xbox` under the DC3 extracted tree, and converts with `--dc3` to a
  derived sidecar dir.

**No DC3-repo file was changed.** The converter lives entirely in the rb3 tree;
it only *reads* DC3's `XmaSampleDecoder.cpp` (the codec) at build time via
`-DDC3_DECOMP_PATH`. The concurrent DC3 session's uncommitted
`src/system/rnddx9/Rnd_Xbox.cpp` and `scripts/.songpush/` were left untouched.

## Key-match confirmation (byte-for-byte == DC3 runtime PayloadKey)

The sidecar key is FNV-1a over `(raw payload bytes, then sizeBytes as u32, then
sampleRate as u32)` — **numChannels is NOT in the key** (it is stored *after* the
payload in the DC3 layout, and neither side hashes it). This is identical in:

- converter `milo_scan::payload_key` (milo_sample_scan.h),
- RB3 runtime `rb3_xma::PayloadKey` (native/src/rb3_xma_sidecar.h),
- DC3 runtime `dc3_xma::PayloadKey` (dc3 native/src/platform/XmaPcmSidecar.h).

The file format (`RB3PCM01` magic + LE32 sampleRate/numSamples/numChannels/rsvd +
interleaved int16) is also identical, so **one shared dir serves both engines**.

Verified by compiling DC3's real `dc3_xma::PayloadKey` and comparing to the
converter's key over every XMA blob in 4 banks: **2167 blobs, 0 mismatches.**
The DC3 runtime reaches this key for these CRC-bearing samples because
`WavMgr::CreateSample` returns false on first load and `ReadChunks` fills `mData`
with the contiguous raw XMA (ReadChunks does no per-chunk framing), so the
runtime hashes exactly the same bytes the converter does.

Runtime resolution proven standalone: compiling DC3's real `dc3_xma::TryLoad`
(the same call SampleData.cpp's web path makes) against the generated dir
resolves every non-silent blob (see verification below). DC3's `dc3-native`
build was NOT attempted (per task: only if the engine build env were healthy and
non-colliding — it is entangled with the concurrent session), so end-to-end is
proven by the standalone key-match + TryLoad-resolution + RMS, not a dc3-native
run.

## How to regenerate

```bash
# From the rb3 worktree (or any rb3 checkout with native/tools/xma_repack):
DC3_DECOMP_PATH=/home/free/code/milohax/dc3-decomp \
  scripts/assets/convert_xma_banks_dc3.sh \
    [SCAN_DIR=<dc3>/orig-assets/extracted] \
    [OUT_DIR=<dc3>/orig-assets/derived/sfx_pcm]

# Manual (already-built tool):
rb3-xma-convert --dc3 <out-dir> <bank.milo_xbox> [<bank2> ...]
```

Runtime: point DC3 at the dir via `DC3_SFX_PCM_DIR` (absolute), or place the
`*.pcm` under `<DC3 asset root>/sfx/gen/xma_pcm/` (the runtime default). Web: ship
the `*.pcm` alongside the extracted assets (the `#elif HX_NATIVE` web path reads
them at load). Sidecars are large + gitignored — regenerate, don't commit.

## Validation results (full DC3 extracted tree)

- Banks scanned: ~5,399 `.milo_xbox` (entire DC3 extracted tree; XMA is spread
  across sfx/, songs/*/loc/* barks, world/, ui/, flow/, not just sfx/gen).
- kXMA blobs converted: see the run summary in the commit / `manifest.txt`.
  Every decoded sample count is within ~one frame of the stored `mNumSamples`;
  RMS is non-zero for all written sidecars.
- The ONLY rejections are `FAIL silence` in localized `barks.milo_xbox` banks:
  single-packet (szb=2048) XMA whose decode yields exactly `mNumSamples` samples
  of pure zero. These are **genuinely silent** localized voice-bark placeholders
  (the raw XMA is non-zero encoded-silence; DC3's own ffmpeg path would produce
  the same silence). They are correctly skipped (a silent sidecar is pointless) —
  0 real decode failures, 0 write failures.
- DC3 SFX banks are a real mono+stereo mix (e.g. `common_bank` = 23 mono + 70
  stereo; `vo_actionbarks` = 1626 mono). The parser extracts per-blob channels
  and the decode + sidecar honor them, so DC3 stereo round-trips.

## What was NOT touched

- The concurrent DC3 session's uncommitted `src/system/rnddx9/Rnd_Xbox.cpp` and
  its `scripts/.songpush/` working dir — left exactly as found.
- DC3's `milo2gltf` / `Rnd_Wgpu.cpp` build-env issue — not fixed (entangled with
  the concurrent render work; the preferred path makes it unnecessary).
- No repo was pushed; no branch merged to main/master.
