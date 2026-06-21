# "25 or 6 to 4" guitar-audio offset — diagnosis (2026-06-21)

**Report:** in "25 or 6 to 4" (Chicago) the **guitar audio is 10+ seconds off** the
chart/gems. User hypotheses: (a) wrong stem loaded, (b) per-stem start offset, (c)
Range-mogg seek offset, (d) deeper sync bug. User note: "some RB3 songs load a
different stem set for single-player."

## TL;DR — root cause

- **Not a wrong stem, not a Range-mogg seek, not song-specific data.** "25 or 6 to 4"
  is a single ordinary 11-channel mogg; guitar = channels 3,4 (stereo). One
  `StandardStream`, one decode cursor, all stems decode sample-locked.
- **Native is byte-perfect:** all 11 stems (incl. guitar 3,4) measure lag **0** /
  Pearson **1.0000** vs an independent offline decode of the same mogg. There is
  **no guitar offset in the native build**. The shared `src/` decode/mix path is
  not the bug.
- **The offset is WEB-ONLY, in the off-main worklet SAB mix path** — the only place
  in the codebase where stems are seeded/advanced **independently** instead of being
  pulled together by one mixer. It landed default-ON 2026-06-21 (engine
  `805e701` / `2aed9e5`), which is why the report is "now."
- **Mechanism (file:line):** each stem's worklet read cursor is seeded
  *independently* to its **own** play-cursor at the moment of its first pump-tick,
  with **no shared song-start anchor** and a **one-shot prime gate that never
  re-aligns a late-joining stem**:
  - seed value = per-stem `mAudioReadPos>>1`:
    `native/src/rb3_stream_receiver_native.cpp:492` (`out->startFrame = readPos >> 1`)
    captured at first publish in `milo-native-engine/src/audio/AudioDevice_Web.cpp:924-931`
    (`js_offmain_seed_stem`, impl `AudioDevice_Web.cpp:473-493`).
  - the worklet mixes each stem at its **own** independent `rp` and advances each
    independently — there is no global play clock:
    `milo-native-engine/src/platform/web/assets/audio-worklet.js:266-303`.
  - the start-up prime gate latches **once** on the min-availability of the
    **currently-seeded** stems and never re-arms when a stem joins later:
    `audio-worklet.js:236-246`.

If any one stem (here, guitar) is seeded in a **later** pump tick than the others —
after the worklet has primed and begun advancing the early stems' read cursors — it
starts from **its** song-start while the bus is already N seconds ahead, so it plays
N seconds behind. ~10s ≈ one stem ring (`kStemRingFrames = 0xC0000/2 = 393216
frames ≈ 8.9s @ 44100`, `AudioDevice_Web.cpp:56`).

## Measured

Native gameplay capture of "25 or 6 to 4" with per-channel decoded-PCM dump
(`RB3_DUMP_STEMS`) + offline per-channel decode of the same mogg
(`scripts/native/audio_stem_verify.py`):

```
  stem     corr     lag        ( /tmp/stemverify_out.txt )
     0   1.0000       0
     1   1.0000       0
     2   1.0000       0
     3   1.0000       0   <- guitar L
     4   1.0000       0   <- guitar R
     5..10 1.0000     0
  median per-stem Pearson : 1.0000   VERDICT: DECODE-CORRECT
```

Whole-mix vs source (`audio_verify.py … --song 25or6to4 --section gameplay`):
`VERDICT: MATCH`, speed `1.000x` (conf 0.90), chroma `0.97`, not clipped. The
`+21.5s` align lag is just the menu/nav lead-in (capture starts at gameplay), with
flat speed → **no drift, no per-stem offset in native.**

Second song cross-check (native, `20thcenturyboy`, 15 channels): all 15 stems
equal-length / sample-locked. Native is **universally** clean — the offset is not
song-specific in native.

**Direction & magnitude (web):** guitar plays **behind** (later than) the chart and
the other stems, by ~one stem-ring (~9–10s). Web-only.

## Why it is NOT the user's guesses

- **(a) Wrong stem / single-player stem set:** No. "25 or 6 to 4" `songs.dta` has one
  `song` block, one mogg, `tracks ((drum (0 1))(bass 2)(guitar (3 4))(vocals (5 6))
  (keys (7 8)))` — 11 channels, guitar is 3,4. `cores (… 1 1 …)` only sets the
  guitar **FXCore** (the miss/crowd FX routing), NOT a separate stream
  (`src/system/beatmatch/MasterAudio.cpp:157-185`). There is exactly ONE
  `mSongStream = TheSynth->NewStream(...)` (`MasterAudio.cpp:129`). No single-player
  stem swap for this song.
- **(c) Range-mogg seek:** `WebRangeFile` is a pure byte-offset `File`; `Seek` sets
  one shared `mPos` and serves byte-identical data — it has no channel concept. The
  decode shares one Vorbis cursor (`VorbisReader`/`StandardStream::ConsumeData`,
  all channels advance by the same `samplesToConsume`,
  `src/system/synth/StandardStream.cpp:421-484`). A Range seek cannot offset one
  channel.
- **(b)/(d) per-stem offset / deeper sync:** Real, but only on the **web** off-main
  SAB path; the shared decode/mix and the native miniaudio mixer are sample-locked.

## Note: the related (different) 20 ms bug already fixed

`d6f56d23` zeroed a separate **20 ms** global audio-leads-video AV-calibration term
(`mSongToTaskMgrMs − mInGameExtraVideoLatency = −20ms`). That is *milliseconds* and
*global* (all tracks); it is **not** this 10-second, guitar-specific, web-only
offset. Don't conflate them.

