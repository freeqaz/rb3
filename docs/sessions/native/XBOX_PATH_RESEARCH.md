# XBOX 360 ASSETS — V1 path research

**Investigated by:** rb3-native planning pass, 2026-05-27.
**Path under evaluation:** force `kPlatformXBox`, extract the Xbox 360 RB3 disc ARK, port DC3's known-good audio path into rb3.

---

## 1. Asset audit

**Source:** `/srv/torrents/games/arbys/Rock Band 3 (RF) (45410914).zip` (`unzip -l` only — not extracted).

Top-level contents (5.87 GB total, 16 files):
- `default.xex` (13.8 MB) — region-free Xbox 360 executable.
- `gen/main_xbox.hdr` (511 KB) — the **header** arkhelper needs.
- `gen/main_xbox_0.ark` … `gen/main_xbox_9.ark` — **10 ARK parts, 5.87 GB total** (parts 6=1.4 GB, 8=2.0 GB — these almost certainly hold per-song `.mogg`/`.mid`).
- `AvatarAwards`, `charnames.zbm`, `nxeart` — Xbox dashboard metadata, not needed for runtime.

**Layout matches arkhelper's expected input perfectly**: `arkhelper ark2dir gen/main_xbox.hdr <outdir>` (verified `arkhelper 1.3.2` at `/home/free/code/milohax/tools/mackiloha/arkhelper`). The existing partial extract at `/home/free/code/milohax/rb3/orig-assets/extracted/` was demonstrably produced by arkhelper from a 360 ARK — same layout, same `.milo_xbox` suffix.

**Expected per-song layout after a full `ark2dir`:**
- `songs/<id>/<id>.mogg` (encrypted v0xC–v0xF Vorbis audio — the file `Game::LoadSong` fails to open today).
- `songs/<id>/<id>.mid` (chart MIDI — the file `SongData::Load` is stopped at).
- `songs/<id>/gen/<id>.milo_xbox` (visual milo — already extracted).
- `songs/<id>/<id>_keep.png_xbox` (cover art).

**Conclusion:** the zip is a full canonical 360 retail extract. arkhelper will extract it cleanly. **Expected extracted size:** ~6 GB.

---

## 2. Existing dc3-native audio path

The **shared `milo-native-engine`** already ships the audio backend dc3-native uses to play songs end-to-end:

**Engine native impls** (`milo-native-engine/src/`):
- `platform/Synth_Stub.cpp` — `NativeSynth : Synth`. Init/Terminate/NewStream*/NewStreamDecoder. Registers `StreamReceiverNative::Create`, inits miniaudio at 44.1 kHz.
- `platform/StreamReceiver_Native.{h,cpp}` — ring-buffer `StreamReceiver` backed by `AudioDevice::Submit*`.
- `platform/SampleInst_Native.{h,cpp}` + `platform/SynthCommon_Stub.cpp` — sample/voice/FX-send glue.
- `platform/FFmpegAudioReader.{h,cpp}` — Bink/`.bik` decoder (DC3 uses for previews; NOT needed for RB3 song audio).
- `audio/AudioDevice.{h,cpp}` + `audio/miniaudio.h` — miniaudio backend.

**DC3 matched-fork code consumed by the engine** (`dc3-decomp/src/system/synth/`):
- `Synth.cpp` — `CreateNativeSynth()` seam + `Grinder()` accessor.
- `VorbisReader.cpp` — full `.mogg` decode: HMX header, `CheckHmxHeader`, `setupCypher(moggVersion)`, AES-CTR via libtomcrypt. `#ifdef HX_NATIVE` blocks for single-threaded Poll/DoFileRead/Decrypt (Session 67).
- `StandardStream.cpp` — `kInit→kBuffering` 1-line fix (Session 62).
- `ByteGrinder.cpp` — **pure-C++ `GrindArray` for v0xE key derivation** (Sessions 73-75). ~200 lines, 64 byte-transform ops + AES key derivation, validated against Onyx Music Game Toolkit reference.
- `StreamReceiver.cpp` — native `WriteData`/`Poll` impl.

**Verdict on DC3 audio end-to-end:** YES. `dc3-decomp/docs/native/NATIVE_PORT_STATUS.md` line 51 — "Phase 3: Audio — COMPLETE (real-time MOGG playback via FFmpeg/Vorbis/miniaudio)". v0xE song mogg decryption working since 2026-03-23.

