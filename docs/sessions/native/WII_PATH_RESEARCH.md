# WII_PATH_RESEARCH — Wii-assets path to V1

**Investigated by:** rb3-native planning pass, 2026-05-27.
**Path under evaluation:** flip `kPlatformWii`, use freshly-extracted Wii ARK, integrate libavcodec BinkAudio + Wii CMPR/TPL texture decode.

Single-song probe: `songs/20thcenturyboy/20thcenturyboy.bik` is 28.5 MB, header is `KIBE 02 00…`, at byte 0x38 the literal magic `BIKi` appears unobfuscated. The KIBE wrapper is a 0x38-byte envelope over a standard RAD Bink Audio v2 ("binka") stream. **Hypothesis: `mVersion=2` here means envelope-only encryption (no per-block XTEA on the body).** Confirm on a second song before committing.

---

## 1. Bink Audio decoder landscape

- **ffmpeg 7.x has both `binkaudio_dct` and `binkaudio_rdft` decoders plus a standalone `binka` demuxer** (verified `ffmpeg -formats | grep -i bink`). System libavcodec is 62.28.101 — already installed.
- API path: `avformat_open_input("binka")` → `av_read_frame()` per-track AVPacket → `avcodec_send_packet()`/`avcodec_receive_frame()` → `AV_SAMPLE_FMT_FLTP` (planar float, one plane per channel) → resample/interleave. For memory: `avio_alloc_context` over custom-read.
- **No usable standalone library.** ffmpeg is the correct dependency.
- **`BinkOpenAX`, `BinkSetSoundSystem`, `AXSetCompressor` are Wii-AX hardware bindings** — no Linux equivalent. The proper strategy is a parallel `HX_NATIVE BinkReader` path: replace open/track-open/poll-decode with libavcodec calls, keeping `StandardStream::InitInfo`/`ConsumeData` plumbing intact. KIBE-envelope detection in `BinkIntegration::BinkFileReadHeader` (the `whatever == 0x494B` branch) does the XTEA key derivation in pure C++ — port into the native reader so the seek-past-0x38 happens before handing the buffer to ffmpeg.

---

## 2. BIK encryption verification

`src/system/utl/BinkIntegration.cpp:127-198` (`BinkFileReadHeader`) reads `BINKENCRYPTIONHEADER` (0x38 bytes: signature, version, keyIndex, magicA, magicB, two nonce qwords, 16-byte keyMask). Path:

1. `mSignature` swapped from LE; compared to `0x4942 4500` ('BI--' high) and `0x494B` ('--KE' low). On `KIBE` match: builds an `XTEABlockEncrypter`, derives `masterKey[256]` via a `DataReadString` script `"{Na 42 'O32'}"` driving registered byte-grinder ops, then `KeyChain::getKey(mKeyIndex, key, masterKey)` produces 16 raw key bytes, `TheSynth->mGrinder.GrindArray(magicA, magicB, key, 16, 0x1D)` grinds them, the 16 bytes are XORed with `mKeyMask`, byteswapped, and fed to XTEA as the round key with nonce `mNonce[2]`.
2. **Body decryption only if `mEncryptionHeader.mVersion == 2`** (`ReadFunc` line 208). Our `20thcenturyboy.bik` shows `version=2` AND clean `BIKi` magic at byte 0x38 — meaning either (a) body is NOT XTEA-encrypted despite version==2, or (b) the first 16 bytes happen to decrypt to `BIKi …`. Most likely (a): `mVersion` field semantics for `.bik` differ from `.mogg`. **Action: 30-line standalone decryptor on song #2 to rule out coincidence.**
3. `ByteGrinder::HvDecrypt` (line 924-931) is **the `.mogg` path** (`rijndael_ecb_decrypt`) — NOT used for `.bik`. The `.bik` path uses XTEA + GrindArray-shaped XOR key derivation.

---

## 3. Multitrack stream shape

`BinkReader.cpp:101-118`: `BinkOpen` returns `BINK*` with `NumTracks` (typically 7 for RB3: drum_stereo_L/R, bass, guitar, vox, backing_L/R) and per-track `BinkOpenTrack(mBink, i)` yields `BINKTRACK*` with `Channels==1, Bits==16, Frequency` is sample rate. Per-frame: `BinkGetTrackData(track, pcmBuffer)` decodes that track's slice; `BinkNextFrame` advances all tracks. `mStream->InitInfo(NumTracks, freq, false, -1)` registers N mono PCM channels.

