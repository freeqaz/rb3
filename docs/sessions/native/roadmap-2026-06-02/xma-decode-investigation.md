# XMA Audio Gap — Investigation & Recommendation (RB3 + DC3 native/web ports)

Date: 2026-06-02. Scope: READ-ONLY research + recommendation. The remaining
silent SFX in the native/web ports are the `.milo_xbox` sample-bank entries
stored in Xbox-360 **XMA** format (`SampleData::Format` enum value `3 = kXMA`).
RB3 song music (MOGG/Vorbis) and big-endian PCM SFX already play.

TL;DR:
- **ffmpeg IS linked into the *native* builds** (RB3 and DC3) and is reachable at
  runtime. **It is NOT in the *web* builds** (Emscripten path hard-excludes it).
- **DC3 already does runtime XMA→PCM decode** via `XmaSampleDecoder.cpp`
  (`DecodeXMAToPCM`, called from `SampleData::Load`). **RB3 does not** — RB3's
  `SampleData.cpp` has no `HX_FFMPEG` block, so kXMA samples are loaded as raw
  bytes and then dropped by `rb3_sampleinst_native.cpp` ("unsupported format").
- XMA samples are **embedded inside the `.milo_xbox` containers** as `SynthSample`
  objects (not loose files).
- **Recommendation: OFFLINE conversion** (matches the user's lean and is the only
  thing that fixes the web build). Convert kXMA→kPCM (16-bit LE) once, in place
  inside the `.milo_xbox`, using ffmpeg's `xma2` decoder. Then both the existing
  RB3 and DC3 native+web PCM playback paths Just Work with zero runtime ffmpeg.

---

## 1. Is ffmpeg linked into the builds?

### Native — YES (both RB3 and DC3), runtime-usable

The shared engine discovers ffmpeg via pkg-config and links it **PUBLIC** onto
`milo-engine`, so it propagates to every consumer:

- `milo-native-engine/CMakeLists.txt:190-196` — `pkg_check_modules(FFMPEG ... libavformat libavcodec libswscale libavutil)`, sets `MILO_ENGINE_HAVE_FFMPEG`.
- `milo-native-engine/CMakeLists.txt:368-371` — gates `FFmpegAudioReader.cpp` + `FFmpegMovieImpl.cpp` into the lib **only** if `MILO_ENGINE_HAVE_FFMPEG`.
- `milo-native-engine/CMakeLists.txt:479-482` — `target_compile_definitions(milo-engine PRIVATE HX_FFMPEG=1)` + `target_link_libraries(milo-engine PUBLIC PkgConfig::FFMPEG)`.

Verified against the built binaries:

```
$ ldd rb3/native/build-native/rb3-native | grep libav
  libavformat.so.62, libavcodec.so.62, libavutil.so.60     # linked
$ grep HX_FFMPEG rb3/native/build-native/compile_commands.json   # HX_FFMPEG=1
$ grep FFMPEG_FOUND rb3/native/build-native/CMakeCache.txt        # FFMPEG_FOUND=1

$ ldd dc3/native/build/dc3-native | grep libav                    # linked
$ nm dc3-native | grep DecodeXMAToPCM
  0000000000eb8420 T _Z14DecodeXMAToPCMPKviiiiPPvPi                # DEFINED
```

So "FFmpeg: 1" (the engine's `message(STATUS ... FFmpeg : ${MILO_ENGINE_HAVE_FFMPEG})`
at `CMakeLists.txt:665`) is **real**: libav is a genuine runtime link dependency
on native, not an offline-only tool.

**But on RB3 native it is currently DEAD for XMA.** RB3 explicitly excludes the
two ffmpeg engine TUs (`rb3/native/CMakeLists.txt:171-179`, in
`MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`) because `FFmpegMovieImpl.h` and
`FFmpegAudioReader` pull DC3-shaped `movie/` and `synth/StandardStream.h`
headers. And RB3's `src/system/synth/SampleData.cpp` has **no** `HX_FFMPEG`
decode block. Confirmed: `nm rb3-native | grep -i 'DecodeXMAToPCM\|FFmpegAudioReader'`
→ nothing. So libav is linked (via the Bink/movie pull-in) but no RB3 code calls
any XMA decoder. The remaining XMA SFX are dropped in
`rb3/native/src/rb3_sampleinst_native.cpp:150-159` ("unsupported sample format 3").