## Why a single lockstep stream can still desync ONE stem on web (the seam)

In the common path all 11 channels register together in
`StandardStream::Play()`'s `std::for_each(... &StreamReceiver::Play)`
(`src/system/synth/StandardStream.cpp:528`), and the next `PumpAudio()`
(`src/App.cpp:569`) seeds them all in the same tick with the same `mRingReadPos`
(lockstep decode → identical play cursor). In that path web *also* aligns.

The desync requires one stem to be **seeded in a later pump tick** than the
already-primed others. Code seams that allow it (each is the place to add the fix's
guard / instrumentation):

1. **Per-stem arm gate** — seeding is gated per stem on `OffMainArmed()` =
   `mPlayStarted` (`rb3_stream_receiver_native.cpp:458`) and on
   `OffMainActive()` (`:451`), checked independently per slot in the pump loop
   (`AudioDevice_Web.cpp:892-893`). Any per-stem delay in reaching `PlayImpl`
   (decode-pipeline arrival skew, a stem that finished/late-armed, slot-claim
   ordering) seeds it later.
2. **No shared anchor** — the seed uses each stem's own `startFrame`
   (`AudioDevice_Web.cpp:925`), never a single captured song-start.
3. **One-shot prime gate** — once `this.primed = true` latches
   (`audio-worklet.js:245`) it never re-primes for a late joiner; the early stems'
   read cursors are already advancing (`audio-worklet.js:301-302`).
4. **CrowdAudio interaction (new, 2026-06-21, suspected aggravator):** the restored
   silent-intro crowd stream (`src/system/bandobj/CrowdAudio.cpp`, commit
   `2eeb6ced`) is a *separate* `StandardStream` that registers a music-stem slot and
   plays during the count-in window — i.e. stems are already being seeded/advanced
   on the worklet *before* the song's 11 channels arm. This is exactly the
   "worklet already primed and advancing when a new stem joins" precondition. It
   also (harmlessly) inflated the diagnostic stem dump: with crowd ON, `stem_00`
   came out ~2× the size of stems 1–10 because the `StemDumper` singleton is shared
   across streams (tool artifact, not the game bug; with `RB3_NO_CROWD_INTRO=1` all
   11 stems are byte-equal).

## Proposed fix (NOT landed — web/native glue only, Wii-match-neutral)

Anchor every stem to **one** song-start frame and don't start mixing until the full
set is seeded. Minimal, surgical, web-only (`AudioDevice_Web.cpp` /
`audio-worklet.js`, both already non-Wii):

1. **Shared seed anchor.** In `AudioDevice::PumpAudioOffMainStems`
   (`AudioDevice_Web.cpp:884-947`), capture the first-seeded stem's `startFrame`
   into a per-song `mSongSeedAnchor`, and seed **every** subsequent stem to that
   same anchor (`js_offmain_seed_stem(i, mSongSeedAnchor, …)`) instead of its own
   `st.startFrame`. Because all channels of one stream share a play cursor, the
   anchor is the correct song-start for all of them; a CrowdAudio stream (different
   `StandardStream`, different start) should anchor to its own group, so key the
   anchor per-`Stream`/owner, not globally.
   *Simplicity alt:* defer seeding any stem of a stream until **all** of that
   stream's expected stems are `OffMainArmed()`, then seed them in one tick (they
   already share `mRingReadPos`, so they get the same `startFrame` for free). This
   removes the staggered-seed window entirely.

2. **Prime gate counts the expected stem set.** In `audio-worklet.js:236-246`, gate
   `primed` on a known expected stem count (passed from the pump), or simply don't
   latch `primed` until the active mask is stable for one quantum — so the worklet
   never starts the bus clock with a subset.

3. **(Optional hardening)** Late-joining-stem re-alignment: if a stem's bit is set
   *after* `primed`, snap its `rp` to the global bus position rather than its own
   seed (carry the bus's consumed-frame count and offset late joiners by it).

Recommended first step: **(1)-defer-seed** — smallest, removes the race at the
source, and makes (2)/(3) unnecessary for the in-stream case. Keep an opt-out env
(e.g. `RB3_NO_STEM_SEED_ANCHOR=1`) consistent with the codebase's pattern.

### How to verify a fix (web)
Build web (`scripts/web/build.sh --debug`), play "25 or 6 to 4", capture the worklet
output, and run the same `audio_verify.py --song 25or6to4 --section gameplay`. With
the fix, the whole-mix lag is constant (menu lead-in only) and chroma stays high
across the whole window; a per-stem split would show the guitar-band chroma lagging.
Also A/B `RB3_NO_CROWD_INTRO=1` (removing the aggravator) as a fast confirmation
that the crowd-stream early-seed is the trigger.

## Artifacts / tools

- `scripts/native/capture_song_guitar_offset.py` (new) — navigates rb3-native
  headless into gameplay of a song chosen by DOWN-count, captures post-mix WAV +
  per-channel `RB3_DUMP_STEMS` dump. "25 or 6 to 4" = `--downs 3`.
- Reused: `scripts/native/audio_stem_verify.py` (per-stem decode lag vs source),
  `scripts/native/audio_verify.py` (whole-mix identity/rate/offset).
- The `StemDumper` (`src/system/synth/StandardStream.cpp:61-127`) is a shared
  singleton across streams — when capturing with the crowd intro ON, ignore the
  inflated `stem_00`; capture with `RB3_NO_CROWD_INTRO=1` for clean per-stem sizes.