Muting/scaling lives in `MasterAudio::MuteTrack` (`beatmatch/MasterAudio.cpp:586,630,657,667-671`) — operates on StandardStream's channel-param array; entirely platform-agnostic, no `.bik`-specific code. The native reader exposes N mono float planes per frame — exactly what libavcodec emits (`AV_SAMPLE_FMT_FLTP`, channel-per-plane).

---

## 4. HX_NATIVE deltas for Wii cached shapes

- **`rndobj/Bitmap.cpp:31-46`** — current `HX_NATIVE` block addresses the **Xbox/PS3 milo trailing-mip-bytes desync** by stashing `mNativeCachedMips`. Matched-fork (Wii) `LoadHeader` simply discards `numMips=0` — Wii GX regenerates mips at runtime. **For Wii the existing block is wrong-but-inert** unless narrowed. Need: `mNativeCachedMips = bs.Cached() && (bs.GetPlatform()==kPlatformXBox || bs.GetPlatform()==kPlatformPS3) ? numMips : 0;`. Same predicate already used in `CharBonesSamples.cpp:570`. **Trivial change.**
- **`char/CharBonesSamples.cpp:554-620`** — explicitly platform-gated to `kPlatformXBox || kPlatformPS3` (line 570). 16-byte cached pad is Xbox/PS3 Save artifact. **No Wii change needed.**
- **`meta/StorePackedMetadata.h:49-70`** — `#pragma pack(push,1)` is on-disk struct layout, NOT platform-dependent. **No change.**
- **`utl/ChunkStream.cpp:70-80`** — generic native-File-fail propagation. **Works unchanged.**
- **Others:**
  - **`rndobj/Tex.cpp:43-90`** (`PlatformBppOrder`) — switches on `kPlatformWii` to set `order=8|0x40|0x100, bpp=4|8` (CMPR/TPL formats). Already coded. **But native gfx backend (`GpuDevice.cpp`) probably handles only Xbox DXT — Wii CMPR/TPL decode is a NEW gap. Biggest hidden cost in Wii path.**
  - **`rndobj/Mesh.cpp:549`** — `CacheStrips` returns true on Wii cached (tri-strip layout). Native renderer may need to handle strips, not just tri lists.
  - `world/LightPreset.cpp:64-65`, `world/CameraShot.cpp:469-470`, `world/SpotlightDrawer.h:103` — minor `kPlatformPC` branches; Wii is the default.
  - `synth/SynthSample.cpp`, `synth/Utl.cpp` — platform-size branches; Wii is matched default.

---

## 5. Platform-flip impact (Xbox → Wii)

Six call sites in `native/src/{main_native,rb3_render_mesh}.cpp` need `kPlatformXBox → kPlatformWii`. **Endianness:** `PlatformLittleEndian(kPlatformXBox)=false, kPlatformWii=false` (`System.cpp:191`) — **both big-endian**; no `BinStream` byteswap diff. But:

- `PlatformSymbol(kPlatformWii)="wii"` → file lookups switch automatically. Asset paths exist in new extraction. Good.
- **Texture format**: Xbox cached bitmaps = DXT1/3/5; **Wii** = CMPR (S3TC variant) + TPL swizzled. **Native `GpuDevice` probably only handles linear/DXT, not Wii CMPR.** Either (a) CPU CMPR→RGBA8 decoder (~150 LoC), or (b) placeholder textures for V1.
- **Mesh strip path** — Wii cached uses triangle strips; matched-fork loads them, but native renderer's index-buffer path may assume `mFaces`.
- `RB3_RENDER_MESH` regression target (`tracksystem_meshes.milo_xbox`) **must continue to work** — file named `_xbox`, `ChunkStream` auto-detects `kPlatformXBox` from suffix at line 229-233 (overrides `mPlatform`). Good — regression is platform-tag-driven, not flag-driven.

---

## 6. Concrete step list