### Web (Emscripten) — NO (both RB3 and DC3), cannot easily link

ffmpeg is **architecturally impossible** on the current web path:

- Engine ffmpeg discovery is gated `if(MILO_ENGINE_HAVE_CONTEXT AND MILO_ENGINE_BUILD_GFX AND NOT EMSCRIPTEN)` — `milo-native-engine/CMakeLists.txt:182`. Under Emscripten the whole block is skipped, so `MILO_ENGINE_HAVE_FFMPEG` is empty/false and `HX_FFMPEG` is never defined.
- DC3 mirrors this explicitly: `dc3/native/CMakeLists.txt:106` `set(ENABLE_FFMPEG OFF)` under EMSCRIPTEN, and `:142` `if(ENABLE_FFMPEG AND NOT EMSCRIPTEN)`.
- Verified against artifacts: `strings rb3/native/build-web/rb3-web.wasm | grep -ci 'avcodec\|DecodeXMAToPCM\|XMA2'` → **0**. Same for `dc3-web.wasm` → **0**.

ffmpeg *can* be compiled to WASM, but it is a heavyweight emscripten port (its own
configure/build, +several MB to the 28 MB wasm) that nobody has set up here. This
is the decisive reason the recommendation below is **offline conversion**: a
runtime decoder would fix native only, leaving web silent.

### Offline conversion scripts already present

- DC3 has `dc3/scripts/web/transcode_bik.sh` + `transcode_bink.py` — these convert Bink `.bik` video to WebM **offline** with the ffmpeg CLI for the web port (it has no runtime ffmpeg). This is the exact precedent for handling web: decode offline, ship a host-friendly format. No XMA/sample equivalent exists yet in either repo.

---

## 2. Can ffmpeg decode XMA at all?

Yes. ffmpeg ships an `xma1`/`xma2` decoder (`AV_CODEC_ID_XMA2`, wmaprodec.c). DC3
already proves it works at runtime in this very engine —
`dc3/native/src/platform/XmaSampleDecoder.cpp`:

- finds `AV_CODEC_ID_XMA2`, sets `ctx->block_align = 2048` (XMA2 packets are always 2048 bytes), `bits_per_coded_sample = 16`.
- synthesizes minimal 34-byte XMA2 extradata: `NumStreams=1` at offset 0, `ChannelMask` (mono=0x04, stereo=0x03) at offset 2, rest zero (`XmaSampleDecoder.cpp:51-68`). This matches the `XMA2WAVEFORMATEX` layout documented in `onyx/doc/xma2defs.h:199-211`.
- feeds the raw `SampleData::mData` as a contiguous array of 2048-byte packets, collects decoded frames (planar/interleaved float or int16), clamps + converts to interleaved int16, returns malloc'd PCM (`:96-186`).

So feasibility of **(a) runtime decode is proven for DC3** and is trivially
portable to RB3 native. But it does **nothing for web** (no runtime ffmpeg).

**(b) offline batch-convert to PCM is equally feasible** and is the same codec
work done once at build/asset-prep time instead of at load. Because the extradata
is synthesized from just (numChannels, sampleRate, packet stream), an offline
tool can reuse the *identical* `DecodeXMAToPCM` logic.

Caveat shared by both: the 34-byte extradata is a minimal guess that DC3 found
sufficient for its banks. RB3 banks should be spot-checked (decode a few, listen /
inspect waveform RMS) since some XMA assets carry per-stream channel masks or 2
streams. The packet count is derived from `mSizeBytes / 2048`; decoded sample
count may differ slightly from the stored `mNumSamples` (XMA encodes in blocks),
which is fine — the loader recomputes `mNumSamples = pcmSize / (2*channels)`.

---

## 3. Where do the XMA samples live + how are they packaged?

