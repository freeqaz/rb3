# audio-perf-loop — independent cross-check (2026-06-06)

> Additive note from a second orchestrator session. Does NOT modify the canonical
> `STATE.md` / `wave-02.md` (owned by the in-flight wave-02 session). Purpose:
> independently verify the wave-01 audio root cause with a separately-authored
> instrument + fresh captures, and answer "are songs playing correctly / is it
> fixed / does web need fixing / are the tools effective".

## Instrument used (independent)
`scripts/native/audio_coherence.py` — reference-FREE clip/overflow-wrap/noise/
dynamics classifier (authored this session, before reading the other session's
`audio_correlate.py`). New vs their toolset: an **overflow-wrap / rail-to-rail
harshness** metric (adjacent samples crossing near +full↔−full) that directly
fingerprints "clipped noise".

## Captures (fresh, my own, default `orig-assets/extracted` song)
- preview:        `scripts/native/song-preview-audio-test.py` → /tmp/rb3_native_preview_w1.wav
- gameplay @1.1×: `scripts/native/capture_gameplay_audio.py` (ship default)
- gameplay @0.30: same, `--gain 0.30` (DC3_AUDIO_GAIN)

## Result — confirms wave-01 (HIGH confidence, two independent instruments agree)
| capture | verdict | clip_ratio | overflow_wraps/s | crest dB | pitch | peak |
|---|---|---|---|---|---|---|
| native preview      | COHERENT | 0.0007 | 0.0  | 19.2 | 0.189 | 32767 |
| native gameplay 1.1×| **DEGRADED** | 0.0054 | **31.3** | 13.7 | 0.167 | 32767 |
| native gameplay 0.30| COHERENT | 0.0003 | 0.0  | 16.1 | **0.592** | 32767 |

JSON artifacts in `baselines/w1-native-{preview,song-default,song-gain030}.json`.

## Findings
1. **Native songs are NOT playing correctly at the shipped default** — gameplay
   output is clipped/harsh (876 rail-to-rail transitions in 28s = the audible
   "clipped noise"; crest collapses 19→14 dB). The **preview is fine** (single
   VorbisReader stream, no multitrack sum). Confirms wave-01 H1.
2. **The fix is NOT landed.** `AudioDevice.cpp:35 sMasterGain = 1.1f` is still the
   default; only the `DC3_AUDIO_GAIN` env knob exists. `DC3_AUDIO_GAIN=0.30`
   flips gameplay to COHERENT (clip→0.03%, wraps→0, pitch 0.167→0.592). The
   permanent fix *shape* is the open wave-02 question (flat gain vs 1/√N vs
   soft limiter) — left to the in-flight wave-02 session; not landed here to
   avoid colliding on the shared engine (DC3 risk per wave-02 W2-B).
3. **Web needs fixing too — same root cause.** `AudioDevice_Web.cpp:412-413`
   hard-clamps the summed mix to [−1,1] with NO master gain before it (native at
   least has `*=sMasterGain` at :378). Web clamps the raw ~3.2× sum → clips at
   least as hard. Confirmed by code; prior memory recorded web song SAB peak
   32767. (Live web A/B not run this session — fast follow if wanted.)
4. **`peak` is a useless quality metric here** — 32767 on clean preview, clipped
   gameplay, AND the fixed 0.30 capture alike. The legacy audibility tests
   (`peak>1500 & nonZero>10%`) PASS all three. Coherence needs crest_db +
   clip_ratio + overflow_wraps, which separate them cleanly.

## Tool effectiveness verdict
**Effective.** Both instruments localize the bug to exact engine lines, the
`DC3_AUDIO_GAIN` knob gives a live fix-preview, the gameplay capture harness
reliably reaches real multitrack gameplay headless, and the encryption blocker
was solved (`decrypt_mogg.py`). Limits surfaced: (a) reference waveform-xcorr is
unreliable on clipped input (phase destroyed) — use spectrogram-shape corr;
(b) the **clean post-fix divergence-vs-reference number is still open** — capture
once the permanent fix lands and report corr-vs-decrypted-reference; (c) peak/
audibility tests must be retired in favor of coherence metrics.
