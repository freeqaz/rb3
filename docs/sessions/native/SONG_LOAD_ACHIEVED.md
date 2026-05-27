# RB3 native — boot + song-load milestone ACHIEVED

**Date:** 2026-05-27 evening (same calendar day as boot-to-song).
**Predecessor:** [BOOT_TO_SONG.md](BOOT_TO_SONG.md) (boot → menu → `Game::LoadSong()` reached).
**Follow-on:** [V1_ONE_SONG.md](V1_ONE_SONG.md) (audio playback + gameplay + scoring — the v1 milestone).
**Decision context:** [V1_PATH_DECISION.md](V1_PATH_DECISION.md) — Xbox 360 asset path chosen.

> 🏁 **STATUS: ACHIEVED.** The game boots, navigates the full menu sequence,
> selects a song, loads it end-to-end into the audio playback state machine,
> and reaches `Game::mLoadState = kReady` — the terminal load-success state.
> No crashes. Clean exit at the frame-budget boundary.

---

## What "boot + song-load" means here

Game::LoadState enum (from Game.h): `{kLoadingSong=0, kWaitingForAudio=1, kReady=2}`. `kReady` is the terminal "audio loaded, ready to play" state — past that, `Game::Go()` would transition to actual gameplay (driven from the screen-flow path, blocked by the headless vignette gate; see follow-on).

Per-layer achievement (each was a separate sub-task this session):

1. **Asset extraction (A1-A6, X1)** — Xbox 360 ARK extracted to `orig-assets/extracted-xbox-full/` via `wit` (Wii WBFS) + Mackiloha `arkhelper` (Xbox ARK). New extract symlinks `.mogg`/`.mid` into the boot-to-song-era `orig-assets/extracted/` layout. 85 songs with audio + chart.

2. **MOGG audio backend port (X3-X6)** — ported DC3-native's HX_NATIVE blocks to RB3:
   - `ByteGrinder::GrindArray` + `magicNumberGeneratorNative` (handles v0xC-v0x10 generically).
   - `VorbisReader::Poll`/`DoFileRead`/`Decrypt` (native single-thread drain).
   - `StandardStream` (Wave-2.4 already had everything; verified).
   - Real `NativeSynth : Synth` + new RB3-shaped `StreamReceiverNative` deriving from both RB3's `StreamReceiver` AND engine's `AudioSource`. Dual `StartSendImpl` overloads.

3. **MIDI parsing brought up (X7a)** — `midi/*.cpp` was NOT in the build (MidiReader was a weak no-op stub). Added `ENGINE_MIDI` source glob. Fixed three clang-LP64 issues: `MidiParser.cpp:213` STLport raw-pointer iterator, `MidiParser.cpp:707` POSIX `index` collision, `MidiParserMgr.cpp:235` switch jump-over-init.

4. **GameGemList brought up (X7b)** — removed from `_NATIVE_FORK_EXCLUDE`; gated STLport `namespace stlpmtx_std` template specializations under `#ifndef HX_NATIVE`. Chart gem analysis runs.

