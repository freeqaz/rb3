# Session 2026-06-03 — Song MOGG audio silence FIXED (native + web)

User report: SFX work but **songs are silent** on web — no song audio during
gameplay, song previews dead. Diagnosed + fixed: rb3 `10af87ab` (bridge-only, one
file, no engine change). The bug was **shared native+web**, not web-specific.

## Root cause — a producer/consumer deadlock in the native StreamReceiver bridge

`native/src/rb3_stream_receiver_native.cpp` (the RB3-shaped glue bridging the
matched-fork `StreamReceiver` onto the engine's `AudioDevice` mixer). The song
MOGG decoded perfectly (15-channel multitrack, decrypt + Vorbis + buffering →
kReady; `Game::Go` fires; `MasterAudio::Play` called; all 15 channels registered;
`streamPlaying=1`; volumes/pans correct) — yet `RenderAudio` output exactly `0.0`.

Two coupled blockers:
1. **Chicken-and-egg deadlock.** `RenderAudio` only rendered when `mSendActive` was
   true; `mSendActive` is raised only by a producer `StartSendImpl`; during kPlaying
   the base `Poll` only sends when `activeBuf != mSendTarget`, where `activeBuf =
   GetPlayCursor()/0xC000` and `GetPlayCursor()` returns the bridge's
   `mAudioReadPos`. At kPlaying start `mAudioReadPos=0 → activeBuf=0 == mSendTarget=0`
   → no send → `mSendActive` stays false → `RenderAudio` zero-fills → cursor never
   advances → permanent silence. On Wii the **hardware DSP** advances the play cursor
   continuously through the kInit-pre-filled ring, which is what triggers the
   refill-sends; the native bridge gated the consumer on an explicit send instead.
2. **Slip gate.** The song channels report `slip=1`, and base `Poll` gates its
   refill-send on `!mSlipEnabled` — so even past the deadlock the channels would
   never request a send (the ring just loops its first ~1.1s). V1 implements no
   slipstream, so the bridge forces `mSlipEnabled=false`.

## Fix (bridge-only; base StreamReceiver / StandardStream / SampleInst untouched)
- `RenderAudio` plays the buffered ring **continuously** like the DSP, ungated by
  `mSendActive`. Available-to-play derived from `mRingWrittenSpace` minus what the
  play cursor consumed past `mRingReadPos` (NOT `writePos−readPos`, which reads 0
  when the ring is full at the kInit→kPlaying handoff — that subtlety re-creates the
  silence if missed). The advancing `mAudioReadPos` (via `GetPlayCursor`) drives the
  base's cursor-based refill-send loop — exactly what the Wii cursor did.
- Back-pressure re-keyed off the play cursor: `SendDoneImpl` releases a chunk only
  once the consumer has played `mSendSize` more bytes (monotonic `mPlayedTotal`
  counter), so `WriteData` refills behind the play cursor (no overrun). Non-blocking
  (`SendDoneImpl` returns false and lets `Poll` revisit — never spins), so web's
  same-thread `PumpAudio` can't deadlock.
- `mAudioReadPos`/`mPlayedTotal` are atomics (acquire/release) for the native audio
  thread; producer-owned ints are racy-benign + clamped. Pause/IsFinished preserved.

## Verification (the unambiguous metric — song RMS)
Null-backend WAV (`MILO_AUDIO=1 MILO_AUDIO_BACKEND=null DC3_DUMP_AUDIO=…`), raw
int16/stereo/44100 RMS over 3s windows. **Before:** menu SFX 0–9s, then exactly
`0.0` for the whole song. **After (main-loop-adjudicated capture):** SFX 0–9s
intact; song region (≥20s) **8/8 windows loud, RMS ~5300–8600, sustained through
45s**; the only silence is the pre-song boot/load gap (songMs=0), not a dropout.
Independent re-verify (separate agent capture) + concurrency review (SAFE native +
web) both PASS.

## Process / lesson
Gated ultracode: **diagnose → (probe-correct) → implement → validate → land.** The
diagnosis workflow's confident root cause was **wrong** ("`Play()` never fires /
panel never Active") — a runtime probe refuted it (`Game::Go` *does* fire,
`streamPlaying=1`, volumes fine) and a `RenderAudio` probe nailed the real bug
(`sendActive=0` always → 0 frames). **Probe-first beat the workflow's static
reasoning** (and the prior "B1 audibility PROVEN" memory was a `sMixBuffer`
capture-hook artifact, not real output — the song was silent in actual playback).
The implementation agent then found the *second* blocker (slip gate) that static
analysis missed, iterating against the RMS metric. The clear ground-truth metric
(song RMS) made the agent loop reliable.

## Open follow-ups
- **Confirm once in-browser** (web PumpAudio path argued correct by construction,
  not run in a real browser this session) — standard CLAUDE.md web-confirm step.
- The back-pressure correctness rests on the 2-chunk-ring `0xC000` lockstep; a
  future change to chunk size / `mNumBuffers` / slip-enable / callback granularity
  must preserve it (the `available<0` clamp guards it).
- Any future slipstream/beat-shift feature must implement the slip cursor + restore
  slip-gated sends.
