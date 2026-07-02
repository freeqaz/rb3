# Song-start "audio a few seconds ahead of the notes" — ROOT CAUSE (2026-07-01)

> **FIXED 2026-07-02** — pre-Play decode cap + first-Play ring resurrect.
> See "RESOLUTION" at the bottom of this file. The fix-attempt post-mortem and
> root cause below are kept for history.

## Symptom (user report)
On the **web build**, when a song loads, the note highway starts at song 0 but the
**audio you first hear is already several seconds into the song** — the first few
seconds are skipped. "Makes the game unplayable." Systematic, every song.

## Why every prior pass missed it
The 2026-06-21 passes (AV-cal `d6f56d23`, INTROSYNC, songstart-2sec) all concluded
the song-start sync was *correct*. They were measuring the wrong thing:

- **The game clock is correct.** `GetAudio()->GetTime()` (= `/api/health songMs`)
  resets to 0 at Play and advances normally. `songMs ≈ audioTime` every frame.
- **The audio amplitude onset is correct.** The loud song audio starts right at
  `Go()`/clock 0.
- **But the audio CONTENT is ~9 s into the song at clock 0.** Neither the clock nor
  an amplitude/RMS probe can see a *content* offset — only cross-correlating the
  actual worklet output against an independent decode of the song reveals it.

## Reproduction + measurement (the key enabler)
`scripts/web/_audio_early_probe.mjs` (Playwright) drives the web build to gameplay,
taps the **AudioWorklet output** to a WAV, and records the `Go()`/clock-0 offset.
Then:

```bash
# 1. capture worklet output, note the Go()/clock=0 offset into the WAV
cd scripts/web && node _audio_early_probe.mjs --port 8455 --song antibodies \
    --secs 16 --wav /tmp/cap.wav       # prints "Go()/clock=0 is at +NNNNms"
# 2. trim the WAV to start exactly at clock 0
ffmpeg -y -i /tmp/cap.wav -ss <NN.NNN> /tmp/cap_pg.wav
# 3. cross-correlate against an independent decode of the song (from song 0)
python3 scripts/native/audio_verify.py /tmp/cap_pg.wav --song <id> --section full
```

Result (multiple songs, off-main ON):
```
align lag / peak:  +9.1s / 0.7-0.9   chroma 0.85-0.94 (same song)  speed 1.00x
```
**At clock 0 the audio content is +9.1 s into the song.** (=~ the ring depth; see
below.) With `RB3_WEB_OFFMAIN_MIX=0` the number is different/garbage — this is an
**off-main-path** bug. Native headless can't show it (no audio device to capture),
which is why native looked clean.

## Root cause (confirmed in source + measured)
The streaming ring is deep (~9.1 s, 16×0xC000 — deepened for gameplay stall
resilience, `StreamReceiver.cpp` ctor + `StandardStream::Init`). During the entire
**pre-Play window** (song load + the ~6-7 s intro / count-in) the Vorbis decoder is
polled every frame (`StandardStream::PollStream` → `mRdr->Poll` → `ConsumeData` →
`WriteData`). The decoder runs *faster than real-time*.

The Wii relied on ring **back-pressure** to stall the decoder at the song start
until playback begins. The native/web bridge **defeats that**:
`RB3StreamReceiverNative::SendDoneImpl` **fast-accepts** (frees) every ring chunk
before Play() — a hack added so `IsReady()` flips and the loader doesn't hang. With
chunks freed as fast as they're written, the ring never applies back-pressure and
the decoder **races seconds ahead** through the count-in.

