# Phase 1 — Independent reference decode (TASK A2)

**Goal:** produce a ground-truth stereo decode of the default song that does **not**
reuse the game's decode/mix (those are the clipped-noise suspects), so the next wave
can correlate the game's captured output against an independent reference and measure
divergence (clip-ratio, crest factor, correlation), not just "is it noise?".

Status: **DONE.** Standalone decrypt (no engine link) validated byte-for-byte against
the native `MOGG_DBG` log; full 236s stream decodes cleanly. Reference WAVs written.

---

## Default song = `20thcenturyboy` (both gameplay AND preview)

The headless harness lands on the **first** song in the (title-sorted) Music Library,
which is **`20thcenturyboy`** — "20th Century Boy", internal `song_id 1011`. Digits
sort before letters, so it is index 0.

- **Preview:** `scripts/native/_capture_preview.sh` boots to song_select and (in the
  headless build) the highlighted node stays at index 0; the log shows
  `Preview: Requesting 20thcenturyboy` / `Preview: Preparing 20thcenturyboy`. The
  preview window is the song's chorus: `(preview 78204 108204)` ms.
- **Gameplay:** `song-end-test.py` / `song-select-capture.py` nav does
  `@320:down, @350:select_highlighted_node`; the `down` does not move the highlight in
  the headless build (verified `{$music_library highlighted_node} => 0` at depths
  0/1/2), so gameplay also confirms on node 0 = `20thcenturyboy`. (If a future harness
  change makes `down` move the selection, re-confirm with the live log line
  `Preview: Preparing <id>` or `MOGG_DBG: setupCypher`.)

mogg path: `orig-assets/extracted/songs/20thcenturyboy/20thcenturyboy.mogg`
(symlink → `orig-assets/extracted-xbox-full/...`, real size **37,350,466** bytes).

---

## Metadata source for pans/vols — `orig-assets/extracted/songs/songs.dta`

The `(20thcenturyboy ... (song (pans ...) (vols ...) (tracks ...)))` block. 15 audio
channels:

| ch | track   | pan   | vol (dB) |
|----|---------|-------|----------|
| 0  | drum    | -1.0  | -4.5 |
| 1  | drum    | +1.0  | -4.5 |
| 2  | drum    | -1.0  | -4.7 |
| 3  | drum    | +1.0  | -4.7 |
| 4  | drum    | -1.0  | -4.5 |
| 5  | drum    | +1.0  | -4.5 |
| 6  | bass    |  0.0  | -1.5 |
| 7  | guitar  | -1.0  | -4.5 |
| 8  | guitar  | +1.0  | -4.5 |
| 9  | vocals  | -1.0  | -4.3 |
| 10 | vocals  | +1.0  | -4.3 |
| 11 | keys    | -1.0  | -4.5 |
| 12 | keys    | +1.0  | -4.5 |
| 13 | keys?   | -1.0  | -4.5 |
| 14 | keys?   | +1.0  | -4.5 |

(`tracks`: drum 0-5, bass 6, guitar 7-8, vocals 9-10, keys 11-12; channels 13-14 are
the extra stereo keys/backing stems present in the 15-ch mogg.)

---

## Standalone decryptor — `scripts/native/decrypt_mogg.py`  (NO engine link)

A faithful, line-for-line Python port of the **native (`HX_NATIVE`)** decrypt path:

| stage | ported from |
|-------|-------------|
| header parse | `src/system/synth/VorbisReader.cpp::CheckHmxHeader` + `OggMap::Read` |
| key reveal (`getMasher`/`getKey`) | `native/src/rb3_keychain_native.cpp` |
| `GrindArray` / `magicNumberGeneratorNative` | `src/system/synth/ByteGrinder.cpp` (HX_NATIVE branch) |
| `HvDecrypt` (AES-ECB, `gHvKeyGreen`) | `src/system/synth/ByteGrinder.cpp` |
| AES-CTR + HMXA→OggS reversal | `src/system/synth/VorbisReader.cpp::Decrypt` (HX_NATIVE) |