**Embedded inside the `.milo_xbox` containers as `SynthSample` objects — NOT loose
files.** Evidence:

- RB3 SFX banks: `rb3/orig-assets/extracted/sfx/gen/*.milo_xbox` (18 files: `common_bank`, `kit01..05_bank`, `ingame_bank`, `cowbell_bank`, `*_fx`, etc.).
- Each is a milo container (magic `afdebeca`, uncompressed type `1008` per the header). `strings cowbell_bank.milo_xbox` →
  ```
  SynthSample
  cowbell1_uc.wav
  SynthSample
  cowbell2_uc.wav     ...
  samples/cowbell1_uc.wav
  ```
  i.e. the bank holds N `SynthSample` objects, each owning a `SampleData` blob.

This means an **offline converter must parse the milo container**: walk its
directory/object list, find each `SynthSample` (which contains a `SampleData`),
read the `SampleData` payload, decode the XMA, and **re-embed** PCM (rewriting the
object's bytes + the container's size/offset bookkeeping). This is meaningfully
harder than converting loose files — it is the same class of work as the char
texture re-pack noted in project memory (`.tex` objects embedded in
`colorpalettes.milo_xbox`).

### On-disk `SampleData` layout (the bytes the converter must rewrite)

From `rb3/src/system/synth/SampleData.cpp:37-83` (`SampleData::Load`) — the common
new-format branch (`rev <= 0xE`):

```
int   rev                       // version
int   fmt                       // 0=kPCM 1=kBigEndPCM 2=kVAG 3=kXMA 4=kATRAC ...
int   mNumSamples
int   mSampleRate
int   mSizeBytes
bool  hasData                   // present iff rev >= 0xB
[payload]                       // iff hasData: ReadChunks(bs, mData, mSizeBytes, 0x8000)
SampleMarker[] mMarkers         // present iff rev >= 0xE
```

Key RB3-vs-DC3 difference: **RB3 `SampleData` has NO `mNumChannels`** (header
`SampleData.h:51-56`: members are `mNumSamples/mSampleRate/mSizeBytes/mFormat/mData/mMarkers`
only; offset 0xc is `mFormat`). RB3 SFX banks are **mono** (confirmed in
`rb3_sampleinst_native.cpp:11`). DC3 `SampleData` (rev up to 0x10) adds `mNumChannels`
at 0xc and a leading `mCRC`, and uses `ReadChunks` the same way
(`dc3/src/system/synth/SampleData.cpp:156-197`). The payload is **chunked**: stored
as 0x8000-byte chunks via `ReadChunks`/`WriteChunks` (`utl/ChunkStream.cpp`), so an
in-place rewrite that changes `mSizeBytes` must re-chunk too (or the offline tool
can just emit a flat payload and write a matching `mSizeBytes`, since `ReadChunks`
reads exactly `mSizeBytes` bytes regardless of chunk count — verify against the
chunk header format before relying on this).

What an offline converter must do per sample, concretely:
1. Locate each `SampleData` blob within the milo object stream.
2. Read `rev, fmt, mNumSamples, mSampleRate, mSizeBytes, hasData`.
3. If `fmt == 3 (kXMA)` and `hasData`: read the chunked payload → raw XMA bytes.
4. Decode XMA→int16 PCM (reuse `DecodeXMAToPCM`; channels=1 for RB3).
5. Rewrite the blob: `fmt = 1 (kBigEndPCM)` *or* `0 (kPCM)`, new `mSizeBytes`,
   new `mNumSamples = pcmSize/2`, and the new payload (re-chunked).
6. Fix up the enclosing milo container's size/offset fields so the file stays valid.

Choosing `kBigEndPCM` lets you write the PCM big-endian to match the rest of the
Xbox bank's endianness, and the existing loaders already byteswap kBigEndPCM at
render time. Choosing `kPCM` (little-endian) is simpler and also already supported.
Either works — the native/web sample players handle both
(`rb3_sampleinst_native.cpp:151`, `SampleInst_Native.cpp:49`).

---

## 4. Recommendation