5. **Chart-assert lenience (X7c)** — `SongData::AddKeyboardRangeShift` fSpan assert: warn+clamp (PART_REAL_KEYS_X authoring artifact). `SongData::PostLoadVocals` skip under HX_NATIVE (V1 doesn't need vocals; VocalNoteList/VocalPlayer still excluded).

6. **K1 KeyChain** — real `class KeyChain` static-method impl in `rb3/native/src/rb3_keychain_native.cpp`. `hiddenKeys[0x180]` table + algorithm lifted from engine's `Keygen_Stub.cpp`; signatures adapted to RB3's `int`-returning class-static shape.

7. **K2 RndMesh::sUpdateApproxLight** — undefined static aliased to a `.text` function via weak stub; `Character::DrawLodOrShadow`'s write SIGSEGV'd. Real definition in `Mesh.cpp` under HX_NATIVE.

8. **K3 libvorbis link** — system `vorbis`/`vorbisfile`/`ogg` linked; 26 weak stubs removed from `rndobj_synth_link_stubs.s`. Tiny `vorbis_synthesis_poll` shim delegates to `vorbis_synthesis`.

9. **K4 MILO_AUDIO env override** — engine `AudioDevice::Init` can now open miniaudio under `MILO_HEADLESS=1` when `MILO_AUDIO=1` is also set. DC3 path unchanged.

10. **K-heap (load-bearing)** — **`_PoolAlloc(classSize, reqSize)` under HX_NATIVE was passing `classSize` instead of `max(classSize, reqSize)`. Multi-inheritance derived classes (e.g. `VorbisReader : StreamReader, CriticalSection`) under-allocated. 160 bytes for ~256 bytes of fields → ctor's `mOggStream(0)` smashed the next chunk's malloc header → glibc consistency check fired on the next allocation.** The matched-fork path's `MILO_ASSERT(reqSize == classSize)` silently dropped under HX_NATIVE. Plus 2 sister fixes (CharClip LP64 alloc, Text.cpp 1-byte OOB read), plus ASan tooling at `build-asan/`.

11. **K-ReadDone** — `NativeStdioFile::ReadDone(int &result)` stashed `mLastReadBytes` from prior `ReadAsync` instead of always returning 0. Enabled VorbisReader's decrypt-drain loop.

12. **K5 tomcrypt** — `aes.c`, `crypt.c`, `ctr.c` were never compiled (`file(GLOB)` only matched `.cpp`); weak no-op stubs in `rndobj_synth_link_stubs.s` won. `rijndael_setup`/`ctr_decrypt`/etc. all no-ops → decrypted output was leftover heap garbage. Three lines in CMakeLists explicitly listing the `.c` files. Decrypted output now starts with `OggS` ✓.

13. **K6 StreamReceiver back-pressure deadlock** — the X6 back-pressure design assumed the audio thread was always available to drain `StartSendImpl`'s data. But the receiver is only registered with `AudioDevice` (via `AddSource`) when `PlayImpl()` fires, and `Play()` is called only after `mState == kReady`. Pre-fill phase happens BEFORE `Play()` — so `SendDoneImpl` returned false forever, `mState` never advanced past `kBuffering`, `Game::mLoadState` stuck at `kLoadingSong`. Fix: only enforce back-pressure during `kPlaying` AND `mRegistered`. The pre-fill phase accepts submissions immediately so the state machine can advance.

---

## Reproducible

```bash
RB3_GAME=1 MILO_HEADLESS=1 MILO_AUDIO=1 \
  RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted \
  MILO_MAX_FRAMES=1500 \
  RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn,@320:down,@350:msg:music_library:select_highlighted_node,@450:msg:overshell:end_override_flow:1:0" \
  /home/free/code/milohax/rb3/native/build-native/rb3-native
```

Final lines:
```
GAME_DBG: *** Game::mLoadState = kReady — audio loaded, PushAllOptions next ***
STREAM_DBG: StandardStream::PollStream state 1 -> 2 (chans=15)   [kReady -> kPlaying]
RB3 Native: 1500 frames done — exiting frame loop
rb3-native: RB3_GAME — Run() returned; exiting cleanly.
```

**Regression guards remain GREEN.** `rb3-dta` 138 songs + 10 shown. `RB3_BOOT` 227 cfg + boot complete. (`RB3_RENDER_MESH` should be retested; not in this loop's automated checks.)

---

## What's next (out of scope for "boot + song-load")

**Audio playback gate (post-load):** `MasterAudio::Play()` is what flips StandardStream to `kPlaying` and starts the audio thread submitting PCM to the speaker. It's called from `Game::Go()`, which is called from `GamePanel::Poll()` after the screen-transition vignette completes. In headless mode the vignette's `transition_camshot_done` never fires → `InterstitialPanel::Exiting()` stalls → `GamePanel` never becomes the active panel → `Game::Go()` never called → audio never PLAYS.

**This is the next blocker, tracked as a V1 follow-on**: drive the screen transition `part_difficulty_screen → tv3_*_screen → game_screen` to completion in headless. `InterstitialPanel.cpp` already has a partial HX_NATIVE block (skips the camshot gate); the outgoing screen's `Exiting()` predicate also needs to clear.

**V1 milestone** = audio actually plays + gem-track HUD + scoring + end-to-end run. Plan + ordered task graph in [V1_REMAINING_PLAN.md](V1_REMAINING_PLAN.md).

---

## Per-file fix inventory (this session)

| File | Change |
|---|---|
| `rb3/native/src/rb3_keychain_native.cpp` | NEW — K1 KeyChain |
| `rb3/native/src/rb3_synth_native.cpp` | X6 — real `NativeSynth` |
| `rb3/native/src/rb3_stream_receiver_native.cpp` | X6 NEW + K6 back-pressure fix |
| `rb3/native/src/rb3_vorbis_poll_shim.cpp` | K3 NEW — `vorbis_synthesis_poll` shim |
| `rb3/native/src/native_file.cpp` | K-ReadDone — `mLastReadBytes` |
| `rb3/native/src/main_native.cpp` | K-heap — ASan signal chain; K4 — env-var matrix docs |
| `rb3/native/src/rndobj_synth_link_stubs.s` | K2 — RndMesh static removed; K3 — 26 ogg/vorbis stubs removed |
| `rb3/native/CMakeLists.txt` | X6 — receiver source; K1 — keychain source; K3 — vorbis link + shim source; K5 — tomcrypt `.c` files; AudioDevice un-excluded |
| `rb3/src/system/synth/ByteGrinder.cpp` | X3 — `GrindArray` + `magicNumberGeneratorNative` HX_NATIVE |
| `rb3/src/system/synth/VorbisReader.cpp` | X4 — Poll/DoFileRead/Decrypt + magicHash; K5 debug logs |
| `rb3/src/system/synth/StandardStream.cpp` | K6 STREAM_DBG state-transition log |
| `rb3/src/system/beatmatch/SongData.cpp` | X7c — fSpan warn+clamp, PostLoadVocals HX_NATIVE skip |
| `rb3/src/system/beatmatch/SongParser.cpp` | X7a — `MidiTrackLister::OnText` debug log |
| `rb3/src/system/beatmatch/BeatMaster.cpp` | K6 BEATMASTER_DBG LoaderPoll-phase log |
| `rb3/src/system/beatmatch/GameGemList.cpp` | X7b — gate STLport `stlpmtx_std` namespace under `#ifndef HX_NATIVE` |
| `rb3/src/system/midi/MidiParser.cpp` | X7a — STLport iterator + POSIX `index` collision |
| `rb3/src/system/midi/MidiParserMgr.cpp` | X7a — switch jump-over-init |
| `rb3/src/system/midi/MidiReader.cpp` | X7a — debug logs |
| `rb3/src/system/utl/PoolAlloc.cpp` | K-heap **load-bearing** — `max(classSize, reqSize)` |
| `rb3/src/system/char/CharClip.cpp` | K-heap — LP64 NodeVector header growth |
| `rb3/src/system/rndobj/Mesh.cpp` | K2 — `RndMesh::sUpdateApproxLight` def |
| `rb3/src/system/rndobj/Text.cpp` | K-heap — `c6[i2-1]` bounds check |
| `rb3/src/system/os/System.cpp` | SetUsingCD(false) restored (extract layout reconciliation) |
| `rb3/src/band3/meta_band/ProfileMgr.cpp` | K6 sister — TheGameMicManager null-check |
| `rb3/src/band3/game/Game.cpp` | K6 GAME_DBG mLoadState log |
| `rb3/src/band3/game/GamePanel.cpp` | K6 GAME_DBG IsLoaded log |
| `milo-native-engine/src/audio/AudioDevice.cpp` | K4 — `MILO_AUDIO` env override |

All HX_NATIVE-additive; matched-fork `#else` paths byte-identical to permuter output.

**Working tree:** 130+ modified, ~37 untracked; uncommitted by convention (permuter rewrites `src/system/**` + `src/band3/**`; whitelisted commits only).

**Engine pin:** unchanged at `9ad4e13`.