Measured decode position at the song stream's first `Play()`:
```
STREAM_DBG: Play() ENTER mState=2(kReady) mCurrentSamp=761856 (=17275.6ms) chans=11
```
The decoder is **17.3 s** into the song at Play. The 9.1 s ring holds the most-recent
9.1 s (`song[~8s..17s]`); `PlayImpl` sets the play cursor to `mRingReadPos` (the
ring's oldest resident ≈ `song[8-9s]`) and the off-main worklet is seeded there,
while the clock resets to 0. → audio ≈ +9 s vs the notes.

**File map:**
- `native/src/rb3_stream_receiver_native.cpp` — `SendDoneImpl` pre-Play fast-accept
  (the missing back-pressure); `PlayImpl` snapshots `mAudioReadPos = mRingReadPos`.
- `src/system/synth/StandardStream.cpp` — `PollStream` (`mRdr->Poll` every frame,
  ungated pre-Play); `ConsumeData` advances `mCurrentSamp`.
- `src/system/synth/StreamReceiver.cpp` — the ring + send loop; `kInit→kReady` needs
  freed sends.
- `milo-native-engine/src/audio/AudioDevice_Web.cpp` — off-main seed
  (`OffMainSnapshot.startFrame = mAudioReadPos`; `PublishOffMainStem`).

## Earlier fix attempts (2026-07-01) — superseded
1. **Back-pressure (never free a chunk pre-Play) + promote `kInit→kReady` on
   ring-fill/one-chunk (HX_NATIVE).** Correctly caps the decode (verified
   `mCurrentSamp` 17.3 s → 8.9 s), but **destabilizes the count-in** (crash / OOM-class
   trap): holding all chunks fills every one of the 14 song-channel rings to the full
   ~9 s (~11 MB) and/or hits an off-main seed edge case. Reverted.
2. **Intro-hold (skip `mRdr->Poll` while StandardStream `mState==kReady`).** Wrong
   phase — the race is during `kBuffering`, before `kReady`. Reverted.

## THE FIX — LANDED + VERIFIED (2026-07-02, commit `5c5eb8a8`)
"Pause the decoder until the song starts", implemented as **cap + resurrect** — no
re-seek, no re-decode, no extra allocation (so no OOM like attempt 1):

- **`StreamReceiver` (cpp/h): pre-Play total-decode cap.** Track a monotonic
  `mTotalWrittenEver`; while `mState <= kReady` (pre-Play), `BytesWriteable()` caps
  the *total* accepted decode at **one ring lap**. The kInit fast-accept prime is
  unchanged (still frees `mNumBuffers` sends so `IsReady()` flips), but the decoder
  can never write a second lap — so `song[0..ringSecs]` stays **physically resident**
  in `mBuffer`. Past the cap the decoder just idles on 0-writable (the normal
  steady-play ring-full state). Opt-out: `RB3_STREAM_PREPLAY_CAP_OFF=1`.
- **`RB3StreamReceiverNative::PlayImpl`: first-Play ring resurrect.** The capped
  pre-Play end state is deterministic (`totalWritten == ringSize`,
  `writtenSpace == 0`, both cursors wrapped to 0). Resurrect the freed-but-resident
  lap (`readPos=0`, `writtenSpace=ringSize`, `freeSpace=0`) so playback + the
  off-main seed begin at **stream byte 0** instead of the ring's oldest survivor.
- HX_NATIVE-only; the Wii-visible functions (ctor / `WriteData` / `BytesWriteable`)
  stay byte-identical (match-neutral).

**Verification (A/B via the correlation pipeline, the check every prior pass lacked):**
The gameplay stem-mix does not chroma-lock against the full-mogg reference (both
fixed and buggy captures return WRONG-SIGNAL from `audio_verify.py` — a *tooling*
limitation, not a result). Two mix-robust measures settle it, run with the fix ON
(default) vs OFF (`RB3_STREAM_PREPLAY_CAP_OFF=1`), antibodies:

- **RMS loudness-contour vs reference (absolute):** fixed capture locks at
  **lag 0.00 s, r 0.66** (clean monotonic falloff) → starts at `song[~0]`. The buggy
  capture is loud-flat from t0 (mid-song), no contour lock (r 0.30).
- **Chroma capture-vs-capture (same mix, relative):** buggy content **leads the fixed
  content by 8.82 s** (sharp cluster 8.6–9.1 s) → the fix pulls the start ~9 s earlier.
- Raw RMS/s confirms it: fixed = quiet-intro ramp matching the reference; buggy =
  already loud at t0. Chroma machinery validated (reference self-lock cos 0.995).
- Reaches `game_screen` cleanly: **no page crash, 0 rAF stalls >250 ms**,
  `songMs ≈ audioTime`, `streamPlaying=1` at `Go()`.

Net: the +9.1 s content skip is **eliminated** (→ ~0, within count-in tolerance).
Repro/measure tool: `scripts/web/_audio_early_probe.mjs` (taps worklet output to
WAV + records the `Go()`/clock-0 offset); correlate with `scripts/native/audio_verify.py`
or a capture-vs-capture chroma scan (mix-robust; the reference path is not).

## Gotchas for the next pass
- Two different `State` enums: `StreamReceiver` = {kInit0, kReady1, kPlaying2};
  `StandardStream` = {kInit0, kBuffering1, kReady2, kPlaying3}. Don't cross them.
- The `STREAM_DBG state X->Y` log uses a single shared static across all streams, so
  "2->3, 3->2" flip-flop is two different streams being polled, not one toggling.
- CrowdAudio (`crowd_intro.mogg` missing) is a separate, silent stream; don't
  conflate its behavior with the song stream.

## RESOLUTION (2026-07-02) — pre-Play decode cap + first-Play ring resurrect

Landed fix (native + web, HX_NATIVE-only, Wii match-neutral — ctor/WriteData/
BytesWriteable all still 100%):

1. **Pre-Play decode cap** (`StreamReceiver.{h,cpp}`): track `mTotalWrittenEver`
   in `WriteData()`; while `mState <= kReady` (pre-first-Play only — the state
   never returns to kReady), `BytesWriteable()` caps TOTAL accepted decode at one
   ring lap (`mRingSize`). The kInit fast-accept prime is completely unchanged
   (state flips at the same 16th freed send as before), but the refunded
   `mRingFreeSpace` can no longer admit a second decode lap — the ring physically
   retains `song[0..ringSecs]` through the count-in. The decoder idles on
   0-writable, the same state it hits every steady-play ring-full frame.
   Opt-out: `RB3_STREAM_PREPLAY_CAP_OFF=1`.

2. **First-Play ring resurrect** (`rb3_stream_receiver_native.cpp::PlayImpl`):
   the pre-Play end state is deterministic (`mTotalWrittenEver == mRingSize`,
   `mRingWrittenSpace == 0`, both cursors wrapped to 0). Restore the bookkeeping
   of the freed-but-still-resident lap: `readPos=0, writtenSpace=ringSize,
   freeSpace=0`. Post-Play dynamics are then bit-identical to the old steady
   state — only the CONTENT is now `song[0..]` instead of `song[ringSecs..2×]`.
   Guarded on the exact deterministic state (+ `!mEndData`, `!mSending`), so
   short/ended streams and the cap-off escape hatch keep legacy behavior.

### Verification
- **Byte-proof (strongest)**: `RB3_STREAM_AUDIO_DBG=1` logs a RINGHEAD
  fingerprint at PlayImpl; vs an independent decode of the decrypted mogg
  (`decode_reference.py <id> --keep-ogg`), the ring at the play cursor ==
  mogg sample 0 per channel within ±1 LSB (20thcenturyboy, all 15 channels;
  ch=14 exact: 224,510,999,453).
- All 15 channels log `resurrected full pre-Play ring` on both native and web
  (both off-main 16-chunk and `RB3_WEB_OFFMAIN_MIX=0` 8-chunk rings).
- Web A/B: off-main capture vs main-thread-mix capture cross-correlate at
  **lag 0.00s** (r=0.42 envelope peak); om0 locks vs the reference at ~+1.1s
  (±0.5s Go-anchor slop) — down from the +9.1s bug.
- No load hang (IsReady flips at the same time as before), no underruns
  (512/512 frames, ~7-8s decode-ahead cushion in steady play), rb3-tests 51/51.

### Measurement gotchas discovered on the way (for the NEXT audio pass)
- `decode_reference` writes FLOAT WAVs with peak > 1 (no clamp). A naive
  "peak>2 ⇒ /32768" int16 heuristic reads them as silence — several hours of
  bogus "silent reference / +182s lag" results came from that. Use the dtype.
- `audio_verify --section full` envelope alignment is unreliable for a capture
  anchored at the QUIET intro of a self-similar song (Du Hast, 20thcenturyboy):
  weak 0.3-ish locks at meaningless lags. It worked pre-fix because the buggy
  +9.1s content started mid-song (dense onsets). Post-fix, verify with either
  the byte-proof above or band-limited (200-2000 Hz) log-envelope sweeps.
- Raw sample-phase xcorr vs the reference is dead on web output (per-chunk
  resampler jitter destroys 30s-window phase coherence). Envelopes only.