Crypto deps: `pycryptodome` (`Crypto.Cipher.AES`) for AES-ECB (HvDecrypt) and the
hand-rolled little-endian AES-CTR keystream.

### Two non-obvious gotchas (both reproduced faithfully)

1. **`long`-width header fields.** `mMagicA`, `mMagicB`, `mKeyIndex` are declared
   `long`. On the native **LP64** build, `BinStream::operator>>(long&)` reads
   **8 bytes** (`sizeof(long long)`, see `BinStream.h:211`). So the on-disk crypto
   stride the native reader sees is `nonce(16) magicA(8) magicB(8) stuff1(16)
   stuff2(16) keyIndex(8)` — **not** 4-byte fields. The low 32 bits (LE) carry the
   real value; the upper 4 bytes happen to be 0 in this file. The standalone tool
   mirrors this with `Reader.long_()`. (Reading them as 4-byte `int` gives
   `magicB=0`, `keyIndexRaw=garbage` → garbage decrypt — that was the first failure.)
2. **Header is little-endian.** `BufStream(..., littleEndian=true)` in
   `CheckHmxHeader`; on an LE host `ReadEndian` does **not** swap. The AES-CTR
   counter is also little-endian (tomcrypt increments `ctr[0]` first), so the
   keystream starts at `nonce` and increments LSB-first.

### Validation (vs native `MOGG_DBG` ground truth — all MATCH)

```
masterKey[0:16] 39a2bf537d881d033538a3804524eeca   MATCH
keyMask         c10e3e13641b32940e86f942f5d0b4bd   MATCH
gKey afterGetKey a4222a5c8cb3b9aafd0113736b4fa138  MATCH
gKey afterGrind 28d6f504771dca2b8b5f733dd87e3d67   MATCH
gKey FINAL(^mask)e9d8cb171306f8bf85d98a7f2dae89da  MATCH   (= AES key)
nonce           00000000c2fedc4b1f4f15166ac47caf   MATCH
magicA / magicB 0x7DBB9925 / 0x771BC747            MATCH
keyIndex (raw 9 -> 9%6+6) = 9                       MATCH
AES-CTR+HMXA decrypt of first 16384 bytes [0:32] == native AFTER-decrypt log  MATCH
```

Plaintext first 4 bytes = `OggS`. Full stream: **15 ch, 44100 Hz, 236.108 s,
8762 OggS pages, decodes with zero ffmpeg errors end-to-end.**

```
python3 scripts/native/decrypt_mogg.py IN.mogg OUT.ogg     # decrypt
python3 scripts/native/decrypt_mogg.py --self-test IN.mogg # validate OggS only
```

Decrypt is **standalone** (no engine dump hook needed). The existing `DoRawSeek`
endianness fix in `VorbisReader.cpp` was left untouched.

---

## Reference pipeline — `scripts/native/decode_reference.py`

`decrypt_mogg → ffmpeg decode (f32, de-interleave 15ch) → pan/vol downmix → WAV`.

**Pan law mirrors the native renderer exactly**
(`milo-native-engine/src/platform/StreamReceiver_Native.cpp:201`):

```
volL = DbToRatio(vol_db) * max(0, 1 - pan)
volR = DbToRatio(vol_db) * max(0, 1 + pan)        DbToRatio(db) = 10^(db/20)
```

The reference applies this per channel and **sums with NO hard clamp** (32-bit float
WAV, full headroom) — so we can measure how far the game's *clamped* output diverges.
A clamped 16-bit listening copy (`_s16.wav`) is also written.

```
python3 scripts/native/decode_reference.py 20thcenturyboy --section gameplay --keep-channels
python3 scripts/native/decode_reference.py 20thcenturyboy --section preview
```

---

## Reference WAV stats