1. **(1 day)** Confirm BIK encryption hypothesis: standalone tool (`native/tools/bik_probe.cpp`, link `tomcrypt`+`ByteGrinder`+`KeyChain`+`EncryptXTEA`) takes `.bik` + key index, derives key, runs XTEA on first 16 bytes, prints magic. Run on 5 songs. Decide: raw-after-0x38 vs body-XTEA.
2. **(1 day)** `RB3_DATA=…/wii-extracted` flag flip: 6 `mPlatform = kPlatformXBox` → `kPlatformWii` sites. Boot menu; expect break in cube-bitmap path + CMPR upload. Narrow `Bitmap.cpp` HX_NATIVE guard. Stub CMPR upload with magenta placeholder so boot proceeds.
3. **(3 days)** Wii CMPR texture decode → RGBA8 in native gfx upload path (`milo-native-engine/src/gfx/GpuDevice.cpp` or sister `rb3_tex_native.cpp`). ~150 LoC.
4. **(2 days)** Parallel `HX_NATIVE BinkReader`: `src/system/synth/BinkReader.cpp` `#ifdef HX_NATIVE` block opens via libavcodec (`avformat_open_input` on custom AVIO over `File*`), reads N tracks as `binkaudio_*`, fills `mPCMBuffers[i]` with int16-mono from `FLTP`, drives same `mStream->InitInfo` + `ConsumeData`. Reuse matched `BinkIntegration` KIBE/XTEA/GrindArray key derivation; seek past 0x38.
5. **(2 days)** KIBE envelope strip + XTEA wiring: `KeyChain::getKey` LP64 cleanup; ensure `ByteGrinder::Init` runs at boot (already in boot path). Validate one song's PCM bit-matches `ffmpeg -i in.bik out.wav` (after stripping 0x38).
6. **(1 day)** Wire ffmpeg into build: `pkg_check_modules(LIBAV libavformat libavcodec libavutil libswresample)` in `native/CMakeLists.txt`, link against system libavcodec 62.x.
7. **(1 week)** V5/V6 play-through: with Wii data + working BinkReader + working CMPR, `Game::LoadSong("20thcenturyboy")` reaches `.mid` parse (chart present at 176 KB), populates `BeatMaster`, triggers Bink open. Fix LP64 byte-correctness gaps in `MidiParser`/`BeatMaster` Load paths.
8. **(3 days)** V7 gem-track HUD + V8 scoring (mostly built; drive from `songMs`).

---

## 7. Risk register

- **R1 (HIGH): Wii CMPR/TPL texture decode**. Native gfx currently has zero Wii-format support. CMPR is S3TC-with-byteswap, but TPL framing + swizzle on cube-bitmaps adds 1-3 days unplanned. Not visible in current HX_NATIVE blocks because `kPlatformXBox` is forced. **Mitigation:** placeholder magenta for V1; polish post-V1.
- **R2 (MED): Bink XTEA body decrypt may actually be required**. Visible `BIKi` at 0x38 might be coincidence. If body XTEA is needed, `XTEABlockEncrypter::Encrypt` ordering must match exactly. **Mitigation:** step 1 resolves before commit.
- **R3 (LOW-MED): libavcodec `binkaudio_dct`/`binkaudio_rdft` may not implement every RB3 sub-variant**. RB3-Wii likely uses DCT mode. 7-track multistream isn't standard "Bink Audio" — it's 7 mono streams multiplexed in BIK. ffmpeg `binka` demuxer handles single-track cleanly; multitrack-audio-only may need custom track-extraction. **Mitigation:** if can't surface per-track packets, feed `binkaudio_dct` codec directly with our own framing from KIBE-stripped BIK parser (~300 LoC).

---

## 8. Recommendation

**Best-case time-to-V1: ~2 weeks of focused work** (steps 1-8 above; assumes existing V3/V4/V5 LP64 cleanup proceeds in parallel AND the favorable Bink encryption hypothesis from step 1 confirms).

This is the **canonical, source-of-truth path** because the decomp matches the Wii binary — every matched-fork code path WAS designed for these exact `.milo_wii`/`.bik` shapes. Total Wii-divergence work is smaller than it looks: most HX_NATIVE blocks are already correctly platform-gated (`kPlatformXBox || kPlatformPS3`), so flipping to Wii actually removes work in `CharBonesSamples`/`Bitmap` (Wii hits the matched `#else` branch).

The new cost concentrates in two boxes: (1) **Wii CMPR texture decode** (~3 days; concrete, well-understood), and (2) **libavcodec BinkReader port** (~3 days incl. KIBE/XTEA wiring). It is the easiest path **PROVIDED** the BIK encryption hypothesis (step 1) confirms favorable. The alternative (Xbox-asset path) blocks on **finding/extracting a 360 ARK** + **MOGG decode** which is also non-trivial; the Wii path's data is already on disk in `orig-assets/wii-extracted/` (114 songs).

---

## Critical Files for Implementation
- `/home/free/code/milohax/rb3/src/system/synth/BinkReader.cpp`
- `/home/free/code/milohax/rb3/src/system/utl/BinkIntegration.cpp`
- `/home/free/code/milohax/rb3/src/system/rndobj/Bitmap.cpp`
- `/home/free/code/milohax/rb3/native/src/main_native.cpp`
- `/home/free/code/milohax/milo-native-engine/src/gfx/GpuDevice.cpp`