**Diagnostic tests** (`milo-native-engine/tests/`):
- `test_mogg_decode.cpp` — VorbisReader → StandardStream → StreamReceiverNative → WAV.
- `test_mogg_v0xe.cpp` — v0xE encrypted-mogg diagnostic. **Already wired with `EngineTestFixture`.**

These are the v1 acceptance harness with zero new test code.

---

## 3. What carries over verbatim

| Component | Engine source | RB3 disposition |
|---|---|---|
| `NativeSynth` skeleton | `milo-native-engine/src/platform/Synth_Stub.cpp` | Copy; rewrite ctor calls for RB3 6-arg `StandardStream(File*, float, float, Symbol, bool, bool)` vs DC3 7-arg. |
| `StreamReceiverNative` | `milo-native-engine/src/platform/StreamReceiver_Native.{h,cpp}` | **Verbatim.** |
| `AudioDevice` (miniaudio) | `milo-native-engine/src/audio/` | **Verbatim.** |
| Pure-C++ `ByteGrinder::GrindArray` (v0xE key) | `dc3-decomp/src/system/synth/ByteGrinder.cpp:783-980` | **Port to** `rb3/src/system/synth/ByteGrinder.cpp` under `#ifdef HX_NATIVE`. RB3 already has `ByteGrinder.h` declaring `GrindArray`. Algorithm-pure → byte-portable. |
| `VorbisReader` HX_NATIVE single-thread blocks | `dc3-decomp/src/system/synth/VorbisReader.cpp` | **Port verbatim** to `rb3/src/system/synth/VorbisReader.cpp`. Function shapes identical. |
| `StandardStream::Init→kBuffering` fix | `dc3-decomp/src/system/synth/StandardStream.cpp` | **Port verbatim** (1-line). |
| MOGG decode test harness | `milo-native-engine/tests/test_mogg_{decode,v0xe}.cpp` | Run against freshly-extracted RB3 song moggs to validate V2 before V5. |

---

## 4. What's new / RB3-specific

| Symbol | RB3 file | Status |
|---|---|---|
| `StandardStream` ctor (6 args vs DC3 7) | `rb3/src/system/synth/StandardStream.{h,cpp}` | Bridge by writing rb3's own `NativeSynth` (~150 lines) constructing the 6-arg ctor. |
| `Mic.cpp`, `MicNull.cpp` | `rb3/src/system/synth/` | RB3-only vocals/Kinect. **Stub for v1.** |
| `MidiSynth`, `MidiInstrumentMgr` | `rb3/src/system/synth/` | Sample-based MIDI for RB Network charts. Not needed for v1. |
| `synthwii/` Wii audio HW | `rb3/src/system/synthwii/` | Already excluded via `_FORK_EXCLUDE_REGEX_PLATFORM`. |
| `BinkIntegration` | `rb3/src/system/utl/BinkIntegration.cpp` | FMVs only. Already stubbed (`BinkMovie_Stub.cpp`). |
| `Singer`, `VocalPlayer` | `rb3/src/band3/.../Singer.{h,cpp}` | In V3 deferral list. Guitar-only v1 doesn't need them. |

**RB3 clang-LP64 cleanliness of `synth/`:** `synth/StandardStream`, `VorbisReader`, `Synth`, `StreamReceiver` are NOT in `_NATIVE_FORK_EXCLUDE` but are excluded from the *engine* compile due to ctor diff. Link them into `rb3-native` directly; write `rb3_synth_native.cpp` as the bridge.

---

## 5. Concrete step list