| WAV | rate | ch | peak | RMS | clip-ratio | crest |
|-----|------|----|------|-----|-----------|-------|
| `/tmp/rb3_ref_20thcenturyboy_gameplay.wav` (full song, no clamp) | 44100 | 2 | **3.1282** | 0.41566 | **0.025129** | 17.53 dB |
| `/tmp/rb3_ref_20thcenturyboy_preview.wav` (78.2–108.2 s, no clamp) | 44100 | 2 | **2.6822** | 0.41563 | **0.023068** | 16.20 dB |
| `/tmp/rb3_ref_20thcenturyboy_channels.wav` (raw 15-ch decode) | 44100 | 15 | ~1.0 | — | — | — |

Also written: `*_s16.wav` (clamped 16-bit listening copies),
`/tmp/rb3_ref_20thcenturyboy.ogg` (decrypted multichannel OggS, kept with `--keep-ogg`).

The reference is **NOT** clipped at the per-channel level (channels peak ≈ unity), but
**the pan/vol-applied sum peaks at 3.13 and would clip 2.5 % of samples if clamped to
[-1, 1]** — see the smoking gun below.

---

## SMOKING GUN — confirms H1 (over-unity sum + hard clamp)

The per-channel gains under the native pan law:

```
ch 0..5  (drums)  pan=±1  vol=-4.5/-4.7 dB  ->  gain on its side = 1+1 times DbToRatio = ~1.19
ch 6     (bass)   pan= 0  vol=-1.5 dB       ->  gain to BOTH L and R              = ~0.84
ch 7..14          pan=±1  vol=-4.3/-4.5 dB  ->  ~1.19–1.22 on its side
```

Two structural problems are visible in the reference, independent of the game's code:

1. **No `0.5` center compensation / no equal-power normalization.** A hard-panned
   channel (`pan=±1`) gets gain `(1+|pan|)·DbToRatio(vol) ≈ 1.19`, i.e. **above unity
   on its hot side**. ~7 such stems land on each of L and R. Summing them drives the
   reference to **peak 3.13** — 3× full scale.
2. **The center channel (bass, pan 0) lands at ~0.84 in BOTH L and R**, so it is
   effectively counted twice in the mono sum.

The game then **hard-clamps to [-1, 1]** (`AudioDevice_Web.cpp::MixSources` L410-414;
the native s16 path saturates at ±32767 — earlier captures peaked at *exactly* 32767).
Clamping a signal that overshoots by 3× produces exactly the reported "clipped noise
that still carries the song envelope": ~2.5 % of samples flat-topped, intermodulation
distortion, but the music still recognizable. The reproducibility-corr/SFM proofs
passed precisely because a clamped copy of the real song is still spectrally non-flat.

**Therefore the fix direction (for the next wave) is a gain-staging / pan-law fix**, not
a decode fix — the decrypt and Vorbis decode are byte-correct (proven here). Candidate
fixes to A/B against this reference: (a) apply the standard RB **`0.5·(1±pan)`** pan
law (so hard-pan = unity, not 2×) in `StreamReceiverNative::RenderAudio`; and/or
(b) a master mix-bus gain / soft limiter before the [-1,1] clamp. The metric wave
should correlate the game's captured WAV against `rb3_ref_*_gameplay.wav` (this file)
and report clip-ratio + crest + correlation; a correct fix should bring the game's
clip-ratio toward 0 and its waveform into high correlation with this reference scaled
to fit [-1,1].

> NOTE on the reference's own peak>1: this reference is deliberately the *un-clamped
> sum under the game's exact pan law*, so it is the faithful "what the game's mix bus
> sees before the clamp." For an *absolute* musical ground truth (headroom-normalized),
> scale this file by `1/peak` (≈0.32) — but do the divergence comparison against the
> game's output using the same pan law + a matched normalization, so the comparison
> isolates the clamp/gain bug rather than a trivial level offset.
