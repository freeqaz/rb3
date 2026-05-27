# V1 path decision — Xbox 360 assets

**Date:** 2026-05-27
**Decision:** drive V1 (one song end-to-end) on **Xbox 360 assets**, not Wii. Wii path retained as canonical future target post-V1.

Full research reports: [XBOX_PATH_RESEARCH.md](XBOX_PATH_RESEARCH.md) | [WII_PATH_RESEARCH.md](WII_PATH_RESEARCH.md).

## Why Xbox

Two parallel Opus research subagents evaluated each path. The Xbox path wins on three dimensions:

| Dimension | Xbox | Wii |
|---|---|---|
| Audio path source | DC3-native already plays MOGG end-to-end. Port verbatim (~6 files, ~400 LoC of HX_NATIVE blocks already validated against the Onyx Music Game Toolkit reference). | New libavcodec BinkAudio integration; KIBE envelope strip; XTEA wiring (encryption hypothesis unverified). |
| Texture/gfx work | Zero. Existing Xbox HX_NATIVE blocks (Bitmap mip discard, CharBonesSamples 16-byte pad, StorePackedMetadata pack(1)) already exercised by boot-to-song. | Wii CMPR/TPL decode needed (currently 0% support in `GpuDevice`). ~3 days unplanned. Mesh triangle-strip path may need new code. |
| Risk profile | One medium risk (MOGG v0xF/0x10 if RB3 went beyond DC3's v0xE) — measurable in hours via existing `test_mogg_v0xe.cpp`. | Two compound risks: CMPR/TPL gfx + BIK XTEA semantics. Both must resolve favorably. |
| Asset acquisition | Extract `/srv/torrents/games/arbys/Rock Band 3 (RF) (45410914).zip` (verified canonical via `unzip -l`: full hdr + 10 ark parts, 5.87 GB). | Already extracted to `orig-assets/wii-extracted/` (114 songs). |
| Time-to-V1 estimate | 2–3 weeks. | ~2 weeks, **conditional on favorable risk resolution**. Expected variance: ±1 week. |
| Reuse signal | Two complete decomp sister files (`dc3-decomp/src/system/synth/{ByteGrinder,VorbisReader,StandardStream}.cpp`) provide line-by-line porting templates. | No DC3-Bink sister; BinkReader is rb3-only. |

## What this changes

- **Asset path:** extract Xbox ARK to `orig-assets/extracted-xbox-full/` (or replace `orig-assets/extracted/`). Update `RB3_DATA` default to the new path.
- **Runtime platform:** stays at `kPlatformXBox` (current default in 6 `main_native.cpp`/`rb3_render_mesh.cpp` sites). No change.
- **V2 audio backend:** port DC3's HX_NATIVE audio path verbatim. Concrete file deltas in [XBOX_PATH_RESEARCH.md §3](XBOX_PATH_RESEARCH.md).
- **Wii extract retained** at `orig-assets/wii-extracted/` (3.7 GB) as ground truth for matched-fork code path validation + future canonical-Wii bring-up.

## Updated V1 task graph

The original V1–V9 in [V1_ONE_SONG.md](V1_ONE_SONG.md) renumbers to:

| Step | What | Time |
|---|---|---|
| **X1** | Extract `/srv/torrents/games/arbys/Rock Band 3 (RF) (45410914).zip` → `orig-assets/extracted-xbox-full/`; verify `.mogg`+`.mid` present | 1 day |
| **X2** | Run engine `test_mogg_v0xe` against extracted RB3 mogg; confirm v0xE pipeline OR identify higher version | 1 day |
| **X3** | Port DC3's pure-C++ `ByteGrinder::GrindArray` HX_NATIVE block to `rb3/src/system/synth/ByteGrinder.cpp` | 1 day |
| **X4** | Port DC3's `VorbisReader` HX_NATIVE Poll/DoFileRead/Decrypt blocks to rb3 | 1 day |
| **X5** | Port DC3's `StandardStream::Init→kBuffering` 1-liner to rb3 | 1 h |
| **X6** | Write `rb3_synth_native.cpp` real NativeSynth (RB3 6-arg `StandardStream` ctor; mirror engine `Synth_Stub.cpp`) | 2 days |
| **X7** | `Game::LoadSong` runs `.mid` to completion (LP64 byte-correctness in MidiParser/BeatMaster; sister-fix from dc3) | 3 days |
| **X8** | Bring up `GameConfig`, `GameGemList`, `DataResults` from `_NATIVE_FORK_EXCLUDE` | 3 days |
| **X9** | V4 venue-char Draw crash fix (no-op acceptable for V1) | 1 day |
| **X10** | V6 audio gameplay verification + V7 gem-track HUD | 3 days |
| **X11** | V8 + V9 scoring + end-to-end with `RB3_GAME_INPUT` | 3 days |

**Cumulative:** X1-X6 audio up ≈ 1 week. X7-X11 full play-through ≈ 2 weeks. **Total ≈ 3 weeks.**

## Subagent strategy

- **X1, X2** sequential, main session (X1 = arkhelper run; X2 = test run).
- **X3-X5** in parallel via three Sonnet subagents (each is a clean DC3→RB3 verbatim port; files don't conflict).
- **X6** Opus subagent (real bridging code).
- **X7-X8** parallel Opus subagents (independent code paths).
- **X9-X11** sequential after X7-X8 land.

## Open questions parked

- Wii BIK XTEA hypothesis (raw-after-0x38 vs body-XTEA) — defer until post-V1 Wii canonical bring-up.
- Wii CMPR/TPL decode — defer.
- Should `RB3_DATA` env var default change, or should we add `RB3_DATA_PLAT=xbox|wii` selector? Decide at X1 time.