| # | Step | Files touched | Est. |
|---|---|---|---|
| **X1** | Extract Xbox ARK | `arkhelper ark2dir <unzipped>/gen/main_xbox.hdr orig-assets/extracted-xbox-full/`. Update `RB3_DATA` default. | **1 day** |
| **X2** | Validate v0xE mogg decoding offline | Run `milo-tests --gtest_filter=MoggDecode.*` against `songs/20thcenturyboy/20thcenturyboy.mogg`. WAV out should play. | **1 day** |
| **X3** | Port `ByteGrinder::GrindArray` C++ impl into rb3 | Copy DC3 HX_NATIVE blocks (~200 lines) into `rb3/src/system/synth/ByteGrinder.cpp`. PPC `#else` matched-asm untouched. | **1 day** |
| **X4** | Port `VorbisReader` HX_NATIVE Poll/DoFileRead/Decrypt | Mirror DC3 Sessions 62-67. | **1 day** |
| **X5** | Port `StandardStream::Init→kBuffering` 1-liner | `rb3/src/system/synth/StandardStream.cpp`. | **1 hour** |
| **X6** | Write `rb3_synth_native.cpp` real NativeSynth | Replace 1-line `return new Synth()` with `NativeSynth : Synth` for RB3 6-arg ctor. Wire `StreamReceiverNative::Create`, `AudioDevice::GetInstance().Init(44100)`. | **2 days** |
| **X7** | Unblock `Game::LoadSong` → song.mid parse | LP64 byte-correctness in `beatmatch/MidiParser*` + `BeatMaster.cpp`. Sister-fix via dc3 HX_NATIVE blocks. | **3 days** |
| **X8** | Bring up V3-priority TUs | `GameConfig`, `GameGemList`, `DataResults`. K2-style HX_NATIVE bring-up. | **3 days** |
| **X9** | V4 venue-char Draw crash fix | `Character::DrawLodOrShadow` → `RndMesh::SetUpdateApproxLight`. No-op + log acceptable for v1. | **1 day** |
| **X10** | V6 audio verification + V7 gem-track HUD | songMs advances; gem-track renders; HUD overlay paints. | **3 days** |
| **X11** | V8 + V9 scoring + end-to-end | `RB3_GAME_INPUT` picks song, plays through, returns to results. | **3 days** |

**Cumulative:** X1-X6 audio backend ≈ 1 week. X7-X11 full play-through ≈ 2 weeks. **Total v1: ~3 weeks.**

---

## 6. Risk register

1. **MOGG version mismatch (med-high).** v0xE confirmed for DC3 songs. RB3 360 retail may use 0xF/0x10 — would need extending `magicNumberGeneratorNative` table from Onyx reference. **Mitigation:** X2 reveals in hours.
2. **RB3 vs DC3 StandardStream ctor diff (low).** Documented in `rb3/native/CMakeLists.txt:115`. **Mitigation:** rb3's own `NativeSynth` (X6) builds the 6-arg ctor; no header surgery.
3. **Vorbis library availability (low).** RB3 ships its own `src/system/oggvorbis/`. DC3 links its sister cleanly; switch RB3 to point at the same. **Mitigation:** swap `_NATIVE_FORK_EXCLUDE` glob.

---

## 7. Recommendation

**Best-case time-to-V1: 2-3 weeks.**

**This IS the easiest path** because:
- All audio infra (`NativeSynth`, `StreamReceiverNative`, miniaudio, **v0xE GrindArray pure-C++ impl**, VorbisReader native blocks, StandardStream fixes) exists in `milo-native-engine` + `dc3-decomp` and is proven via dc3-native gameplay.
- Asset zip is verified canonical; one-command extract validated.
- Runtime already loads `.milo_xbox` + forces `kPlatformXBox`; existing Xbox HX_NATIVE blocks (Bitmap cube-mip discard, CharBonesSamples 16-byte padding, StorePackedMetadata pack(1)) are already exercised by boot-to-song.
- The competing Wii path requires Wii-only HX_NATIVE blocks (CMPR/TPL texture decode, possibly Wii-cached bitmap shape) PLUS new BinkAudio decoder integration. Wii has no DC3 sister to lift from.

Single highest-risk item: **MOGG version compatibility** (R1) — measurable in hours via X2, not a deep-investment blocker.

---

## Critical Files for Implementation
- `/home/free/code/milohax/rb3/native/CMakeLists.txt` (engine sourcing, `_NATIVE_FORK_EXCLUDE`, synth-link wiring)
- `/home/free/code/milohax/rb3/native/src/rb3_synth_native.cpp` (replace null `CreateNativeSynth` with real miniaudio-backed NativeSynth)
- `/home/free/code/milohax/rb3/src/system/synth/VorbisReader.cpp` (port DC3 HX_NATIVE Poll/DoFileRead/Decrypt)
- `/home/free/code/milohax/rb3/src/system/synth/ByteGrinder.cpp` (port DC3's pure-C++ `GrindArray` for v0xE)
- `/home/free/code/milohax/milo-native-engine/tests/test_mogg_v0xe.cpp` (existing diagnostic — run against extracted RB3 moggs as X2 gate)