**Do OFFLINE conversion (kXMA → kPCM/kBigEndPCM, in place inside the `.milo_xbox`),
not runtime decode.** Rationale:

- It is the **only** option that fixes the **web** build (no runtime ffmpeg there, and standing up an emscripten ffmpeg port is large/risky).
- It removes the runtime ffmpeg dependency from native too (smaller, simpler; ffmpeg currently only rides along for Bink/movie which RB3 doesn't even use).
- It makes the **existing PCM playback path Just Work** in both RB3 and DC3, native and web, with a *zero-line* or near-zero loader change — the loaders already handle kPCM/kBigEndPCM.
- It reuses DC3's already-working, already-validated `DecodeXMAToPCM` codec logic.

Runtime decode is the *fallback* if re-packing the milo container proves too fiddly
(see risk below): it's a ~30-minute copy of DC3's two files into RB3 for **native
only**, but leaves web silent — so it does not actually close the gap.

### Recommended implementation: a milo-aware offline XMA→PCM repacker

**Tool**: a small standalone C++ utility built in the engine/native tree (so it can
`#include` the engine's milo container reader + `ReadChunks`/`WriteChunks` +
`DecodeXMAToPCM`), or a Python script using the existing milo-parsing already in
the native milo loader. C++ is preferred because (a) it can link the exact
`DecodeXMAToPCM` ffmpeg path DC3 already validated, and (b) it can reuse the
engine's milo dir read/write so container bookkeeping is correct by construction.

**Reads**: `rb3/orig-assets/extracted/sfx/gen/*.milo_xbox` (and DC3's sample banks).
**Writes**: the same `.milo_xbox` files (in place, after a backup) **or** a sidecar
`*.milo_xbox.pcm` next to each original that the asset pipeline prefers. In-place
rewrite is cleanest (no loader change at all); sidecar is safer (keeps originals)
but needs a loader to prefer the sidecar. **Recommend in-place rewrite of a copy
in a derived asset dir** (e.g. `orig-assets/extracted-pcm/sfx/gen/`) so originals
are never mutated and the build can point at the derived dir.

**Loader change**: with in-place kXMA→kPCM conversion, **none** — `SampleData::Load`
reads `fmt=0/1`, allocates, `ReadChunks`, done; `rb3_sampleinst_native.cpp` /
`SampleInst_Native.cpp` see kPCM/kBigEndPCM and play. Optionally add a one-line
guard so a stray un-converted kXMA still warns instead of silently allocating
garbage (already present in RB3).

### If runtime decode is chosen instead (native-only fallback)

- RB3: add `rb3/native/src/rb3_xma_decoder.cpp` (copy of DC3 `XmaSampleDecoder.cpp`, drop the `numChannels` param or hardcode 1) and an `#ifdef HX_FFMPEG` block in... **but `SampleData.cpp` is matched-fork source** — do NOT edit it. Instead hook decode where RB3 first touches the sample on the native side: in `rb3_sampleinst_native.cpp::StartImpl`, when `fmt==kXMA`, lazily decode `data.mData`→PCM into an owned buffer and play that. This keeps the change entirely in native glue (same strong-def-over-weak-stub discipline the file already follows). Un-exclude nothing from the engine; just add the new TU + link (libav already linked).
- DC3: already done. No change.
- Web: not possible without an emscripten ffmpeg port — so this path does not close the web gap.

### Files each repo would change

**Offline (recommended):**
- New (either repo, or shared): `tools/xma-repack/` (C++ util) **or** `rb3/scripts/assets/convert_xma_banks.py`. Reuses `dc3/native/src/platform/XmaSampleDecoder.cpp` (`DecodeXMAToPCM`).
- RB3: no source change (or a 1-line stray-XMA warning already present). Asset path may point at a derived PCM bank dir — config only.
- DC3: no source change (DC3 already decodes at runtime; offline conversion would let DC3 *drop* its runtime path too, but that's optional cleanup).
- Build/asset docs: note the new pre-build conversion step (mirrors `transcode_bik.sh`).

**Runtime (fallback, native only):**
- RB3: `rb3/native/src/rb3_xma_decoder.{cpp,h}` (new), edit `rb3/native/src/rb3_sampleinst_native.cpp` (call decoder on kXMA), `rb3/native/CMakeLists.txt` (add TU). No edits to matched-fork `SampleData.cpp`.
- DC3: none.

### Effort / risk

- **Offline conversion: ~1.5–3 person-days.** The codec is solved (DC3's `DecodeXMAToPCM`). The real work + risk is the **milo container re-pack**: correctly locating `SampleData` blobs inside the object stream and fixing container/offset/size bookkeeping after the payload grows (XMA→PCM is ~5–10× larger). Risk: medium — a malformed rewrite breaks the whole bank. Mitigations: write to a derived dir (never mutate originals); validate by re-loading each converted bank through the existing native loader and checking RMS > 0 per sample; cross-check decoded sample counts against `mNumSamples`.
- **Runtime decode (RB3 native only): ~0.5 day, low risk** — but does not fix web, so it does not actually close the stated gap. Not recommended as the primary.
- **Web emscripten-ffmpeg: high effort (days), high risk, large wasm bloat.** Avoid.

### Tooling notes

- **vgmstream** (https://github.com/vgmstream/vgmstream) decodes Xbox XMA and is the standard external option — but it is **NOT present anywhere under `/home/free/code/milohax`** (searched). It could be used as the offline decoder CLI instead of writing a C++ decoder, but it still does **not** parse milo containers, so the milo re-pack work is identical either way. ffmpeg's own `xma2` decoder (already linked, already validated by DC3) is the simpler dependency.
- **arkhelper** exists at `tools/mackiloha/arkhelper` (Mackiloha) — handles ARK/milo extraction/repack and is the most promising existing tool for the *container* half of the job; pair it with ffmpeg/DC3's decoder for the *codec* half. Worth evaluating whether arkhelper can already round-trip a `.milo_xbox` with a modified `SampleData` object.
- **onyx** (`/home/free/code/milohax/onyx`, mtolly's toolkit) bundles `xma2encode.exe` / `xmaencoder2.dll` (Windows XMA **encoders**, the reverse direction) and documents the XMA2 format in `onyx/doc/xma2defs.h` — a useful spec reference for extradata, not a decoder.
- **milo-executable-library** (`/home/free/code/milohax/milo-executable-library`) has per-game dirs incl. `rb3/` and a `MiloEditor` — potentially useful for inspecting/validating bank structure, not for decode.

---

## Appendix — key file references

- RB3 sample player (drops XMA): `rb3/native/src/rb3_sampleinst_native.cpp:150-159`
- RB3 SampleData loader (no HX_FFMPEG block): `rb3/src/system/synth/SampleData.cpp:37-83`, `:150-153` (`SizeAs kXMA` warns "don't know size as XMA")
- RB3 SampleData header (no mNumChannels): `rb3/src/system/synth/SampleData.h:26-57`
- DC3 SampleData loader (HAS HX_FFMPEG XMA→PCM block): `dc3/src/system/synth/SampleData.cpp:198-219`
- DC3 XMA decoder: `dc3/native/src/platform/XmaSampleDecoder.{cpp,h}`
- DC3 sample player (consumes decoded PCM): `milo-native-engine/src/platform/SampleInst_Native.cpp:48-54`
- Engine ffmpeg discovery + link: `milo-native-engine/CMakeLists.txt:182,190-196,368-371,479-482,665`
- RB3 native ffmpeg exclude: `rb3/native/CMakeLists.txt:171-179`; web gate: `:206-223`
- DC3 native ffmpeg option: `dc3/native/CMakeLists.txt:106,140-144,1107-1109`
- Offline-conversion precedent (Bink→WebM for web): `dc3/scripts/web/transcode_bik.sh`
- XMA2 format spec: `onyx/doc/xma2defs.h:181-211`
- Container evidence: `strings rb3/orig-assets/extracted/sfx/gen/cowbell_bank.milo_xbox` → `SynthSample` + `*.wav` names
