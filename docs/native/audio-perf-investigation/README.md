# Audio-correctness + asset-stutter investigation

**Goal (user, 2026-06-06):** the web/native build's song audio "sounds like clipped
noise." Build a rigorous way to **measure how far the game's audio diverges from the
expected (reference) decode** — not just "is it noise?" — then fix it. Separately,
**profile the asset-load stutters** and identify their cause.

This folder is the **handoff hub** between orchestration waves. Each wave reads the
prior wave's docs here and appends its own. The orchestrator (main loop) reads
`phaseN-FINDINGS.md` to plan the next phase.

## Why the earlier "real music, not noise" proof was insufficient
A **clipped / wrongly-scaled copy of the real song is still reproducible and
spectrally non-flat** — so reproducibility-corr (0.61) and SFM (0.58) PASS even when
the output is badly distorted. The correct test is **correlation against an
independent reference decode** + clip-ratio + crest-factor + spectral divergence.

## Leading hypotheses (pre-investigation, to confirm/refute with data)
- **H1 — over-unity sum + hard clamp (gain staging).** `AudioDevice_Web.cpp::MixSources`
  sums ~15 song channels additively at unity then clamps to [-1,1]. If per-channel
  `vols[]`/`pans[]` aren't applied (or applied wrong), the sum saturates → clipped
  distortion that still carries the song envelope. Native gameplay capture already
  peaks at *exactly* 32767 (full-scale clip); preview at 27879.
- **H2 — scale/format mismatch.** If `VorbisReader::RenderAudio` emits int16-range
  floats (±32768) instead of normalized [-1,1], every sample clamps to ±1 → square-ish
  full-scale "clipped noise" correlated to the music.
- **H3 — sample-rate mismatch (web).** `AudioContext({sampleRate})` is often ignored/
  clamped by browsers to the device rate (48k); ring filled at source rate, consumed
  at device rate → pitch shift + ring drift glitches.
- **H4 — channel/stride/pan-law downmix error.** wrong interleave or all channels to
  both L+R at unity → mono-summed clipping.
- **HP — asset stutter:** synchronous main-thread asset I/O (ContentMgr mount, LoadMgr,
  texture/GPU upload, mogg open) blocks the frame. (cf. web-loadperf-findings: boot
  bottleneck is sync MEMFS I/O.)

## Key code surfaces
- `../milo-native-engine/src/audio/AudioDevice_Web.cpp` — web `MixSources` (clamp at
  L410-414), `PumpAudio` greedy fill, SAB ring, capture buffer.
- `../milo-native-engine/src/audio/AudioDevice.cpp` — native (miniaudio) mix path.
- `../milo-native-engine/src/platform/web/assets/audio-worklet.js` — SAB consumer.
- `src/system/synth/VorbisReader.cpp` — HMX mogg AES-CTR decrypt + vorbis decode;
  per-channel `RenderAudio` (normalization lives here). DoRawSeek endianness fix landed.
- `src/system/meta/SongPreview.cpp` / `src/band3/meta_band/MusicLibrary.cpp` — preview.
- pan/vol per song: `orig-assets/extracted/songs/<id>/<id>.mogg` (channels) +
  song metadata (`.moggsong`/songs.dta) for `pans`/`vols`/track channel map.

## Reference / capture how-to
- Native capture (null backend WAV, real-time pace, auto-finalize):
  `MILO_AUDIO=1 MILO_AUDIO_BACKEND=null DC3_DUMP_AUDIO=<path> DC3_DUMP_SECONDS=N`
  (+ `RB3_HTTP=1 RB3_HTTP_PORT=P` for nav over `/api/input`, `/api/health`).
- Web capture: `scripts/web/web-song-preview-audio.mjs` (`rb3CaptureAudio()` → WAV);
  SAB-ring nonZero is the reliable output proof, not the capture buffer.
- ffmpeg present (decode multichannel ogg). numpy+scipy present; soundfile NOT (use
  ffmpeg→wav or `wave` module).
- **DEBUG NATIVE, NOT WEB** — shared `src/`+engine, native rebuilds ~3s
  (`cmake --build native/build-native -j$(nproc)`), web build is minutes. The
  clamp/decode/mix code is shared, so the clip bug reproduces in native.

## Constraints
- Concurrent agents have unrelated char-skinning edits in `src/system/char/*`,
  `BandCharacter.cpp`, `Geo.cpp`, `Mesh.cpp`, `src/system/rndobj/Mesh.cpp` — **do not
  touch those files.** Audio work touches `src/App.cpp`, `src/system/synth/*`,
  `../milo-native-engine/src/audio/*`, `scripts/*`.
- No `git stash/revert/checkout/restore` in the main repo. Small surgical edits only;
  use `tools/setup-worktree.sh` for larger isolated changes.
- HX_NATIVE-gate any engine/src change so the Wii decomp match stays intact.

## Phase log
- **Phase 1 (diagnose + instrument)** — IN PROGRESS. Outputs: `phase1-audiopath.md`,
  `phase1-reference.md`, `phase1-metric.md`, `phase1-perf.md`, `phase1-FINDINGS.md`.
