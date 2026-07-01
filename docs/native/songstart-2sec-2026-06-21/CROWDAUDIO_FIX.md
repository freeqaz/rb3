# Restoring the silent song-intro window — CrowdAudio venue/crowd audio FIX (2026-06-21)

**Worktree:** rb3 `wt-crowdaud-intro179` (branched from clean `179fcabb`, NOT from the
diag branch `ecc18786`). Paired engine worktree
`milo-native-engine-worktrees/crowdaud-intro179` (UNTOUCHED, pin `b8f3cfa`).
No engine change, no `MILO_ENGINE_PIN` bump, no push.

## TL;DR

The multi-second silent song-start window the user reported ("2+ second audio lag /
no audio during the intro") is the chart's faithful negative-clock count-in (the
song stems correctly start at clock 0 — see `NATIVE_MEASUREMENT.md`). The REAL bug
is that during that intro window the native port plays **no crowd / venue-intro
audio at all** — the venue cinematic rolls silently, then the song snaps in.

Per `CROWDAUDIO_DIAGNOSIS.md`, the gap is an **asset gap, not a wiring gap**:
`CrowdAudio::OnIntro` fires and calls `PlayLoop("crowd_intro.mogg")`, but
`mBank->Find<BinkClip>("crowd_intro.mogg")` returns nil because the Xbox-360 venue
milo the native port loads has **zero `BinkClip` objects**, and the crowd stream
payloads (`world/venue/<class>/streams/crowd_*.mogg`) aren't in the extracted tree.

This fix **restores the intro crowd audio**, native-first, match-neutral.

### Before / after (anchored on the song-stem onset, native capture)

| | 6s INTRO window before the song stems | song stems |
|---|---|---|
| **BEFORE** | **100% silent** (mean RMS 0.00000) | start at clock 0, full level (~0.19 RMS) |
| **AFTER**  | **0% silent** (mean RMS ~0.032, max ~0.07) | UNCHANGED — start at clock 0, full level |

The intro-window audio is confirmed to be the crowd intro (not noise/leaked music):
its spectral centroid matches the source crowd stream (2350 Hz vs 2318 Hz) and the
amplitude envelope cross-correlates at **0.93** against the decoded source crowd
intro. The song stems are untouched (no song-clock / gem-timing change).

## The fix — two parts

### 1. Asset: transcode the Wii crowd intros → unencrypted `.mogg`
`scripts/native/transcode_crowd_audio.py` decodes the three Wii
`crowd_<class>_intro.bik` Bink-audio streams (the ONLY crowd stream payloads that
exist in any extracted asset tree) and wraps the Ogg/Vorbis in a minimal
**version-10 (unencrypted) little-endian** RB3 mogg header:

```
u32 version=10            ; CheckHmxHeader skips ALL crypto for v10 (mCtrState=null)
u32 hdrSize=28            ; offset to the Ogg data
u32 oggmapVersion=0x0B    ; OggMap::Read
u32 gran=1000
u32 lookupCount=1 ; (0,0)
<raw Ogg/Vorbis @ offset 28>
```

Output (gitignored derived asset; regenerated from a fresh checkout):
`orig-assets/extracted/world/venue/{small_club,big_club,arena}/streams/crowd_<short>_intro.mogg`

`ffmpeg` decodes `BIKi` (binkaudio_dct, 44.1kHz mono) → `libvorbis` cleanly.
The native `VorbisReader` plays a v10 mogg with no decryption (`::Decrypt` is a
no-op when `mCtrState==null && magicHash==0`).

> NOTE on the asset path: in this worktree `orig-assets/extracted` is a SYMLINK to
> the main repo's tree, so the generated moggs are shared by all instances. They
> are NOT committed (gitignored); the transcode script reproduces them.

### 2. Code: native BinkClip-synthesis bridge (HX_NATIVE, match-neutral)
`src/system/bandobj/CrowdAudio.cpp` — a file-local `NativeSynthCrowdIntroClip()`
helper + one call site in `PlayLoop`, **both fully `#ifdef HX_NATIVE`**:

- When `Find<BinkClip>(clipname)` misses and `clipname` is the (venue) intro loop,
  derive the venue class from `mBank->GetPathName()` (`world/venue/<class>/...`),
  `bank->New<BinkClip>(clipname)` (registers it in the bank so subsequent `Find`
  resolves it), and `SetFile("world/venue/<class>/streams/crowd_<short>_intro.bik")`
  + `SetLoop(true)`.
- `BinkClip::Play` strips the trailing `.bik` and `Synth::NewStreamFile` re-appends
  `.mogg`, so the file actually streamed is the transcoded
  `crowd_<short>_intro.mogg` — through the already-live native `NewStream` →
  `StandardStream` → `VorbisReader` path. No engine/route change needed.
- Only the intro is bridged; the excitement-level loops (norm/good/peak/...) have no
  extracted source assets and fall through unchanged (PlayLoop returns false, the
  intro loop keeps playing as crowd ambience through gameplay at -3dB crowd_volume).
- Opt-out: `RB3_NO_CROWD_INTRO=1`.

## Match-neutrality (Wii byte-identical) — VERIFIED
Every added line in `CrowdAudio.cpp` is inside `#ifdef HX_NATIVE`. objdiff vs the
Bank-8 target after the change:
- `CrowdAudio::PlayLoop` — **100.0%** (raw/normalized/fuzzy, diff_score 0)
- `CrowdAudio::OnIntro` — **100.0%**
- `CrowdAudio::CrowdAudio` (ctor) — **100.0%**

The new `.mogg` assets + the two scripts are pure data/tooling, zero match impact.

## Reproduce / verify
```bash
# (1) produce the crowd-intro moggs (idempotent; --force to re-encode)
scripts/native/transcode_crowd_audio.py
# (2) build native
cmake --build native/build-native --target rb3-native -j"$(nproc)"
# (3) capture the whole song-start window + per-second RMS (anchored intro analysis)
python3 scripts/native/capture_intro_crowd_audio.py /tmp/crowd_after.wav --secs 40 --port 8782
#   -> the 6s window before the song-stem onset is now NON-silent (crowd intro)
# functional proof the bridge fires (env-gated is removed in the landed code; use
# RB3_CROWD_DBG only on the diag branch): the boot log shows the synthesized clip's
# stream open of world/venue/<class>/streams/crowd_<short>_intro.mogg
```

## Scope notes / follow-ups
- **Intro only.** The excitement-level crowd loops (danger/poor/norm/good/peak) and
  `venue_intro`/`venue_outro` have no extracted stream payloads in any tree, so
  they remain silent — that needs an ARK re-extract of the 360 crowd streams (or a
  Bink-audio decoder), out of scope here. The intro is the window the user reported.
- The crowd intro is a 15.76s loop; it loops through the count-in and continues as
  low-level ambience once the song starts (correct behavior, additive, -3dB).
- Web: the bridge is in shared `src/` under HX_NATIVE (compiled for web too) and the
  stream path is the same VorbisReader, so this carries to web once the crowd moggs
  are in the web asset bundle. Native-first per the task; web bundle wiring is a
  follow-up (server.py fallback root already serves extracted sidecars).
